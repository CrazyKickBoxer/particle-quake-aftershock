param([switch]$RebuildPhysics)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskVS=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$taskVS){throw 'Install Visual Studio 2022 Build Tools with Desktop development with C++ and CMake.'}
$taskCMake=Join-Path $taskVS 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
Push-Location $taskRoot
try {
 if(!(Test-Path engine/Quake/aftershock.c)){throw 'This source checkout is incomplete: engine/Quake/aftershock.c is required.'}
 New-Item -ItemType Directory -Force dependencies,tools,build | Out-Null
 if(!(Test-Path dependencies/physx/.git)){
  git clone --depth 1 --branch 107.0-physx-5.6.0 https://github.com/NVIDIA-Omniverse/PhysX.git dependencies/physx
  if($LASTEXITCODE){throw 'PhysX download failed'}
 }
 $taskRevision=git -C dependencies/physx rev-parse HEAD
 if($taskRevision -ne '2264315594478a9aa0bda3464761a666fa107d76'){throw 'PhysX checkout has a different revision. Preserve your changes and supply the pinned SDK.'}
 $taskPhysicsCMake=Join-Path $taskRoot 'dependencies/physx/physx/source/compiler/cmake/windows/CMakeLists.txt'
 $taskContent=[IO.File]::ReadAllText($taskPhysicsCMake)
 $taskOld='IF(PX_COPY_EXTERNAL_DLL OR PUBLIC_RELEASE)'
 $taskNew='IF(PX_COPY_EXTERNAL_DLL OR (PUBLIC_RELEASE AND PX_BUILDSNIPPETS))'
 if($taskContent.Contains($taskOld)){[IO.File]::WriteAllText($taskPhysicsCMake,$taskContent.Replace($taskOld,$taskNew))}
 elseif(!$taskContent.Contains($taskNew)){throw 'PhysX CMake adjustment no longer matches the pinned SDK'}
 if(!(Test-Path tools/vulkan/bin/glslangValidator.exe)){
  $taskArchive=Join-Path $taskRoot 'tools/glslang.zip'
  if(!(Test-Path $taskArchive)){Invoke-WebRequest 'https://github.com/KhronosGroup/glslang/releases/download/16.5.0/glslang-16.5.0-windows-x86_64-release.zip' -OutFile $taskArchive}
  if((Get-FileHash $taskArchive -Algorithm SHA256).Hash -ne '06B71298B750268C127F2EE7AE0EF7525E2068120C6C8A3A08B2F58CA6F325CE'){throw 'Shader compiler archive checksum mismatch'}
  Expand-Archive -LiteralPath $taskArchive -DestinationPath tools/vulkan -Force
  Copy-Item tools/vulkan/bin/glslang.exe tools/vulkan/bin/glslangValidator.exe
 }
 if($RebuildPhysics -or !(Test-Path build/physx/lib/bin/win.x86_64.vc143.md/release/PhysX_static.lib)){
  & $taskCMake -S dependencies/physx/physx/compiler/public -B build/physx -G 'Visual Studio 17 2022' -A x64 "-DPHYSX_ROOT_DIR=$taskRoot/dependencies/physx/physx" -DTARGET_BUILD_PLATFORM=windows -DPX_GENERATE_STATIC_LIBRARIES=ON -DPX_GENERATE_GPU_PROJECTS=OFF -DPX_BUILDSNIPPETS=OFF -DPX_BUILDPVDRUNTIME=OFF -DNV_USE_STATIC_WINCRT=OFF -DNV_USE_DEBUG_WINCRT=ON "-DPX_OUTPUT_LIB_DIR=$taskRoot/build/physx/lib" "-DPX_OUTPUT_BIN_DIR=$taskRoot/build/physx/bin"
  if($LASTEXITCODE){throw 'PhysX configuration failed'}
  & $taskCMake --build build/physx --config Release --parallel 4
  if($LASTEXITCODE){throw 'PhysX compilation failed'}
 }
 & (Join-Path $PSScriptRoot 'build.ps1')
} finally {Pop-Location}
