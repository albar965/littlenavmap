@echo off

echo ============================================================================================
echo ======== build_installer.cmd ===============================================================
echo ============================================================================================

setlocal enableextensions

if defined APROJECTS ( echo APROJECTS=%APROJECTS% ) else ( echo APROJECTS not set && exit /b 1 )
if defined WINARCH ( echo WINARCH=%WINARCH% ) else ( echo WINARCH not set && exit /b 1 )

rem Get file version number and remove spaces from variable
rem LittleNavmap-win32-2.8.2.beta-Install.zip

set /p FILENAMETEMP=<"%APROJECTS%\deploy\Little Navmap %WINARCH%\version.txt"
set FILENAME_LNM=%FILENAMETEMP: =%

FOR /F "tokens=2 delims=-" %%A IN ("%FILENAME_LNM%") DO set LNM_VERSION=%%A

echo LNM_VERSION=%LNM_VERSION%
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer.iss /DLnmAppArch=%WINARCH% /DLnmAppVersion=%LNM_VERSION%
IF ERRORLEVEL 1 goto :err

echo ---- Success ----

if not "%1" == "nopause" pause

exit /b 0

:err

echo **** ERROR ****

popd

pause

exit /b 1
