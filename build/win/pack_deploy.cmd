@echo off

rem only for German locale
set year=%date:~-4,4%
set month=%date:~-7,2%
set day=%date:~0,2%
set hour=%time:~0,2%
set min=%time:~3,2%
set FILEDATE=%year%%month%%day%-%hour%%min%

cd "%APROJECTS%\deploy"
 
del navdatareader.zip 
rem del LittleNavconnect.zip 
del LittleNavmap.zip

7z.exe a navdatareader.zip "navdatareader"
IF ERRORLEVEL 1 goto :err

7z.exe a LittleNavmap.zip "Little Navmap"
IF ERRORLEVEL 1 goto :err

"C:\Program Files\Windows Defender\MpCmdRun.exe" -Scan -ScanType 3 -DisableRemediation -File "%APROJECTS%\deploy\navdatareader.zip"
IF ERRORLEVEL 1 goto :err
 
"C:\Program Files\Windows Defender\MpCmdRun.exe" -Scan -ScanType 3 -DisableRemediation -File "%APROJECTS%\deploy\LittleNavmap.zip"
IF ERRORLEVEL 1 goto :err

del \\darkon\public\navdatareader-%FILEDATE%.zip
copy /Y /Z /B navdatareader.zip \\darkon\public\navdatareader-%FILEDATE%.zip
IF ERRORLEVEL 1 goto :err

del \\darkon\public\LittleNavmap-%FILEDATE%.zip
copy /Y /Z /B LittleNavmap.zip \\darkon\public\LittleNavmap-%FILEDATE%.zip
IF ERRORLEVEL 1 goto :err

del \\frida\public\navdatareader-%FILEDATE%.zip
copy /Y /Z /B navdatareader.zip \\frida\public\navdatareader-%FILEDATE%.zip
IF ERRORLEVEL 1 goto :err

del \\frida\public\LittleNavmap-%FILEDATE%.zip
copy /Y /Z /B LittleNavmap.zip \\frida\public\LittleNavmap-%FILEDATE%.zip
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