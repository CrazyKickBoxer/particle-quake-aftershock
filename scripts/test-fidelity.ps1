param([string]$Quake='C:\Program Files (x86)\Steam\steamapps\common\Quake',[switch]$Validation)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskResults=Join-Path $taskRoot 'build/results/fidelity'
$taskUser=Join-Path $taskRoot 'build/fidelity-userdata'
New-Item -ItemType Directory -Force $taskResults,"$taskUser/id1" | Out-Null
$taskCases=@(
 @{Name='reference-off';Fidelity=0;Reflections=0;Oit=1;Scale=1;Msaa=0;Map='e1m2'},
 @{Name='fidelity';Fidelity=1;Reflections=0;Oit=1;Scale=1;Msaa=0;Map='e1m2'},
 @{Name='reflections';Fidelity=1;Reflections=1;Oit=1;Scale=1;Msaa=0;Map='e1m2'},
 @{Name='msaa';Fidelity=1;Reflections=1;Oit=0;Scale=1;Msaa=4;Map='start'},
 @{Name='scaled';Fidelity=1;Reflections=1;Oit=2;Scale=2;Msaa=0;Map='e1m2'},
 @{Name='motion-restart';Fidelity=1;Reflections=1;Oit=1;Scale=1;Msaa=0;Map='e1m2';Motion=1},
 @{Name='live-toggles';Fidelity=1;Reflections=1;Oit=1;Scale=1;Msaa=0;Map='e1m1';Breach=1}
)
foreach($taskCase in $taskCases){
 $taskName=$taskCase.Name
 $taskArgs=@('-basedir',('"'+$Quake+'"'),'-userdir',('"'+$taskUser+'"'),'-window','-width','1280','-height','720','-renderer','particle','-worldmode',$(if($taskCase.Breach){'destruction'}else{'faithful'}),'-physics',$(if($taskCase.Breach){'physx-cpu'}else{'off'}),'-density','fine','-condebug','-capture-tick','150','-screenshot',"$taskName.png",'-frames','190','-benchmark-json',('"'+$taskResults+'/'+$taskName+'.json"'),'+as_structure','11','+as_layers','3','+as_fidelity',"$($taskCase.Fidelity)",'+as_reflections',"$($taskCase.Reflections)",'+as_effects','1','+as_gibs','1','+as_goo','1','+as_shake','0','+as_particle_amount','2','+as_reduced_flashes','1','+vid_vsync','0','+con_notifytime','0','+r_tasks','1','+r_oit',"$($taskCase.Oit)",'+r_scale',"$($taskCase.Scale)",'+vid_fsaa',"$($taskCase.Msaa)",'+host_maxfps','60','+host_framerate','0.016666667','+map',$taskCase.Map)
 if($taskCase.Breach){$taskArgs+=@('-test-panel','9','-test-view','-test-explosion-tick','36','-test-fidelity-cycle')}
 if($taskCase.Motion){$taskArgs+=@('-test-fidelity-motion','-test-fidelity-restart','-capture-sequence','motion','-capture-start-tick','60','-capture-ticks','20')}
 if($Validation){$taskArgs+=@('-validation','2')}
 Set-Content "$taskResults/$taskName-command.txt" ('"'+$taskRoot+'/bin/vkQuake.exe" '+($taskArgs -join ' '))
 $taskProcess=Start-Process "$taskRoot/bin/vkQuake.exe" -ArgumentList $taskArgs -WindowStyle Hidden -PassThru
 if(!$taskProcess.WaitForExit(60000)){Stop-Process -Id $taskProcess.Id;throw "$taskName timed out"}
 $taskLog=Get-Content "$env:APPDATA/vkQuake/qconsole.log" -Raw
 Set-Content "$taskResults/$taskName.log" $taskLog
 if($taskProcess.ExitCode -ne 0 -or $taskLog -match 'Sys_Error|Host_Error|Validation Error|Unknown command|VUID-|SYNC-HAZARD'){throw "$taskName engine error"}
 $taskStats=Get-Content "$taskResults/$taskName.json" -Raw | ConvertFrom-Json
 if($taskStats.msaa_samples -ne [Math]::Max(1,$taskCase.Msaa) -or $taskStats.render_scale -ne $taskCase.Scale){throw "$taskName actual render configuration differs from request"}
 if($taskStats.errors -ne 0 -or $taskStats.structure -ne 11){throw "$taskName settings/error check failed"}
 if($taskCase.Breach -and ($taskStats.detached -ne 36 -or ([regex]::Matches($taskLog,'Aftershock fidelity toggle:')).Count -ne 5)){throw 'Live toggle/breach check failed'}
 if($taskCase.Motion){Copy-Item "$taskUser/id1/motion-*.png" $taskResults;if($taskLog -notmatch "fidelity video restart requested"){throw "Restart not exercised"}}
 Copy-Item "$taskUser/id1/$taskName.png" $taskResults
 Write-Output "PASS $taskName : OIT $($taskCase.Oit) / scale $($taskCase.Scale) / MSAA $($taskCase.Msaa), GPU $($taskStats.gpu_frame_mean_ms) ms"
}
Get-FileHash "$taskRoot/bin/vkQuake.exe" | Format-List
