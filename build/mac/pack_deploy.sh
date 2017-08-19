#!/bin/bash

set -e
set -x

export FILENAME=`date "+20%y%m%d-%H%M"`

(
  cd ${APROJECTS}/deploy

  rm -rfv LittleNavmap.zip LittleXpConnect.zip "Little Navmap.app"

  cp -avf ${APROJECTS}/build-littlenavmap-release/littlenavmap.app "${APROJECTS}/deploy/Little Navmap.app"

  zip -r LittleNavmap.zip "Little Navmap.app" "Little XpConnect"
  cp -afv LittleNavmap.zip /Volumes/public/LittleNavmap-macOS-${FILENAME}.zip

  zip -r LittleXpConnect.zip "Little XpConnect"
  cp -afv LittleXpConnect.zip /Volumes/public/LittleXpConnect-macOS-${FILENAME}.zip
)
