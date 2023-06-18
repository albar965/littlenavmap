; =====================================================================================================
; Script to create the Little Navmap installer for the 32-bit and 64-bit versions
;
; Run like below
; "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer.iss /DLnmAppArch=win32 /DLnmAppVersion=2.8.11
; =====================================================================================================

; Have to be passed on the command line
;#define LnmAppVersion "2.8.11"
;#define LnmAppArch "win64"

#define LnmAppProjects GetEnv("APROJECTS")
#define LnmAppName "Little Navmap"
#define LnmAppPublisher "Alexander Barthel"
#define LnmAppURL "https://www.littlenavmap.org/"
#define LnmAppExeName "littlenavmap.exe"
#define LnmAppSourceBase LnmAppProjects + "\deploy\LittleNavmap-" + LnmAppArch + "-" + LnmAppVersion
#define LnmAppSourceDir LnmAppSourceBase + "\*"

; No file associations yet
;#define LnmAppAssocName LnmAppName + " Flight Plan"
;#define LnmAppAssocExt ".lnmpln"
;#define LnmAppAssocKey StringChange(LnmAppAssocName, " ", "") + LnmAppAssocExt

; LittleNavmap-win64-2.8.11-Install.exe
#define MyInstaller "LittleNavmap-" + LnmAppArch + "-" + LnmAppVersion + "-Install"

[Setup]
; NOTE: The value of AppId uniquely identifies this application. Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{C029A69A-03F4-4781-9B80-FACCD4E3A816}
AppName={#LnmAppName}
AppVersion={#LnmAppVersion}
;AppVerName={#LnmAppName} {#LnmAppVersion}
AppPublisher={#LnmAppPublisher}
AppPublisherURL={#LnmAppURL}
AppSupportURL={#LnmAppURL}
AppUpdatesURL={#LnmAppURL}
DefaultDirName={autopf}\{#LnmAppName}
ChangesAssociations=no
DisableProgramGroupPage=yes
LicenseFile={#LnmAppSourceBase}\LICENSE.txt
;InfoAfterFile={#LnmAppSourceBase}\CHANGELOG.txt
; Uncomment the following line to run in non administrative install mode (install for current user only.)
;PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
OutputDir={#LnmAppProjects}\deploy
OutputBaseFilename={#MyInstaller}
SetupIconFile={#LnmAppProjects}\littlenavmap\resources\icons\littlenavmap.ico
Compression=lzma2/max
;Compression=zip/1
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl"
Name: "portuguese"; MessagesFile: "compiler:Languages\Portuguese.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

; LNM exe and whole folder
[Files]
Source: "{#LnmAppSourceBase}\{#LnmAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#LnmAppSourceDir}"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

; No file associations yet
;[Registry]
;Root: HKA; Subkey: "Software\Classes\{#LnmAppAssocExt}\OpenWithProgids"; ValueType: string; ValueName: "{#LnmAppAssocKey}"; ValueData: ""; Flags: uninsdeletevalue
;Root: HKA; Subkey: "Software\Classes\{#LnmAppAssocKey}"; ValueType: string; ValueName: ""; ValueData: "{#LnmAppAssocName}"; Flags: uninsdeletekey
;Root: HKA; Subkey: "Software\Classes\{#LnmAppAssocKey}\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#LnmAppExeName},0"
;Root: HKA; Subkey: "Software\Classes\{#LnmAppAssocKey}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#LnmAppExeName}"" ""%1"""
;Root: HKA; Subkey: "Software\Classes\Applications\{#LnmAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".lnmpln"; ValueData: ""

; Start menu entries for all programs and documentation
[Icons]
Name: "{autoprograms}\{#LnmAppName}\{#LnmAppName}"; Filename: "{app}\{#LnmAppExeName}"
Name: "{autoprograms}\{#LnmAppName}\Little Navconnect"; Filename: "{app}\Little Navconnect\littlenavconnect.exe"
Name: "{autoprograms}\{#LnmAppName}\Little Navmap User Manual PDF (Offline)"; Filename: "{app}\help\little-navmap-user-manual-en.pdf"
Name: "{autoprograms}\{#LnmAppName}\Little Navmap User Manual (Online)"; Filename: "{app}\help\Little Navmap User Manual Online.url"
Name: "{autoprograms}\{#LnmAppName}\Changelog"; Filename: "{app}\CHANGELOG.TXT"
Name: "{autoprograms}\{#LnmAppName}\Readme"; Filename: "{app}\README.TXT"
Name: "{autodesktop}\{#LnmAppName}"; Filename: "{app}\{#LnmAppExeName}"; Tasks: desktopicon

; Optional run at end
[Run]
Filename: "{app}\{#LnmAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(LnmAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
Filename: "{app}\CHANGELOG.txt"; Description: "Open the changelog"; Flags: postinstall shellexec skipifsilent
