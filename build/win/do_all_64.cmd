@echo off

echo ============================================================================================
echo ======== do_all_64.cmd =====================================================================
echo ============================================================================================

setlocal enableextensions

if defined APROJECTS ( echo APROJECTS=%APROJECTS% ) else ( echo APROJECTS not set && exit /b 1 )

rem === Pull, build and deploy atools, littlenavconnect and littlenavmap =============================

set WINARCH=win64

call pull_all.cmd nopause
if errorlevel 1 goto :err

call build_release_64.cmd nopause
if errorlevel 1 goto :err

call build_release_xpconnect.cmd nopause
if errorlevel 1 goto :err

call pack_deploy.cmd nopause
if errorlevel 1 goto :err

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
