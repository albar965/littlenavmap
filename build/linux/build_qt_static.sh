#!/bin/bash

# Echo all commands and exit on failure
set -euxo pipefail

# Error checking for required variable APROJECTS
if [ -z "$APROJECTS" ] ; then echo APROJECTS environment variable not set ; exit 1 ; fi
if [ ! -d "$APROJECTS" ]; then echo "$APROJECTS" does not exist ; exit 1 ; fi
if [ -z "$QT_VERSION_STATIC" ] ; then echo QT_VERSION_STATIC environment variable not set ; exit 1 ; fi

echo Cleaning up ... ========================================
cd $APROJECTS
rm -rf $APROJECTS/qt-$QT_VERSION_STATIC-static
mkdir -p $APROJECTS/qt-$QT_VERSION_STATIC-static

rm -rf $APROJECTS/build-qt-$QT_VERSION_STATIC-static
mkdir -p $APROJECTS/build-qt-$QT_VERSION_STATIC-static

echo Configure ... ========================================
cd $APROJECTS/build-qt-$QT_VERSION_STATIC-static
bash $APROJECTS/qtbase-everywhere-src-$QT_VERSION_STATIC/configure -openssl-linked -static -release -prefix $APROJECTS/qt-$QT_VERSION_STATIC-static -opensource -no-gui -nomake examples -nomake tests -no-dbus -confirm-license -qt-pcre -qt-zlib -qt-sqlite

echo Building ... ========================================
cd $APROJECTS/build-qt-$QT_VERSION_STATIC-static
nice cmake --build .

#echo Installing ... ========================================
cd $APROJECTS/build-qt-$QT_VERSION_STATIC-static
cmake --install .

echo Done. ========================================
