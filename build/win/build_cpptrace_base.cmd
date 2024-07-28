@echo off

setlocal enableextensions

if defined APROJECTS ( echo %APROJECTS% ) else ( echo APROJECTS not set && exit /b 1 )
if defined DEPLOYDIR ( echo %DEPLOYDIR% ) else ( echo DEPLOYDIR not set && exit /b 1 )
if defined BUILDDIR ( echo %BUILDDIR% ) else ( echo BUILDDIR not set && exit /b 1 )
if defined BUILDTYPE ( echo %BUILDTYPE% ) else ( echo BUILDTYPE not set && exit /b 1 )

rmdir /s/q "%DEPLOYDIR%"
mkdir "%DEPLOYDIR%"

rmdir /s/q "%BUILDDIR%"
mkdir "%BUILDDIR%"

pushd "%BUILDDIR%"
IF ERRORLEVEL 1 goto :err

rem  -DCPPTRACE_UNWIND_WITH_WINAPI=On -DCPPTRACE_GET_SYMBOLS_WITH_LIBDWARF=On -DCPPTRACE_DEMANGLE_WITH_CXXABI=On
C:\Qt\Tools\CMake_64\bin\cmake -Wno-dev -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=%BUILDTYPE% -DCMAKE_INSTALL_PREFIX=%DEPLOYDIR% ..\cpptrace
IF ERRORLEVEL 1 goto :err

C:\Qt\Tools\CMake_64\bin\cmake --build %BUILDDIR% --target clean
IF ERRORLEVEL 1 goto :err

C:\Qt\Tools\CMake_64\bin\cmake --build %BUILDDIR% --target all
IF ERRORLEVEL 1 goto :err

C:\Qt\Tools\CMake_64\bin\cmake --install %BUILDDIR%
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
