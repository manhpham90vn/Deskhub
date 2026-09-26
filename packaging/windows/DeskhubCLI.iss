#ifndef AppVersion
  #error AppVersion must be passed to ISCC with /DAppVersion=...
#endif

[Setup]
AppId={{F62A9A7E-FC4C-4D1E-983B-03B6D67B8209}
AppName=DeskHub CLI
AppVersion={#AppVersion}
AppPublisher=ManhPham
DefaultDirName={localappdata}\Programs\DeskHub CLI
PrivilegesRequired=lowest
ChangesEnvironment=yes
OutputDir=..\..\out\installer
OutputBaseFilename=DeskHub-CLI-Setup
SetupIconFile=..\..\client\windows\win32\Deskhub.ico
Compression=lzma2
SolidCompression=yes
UninstallDisplayIcon={app}\deskhub-cli.exe

[Files]
Source: "..\..\out\build\x64-release\client\cli\deskhub-cli.exe"; DestDir: "{app}"; Flags: ignoreversion

[Code]
const
  PathKey = 'Environment';
  MarkerKey = 'Software\ManhPham\DeskHubCLI';

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

procedure CurStepChanged(CurStep: TSetupStep);
var
  Existing, NewPath, Directory: String;
begin
  if CurStep <> ssPostInstall then Exit;
  Directory := ExpandConstant('{app}');
  if not RegQueryStringValue(HKCU, PathKey, 'Path', Existing) then
    Existing := '';
  if ContainsPath(Existing, Directory) then Exit;

  NewPath := Existing;
  if NewPath <> '' then
  begin
    if NewPath[Length(NewPath)] <> ';' then NewPath := NewPath + ';';
  end;
  NewPath := NewPath + Directory;
  if not RegWriteStringValue(HKCU, MarkerKey, 'PathAdded', '1') then
    RaiseException('Could not record the DeskHub CLI PATH change.');
  if not RegWriteExpandStringValue(HKCU, PathKey, 'Path', NewPath) then
  begin
    RegDeleteValue(HKCU, MarkerKey, 'PathAdded');
    RegDeleteKeyIfEmpty(HKCU, MarkerKey);
    RaiseException('Could not add DeskHub CLI to your user PATH.');
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Existing, NewPath, Marker, Directory: String;
begin
  if CurUninstallStep <> usPostUninstall then Exit;
  if not RegQueryStringValue(HKCU, MarkerKey, 'PathAdded', Marker) then Exit;
  if Marker <> '1' then Exit;
  Directory := ExpandConstant('{app}');
  if RegQueryStringValue(HKCU, PathKey, 'Path', Existing) then
  begin
    NewPath := RemovePath(Existing, Directory);
    if NewPath <> Existing then
    begin
      if NewPath = '' then
        RegDeleteValue(HKCU, PathKey, 'Path')
      else
        RegWriteExpandStringValue(HKCU, PathKey, 'Path', NewPath);
    end;
  end;
  RegDeleteValue(HKCU, MarkerKey, 'PathAdded');
  RegDeleteKeyIfEmpty(HKCU, MarkerKey);
end;
