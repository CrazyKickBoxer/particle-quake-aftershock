param([string]$Version='v0.1.0')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskStamp=Get-Date -Format 'yyyyMMdd-HHmmss'
$taskDistribution=Join-Path $taskRoot 'build/dist'
$taskDestination=Join-Path $taskDistribution "Aftershock-$taskStamp"
$taskSource=Join-Path $taskDistribution "Aftershock-source-$taskStamp"
New-Item -ItemType Directory -Force $taskDestination,"$taskDestination/notices",$taskSource | Out-Null
$taskRuntime=@('SDL3.dll','libFLAC-8.dll','libmpg123-0.dll','libogg-0.dll','libopus-0.dll','libopusfile-0.dll','libvorbis-0.dll','libvorbisfile-3.dll','libxmp.dll')
Copy-Item "$taskRoot/bin/vkQuake.exe","$taskRoot/bin/vkquake_launcher.exe" $taskDestination
foreach($taskFile in $taskRuntime){Copy-Item "$taskRoot/bin/$taskFile" $taskDestination}
Copy-Item "$taskRoot/README.txt" $taskDestination
Copy-Item "$taskRoot/HOLO.md" $taskDestination
Copy-Item "$taskRoot/engine/LICENSE.txt" "$taskDestination/LICENSE.txt"
Copy-Item "$taskRoot/NOTICE.md" "$taskDestination/NOTICE.md"
# docs/results/ is raw local test-run evidence (command lines/logs baked with this machine's
# absolute paths) - never ship it. Everything else in docs/ is player/dev-facing reference.
Copy-Item "$taskRoot/docs" "$taskDestination/docs" -Recurse -Exclude 'results'
Remove-Item "$taskDestination/docs/results" -Recurse -Force -ErrorAction SilentlyContinue
Copy-Item "$taskRoot/engine/LICENSE.txt" "$taskDestination/notices/vkQuake-GPL.txt"
Copy-Item "$taskRoot/engine/Quake/mimalloc/LICENSE" "$taskDestination/notices/mimalloc-MIT.txt"
Copy-Item "$taskRoot/engine/Windows/SDL3/LICENSE.txt" "$taskDestination/notices/SDL3-zlib.txt"
Copy-Item "$taskRoot/dependencies/physx/LICENSE.md" "$taskDestination/notices/PhysX-BSD.md"
Copy-Item "$taskRoot/dependencies/physx/blast/PACKAGE-LICENSES/blast-sdk-LICENSE.md" "$taskDestination/notices/Blast-BSD.md"
# Snapshot the actual modified sources, never an unmodified upstream replacement.
foreach($taskFolder in @('src','tests','scripts')){Copy-Item "$taskRoot/$taskFolder" "$taskSource/$taskFolder" -Recurse}
Copy-Item "$taskRoot/docs" "$taskSource/docs" -Recurse -Exclude 'results'
Remove-Item "$taskSource/docs/results" -Recurse -Force -ErrorAction SilentlyContinue
Copy-Item "$taskRoot/engine/LICENSE.txt" "$taskSource/LICENSE.txt"
foreach($taskFile in @('README.md','NOTICE.md','HOLO.md','CMakeLists.txt','.gitignore','PARTICLE_QUAKE_BUILD_PROMPT.md')){Copy-Item "$taskRoot/$taskFile" $taskSource}
if(Test-Path "$taskRoot/engine/.git"){
 $taskEngineFiles=@(git -C "$taskRoot/engine" ls-files --cached --others --exclude-standard) | Sort-Object -Unique
 if($LASTEXITCODE){throw 'Could not enumerate the engine source snapshot'}
}else{$taskEngineFiles=@(Get-ChildItem "$taskRoot/engine" -Recurse -File | ForEach-Object {$_.FullName.Substring((Join-Path $taskRoot 'engine').Length+1).Replace('\','/')})}
foreach($taskRelative in $taskEngineFiles){
 if($taskRelative -match '\.(pak|wad|bsp|mdl|spr|lmp|sav|pqas|png|jpg|jpeg|gif|webp)$' -or $taskRelative -match '(^|/)(Build-[^/]+|id1|captures)/'){continue}
 if($taskRelative -match '(?i)(libmad|libmikmod).*(dll|lib|a)$'){continue}
 $taskInput=Join-Path "$taskRoot/engine" $taskRelative
 if(!(Test-Path -LiteralPath $taskInput -PathType Leaf)){continue}
 $taskOutput=Join-Path "$taskSource/engine" $taskRelative
 New-Item -ItemType Directory -Force (Split-Path -Parent $taskOutput) | Out-Null
 Copy-Item -LiteralPath $taskInput -Destination $taskOutput
}
# The MSVC project consumes generated shader C sources. Git ignores these;
# include the exact Release modules used by this build in the source snapshot.
New-Item -ItemType Directory -Force "$taskSource/engine/Shaders/Compiled/Release" | Out-Null
Copy-Item "$taskRoot/engine/Shaders/Compiled/*.c" "$taskSource/engine/Shaders/Compiled"
Copy-Item "$taskRoot/engine/Shaders/Compiled/Release/*.c" "$taskSource/engine/Shaders/Compiled/Release"
Copy-Item "$taskDestination/notices" "$taskSource/notices" -Recurse
New-Item -ItemType Directory -Force "$taskSource/dependency-source" | Out-Null
if(!(Test-Path "$taskRoot/dependencies/mpg123-1.22.4.tar.bz2")){Invoke-WebRequest 'https://www.mpg123.de/download/mpg123-1.22.4.tar.bz2' -OutFile "$taskRoot/dependencies/mpg123-1.22.4.tar.bz2"}
if((Get-FileHash "$taskRoot/dependencies/mpg123-1.22.4.tar.bz2").Hash -ne '5069E02E50138600F10CC5F7674E44E9BF6F1930AF81D0E1D2F869B3C0EE40D2'){throw 'mpg123 source checksum mismatch'}
Copy-Item "$taskRoot/dependencies/mpg123-1.22.4.tar.bz2" "$taskSource/dependency-source"
$taskManifest=[ordered]@{created=(Get-Date).ToString('o');upstream='4519111e8ee33e6d7570c0b483f0f45c2fac6d52';physx='2264315594478a9aa0bda3464761a666fa107d76';configuration='Windows x64 Release, CPU PhysX';executableSha256=(Get-FileHash "$taskDestination/vkQuake.exe").Hash;files=@()}
$taskManifest.files=@(Get-ChildItem $taskSource -Recurse -File | ForEach-Object {[ordered]@{path=$_.FullName.Substring($taskSource.Length+1).Replace('\','/');sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
$taskManifest | ConvertTo-Json -Depth 5 | Set-Content "$taskDestination/build-manifest.json"
Copy-Item "$taskDestination/build-manifest.json" $taskSource
foreach($taskDirectory in @($taskDestination,$taskSource)){
 $taskForbidden=Get-ChildItem $taskDirectory -Recurse -File | Where-Object {$_.Extension -match '^\.(pak|wad|bsp|mdl|spr|lmp|pqas|sav|cache|png|jpg|jpeg|gif|webp)$'}
 if($taskForbidden){throw "Package contains assets, captures or saves: $($taskForbidden.FullName -join ', ')"}
}
$taskRuntimeZip="$taskDistribution/ParticleQuake-Aftershock-$Version-win64.zip"
$taskSourceZip="$taskDistribution/ParticleQuake-Aftershock-$Version-source.zip"
Compress-Archive -Path "$taskDestination/*" -DestinationPath $taskRuntimeZip -Force
Compress-Archive -Path "$taskSource/*" -DestinationPath $taskSourceZip -Force
Write-Output "Runtime: $taskRuntimeZip"
Write-Output "Runtime SHA-256: $((Get-FileHash $taskRuntimeZip).Hash)"
Write-Output "Source: $taskSourceZip"
Write-Output "Source SHA-256: $((Get-FileHash $taskSourceZip).Hash)"
Write-Output "Manifest: $taskDestination/build-manifest.json"
