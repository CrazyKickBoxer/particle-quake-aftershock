param([string]$Quake='C:\Program Files (x86)\Steam\steamapps\common\Quake')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskResults=Join-Path $taskRoot 'build\results\structures'
New-Item -ItemType Directory -Force $taskResults | Out-Null
foreach($taskOit in 0,1,2){
 $taskArgs=@('-basedir',('"'+$Quake+'"'),'-userdir',('"'+$taskRoot+'\build\structure-userdata"'),'-window','-width','960','-height','540','-condebug','-renderer','particle','-density','play','-worldmode','destruction','-physics','physx-cpu','-test-panel','9','-test-view','-test-structure-cycle','-test-explosion-tick','45','-frames','190','-capture-tick','150','-screenshot',"switching-$taskOit.png",'+host_maxfps','60','+host_framerate','0.013888889','+r_tasks','1','+r_oit',"$taskOit",'+vid_vsync','0','+as_effects','1','+as_gibs','1','+as_goo','1','+as_particle_amount','4','+as_reduced_flashes','1','+as_shake','0','+map','e1m1')
 Set-Content "$taskResults\switching-$taskOit-command.txt" ('"'+$taskRoot+'\bin\vkQuake.exe" '+($taskArgs -join ' '))
 $taskProcess=Start-Process -FilePath "$taskRoot\bin\vkQuake.exe" -ArgumentList $taskArgs -WindowStyle Hidden -PassThru
 if(!$taskProcess.WaitForExit(60000)){Stop-Process -Id $taskProcess.Id;throw 'Structure switching timed out'}
 $taskLog=Get-Content "$env:APPDATA\vkQuake\qconsole.log" -Raw
 Set-Content "$taskResults\switching-$taskOit.log" $taskLog
 if($taskProcess.ExitCode -ne 0 -or $taskLog -match 'Sys_Error|Host_Error|WARNING:.*Vulkan|Validation Error'){throw 'Structure switching engine error'}
 if([regex]::Matches($taskLog,'Aftershock live structure: mode=').Count -ne 11){throw 'Some live structure changes were missed'}
 if([regex]::Matches($taskLog,'four stable layers, hash=').Count -ne 1){throw 'Live switch unexpectedly rebuilt geometry'}
 if($taskLog -notmatch 'live structure: mode=10 layers=3 detached=36 \(no rebuild\)'){throw 'Live shape/layer changes lost destruction'}
 if(!(Test-Path "$taskRoot\build\structure-userdata\id1\switching-$taskOit.png")){throw 'Missing final switching capture'}
 Write-Output "PASS OIT ${taskOit}: all 11 live structure choices, all four layer counts, maximum effects, reduced flashes, breach preserved"
}

