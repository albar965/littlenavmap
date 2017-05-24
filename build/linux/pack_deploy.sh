#!/bin/bash

set -e
set -x

export FILENAME=`date "+20%y%m%d-%H%M"`

(
cd ${APROJECTS}/deploy
tar cfvz LittleNavmap.tar.gz "Little Navmap"
)

cp -afv ${APROJECTS}/deploy/LittleNavmap.tar.gz /data/alex/Public/LittleNavmap-${FILENAME}.tar.gz


