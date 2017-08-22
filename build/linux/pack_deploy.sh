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

cp -afv ${APROJECTS}/deploy/LittleNavmap.tar.gz /data/alex/Public/LittleNavmap-linux-${FILENAME}.tar.gz
cp -afv ${APROJECTS}/deploy/LittleXpconnect.tar.gz /data/alex/Public/LittleXpconnect-linux-${FILENAME}.tar.gz
cp -afv ${APROJECTS}/deploy/LittleNavconnect.tar.gz /data/alex/Public/LittleNavconnect-linux-${FILENAME}.tar.gz


