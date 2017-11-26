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

  scp LittleXpconnect.zip darkon:/data/alex/Public/Releases/LittleXpconnect-macOS-${FILENAME}.zip
  scp LittleNavconnect.zip darkon:/data/alex/Public/Releases/LittleNavconnect-macOS-${FILENAME}.zip
  scp LittleNavmap.zip darkon:/data/alex/Public/Releases/LittleNavmap-macOS-${FILENAME}.zip
)
