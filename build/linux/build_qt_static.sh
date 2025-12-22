#!/bin/bash

# Echo all commands and exit on failure
set -e
set -x

# Error checking for required variable APROJECTS
if [ -z "$APROJECTS" ] ; then echo APROJECTS environment variable not set ; exit 1 ; fi
if [ ! -d "$APROJECTS" ]; then echo "$APROJECTS" does not exist ; exit 1 ; fi
if [ -z "$QT_VERSION" ] ; then echo QT_VERSION environment variable not set ; exit 1 ; fi

echo Cleaning up ... ========================================
cd $APROJECTS
rm -rf $APROJECTS/qt-$QT_VERSION-static
mkdir -p $APROJECTS/qt-$QT_VERSION-static

rm -rf $APROJECTS/build-qt-$QT_VERSION-static
mkdir -p $APROJECTS/build-qt-$QT_VERSION-static

echo Configure ... ========================================
cd $APROJECTS/build-qt-$QT_VERSION-static
$APROJECTS/qt-everywhere-src-$QT_VERSION/configure -submodules qtbase -openssl-linked -static -release -prefix $APROJECTS/qt-$QT_VERSION-static -opensource -no-gui -nomake examples -nomake tests -no-dbus -confirm-license -qt-pcre -qt-zlib -qt-libpng -qt-libjpeg -qt-sqlite -qt-freetype -qt-harfbuzz

echo Building ... ========================================
cd $APROJECTS/build-qt-$QT_VERSION-static
nice cmake --build . -j8

#echo Installing ... ========================================
cd $APROJECTS/build-qt-$QT_VERSION-static
cmake --install .

echo Done. ========================================
