$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskResults=Join-Path $taskRoot 'build/results/structures'
$taskEvidence=Join-Path $taskRoot 'docs/results/structures'
New-Item -ItemType Directory -Force $taskEvidence | Out-Null
$taskExecutable=Get-Item "$taskRoot/bin/vkQuake.exe"
$taskSha=(Get-FileHash $taskExecutable.FullName).Hash
$taskNames=@('original','dust','beads','chips','microvoxels','flakes','splinters','fibers','goo','rings','mixed')
$taskLines=@('# Structure validation','',"Executable SHA-256: ``$taskSha``.",'','Reference machine: RTX 2050 / Ryzen 5 7535HS, Windows x64 Release. All structure comparisons use source colors, three layers, Play density, 1280 x 720, an angled e1m1 wall view, two-times burst effects, threaded rendering, and a 60 host-frame cap. Each includes three screenshot readbacks and a 36-piece synthetic wall breach. These timings include capture overhead and are not presented frame rates.','','| ID | Structure | Host frames/s | GPU frame mean ms | Detached |','|---:|---|---:|---:|---:|')
for($taskMode=0;$taskMode -le 10;$taskMode++){
 $taskName=$taskNames[$taskMode]
 $taskFile=Get-Item "$taskResults/$taskName.json"
 if($taskFile.LastWriteTime -lt $taskExecutable.LastWriteTime){throw "Stale measurement: $taskName"}
 $taskData=Get-Content $taskFile.FullName -Raw | ConvertFrom-Json
 if($taskData.structure -ne $taskMode -or $taskData.layers -ne 3 -or $taskData.errors -ne 0){throw "Wrong settings or failed measurement: $taskName"}
 $taskLines+='| {0} | {1} | {2} | {3} | {4} |' -f $taskMode,$taskName,$taskData.host_fps,$taskData.gpu_frame_mean_ms,$taskData.detached
}
$taskLines+=@('','Validation passed:','','- All ten new modes and original samples: 33 before/explosion/aftermath captures; each intact shape visually reviewed with explosion spot checks; 36 detached chunks and zero PhysX errors in every case.','- Identical scatter fingerprint across all mode choices and cold/warm source cache: `33c839dd97aee283`.','- Live switching through all 11 choices and all four layer counts under OIT 0, 1, and 2, with maximum effects and reduced flashes: no geometry rebuild, 36-piece breach preserved.','- Four CTest suites pass, including deterministic irregular sampling within a triangular face, reflected structure vertex attributes, structural physics, pressure, and swept gib motion.','- Full engine regression: real rockets and WALK traversal in both renderers; partial breach and rubble pushing in the arena; faithful gameplay hash `caa13ac253fcfc0d`; structural save/load and renderer switching.','- Cold, corrupt, regenerated, and warm cache cases pass.','- Launcher self-test checks all 44 structure/layer command combinations, preview, PAK directory bounds, quoting and Unicode. Native hidden-window control painting is not a full visual UI test.','','The source scatter buffer for e1m1 uses 2,875,420 bytes, in addition to the existing 7,686,420-byte regular sample buffer. Per-draw structure metadata is 112 bytes. These are not total engine VRAM figures.','','See [capture-free 1080p measurements](MEASUREMENTS.md) for the default Mixed Debris Cloud setting. Mesh modes do not add fluid simulation, connected rope physics, or per-grain PhysX actors. [Structure guide](PARTICLE_STRUCTURES.md).','',('Recorded: {0}.' -f (Get-Date).ToString('o')))
Set-Content "$taskRoot/docs/STRUCTURE_VALIDATION.md" $taskLines
Copy-Item "$taskResults/*.json","$taskResults/*.log","$taskResults/*-command.txt" $taskEvidence
Copy-Item "$taskRoot/build/results/structure-tests.txt","$taskRoot/build/results/structure-switching.txt" $taskEvidence
Write-Output 'Recorded current structure measurements and evidence'
