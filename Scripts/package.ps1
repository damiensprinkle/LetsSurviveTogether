<#
Builds a packaged Windows game (Development: the ` console works, logs are written) to copy to other PCs for testing.

  .\Scripts\package.ps1             build, cook and package into Builds\Windows, then zip it
  .\Scripts\package.ps1 -NoZip      skip the zip

Output: Builds\Windows\LetsSurviveTogether.exe, and Builds\LetsSurviveTogether-<date>.zip to copy to another PC.
The first run cooks everything and takes a long time; later runs only cook what changed. Editor must be closed.
Builds\ is not in git. See TESTING.md, "Two PCs".
#>
param(
    [switch]$NoZip,
    [string]$Engine = "C:\Program Files\Epic Games\UE_5.7\Engine"
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$project = Join-Path $root "LetsSurviveTogether.uproject"
$builds = Join-Path $root "Builds"

if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) {
    Write-Host "The Unreal editor is open: close it first." -ForegroundColor Yellow
    exit 2
}

$started = Get-Date
& "$Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun "-project=$project" -platform=Win64 -clientconfig=Development `
    -build -cook -stage -pak -prereqs -archive "-archivedirectory=$builds" -nop4 -utf8output -unattended
if ($LASTEXITCODE -ne 0) { Write-Host "Packaging failed (exit $LASTEXITCODE). See the output above." -ForegroundColor Red; exit 1 }

$game = Join-Path $builds "Windows"
if (-not (Test-Path (Join-Path $game "LetsSurviveTogether.exe"))) { Write-Host "No game in $game" -ForegroundColor Red; exit 1 }

if (-not $NoZip) {
    $zip = Join-Path $builds ("LetsSurviveTogether-{0:yyyy-MM-dd-HHmm}.zip" -f (Get-Date))
    Compress-Archive -Path "$game\*" -DestinationPath $zip -CompressionLevel Fastest
    Write-Host "Zip: $zip"
}
Write-Host ("Game: {0}\LetsSurviveTogether.exe ({1:N0} min)" -f $game, ((Get-Date) - $started).TotalMinutes) -ForegroundColor Green
