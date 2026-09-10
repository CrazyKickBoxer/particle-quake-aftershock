param([string]$Quake='C:\Program Files (x86)\Steam\steamapps\common\Quake')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskResults=Join-Path $taskRoot 'build/results/neon'
$taskUser=Join-Path $taskRoot 'build/neon-userdata'
New-Item -ItemType Directory -Force $taskResults,"$taskUser/id1" | Out-Null
$taskCases=@(
 @{Name='cathedral';Map='e1m2';Oit=1;Scale=1;World='faithful'},
 @{Name='entrance';Map='start';Oit=0;Scale=1;World='faithful'},
 @{Name='breach';Map='e1m1';Oit=2;Scale=1;World='destruction'},
 @{Name='scaled';Map='e1m2';Oit=1;Scale=2;World='faithful'}
)
foreach($taskCase in $taskCases){
 $taskName=$taskCase.Name
 $taskArgs=@('-basedir',('"'+$Quake+'"'),'-userdir',('"'+$taskUser+'"'),'-window','-width','1280','-height','720','-renderer','particle','-worldmode',$taskCase.World,'-physics',$(if($taskCase.World -eq 'faithful'){'off'}else{'physx-cpu'}),'-density','fine','-condebug','-capture-tick','90','-screenshot',"neon-$taskName.png",'-frames','125','-benchmark-json',('"'+$taskResults+'\'+$taskName+'.json"'),'+as_structure','11','+as_layers','3','+as_effects','1','+as_gibs','1','+as_goo','1','+as_shake','0','+as_particle_amount','2','+as_reduced_flashes','1','+vid_vsync','0','+con_notifytime','0','+r_oit',"$($taskCase.Oit)",'+r_scale',"$($taskCase.Scale)",'+host_maxfps','60','+map',$taskCase.Map)
 if($taskCase.World -eq 'destruction'){$taskArgs+=@('-test-panel','9','-test-view','-test-explosion-tick','45')}
 Set-Content "$taskResults/$taskName-command.txt" ('"'+$taskRoot+'\bin\vkQuake.exe" '+($taskArgs -join ' '))
 $taskProcess=Start-Process "$taskRoot/bin/vkQuake.exe" -ArgumentList $taskArgs -WindowStyle Hidden -PassThru
 if(!$taskProcess.WaitForExit(60000)){Stop-Process -Id $taskProcess.Id;throw "$taskName timed out"}
 $taskLog=Get-Content "$env:APPDATA/vkQuake/qconsole.log" -Raw
 Set-Content "$taskResults/$taskName.log" $taskLog
 if($taskProcess.ExitCode -ne 0 -or $taskLog -match 'Sys_Error|Host_Error|Validation Error|Unknown command'){throw "$taskName engine error"}
 $taskStats=Get-Content "$taskResults/$taskName.json" -Raw | ConvertFrom-Json
 if($taskStats.structure -ne 11 -or $taskStats.layers -ne 3 -or $taskStats.errors -ne 0){throw "$taskName settings/error check failed"}
 if($taskCase.World -eq 'destruction' -and $taskStats.detached -ne 36){throw 'Neon breach failed'}
 Copy-Item "$taskUser/id1/neon-$taskName.png" $taskResults
 Write-Output "PASS $taskName : neon 11 / 3 layers / OIT $($taskCase.Oit) / scale $($taskCase.Scale), GPU $($taskStats.gpu_frame_mean_ms) ms"
}
