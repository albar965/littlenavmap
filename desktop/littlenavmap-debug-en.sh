#!/bin/bash

export LD_LIBRARY_PATH=~/Programme/Marble-debug/lib:~/Qt/5.9.5/gcc_64/lib
export LANG=en_US

cd $HOME/Projekte/build-littlenavmap-debug
$HOME/Projekte/build-littlenavmap-debug/littlenavmap $@
