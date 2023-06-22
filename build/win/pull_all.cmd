@echo off

echo ============================================================================================
echo ======== pull_all.cmd ======================================================================
echo ============================================================================================

setlocal enableextensions

if defined APROJECTS ( echo APROJECTS=%APROJECTS% ) else ( echo APROJECTS not set && exit /b 1 )

rem === Pull from atools, littlenavconnect, littlexpconnect and littlenavmap repositories =============================

pushd "%APROJECTS%\atools"
"%ATOOLS_GIT_PATH%" pull --verbose  --tags
if errorlevel 1 goto :err
popd

pushd "%APROJECTS%\littlenavconnect"
"%ATOOLS_GIT_PATH%" pull  --verbose  --tags
if errorlevel 1 goto :err
popd

pushd "%APROJECTS%\littlexpconnect"
"%ATOOLS_GIT_PATH%" pull  --verbose  --tags
if errorlevel 1 goto :err
popd

pushd "%APROJECTS%\littlenavmap"
"%ATOOLS_GIT_PATH%" pull  --verbose  --tags
if errorlevel 1 goto :err
popd

echo ---- Success ----

if not "%1" == "nopause" pause

exit /b 0

:err

popd

echo **** ERROR ****

pause

exit /b 1
