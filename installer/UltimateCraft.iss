; Inno Setup script for the Windows installer (built by CI: ISCC installer\UltimateCraft.iss after tools/package.sh windows)
#define AppVer "5.1.0"
[Setup]
AppId={{6E2C1F0A-5B1D-4D7E-9C55-7A1D2B3C4E50}
AppName=UltimateCraft
AppVersion={#AppVer}
AppPublisher=UltimateCraft
DefaultDirName={autopf}\UltimateCraft
DefaultGroupName=UltimateCraft
OutputDir=..\dist
OutputBaseFilename=UltimateCraft-Setup-{#AppVer}
Compression=lzma2
SolidCompression=yes
LicenseFile=..\EULA.md
PrivilegesRequired=lowest
ArchitecturesInstallIn64BitMode=x64compatible
[Files]
Source: "..\dist\UltimateCraft\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion
[Icons]
Name: "{group}\UltimateCraft"; Filename: "{app}\ultimatecraft.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\UltimateCraft"; Filename: "{app}\ultimatecraft.exe"; WorkingDir: "{app}"; Tasks: desktopicon
[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Flags: unchecked
[Run]
Filename: "{app}\ultimatecraft.exe"; Description: "Play UltimateCraft"; WorkingDir: "{app}"; Flags: nowait postinstall skipifsilent
