param([string]$Quake='C:\Program Files (x86)\Steam\steamapps\common\Quake')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskResults=Join-Path $taskRoot 'build\results\particle-modes'
$taskUser=Join-Path $taskRoot 'build\mode-userdata'
New-Item -ItemType Directory -Force $taskResults,"$taskUser\id1\maps" | Out-Null
& "$taskRoot\build\aftershock\Release\aftershock_arena_generator.exe" "$taskUser\id1\maps\aftershock_arena.bsp"
if($LASTEXITCODE){throw 'Arena generator failed'}
function Run-ParticleCase([string]$Name,[string[]]$Options){
 $taskArgs=@('-basedir',('"'+$Quake+'"'),'-userdir',('"'+$taskUser+'"'),'-window','-width','1280','-height','720','-condebug','+host_maxfps','60','+host_framerate','0.013888889','+r_tasks','1','+vid_vsync','0','+con_notifytime','0','+as_effects','1','+as_gibs','1','+as_goo','1','+as_particle_amount','2','+as_reduced_flashes','0','+as_shake','0','-renderer','particle','-density','play','-benchmark-json',('"'+$taskResults+'\'+$Name+'.json"'))+$Options
 Set-Content -LiteralPath "$taskResults\$Name-command.txt" -Value ('"'+$taskRoot+'\bin\vkQuake.exe" '+($taskArgs -join ' '))
 $taskProcess=Start-Process -FilePath "$taskRoot\bin\vkQuake.exe" -ArgumentList $taskArgs -WindowStyle Hidden -PassThru
 if(!$taskProcess.WaitForExit(60000)){Stop-Process -Id $taskProcess.Id;throw "$Name timed out"}
 $taskLog=Get-Content "$env:APPDATA\vkQuake\qconsole.log" -Raw
 Set-Content -LiteralPath "$taskResults\$Name.log" -Value $taskLog
 if($taskProcess.ExitCode -ne 0 -or $taskLog -match 'Sys_Error|Host_Error|WARNING:.*Vulkan|Validation Error'){throw "$Name engine error"}
 return $taskLog
}
$taskStyles=@('living-stone','volcanic','sandstorm','crystal','corrupted-flesh','industrial-rust','spectral','frozen-ruins','electric-grid','cosmic-dust')
foreach($taskStyle in $taskStyles){
 $taskLog=Run-ParticleCase $taskStyle @('-style',$taskStyle,'-worldmode','destruction','-physics','physx-cpu','-test-panel','9','-test-view','-test-explosion-tick','45','-capture-sequence',"mode-$taskStyle",'-capture-start-tick','25','-capture-ticks','60','-frames','165','+map','e1m1')
 if($taskLog -notmatch 'Aftershock gore: spawned=[1-9]\d*'){throw "$taskStyle did not emit gibs"}
 foreach($taskTick in '00025','00085','00145'){
  if(!(Test-Path "$taskUser\id1\mode-$taskStyle-$taskTick.png")){throw "Missing $taskStyle capture $taskTick"}
 }
 Write-Output "PASS mode: $taskStyle"
}
$taskLog=Run-ParticleCase 'gore-arena' @('-style','enhanced','-worldmode','faithful','-physics','off','-test-gore','-capture-tick','170','-screenshot','gore-arena.png','-frames','260','+map','aftershock_arena')
if($taskLog -notmatch 'Aftershock gore: spawned=(\d+) wall=(\d+) floor=(\d+) smears=(\d+) splats=(\d+)'){throw 'Missing gore counters'}
if([int]$Matches[1] -lt 48 -or [int]$Matches[2] -lt 1 -or [int]$Matches[3] -lt 1 -or [int]$Matches[4] -lt 1 -or [int]$Matches[5] -lt 1){throw "Gore fixture did not exercise slides and splashes: $($Matches[0])"}
Write-Output "PASS gib collision fixture: $($Matches[0])"
foreach($taskOit in 0,2){
 $taskLog=Run-ParticleCase "gore-oit-$taskOit" @('-style','electric-grid','-worldmode','faithful','-physics','off','-test-gore','-frames','200','+map','aftershock_arena','+r_oit',"$taskOit",'+as_particle_amount','4','+as_reduced_flashes','1')
 if($taskLog -notmatch 'Aftershock gore: spawned=192 wall=[1-9]'){throw "Transparency mode $taskOit failed"}
 Write-Output "PASS transparency $taskOit, maximum effects, reduced flashes"
}
$taskLog=Run-ParticleCase 'gore-disabled' @('-worldmode','faithful','-physics','off','-test-gore','-frames','80','+map','aftershock_arena','+as_gibs','0')
if($taskLog -notmatch 'Aftershock gore: spawned=0 wall=0 floor=0 smears=0 splats=0'){throw 'Gib disable switch failed'}
Write-Output 'PASS gib disable switch'
Copy-Item -Path "$taskUser\id1\mode-*.png","$taskUser\id1\gore-arena.png" -Destination $taskResults
