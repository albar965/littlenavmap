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

rem ===========================================================================
rem ========================== atools
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
rem ========================== littlenavconnect
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

rem ===========================================================================
rem ========================== littlenavmap
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

rem Copy navconnect =======================================================
xcopy "%APROJECTS%\deploy\Little Navconnect\littlenavconnect.exe" "%APROJECTS%\deploy\Little Navmap\"
IF ERRORLEVEL 1 goto :err
xcopy "%APROJECTS%\deploy\Little Navconnect\littlenavconnect.exe.simconnect" "%APROJECTS%\deploy\Little Navmap\"
IF ERRORLEVEL 1 goto :err
xcopy /i /s /e /f /y "%APROJECTS%\deploy\Little Navconnect\help" "%APROJECTS%\deploy\Little Navmap\help"
IF ERRORLEVEL 1 goto :err
xcopy "%APROJECTS%\deploy\Little Navconnect\README.txt" "%APROJECTS%\deploy\Little Navmap\README-Navconnect.txt*"
IF ERRORLEVEL 1 goto :err
xcopy "%APROJECTS%\deploy\Little Navconnect\CHANGELOG.txt" "%APROJECTS%\deploy\Little Navmap\CHANGELOG-Navconnect.txt*"
IF ERRORLEVEL 1 goto :err

echo ---- Success ----

if not "%1" == "nopause" pause

exit /b 0

:err

echo **** ERROR ****

popd

pause

exit /b 1
