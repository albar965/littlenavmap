#!/bin/bash

# Script for internal debugging/startup

CONF=debug
export LANG=en_US.UTF-8
export LANGUAGE=en_US:en_GB
export LD_LIBRARY_PATH=${APROJECTS}/Marble-${CONF}/lib:~/Qt/5.12.4/gcc_64/lib:${APROJECTS}/build-littlenavmap-${CONF}

cd ${APROJECTS}/build-littlenavmap-${CONF}

${APROJECTS}/build-littlenavmap-${CONF}/littlenavmap $@
