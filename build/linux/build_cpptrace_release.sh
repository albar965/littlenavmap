#!/bin/bash

# Echo all commands and exit on failure
set -e
set -x

# Error checking for required variable APROJECTS
if [ -z "$APROJECTS" ] ; then echo APROJECTS environment variable not set ; exit 1 ; fi
if [ ! -d "$APROJECTS" ]; then echo "$APROJECTS" does not exist ; exit 1 ; fi


rm -rf $APROJECTS/cpptrace-release $APROJECTS/build-cpptrace-release

mkdir -p $APROJECTS/cpptrace-release
mkdir -p $APROJECTS/build-cpptrace-release


cd $APROJECTS//build-cpptrace-release
cmake -S $APROJECTS/cpptrace -B $APROJECTS/build-cpptrace-release -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$APROJECTS/cpptrace-release
cmake --build $APROJECTS/build-cpptrace-release --target clean
cmake --build $APROJECTS/build-cpptrace-release --target all
cmake --install $APROJECTS/build-cpptrace-release
