; SPDX-License-Identifier: GPL-2.0-or-later
; Copyright (C) 2026 Josh Nicholls
; Built by scripts/make-installer.ps1, which passes MyAppVersion/SourceDir/
; OutputDir via /D defines. The defaults below only matter if you run ISCC
; directly against this file for a quick local test.
#ifndef MyAppVersion
#define MyAppVersion "0.1.0"
#endif
#ifndef SourceDir
#define SourceDir "..\build\dist\runtime"
#endif
#ifndef OutputDir
#define OutputDir "..\build\dist"
#endif
#define MyAppName "Particle Quake: Aftershock"
#define MyAppPublisher "Josh Nicholls"
#define MyAppURL "https://github.com/CrazyKickBoxer/particle-quake-aftershock"
#define MyAppExeName "vkquake_launcher.exe"

[Setup]
AppId={{9A4AD77E-4058-4A5D-A640-D823EECCEE0E}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
; No admin rights needed - this only ever writes inside the user's own
; profile (Programs/Start Menu), never system-wide. Avoids a UAC prompt.
PrivilegesRequired=lowest
DefaultDirName={userpf}\Particle Quake Aftershock
DefaultGroupName=Particle Quake Aftershock
DisableProgramGroupPage=yes
LicenseFile={#SourceDir}\LICENSE.txt
OutputDir={#OutputDir}
OutputBaseFilename=ParticleQuake-Aftershock-{#MyAppVersion}-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\vkQuake.exe
; This installs the engine only. It never bundles, downloads or asks for
; Quake game data - that stays the player's own responsibility, same as
; the plain zip release.

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion

[Icons]
Name: "{group}\Particle Quake Aftershock"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Particle Quake Aftershock"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\README.txt"; Description: "View the README (where to put your Quake data)"; Flags: postinstall shellexec skipifsilent unchecked
Filename: "{app}\{#MyAppExeName}"; Description: "Launch Particle Quake: Aftershock"; Flags: postinstall nowait skipifsilent
