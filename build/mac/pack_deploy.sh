#!/bin/bash

set -e
set -x

export FILENAME=`date "+20%y%m%d-%H%M"`

(
  cd ${APROJECTS}/deploy

  rm -rfv LittleNavmap.zip LittleNavconnect.zip LittleXpconnect.zip

  zip -r LittleNavmap.zip "Little Navmap.app" "Little Navconnect.app" "Little Xpconnect" \
         LICENSE.txt README-LittleNavconnect.txt CHANGELOG-LittleNavconnect.txt \
         README-LittleNavmap.txt CHANGELOG-LittleNavmap.txt

  zip -r LittleXpconnect.zip "Little Xpconnect"

  zip -r LittleNavconnect.zip "Little Navconnect.app" \
         LICENSE.txt README-LittleNavconnect.txt CHANGELOG-LittleNavconnect.txt

  cp -afv LittleXpconnect.zip /Volumes/public/LittleXpconnect-macOS-${FILENAME}.zip
  cp -afv LittleNavconnect.zip /Volumes/public/LittleNavconnect-macOS-${FILENAME}.zip
  cp -afv LittleNavmap.zip /Volumes/public/LittleNavmap-macOS-${FILENAME}.zip
)
