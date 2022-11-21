@echo off
rem ===========================================================================
rem This script runs Little Navmap in portable mode.
rem It can be used to execute the program from a memory stick without modifying
rem the system. Cached files, databases and settings are placed in the
rem installation folder when using this script.
rem Cached map tiles are stored in the local folder "Little Navmap Cache" and
rem setting files as well as databases are stored in "Little Navmap Settings"
rem
rem In normal operation, this is not necessary. Start the program as usual by
rem double clicking on the file "littlenavmap.exe".
rem ===========================================================================

start /B littlenavmap -c "Little Navmap Cache" -p "Little Navmap Settings"  -l "Little Navmap Logs"
