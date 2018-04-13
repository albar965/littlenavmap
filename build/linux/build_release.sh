#!/bin/bash

set -e
set -x

# ===========================================================================
# ========================== atools
rm -rf ${APROJECTS}/build-atools-release
mkdir -p ${APROJECTS}/build-atools-release
cd ${APROJECTS}/build-atools-release

~/Qt/5.9.5/gcc_64/bin/qmake ${APROJECTS}/atools/atools.pro -spec linux-g++ CONFIG+=release
make -j4

# ===========================================================================
# ========================== littlenavmap
rm -rf ${APROJECTS}/build-littlenavmap-release
mkdir -p ${APROJECTS}/build-littlenavmap-release
cd ${APROJECTS}/build-littlenavmap-release

~/Qt/5.9.5/gcc_64/bin/qmake ${APROJECTS}/littlenavmap/littlenavmap.pro -spec linux-g++ CONFIG+=release
make -j4

make copydata
make deploy

# ===========================================================================
# ========================== littlenavconnect
rm -rf ${APROJECTS}/build-littlenavconnect-release
mkdir -p ${APROJECTS}/build-littlenavconnect-release
cd ${APROJECTS}/build-littlenavconnect-release

~/Qt/5.9.5/gcc_64/bin/qmake ${APROJECTS}/littlenavconnect/littlenavconnect.pro -spec linux-g++ CONFIG+=release
make -j4

make copydata
make deploy

# ===========================================================================
# ========================== littlexpconnect
rm -rf ${APROJECTS}/build-littlexpconnect-release
mkdir -p ${APROJECTS}/build-littlexpconnect-release
cd ${APROJECTS}/build-littlexpconnect-release

~/Qt/5.9.5/gcc_64/bin/qmake ${APROJECTS}/littlexpconnect/littlexpconnect.pro -spec linux-g++ CONFIG+=release
make -j4

make deploy



