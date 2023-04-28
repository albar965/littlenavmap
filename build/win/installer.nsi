;NSIS Modern User Interface
;Written by Harvey Walker



;--------------------------------
;Include Modern UI & uninstall log

  !include "MUI2.nsh"
  !include "UnInstallLog.nsh"

;--------------------------------s
;Uninstaller Log Section

  ;Set the name of the uninstall log
    !define UninstLog "uninstall.log"
    Var UninstLog
  ;The root registry to write to
    !define REG_ROOT "HKLM"
  ;The registry path to write to
    !define REG_APP_PATH "SOFTWARE\appname"
 
  ;Uninstall log file missing.
    LangString UninstLogMissing ${LANG_ENGLISH} "${UninstLog} not found!$\r$\nUninstallation cannot proceed!"
 
  ;AddItem macro
    !define AddItem "!insertmacro AddItem"
 
  ;BackupFile macro
    !define BackupFile "!insertmacro BackupFile" 
 
  ;BackupFiles macro
    !define BackupFiles "!insertmacro BackupFiles" 
 
  ;Copy files macro
    !define CopyFiles "!insertmacro CopyFiles"
 
  ;CreateDirectory macro
    !define CreateDirectory "!insertmacro CreateDirectory"
 
  ;CreateShortcut macro
    !define CreateShortcut "!insertmacro CreateShortcut"
 
  ;File macro
    !define File "!insertmacro File"
 
  ;Rename macro
    !define Rename "!insertmacro Rename"
 
  ;RestoreFile macro
    !define RestoreFile "!insertmacro RestoreFile"    
 
  ;RestoreFiles macro
    !define RestoreFiles "!insertmacro RestoreFiles"
 
  ;SetOutPath macro
    !define SetOutPath "!insertmacro SetOutPath"
 
  ;WriteRegDWORD macro
    !define WriteRegDWORD "!insertmacro WriteRegDWORD" 
 
  ;WriteRegStr macro
    !define WriteRegStr "!insertmacro WriteRegStr"
 
  ;WriteUninstaller macro
    !define WriteUninstaller "!insertmacro WriteUninstaller"
 
  Section -openlogfile
    CreateDirectory "$INSTDIR"
    IfFileExists "$INSTDIR\${UninstLog}" +3
      FileOpen $UninstLog "$INSTDIR\${UninstLog}" w
    Goto +4
      SetFileAttributes "$INSTDIR\${UninstLog}" NORMAL
      FileOpen $UninstLog "$INSTDIR\${UninstLog}" a
      FileSeek $UninstLog 0 END
  SectionEnd

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

  !define MUI_FINISHPAGE_RUN
  !define MUI_FINISHPAGE_RUN_TEXT "Run Little Navmap"
  !define MUI_FINISHPAGE_RUN_FUNCTION LaunchLNM
  !insertmacro MUI_PAGE_FINISH

;--------------------------------
;Languages

  !insertmacro MUI_LANGUAGE "English"

;--------------------------------
;Installer Sections

Section "Little Navmap" SecDummy

  ${SetOutPath} "$INSTDIR"

  File /r "$%INSTALLER_SOURCE%\*"

  ;Create uninstaller
  ${WriteUninstaller} "$INSTDIR\uninstall.exe"

  ;Registry keys for uninstaller
  ${WriteRegStr} HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Little Navmap" "DisplayName" "Little Navmap"
  ${WriteRegStr} HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Little Navmap" "DisplayIcon" "$INSTDIR\littlenavmap.exe"
  ${WriteRegStr} HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Little Navmap" "DisplayVersion" "2.8.10"
  ${WriteRegStr} HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Little Navmap" "Publisher" "Alexander Barthel"
  ${WriteRegStr} HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Little Navmap" "UninstallString" "$INSTDIR\uninstall.exe"

  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application

  ; Create start menu shortcuts
  ; C:\Users\USERNAME\AppData\Roaming\Microsoft\Windows\Start Menu\Programs\Little Navmap
  ${CreateDirectory} "$SMPROGRAMS\Little Navmap"
  ${CreateShortcut} "$SMPROGRAMS\Little Navmap\Little Navmap.lnk"  "$INSTDIR\littlenavmap.exe"
  ${CreateShortcut} "$SMPROGRAMS\Little Navmap\Little Navconnect.lnk" "$INSTDIR\Little Navconnect\littlenavconnect.exe"
  ${CreateShortcut} "$SMPROGRAMS\Little Navmap\Little Navmap User Manual Online.lnk" "$INSTDIR\help\Little Navmap User Manual Online.url"
  ${CreateShortcut} "$SMPROGRAMS\Little Navmap\Little Navmap User Manual Local PDF.lnk" "$INSTDIR\help\little-navmap-user-manual-en.pdf"
  ${CreateShortcut} "$SMPROGRAMS\Little Navmap\Little Navmap Changelog.lnk" "$INSTDIR\CHANGELOG.txt"
  ${CreateShortcut} "$SMPROGRAMS\Little Navmap\Uninstall Little Navmap.lnk" "$INSTDIR\uninstall.exe"

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


;--------------------------------
; Uninstaller
;--------------------------------

Section Uninstall
  ;Can't uninstall if uninstall log is missing!
  IfFileExists "$INSTDIR\${UninstLog}" +3
    MessageBox MB_OK|MB_ICONSTOP "$(UninstLogMissing)"
      Abort
 
  Push $R0
  Push $R1
  Push $R2
  SetFileAttributes "$INSTDIR\${UninstLog}" NORMAL
  FileOpen $UninstLog "$INSTDIR\${UninstLog}" r
  StrCpy $R1 -1
 
  GetLineCount:
    ClearErrors
    FileRead $UninstLog $R0
    IntOp $R1 $R1 + 1
    StrCpy $R0 $R0 -2
    Push $R0   
    IfErrors 0 GetLineCount
 
  Pop $R0
 
  LoopRead:
    StrCmp $R1 0 LoopDone
    Pop $R0
 
    IfFileExists "$R0\*.*" 0 +3
      RMDir $R0  #is dir
    Goto +9
    IfFileExists $R0 0 +3
      Delete $R0 #is file
    Goto +6
    StrCmp $R0 "${REG_ROOT} ${REG_APP_PATH}" 0 +3
      DeleteRegKey ${REG_ROOT} "${REG_APP_PATH}" #is Reg Element
    Goto +3
    StrCmp $R0 "${REG_ROOT} ${UNINSTALL_PATH}" 0 +2
      DeleteRegKey ${REG_ROOT} "${UNINSTALL_PATH}" #is Reg Element
 
    IntOp $R1 $R1 - 1
    Goto LoopRead
  LoopDone:
  FileClose $UninstLog
  Delete "$INSTDIR\${UninstLog}"
  RMDir "$INSTDIR"
  Pop $R2
  Pop $R1
  Pop $R0
 
  ;Remove registry keys
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Little Navmap"
SectionEnd

Function LaunchLNM
  ExecShell "" "$INSTDIR\littlenavmap.exe"
FunctionEnd