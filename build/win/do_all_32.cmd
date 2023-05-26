@echo off

setlocal enableextensions

if defined APROJECTS ( echo %APROJECTS% ) else ( echo APROJECTS not set && exit /b 1 )

rem === Pull, build and deploy atools, littlenavconnect and littlenavmap =============================

set WINARCH=win32

call pull_all.cmd nopause
IF ERRORLEVEL 1 goto :err

call build_release_32.cmd nopause
IF ERRORLEVEL 1 goto :err

call build_release_xpconnect.cmd nopause
IF ERRORLEVEL 1 goto :err

call build_installer.cmd nopause
IF ERRORLEVEL 1 goto :err

call pack_deploy.cmd nopause
IF ERRORLEVEL 1 goto :err

call build_clean.cmd nopause

echo -------------------------
echo ---- Success for all ----
echo -------------------------

if not "%1" == "nopause" pause

exit /b 0

:err

echo **** ERROR ****

pause

exit /b 1
