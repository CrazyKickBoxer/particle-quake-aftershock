param([ValidateSet('Release')][string]$Configuration='Release')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskVS=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
$taskCMake=Join-Path $taskVS 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$taskMSBuild=Join-Path $taskVS 'MSBuild\Current\Bin\MSBuild.exe'
Push-Location $taskRoot
try {
 if(!(Test-Path build/physx/lib/bin/win.x86_64.vc143.md/release/PhysX_static.lib) -or !(Test-Path tools/vulkan/bin/glslangValidator.exe)){throw 'Run scripts/bootstrap.ps1 to prepare the pinned dependencies.'}
 & $taskCMake -S . -B build/aftershock -G 'Visual Studio 17 2022' -A x64
 if($LASTEXITCODE){throw 'Aftershock configuration failed'}
 & $taskCMake --build build/aftershock --config $Configuration --parallel 4
 if($LASTEXITCODE){throw 'Aftershock build failed'}
 & $taskMSBuild engine\Windows\VisualStudio\bintoc.vcxproj /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /v:minimal /nologo
 if($LASTEXITCODE){throw 'Shader embedding tool build failed'}
 foreach($taskShader in @('aftershock.vert','aftershock_structure.vert','aftershock.frag','aftershock_fx.vert','aftershock_alias.vert','aftershock_alias.frag')){
  & "$taskRoot\tools\vulkan\bin\glslangValidator.exe" --target-env vulkan1.1 -V "engine\Shaders\$taskShader" -o "build\$taskShader.spv"
  if($LASTEXITCODE){throw "Shader failed: $taskShader"}
  & "engine\Windows\VisualStudio\Build-bintoc\x64\Release\bintoc.exe" "build\$taskShader.spv" "${taskShader}_spv" "engine\Shaders\Compiled\$taskShader.c"
  if($LASTEXITCODE){throw 'Shader embedding failed'}
 }
 foreach($taskPass in 0..16) {
  & "$taskRoot/tools/vulkan/bin/glslangValidator.exe" --target-env vulkan1.1 -V "-DHP_PASS=$taskPass" engine/Shaders/holo_physics.comp -o "build/holo_$taskPass.spv"
  if($LASTEXITCODE){throw 'Holo compute shader failed'}
  & "engine/Windows/VisualStudio/Build-bintoc/x64/Release/bintoc.exe" "build/holo_$taskPass.spv" "holo_${taskPass}_spv" "engine/Shaders/Compiled/holo_$taskPass.c"
 }
 foreach($taskHolo in @(@{Name='splat';Source='aftershock.vert'},@{Name='structure';Source='aftershock_structure.vert'},@{Name='fragment';Source='aftershock.frag'},@{Name='gore_vert';Source='holo_gore.vert'},@{Name='gore_frag';Source='holo_gore.frag'})) {
  $taskName=$taskHolo.Name
  & "$taskRoot/tools/vulkan/bin/glslangValidator.exe" --target-env vulkan1.1 -V '-DHOLO_PHYSICS=1' "engine/Shaders/$($taskHolo.Source)" -o "build/holo_$taskName.spv"
  if($LASTEXITCODE){throw 'Holo vertex shader failed'}
  & "engine/Windows/VisualStudio/Build-bintoc/x64/Release/bintoc.exe" "build/holo_$taskName.spv" "holo_${taskName}_spv" "engine/Shaders/Compiled/holo_$taskName.c"
 }
 foreach($taskVariant in 'single','msaa') {
  $taskDefines=@();if($taskVariant -eq 'msaa'){$taskDefines+='-DDEPTH_MS'}
  & "$taskRoot/tools/vulkan/bin/glslangValidator.exe" --target-env vulkan1.1 -V @taskDefines engine/Shaders/aftershock_fidelity.comp -o "build/fidelity_$taskVariant.spv"
  if($LASTEXITCODE){throw 'Fidelity shader failed'}
  & "engine/Windows/VisualStudio/Build-bintoc/x64/Release/bintoc.exe" "build/fidelity_$taskVariant.spv" "fidelity_${taskVariant}_spv" "engine/Shaders/Compiled/fidelity_$taskVariant.c"
 }
 foreach($taskBits in '8bit','10bit') { foreach($taskScale in '','_scale','_scale_sops') {
  $taskName="screen_effects_$taskBits$taskScale.comp"
  $taskDefines=@()
  if($taskBits -eq '10bit'){$taskDefines+='-DUSE_10BIT'}
  if($taskScale){$taskDefines+='-DSCALING'}
  if($taskScale -eq '_scale_sops'){$taskDefines+='-DUSE_SUBGROUP_OPS'}
  & "$taskRoot\tools\vulkan\bin\glslangValidator.exe" --target-env vulkan1.1 -V @taskDefines engine/Shaders/screen_effects.comp -o "build/$taskName.spv"
  if($LASTEXITCODE){throw "Screen shader failed: $taskName"}
  & "engine\Windows\VisualStudio\Build-bintoc\x64\Release\bintoc.exe" "build/$taskName.spv" "${taskName}_spv" "engine/Shaders/Compiled/Release/$taskName.c"
  if($LASTEXITCODE){throw 'Screen shader embedding failed'}
 }}
 & $taskMSBuild engine\Windows\VisualStudio\vkquake.sln /m:4 "/p:Configuration=$Configuration" /p:Platform=x64 /p:PlatformToolset=v143 "/p:VULKAN_SDK=$taskRoot\tools\vulkan" /v:minimal /nologo
 if($LASTEXITCODE){throw 'vkQuake build failed'}
 New-Item -ItemType Directory -Force bin | Out-Null
 Copy-Item "engine\Windows\VisualStudio\Build-vkQuake\x64\$Configuration\*.exe" bin
 Copy-Item "engine\Windows\VisualStudio\Build-vkQuake\x64\$Configuration\*.dll" bin
 # An open launcher does not need replacement when its build is unchanged.
 $taskLauncherSource="build/aftershock/$Configuration/vkquake_launcher.exe"
 foreach($taskLauncherDestination in @('bin/vkquake_launcher.exe','bin/ParticleQuakeLauncher.exe')) {
  if(!(Test-Path $taskLauncherDestination) -or (Get-FileHash $taskLauncherSource).Hash -ne (Get-FileHash $taskLauncherDestination).Hash){
   try { Copy-Item $taskLauncherSource $taskLauncherDestination -ErrorAction Stop }
   catch { Write-Warning "Could not replace $taskLauncherDestination (it may be open). The rebuilt launcher is at $taskLauncherSource. $($_.Exception.Message)" }
  }
 }
} finally {Pop-Location}

