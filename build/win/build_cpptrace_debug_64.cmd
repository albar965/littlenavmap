@echo off

setlocal enableextensions

if defined APROJECTS ( echo %APROJECTS% ) else ( echo APROJECTS not set && exit /b 1 )

set BUILDDIR=%APROJECTS%\build-cpptrace-debug
set DEPLOYDIR=%APROJECTS%\cpptrace-debug-win64
set BUILDTYPE=Debug

set QTINSTALL=C:\Qt
set QTDIR=%QTINSTALL%\%QT_VERSION%\mingw_64
set PATH=%PATH%;%QTINSTALL%\Tools\CMake_64\bin;%QTINSTALL%\Tools\mingw1510_64\bin\;%QTDIR%\bin\;%QTINSTALL%\Tools\Ninja\

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
