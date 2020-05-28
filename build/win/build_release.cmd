@echo off

setlocal enableextensions

if defined APROJECTS ( echo %APROJECTS% ) else ( echo APROJECTS not set && exit /b 1 )

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
rem set PATH_SHARED=C:\Qt\5.12.8\mingw73_32\bin;C:\Qt\Tools\mingw730_32\bin
rem set PATH_STATIC=C:\msys64\mingw64\bin;C:\msys64\mingw64\bin
rem set MARBLE_LIB_PATH=C:\Users\YOURNAME\Programme\Marble-debug\lib
rem set MARBLE_INC_PATH=C:\Users\YOURNAME\Programme\Marble-debug\include
rem set OPENSSL_PATH="C:\Program Files (x86)\OpenSSL-Win32"
rem set XPSDK_BASE="C:\X-Plane SDK"
rem
rem See links below for OpenSSL binaries
rem https://wiki.openssl.org/index.php/Binaries and https://bintray.com/vszakats/generic/openssl

if defined CONF_TYPE ( echo %CONF_TYPE% ) else ( set CONF_TYPE=release)
if defined ATOOLS_INC_PATH ( echo %ATOOLS_INC_PATH% ) else ( set ATOOLS_INC_PATH=%APROJECTS%\atools\src)
if defined ATOOLS_LIB_PATH ( echo %ATOOLS_LIB_PATH% ) else ( set ATOOLS_LIB_PATH=%APROJECTS%\build-atools-%CONF_TYPE%)
if defined MARBLE_INC_PATH ( echo %MARBLE_INC_PATH% ) else ( set MARBLE_INC_PATH=%APROJECTS%\Marble-%CONF_TYPE%\include)
if defined MARBLE_LIB_PATH ( echo %MARBLE_LIB_PATH% ) else ( set MARBLE_LIB_PATH=%APROJECTS%\Marble-%CONF_TYPE%\lib)
if defined DEPLOY_BASE ( echo %DEPLOY_BASE% ) else ( set DEPLOY_BASE=%APROJECTS%\deploy)
if defined DATABASE_BASE ( echo %DATABASE_BASE% ) else ( set DATABASE_BASE=%APROJECTS%\little_navmap_db)
if defined HELP_BASE ( echo %HELP_BASE% ) else ( set HELP_BASE=%APROJECTS%\little_navmap_help)
if defined ATOOLS_GIT_PATH ( echo %ATOOLS_GIT_PATH% ) else ( set ATOOLS_GIT_PATH=C:\Git\bin\git)
if defined OPENSSL_PATH ( echo %OPENSSL_PATH% ) else ( set OPENSSL_PATH="%APROJECTS%\openssl-1.1.1d-win32-mingw")

rem Windows/qmake cannot deal with paths containing spaces/quotes - defines these variables in the Windows GUI
rem if defined ATOOLS_SIMCONNECT_PATH ( echo ATOOLS_SIMCONNECT_PATH ) else ( set ATOOLS_SIMCONNECT_PATH="C:\Program Files (x86)\Microsoft Games\Microsoft Flight Simulator X SDK\SDK\Core Utilities Kit\SimConnect SDK")
rem if defined XPSDK_BASE ( echo %XPSDK_BASE% ) else ( set XPSDK_BASE="%APROJECTS%\X-Plane SDK")

rem Defines the used Qt for all builds
if defined PATH_SHARED ( echo %PATH_SHARED% ) else ( set PATH_SHARED=C:\Qt\5.12.8\mingw73_32\bin;C:\Qt\Tools\mingw730_32\bin)

rem Defines the used Qt for Xpconnect
if defined PATH_STATIC ( echo %PATH_STATIC% ) else ( set PATH_STATIC=C:\msys64\mingw64\qt5-static\bin;C:\msys64\mingw64\bin)

rem === Build atools, littlenavconnect and littlenavmap =============================
rem === Merge all files into one littlenavmap directory ===========================


rem ===========================================================================
rem First delete all deploy directories =======================================
pushd "%APROJECTS%\deploy"
del /S /Q /F "%APROJECTS%\deploy\Little Navmap"
for /f %%f in ('dir /ad /b "%APROJECTS%\deploy\Little Navmap"') do rd /s /q "%APROJECTS%\deploy\Little Navmap\%%f"
rd /S /Q "%APROJECTS%\deploy\Little Navmap"
IF ERRORLEVEL 1 goto :err

del /S /Q /F "%APROJECTS%\deploy\Little Navconnect"
for /f %%f in ('dir /ad /b "%APROJECTS%\deploy\Little Navconnect"') do rd /s /q "%APROJECTS%\deploy\Little Navconnect\%%f"
rd /S /Q "%APROJECTS%\deploy\Little Navconnect"
IF ERRORLEVEL 1 goto :err
popd

del /S /Q /F "%APROJECTS%\deploy\Little Xpconnect"
for /f %%f in ('dir /ad /b "%APROJECTS%\deploy\Little Xpconnect"') do rd /s /q "%APROJECTS%\deploy\Little Xpconnect\%%f"
rd /S /Q "%APROJECTS%\deploy\Little Xpconnect"
IF ERRORLEVEL 1 goto :err
popd

setlocal


rem ===========================================================================
rem ========================== atools 32 bit
set PATH=%PATH%;%PATH_SHARED%
pushd "%APROJECTS%\build-atools-%CONF_TYPE%"

del /S /Q /F "%APROJECTS%\build-atools-%CONF_TYPE%"
for /f %%f in ('dir /ad /b "%APROJECTS%\build-atools-%CONF_TYPE%"') do rd /s /q "%APROJECTS%\build-atools-%CONF_TYPE%\%%f"
IF ERRORLEVEL 1 goto :err

qmake.exe "%APROJECTS%\atools\atools.pro" -spec win32-g++ CONFIG+=%CONF_TYPE%
IF ERRORLEVEL 1 goto :err
mingw32-make.exe -j4
IF ERRORLEVEL 1 goto :err
popd

rem ===========================================================================
rem ========================== littlenavconnect 32 bit
pushd "%APROJECTS%\build-littlenavconnect-%CONF_TYPE%"
echo %PATH%
del /S /Q /F "%APROJECTS%\build-littlenavconnect-%CONF_TYPE%"
for /f %%f in ('dir /ad /b "%APROJECTS%\build-littlenavconnect-%CONF_TYPE%"') do rd /s /q "%APROJECTS%\build-littlenavconnect-%CONF_TYPE%\%%f"
IF ERRORLEVEL 1 goto :err

qmake.exe "%APROJECTS%\littlenavconnect\littlenavconnect.pro" -spec win32-g++ CONFIG+=%CONF_TYPE%
IF ERRORLEVEL 1 goto :err
mingw32-make.exe -j4
IF ERRORLEVEL 1 goto :err
mingw32-make.exe deploy
IF ERRORLEVEL 1 goto :err
popd


rem ===========================================================================
rem ========================== littlenavmap 32 bit
pushd "%APROJECTS%\build-littlenavmap-%CONF_TYPE%"

del /S /Q /F "%APROJECTS%\build-littlenavmap-%CONF_TYPE%"
for /f %%f in ('dir /ad /b "%APROJECTS%\build-littlenavmap-%CONF_TYPE%"') do rd /s /q "%APROJECTS%\build-littlenavmap-%CONF_TYPE%\%%f"
IF ERRORLEVEL 1 goto :err

qmake.exe "%APROJECTS%\littlenavmap\littlenavmap.pro" -spec win32-g++ CONFIG+=%CONF_TYPE%
IF ERRORLEVEL 1 goto :err
mingw32-make.exe -j4
IF ERRORLEVEL 1 goto :err
mingw32-make.exe deploy
IF ERRORLEVEL 1 goto :err
popd
endlocal

rem ===========================================================================
rem ========================== atools 64 bit
setlocal
pushd "%APROJECTS%\build-atools-%CONF_TYPE%"
set PATH=%PATH%;%PATH_STATIC%

del /S /Q /F "%APROJECTS%\build-atools-%CONF_TYPE%"
for /f %%f in ('dir /ad /b "%APROJECTS%\build-atools-%CONF_TYPE%"') do rd /s /q "%APROJECTS%\build-atools-%CONF_TYPE%\%%f"
IF ERRORLEVEL 1 goto :err

set ATOOLS_NO_GRIB=true
set ATOOLS_NO_FS=true

qmake.exe "%APROJECTS%\atools\atools.pro" -spec win32-g++ CONFIG+=%CONF_TYPE%
IF ERRORLEVEL 1 goto :err
mingw32-make.exe -j4
IF ERRORLEVEL 1 goto :err
popd

rem ===========================================================================
rem ========================== littlexpconnect 64 bit

pushd "%APROJECTS%\build-littlexpconnect-%CONF_TYPE%"

del /S /Q /F "%APROJECTS%\build-littlexpconnect-%CONF_TYPE%"
for /f %%f in ('dir /ad /b "%APROJECTS%\build-littlexpconnect-%CONF_TYPE%"') do rd /s /q "%APROJECTS%\build-littlexpconnect-%CONF_TYPE%\%%f"
IF ERRORLEVEL 1 goto :err

qmake.exe "%APROJECTS%\littlexpconnect\littlexpconnect.pro" -spec win32-g++ CONFIG+=%CONF_TYPE% DEFINES+=XPLM200=1 DEFINES+=APL=0 DEFINES+=IBM=0 DEFINES+=LIN=1
IF ERRORLEVEL 1 goto :err
mingw32-make.exe -j4
IF ERRORLEVEL 1 goto :err
mingw32-make.exe deploy
IF ERRORLEVEL 1 goto :err
popd
endlocal

rem ===========================================================================
rem Copy navconnect =======================================================
xcopy /i /s /e /f /y "%APROJECTS%\deploy\Little Navconnect" "%APROJECTS%\deploy\Little Navmap\Little Navconnect"
IF ERRORLEVEL 1 goto :err

rem ===========================================================================
rem Copy xpconnect =======================================================
xcopy /i /s /e /f /y "%APROJECTS%\deploy\Little Xpconnect" "%APROJECTS%\deploy\Little Navmap\Little Xpconnect"
IF ERRORLEVEL 1 goto :err

echo ---- Success ----

if not "%1" == "nopause" pause

exit /b 0

:err

echo **** ERROR ****

popd

pause

exit /b 1
