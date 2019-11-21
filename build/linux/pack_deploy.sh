#!/bin/bash

# Echo all commands and exit on failure
set -e
set -x

# Error checking for required variable APROJECTS
if [ -z "$APROJECTS" ] ; then echo APROJECTS environment variable not set ; exit 1 ; fi
if [ ! -d "$APROJECTS" ]; then echo "$APROJECTS" does not exist ; exit 1 ; fi

# Override by envrionment variable for another target
export SSH_DEPLOY_TARGET=${SSH_DEPLOY_TARGET:-"sol:/data/alex/Public/Releases"}

export FILENAME=`date "+20%y%m%d-%H%M"`

(
cd ${APROJECTS}/deploy
cp -av "Little Navconnect" "Little Navmap"
cp -av "Little Xpconnect" "Little Navmap"

tar cfvz LittleNavmap.tar.gz "Little Navmap"
tar cfvz LittleXpconnect.tar.gz "Little Xpconnect"
tar cfvz LittleNavconnect.tar.gz "Little Navconnect"
)


scp ${APROJECTS}/deploy/LittleNavmap.tar.gz ${SSH_DEPLOY_TARGET}/LittleNavmap-linux-${FILENAME}.tar.gz
scp ${APROJECTS}/deploy/LittleXpconnect.tar.gz ${SSH_DEPLOY_TARGET}/LittleXpconnect-linux-${FILENAME}.tar.gz
scp ${APROJECTS}/deploy/LittleNavconnect.tar.gz ${SSH_DEPLOY_TARGET}/LittleNavconnect-linux-${FILENAME}.tar.gz


