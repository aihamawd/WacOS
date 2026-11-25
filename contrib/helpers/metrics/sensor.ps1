param([switch]$Once)
$ErrorActionPreference='Stop'
Set-Location -LiteralPath $PSScriptRoot
Add-Type -Path (Join-Path $PSScriptRoot 'LibreHardwareMonitorLib.dll')
$c=New-Object LibreHardwareMonitor.Hardware.Computer
$c.IsCpuEnabled=$true
$c.IsGpuEnabled=$true
$out=Join-Path $PSScriptRoot 'temperatures.json'
$tmp=Join-Path $PSScriptRoot 'temperatures.tmp'
try {
  $c.Open()
  do {
    $cpus=@();$gpus=@()
    foreach($h in $c.Hardware) {
      $h.Update()
      foreach($s in $h.Sensors) {
        if($s.SensorType.ToString() -ne 'Temperature' -or $null -eq $s.Value){continue}
        $v=[double]$s.Value
        if($v -le 0 -or $v -ge 125){continue}
        if($h.HardwareType.ToString() -eq 'Cpu' -and $s.Name -match '^(CPU Package|Core \(Tctl/Tdie\)|CPU \(Tctl/Tdie\)|CPU Tctl/Tdie|CPU Die \(average\)|CPU Die|Package)$'){$cpus+=$v}
        if($h.HardwareType.ToString() -match '^Gpu' -and $s.Name -match '^(GPU Core|GPU Temperature|GPU Core Temperature)$'){$gpus+=$v}
      }
    }
    $cpu=if($cpus.Count){($cpus|Measure-Object -Maximum).Maximum}else{$null}
    $gpu=if($gpus.Count){($gpus|Measure-Object -Maximum).Maximum}else{$null}
    $j=[ordered]@{utc=[DateTimeOffset]::UtcNow.ToUnixTimeSeconds();cpu=$cpu;gpu=$gpu;source='LibreHardwareMonitor 0.9.6 / PawnIO 2.2.0'} | ConvertTo-Json -Compress
    [IO.File]::WriteAllText($tmp,$j,[Text.UTF8Encoding]::new($false))
    try { if([IO.File]::Exists($out)){[IO.File]::Replace($tmp,$out,(Join-Path $PSScriptRoot 'temperatures.prev'))}else{[IO.File]::Move($tmp,$out)} } catch [IO.IOException] {}
    if($Once){Write-Output $j;break}
    Start-Sleep -Seconds 2
  } while($true)
} finally { $c.Close() }
