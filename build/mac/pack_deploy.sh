#!/bin/bash

set -e
set -x

export FILENAME=`date "+20%y%m%d-%H%M"`

cp -afv ${APROJECTS}/build-littlenavmap-release/littlenavmap.dmg /Volumes/public/LittleNavmap-${FILENAME}.dmg


