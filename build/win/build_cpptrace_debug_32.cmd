@echo off

setlocal enableextensions

if defined APROJECTS ( echo %APROJECTS% ) else ( echo APROJECTS not set && exit /b 1 )

set BUILDDIR=%APROJECTS%\build-cpptrace-debug
set DEPLOYDIR=%APROJECTS%\cpptrace-debug-win32
set PATH=%PATH%;C:\Qt\5.15.2\mingw81_32\bin;C:\Qt\Tools\mingw810_32\bin
set BUILDTYPE=Debug

call build_cpptrace_base.cmd nopause
IF ERRORLEVEL 1 goto :err

echo ---- Success ----

if not "%1" == "nopause" pause

exit /b 0

:err

echo **** ERROR ****

popd

pause

exit /b 1
