#ifndef AppVersion
  #error AppVersion must be passed to ISCC with /DAppVersion=...
#endif

[Setup]
AppId={{A54D0B95-4B42-4A02-A9C6-EC43BA13034E}
AppName=DeskHub
AppVersion={#AppVersion}
AppPublisher=ManhPham
DefaultDirName={localappdata}\Programs\DeskHub
DefaultGroupName=DeskHub
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir=..\..\out\installer
OutputBaseFilename=DeskHub-Setup
SetupIconFile=..\..\client\windows\win32\Deskhub.ico
Compression=lzma2
SolidCompression=yes
UninstallDisplayIcon={app}\Deskhub.exe

[Files]
Source: "..\..\out\build\x64-release\client\windows\win32\Deskhub.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\DeskHub"; Filename: "{app}\Deskhub.exe"
Name: "{autodesktop}\DeskHub"; Filename: "{app}\Deskhub.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"

[Run]
Filename: "{app}\Deskhub.exe"; Description: "Launch DeskHub"; Flags: nowait postinstall skipifsilent
