; =====================================================================================================
; Copyright 2015-2025 Alexander Barthel alex@littlenavmap.org
;
; Inno Setup installer script https://jrsoftware.org/isinfo.php
; Help at https://jrsoftware.org/ishelp/
;
; Script to create the Little Navmap installer for the 32-bit and 64-bit versions.
;
; This file has to be encoded using UTF-8 with BOM.
;
; Run like below
; "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer.iss /DLnmAppArch=%WINARCH%
;   /DLnmAppVersion=%LNM_VERSION% /DLnmAppId=%LNM_APPID% /DLnmAppCompress="lzma2/max"
; =====================================================================================================

; Command line parameters with defaults ==========================================================================
; Set defaults for debugging if not given on the command line
#ifndef LnmAppVersion
  #define LnmAppVersion "3.0.18"
#endif

#ifndef LnmAppArch
  #define LnmAppArch "win64"
#endif

; Use lowest compression as default for debugging - Compression=lzma2/max from command line for release
#ifndef LnmAppCompress
  #define LnmAppCompress "zip/1"
#endif

; Defines ==========================================================================
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

; No file associations yet
#define LnmAppAssocName LnmAppName + " Flight Plan"
#define LnmAppAssocExt ".lnmpln"
#define LnmAppAssocKey StringChange(LnmAppAssocName, " ", "") + LnmAppAssocExt

; LittleNavmap-win64-2.8.11-Install.exe
#define LnmInstaller "LittleNavmap-" + LnmAppArch + "-" + LnmAppVersion + "-Install"

; ==========================================================================
[Setup]
ShowLanguageDialog=auto
; The small image should be 55x58 pixels. The large image should be 164x314 pixels.  256 bit BMP
WizardSmallImageFile={#LnmAppProjects}\littlenavmap\resources\icons\littlenavmap_55x58.bmp
WizardImageFile={#LnmAppProjects}\littlenavmap\resources\icons\background_164x314.bmp
; The value of AppId uniquely identifies this application. Do not use the same AppId value in installers for other applications.
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
; InfoAfterFile={#LnmAppSourceBase}\CHANGELOG.txt
; Uncomment the following line to run in non administrative install mode (install for current user only.)
PrivilegesRequired=admin
; PrivilegesRequiredOverridesAllowed=dialog
OutputDir={#LnmAppProjects}\deploy
OutputBaseFilename={#LnmInstaller}
; convert -density 300 -define icon:auto-resize=256,64,48,32,16 -background none littlenavmapinstall.svg littlenavmapinstall.ico
SetupIconFile={#LnmAppProjects}\littlenavmap\resources\icons\littlenavmapinstall.ico
Compression={#LnmAppCompress}
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
CloseApplicationsFilter=*.exe
#if LnmAppArch == "win64"
  ; x64compatible
  ; Matches systems capable of running x64 binaries.
  ; This includes systems running x64 Windows, and also Arm64-based Windows 11 systems,
  ; which have the ability to run x64 binaries via emulation.

  ; Specifies which architectures Setup is allowed to run on.
  ; If none of the specified architecture identifiers match the current system,
  ; Setup will display an error message (WindowsVersionNotSupported) and exit.
  ArchitecturesAllowed=x64compatible

  ; Only allow the installer to run on x64-compatible systems,
  ; and enable 64-bit install mode.
  ; 64-bit Program Files directory and the 64-bit view of the registry.
  ArchitecturesInstallIn64BitMode=x64compatible
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
QuitProgramMessage=Please quit Little Navmap and/or Little Navconnect if they are still running.%nOtherwise the uninstallation may be incomplete.%n%nTrying to quit the programs automatically now.
en.QuitProgramMessage=Please quit Little Navmap and/or Little Navconnect if they are still running.%nOtherwise the uninstallation may be incomplete.%n%nTrying to quit the programs automatically now.
de.QuitProgramMessage=Bitte beenden Sie Little Navmap bzw. Little Navconnect, falls sie noch laufen.%nAnsonsten kann die Deinstallation unvollständig sein.%n%nVersuche jetzt, die Programme automatisch zu beenden.

; ==========================================================================
[Tasks]
Name: desktopicon; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: lnmassociation; Description: "{cm:AssocFileExtension,{#LnmAppName},"".lnmpln""%2c "".lnmperf""%2c "".lnmlayout""}"; \
  GroupDescription: "{cm:AssocLnmMessage}"
Name: lnmplnassociation; Description: "{cm:AssocFileExtension,{#LnmAppName},FSX%2c P3D%2c MSFS "".pln""}"; Flags: unchecked; \
  GroupDescription: "{cm:AssocOtherMessage}"
Name: lnmfmsassociation; Description: "{cm:AssocFileExtension,{#LnmAppName},X-Plane "".fms""}"; Flags: unchecked; \
  GroupDescription: "{cm:AssocOtherMessage}"

; Disabled associations
; Name: lnmfgfpssociation; Description: "{cm:AssocFileExtension,{#LnmAppName},FlightGear "".fgfp""}"; Flags: unchecked; GroupDescription: "Associate other flight plan file extensions:"
; Name: lnmfplassociation; Description: "{cm:AssocFileExtension,{#LnmAppName},Garmin "".fpl""}"; Flags: unchecked; GroupDescription: "Associate other flight plan file extensions:"
; Name: lnmgfpassociation; Description: "{cm:AssocFileExtension,{#LnmAppName},Garmin "".gfp""}"; Flags: unchecked; GroupDescription: "Associate other flight plan file extensions:"
; Name: lnmflpassociation; Description: "{cm:AssocFileExtension,{#LnmAppName},"".flp""}"; Flags: unchecked; GroupDescription: "Associate other flight plan file extensions:"

; ==========================================================================
; LNM exe and whole folder
[Files]
Source: "{#LnmAppSourceBase}\{#LnmAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#LnmAppSourceDir}"; DestDir: "{app}"; Excludes: "\Little Navmap Portable.cmd, \littlenavmap.debug, \Little Navconnect\littlenavconnect.debug"; Flags: ignoreversion recursesubdirs createallsubdirs
; VC++ redistributable runtime. Extracted by VC2017RedistNeedsInstall(), if needed.
; %APROJECTS%\Latest\VC_redist.x64.exe for MSFS SimConnect
; %APROJECTS%\2005\vcredist_x86.exe for FSX and P3D SimConnect
#if LnmAppArch == "win64"
  Source: "{#LnmAppProjects}\Redist\Latest\VC_redist.x64.exe"; DestDir: {tmp}; Flags: deleteafterinstall
#elif LnmAppArch == "win32"
  Source: "{#LnmAppProjects}\Redist\2005\vcredist_x86.exe"; DestDir: {tmp}; Flags: deleteafterinstall
#endif
Source: "{#LnmAppProjects}\littlenavmap\build\win\Little Navmap User Manual Online.url"; DestDir: "{app}\help";
Source: "{#LnmAppProjects}\littlenavmap\build\win\Little Navmap User Manual Online Start.url"; DestDir: "{app}\help";

; Delete obsolete files before installation ==========================================================================
[InstallDelete]
Type: files; Name: "{app}\translations\atools_*.qm"
Type: files; Name: "{app}\translations\littlenavmap_*.qm"
Type: files; Name: "{app}\Little Navconnect\translations\atools_*.qm"
Type: files; Name: "{app}\Little Navconnect\translations\littlenavconnect_*.qm"
Type: files; Name: "{app}\Little Navmap Portable.cmd"
Type: files; Name: "{app}\Little Navmap\help\little-navmap-user-manual-en.pdf"
Type: files; Name: "{autoprograms}\{#LnmAppName} {#AppSuffix}\Little Navmap*PDF*.lnk"
Type: files; Name: "{app}\data\maps\earth\stamenterrain\stamenterrain.dgml"
Type: files; Name: "{app}\data\maps\earth\stamenterrain\stamenterrain-preview.png"
Type: files; Name: "{app}\data\maps\earth\stamenterrain\0\0\0.png"
Type: dirifempty; Name: "{app}\data\maps\earth\stamenterrain\0\0"
Type: dirifempty; Name: "{app}\data\maps\earth\stamenterrain\0"
Type: dirifempty; Name: "{app}\data\maps\earth\stamenterrain"
; Qt5 from 3.0 branches ===================================================
Type: files; Name: "{app}\libastro.dll"
Type: files; Name: "{app}\libEGL.dll"
Type: files; Name: "{app}\libGLESv2.dll"
Type: files; Name: "{app}\libmarblewidget"
Type: files; Name: "{app}\Qt5Core.dll"
Type: files; Name: "{app}\Qt5Gui.dll"
Type: files; Name: "{app}\Qt5Network.dll"
Type: files; Name: "{app}\Qt5OpenGL.dll"
Type: files; Name: "{app}\Qt5PrintSupport.dll"
Type: files; Name: "{app}\Qt5Qml.dll"
Type: files; Name: "{app}\Qt5QmlModels.dll"
Type: files; Name: "{app}\Qt5Quick.dll"
Type: files; Name: "{app}\Qt5Sql.dll"
Type: files; Name: "{app}\Qt5Svg.dll"
Type: files; Name: "{app}\Qt5Widgets.dll"
Type: files; Name: "{app}\Qt5Xml.dll"
Type: files; Name: "{app}\bearer\qgenericbearer.dll"
Type: files; Name: "{app}\imageformats\qicns.dll"
Type: files; Name: "{app}\imageformats\qtga.dll"
Type: files; Name: "{app}\imageformats\qtiff.dll"
Type: files; Name: "{app}\imageformats\qwbmp.dll"
Type: files; Name: "{app}\imageformats\qwebp.dll"
Type: files; Name: "{app}\plugins\libLatLonPlugin.dll"
Type: files; Name: "{app}\plugins\printsupport\windowsprintersupport.dll"
Type: files; Name: "{app}\web\images\point\point-down.cur"
Type: files; Name: "{app}\web\images\point\point-left.cur"
Type: files; Name: "{app}\web\images\point\point-right.cur"
Type: files; Name: "{app}\web\images\point\point-up.cur"
Type: files; Name: "{app}\web\images\point\there.cur"
Type: files; Name: "{app}\data\precipcolors.leg"
Type: files; Name: "{app}\data\bitmaps\cursor_bc.png"
Type: files; Name: "{app}\data\bitmaps\cursor_bl.png"
Type: files; Name: "{app}\data\bitmaps\cursor_br.png"
Type: files; Name: "{app}\data\bitmaps\cursor_cl.png"
Type: files; Name: "{app}\data\bitmaps\cursor_cr.png"
Type: files; Name: "{app}\data\bitmaps\cursor_tc.png"
Type: files; Name: "{app}\data\bitmaps\cursor_tl.png"
Type: files; Name: "{app}\data\bitmaps\cursor_tr.png"
Type: files; Name: "{app}\data\bitmaps\turn-around.png"
Type: files; Name: "{app}\data\bitmaps\turn-continue.png"
Type: files; Name: "{app}\data\bitmaps\turn-exit-left.png"
Type: files; Name: "{app}\data\bitmaps\turn-exit-right.png"
Type: files; Name: "{app}\data\bitmaps\turn-left.png"
Type: files; Name: "{app}\data\bitmaps\turn-merge.png"
Type: files; Name: "{app}\data\bitmaps\turn-right.png"
Type: files; Name: "{app}\data\bitmaps\turn-roundabout-far.png"
Type: files; Name: "{app}\data\bitmaps\turn-roundabout-first.png"
Type: files; Name: "{app}\data\bitmaps\turn-roundabout-second.png"
Type: files; Name: "{app}\data\bitmaps\turn-roundabout-third.png"
Type: files; Name: "{app}\data\bitmaps\turn-sharp-left.png"
Type: files; Name: "{app}\data\bitmaps\turn-sharp-right.png"
Type: files; Name: "{app}\data\bitmaps\turn-slight-left.png"
Type: files; Name: "{app}\data\bitmaps\turn-slight-right.png"
Type: files; Name: "{app}\data\bitmaps\unmanned_hard_landing.png"
Type: files; Name: "{app}\data\bitmaps\unmanned_soft_landing.png"
Type: files; Name: "{app}\data\bitmaps\editarrows\arrow-diagonal-topleft-active.png"
Type: files; Name: "{app}\data\bitmaps\editarrows\arrow-diagonal-topleft.png"
Type: files; Name: "{app}\data\bitmaps\editarrows\arrow-diagonal-topright-active.png"
Type: files; Name: "{app}\data\bitmaps\editarrows\arrow-diagonal-topright.png"
Type: files; Name: "{app}\data\bitmaps\editarrows\arrow-horizontal-active.png"
Type: files; Name: "{app}\data\bitmaps\editarrows\arrow-horizontal.png"
Type: files; Name: "{app}\data\bitmaps\editarrows\arrow-rotation-bottomleft-active.png"
Type: files; Name: "{app}\data\bitmaps\editarrows\arrow-rotation-bottomleft.png"
Type: files; Name: "{app}\data\bitmaps\editarrows\arrow-rotation-bottomright-active.png"
Type: files; Name: "{app}\data\bitmaps\editarrows\arrow-rotation-bottomright.png"
Type: files; Name: "{app}\data\bitmaps\editarrows\arrow-rotation-topleft-active.png"
Type: files; Name: "{app}\data\bitmaps\editarrows\arrow-rotation-topleft.png"
Type: files; Name: "{app}\data\bitmaps\editarrows\arrow-rotation-topright-active.png"
Type: files; Name: "{app}\data\bitmaps\editarrows\arrow-rotation-topright.png"
Type: files; Name: "{app}\data\bitmaps\editarrows\arrow-vertical-active.png"
Type: files; Name: "{app}\data\bitmaps\editarrows\arrow-vertical.png"
Type: files; Name: "{app}\data\bitmaps\stars\star_0_blue.png"
Type: files; Name: "{app}\data\bitmaps\stars\star_0_bluewhite.png"
Type: files; Name: "{app}\data\bitmaps\stars\star_0_garnetred.png"
Type: files; Name: "{app}\data\bitmaps\stars\star_0_orange.png"
Type: files; Name: "{app}\data\bitmaps\stars\star_0_red.png"
Type: files; Name: "{app}\data\bitmaps\stars\star_0_white.png"
Type: files; Name: "{app}\data\bitmaps\stars\star_0_yellow.png"
Type: files; Name: "{app}\data\bitmaps\stars\star_3_blue.png"
Type: files; Name: "{app}\data\bitmaps\stars\star_3_bluewhite.png"
Type: files; Name: "{app}\data\bitmaps\stars\star_3_garnetred.png"
Type: files; Name: "{app}\data\bitmaps\stars\star_3_orange.png"
Type: files; Name: "{app}\data\bitmaps\stars\star_3_red.png"
Type: files; Name: "{app}\data\bitmaps\stars\star_3_white.png"
Type: files; Name: "{app}\data\bitmaps\stars\star_3_yellow.png"
Type: files; Name: "{app}\data\svg\lunarmap.svg"
Type: files; Name: "{app}\data\svg\marsmap.svg"
Type: files; Name: "{app}\data\svg\moon.png"
Type: files; Name: "{app}\Little Navconnect\libEGL.dll"
Type: files; Name: "{app}\Little Navconnect\libGLESv2.dll"
Type: files; Name: "{app}\Little Navconnect\Qt5Core.dll"
Type: files; Name: "{app}\Little Navconnect\Qt5Gui.dll"
Type: files; Name: "{app}\Little Navconnect\Qt5Network.dll"
Type: files; Name: "{app}\Little Navconnect\Qt5Svg.dll"
Type: files; Name: "{app}\Little Navconnect\Qt5Widgets.dll"
Type: files; Name: "{app}\Little Navconnect\bearer\qgenericbearer.dll"
Type: files; Name: "{app}\Little Navconnect\imageformats\qicns.dll"
Type: files; Name: "{app}\Little Navconnect\imageformats\qtga.dll"
Type: files; Name: "{app}\Little Navconnect\imageformats\qtiff.dll"
Type: files; Name: "{app}\Little Navconnect\imageformats\qwbmp.dll"
Type: files; Name: "{app}\Little Navconnect\imageformats\qwebp.dll"


; File associations ==========================================================================
[Registry]
Root: HKCR; Subkey: ".lnmpln"; ValueType: string; ValueName: ""; ValueData: "{#LnmAppNameReg}"; Flags: uninsdeletekey; Tasks: lnmassociation
Root: HKCR; Subkey: ".lnmperf"; ValueType: string; ValueName: ""; ValueData: "{#LnmAppNameReg}"; Flags: uninsdeletekey; Tasks: lnmassociation
Root: HKCR; Subkey: ".lnmlayout"; ValueType: string; ValueName: ""; ValueData: "{#LnmAppNameReg}"; Flags: uninsdeletekey; Tasks: lnmassociation
Root: HKCR; Subkey: ".pln"; ValueType: string; ValueName: ""; ValueData: "{#LnmAppNameReg}"; Flags: uninsdeletekey; Tasks: lnmplnassociation
Root: HKCR; Subkey: ".fms"; ValueType: string; ValueName: ""; ValueData: "{#LnmAppNameReg}"; Flags: uninsdeletekey; Tasks: lnmfmsassociation
Root: HKCR; Subkey: "{#LnmAppNameReg}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#LnmAppExeName}"" ""%1"""

; Disabled associations
; Root: HKCR; Subkey: ".fgfp"; ValueType: string; ValueName: ""; ValueData: "{#LnmAppNameReg}"; Flags: uninsdeletekey; Tasks: lnmfgfpssociation
; Root: HKCR; Subkey: ".gfp"; ValueType: string; ValueName: ""; ValueData: "{#LnmAppNameReg}"; Flags: uninsdeletekey; Tasks: lnmgfpassociation
; Root: HKCR; Subkey: ".flp"; ValueType: string; ValueName: ""; ValueData: "{#LnmAppNameReg}"; Flags: uninsdeletekey; Tasks: lnmflpassociation
; Root: HKCR; Subkey: "{#LnmAppNameReg}"; ValueType: string; ValueName: ""; ValueData: "Little Navmap File"; Flags: uninsdeletekey
; Root: HKCR; Subkey: "{#LnmAppNameReg}\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\littlenavmap.exe,1"

; ==========================================================================
; Start menu entries for all programs and documentation
[Icons]
Name: "{autoprograms}\{#LnmAppName} {#AppSuffix}\{#LnmAppName}"; Filename: "{app}\{#LnmAppExeName}"
Name: "{autoprograms}\{#LnmAppName} {#AppSuffix}\{#LnmAppConnectName}"; Filename: "{app}\{#LnmAppConnectName}\{#LnmAppConnectExeName}"
Name: "{autoprograms}\{#LnmAppName} {#AppSuffix}\Little Navmap {cm:UserManualMessage} (Online)"; Filename: "{app}\help\Little Navmap User Manual Online.url"
Name: "{autoprograms}\{#LnmAppName} {#AppSuffix}\{cm:ChangelogMessage}"; Filename: "{app}\CHANGELOG.TXT"
Name: "{autoprograms}\{#LnmAppName} {#AppSuffix}\{cm:ReadmeMessage}"; Filename: "{app}\README.TXT"
Name: "{autodesktop}\{#LnmAppName} {#AppSuffix}"; Filename: "{app}\{#LnmAppExeName}"; Tasks: desktopicon

; ==========================================================================
; Optional run at end
[Run]
Filename: "{app}\{#LnmAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(LnmAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
Filename: "https://www.littlenavmap.org/manuals/littlenavmap/release/latest/en/START.html"; Description: "{cm:OpenStartMessage}"; Flags: nowait shellexec postinstall skipifsilent
Filename: "{app}\CHANGELOG.txt"; Description: "{cm:ChangelogMessage}"; Flags: nowait postinstall shellexec skipifsilent
#if LnmAppArch == "win64"
  Filename: "{tmp}\VC_redist.x64.exe"; StatusMsg: "{cm:InstallingRedistMessage}"; Parameters: "/quiet /norestart"; Flags: runascurrentuser waituntilterminated
#elif LnmAppArch == "win32"
  Filename: "{tmp}\vcredist_x86.exe"; StatusMsg: "{cm:InstallingRedistMessage}"; Parameters: "/Q"; Flags: runascurrentuser waituntilterminated
#endif

[Code]
function InitializeUninstall(): Boolean;
  var ErrorCode: Integer;
begin
  MsgBox(CustomMessage('QuitProgramMessage'), mbInformation, MB_OK)
  ShellExec('open','taskkill.exe',' /im {#LnmAppExeName}', '', SW_HIDE, ewNoWait, ErrorCode);
  ShellExec('open','taskkill.exe',' /im {#LnmAppConnectExeName}', '', SW_HIDE, ewNoWait, ErrorCode);
  result := True;
end;

; ==========================================================================
; Ask to delete 'ABarthel' settings and the Marble cache files on uninstall
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
