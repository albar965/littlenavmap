#!/bin/bash

set -e
set -x

# ===========================================================================
# ========================== atools
rm -rf ${APROJECTS}/build-atools-release
mkdir -p ${APROJECTS}/build-atools-release
cd ${APROJECTS}/build-atools-release

~/Qt/5.6/gcc_64/bin/qmake ${APROJECTS}/atools/atools.pro -spec linux-g++ CONFIG+=release
make -j4

# ===========================================================================
# ========================== littlenavmap
rm -rf ${APROJECTS}/build-littlenavmap-release
mkdir -p ${APROJECTS}/build-littlenavmap-release
cd ${APROJECTS}/build-littlenavmap-release

~/Qt/5.6/gcc_64/bin/qmake ${APROJECTS}/littlenavmap/littlenavmap.pro -spec linux-g++ CONFIG+=release
make -j4

make copydata
make deploy



