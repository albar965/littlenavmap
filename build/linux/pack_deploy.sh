#!/bin/bash

# Echo all commands and exit on failure
set -e
set -x

# Error checking for required variable APROJECTS
if [ -z "$APROJECTS" ] ; then echo APROJECTS environment variable not set ; exit 1 ; fi
if [ ! -d "$APROJECTS" ]; then echo "$APROJECTS" does not exist ; exit 1 ; fi

# Override by envrionment variable for another target
export SSH_DEPLOY_TARGET=${SSH_DEPLOY_TARGET:-"sol:/data/alex/Public/Releases"}

# LittleNavmap-linux-22.04-2.8.2.beta.tar.gz
if [ -f "/etc/lsb-release" ]; then
  source /etc/lsb-release
  export FILENAME_LNM=$DISTRIB_RELEASE-$(head -n1 "${APROJECTS}/deploy/Little Navmap/version.txt")
else
  export FILENAME_LNM=$(head -n1 "${APROJECTS}/deploy/Little Navmap/version.txt")
fi

(
  cd ${APROJECTS}/deploy

  rm -rfv "LittleNavmap-linux-${FILENAME_LNM}"
  rm -fv LittleNavmap-linux-${FILENAME_LNM}.tar.gz

  cp -av "Little Navmap" "LittleNavmap-linux-${FILENAME_LNM}"

  cp -av "Little Navconnect" "LittleNavmap-linux-${FILENAME_LNM}"
  cp -av "Little Xpconnect" "LittleNavmap-linux-${FILENAME_LNM}"


  tar cfvz LittleNavmap-linux-${FILENAME_LNM}.tar.gz "LittleNavmap-linux-${FILENAME_LNM}"
)

scp ${APROJECTS}/deploy/LittleNavmap-linux-${FILENAME_LNM}.tar.gz ${SSH_DEPLOY_TARGET}/LittleNavmap-linux-${FILENAME_LNM}.tar.gz


