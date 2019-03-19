#!/bin/bash

# Echo all commands and exit on failure
set -e
set -x

# Error checking for required variable APROJECTS
if [ -z "$APROJECTS" ] ; then echo APROJECTS environment variable not set ; exit 1 ; fi
if [ ! -d "$APROJECTS" ]; then echo "$APROJECTS" does not exist ; exit 1 ; fi

# =============================================================================
# Set the required environment variable APROJECTS to the base directory for
# atools, littlenavmap, littlenavconnect and littlexpconnect.

# =============================================================================
# Configuration can be overloaded on the command line by setting the
# variables below before calling this script.
#
# See the *.pro project files for more information.
#
# Example:
# export QMAKE_STATIC=~/Projekte/build-qt-5.12.0-release/bin/qmake
# export MARBLE_LIB_PATH=/home/alex/Programme/Marble-debug/lib
# export MARBLE_INC_PATH=/home/alex/Programme/Marble-debug/include

export CONF_TYPE=${CONF_TYPE:-"release"}
export ATOOLS_INC_PATH=${ATOOLS_INC_PATH:-"../atools/src"}
export ATOOLS_LIB_PATH=${ATOOLS_LIB_PATH:-"../build-atools-${CONF_TYPE}"}
export MARBLE_INC_PATH=${MARBLE_INC_PATH:-"${APROJECTS}/Marble-${CONF_TYPE}/include"}
export MARBLE_LIB_PATH=${MARBLE_LIB_PATH:-"${APROJECTS}/Marble-${CONF_TYPE}/lib"}
export DATABASE_BASE=${DATABASE_BASE:-"${APROJECTS}/little_navmap_db"}
export HELP_BASE=${HELP_BASE:-"${APROJECTS}/little_navmap_help"}

# Defines the used Qt for all builds
export QMAKE_SHARED=${QMAKE_SHARED:-"${HOME}/Qt/5.9.5/gcc_64/bin/qmake"}

# Defines the used Qt for Xpconnect
export QMAKE_STATIC=${QMAKE_STATIC:-"/mnt/disk/build-qt-5.12-release/bin/qmake"}

# Do not change the DEPLOY_BASE since some scripts depend on it
export DEPLOY_BASE="${APROJECTS}/deploy"

# ===========================================================================
# ========================== littlenavmap and littlenavconnect - shared Qt
# ===========================================================================

# ===========================================================================
# ========================== atools
rm -rf ${APROJECTS}/build-atools-release
mkdir -p ${APROJECTS}/build-atools-release
cd ${APROJECTS}/build-atools-release

${QMAKE_SHARED} ${APROJECTS}/atools/atools.pro -spec linux-g++ CONFIG+=release
make -j4

# ===========================================================================
# ========================== littlenavmap
rm -rf ${APROJECTS}/build-littlenavmap-release
mkdir -p ${APROJECTS}/build-littlenavmap-release
cd ${APROJECTS}/build-littlenavmap-release

${QMAKE_SHARED} ${APROJECTS}/littlenavmap/littlenavmap.pro -spec linux-g++ CONFIG+=release
make -j4

make copydata
make deploy

# ===========================================================================
# ========================== littlenavconnect
rm -rf ${APROJECTS}/build-littlenavconnect-release
mkdir -p ${APROJECTS}/build-littlenavconnect-release
cd ${APROJECTS}/build-littlenavconnect-release

${QMAKE_SHARED} ${APROJECTS}/littlenavconnect/littlenavconnect.pro -spec linux-g++ CONFIG+=release
make -j4

make copydata
make deploy

# ===========================================================================
# ========================== littlexpconnect - static Qt
# ===========================================================================

# ========================== atools
rm -rf ${APROJECTS}/build-atools-release
mkdir -p ${APROJECTS}/build-atools-release
cd ${APROJECTS}/build-atools-release

${QMAKE_STATIC} ${APROJECTS}/atools/atools.pro -spec linux-g++ CONFIG+=release
make -j4

# ========================== xpconnect
rm -rf ${APROJECTS}/build-littlexpconnect-release
mkdir -p ${APROJECTS}/build-littlexpconnect-release
cd ${APROJECTS}/build-littlexpconnect-release

${QMAKE_STATIC} ${APROJECTS}/littlexpconnect/littlexpconnect.pro -spec linux-g++ CONFIG+=release
make -j4

make deploy
