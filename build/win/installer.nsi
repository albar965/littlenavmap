;NSIS Modern User Interface
;Written by Harvey Walker

;--------------------------------
;Include Modern UI

  !include "MUI2.nsh"

;--------------------------------
;General

  ;Name and file
  Name "Little Navmap"
  OutFile "$%INSTALLER_TARGET%"
  Unicode True

  ;Default installation folder
  InstallDir "$LOCALAPPDATA\Little Navmap"

  ;Request application privileges for Windows
  RequestExecutionLevel user

;--------------------------------
;Variables

  Var StartMenuFolder

;--------------------------------
;Interface Settings

  !define MUI_ABORTWARNING
  !define MUI_FINISHPAGE_NOAUTOCLOSE

  ; This will make the logo of the installer
  !define MUI_ICON "$%APROJECTS%\littlenavmap\resources\icons\littlenavmap.ico"

;--------------------------------
;Pages
  !insertmacro MUI_PAGE_WELCOME
  !insertmacro MUI_PAGE_LICENSE "$%APROJECTS%\littlenavmap\LICENSE.txt"
  !insertmacro MUI_PAGE_COMPONENTS
  !insertmacro MUI_PAGE_DIRECTORY

  !insertmacro MUI_PAGE_STARTMENU Application $StartMenuFolder

  !insertmacro MUI_PAGE_INSTFILES

  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES

  !insertmacro MUI_PAGE_FINISH

;TODO Start LNM option (checkbox) at end of installation

;--------------------------------
;Languages

  !insertmacro MUI_LANGUAGE "English"

;--------------------------------
;Installer Sections

Section "Little Navmap" SecDummy

  SetOutPath "$INSTDIR"

  File /r "$%INSTALLER_SOURCE%\*"

  ;Create uninstaller
  WriteUninstaller "$INSTDIR\uninstall.exe"

  ;Registry keys for uninstaller
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Little Navmap" "DisplayName" "Little Navmap"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Little Navmap" "DisplayIcon" "$INSTDIR\littlenavmap.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Little Navmap" "DisplayVersion" "2.8.10"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Little Navmap" "Publisher" "Alexander Barthel"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Little Navmap" "UninstallString" "$INSTDIR\uninstall.exe"

  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application

  ; Create start menu shortcuts
  ; C:\Users\USERNAME\AppData\Roaming\Microsoft\Windows\Start Menu\Programs\Little Navmap\
  CreateDirectory "$SMPROGRAMS\Little Navmap"
  CreateShortcut "$SMPROGRAMS\Little Navmap\Little Navmap.lnk" "$INSTDIR\littlenavmap.exe"
  CreateShortcut "$SMPROGRAMS\Little Navmap\Little Navconnect.lnk" "$INSTDIR\Little Navconnect\littlenavconnect.exe"
  CreateShortcut "$SMPROGRAMS\Little Navmap\Little Navmap User Manual Online.lnk" "$INSTDIR\help\Little Navmap User Manual Online.url"
  CreateShortcut "$SMPROGRAMS\Little Navmap\Little Navmap User Manual Local PDF.lnk" "$INSTDIR\help\little-navmap-user-manual-en.pdf"
  CreateShortcut "$SMPROGRAMS\Little Navmap\Little Navmap Changelog.lnk" "$INSTDIR\CHANGELOG.txt"
  CreateShortcut "$SMPROGRAMS\Little Navmap\Uninstall Little Navmap.lnk" "$INSTDIR\uninstall.exe"

  !insertmacro MUI_STARTMENU_WRITE_END

SectionEnd

;--------------------------------
;Descriptions

  ;Language strings
  LangString DESC_SecDummy ${LANG_ENGLISH} "Language Dummy."

  ;Assign language strings to sections
  !insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecDummy} $(DESC_SecDummy)
  !insertmacro MUI_FUNCTION_DESCRIPTION_END

;--------------------------------
;Uninstaller Section

;TODO Offer an option to delete settings and cache too

Section "Uninstall"

  Delete "$INSTDIR\uninstall.exe"

  ;TODO This will remove the whole programs folder if user selected the wrong installation folder, for example
  ;Solve by using a list of installed files and remove only these
  RMDir /r "$INSTDIR"

  !insertmacro MUI_STARTMENU_GETFOLDER Application $StartMenuFolder

  Delete "$SMPROGRAMS\Little Navmap\*"
  RMDir "$SMPROGRAMS\Little Navmap"

  # Remove uninstaller information from the registry
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Little Navmap"

SectionEnd
