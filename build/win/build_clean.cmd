@echo off

echo ============================================================================================
echo ======== build_clean.cmd ===================================================================
echo ============================================================================================

setlocal enableextensions

if defined APROJECTS ( echo APROJECTS=%APROJECTS% ) else ( echo APROJECTS not set && exit /b 1 )
if defined CONF_TYPE ( echo CONF_TYPE=%CONF_TYPE% ) else ( set CONF_TYPE=release)

setlocal

rem ===========================================================================
rem ========================== atools
:atools

rmdir /s/q "%APROJECTS%\build-atools-%CONF_TYPE%"
mkdir "%APROJECTS%\build-atools-%CONF_TYPE%"

rem ===========================================================================
rem ========================== littlenavconnect
:littlenavconnect

rmdir /s/q "%APROJECTS%\build-littlenavconnect-%CONF_TYPE%"
mkdir "%APROJECTS%\build-littlenavconnect-%CONF_TYPE%"

rem ===========================================================================
rem ========================== littlenavmap
:littlenavmap

rmdir /s/q "%APROJECTS%\build-littlenavmap-%CONF_TYPE%"
mkdir "%APROJECTS%\build-littlenavmap-%CONF_TYPE%"

rem ===========================================================================
rem ========================== littlexpconnect
:littlexpconnect

rmdir /s/q "%APROJECTS%\build-littlexpconnect-%CONF_TYPE%"
mkdir "%APROJECTS%\build-littlexpconnect-%CONF_TYPE%"

endlocal

echo ---- Success ----

if not "%1" == "nopause" pause

exit /b 0

:err

echo **** ERROR ****

popd

pause

exit /b 1
