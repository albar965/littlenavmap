#!/bin/bash

# Echo all commands and exit on failure
set -euxo pipefail

# Error checking for required variable APROJECTS
if [ -z "$APROJECTS" ] ; then echo APROJECTS environment variable not set ; exit 1 ; fi
if [ ! -d "$APROJECTS" ]; then echo "$APROJECTS" does not exist ; exit 1 ; fi
if [ -z "$QT_VERSION_DYNAMIC" ] ; then echo QT_VERSION_DYNAMIC environment variable not set ; exit 1 ; fi

echo Cleaning up ... ========================================
cd $APROJECTS
rm -rf $APROJECTS/qt-$QT_VERSION_DYNAMIC
mkdir -p $APROJECTS/qt-$QT_VERSION_DYNAMIC

rm -rf $APROJECTS/build-qt-$QT_VERSION_DYNAMIC
mkdir -p $APROJECTS/build-qt-$QT_VERSION_DYNAMIC

echo Configure ... ========================================
cd $APROJECTS/build-qt-$QT_VERSION_DYNAMIC
bash $APROJECTS/qt-everywhere-src-$QT_VERSION_DYNAMIC/configure -openssl-linked -release -ltcg -reduce-exports \
    -prefix $APROJECTS/qt-$QT_VERSION_DYNAMIC \
    -opensource -confirm-license -nomake examples -nomake tests -qt-pcre -qt-zlib -qt-sqlite \
    -skip qt3d \
    -skip qtactiveqt \
    -skip qtcharts \
    -skip qtcoap \
    -skip qtconnectivity \
    -skip qtdatavis3d \
    -skip qtdoc \
    -skip qtgraphs \
    -skip qtgrpc \
    -skip qthttpserver \
    -skip qtlocation \
    -skip qtlottie \
    -skip qtmqtt \
    -skip qtmultimedia \
    -skip qtnetworkauth \
    -skip qtopcua \
    -skip qtpositioning \
    -skip qtquick3d \
    -skip qtquick3dphysics \
    -skip qtquickeffectmaker \
    -skip qtquicktimeline \
    -skip qtremoteobjects \
    -skip qtscxml \
    -skip qtsensors \
    -skip qtserialbus \
    -skip qtserialport \
    -skip qtspeech \
    -skip qttools \
    -skip qttranslations \
    -skip qtvirtualkeyboard \
    -skip qtwebchannel \
    -skip qtwebengine \
    -skip qtwebsockets \
    -skip qtwebview

#    -skip qtwayland \
# -skip qtshadertools \
# -skip qtlanguageserver \
# -skip qtimageformats \
# -skip qtsvg \
# -skip qtdeclarative \

echo "uilding ... ======================================="
cd $APROJECTS/build-qt-$QT_VERSION_DYNAMIC
nice cmake --build .

#echo Installing ... ========================================
cd $APROJECTS/build-qt-$QT_VERSION_DYNAMIC
cmake --install .

echo Done. ========================================
