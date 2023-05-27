@echo off

echo ============================================================================================
echo ======== build_installer.cmd ===============================================================
echo ============================================================================================

setlocal enableextensions

if defined APROJECTS ( echo APROJECTS=%APROJECTS% ) else ( echo APROJECTS not set && exit /b 1 )
if defined WINARCH ( echo WINARCH=%WINARCH% ) else ( echo WINARCH not set && exit /b 1 )

echo DIR IS
cd 

rem Get file version number and remove spaces from variable
rem LittleNavmap-win32-2.8.2.beta-Install.zip

set /p FILENAMETEMP=<"%APROJECTS%\deploy\Little Navmap %WINARCH%\version.txt"
set FILENAME_LNM=%FILENAMETEMP: =%
set FILENAME_LNM_RELEASE=LittleNavmap-%FILENAME_LNM%

rem variables used in installer.nsi
set /p INSTALLER_VERSION=<"%APROJECTS%\deploy\Little Navmap %WINARCH%\version.txt"
set INSTALLER_SOURCE=%APROJECTS%\deploy\%FILENAME_LNM_RELEASE%
set INSTALLER_TARGET=%APROJECTS%\deploy\%FILENAME_LNM_RELEASE%-Install.exe

xcopy /i /s /e /f /y "%APROJECTS%\littlenavmap\build\win\Little Navmap User Manual Online.url" "%INSTALLER_SOURCE%"\help\

echo INSTALLER_SOURCE=%INSTALLER_SOURCE%
echo INSTALLER_TARGET=%INSTALLER_TARGET%
echo INSTALLER_VERSION=%INSTALLER_VERSION%

"C:\Program Files (x86)\NSIS\makensis.exe" /V3 installer.nsi
IF ERRORLEVEL 1 goto :err

echo ---- Success ----

if not "%1" == "nopause" pause

exit /b 0

:err

echo **** ERROR ****

popd

pause

exit /b 1
