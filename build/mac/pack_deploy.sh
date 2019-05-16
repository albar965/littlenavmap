#!/bin/bash

set -e
set -x

export FILENAME=`date "+20%y%m%d-%H%M"`

(
  cd ${APROJECTS}/deploy

  rm -rfv LittleNavmap.zip LittleNavconnect.zip LittleXpconnect.zip

  zip -r -y -9 LittleNavmap.zip "Little Navmap.app" "Little Navconnect.app" "Little Xpconnect" \
         LICENSE.txt README-LittleNavconnect.txt CHANGELOG-LittleNavconnect.txt \
         README-LittleNavmap.txt CHANGELOG-LittleNavmap.txt

  zip -r -y -9 LittleXpconnect.zip "Little Xpconnect"

  zip -r -y -9 LittleNavconnect.zip "Little Navconnect.app" \
         LICENSE.txt README-LittleNavconnect.txt CHANGELOG-LittleNavconnect.txt

  scp LittleXpconnect.zip sol:/data/alex/Public/Releases/LittleXpconnect-macOS-${FILENAME}.zip
  scp LittleNavconnect.zip sol:/data/alex/Public/Releases/LittleNavconnect-macOS-${FILENAME}.zip
  scp LittleNavmap.zip sol:/data/alex/Public/Releases/LittleNavmap-macOS-${FILENAME}.zip
)
