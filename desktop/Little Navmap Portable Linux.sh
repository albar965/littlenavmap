#!/bin/bash

# ===========================================================================
# This script runs Little Navmap in portable mode.
# It can be used to execute the program from a memory stick without modifying
# the system. Cached files, databases and settings are placed in the
# installation folder when using this script.
# Cached map tiles are stored in the local folder "Little Navmap Cache" and
# setting files as well as databases are stored in "Little Navmap Settings"
#
# In normal operation, this is not necessary. Start the program as usual by
# double clicking on the file "littlenavmap".
# ===========================================================================

./littlenavmap -c "Little Navmap Cache" -p "Little Navmap Settings" -l "Little Navmap Logs"
