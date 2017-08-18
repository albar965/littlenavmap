@echo off

rem === Pull from atools, littlenavconnect, littlexpconnect and littlenavmap repositories =============================

pushd "%APROJECTS%\atools"
C:\Git\bin\git pull --verbose  --tags
IF ERRORLEVEL 1 goto :err
popd

pushd "%APROJECTS%\littlenavconnect"
C:\Git\bin\git pull  --verbose  --tags
IF ERRORLEVEL 1 goto :err
popd

pushd "%APROJECTS%\littlexpconnect"
C:\Git\bin\git pull  --verbose  --tags
IF ERRORLEVEL 1 goto :err
popd

pushd "%APROJECTS%\littlenavmap"
C:\Git\bin\git pull  --verbose  --tags
IF ERRORLEVEL 1 goto :err
popd

echo ---- Success ----

if not "%1" == "nopause" pause

exit /b 0

:err

popd

echo **** ERROR ****

pause

exit /b 1
