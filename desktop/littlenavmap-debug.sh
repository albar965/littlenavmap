#!/bin/bash

export LD_LIBRARY_PATH=${APROJECTS}/Marble-debug/lib:~/Qt/5.9.5/gcc_64/lib

cd $HOME/Projekte/build-littlenavmap-debug
$HOME/Projekte/build-littlenavmap-debug/littlenavmap $@
