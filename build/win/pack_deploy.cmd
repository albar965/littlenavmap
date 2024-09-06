@echo off

echo ============================================================================================
echo ======== pack_deploy.cmd ===================================================================
echo ============================================================================================

setlocal enableextensions

if defined APROJECTS ( echo APROJECTS=%APROJECTS% ) else ( echo APROJECTS not set && exit /b 1 )
if defined WINARCH ( echo WINARCH=%WINARCH% ) else ( echo WINARCH not set && exit /b 1 )

rem Override by envrionment variable for another target or leave empty for no copying - needs putty tools in path
rem set SSH_DEPLOY_TARGET=user@host:/data/alex/Public/Releases

rem === Deploy built programs. ZIP, check with Windows Defender and copy them to network shares =============================

pushd "%APROJECTS%\deploy"
if errorlevel 1 goto :err

rem Get file version number and remove spaces from variable
rem LittleNavmap-win32-2.8.2.beta.zip
set /p FILENAMETEMP=<"%APROJECTS%\deploy\Little Navmap %WINARCH%\version.txt"
set FILENAME_LNM=%FILENAMETEMP: =%
set FILENAME_LNM_RELEASE=LittleNavmap-%FILENAME_LNM%

rem "%APROJECTS%\deploy\Little Navmap win64" to "%APROJECTS%\deploy\LittleNavmap-win64-3.0.9"
rmdir /Q /S "%APROJECTS%\deploy\%FILENAME_LNM_RELEASE%"
xcopy /i /s /e /f /y "%APROJECTS%\deploy\Little Navmap %WINARCH%" "%APROJECTS%\deploy\%FILENAME_LNM_RELEASE%"
if errorlevel 1 goto :err

rem ===========================================================================
rem Copy navconnect ===========================================================
rem "%APROJECTS%\deploy\Little Navconnect win64" to "%APROJECTS%\deploy\LittleNavmap-win64-3.0.9\Little Navconnect"
xcopy /i /s /e /f /y "%APROJECTS%\deploy\Little Navconnect %WINARCH%" "%APROJECTS%\deploy\%FILENAME_LNM_RELEASE%\Little Navconnect"
if errorlevel 1 goto :err

rem ===========================================================================
rem Copy xpconnect ============================================================
rem "%APROJECTS%\deploy\Little Xpconnect" to "%APROJECTS%\deploy\LittleNavmap-win64-3.0.9\Little Xpconnect"
xcopy /i /s /e /f /y "%APROJECTS%\deploy\Little Xpconnect" "%APROJECTS%\deploy\%FILENAME_LNM_RELEASE%\Little Xpconnect"
if errorlevel 1 goto :err

rem ===========================================================================
rem ==== Build installer ======================================================
popd
call build_installer.cmd nopause
if errorlevel 1 goto :err
pushd "%APROJECTS%\deploy"

rem ===========================================================================
rem ==== Pack Little Navmap ===================================================
del "%APROJECTS%\deploy\%FILENAME_LNM_RELEASE%.zip"

rem Check and copy installer =======================================
"C:\Program Files\Windows Defender\MpCmdRun.exe" -Scan -ScanType 3 -DisableRemediation -File "%APROJECTS%\deploy\%FILENAME_LNM_RELEASE%-Install.exe"
if errorlevel 1 goto :err

if defined SSH_DEPLOY_TARGET (
pscp -i %HOMEDRIVE%\%HOMEPATH%\.ssh\id_rsa "%APROJECTS%\deploy\%FILENAME_LNM_RELEASE%-Install.exe" %SSH_DEPLOY_TARGET%/%FILENAME_LNM_RELEASE%-Install.exe
)

rem Create, check and copy Zip =======================================
"C:\Program Files\7-Zip\7z.exe" -mx9 -mm=Deflate -mfb=258 -mpass=15 a "%APROJECTS%\deploy\%FILENAME_LNM_RELEASE%.zip" "%APROJECTS%\deploy\%FILENAME_LNM_RELEASE%" -xr!littlenavmap.debug -xr!littlenavconnect.debug
if errorlevel 1 goto :err

"C:\Program Files\Windows Defender\MpCmdRun.exe" -Scan -ScanType 3 -DisableRemediation -File "%APROJECTS%\deploy\%FILENAME_LNM_RELEASE%.zip"
if errorlevel 1 goto :err

if defined SSH_DEPLOY_TARGET (
pscp -i %HOMEDRIVE%\%HOMEPATH%\.ssh\id_rsa "%APROJECTS%\deploy\%FILENAME_LNM_RELEASE%.zip" %SSH_DEPLOY_TARGET%/%FILENAME_LNM_RELEASE%.zip
)

rem Create , check and copy Zip =======================================
"C:\Program Files\7-Zip\7z.exe" a -r "%APROJECTS%\deploy\%FILENAME_LNM_RELEASE%_debug.zip" "%APROJECTS%\deploy\%FILENAME_LNM_RELEASE%\*.debug"
if errorlevel 1 goto :err

"C:\Program Files\Windows Defender\MpCmdRun.exe" -Scan -ScanType 3 -DisableRemediation -File "%APROJECTS%\deploy\%FILENAME_LNM_RELEASE%_debug.zip"
if errorlevel 1 goto :err

if defined SSH_DEPLOY_TARGET (
pscp -i %HOMEDRIVE%\%HOMEPATH%\.ssh\id_rsa "%APROJECTS%\deploy\%FILENAME_LNM_RELEASE%_debug.zip" %SSH_DEPLOY_TARGET%/Debug/%FILENAME_LNM_RELEASE%_debug.zip"
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
