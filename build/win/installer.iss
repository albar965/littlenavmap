; =====================================================================================================
; Script to create the Little Navmap installer for the 32-bit and 64-bit versions
;
; Run like below
; "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer.iss /DLnmAppArch=%WINARCH%
;   /DLnmAppVersion=%LNM_VERSION% /DLnmAppId=%LNM_APPID% /DLnmAppCompress="lzma2/max"
; =====================================================================================================

; Command line parameters with defaults ==========================================================================
; Set defaults for debugging if not given on the command line
#ifndef LnmAppVersion
  #define LnmAppVersion "2.8.11"
#endif

#ifndef LnmAppArch
  #define LnmAppArch "win64"
#endif

; Use lowest compression as default for debugging - Compression=lzma2/max from command line for release
#ifndef LnmAppCompress
  #define LnmAppCompress "zip/1"
#endif

; Defines ==========================================================================
#define LnmAppProjects GetEnv("APROJECTS")
#define LnmAppName "Little Navmap"
#define LnmAppPublisher "Alexander Barthel"
#define LnmAppURL "https://www.littlenavmap.org/"
#define LnmAppExeName "littlenavmap.exe"
#define LnmAppSourceBase LnmAppProjects + "\deploy\LittleNavmap-" + LnmAppArch + "-" + LnmAppVersion
#define LnmAppSourceDir LnmAppSourceBase + "\*"

#if LnmAppArch == "win64"
  #define AppSuffix "64-bit"
#elif LnmAppArch == "win32"
  #define AppSuffix "32-bit"
#endif

; No file associations yet
;#define LnmAppAssocName LnmAppName + " Flight Plan"
;#define LnmAppAssocExt ".lnmpln"
;#define LnmAppAssocKey StringChange(LnmAppAssocName, " ", "") + LnmAppAssocExt

; LittleNavmap-win64-2.8.11-Install.exe
#define LnmInstaller "LittleNavmap-" + LnmAppArch + "-" + LnmAppVersion + "-Install"

; ==========================================================================
[Setup]
; The small image should be 55x58 pixels. The large image should be 164x314 pixels.  256 bit BMP
WizardSmallImageFile={#LnmAppProjects}\littlenavmap\resources\icons\littlenavmap_55x58.bmp
WizardImageFile={#LnmAppProjects}\littlenavmap\resources\icons\background_164x314.bmp
; The value of AppId uniquely identifies this application. Do not use the same AppId value in installers for other applications.
#if LnmAppArch == "win64"
  AppId={{61C4D3CA-FE30-4467-BE6D-66F8634931BC}
  InfoBeforeFile={#LnmAppProjects}\littlenavmap\build\win\infobefore_64.txt
#elif LnmAppArch == "win32"
  AppId={{BB829DC7-42F4-411B-B9DF-F1ED5BA862EC}
  InfoBeforeFile={#LnmAppProjects}\littlenavmap\build\win\infobefore_32.txt
#endif
AppName={#LnmAppName}
AppVersion={#LnmAppVersion}
AppVerName={#LnmAppName} {#LnmAppVersion} {#AppSuffix}
DisableWelcomePage=no
AppPublisher={#LnmAppPublisher}
AppPublisherURL={#LnmAppURL}
AppSupportURL={#LnmAppURL}
AppUpdatesURL={#LnmAppURL}
DefaultDirName={autopf}\{#LnmAppName}
UninstallDisplayIcon={app}\{#LnmAppExeName}
ChangesAssociations=no
DisableProgramGroupPage=yes
LicenseFile={#LnmAppSourceBase}\LICENSE.txt
;InfoAfterFile={#LnmAppSourceBase}\CHANGELOG.txt
; Uncomment the following line to run in non administrative install mode (install for current user only.)
PrivilegesRequired=admin
;PrivilegesRequiredOverridesAllowed=dialog
OutputDir={#LnmAppProjects}\deploy
OutputBaseFilename={#LnmInstaller}
SetupIconFile={#LnmAppProjects}\littlenavmap\resources\icons\littlenavmap.ico
Compression={#LnmAppCompress}
SolidCompression=yes
WizardStyle=modern
#if LnmAppArch == "win64"
  ; "ArchitecturesAllowed=x64" specifies that Setup cannot run on
  ; anything but x64.
  ArchitecturesAllowed=x64
  ; "ArchitecturesInstallIn64BitMode=x64" requests that the install be
  ; done in "64-bit mode" on x64, meaning it should use the native
  ; 64-bit Program Files directory and the 64-bit view of the registry.
  ArchitecturesInstallIn64BitMode=x64
#endif

; ==========================================================================
[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl"
Name: "portuguese"; MessagesFile: "compiler:Languages\Portuguese.isl"

; ==========================================================================
[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

; ==========================================================================
; LNM exe and whole folder
[Files]
Source: "{#LnmAppSourceBase}\{#LnmAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#LnmAppSourceDir}"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; VC++ redistributable runtime. Extracted by VC2017RedistNeedsInstall(), if needed.
#if LnmAppArch == "win64"
  Source: "{#LnmAppProjects}\Redist\vcredist_2015-2022.x64.exe"; DestDir: {tmp}; Flags: deleteafterinstall
#endif

; No file associations yet
;[Registry]
;Root: HKA; Subkey: "Software\Classes\{#LnmAppAssocExt}\OpenWithProgids"; ValueType: string; ValueName: "{#LnmAppAssocKey}"; ValueData: ""; Flags: uninsdeletevalue
;Root: HKA; Subkey: "Software\Classes\{#LnmAppAssocKey}"; ValueType: string; ValueName: ""; ValueData: "{#LnmAppAssocName}"; Flags: uninsdeletekey
;Root: HKA; Subkey: "Software\Classes\{#LnmAppAssocKey}\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#LnmAppExeName},0"
;Root: HKA; Subkey: "Software\Classes\{#LnmAppAssocKey}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#LnmAppExeName}"" ""%1"""
;Root: HKA; Subkey: "Software\Classes\Applications\{#LnmAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".lnmpln"; ValueData: ""

; ==========================================================================
; Start menu entries for all programs and documentation
[Icons]
Name: "{autoprograms}\{#LnmAppName} {#AppSuffix}\{#LnmAppName}"; Filename: "{app}\{#LnmAppExeName}"
Name: "{autoprograms}\{#LnmAppName} {#AppSuffix}\Little Navconnect"; Filename: "{app}\Little Navconnect\littlenavconnect.exe"
Name: "{autoprograms}\{#LnmAppName} {#AppSuffix}\Little Navmap User Manual PDF (Offline)"; Filename: "{app}\help\little-navmap-user-manual-en.pdf"
Name: "{autoprograms}\{#LnmAppName} {#AppSuffix}\Little Navmap User Manual (Online)"; Filename: "{app}\help\Little Navmap User Manual Online.url"
Name: "{autoprograms}\{#LnmAppName} {#AppSuffix}\Changelog"; Filename: "{app}\CHANGELOG.TXT"
Name: "{autoprograms}\{#LnmAppName} {#AppSuffix}\Readme"; Filename: "{app}\README.TXT"
Name: "{autodesktop}\{#LnmAppName} {#AppSuffix}"; Filename: "{app}\{#LnmAppExeName}"; Tasks: desktopicon

; ==========================================================================
; Optional run at end
[Run]
Filename: "{app}\{#LnmAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(LnmAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
Filename: "{app}\CHANGELOG.txt"; Description: "Open the changelog"; Flags: postinstall shellexec skipifsilent
#if LnmAppArch == "win64"
  Filename: "{tmp}\vcredist_2015-2022.x64.exe"; StatusMsg: "Installing MSVC Redistributables 2015-2022 64-bit ..."; Parameters: "/quiet /norestart"; Flags: runascurrentuser waituntilterminated
#endif

