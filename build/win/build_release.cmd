@echo off

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

qmake.exe "%APROJECTS%\atools\atools.pro" -spec win32-g++ CONFIG+=release
IF ERRORLEVEL 1 goto :err
mingw32-make.exe -j2
IF ERRORLEVEL 1 goto :err
popd

rem ===========================================================================
rem ========================== littlenavconnect 32 bit
pushd "%APROJECTS%\build-littlenavconnect-release"
del /S /Q /F "%APROJECTS%\build-littlenavconnect-release"
for /f %%f in ('dir /ad /b "%APROJECTS%\build-littlenavconnect-release"') do rd /s /q "%APROJECTS%\build-littlenavconnect-release\%%f"
IF ERRORLEVEL 1 goto :err

qmake.exe "%APROJECTS%\littlenavconnect\littlenavconnect.pro" -spec win32-g++ CONFIG+=release
IF ERRORLEVEL 1 goto :err
mingw32-make.exe -j2
IF ERRORLEVEL 1 goto :err
mingw32-make.exe deploy
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

qmake.exe "%APROJECTS%\littlenavmap\littlenavmap.pro" -spec win32-g++ CONFIG+=release
IF ERRORLEVEL 1 goto :err
mingw32-make.exe -j2
IF ERRORLEVEL 1 goto :err
mingw32-make.exe deploy
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

C:\msys64\mingw64\qt5-static\bin\qmake.exe "%APROJECTS%\atools\atools.pro" -spec win32-g++ CONFIG+=release
IF ERRORLEVEL 1 goto :err
mingw32-make.exe -j2
IF ERRORLEVEL 1 goto :err
popd

rem ===========================================================================
rem ========================== littlexpconnect 64 bit
pushd "%APROJECTS%\build-littlexpconnect-release"
del /S /Q /F "%APROJECTS%\build-littlexpconnect-release"
for /f %%f in ('dir /ad /b "%APROJECTS%\build-littlexpconnect-release"') do rd /s /q "%APROJECTS%\build-littlexpconnect-release\%%f"
IF ERRORLEVEL 1 goto :err

C:\msys64\mingw64\qt5-static\bin\qmake.exe "%APROJECTS%\littlexpconnect\littlexpconnect.pro" -spec win32-g++ CONFIG+=release DEFINES+=XPLM200=1 DEFINES+=APL=0 DEFINES+=IBM=0 DEFINES+=LIN=1
IF ERRORLEVEL 1 goto :err
mingw32-make.exe -j2
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
