param([string]$Quake='C:\Program Files (x86)\Steam\steamapps\common\Quake')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskResults=Join-Path $taskRoot 'build\results\structures'
$taskUser=Join-Path $taskRoot 'build\structure-userdata'
New-Item -ItemType Directory -Force $taskResults,"$taskUser\id1" | Out-Null
$taskNames=@('original','dust','beads','chips','microvoxels','flakes','splinters','fibers','goo','rings','mixed')
$taskHashes=@()
for($taskMode=0;$taskMode -le 10;$taskMode++){
 $taskName=$taskNames[$taskMode]
 $taskArgs=@('-basedir',('"'+$Quake+'"'),'-userdir',('"'+$taskUser+'"'),'-window','-width','1280','-height','720','-condebug','-renderer','particle','-density','play','-worldmode','destruction','-physics','physx-cpu','-test-panel','9','-test-view','-test-view-oblique','-test-explosion-tick','65','-capture-sequence',"structure-$taskName",'-capture-start-tick','35','-capture-ticks','60','-frames','160','-benchmark-json',('"'+$taskResults+'\'+$taskName+'.json"'),'+host_maxfps','60','+host_framerate','0.013888889','+r_tasks','1','+vid_vsync','0','+con_notifytime','0','+as_style','0','+as_structure',"$taskMode",'+as_layers','3','+as_effects','1','+as_gibs','1','+as_goo','1','+as_particle_amount','2','+as_shake','0','+map','e1m1')
 Set-Content -LiteralPath "$taskResults\$taskName-command.txt" -Value ('"'+$taskRoot+'\bin\vkQuake.exe" '+($taskArgs -join ' '))
 $taskProcess=Start-Process -FilePath "$taskRoot\bin\vkQuake.exe" -ArgumentList $taskArgs -WindowStyle Hidden -PassThru
 if(!$taskProcess.WaitForExit(60000)){Stop-Process -Id $taskProcess.Id;throw "$taskName timed out"}
 $taskLog=Get-Content "$env:APPDATA\vkQuake\qconsole.log" -Raw
 Set-Content "$taskResults\$taskName.log" $taskLog
 if($taskProcess.ExitCode -ne 0 -or $taskLog -match 'Sys_Error|Host_Error|WARNING:.*Vulkan|Validation Error'){throw "$taskName engine error"}
 if($taskLog -notmatch 'four stable layers, hash=([0-9a-f]+)'){throw 'Missing scatter fingerprint'}
 $taskHashes+=$Matches[1]
 foreach($taskTick in '00035','00095','00155'){
  $taskImage="$taskUser\id1\structure-$taskName-$taskTick.png"
  if(!(Test-Path $taskImage)){throw "Missing $taskName capture $taskTick"}
  Copy-Item $taskImage $taskResults
 }
 $taskStats=Get-Content "$taskResults\$taskName.json" -Raw | ConvertFrom-Json
 if($taskStats.errors -ne 0){throw "$taskName physics error"}
 Write-Output "PASS structure $taskMode / $taskName : GPU $($taskStats.gpu_frame_mean_ms) ms, $($taskStats.detached) detached"
}
if(@($taskHashes | Select-Object -Unique).Count -ne 1){throw 'Scatter changed across modes or cold/warm cache'}
Write-Output "PASS identical placement fingerprint for all modes and cold/warm cache: $($taskHashes[0])"
