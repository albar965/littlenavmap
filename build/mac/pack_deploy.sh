#!/bin/bash

# Echo all commands and exit on failure
set -e
set -x

# Error checking for required variable APROJECTS
if [ -z "$APROJECTS" ] ; then echo APROJECTS environment variable not set ; exit 1 ; fi
if [ ! -d "$APROJECTS" ]; then echo "$APROJECTS" does not exist ; exit 1 ; fi

# Override by envrionment variable for another target
export SSH_DEPLOY_TARGET=${SSH_DEPLOY_TARGET:-"sol:/data/alex/Public/Releases"}

(
  cd ${APROJECTS}/deploy

  rm -rfv LittleNavmap.zip LittleNavconnect.zip LittleXpconnect.zip

  cp -avf "Little Xpconnect arm64/README.txt" README-LittleXpconnect.txt
  cp -avf "Little Xpconnect arm64/CHANGELOG.txt" CHANGELOG-LittleXpconnect.txt

  zip -r -y -9 LittleNavmap.zip \
    "CHANGELOG-LittleNavconnect.txt" \
    "CHANGELOG-LittleNavmap.txt" \
    "CHANGELOG-LittleXpconnect.txt" \
    "LICENSE.txt" \
    "Little Navconnect.app" \
    "Little Navmap Portable.command" \
    "Little Navmap.app" \
    "Little Xpconnect arm64" \
    "Little Xpconnect x86" \
    "README-LittleNavconnect.txt" \
    "README-LittleNavmap.txt" \
    "README-LittleXpconnect.txt" \
    "revision-LittleNavmap.txt" \
    "version-LittleNavmap.txt"

#  zip -r -y -9 LittleXpconnect.zip "Little Xpconnect"

#  zip -r -y -9 LittleNavconnect.zip "Little Navconnect.app" \
#         LICENSE.txt README-LittleNavconnect.txt CHANGELOG-LittleNavconnect.txt \
#         revision-LittleNavconnect.txt version-LittleNavconnect.txt

  export FILENAME_LNM=$(head -n1 "${APROJECTS}/deploy/version-LittleNavmap.txt")
#  export FILENAME_LNC=$(head -n1 "${APROJECTS}/deploy/version-LittleNavconnect.txt")
#  export FILENAME_LXP=$(head -n1 "${APROJECTS}/deploy/Little Xpconnect/version.txt")

  scp LittleNavmap.zip ${SSH_DEPLOY_TARGET}/LittleNavmap-macOS-${FILENAME_LNM}.zip
#  scp LittleNavconnect.zip ${SSH_DEPLOY_TARGET}/LittleNavconnect-macOS-${FILENAME_LNC}.zip
#  scp LittleXpconnect.zip ${SSH_DEPLOY_TARGET}/LittleXpconnect-macOS-${FILENAME_LXP}.zip
)
