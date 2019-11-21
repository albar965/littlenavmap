#!/bin/bash

# Echo all commands and exit on failure
set -e
set -x

# Error checking for required variable APROJECTS
if [ -z "$APROJECTS" ] ; then echo APROJECTS environment variable not set ; exit 1 ; fi
if [ ! -d "$APROJECTS" ]; then echo "$APROJECTS" does not exist ; exit 1 ; fi

export FILENAME=`date "+20%y%m%d-%H%M"`

# Override by envrionment variable for another target
export SSH_DEPLOY_TARGET=${SSH_DEPLOY_TARGET:-"sol:/data/alex/Public/Releases"}

(
  cd ${APROJECTS}/deploy


  rm -rfv LittleNavmap.zip LittleNavconnect.zip LittleXpconnect.zip

  zip -r -y -9 LittleNavmap.zip "Little Navmap.app" "Little Navconnect.app" "Little Xpconnect" \
         LICENSE.txt README-LittleNavconnect.txt CHANGELOG-LittleNavconnect.txt \
         README-LittleNavmap.txt CHANGELOG-LittleNavmap.txt

  zip -r -y -9 LittleXpconnect.zip "Little Xpconnect"

  zip -r -y -9 LittleNavconnect.zip "Little Navconnect.app" \
         LICENSE.txt README-LittleNavconnect.txt CHANGELOG-LittleNavconnect.txt

  scp LittleXpconnect.zip ${SSH_DEPLOY_TARGET}/LittleXpconnect-macOS-${FILENAME}.zip
  scp LittleNavconnect.zip ${SSH_DEPLOY_TARGET}/LittleNavconnect-macOS-${FILENAME}.zip
  scp LittleNavmap.zip ${SSH_DEPLOY_TARGET}/LittleNavmap-macOS-${FILENAME}.zip
)
