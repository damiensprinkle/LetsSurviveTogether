<#
Runs the project's automated tests headless (no window, editor must be closed) and prints a pass / fail summary.

  .\Scripts\run_tests.ps1                      all tests (TacticalHorror.*)
  .\Scripts\run_tests.ps1 TacticalHorror.Unit  only some (any test name prefix)
  .\Scripts\run_tests.ps1 -Build               build the editor first

Exit code 0 = all passed. Full report: Saved/Automation/Report/index.json; log: Saved/Logs/AutomationTests.log.
See TESTING.md.
#>
param(
    [string]$Filter = "TacticalHorror",
    [switch]$Build,
    [string]$Engine = "C:\Program Files\Epic Games\UE_5.7\Engine"
)

$ErrorActionPreference = "Stop"
# -DisablePlugins=Fab (below): the Fab plugin crashes a windowless editor when the saved layout has its tab open.
$root = Split-Path $PSScriptRoot -Parent
$project = Join-Path $root "LetsSurviveTogether.uproject"
$report = Join-Path $root "Saved\Automation\Report"

if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) {
    Write-Host "The Unreal editor is open: close it first (tests and builds can't share the project with it)." -ForegroundColor Yellow
    exit 2
}

if ($Build) {
    & "$Engine\Build\BatchFiles\Build.bat" LetsSurviveTogetherEditor Win64 Development "-Project=$project" -WaitMutex -MaxParallelActions=2
    if ($LASTEXITCODE -ne 0) { Write-Host "Build failed." -ForegroundColor Red; exit 1 }
}

if (Test-Path $report) { Remove-Item $report -Recurse -Force }
$started = Get-Date
& "$Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $project `
    "-ExecCmds=Automation RunTests $Filter" "-TestExit=Automation Test Queue Empty" `
    "-ReportExportPath=$report" -unattended -nullrhi -nosplash -nosound -NoP4 -DisablePlugins=Fab `
    "-log=AutomationTests.log" | Out-Null

$index = Join-Path $report "index.json"
if (-not (Test-Path $index)) {
    Write-Host "No test report was written; see Saved\Logs\AutomationTests.log" -ForegroundColor Red
    exit 1
}

$results = Get-Content $index -Raw -Encoding UTF8 | ConvertFrom-Json
$failed = @($results.tests | Where-Object { $_.state -ne "Success" })
foreach ($test in $results.tests) {
    $color = if ($test.state -eq "Success") { "Green" } else { "Red" }
    Write-Host ("{0,-8} {1}" -f $test.state, $test.fullTestPath) -ForegroundColor $color
    if ($test.state -ne "Success") {
        foreach ($entry in $test.entries) {
            if ($entry.event.type -eq "Error") { Write-Host "         $($entry.event.message)" -ForegroundColor Red }
        }
    }
}
$seconds = [int]((Get-Date) - $started).TotalSeconds
if ($results.tests.Count -eq 0) { Write-Host "No tests matched '$Filter'." -ForegroundColor Yellow; exit 1 }
Write-Host ("{0} passed, {1} failed ({2} s)" -f ($results.tests.Count - $failed.Count), $failed.Count, $seconds)
if ($failed.Count -gt 0) { exit 1 } else { exit 0 }
