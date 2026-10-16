#ifndef AppVersion
  #error AppVersion must be passed to ISCC with /DAppVersion=...
#endif

[Setup]
AppId={{A54D0B95-4B42-4A02-A9C6-EC43BA13034E}
AppName=DeskHub
AppVersion={#AppVersion}
AppPublisher=ManhPham
DefaultDirName={autopf}\DeskHub
DefaultGroupName=DeskHub
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\..\out\installer
OutputBaseFilename=DeskHub-Setup
SetupIconFile=..\..\client\windows\win32\Deskhub.ico
Compression=lzma2
SolidCompression=yes
UninstallDisplayIcon={app}\Deskhub.exe

[Files]
Source: "..\..\out\build\x64-release\client\windows\win32\Deskhub.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\out\build\x64-release\client\windows\win32\DeskhubService.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\licenses\BSD-2-Clause-quiche.txt"; DestDir: "{app}\licenses"; Flags: ignoreversion
Source: "..\..\licenses\BoringSSL.txt"; DestDir: "{app}\licenses"; Flags: ignoreversion
Source: "..\..\licenses\Apache-2.0.txt"; DestDir: "{app}\licenses"; Flags: ignoreversion
Source: "..\..\licenses\rust-crates.txt"; DestDir: "{app}\licenses"; Flags: ignoreversion
Source: "..\..\licenses\BSD-3-Clause-opus.txt"; DestDir: "{app}\licenses"; Flags: ignoreversion
Source: "..\..\licenses\wxWindows.txt"; DestDir: "{app}\licenses"; Flags: ignoreversion
Source: "..\..\licenses\wxWidgets-bundled.txt"; DestDir: "{app}\licenses"; Flags: ignoreversion
Source: "..\..\licenses\LGPL-2.1.txt"; DestDir: "{app}\licenses"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\DeskHub"; Filename: "{app}\Deskhub.exe"
Name: "{autodesktop}\DeskHub"; Filename: "{app}\Deskhub.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"

[Run]
Filename: "{app}\DeskhubService.exe"; Parameters: "--install"; StatusMsg: "Installing the DeskHub Secure Desktop service..."; Flags: runhidden waituntilterminated
Filename: "{app}\Deskhub.exe"; Description: "Launch DeskHub"; Flags: nowait postinstall skipifsilent shellexec

[UninstallRun]
Filename: "{app}\DeskhubService.exe"; Parameters: "--uninstall"; RunOnceId: "RemoveSecureDesktopService"; Flags: runhidden waituntilterminated

[Code]
const
  PerUserUninstallKey = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{A54D0B95-4B42-4A02-A9C6-EC43BA13034E}_is1';
  AutostartTaskName = 'Deskhub';
  PerUserUninstallWaitMs = 30000;
  PerUserUninstallPollMs = 250;

var
  RemovedPerUserInstall: Boolean;

function RunHidden(const FileName, Params: String): Integer;
var
  ResultCode: Integer;
begin
  if Exec(FileName, Params, '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then
    Result := ResultCode
  else
    Result := -1;
end;

procedure StopExistingService();
var
  ServiceExe: String;
begin
  ServiceExe := ExpandConstant('{app}\DeskhubService.exe');
  if FileExists(ServiceExe) then
    RunHidden(ServiceExe, '--uninstall');
end;

procedure WaitForPerUserUninstall();
var
  Waited: Integer;
begin
  Waited := 0;
  while RegKeyExists(HKCU, PerUserUninstallKey) and (Waited < PerUserUninstallWaitMs) do
  begin
    Sleep(PerUserUninstallPollMs);
    Waited := Waited + PerUserUninstallPollMs;
  end;
  if RegKeyExists(HKCU, PerUserUninstallKey) then
    Log('The earlier per-user DeskHub install is still registered; continuing anyway.');
end;

procedure RemovePerUserInstall();
var
  Uninstaller: String;
begin
  if not RegQueryStringValue(HKCU, PerUserUninstallKey, 'UninstallString', Uninstaller) then
    Exit;
  Uninstaller := RemoveQuotes(Uninstaller);
  if not FileExists(Uninstaller) then
    Exit;
  Log('Removing the earlier per-user DeskHub install: ' + Uninstaller);
  RunHidden(Uninstaller, '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART');
  WaitForPerUserUninstall();
  RemovedPerUserInstall := True;
end;

procedure PointAutostartAtNewPath();
var
  Schtasks: String;
begin
  Schtasks := ExpandConstant('{sys}\schtasks.exe');
  if RunHidden(Schtasks, '/Query /TN ' + AutostartTaskName) <> 0 then
    Exit;
  RunHidden(Schtasks, '/Create /F /TN ' + AutostartTaskName + ' /SC ONLOGON /RL HIGHEST /TR "\"' +
    ExpandConstant('{app}\Deskhub.exe') + '\""');
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  StopExistingService();
  RemovePerUserInstall();
  Result := '';
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssPostInstall) and RemovedPerUserInstall then
    PointAutostartAtNewPath();
end;
