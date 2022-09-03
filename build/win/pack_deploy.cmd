@echo off

if defined APROJECTS ( echo %APROJECTS% ) else ( echo APROJECTS not set && exit /b 1 )

rem Override by envrionment variable for another target or leave empty for no copying - needs putty tools in path
rem set SSH_DEPLOY_TARGET=user@host:/data/alex/Public/Releases

rem === Deploy built programs. ZIP, check with Windows Defender and copy them to network shares =============================

pushd "%APROJECTS%\deploy"

rem ===========================================================================
rem ==== Pack Little Navconnect ===================================================================
del LittleNavconnect.zip

"C:\Program Files\7-Zip\7z.exe" a LittleNavconnect.zip "Little Navconnect"
IF ERRORLEVEL 1 goto :err

"C:\Program Files\Windows Defender\MpCmdRun.exe" -Scan -ScanType 3 -DisableRemediation -File "%APROJECTS%\deploy\LittleNavconnect.zip"
IF ERRORLEVEL 1 goto :err

rem ===========================================================================
rem ==== Pack Little Xpconnect ===================================================================
del LittleXpconnect.zip

"C:\Program Files\7-Zip\7z.exe" a LittleXpconnect.zip "Little Xpconnect"
IF ERRORLEVEL 1 goto :err

"C:\Program Files\Windows Defender\MpCmdRun.exe" -Scan -ScanType 3 -DisableRemediation -File "%APROJECTS%\deploy\LittleXpconnect.zip"
IF ERRORLEVEL 1 goto :err

rem ===========================================================================
rem ==== Pack Little Navmap ===================================================================
del LittleNavmap.zip

"C:\Program Files\7-Zip\7z.exe" a LittleNavmap.zip "Little Navmap"
IF ERRORLEVEL 1 goto :err

"C:\Program Files\Windows Defender\MpCmdRun.exe" -Scan -ScanType 3 -DisableRemediation -File "%APROJECTS%\deploy\LittleNavmap.zip"
IF ERRORLEVEL 1 goto :err

rem ===========================================================================
rem ==== Copy all =============================================================

rem Get file version number and emove spaces from variable
set /p FILENAMETEMP=<"%APROJECTS%\deploy\Little Navmap\version.txt"
set FILENAME_LNM=%FILENAMETEMP: =%

set /p FILENAMETEMP=<"%APROJECTS%\deploy\Little Navconnect\version.txt"
set FILENAME_LNC=%FILENAMETEMP: =%

set /p FILENAMETEMP=<"%APROJECTS%\deploy\Little Xpconnect\version.txt"
set FILENAME_LXP=%FILENAMETEMP: =%

if defined SSH_DEPLOY_TARGET (
pscp -i %HOMEDRIVE%\%HOMEPATH%\.ssh\id_rsa LittleNavmap.zip %SSH_DEPLOY_TARGET%/LittleNavmap-win-%FILENAME_LNM%.zip
rem pscp -i %HOMEDRIVE%\%HOMEPATH%\.ssh\id_rsa LittleNavconnect.zip %SSH_DEPLOY_TARGET%/LittleNavconnect-win-%FILENAME_LNC%.zip
rem pscp -i %HOMEDRIVE%\%HOMEPATH%\.ssh\id_rsa LittleXpconnect.zip %SSH_DEPLOY_TARGET%/LittleXpconnect-win-%FILENAME_LXP%.zip
)

popd

echo ---- Success ----

if not "%1" == "nopause" pause

exit /b 0

:err

echo **** ERROR ****

popd

pause

exit /b 1
