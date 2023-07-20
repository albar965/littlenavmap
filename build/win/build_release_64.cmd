@echo off

echo ============================================================================================
echo ======== build_release_64.cmd ==============================================================
echo ============================================================================================

setlocal enableextensions

if defined APROJECTS ( echo APROJECTS=%APROJECTS% ) else ( echo APROJECTS not set && exit /b 1 )

rem =============================================================================
rem Set the required environment variable APROJECTS to the base directory for
rem atools, littlenavmap, littlenavconnect and littlexpconnect.
rem
rem =============================================================================
rem Configuration can be overloaded on the command line by setting the
rem variables below before calling this script.
rem
rem See the *.pro project files for more information.
rem
rem Example:
rem set PATH_SHARED=C:\Qt\5.12.11\mingw73_32\bin;C:\Qt\Tools\mingw730_32\bin
rem set PATH_STATIC=C:\msys64\mingw64\bin;C:\msys64\mingw64\bin
rem set MARBLE_LIB_PATH=C:\Users\YOURNAME\Programme\Marble-debug\lib
rem set MARBLE_INC_PATH=C:\Users\YOURNAME\Programme\Marble-debug\include
rem set OPENSSL_PATH="C:\Program Files (x86)\OpenSSL-Win32"
rem set XPSDK_BASE="C:\X-Plane SDK"
rem
rem See links below for OpenSSL binaries
rem https://wiki.openssl.org/index.php/Binaries and https://bintray.com/vszakats/generic/openssl

set WINARCH=win64

if defined CONF_TYPE ( echo CONF_TYPE=%CONF_TYPE% ) else ( set CONF_TYPE=release)
if defined ATOOLS_INC_PATH ( echo ATOOLS_INC_PATH=%ATOOLS_INC_PATH% ) else ( set ATOOLS_INC_PATH=%APROJECTS%\atools\src)
if defined ATOOLS_LIB_PATH ( echo ATOOLS_LIB_PATH=%ATOOLS_LIB_PATH% ) else ( set ATOOLS_LIB_PATH=%APROJECTS%\build-atools-%CONF_TYPE%)
if defined MARBLE_INC_PATH ( echo MARBLE_INC_PATH=%MARBLE_INC_PATH% ) else ( set MARBLE_INC_PATH=%APROJECTS%\Marble-%CONF_TYPE%-%WINARCH%\include)
if defined MARBLE_LIB_PATH ( echo MARBLE_LIB_PATH=%MARBLE_LIB_PATH% ) else ( set MARBLE_LIB_PATH=%APROJECTS%\Marble-%CONF_TYPE%-%WINARCH%\lib)
if defined DEPLOY_BASE ( echo DEPLOY_BASE=%DEPLOY_BASE% ) else ( set DEPLOY_BASE=%APROJECTS%\deploy)
if defined DATABASE_BASE ( echo DATABASE_BASE=%DATABASE_BASE% ) else ( set DATABASE_BASE=%APROJECTS%\little_navmap_db)
if defined HELP_BASE ( echo HELP_BASE=%HELP_BASE% ) else ( set HELP_BASE=%APROJECTS%\little_navmap_help)
if defined ATOOLS_GIT_PATH ( echo ATOOLS_GIT_PATH=%ATOOLS_GIT_PATH% ) else ( set ATOOLS_GIT_PATH=C:\Git\bin\git)

rem Default points to Qt installation
rem if defined OPENSSL_PATH ( echo %OPENSSL_PATH% ) else ( set OPENSSL_PATH="C:\Qt\Tools\OpenSSL\Win_x86\bin\")

rem Windows/qmake cannot deal with paths containing spaces/quotes - defines these variables in the Windows GUI
rem if defined ATOOLS_SIMCONNECT_PATH ( echo ATOOLS_SIMCONNECT_PATH ) else ( set ATOOLS_SIMCONNECT_PATH="C:\Program Files (x86)\Microsoft Games\Microsoft Flight Simulator X SDK\SDK\Core Utilities Kit\SimConnect SDK")
rem if defined XPSDK_BASE ( echo %XPSDK_BASE% ) else ( set XPSDK_BASE="%APROJECTS%\X-Plane SDK")

rem Defines the used Qt for all builds
if defined PATH_SHARED ( echo PATH_SHARED=%PATH_SHARED% ) else ( set PATH_SHARED=C:\Qt\5.15.2\mingw81_64\bin;C:\Qt\Tools\mingw810_64\bin)

call build_release_base.cmd nopause
if errorlevel 1 goto :err

echo ---- Success ----

if not "%1" == "nopause" pause

exit /b 0

:err

echo **** ERROR ****

popd

pause

exit /b 1
