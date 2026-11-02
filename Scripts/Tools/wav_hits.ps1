param([string]$Dir)
# Per WAV: length, and the hits (10 ms windows that jump above 35% of the clip's peak after a quieter stretch), in ms
Get-ChildItem $Dir -Filter "*.wav" | Sort-Object Name | ForEach-Object {
  $b = [System.IO.File]::ReadAllBytes($_.FullName)
  $pos = 12; $dataStart = 0; $dataLen = 0
  while ($pos -lt $b.Length - 8) {
    $id = [System.Text.Encoding]::ASCII.GetString($b, $pos, 4); $len = [BitConverter]::ToInt32($b, $pos + 4)
    if ($id -eq "fmt ") { $ch = [int][BitConverter]::ToInt16($b, $pos + 10); $rate = [BitConverter]::ToInt32($b, $pos + 12); $bits = [int][BitConverter]::ToInt16($b, $pos + 22) }
    if ($id -eq "data") { $dataStart = $pos + 8; $dataLen = $len; break }
    $pos += 8 + $len + ($len % 2)
  }
  $frame = ($bits / 8) * $ch; $frames = [int]($dataLen / $frame)
  $win = [int]($rate / 100); $nw = [int][Math]::Floor($frames / $win)
  $env = New-Object double[] $nw
  for ($w = 0; $w -lt $nw; $w++) {
    $m = 0
    for ($i = $w * $win; $i -lt ($w + 1) * $win; $i += 2) {
      $o = $dataStart + $i * $frame
      $v = [Math]::Abs([int][BitConverter]::ToInt16($b, $o))
      if ($v -gt $m) { $m = $v }
    }
    $env[$w] = $m
  }
  $peak = ($env | Measure-Object -Maximum).Maximum
  $hits = @(); $armed = $true
  for ($w = 0; $w -lt $nw; $w++) {
    if ($armed -and $env[$w] -gt 0.35 * $peak) { $hits += ($w * 10); $armed = $false }
    elseif ($env[$w] -lt 0.15 * $peak) { $armed = $true }
  }
  $pk = [Array]::IndexOf($env, $peak) * 10
  "{0,-34} {1,5} ms  peak@{2,4}  hits: {3}" -f $_.BaseName, ($nw * 10), $pk, ($hits -join ",")
}
