@echo off

rem === Deploy built programs. ZIP, check with Windows Defender and copy them to network shares =============================


rem only for German locale
set year=%date:~-4,4%
set month=%date:~-7,2%
set day=%date:~0,2%
set hour=%time:~0,2%
set min=%time:~3,2%
set FILEDATE=%year%%month%%day%-%hour%%min%

pushd "%APROJECTS%\deploy"

rem ==== Pack Little Navconnect ===================================================================
del LittleNavconnect.zip

"C:\Program Files\7-Zip\7z.exe" a LittleNavconnect.zip "Little Navconnect"
IF ERRORLEVEL 1 goto :err

"C:\Program Files\Windows Defender\MpCmdRun.exe" -Scan -ScanType 3 -DisableRemediation -File "%APROJECTS%\deploy\LittleNavconnect.zip"
IF ERRORLEVEL 1 goto :err

del \\darkon\public\LittleNavconnect-%FILEDATE%.zip
copy /Y /Z /B LittleNavconnect.zip \\darkon\public\LittleNavconnect-%FILEDATE%.zip
IF ERRORLEVEL 1 goto :err

del \\frida\public\LittleNavconnect-%FILEDATE%.zip
copy /Y /Z /B LittleNavconnect.zip \\frida\public\LittleNavconnect-%FILEDATE%.zip
IF ERRORLEVEL 1 goto :err


rem ==== Pack Little Navmap ===================================================================
del LittleNavmap.zip

"C:\Program Files\7-Zip\7z.exe" a LittleNavmap.zip "Little Navmap"
IF ERRORLEVEL 1 goto :err

"C:\Program Files\Windows Defender\MpCmdRun.exe" -Scan -ScanType 3 -DisableRemediation -File "%APROJECTS%\deploy\LittleNavmap.zip"
IF ERRORLEVEL 1 goto :err

del \\darkon\public\LittleNavmap-%FILEDATE%.zip
copy /Y /Z /B LittleNavmap.zip \\darkon\public\LittleNavmap-%FILEDATE%.zip
IF ERRORLEVEL 1 goto :err

del \\frida\public\LittleNavmap-%FILEDATE%.zip
copy /Y /Z /B LittleNavmap.zip \\frida\public\LittleNavmap-%FILEDATE%.zip
IF ERRORLEVEL 1 goto :err

popd

echo ---- Success ----

if not "%1" == "nopause" pause

exit /b 0

:err

echo **** ERROR ****

popd

pause

exit /b 1
