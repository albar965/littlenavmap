#!/bin/bash -x

QT_VERSION=6.5.3
QT_BUILD=release

# -------------------

# Error checking for required variable APROJECTS
if [ -z "$APROJECTS" ] ; then echo APROJECTS environment variable not set ; exit 1 ; fi
if [ ! -d "$APROJECTS" ]; then echo "$APROJECTS" does not exist ; exit 1 ; fi

echo Cleaning up ... ========================================
cd $APROJECTS
rm -rf $APROJECTS/qt-$QT_VERSION-$QT_BUILD
mkdir -p $APROJECTS/qt-$QT_VERSION-$QT_BUILD

rm -rf $APROJECTS/build-qt-$QT_VERSION-$QT_BUILD
mkdir -p $APROJECTS/build-qt-$QT_VERSION-$QT_BUILD

echo Configure ... ========================================
cd $APROJECTS/build-qt-$QT_VERSION-$QT_BUILD
$APROJECTS/qt-everywhere-src-$QT_VERSION/configure -submodules qtbase -openssl-linked -static -debug -prefix $APROJECTS/qt-$QT_VERSION-$QT_BUILD -opensource -no-gui -nomake examples -nomake tests -no-dbus -confirm-license -qt-pcre -qt-zlib -qt-libpng -qt-libjpeg -qt-sqlite -qt-freetype -qt-harfbuzz

echo Building ... ========================================
cd $APROJECTS/build-qt-$QT_VERSION-$QT_BUILD
nice cmake --build . -j8

#echo Installing ... ========================================
cd $APROJECTS/build-qt-$QT_VERSION-$QT_BUILD
cmake --install .

echo Done. ========================================
