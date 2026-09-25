; Inno Setup script for melo - LZMA2/max compression installer.
; Built in CI: iscc /O<outdir> scripts/melo.iss (dist/ prepared by the deploy step)
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif

[Setup]
; fixed, so a newer installer upgrades the old install in place
AppId={{54490D21-C3EC-42E2-8B30-D6314E8FA569}
AppName=melo
AppVersion={#AppVersion}
AppPublisher=melo
DefaultDirName={autopf}\melo
DisableProgramGroupPage=yes
Compression=lzma2/max
SolidCompression=yes
OutputBaseFilename=melo-setup-win64
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequiredOverridesAllowed=dialog
UninstallDisplayIcon={app}\melo.exe
; GPL-3.0 s4/s5: the licence is conveyed with the work. The installer showed
; none, and no file in dist\ carried one either.
LicenseFile=..\LICENSE
SetupIconFile=..\resources\melo.ico

[InstallDelete]
; an upgrade replaces these folders whole: files a newer build dropped must go
Type: filesandordirs; Name: "{app}\gst-plugins"
Type: filesandordirs; Name: "{app}\melo-qml"
Type: filesandordirs; Name: "{app}\plugin-qml-imports"
Type: filesandordirs; Name: "{app}\qml"
Type: filesandordirs; Name: "{app}\sidecar"

[Files]
Source: "..\dist\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion

[Icons]
Name: "{autoprograms}\melo"; Filename: "{app}\melo.exe"
Name: "{autodesktop}\melo"; Filename: "{app}\melo.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; Flags: unchecked

[Run]
Filename: "{app}\melo.exe"; Description: "Launch melo"; Flags: nowait postinstall skipifsilent
