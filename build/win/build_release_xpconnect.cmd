@echo off

echo ============================================================================================
echo ======== build_release_xpconnect.cmd =======================================================
echo ============================================================================================

setlocal enableextensions

if defined APROJECTS ( echo APROJECTS=%APROJECTS% ) else ( echo APROJECTS not set && exit /b 1 )

rem =============================================================================
rem Set the required environment variable APROJECTS to the base directory for
rem littlexpconnect.
rem
rem =============================================================================
rem Configuration can be overloaded on the command line by setting the
rem variables below before calling this script.
rem
rem See the *.pro project files for more information.
rem
rem Example:
rem set PATH_STATIC=C:\msys64\mingw64\bin;C:\msys64\mingw64\bin
rem set XPSDK_BASE="C:\X-Plane SDK"
rem

if defined CONF_TYPE ( echo CONF_TYPE=%CONF_TYPE% ) else ( set CONF_TYPE=release)
if defined ATOOLS_INC_PATH ( echo ATOOLS_INC_PATH=%ATOOLS_INC_PATH% ) else ( set ATOOLS_INC_PATH=%APROJECTS%\atools\src)
if defined ATOOLS_LIB_PATH ( echo ATOOLS_LIB_PATH=%ATOOLS_LIB_PATH% ) else ( set ATOOLS_LIB_PATH=%APROJECTS%\build-atools-%CONF_TYPE%)
if defined DEPLOY_BASE ( echo DEPLOY_BASE=%DEPLOY_BASE% ) else ( set DEPLOY_BASE=%APROJECTS%\deploy)
if defined ATOOLS_GIT_PATH ( echo ATOOLS_GIT_PATH=%ATOOLS_GIT_PATH% ) else ( set ATOOLS_GIT_PATH=C:\Git\bin\git)

rem Windows/qmake cannot deal with paths containing spaces/quotes - defines these variables in the Windows GUI
rem if defined XPSDK_BASE ( echo %XPSDK_BASE% ) else ( set XPSDK_BASE="%APROJECTS%\X-Plane SDK")

rem Defines the used Qt for Xpconnect
if defined PATH_STATIC ( echo PATH_STATIC=%PATH_STATIC% ) else ( set PATH_STATIC=C:\msys64\mingw64\qt5-static\bin;C:\msys64\mingw64\bin)

rem === Build littlexpconnect =============================

rem ===========================================================================
rem First delete all deploy directories =======================================

@echo Deleting deploy directories =======================================
rmdir /s/q "%APROJECTS%\deploy\Little Xpconnect"
mkdir "%APROJECTS%\deploy\Little Xpconnect"

setlocal

rem ===========================================================================
rem ========================== atools 64 bit static
rmdir /s/q "%APROJECTS%\build-atools-%CONF_TYPE%"
mkdir "%APROJECTS%\build-atools-%CONF_TYPE%"

pushd "%APROJECTS%\build-atools-%CONF_TYPE%"
if errorlevel 1 goto :err

set PATH=%PATH%;%PATH_STATIC%

set ATOOLS_NO_FS=true
set ATOOLS_NO_GRIB=true
set ATOOLS_NO_GUI=true
set ATOOLS_NO_ROUTING=true
set ATOOLS_NO_SQL=true
set ATOOLS_NO_TRACK=true
set ATOOLS_NO_USERDATA=true
set ATOOLS_NO_WEATHER=true
set ATOOLS_NO_WEB=true
set ATOOLS_NO_WMM=true

qmake.exe "%APROJECTS%\atools\atools.pro" -spec win32-g++ CONFIG+=%CONF_TYPE%
if errorlevel 1 goto :err

mingw32-make.exe -j4
if errorlevel 1 goto :err
popd

rem ===========================================================================
rem ========================== littlexpconnect 64 bit static

rmdir /s/q "%APROJECTS%\build-littlexpconnect-%CONF_TYPE%"
mkdir "%APROJECTS%\build-littlexpconnect-%CONF_TYPE%"

pushd "%APROJECTS%\build-littlexpconnect-%CONF_TYPE%"
if errorlevel 1 goto :err

qmake.exe "%APROJECTS%\littlexpconnect\littlexpconnect.pro" -spec win32-g++ CONFIG+=%CONF_TYPE%
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
