; AVIF-Master Inno Setup Script
; --------------------------------------------------

[Setup]
AppName=AVIF-Master
AppVersion=1.0
AppPublisher=Advanced Micro Conversions
DefaultDirName={autopf}\AVIFMaster
DefaultGroupName=AVIFMaster
UninstallDisplayIcon={app}\AVIFMaster.exe
Compression=lzma2/ultra
SolidCompression=yes
SetupIconFile=Setup.ico
SetupLogging=yes
OutputDir=Setup
OutputBaseFilename=AVIFMaster_Setup
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible

[Files]
Source: "build\AVIFMaster.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "Dependencies\libarchive\bin\libarchive.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "Setup.ico"; DestDir: "{app}"; Flags: ignoreversion
; Source: "Dependencies\7z.dll"; DestDir: "{app}"; Flags: ignoreversion
; Source: "Dependencies\avifenc.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\AVIF-Master"; Filename: "{app}\AVIFMaster.exe"; IconFilename: "{app}\Setup.ico"
Name: "{group}\AVIF-Master 제거"; Filename: "{uninstallexe}"
Name: "{commondesktop}\AVIF-Master"; Filename: "{app}\AVIFMaster.exe"; IconFilename: "{app}\Setup.ico"; Tasks: desktopicon

[Tasks]
Name: desktopicon; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Registry]
; --- 모든 파일: 혼합 다중 선택 지원 (jpg+png+webp+zip+7z 동시 선택) ---
; MultiSelectModel=Player: 선택 파일마다 슬레이브 프로세스 -> Master-Slave IPC 수집
Root: HKCR; Subkey: "*\shell\AVIFMaster"; ValueType: string; ValueName: ""; ValueData: "AVIF-Master로 고속 변환"; Flags: uninsdeletekey
Root: HKCR; Subkey: "*\shell\AVIFMaster"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\Setup.ico,0"; Flags: uninsdeletekey
Root: HKCR; Subkey: "*\shell\AVIFMaster"; ValueType: string; ValueName: "MultiSelectModel"; ValueData: "Player"; Flags: uninsdeletekey
Root: HKCR; Subkey: "*\shell\AVIFMaster\command"; ValueType: string; ValueName: ""; ValueData: """{app}\AVIFMaster.exe"" ""%1"""; Flags: uninsdeletekey

; --- 폴더 우클릭 (Directory\shell) ---
Root: HKCR; Subkey: "Directory\shell\AVIFMaster"; ValueType: string; ValueName: ""; ValueData: "AVIF-Master로 고속 변환"; Flags: uninsdeletekey
Root: HKCR; Subkey: "Directory\shell\AVIFMaster"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\Setup.ico,0"; Flags: uninsdeletekey
Root: HKCR; Subkey: "Directory\shell\AVIFMaster\command"; ValueType: string; ValueName: ""; ValueData: """{app}\AVIFMaster.exe"" ""%1"""; Flags: uninsdeletekey

; --- 폴더 배경 우클릭 (Directory\Background\shell) ---
Root: HKCR; Subkey: "Directory\Background\shell\AVIFMaster"; ValueType: string; ValueName: ""; ValueData: "AVIF-Master로 고속 변환"; Flags: uninsdeletekey
Root: HKCR; Subkey: "Directory\Background\shell\AVIFMaster"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\Setup.ico,0"; Flags: uninsdeletekey
Root: HKCR; Subkey: "Directory\Background\shell\AVIFMaster\command"; ValueType: string; ValueName: ""; ValueData: """{app}\AVIFMaster.exe"" ""%V"""; Flags: uninsdeletekey

[Code]
function InitializeSetup(): Boolean;
begin
  Result := True;
end;
