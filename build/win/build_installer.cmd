@echo off

echo ============================================================================================
echo ======== build_installer.cmd ===============================================================
echo ============================================================================================

setlocal enableextensions

if defined APROJECTS ( echo APROJECTS=%APROJECTS% ) else ( echo APROJECTS not set && exit /b 1 )
if defined WINARCH ( echo WINARCH=%WINARCH% ) else ( echo WINARCH not set && exit /b 1 )

rem Get file version number and remove spaces from variable
set /p FILENAMETEMP=<"%APROJECTS%\deploy\Little Navmap %WINARCH%\version.txt"
set FILENAME_LNM=%FILENAMETEMP: =%

rem get version only after "-" without the build type
for /F "tokens=2 delims=-" %%A in ("%FILENAME_LNM%") do set LNM_VERSION=%%A

"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer.iss /DLnmAppArch=%WINARCH%^
        /DLnmAppVersion=%LNM_VERSION% /DLnmAppId=%LNM_APPID% /DLnmAppCompress="lzma2/max"
if errorlevel 1 goto :err

echo ---- Success ----

if not "%1" == "nopause" pause

exit /b 0

:err

echo **** ERROR ****

popd

pause

exit /b 1
