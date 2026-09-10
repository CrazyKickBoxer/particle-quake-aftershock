$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskResults=Join-Path $taskRoot 'build/results'
$taskEvidence=Join-Path $taskRoot 'docs/results'
New-Item -ItemType Directory -Force $taskEvidence | Out-Null
$taskNames=@('1080-rocket-0','1080-rocket-1','geometry-start','geometry-e1m2','geometry-e1m3')
$taskLines=@('# Measured engine throughput','','Windows x64 Release; CPU PhysX; Enhanced / Mixed Debris Cloud / 3 layers / Play / Cinematic; 1920 x 1080 windowed; v-sync off; host cap 60; threaded renderer. Capture I/O disabled. See VALIDATION.md for methodology and hardware.','',('Measured executable SHA-256: `{0}`.' -f (Get-FileHash "$taskRoot/bin/vkQuake.exe").Hash),'','| Scenario | Host frames/s | p50 ms | p95 ms | p99 ms | GPU frame mean ms | PhysX step mean ms | Detached | PhysX peak bytes |','|---|---:|---:|---:|---:|---:|---:|---:|---:|')
foreach($taskName in $taskNames){
 $taskData=Get-Content "$taskResults/$taskName.json" -Raw | ConvertFrom-Json
 $taskLabel=switch($taskName){'1080-rocket-0'{'e1m1 rockets, Classic'};'1080-rocket-1'{'e1m1 rockets, Particle'};default{$taskName.Replace('geometry-','')+', synthetic explosion, Particle'}}
 $taskLines+=('| {0} | {1} | {2} | {3} | {4} | {5} | {6} | {7} | {8} |' -f $taskLabel,$taskData.host_fps,$taskData.p50_ms,$taskData.p95_ms,$taskData.p99_ms,$taskData.gpu_frame_mean_ms,$taskData.physics_mean_ms,$taskData.detached,$taskData.physx_peak_bytes)
 Copy-Item "$taskResults/$taskName.json","$taskResults/$taskName-command.txt" $taskEvidence
}
$taskLines+=@('','All five scenarios reported zero PhysX errors. These are short engine-frame measurements, not presented frame rates. The 60 presented-FPS target, isolated GPU pass costs, and total VRAM/effect budgets remain unverified. PhysX allocator values exclude Blast, Quake and graphics memory.','',('Recorded: {0}.' -f (Get-Date).ToString('yyyy-MM-dd HH:mm:ss zzz')))
Set-Content "$taskRoot/docs/MEASUREMENTS.md" $taskLines
foreach($taskName in @('hardware.json','test-engine.txt','unit-tests.txt','cache-test.txt','benchmark.txt','persistence-repeats.txt','persistence-final.txt','projectile-check.txt','particle-modes-test.txt')){if(Test-Path "$taskResults/$taskName"){Copy-Item "$taskResults/$taskName" $taskEvidence}}
if(Test-Path "$taskResults/particle-modes"){
 New-Item -ItemType Directory -Force "$taskEvidence/particle-modes" | Out-Null
 Copy-Item "$taskResults/particle-modes/*.json","$taskResults/particle-modes/*.log","$taskResults/particle-modes/*-command.txt" "$taskEvidence/particle-modes"
}
Copy-Item "$taskRoot/bin/launcher-selftest.txt" $taskEvidence
Copy-Item "$taskRoot/build/aftershock/Testing/Temporary/LastTest.log" "$taskEvidence/adapter-test-details.txt"
Write-Output 'Recorded measured results and verification evidence in docs.'
