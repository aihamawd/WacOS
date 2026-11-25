# Run from an administrator PowerShell. Installs only the pinned official releases.
$ErrorActionPreference='Stop'
$me=[Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
if(!$me.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){throw 'Administrator required for sensor driver and protected helper'}
$d=Join-Path $env:ProgramFiles 'WacOS Metrics Sensor'
if(Test-Path $d){throw 'Sensor directory already exists; review existing installation rather than overwrite it'}
$stage=Join-Path ([IO.Path]::GetTempPath()) ('wmp-sensors-'+[guid]::NewGuid())
New-Item -ItemType Directory $stage | Out-Null
try {
 $p=Join-Path $stage 'PawnIO_setup.exe';$z=Join-Path $stage 'LibreHardwareMonitor.zip'
 curl.exe -fL --connect-timeout 10 --max-time 90 -o $p 'https://github.com/namazso/PawnIO.Setup/releases/download/2.2.0/PawnIO_setup.exe'
 if($LASTEXITCODE -ne 0 -or (Get-FileHash $p).Hash -ne '1F519A22E47187F70A1379A48CA604981C4FCF694F4E65B734AAA74A9FBA3032' -or (Get-AuthenticodeSignature $p).Status -ne 'Valid'){throw 'PawnIO integrity check failed'}
 curl.exe -fL --connect-timeout 10 --max-time 90 -o $z 'https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/releases/download/v0.9.6/LibreHardwareMonitor.zip'
 if($LASTEXITCODE -ne 0 -or (Get-FileHash $z).Hash -ne '086D9F1B5A99E643EDC2CFAAAC16051685B551E4C5AC0B32A57C58C0E529C001'){throw 'LibreHardwareMonitor integrity check failed'}
 if(!(Get-CimInstance Win32_SystemDriver | Where-Object Name -eq 'PawnIO')){
  $x=Start-Process $p -ArgumentList '-install','-silent' -Wait -PassThru
  if($x.ExitCode -eq 3010){throw 'PawnIO installed; reboot before continuing'}
  if($x.ExitCode -ne 0){throw "PawnIO installation failed: $($x.ExitCode)"}
 }
 New-Item -ItemType Directory $d | Out-Null
 # Remove inherited access and all preexisting ACEs; grant only admin/system write access.
 $acl=New-Object Security.AccessControl.DirectorySecurity
 $acl.SetAccessRuleProtection($true,$false)
 foreach($sid in 'S-1-5-18','S-1-5-32-544','S-1-5-32-545'){
   $rights=if($sid -eq 'S-1-5-32-545'){'ReadAndExecute'}else{'FullControl'}
   $rule=New-Object Security.AccessControl.FileSystemAccessRule([Security.Principal.SecurityIdentifier]::new($sid),$rights,'ContainerInherit,ObjectInherit','None','Allow')
   $acl.AddAccessRule($rule)
 }
 $acl.SetOwner([Security.Principal.SecurityIdentifier]::new('S-1-5-32-544'))
 Set-Acl -LiteralPath $d -AclObject $acl
 Expand-Archive $z $d
 Copy-Item (Join-Path $PSScriptRoot 'sensor.ps1') (Join-Path $d 'sensor.ps1')
 icacls $d /setowner '*S-1-5-32-544' /T /Q
 if($LASTEXITCODE -ne 0){throw 'Ownership setup failed'}
 $shell=Join-Path $env:windir 'System32\WindowsPowerShell\v1.0\powershell.exe'
 $a=New-ScheduledTaskAction -Execute $shell -Argument ('-NoProfile -NonInteractive -ExecutionPolicy RemoteSigned -File "'+$d+'\sensor.ps1"') -WorkingDirectory $d
 $p=New-ScheduledTaskPrincipal -UserId 'SYSTEM' -LogonType ServiceAccount -RunLevel Highest
 $t=New-ScheduledTaskTrigger -AtStartup
 $s=New-ScheduledTaskSettingsSet -ExecutionTimeLimit ([TimeSpan]::Zero) -RestartCount 3 -RestartInterval (New-TimeSpan -Minutes 1) -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -MultipleInstances IgnoreNew
 Register-ScheduledTask -TaskName 'WacOSMetrics-Sensor' -Action $a -Principal $p -Trigger $t -Settings $s -Description 'Read-only hardware temperature snapshot' | Out-Null
 Start-ScheduledTask -TaskName 'WacOSMetrics-Sensor'
 Write-Output 'Helper installed. Verify fresh temperatures.json and sensor availability.'
} finally { Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue }
