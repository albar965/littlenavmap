#!/bin/bash

# Script for internal debugging/startup

CONF=debug

cd ${APROJECTS}/build-littlenavmap-${CONF}

export LD_LIBRARY_PATH=${APROJECTS}/Marble-${CONF}/lib:~/Qt/5.12.8/gcc_64/lib:${APROJECTS}/build-littlenavmap-${CONF}

${APROJECTS}/build-littlenavmap-${CONF}/littlenavmap $@
