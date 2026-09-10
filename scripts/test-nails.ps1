param([string]$Quake='C:\Program Files (x86)\Steam\steamapps\common\Quake')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskResults=Join-Path $taskRoot 'build/results/nails'
$taskUser=Join-Path $taskRoot 'build/nails-userdata'
New-Item -ItemType Directory -Force $taskResults,"$taskUser/id1" | Out-Null
$taskHashes=@()
foreach($taskCase in @(@{Name='off';Nails=0;Oit=1},@{Name='on';Nails=1;Oit=1},@{Name='sorted';Nails=1;Oit=0},@{Name='moment';Nails=1;Oit=2})){
 $taskName=$taskCase.Name
 $taskArgs=@('-basedir',('"'+$Quake+'"'),'-userdir',('"'+$taskUser+'"'),'-window','-width','1280','-height','720','-renderer','particle','-worldmode','faithful','-physics','off','-density','fine','-condebug','-test-nails','-test-seed','1234','-sim-checksum-tick','180','-frames','200','-capture-sequence',$taskName,'-capture-start-tick','65','-capture-ticks','30','-benchmark-json',('"'+$taskResults+'/'+$taskName+'.json"'),'+as_nails',"$($taskCase.Nails)",'+as_structure','11','+as_layers','3','+as_fidelity','1','+as_reflections','1','+as_effects','1','+as_gibs','0','+as_goo','0','+as_shake','0','+as_reduced_flashes','0','+vid_vsync','0','+con_notifytime','0','+r_tasks','1','+r_oit',"$($taskCase.Oit)",'+vid_fsaa','0','+host_maxfps','60','+host_framerate','0.016666667','+map','e1m1')
 Set-Content "$taskResults/$taskName-command.txt" ('"'+$taskRoot+'/bin/vkQuake.exe" '+($taskArgs -join ' '))
 $taskProcess=Start-Process "$taskRoot/bin/vkQuake.exe" -ArgumentList $taskArgs -WindowStyle Hidden -PassThru
 if(!$taskProcess.WaitForExit(60000)){Stop-Process -Id $taskProcess.Id;throw "$taskName timed out"}
 $taskLog=Get-Content "$env:APPDATA/vkQuake/qconsole.log" -Raw
 Set-Content "$taskResults/$taskName.log" $taskLog
 if($taskProcess.ExitCode -ne 0 -or $taskLog -match 'Sys_Error|Host_Error|Validation Error|Unknown command|VUID-|SYNC-HAZARD'){throw "$taskName engine error"}
 if($taskLog -notmatch 'normalized gameplay tick=180 hash=([0-9a-f]+)'){throw 'Missing gameplay hash'}
 $taskHashes+=$Matches[1]
 if($taskCase.Nails -and ($taskLog -notmatch 'nail impact: super=0 surface=0' -or $taskLog -notmatch 'nail impact: super=1 surface=0')){throw "$taskName missing real nail/supernail wall hits"}
 Copy-Item "$taskUser/id1/$taskName-*.png" $taskResults
 $taskStats=Get-Content "$taskResults/$taskName.json" -Raw | ConvertFrom-Json
 if($taskStats.detached -ne 0 -or $taskStats.errors -ne 0){throw 'Nails changed structural state'}
 Write-Output "PASS $taskName : GPU $($taskStats.gpu_frame_mean_ms) ms, gameplay $($taskHashes[-1])"
}
if(@($taskHashes|Select-Object -Unique).Count -ne 1){throw "Nail cosmetics changed gameplay: $taskHashes"}
Get-FileHash "$taskRoot/bin/vkQuake.exe" | Format-List
