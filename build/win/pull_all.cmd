@echo off

cd "%APROJECTS%\atools"
git pull --verbose  --tags
IF ERRORLEVEL 1 goto :err

cd "%APROJECTS%\littlenavconnect"
git pull  --verbose  --tags
IF ERRORLEVEL 1 goto :err

cd "%APROJECTS%\littlenavmap"
git pull  --verbose  --tags
IF ERRORLEVEL 1 goto :err

cd "%APROJECTS%\navdatareader"
git pull --verbose  --tags
IF ERRORLEVEL 1 goto :err


echo ---- Success ----

cd "%APROJECTS%"

if not "%1" == "nopause" pause

exit /b 0

:err

echo **** ERROR ****

cd "%APROJECTS%"

pause 

exit /b 1