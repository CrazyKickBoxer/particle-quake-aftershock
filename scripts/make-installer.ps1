# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Josh Nicholls
param([string]$Version='v0.1.0')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
Push-Location $taskRoot
try {
 $taskIscc="$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
 if(!(Test-Path $taskIscc)){$taskIscc=(Get-Command ISCC.exe -ErrorAction SilentlyContinue).Source}
 if(!$taskIscc -or !(Test-Path $taskIscc)){throw 'Inno Setup 6 (ISCC.exe) not found. Install it: winget install --id JRSoftware.InnoSetup -e'}
 # Reuse package.ps1 for the runtime file layout - single source of truth for
 # what a release contains, rather than duplicating that copy/exclude logic here.
 & (Join-Path $PSScriptRoot 'package.ps1') -Version $Version
 if($LASTEXITCODE){throw 'package.ps1 failed'}
 $taskRuntimeFolder=Get-ChildItem "$taskRoot/build/dist" -Directory |
  Where-Object {$_.Name -like 'Aftershock-*' -and $_.Name -notlike 'Aftershock-source-*'} |
  Sort-Object LastWriteTime -Descending | Select-Object -First 1
 if(!$taskRuntimeFolder){throw 'Could not find a packaged runtime folder under build/dist'}
 & $taskIscc "/DMyAppVersion=$Version" "/DSourceDir=$($taskRuntimeFolder.FullName)" `
  "/DOutputDir=$taskRoot/build/dist" "$PSScriptRoot/installer.iss"
 if($LASTEXITCODE){throw 'Inno Setup compilation failed'}
 $taskSetup="$taskRoot/build/dist/ParticleQuake-Aftershock-$Version-setup.exe"
 Write-Output "Installer: $taskSetup"
 Write-Output "SHA-256: $((Get-FileHash $taskSetup).Hash)"
} finally { Pop-Location }
