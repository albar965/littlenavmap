#!/bin/bash

# Echo all commands and exit on failure
set -e
set -x

# Error checking for required variable APROJECTS
if [ -z "$APROJECTS" ] ; then echo APROJECTS environment variable not set ; exit 1 ; fi
if [ ! -d "$APROJECTS" ]; then echo "$APROJECTS" does not exist ; exit 1 ; fi

cd ${APROJECTS}/atools

git pull --verbose  --tags

cd ${APROJECTS}/littlenavmap

git pull --verbose  --tags

cd ${APROJECTS}/littlenavconnect

git pull --verbose  --tags

cd ${APROJECTS}/littlexpconnect

git pull --verbose  --tags
