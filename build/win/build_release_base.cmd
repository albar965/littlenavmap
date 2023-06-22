@echo off

echo ============================================================================================
echo ======== build_release_base.cmd ============================================================
echo ============================================================================================

setlocal enableextensions

if defined APROJECTS ( echo APROJECTS=%APROJECTS% ) else ( echo APROJECTS not set && exit /b 1 )
if defined WINARCH ( echo WINARCH=%WINARCH% ) else ( echo WINARCH not set && exit /b 1 )

rem === Build atools, littlenavconnect and littlenavmap =============================

rem ===========================================================================
rem First delete all deploy directories =======================================

@echo Deleting deploy directories =======================================
rmdir /s/q "%APROJECTS%\deploy\Little Navmap %WINARCH%"
mkdir "%APROJECTS%\deploy\Little Navmap %WINARCH%"

rmdir /s/q "%APROJECTS%\deploy\Little Navconnect %WINARCH%"
mkdir "%APROJECTS%\deploy\Little Navconnect %WINARCH%"

setlocal

set PATH=%PATH%;%PATH_SHARED%

rem ===========================================================================
rem ========================== atools
:atools

rmdir /s/q "%APROJECTS%\build-atools-%CONF_TYPE%"
mkdir "%APROJECTS%\build-atools-%CONF_TYPE%"

pushd "%APROJECTS%\build-atools-%CONF_TYPE%"
if errorlevel 1 goto :err

qmake.exe "%APROJECTS%\atools\atools.pro" -spec win32-g++ CONFIG+=%CONF_TYPE%
if errorlevel 1 goto :err

mingw32-make.exe -j4
if errorlevel 1 goto :err

popd

rem ===========================================================================
rem ========================== littlenavconnect
:littlenavconnect

rmdir /s/q "%APROJECTS%\build-littlenavconnect-%CONF_TYPE%"
mkdir "%APROJECTS%\build-littlenavconnect-%CONF_TYPE%"

pushd "%APROJECTS%\build-littlenavconnect-%CONF_TYPE%"
if errorlevel 1 goto :err

qmake.exe "%APROJECTS%\littlenavconnect\littlenavconnect.pro" -spec win32-g++ CONFIG+=%CONF_TYPE%
if errorlevel 1 goto :err

mingw32-make.exe -j4
if errorlevel 1 goto :err

mingw32-make.exe deploy
if errorlevel 1 goto :err

popd

rem ===========================================================================
rem ========================== littlenavmap
:littlenavmap

rmdir /s/q "%APROJECTS%\build-littlenavmap-%CONF_TYPE%"
mkdir "%APROJECTS%\build-littlenavmap-%CONF_TYPE%"

pushd "%APROJECTS%\build-littlenavmap-%CONF_TYPE%"
if errorlevel 1 goto :err

qmake.exe "%APROJECTS%\littlenavmap\littlenavmap.pro" -spec win32-g++ CONFIG+=%CONF_TYPE%
if errorlevel 1 goto :err

mingw32-make.exe -j4
if errorlevel 1 goto :err

mingw32-make.exe deploy
if errorlevel 1 goto :err

popd
endlocal

echo ---- Success ----

if not "%1" == "nopause" pause

exit /b 0

:err

echo **** ERROR ****

popd

pause

exit /b 1
