#!/bin/bash

# Echo all commands and exit on failure
set -e
set -x

# Error checking for required variable APROJECTS
if [ -z "$APROJECTS" ] ; then echo APROJECTS environment variable not set ; exit 1 ; fi
if [ ! -d "$APROJECTS" ]; then echo "$APROJECTS" does not exist ; exit 1 ; fi

# Override by envrionment variable for another target
export SSH_DEPLOY_TARGET=${SSH_DEPLOY_TARGET:-"sol:/data/alex/Public/Releases"}

if [ -f "/etc/lsb-release" ]; then
  source /etc/lsb-release
  export FILENAME_LNM=$DISTRIB_RELEASE-$(head -n1 "${APROJECTS}/deploy/Little Navmap/version.txt")
  export FILENAME_LNC=$DISTRIB_RELEASE-$(head -n1 "${APROJECTS}/deploy/Little Navconnect/version.txt")
  export FILENAME_LXP=$DISTRIB_RELEASE-$(head -n1 "${APROJECTS}/deploy/Little Xpconnect/version.txt")
else
  export FILENAME_LNM=$(head -n1 "${APROJECTS}/deploy/Little Navmap/version.txt")
  export FILENAME_LNC=$(head -n1 "${APROJECTS}/deploy/Little Navconnect/version.txt")
  export FILENAME_LXP=$(head -n1 "${APROJECTS}/deploy/Little Xpconnect/version.txt")
fi

(
  cd ${APROJECTS}/deploy
  cp -av "Little Navconnect" "Little Navmap"
  cp -av "Little Xpconnect" "Little Navmap"

  tar cfvz LittleNavmap.tar.gz "Little Navmap"
  tar cfvz LittleXpconnect.tar.gz "Little Xpconnect"
  tar cfvz LittleNavconnect.tar.gz "Little Navconnect"
)

scp ${APROJECTS}/deploy/LittleNavmap.tar.gz ${SSH_DEPLOY_TARGET}/LittleNavmap-linux-${FILENAME_LNM}.tar.gz
scp ${APROJECTS}/deploy/LittleNavconnect.tar.gz ${SSH_DEPLOY_TARGET}/LittleNavconnect-linux-${FILENAME_LNC}.tar.gz
scp ${APROJECTS}/deploy/LittleXpconnect.tar.gz ${SSH_DEPLOY_TARGET}/LittleXpconnect-linux-${FILENAME_LXP}.tar.gz


