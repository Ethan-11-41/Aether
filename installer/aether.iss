; Aether installer script for Inno Setup 6.3 or newer (https://jrsoftware.org/isinfo.php).
;
; Normally run by build-windows.bat (locally and on GitHub), which passes:
;   /DAppVersion=0.1.0          version number (from the VERSION file)
;   /DStageDir=<folder>         folder holding Aether.exe and everything else to install
;   /DOutputDir=<folder>        where Aether-Setup-<version>.exe is written

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef StageDir
  #define StageDir "..\build\windows-release\stage"
#endif
#ifndef OutputDir
  #define OutputDir "..\build\installer"
#endif

[Setup]
; AppId identifies Aether to Windows (for upgrades and uninstalling). Never change it.
AppId={{07A6B952-BEAB-403D-9EAB-FAEF9E9C549C}
AppName=Aether
AppVersion={#AppVersion}
AppVerName=Aether {#AppVersion}
AppPublisher=The Aether authors
AppPublisherURL=https://github.com/Ethan-11-41/aether
AppSupportURL=https://github.com/Ethan-11-41/aether/issues
AppUpdatesURL=https://github.com/Ethan-11-41/aether/releases
AppCopyright=Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3.
DefaultDirName={autopf}\Aether
DefaultGroupName=Aether
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=Aether-Setup-{#AppVersion}
SetupIconFile=..\resources\icon\aether.ico
UninstallDisplayIcon={app}\Aether.exe
UninstallDisplayName=Aether
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
; 64-bit Windows 10 (version 1607) or newer, which is what JUCE 9 supports.
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.14393
; Installs for all users by default; the user may choose "just for me" instead (no admin needed).
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog
; Ask to close Aether if it is running while updating.
CloseApplications=yes
; Double-clicking a .aether project file opens it in Aether.
ChangesAssociations=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[InstallDelete]
; Versions before 0.2.1 accidentally installed JUCE's developer files; remove them when updating.
Type: filesandordirs; Name: "{app}\include"
Type: filesandordirs; Name: "{app}\lib"
Type: filesandordirs; Name: "{app}\bin"
; The recorded instruments are replaced as a whole, so ones renamed or removed don't linger.
Type: filesandordirs; Name: "{app}\Sounds"

[Files]
Source: "{#StageDir}\*"; Excludes: "\Sounds,\Sounds\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; The recorded instruments (Ogg Vorbis, already compressed), if the build has them.
Source: "{#StageDir}\Sounds\*"; DestDir: "{app}\Sounds"; Flags: ignoreversion recursesubdirs createallsubdirs nocompression skipifsourcedoesntexist

[Icons]
Name: "{autoprograms}\Aether"; Filename: "{app}\Aether.exe"
Name: "{autodesktop}\Aether"; Filename: "{app}\Aether.exe"; Tasks: desktopicon

[Registry]
; "HKA" = for all users when installed for everyone, or just this user otherwise.
Root: HKA; Subkey: "Software\Classes\.aether"; ValueType: string; ValueName: ""; ValueData: "Aether.Project"; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Aether.Project"; ValueType: string; ValueName: ""; ValueData: "Aether project"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\Aether.Project\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\Aether.exe,0"
Root: HKA; Subkey: "Software\Classes\Aether.Project\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\Aether.exe"" ""%1"""

[Run]
Filename: "{app}\Aether.exe"; Description: "{cm:LaunchProgram,Aether}"; Flags: nowait postinstall skipifsilent
