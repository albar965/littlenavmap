#!/bin/bash

# Echo all commands and exit on failure
set -e
set -x

# Error checking for required variable APROJECTS
if [ -z "$APROJECTS" ] ; then echo APROJECTS environment variable not set ; exit 1 ; fi
if [ ! -d "$APROJECTS" ]; then echo "$APROJECTS" does not exist ; exit 1 ; fi


rm -rf $APROJECTS/cpptrace-debug $APROJECTS/build-cpptrace-debug

mkdir -p $APROJECTS/cpptrace-debug
mkdir -p $APROJECTS/build-cpptrace-debug


cd $APROJECTS//build-cpptrace-debug
cmake -S $APROJECTS/cpptrace -B $APROJECTS/build-cpptrace-debug -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=$APROJECTS/cpptrace-debug
cmake --build $APROJECTS/build-cpptrace-debug --target clean
cmake --build $APROJECTS/build-cpptrace-debug --target all
cmake --install $APROJECTS/build-cpptrace-debug
