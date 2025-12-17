#!/bin/bash

# Script for internal debugging/startup

CONF=debug
export QT_PATH=${QT_PATH:-"$HOME/Qt"}
export QT_PREFIX_PATH="${QT_PATH}/6.5.3/gcc_64"

cd ${APROJECTS}/build-littlenavmap-${CONF}

export LD_LIBRARY_PATH=${APROJECTS}/Marble-${CONF}/lib:${QT_PREFIX_PATH}/lib:${APROJECTS}/build-littlenavmap-${CONF}

${APROJECTS}/build-littlenavmap-${CONF}/littlenavmap "$@"
