@echo off

rem === Pull, build and deploy atools, littlenavconnect and littlenavmap =============================

call pull_all.cmd nopause
IF ERRORLEVEL 1 goto :err

call build_release.cmd nopause
IF ERRORLEVEL 1 goto :err

call pack_deploy.cmd nopause
IF ERRORLEVEL 1 goto :err

echo -------------------------
echo ---- Success for all ----
echo -------------------------

pause

exit /b 0

:err

echo **** ERROR ****

pause

exit /b 1
