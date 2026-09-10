param([string]$Quake='C:\Program Files (x86)\Steam\steamapps\common\Quake')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskUser=Join-Path $taskRoot (('build/cache-test-'+[char]0xE9+'-')+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force $taskUser | Out-Null
function Invoke-CacheCase([string]$Name){
 $taskArgs=@('+map','e1m1','-basedir',('"'+$Quake+'"'),'-userdir',('"'+$taskUser+'"'),'-window','-width','960','-height','540','-condebug','-renderer','particle','-worldmode','faithful','-density','play','-physics','off','-frames','40','+host_maxfps','60')
 $taskProcess=Start-Process -FilePath "$taskRoot/bin/vkQuake.exe" -ArgumentList $taskArgs -WindowStyle Hidden -PassThru
 if(!$taskProcess.WaitForExit(30000)){Stop-Process -Id $taskProcess.Id;throw "Cache $Name timed out"}
 $taskLog=Get-Content "$env:APPDATA/vkQuake/qconsole.log" -Raw
 Set-Content "$taskRoot/build/results/cache-$Name.log" $taskLog
 if($taskProcess.ExitCode -ne 0 -or $taskLog -match 'Sys_Error|Host_Error'){throw "Cache $Name failed"}
 return $taskLog
}
$null=Invoke-CacheCase 'cold'
$taskFiles=@(Get-ChildItem "$taskUser/aftershock-cache" -Filter 'samples-v4-*.bin')
if($taskFiles.Count -ne 1){throw 'Expected one sample cache in the dedicated test folder'}
$taskStream=[IO.File]::Open($taskFiles[0].FullName,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite)
try{$taskStream.Position=$taskStream.Length-1;$taskByte=$taskStream.ReadByte();$taskStream.Position=$taskStream.Length-1;$taskStream.WriteByte($taskByte -bxor 1)}finally{$taskStream.Dispose()}
$taskLog=Invoke-CacheCase 'corrupt'
if($taskLog -notmatch 'rejected corrupt sample cache'){throw 'Corrupt cache was not rejected'}
$taskLog=Invoke-CacheCase 'rebuilt'
if($taskLog -notmatch 'validated sample cache hit'){throw 'Rebuilt cache did not validate'}
Write-Output 'PASS cold generation, corrupt checksum rejection, atomic regeneration, subsequent validated cache hit'
