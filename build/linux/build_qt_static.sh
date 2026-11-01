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
bash $APROJECTS/qtbase-everywhere-src-$QT_VERSION_STATIC/configure -openssl-linked -static -release -prefix $APROJECTS/qt-$QT_VERSION_STATIC-static \
    -opensource -no-widgets -no-glib -no-inotify -no-gui -no-icu -no-pch -no-kms -no-directfb -no-eglfs -no-xcb -no-cups -no-gif -no-ico -no-evdev -no-accessibility \
    -no-opengl -nomake examples -nomake tests -no-dbus -confirm-license -qt-pcre -qt-zlib -qt-sqlite -no-cups -no-opengl -no-egl \
    -no-libudev -no-evdev -no-libinput -no-mtdev -no-tslib -no-bundled-xcb-xinput -no-xkbcommon -no-gif -no-ico -no-libpng -no-libjpeg -no-directfb

echo Building ... ========================================
cd $APROJECTS/build-qt-$QT_VERSION_STATIC-static
nice cmake --build .

#echo Installing ... ========================================
cd $APROJECTS/build-qt-$QT_VERSION_STATIC-static
cmake --install .

echo Done. ========================================
