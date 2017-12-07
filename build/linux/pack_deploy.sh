#!/bin/bash

set -e
set -x

export FILENAME=`date "+20%y%m%d-%H%M"`

(
cd ${APROJECTS}/deploy
cp -av "Little Navconnect" "Little Navmap"
cp -av "Little Xpconnect" "Little Navmap"

tar cfvz LittleNavmap.tar.gz "Little Navmap"
tar cfvz LittleXpconnect.tar.gz "Little Xpconnect"
tar cfvz LittleNavconnect.tar.gz "Little Navconnect"
)

scp ${APROJECTS}/deploy/LittleNavmap.tar.gz darkon:/data/alex/Public/Releases/LittleNavmap-linux-${FILENAME}.tar.gz
scp ${APROJECTS}/deploy/LittleXpconnect.tar.gz darkon:/data/alex/Public/Releases/LittleXpconnect-linux-${FILENAME}.tar.gz
scp ${APROJECTS}/deploy/LittleNavconnect.tar.gz darkon:/data/alex/Public/Releases/LittleNavconnect-linux-${FILENAME}.tar.gz


