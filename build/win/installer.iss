// =====================================================================================================
// Script to create the Little Navmap installer for the 32-bit and 64-bit versions.
//
// This file has to be encoded using UTF-8 with BOM.
//
// Run like below
// "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer.iss /DLnmAppArch=%WINARCH%
//   /DLnmAppVersion=%LNM_VERSION% /DLnmAppId=%LNM_APPID% /DLnmAppCompress="lzma2/max"
// =====================================================================================================

// Command line parameters with defaults ==========================================================================
// Set defaults for debugging if not given on the command line
#ifndef LnmAppVersion
  #define LnmAppVersion "2.8.12.rc2"
#endif

#ifndef LnmAppArch
  #define LnmAppArch "win64"
#endif

// Use lowest compression as default for debugging - Compression=lzma2/max from command line for release
#ifndef LnmAppCompress
  #define LnmAppCompress "zip/1"
#endif

// Defines ==========================================================================
#if LnmAppArch == "win64"
  #define AppSuffix "64-bit"
#elif LnmAppArch == "win32"
  #define AppSuffix "32-bit"
#endif

#define LnmAppProjects GetEnv("APROJECTS")
#define LnmAppName "Little Navmap"
#define LnmAppConnectName "Little Navconnect"
#define LnmAppNameReg "LittleNavmap"
#define LnmAppPublisher "Alexander Barthel"
#define LnmAppURL "https://www.littlenavmap.org/"
#define LnmAppExeName "littlenavmap.exe"
#define LnmAppConnectExeName "littlenavconnect.exe"
#define LnmAppSourceBase LnmAppProjects + "\deploy\LittleNavmap-" + LnmAppArch + "-" + LnmAppVersion
#define LnmAppSourceDir LnmAppSourceBase + "\*"

// No file associations yet
#define LnmAppAssocName LnmAppName + " Flight Plan"
#define LnmAppAssocExt ".lnmpln"
#define LnmAppAssocKey StringChange(LnmAppAssocName, " ", "") + LnmAppAssocExt

// LittleNavmap-win64-2.8.11-Install.exe
#define LnmInstaller "LittleNavmap-" + LnmAppArch + "-" + LnmAppVersion + "-Install"

// ==========================================================================
[Setup]
ShowLanguageDialog=auto
// The small image should be 55x58 pixels. The large image should be 164x314 pixels.  256 bit BMP
WizardSmallImageFile={#LnmAppProjects}\littlenavmap\resources\icons\littlenavmap_55x58.bmp
WizardImageFile={#LnmAppProjects}\littlenavmap\resources\icons\background_164x314.bmp
// The value of AppId uniquely identifies this application. Do not use the same AppId value in installers for other applications.
#if LnmAppArch == "win64"
  AppId={{61C4D3CA-FE30-4467-BE6D-66F8634931BC}
#elif LnmAppArch == "win32"
  AppId={{BB829DC7-42F4-411B-B9DF-F1ED5BA862EC}
#endif
RestartIfNeededByRun=no
InfoBeforeFile={#LnmAppProjects}\littlenavmap\build\win\INFOBEFORE_{#LnmAppArch}_en.txt
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
ChangesAssociations=yes
DisableProgramGroupPage=yes
LicenseFile={#LnmAppSourceBase}\LICENSE.txt
// InfoAfterFile={#LnmAppSourceBase}\CHANGELOG.txt
// Uncomment the following line to run in non administrative install mode (install for current user only.)
PrivilegesRequired=admin
// PrivilegesRequiredOverridesAllowed=dialog
OutputDir={#LnmAppProjects}\deploy
OutputBaseFilename={#LnmInstaller}
SetupIconFile={#LnmAppProjects}\littlenavmap\resources\icons\littlenavmap.ico
Compression={#LnmAppCompress}
SolidCompression=yes
WizardStyle=modern
#if LnmAppArch == "win64"
  // "ArchitecturesAllowed=x64" specifies that Setup cannot run on
  // anything but x64.
  ArchitecturesAllowed=x64
  // "ArchitecturesInstallIn64BitMode=x64" requests that the install be
  // done in "64-bit mode" on x64, meaning it should use the native
  // 64-bit Program Files directory and the 64-bit view of the registry.
  ArchitecturesInstallIn64BitMode=x64
#endif

; ==========================================================================
[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"; LicenseFile: {#LnmAppProjects}\littlenavmap\build\win\LICENSE_en.rtf; \
  InfoBeforeFile: {#LnmAppProjects}\littlenavmap\build\win\INFOBEFORE_{#LnmAppArch}_en.txt
Name: "de"; MessagesFile: "compiler:Languages\German.isl"; LicenseFile: {#LnmAppProjects}\littlenavmap\build\win\LICENSE_de.rtf; \
  InfoBeforeFile: {#LnmAppProjects}\littlenavmap\build\win\INFOBEFORE_{#LnmAppArch}_de.txt
Name: "fr"; MessagesFile: "compiler:Languages\French.isl"
Name: "it"; MessagesFile: "compiler:Languages\Italian.isl"
Name: "pt_BR"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "nl"; MessagesFile: "compiler:Languages\Dutch.isl"
Name: "es"; MessagesFile: "compiler:Languages\Spanish.isl"

; ==========================================================================
[CustomMessages]
ChangelogMessage=Open the changelog
en.ChangelogMessage=Open the changelog
de.ChangelogMessage=Liste der Änderungen öffnen
OpenStartMessage=Open the user manual start page, which contains tips and tricks for first-time users.
en.OpenStartMessage=Open the user manual start page, which contains tips and tricks for first-time users.
de.OpenStartMessage=Startseite des Benutzerhandbuchs öffnen, die Tipps und Tricks für Erstbenutzer enthält.
InstallingRedistMessage=Installing MSVC Redistributables ...
en.InstallingRedistMessage=Installing MSVC Redistributables ...
de.InstallingRedistMessage=Installiere MSVC Redistributables ...
UserManualMessage=User Manual
en.UserManualMessage=User Manual
de.UserManualMessage=Benutzerhandbuch
ChangelogMessage=Changelog
en.ChangelogMessage=Changelog
de.ChangelogMessage=Liste der Änderungen
ReadmeMessage=Readme
en.ReadmeMessage=Readme
de.ReadmeMessage=Liesmich
AssocLnmMessage=Associate {#LnmAppName} file types with program:
en.AssocLnmMessage=Associate {#LnmAppName} file types with program:
de.AssocLnmMessage=Dateitypen von {#LnmAppName} mit dem Programm verknüpfen:
AssocOtherMessage=Associate other flight plan file extensions with {#LnmAppName}:
en.AssocOtherMessage=Associate other flight plan file extensions with {#LnmAppName}:
de.AssocOtherMessage=Weitere Flugplan-Dateitypen mit {#LnmAppName} verknüpfen:
DeleteSettingsMessage=Do you also want to remove all settings, databases and cached files?%n%nThis will remove the folders %n"%1"%nand%n"%2".
en.DeleteSettingsMessage=Do you also want to remove all settings, databases and cached files?%n%nThis will remove the folders %n"%1"%nand%n"%2".
de.DeleteSettingsMessage=Alle Einstellungen, Datenbanken und Dateien im Zwischenspeicher ebenfalls entfernen?%n%nDies wird die Verzeichnisse%n"%1"%nund%n"%2"%nlöschen.
DeleteSettingsMessage2=This will delete all userpoints as well%nas the logbook and cannot be undone.%n%nAre your sure?
en.DeleteSettingsMessage2=This will delete all userpoints as well%nas the logbook and cannot be undone.%n%nAre your sure?
de.DeleteSettingsMessage2=Dies löscht alle Nutzerpunkte und das Logbuch und kann nicht rückgängig gemacht werden.%n%nWirklich löschen?

// ==========================================================================
[Tasks]
Name: desktopicon; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: lnmassociation; Description: "{cm:AssocFileExtension,{#LnmAppName},"".lnmpln""%2c "".lnmperf""%2c "".lnmlayout""}"; \
  GroupDescription: "{cm:AssocLnmMessage}"
Name: lnmplnassociation; Description: "{cm:AssocFileExtension,{#LnmAppName},FSX%2c P3D%2c MSFS "".pln""}"; Flags: unchecked; \
  GroupDescription: "{cm:AssocOtherMessage}"
Name: lnmfmsassociation; Description: "{cm:AssocFileExtension,{#LnmAppName},X-Plane "".fms""}"; Flags: unchecked; \
  GroupDescription: "{cm:AssocOtherMessage}"

// Disabled associations
// Name: lnmfgfpssociation; Description: "{cm:AssocFileExtension,{#LnmAppName},FlightGear "".fgfp""}"; Flags: unchecked; GroupDescription: "Associate other flight plan file extensions:"
// Name: lnmfplassociation; Description: "{cm:AssocFileExtension,{#LnmAppName},Garmin "".fpl""}"; Flags: unchecked; GroupDescription: "Associate other flight plan file extensions:"
// Name: lnmgfpassociation; Description: "{cm:AssocFileExtension,{#LnmAppName},Garmin "".gfp""}"; Flags: unchecked; GroupDescription: "Associate other flight plan file extensions:"
//Name: lnmflpassociation; Description: "{cm:AssocFileExtension,{#LnmAppName},"".flp""}"; Flags: unchecked; GroupDescription: "Associate other flight plan file extensions:"

// ==========================================================================
// LNM exe and whole folder
[Files]
Source: "{#LnmAppSourceBase}\{#LnmAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#LnmAppSourceDir}"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
// VC++ redistributable runtime. Extracted by VC2017RedistNeedsInstall(), if needed.
#if LnmAppArch == "win64"
  Source: "{#LnmAppProjects}\Redist\vcredist_2015-2022.x64.exe"; DestDir: {tmp}; Flags: deleteafterinstall
#elif LnmAppArch == "win32"
  Source: "{#LnmAppProjects}\Redist\vcredist_2005_x86.exe"; DestDir: {tmp}; Flags: deleteafterinstall
#endif

Source: "{#LnmAppProjects}\littlenavmap\build\win\Little Navmap User Manual Online.url"; DestDir: "{app}\help";
Source: "{#LnmAppProjects}\littlenavmap\build\win\Little Navmap User Manual Online Start.url"; DestDir: "{app}\help";

// File associations ==========================================================================
[Registry]
Root: HKCR; Subkey: ".lnmpln"; ValueType: string; ValueName: ""; ValueData: "{#LnmAppNameReg}"; Flags: uninsdeletekey; Tasks: lnmassociation
Root: HKCR; Subkey: ".lnmperf"; ValueType: string; ValueName: ""; ValueData: "{#LnmAppNameReg}"; Flags: uninsdeletekey; Tasks: lnmassociation
Root: HKCR; Subkey: ".lnmlayout"; ValueType: string; ValueName: ""; ValueData: "{#LnmAppNameReg}"; Flags: uninsdeletekey; Tasks: lnmassociation
Root: HKCR; Subkey: ".pln"; ValueType: string; ValueName: ""; ValueData: "{#LnmAppNameReg}"; Flags: uninsdeletekey; Tasks: lnmplnassociation
Root: HKCR; Subkey: ".fms"; ValueType: string; ValueName: ""; ValueData: "{#LnmAppNameReg}"; Flags: uninsdeletekey; Tasks: lnmfmsassociation
Root: HKCR; Subkey: "{#LnmAppNameReg}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#LnmAppExeName}"" ""%1"""

// Disabled associations
// Root: HKCR; Subkey: ".fgfp"; ValueType: string; ValueName: ""; ValueData: "{#LnmAppNameReg}"; Flags: uninsdeletekey; Tasks: lnmfgfpssociation
// Root: HKCR; Subkey: ".gfp"; ValueType: string; ValueName: ""; ValueData: "{#LnmAppNameReg}"; Flags: uninsdeletekey; Tasks: lnmgfpassociation
// Root: HKCR; Subkey: ".flp"; ValueType: string; ValueName: ""; ValueData: "{#LnmAppNameReg}"; Flags: uninsdeletekey; Tasks: lnmflpassociation
// Root: HKCR; Subkey: "{#LnmAppNameReg}"; ValueType: string; ValueName: ""; ValueData: "Little Navmap File"; Flags: uninsdeletekey
// Root: HKCR; Subkey: "{#LnmAppNameReg}\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\littlenavmap.exe,1"

// ==========================================================================
// Start menu entries for all programs and documentation
[Icons]
Name: "{autoprograms}\{#LnmAppName} {#AppSuffix}\{#LnmAppName}"; Filename: "{app}\{#LnmAppExeName}"
Name: "{autoprograms}\{#LnmAppName} {#AppSuffix}\{#LnmAppConnectName}"; Filename: "{app}\{#LnmAppConnectName}\{#LnmAppConnectExeName}"
Name: "{autoprograms}\{#LnmAppName} {#AppSuffix}\Little Navmap {cm:UserManualMessage} PDF (Offline)"; Filename: "{app}\help\little-navmap-user-manual-en.pdf"
Name: "{autoprograms}\{#LnmAppName} {#AppSuffix}\Little Navmap {cm:UserManualMessage} (Online)"; Filename: "{app}\help\Little Navmap User Manual Online.url"
Name: "{autoprograms}\{#LnmAppName} {#AppSuffix}\{cm:ChangelogMessage}"; Filename: "{app}\CHANGELOG.TXT"
Name: "{autoprograms}\{#LnmAppName} {#AppSuffix}\{cm:ReadmeMessage}"; Filename: "{app}\README.TXT"
Name: "{autodesktop}\{#LnmAppName} {#AppSuffix}"; Filename: "{app}\{#LnmAppExeName}"; Tasks: desktopicon

// ==========================================================================
// Optional run at end
[Run]
Filename: "{app}\{#LnmAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(LnmAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
Filename: "https://www.littlenavmap.org/manuals/littlenavmap/release/latest/en/START.html"; Description: "{cm:OpenStartMessage}"; Flags: nowait shellexec postinstall skipifsilent
Filename: "{app}\CHANGELOG.txt"; Description: "{cm:ChangelogMessage}"; Flags: nowait postinstall shellexec skipifsilent
#if LnmAppArch == "win64"
  Filename: "{tmp}\vcredist_2015-2022.x64.exe"; StatusMsg: "{cm:InstallingRedistMessage}"; Parameters: "/quiet /norestart"; Flags: runascurrentuser waituntilterminated
#elif LnmAppArch == "win32"
  Filename: "{tmp}\vcredist_2005_x86.exe"; StatusMsg: "{cm:InstallingRedistMessage}"; Parameters: "/Q"; Flags: runascurrentuser waituntilterminated
#endif


// ==========================================================================
// Ask to delete 'ABarthel' settings and the Marble cache files on uninstall
[Code]
procedure CurUninstallStepChanged(CurUninstallStep : TUninstallStep);
var
  appdata : string;
  localappdata : string;
begin
  if CurUninstallStep = usPostUninstall then
  begin
    // Get and build paths
    // C:\Users\alex\AppData\Roaming\ABarthel\
    appdata := ExpandConstant('{userappdata}') + '\ABarthel';
    // C:\Users\alex\AppData\Local\.marble\data\maps\earth
    localappdata := ExpandConstant('{localappdata}') + '\.marble\data\maps\earth';

    // Ask with localized message
    if MsgBox(FmtMessage(CustomMessage('DeleteSettingsMessage'), [appdata, localappdata]), mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES then
    begin
      if MsgBox(FmtMessage(CustomMessage('DeleteSettingsMessage2'), [appdata, localappdata]), mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES then
      begin
        DelTree(appdata, True, True, True);
        DelTree(localappdata, True, True, True);
      end;
    end;
  end;
end;
