#!/bin/bash

# Echo all commands and exit on failure
set -e
set -x

# Error checking for required variable APROJECTS
if [ -z "$APROJECTS" ] ; then echo APROJECTS environment variable not set ; exit 1 ; fi
if [ ! -d "$APROJECTS" ]; then echo "$APROJECTS" does not exist ; exit 1 ; fi



rm -rf $APROJECTS/build-qt-5.15.2-release
mkdir -p $APROJECTS/build-qt-5.15.2-release

cd  $APROJECTS/qt-everywhere-src-5.15.2

set +e
make -k distclean
set -e

cd  $APROJECTS/qt-everywhere-src-5.15.2

./configure -openssl-linked -static -prefix ../build-qt-5.15.2-release -opensource -nomake examples -nomake tests -confirm-license -skip qtwayland -no-gui -no-widgets -no-dbus -qt-zlib -no-libpng -no-libjpeg -no-sqlite -no-freetype -no-harfbuzz -no-icu -no-iconv -nomake tools -no-evdev -no-accessibility -no-opengl -skip multimedia -no-qpa -skip tools -no-pch -no-kms -no-directfb -no-eglfs -no-xcb -no-cups -no-gif -no-tiff -no-ico -no-webp -skip declarative -skip location -skip serialbus

cd  $APROJECTS/qt-everywhere-src-5.15.2

make -j4 module-qtbase

cd  $APROJECTS/qt-everywhere-src-5.15.2/qtbase

make install

set +e
make -k distclean
set -e
