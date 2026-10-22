; AudioZ - Inno-Setup-Skript fuer die Windows-Installation (Setup.exe)
; Aufruf (siehe .github/workflows/build.yml):
;   ISCC.exe /DAppVersion=0.1.0 /DSourceExe=..\build\src\app\AudioZ_artefacts\Release\AudioZ.exe installer\AudioZ.iss

#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef SourceExe
  #define SourceExe "..\build\src\app\AudioZ_artefacts\Release\AudioZ.exe"
#endif

[Setup]
; Die AppId identifiziert das Programm fuer Updates und Deinstallation. Nicht aendern!
AppId={{6F2A9C54-3D1B-4E7A-8B35-A0D7C1E94B62}
AppName=AudioZ
AppVersion={#AppVersion}
AppVerName=AudioZ {#AppVersion}
AppPublisher=AudioZ
DefaultDirName={autopf}\AudioZ
DefaultGroupName=AudioZ
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=AudioZ-Setup-{#AppVersion}
SetupIconFile=..\assets\icon.ico
UninstallDisplayIcon={app}\AudioZ.exe
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog
#if FileExists("..\LICENSE")
LicenseFile=..\LICENSE
#endif

[Languages]
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\THIRD-PARTY-NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion
#if FileExists("..\LICENSE")
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
#endif

[Icons]
Name: "{autoprograms}\AudioZ"; Filename: "{app}\AudioZ.exe"
Name: "{autodesktop}\AudioZ"; Filename: "{app}\AudioZ.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\AudioZ.exe"; Description: "{cm:LaunchProgram,AudioZ}"; Flags: nowait postinstall skipifsilent
