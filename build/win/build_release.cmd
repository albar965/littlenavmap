@echo off

rem ===========================================================================
rem First delete all deploy directories =======================================
cd "%APROJECTS%\deploy\Little Navmap"
del /S /Q /F "%APROJECTS%\deploy\Little Navmap"
for /f %%f in ('dir /ad /b "%APROJECTS%\deploy\Little Navmap"') do rd /s /q "%APROJECTS%\deploy\Little Navmap\%%f"
rd /S /Q "%APROJECTS%\deploy\Little Navmap"
IF ERRORLEVEL 1 goto :err

cd "%APROJECTS%\deploy\Little Navconnect"
del /S /Q /F "%APROJECTS%\deploy\Little Navconnect"
for /f %%f in ('dir /ad /b "%APROJECTS%\deploy\Little Navconnect"') do rd /s /q "%APROJECTS%\deploy\Little Navconnect\%%f"
rd /S /Q "%APROJECTS%\deploy\Little Navconnect"
IF ERRORLEVEL 1 goto :err

cd "%APROJECTS%\deploy\navdatareader"
del /S /Q /F "%APROJECTS%\deploy\navdatareader"
for /f %%f in ('dir /ad /b "%APROJECTS%\deploy\navdatareader"') do rd /s /q "%APROJECTS%\deploy\navdatareader\%%f"
rd /S /Q "%APROJECTS%\deploy\navdatareader"
IF ERRORLEVEL 1 goto :err

rem ===========================================================================
rem ========================== atools
cd "%APROJECTS%\build-atools-release"
del /S /Q /F "%APROJECTS%\build-atools-release"
for /f %%f in ('dir /ad /b "%APROJECTS%\build-atools-release"') do rd /s /q "%APROJECTS%\build-atools-release\%%f"
IF ERRORLEVEL 1 goto :err

%APROJECTS%\Qt\bin\qmake.exe "%APROJECTS%\atools\atools.pro" -spec win32-g++ CONFIG+=release
IF ERRORLEVEL 1 goto :err
C:\Qt\Tools\mingw492_32\bin\mingw32-make.exe -j2
IF ERRORLEVEL 1 goto :err

rem ===========================================================================
rem ========================== navdatareader
cd "%APROJECTS%\build-navdatareader-release"
del /S /Q /F "%APROJECTS%\build-navdatareader-release"
for /f %%f in ('dir /ad /b "%APROJECTS%\build-navdatareader-release"') do rd /s /q "%APROJECTS%\build-navdatareader-release\%%f"
IF ERRORLEVEL 1 goto :err

"%APROJECTS%\Qt\bin\qmake.exe" "%APROJECTS%\navdatareader\navdatareader.pro" -spec win32-g++ CONFIG+=release
IF ERRORLEVEL 1 goto :err
C:\Qt\Tools\mingw492_32\bin\mingw32-make.exe -j2
IF ERRORLEVEL 1 goto :err
C:\Qt\Tools\mingw492_32\bin\mingw32-make.exe deploy
IF ERRORLEVEL 1 goto :err

rem ===========================================================================
rem ========================== littlenavconnect
cd "%APROJECTS%\build-littlenavconnect-release"
del /S /Q /F "%APROJECTS%\build-littlenavconnect-release"
for /f %%f in ('dir /ad /b "%APROJECTS%\build-littlenavconnect-release"') do rd /s /q "%APROJECTS%\build-littlenavconnect-release\%%f"
IF ERRORLEVEL 1 goto :err

"%APROJECTS%\Qt\bin\qmake.exe" "%APROJECTS%\littlenavconnect\littlenavconnect.pro" -spec win32-g++ CONFIG+=release
IF ERRORLEVEL 1 goto :err
C:\Qt\Tools\mingw492_32\bin\mingw32-make.exe -j2
IF ERRORLEVEL 1 goto :err
C:\Qt\Tools\mingw492_32\bin\mingw32-make.exe deploy
IF ERRORLEVEL 1 goto :err

rem ===========================================================================
rem ========================== littlenavmap
cd "%APROJECTS%\build-littlenavmap-release"
del /S /Q /F "%APROJECTS%\build-littlenavmap-release"
for /f %%f in ('dir /ad /b "%APROJECTS%\build-littlenavmap-release"') do rd /s /q "%APROJECTS%\build-littlenavmap-release\%%f"
IF ERRORLEVEL 1 goto :err

"%APROJECTS%\Qt\bin\qmake.exe" "%APROJECTS%\littlenavmap\littlenavmap.pro" -spec win32-g++ CONFIG+=release
IF ERRORLEVEL 1 goto :err
C:\Qt\Tools\mingw492_32\bin\mingw32-make.exe -j2
IF ERRORLEVEL 1 goto :err
C:\Qt\Tools\mingw492_32\bin\mingw32-make.exe deploy
IF ERRORLEVEL 1 goto :err

rem ===========================================================================
rem ========================== atools no SimConnect
cd "%APROJECTS%\build-atools-release"
del /S /Q /F "%APROJECTS%\build-atools-release"
for /f %%f in ('dir /ad /b "%APROJECTS%\build-atools-release"') do rd /s /q "%APROJECTS%\build-atools-release\%%f"
IF ERRORLEVEL 1 goto :err

"%APROJECTS%\Qt\bin\qmake.exe" "%APROJECTS%\atools\atools.pro" -spec win32-g++ CONFIG+=release DEFINES-=SIMCONNECT_REAL DEFINES+=SIMCONNECT_DUMMY
IF ERRORLEVEL 1 goto :err
C:\Qt\Tools\mingw492_32\bin\mingw32-make.exe -j2
IF ERRORLEVEL 1 goto :err

rem ===========================================================================
rem ========================== littlenavmap no SimConnect
cd "%APROJECTS%\build-littlenavmap-release"
del /S /Q /F "%APROJECTS%\build-littlenavmap-release"
for /f %%f in ('dir /ad /b "%APROJECTS%\build-littlenavmap-release"') do rd /s /q "%APROJECTS%\build-littlenavmap-release\%%f"
IF ERRORLEVEL 1 goto :err

"%APROJECTS%\Qt\bin\qmake.exe" "%APROJECTS%\littlenavmap\littlenavmap.pro" -spec win32-g++ CONFIG+=release DEFINES-=SIMCONNECT_REAL DEFINES+=SIMCONNECT_DUMMY
IF ERRORLEVEL 1 goto :err
C:\Qt\Tools\mingw492_32\bin\mingw32-make.exe -j2
IF ERRORLEVEL 1 goto :err
C:\Qt\Tools\mingw492_32\bin\mingw32-make.exe
IF ERRORLEVEL 1 goto :err

rem Copy nosimconnect =======================================================
rename "%APROJECTS%\build-littlenavmap-release\release\littlenavmap.exe" littlenavmap-nosimconnect.exe
IF ERRORLEVEL 1 goto :err
xcopy "%APROJECTS%\build-littlenavmap-release\release\littlenavmap-nosimconnect.exe" "%APROJECTS%\deploy\Little Navmap\"
IF ERRORLEVEL 1 goto :err

rem Copy navconnect =======================================================
xcopy "%APROJECTS%\deploy\Little Navconnect\littlenavconnect.exe" "%APROJECTS%\deploy\Little Navmap\"
IF ERRORLEVEL 1 goto :err
xcopy "%APROJECTS%\deploy\Little Navconnect\littlenavconnect.exe.manifest" "%APROJECTS%\deploy\Little Navmap\"
IF ERRORLEVEL 1 goto :err
xcopy /i /s /e /f /y "%APROJECTS%\deploy\Little Navconnect\help" "%APROJECTS%\deploy\Little Navmap\help"
IF ERRORLEVEL 1 goto :err
xcopy "%APROJECTS%\deploy\Little Navconnect\README.txt" "%APROJECTS%\deploy\Little Navmap\README-Navconnect.txt*"
IF ERRORLEVEL 1 goto :err
xcopy "%APROJECTS%\deploy\Little Navconnect\CHANGELOG.txt" "%APROJECTS%\deploy\Little Navmap\CHANGELOG-Navconnect.txt*"
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