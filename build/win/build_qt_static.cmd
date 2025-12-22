@echo off

if defined APROJECTS ( echo APROJECTS=%APROJECTS% ) else ( echo APROJECTS not set && exit /b 1 )
if defined QT_VERSION ( echo QT_VERSION=%QT_VERSION% ) else ( echo QT_VERSION not set && exit /b 1 )

setlocal enableextensions

setlocal

rem Defines the used Qt for all builds
if defined QTDIR_SHARED ( echo QTDIR_SHARED=%QTDIR_SHARED% ) else ( set QTDIR_SHARED=C:\Qt\%QT_VERSION%\mingw_64)
if defined PATH_SHARED ( echo PATH_SHARED=%PATH_SHARED% ) else ( set PATH_SHARED=C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\mingw1120_64\bin\;%QTDIR_SHARED%\bin)

set QTDIR=%QTDIR_SHARED%
set PATH=%PATH%;%PATH_SHARED%;C:\Qt\Tools\Ninja

echo Cleaning up ... ========================================
cd %APROJECTS%
rmdir /s/q %APROJECTS%\qt-%QT_VERSION%-static
mkdir %APROJECTS%\qt-%QT_VERSION%-static

rmdir /s/q %APROJECTS%\build-qt-%QT_VERSION%-static
mkdir %APROJECTS%\build-qt-%QT_VERSION%-static

echo Configure ... ========================================
cd %APROJECTS%\build-qt-%QT_VERSION%-static

call %APROJECTS%\qt-everywhere-src-%QT_VERSION%\configure.bat -submodules qtbase -static -release -prefix %APROJECTS%/qt-%QT_VERSION%-static -opensource -no-gui -nomake examples -nomake tests -no-dbus -confirm-license -qt-pcre -qt-zlib -qt-libpng -qt-libjpeg -qt-sqlite -qt-freetype -qt-harfbuzz
if errorlevel 1 goto :err

echo Building ... ========================================
cd %APROJECTS%/build-qt-%QT_VERSION%-static
cmake --build . -j4
if errorlevel 1 goto :err

echo Installing ... ========================================
cd %APROJECTS%/build-qt-%QT_VERSION%-static
cmake --install .
if errorlevel 1 goto :err

echo ---- Success ----

if not "%1" == "nopause" pause

exit /b 0

:err

echo **** ERROR ****

popd

pause

exit /b 1
