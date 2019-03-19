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
rem set QMAKE_STATIC=C:\Projekte\build-qt-5.12.0-release\bin\qmake
rem set MARBLE_LIB_PATH=C:\Users\YOURNAME\Programme\Marble-debug\lib
rem set MARBLE_INC_PATH=C:\Users\YOURNAME\Programme\Marble-debug\include

if defined CONF_TYPE ( echo %CONF_TYPE% ) else ( set CONF_TYPE=release )
if defined ATOOLS_INC_PATH ( echo %ATOOLS_INC_PATH% ) else ( set ATOOLS_INC_PATH=../atools/src )
if defined ATOOLS_LIB_PATH ( echo %ATOOLS_LIB_PATH% ) else ( set ATOOLS_LIB_PATH=../build-atools-${CONF_TYPE} )
if defined MARBLE_INC_PATH ( echo %MARBLE_INC_PATH% ) else ( set MARBLE_INC_PATH=%APROJECTS%/Marble-${CONF_TYPE}/include )
if defined MARBLE_LIB_PATH ( echo %MARBLE_LIB_PATH% ) else ( set MARBLE_LIB_PATH=%APROJECTS%/Marble-${CONF_TYPE}/lib )
if defined DEPLOY_BASE ( echo %DEPLOY_BASE% ) else ( set DEPLOY_BASE=../deploy )
if defined DATABASE_BASE ( echo %DATABASE_BASE% ) else ( set DATABASE_BASE=%APROJECTS%/little_navmap_db )
if defined HELP_BASE ( echo %HELP_BASE% ) else ( set HELP_BASE=%APROJECTS%/little_navmap_help )

rem Defines the used Qt for all builds
if defined QMAKE_SHARED ( echo %QMAKE_SHARED% ) else ( set QMAKE_SHARED=C:\Qt\5.9.5\mingw53_32\bin\qmake )
if defined MAKE_SHARED ( echo %MAKE_SHARED% ) else ( set MAKE_SHARED=C:\Qt\Tools\mingw530_32\bin\mingw32-make.exe )

rem Defines the used Qt for Xpconnect
if defined QMAKE_STATIC ( echo %QMAKE_STATIC% ) else ( set QMAKE_STATIC=C:\msys64\mingw64\bin\qmake.exe )
if defined MAKE_STATIC ( echo %MAKE_STATIC% ) else ( set MAKE_STATIC=C:\msys64\mingw64\bin\mingw32-make.exe )

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
set PATH=%PATH%;C:\Qt\5.9.5\mingw53_32\bin;C:\Qt\Tools\mingw530_32\bin

rem ===========================================================================
rem ========================== atools 32 bit
pushd "%APROJECTS%\build-atools-release"
del /S /Q /F "%APROJECTS%\build-atools-release"
for /f %%f in ('dir /ad /b "%APROJECTS%\build-atools-release"') do rd /s /q "%APROJECTS%\build-atools-release\%%f"
IF ERRORLEVEL 1 goto :err

%QMAKE_SHARED% "%APROJECTS%\atools\atools.pro" -spec win32-g++ CONFIG+=release
IF ERRORLEVEL 1 goto :err
%MAKE_SHARED% -j2
IF ERRORLEVEL 1 goto :err
popd

rem ===========================================================================
rem ========================== littlenavconnect 32 bit
pushd "%APROJECTS%\build-littlenavconnect-release"
del /S /Q /F "%APROJECTS%\build-littlenavconnect-release"
for /f %%f in ('dir /ad /b "%APROJECTS%\build-littlenavconnect-release"') do rd /s /q "%APROJECTS%\build-littlenavconnect-release\%%f"
IF ERRORLEVEL 1 goto :err

%QMAKE_SHARED% "%APROJECTS%\littlenavconnect\littlenavconnect.pro" -spec win32-g++ CONFIG+=release
IF ERRORLEVEL 1 goto :err
%MAKE_SHARED% -j2
IF ERRORLEVEL 1 goto :err
%MAKE_SHARED% deploy
IF ERRORLEVEL 1 goto :err
popd
endlocal

rem ===========================================================================
rem ========================== littlenavmap 32 bit
setlocal
set PATH=%PATH%;C:\Qt\5.9.5\mingw53_32\bin;C:\Qt\Tools\mingw530_32\bin
pushd "%APROJECTS%\build-littlenavmap-release"
del /S /Q /F "%APROJECTS%\build-littlenavmap-release"
for /f %%f in ('dir /ad /b "%APROJECTS%\build-littlenavmap-release"') do rd /s /q "%APROJECTS%\build-littlenavmap-release\%%f"
IF ERRORLEVEL 1 goto :err

%QMAKE_SHARED% "%APROJECTS%\littlenavmap\littlenavmap.pro" -spec win32-g++ CONFIG+=release
IF ERRORLEVEL 1 goto :err
%MAKE_SHARED% -j2
IF ERRORLEVEL 1 goto :err
%MAKE_SHARED% deploy
IF ERRORLEVEL 1 goto :err
popd
endlocal

rem ===========================================================================
rem ========================== atools 64 bit
setlocal
set PATH=%PATH%;C:\msys64\mingw64\bin
pushd "%APROJECTS%\build-atools-release"
del /S /Q /F "%APROJECTS%\build-atools-release"
for /f %%f in ('dir /ad /b "%APROJECTS%\build-atools-release"') do rd /s /q "%APROJECTS%\build-atools-release\%%f"
IF ERRORLEVEL 1 goto :err

%QMAKE_STATIC% "%APROJECTS%\atools\atools.pro" -spec win32-g++ CONFIG+=release
IF ERRORLEVEL 1 goto :err
%MAKE_STATIC% -j2
IF ERRORLEVEL 1 goto :err
popd

rem ===========================================================================
rem ========================== littlexpconnect 64 bit
pushd "%APROJECTS%\build-littlexpconnect-release"
del /S /Q /F "%APROJECTS%\build-littlexpconnect-release"
for /f %%f in ('dir /ad /b "%APROJECTS%\build-littlexpconnect-release"') do rd /s /q "%APROJECTS%\build-littlexpconnect-release\%%f"
IF ERRORLEVEL 1 goto :err

%QMAKE_STATIC% "%APROJECTS%\littlexpconnect\littlexpconnect.pro" -spec win32-g++ CONFIG+=release DEFINES+=XPLM200=1 DEFINES+=APL=0 DEFINES+=IBM=0 DEFINES+=LIN=1
IF ERRORLEVEL 1 goto :err
%MAKE_STATIC% -j2
IF ERRORLEVEL 1 goto :err
%MAKE_STATIC% deploy
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
