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
ChangesEnvironment=yes
OutputDir=..\..\out\installer
OutputBaseFilename=DeskHub-Setup
SetupIconFile=..\..\client\windows\win32\Deskhub.ico
Compression=lzma2
SolidCompression=yes
UninstallDisplayIcon={app}\Deskhub.exe

[Files]
Source: "..\..\out\build\x64-release\client\windows\win32\Deskhub.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\out\build\x64-release\client\windows\win32\DeskhubService.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\out\build\x64-release\client\cli\deskhub-cli.exe"; DestDir: "{app}"; Flags: ignoreversion
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
  PerUserAppUninstallKey = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{A54D0B95-4B42-4A02-A9C6-EC43BA13034E}_is1';
  PerUserCliUninstallKey = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{F62A9A7E-FC4C-4D1E-983B-03B6D67B8209}_is1';
  MachinePathKey = 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment';
  MarkerKey = 'Software\ManhPham\DeskHub';
  AutostartTaskName = 'Deskhub';
  PerUserUninstallWaitMs = 30000;
  PerUserUninstallPollMs = 250;

var
  RemovedPerUserApp: Boolean;

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

procedure WaitForPerUserUninstall(const UninstallKey: String);
var
  Waited: Integer;
begin
  Waited := 0;
  while RegKeyExists(HKCU, UninstallKey) and (Waited < PerUserUninstallWaitMs) do
  begin
    Sleep(PerUserUninstallPollMs);
    Waited := Waited + PerUserUninstallPollMs;
  end;
  if RegKeyExists(HKCU, UninstallKey) then
    Log('The earlier per-user install under ' + UninstallKey + ' is still registered; continuing anyway.');
end;

function RemovePerUserInstall(const UninstallKey: String): Boolean;
var
  Uninstaller: String;
begin
  Result := False;
  if not RegQueryStringValue(HKCU, UninstallKey, 'UninstallString', Uninstaller) then
    Exit;
  Uninstaller := RemoveQuotes(Uninstaller);
  if not FileExists(Uninstaller) then
    Exit;
  Log('Removing the earlier per-user install: ' + Uninstaller);
  RunHidden(Uninstaller, '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART');
  WaitForPerUserUninstall(UninstallKey);
  Result := True;
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

function SamePath(const Left, Right: String): Boolean;
begin
  Result := SameText(RemoveBackslashUnlessRoot(Trim(Left)),
    RemoveBackslashUnlessRoot(Trim(Right)));
end;

function ContainsPath(const Value, Directory: String): Boolean;
var
  Remaining, Part: String;
  Separator: Integer;
begin
  Result := False;
  Remaining := Value;
  repeat
    Separator := Pos(';', Remaining);
    if Separator = 0 then
      Part := Remaining
    else
    begin
      Part := Copy(Remaining, 1, Separator - 1);
      Delete(Remaining, 1, Separator);
    end;
    if SamePath(Part, Directory) then
    begin
      Result := True;
      Exit;
    end;
  until Separator = 0;
end;

function RemovePath(const Value, Directory: String): String;
var
  Remaining, Part: String;
  Separator: Integer;
  Removed, FirstPart: Boolean;
begin
  Result := '';
  Remaining := Value;
  Removed := False;
  FirstPart := True;
  repeat
    Separator := Pos(';', Remaining);
    if Separator = 0 then
      Part := Remaining
    else
    begin
      Part := Copy(Remaining, 1, Separator - 1);
      Delete(Remaining, 1, Separator);
    end;
    if not Removed and SamePath(Part, Directory) then
      Removed := True
    else
    begin
      if FirstPart then
        Result := Part
      else
        Result := Result + ';' + Part;
      FirstPart := False;
    end;
  until Separator = 0;
end;

function EndsWithDirectory(const Value, Directory: String): Boolean;
var
  Start: Integer;
begin
  Result := False;
  Start := Length(Value) - Length(Directory) + 1;
  if Start < 2 then Exit;
  if Value[Start - 1] <> ';' then Exit;
  Result := SameText(Copy(Value, Start, Length(Directory)), Directory);
end;

procedure AddToMachinePath();
var
  Existing, NewPath, Directory, Marker: String;
begin
  Directory := ExpandConstant('{app}');
  if not RegQueryStringValue(HKLM, MachinePathKey, 'Path', Existing) then
    Existing := '';
  if ContainsPath(Existing, Directory) then Exit;

  NewPath := Existing;
  Marker := '1';
  if NewPath <> '' then
  begin
    if NewPath[Length(NewPath)] = ';' then
      Marker := '2'
    else
      NewPath := NewPath + ';';
  end;
  NewPath := NewPath + Directory;
  if not RegWriteStringValue(HKLM, MarkerKey, 'PathAdded', Marker) then
    RaiseException('Could not record the DeskHub PATH change.');
  if not RegWriteExpandStringValue(HKLM, MachinePathKey, 'Path', NewPath) then
  begin
    RegDeleteValue(HKLM, MarkerKey, 'PathAdded');
    RegDeleteKeyIfEmpty(HKLM, MarkerKey);
    RaiseException('Could not add DeskHub to the system PATH.');
  end;
end;

procedure RemoveFromMachinePath();
var
  Existing, NewPath, Marker, Directory: String;
begin
  if not RegQueryStringValue(HKLM, MarkerKey, 'PathAdded', Marker) then Exit;
  if (Marker <> '1') and (Marker <> '2') then Exit;
  Directory := ExpandConstant('{app}');
  if RegQueryStringValue(HKLM, MachinePathKey, 'Path', Existing) then
  begin
    if (Marker = '2') and EndsWithDirectory(Existing, Directory) then
      NewPath := Copy(Existing, 1, Length(Existing) - Length(Directory))
    else
      NewPath := RemovePath(Existing, Directory);
    if NewPath <> Existing then
    begin
      if NewPath = '' then
        RegDeleteValue(HKLM, MachinePathKey, 'Path')
      else
        RegWriteExpandStringValue(HKLM, MachinePathKey, 'Path', NewPath);
    end;
  end;
  RegDeleteValue(HKLM, MarkerKey, 'PathAdded');
  RegDeleteKeyIfEmpty(HKLM, MarkerKey);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  StopExistingService();
  RemovedPerUserApp := RemovePerUserInstall(PerUserAppUninstallKey);
  RemovePerUserInstall(PerUserCliUninstallKey);
  Result := '';
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep <> ssPostInstall then Exit;
  if RemovedPerUserApp then
    PointAutostartAtNewPath();
  AddToMachinePath();
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
    RemoveFromMachinePath();
end;
