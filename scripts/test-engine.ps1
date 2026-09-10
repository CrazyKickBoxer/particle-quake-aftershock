param([string]$Quake='C:\Program Files (x86)\Steam\steamapps\common\Quake',[switch]$Quick,[switch]$PersistenceOnly)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskResults=Join-Path $taskRoot 'build\results'
New-Item -ItemType Directory -Force $taskResults | Out-Null
function Run-Case([string]$Name,[string[]]$Options,[string]$Map='e1m1'){
 $taskArgs=@('+map',$Map,'-basedir',('"'+$Quake+'"'),'-userdir',('"'+$taskRoot+'\build\userdata"'),'-window','-width','960','-height','540','-condebug','+host_maxfps','60','+host_framerate','0','+r_tasks','1','+con_notifytime','0','+as_progressive','0','-benchmark-json',('"'+$taskResults+'\'+$Name+'.json"'))+$Options
 $taskProcess=Start-Process -FilePath "$taskRoot\bin\vkQuake.exe" -ArgumentList $taskArgs -WindowStyle Hidden -PassThru
 Set-Content -LiteralPath "$taskResults\$Name-command.txt" -Value ('"'+$taskRoot+'\bin\vkQuake.exe" '+($taskArgs -join ' '))
 if(!$taskProcess.WaitForExit(60000)){Stop-Process -Id $taskProcess.Id;Copy-Item "$env:APPDATA\vkQuake\qconsole.log" "$taskResults\$Name.log";throw "$Name timed out; see $taskResults\$Name.log"}
 if($taskProcess.ExitCode -ne 0){Copy-Item "$env:APPDATA\vkQuake\qconsole.log" "$taskResults\$Name.log";throw "$Name exited with $($taskProcess.ExitCode); see $taskResults\$Name.log"}
 $taskLog=Get-Content "$env:APPDATA\vkQuake\qconsole.log" -Raw
 Set-Content -LiteralPath "$taskResults\$Name.log" -Value $taskLog
 if($taskLog -match 'Sys_Error|Host_Error|WARNING:.*Vulkan|Validation Error'){throw "$Name engine error"}
 return $taskLog
}
if(!$PersistenceOnly){
foreach($taskRenderer in 0,1){
 $taskLog=Run-Case "rocket-$taskRenderer" @('-worldmode','destruction','-physics','physx-cpu','-test-scenario','rocket','-test-rocket','-test-projectile','-test-panel','9','-capture-tick','280','-screenshot',"rocket-$taskRenderer.png",'-frames','420','+as_renderer',"$taskRenderer")
 if($taskLog -notmatch 'real-weapon result: events=[1-9]\d* detached=36 walk=PASS' -or $taskLog -notmatch 'errors=0'){throw "Rocket $taskRenderer failed"}
 if($taskLog -notmatch 'rocket traversal PASS'){throw "Rocket projectile did not cross the opening with renderer $taskRenderer"}
 Write-Output "PASS rocket-${taskRenderer}: actual rockets, full panel removal, WALK traversal, threaded rendering"
}
if($Quick){return}
& "$taskRoot/build/aftershock/Release/aftershock_arena_generator.exe" "$taskRoot/build/userdata/id1/maps/aftershock_arena.bsp"
if($LASTEXITCODE){throw 'Arena generation failed'}
foreach($taskRenderer in 0,1){
 $taskLog=Run-Case "arena-$taskRenderer" @('-worldmode','destruction','-physics','physx-cpu','-test-scenario','arena','-test-rocket','-test-traverse-ticks','300','-test-panel','0','-frames','540','+as_renderer',"$taskRenderer") 'aftershock_arena'
 if($taskLog -notmatch 'real-weapon result: events=[1-9]\d* detached=43 walk=PASS' -or $taskLog -notmatch 'errors=0'){throw "Partial breach $taskRenderer failed"}
 Write-Output "PASS arena-${taskRenderer}: partial breach, physical rubble pushing, WALK traversal"
}
$taskHashes=@()
foreach($taskRenderer in 0,1){
 foreach($taskEffects in 0,1){
 $taskLog=Run-Case "faithful-$taskRenderer-fx-$taskEffects" @('-worldmode','faithful','-physics','off','-test-seed','1234','-sim-checksum-tick','180','-frames','250','+host_framerate','0.013888889','+as_effects',"$taskEffects",'+as_shake',"$taskEffects",'+as_renderer',"$taskRenderer")
 if($taskLog -notmatch 'normalized gameplay tick=180 hash=([0-9a-f]+)'){throw 'Missing gameplay hash'}
 $taskHashes+=$Matches[1]
 }
}
if(@($taskHashes | Select-Object -Unique).Count -ne 1){throw "Gameplay diverged: $taskHashes"}
Write-Output "PASS faithful normalized entity state: $($taskHashes[0])"
}
$taskLog=Run-Case 'persistence' @('-worldmode','destruction','-physics','physx-cpu','-test-scenario','breach','-test-panel','9','-test-explosion-tick','45','-test-save-tick','110','-test-renderer-tick','130','-test-load-tick','160','-frames','350','+as_renderer','1','+host_framerate','0')
if($taskLog -notmatch 'Aftershock restored: detached=36 bodies=36'){throw 'Save/load failed'}
Write-Output 'PASS integrated Quake + structural sidecar save/load'
if($taskLog -notmatch 'renderer switched: renderer=0 detached=36 \(no rebuild\)'){throw 'Renderer switch did not preserve damage'}
Write-Output 'PASS live renderer switch preserves destruction'
