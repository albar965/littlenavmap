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
# Configuration can be overloaded on the command line by defining the
# variables below before calling this script.
#
# See the *.pro project files for more information.
#
# Example:
# export MARBLE_LIB_PATH=/home/alex/Programme/Marble-debug/lib
# export MARBLE_INC_PATH=/home/alex/Programme/Marble-debug/include

export CONF_TYPE=${CONF_TYPE:-"release"}
export ATOOLS_INC_PATH=${ATOOLS_INC_PATH:-"${APROJECTS}/atools/src"}
export ATOOLS_LIB_PATH=${ATOOLS_LIB_PATH:-"${APROJECTS}/build-atools-${CONF_TYPE}"}
export MARBLE_INC_PATH=${MARBLE_INC_PATH:-"${APROJECTS}/Marble-${CONF_TYPE}/include"}
export MARBLE_LIB_PATH=${MARBLE_LIB_PATH:-"${APROJECTS}/Marble-${CONF_TYPE}/lib"}
export XPSDK_BASE=${XPSDK_BASE:-"${APROJECTS}/X-Plane SDK"}
export DATABASE_BASE=${DATABASE_BASE:-"${APROJECTS}/little_navmap_db"}
export HELP_BASE=${HELP_BASE:-"${APROJECTS}/little_navmap_help"}

# Defines the used Qt for all builds  - x86 and Qt 5.15
export QMAKE_SHARED=${QMAKE_SHARED:-"${HOME}/Qt/5.15.2/clang_64/bin/qmake"}

# Defines the used Qt for all the Intel/ARM Xpconnect build - x86 and arm64 and Qt 6.4
export QMAKE_SHARED_ARM=${QMAKE_SHARED_ARM:-"${HOME}/Qt/6.4.0/macos/bin/qmake"}


# Do not change the DEPLOY_BASE since some scripts depend on it
export DEPLOY_BASE="${APROJECTS}/deploy"

export INSTALL_MARBLE_DYLIB=$APROJECTS/build-marble-release/src/lib/marble/libmarblewidget-qt5.25.dylib


# ===========================================================================
# ========================== atools - x86 and arm64 and Qt 6.4
rm -rf ${APROJECTS}/build-atools-${CONF_TYPE}
mkdir -p ${APROJECTS}/build-atools-${CONF_TYPE}
cd ${APROJECTS}/build-atools-${CONF_TYPE}

export ATOOLS_NO_FS=true
export ATOOLS_NO_GRIB=true
export ATOOLS_NO_GUI=true
export ATOOLS_NO_ROUTING=true
export ATOOLS_NO_SQL=true
export ATOOLS_NO_TRACK=true
export ATOOLS_NO_USERDATA=true
export ATOOLS_NO_WEATHER=true
export ATOOLS_NO_WEB=true
export ATOOLS_NO_WMM=true

${QMAKE_SHARED_ARM} ${APROJECTS}/atools/atools.pro -spec macx-clang CONFIG+=x86_64 CONFIG+=${CONF_TYPE}  'QMAKE_APPLE_DEVICE_ARCHS=x86_64 arm64'
make -j4

# ===========================================================================
# ========================== littlexpconnect - x86 and arm64 and Qt 6.4
rm -rf ${APROJECTS}/build-littlexpconnect-${CONF_TYPE}
mkdir -p ${APROJECTS}/build-littlexpconnect-${CONF_TYPE}
cd ${APROJECTS}/build-littlexpconnect-${CONF_TYPE}

${QMAKE_SHARED_ARM} ${APROJECTS}/littlexpconnect/littlexpconnect.pro -spec macx-clang CONFIG+=x86_64 CONFIG+=${CONF_TYPE} 'QMAKE_APPLE_DEVICE_ARCHS=x86_64 arm64'
make -j4
make deploy -i -l

rm -rf "${DEPLOY_BASE}/Little Xpconnect arm64"
mv "${DEPLOY_BASE}/Little Xpconnect" "${DEPLOY_BASE}/Little Xpconnect arm64"

unset ATOOLS_NO_FS
unset ATOOLS_NO_GRIB
unset ATOOLS_NO_GUI
unset ATOOLS_NO_ROUTING
unset ATOOLS_NO_SQL
unset ATOOLS_NO_TRACK
unset ATOOLS_NO_USERDATA
unset ATOOLS_NO_WEATHER
unset ATOOLS_NO_WEB
unset ATOOLS_NO_WMM

# ===========================================================================
# ========================== atools - x86 and Qt 5.15
rm -rf ${APROJECTS}/build-atools-${CONF_TYPE}
mkdir -p ${APROJECTS}/build-atools-${CONF_TYPE}
cd ${APROJECTS}/build-atools-${CONF_TYPE}

${QMAKE_SHARED} ${APROJECTS}/atools/atools.pro -spec macx-clang CONFIG+=x86_64 CONFIG+=${CONF_TYPE}
make -j4

# ===========================================================================
# ========================== littlexpconnect - x86 and Qt 5.15
rm -rf ${APROJECTS}/build-littlexpconnect-x86-${CONF_TYPE}
mkdir -p ${APROJECTS}/build-littlexpconnect-x86-${CONF_TYPE}
cd ${APROJECTS}/build-littlexpconnect-x86-${CONF_TYPE}

${QMAKE_SHARED} ${APROJECTS}/littlexpconnect/littlexpconnect.pro -spec macx-clang CONFIG+=x86_64 CONFIG+=${CONF_TYPE}
make -j4
make deploy -i -l

rm -rf "${DEPLOY_BASE}/Little Xpconnect x86"
mv "${DEPLOY_BASE}/Little Xpconnect" "${DEPLOY_BASE}/Little Xpconnect x86"

# ===========================================================================
# ========================== littlenavconnect
rm -rf ${APROJECTS}/build-littlenavconnect-${CONF_TYPE}
mkdir -p ${APROJECTS}/build-littlenavconnect-${CONF_TYPE}
cd ${APROJECTS}/build-littlenavconnect-${CONF_TYPE}

${QMAKE_SHARED} ${APROJECTS}/littlenavconnect/littlenavconnect.pro -spec macx-clang CONFIG+=x86_64 CONFIG+=${CONF_TYPE}
make -j4
make copydata
make deploy -i -l

# ===========================================================================
# ========================== littlenavmap
rm -rf ${APROJECTS}/build-littlenavmap-${CONF_TYPE}
mkdir -p ${APROJECTS}/build-littlenavmap-${CONF_TYPE}
cd ${APROJECTS}/build-littlenavmap-${CONF_TYPE}

${QMAKE_SHARED} ${APROJECTS}/littlenavmap/littlenavmap.pro -spec macx-clang CONFIG+=x86_64 CONFIG+=${CONF_TYPE}
make -j4
make copydata
make deploy -i -l



