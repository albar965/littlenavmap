#!/bin/bash

set -e
set -x

# ===========================================================================
# ========================== atools
rm -rf ${APROJECTS}/build-atools-release
mkdir -p ${APROJECTS}/build-atools-release
cd ${APROJECTS}/build-atools-release

qmake ${APROJECTS}/atools/atools.pro -spec macx-clang CONFIG+=x86_64 CONFIG+=release
make -j2

# ===========================================================================
# ========================== littlexpconnect
rm -rf ${APROJECTS}/build-littlexpconnect-release
mkdir -p ${APROJECTS}/build-littlexpconnect-release
cd ${APROJECTS}/build-littlexpconnect-release

qmake ${APROJECTS}/littlexpconnect/littlexpconnect.pro -spec macx-clang CONFIG+=x86_64 CONFIG+=release
make -j2
make deploy -i -l

# ===========================================================================
# ========================== littlenavconnect
rm -rf ${APROJECTS}/build-littlenavconnect-release
mkdir -p ${APROJECTS}/build-littlenavconnect-release
cd ${APROJECTS}/build-littlenavconnect-release

qmake ${APROJECTS}/littlenavconnect/littlenavconnect.pro -spec macx-clang CONFIG+=x86_64 CONFIG+=release
make -j2
make copydata
make deploy -i -l

# ===========================================================================
# ========================== littlenavmap
rm -rf ${APROJECTS}/build-littlenavmap-release
mkdir -p ${APROJECTS}/build-littlenavmap-release
cd ${APROJECTS}/build-littlenavmap-release

qmake ${APROJECTS}/littlenavmap/littlenavmap.pro -spec macx-clang CONFIG+=x86_64 CONFIG+=release
make -j2
make copydata
make deploy -i -l



