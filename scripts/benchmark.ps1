param([string]$Quake='C:\Program Files (x86)\Steam\steamapps\common\Quake')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskResults=Join-Path $taskRoot 'build/results'
function Invoke-Scenario([string]$Name,[string]$Map,[string[]]$Options){
 $taskArgs=@('+map',$Map,'-basedir',('"'+$Quake+'"'),'-userdir',('"'+$taskRoot+'\build\userdata"'),'-window','-width','1920','-height','1080','-condebug','-worldmode','destruction','-physics','physx-cpu','-density','play','-test-scenario',$Name,'+host_maxfps','60','+host_framerate','0','+vid_vsync','0','+r_tasks','1','+as_style','1','+as_structure','10','+as_layers','3','+as_effects','1','+as_gibs','1','+as_goo','1','+as_particle_amount','2','+as_reduced_flashes','0','+con_notifytime','0','-benchmark-json',('"'+$taskResults+'\'+$Name+'.json"'))+$Options
 Set-Content "$taskResults/$Name-command.txt" ('"'+$taskRoot+'\bin\vkQuake.exe" '+($taskArgs -join ' '))
 $taskProcess=Start-Process -FilePath "$taskRoot/bin/vkQuake.exe" -ArgumentList $taskArgs -WindowStyle Hidden -PassThru
 if(!$taskProcess.WaitForExit(60000)){Stop-Process -Id $taskProcess.Id;throw "$Name timed out"}
 if($taskProcess.ExitCode -ne 0){throw "$Name failed ($($taskProcess.ExitCode))"}
 Copy-Item "$env:APPDATA/vkQuake/qconsole.log" "$taskResults/$Name.log"
 $taskStats=Get-Content "$taskResults/$Name.json" -Raw | ConvertFrom-Json
 Write-Output "$Name host FPS=$($taskStats.host_fps) p95=$($taskStats.p95_ms)ms GPU mean=$($taskStats.gpu_frame_mean_ms)ms detached=$($taskStats.detached) errors=$($taskStats.errors)"
 if($taskStats.errors -ne 0){throw "$Name reported physics errors"}
}
foreach($taskRenderer in 0,1){Invoke-Scenario "1080-rocket-$taskRenderer" 'e1m1' @('-test-rocket','-test-panel','9','-frames','420','+as_renderer',"$taskRenderer")}
foreach($taskMap in 'start','e1m2','e1m3'){Invoke-Scenario "geometry-$taskMap" $taskMap @('-test-explosion-tick','45','-test-panel','0','-frames','220','+as_renderer','1')}
