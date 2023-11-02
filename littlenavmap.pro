#*****************************************************************************
# Copyright 2015-2020 Alexander Barthel alex@littlenavmap.org
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#****************************************************************************

# =============================================================================
# Set these environment variables for configuration - do not change this .pro file
# =============================================================================
#
# ATOOLS_INC_PATH
# Optional. Path to atools include. Default is "../atools/src" if not set.
# Also reads *.qm translation files from "$ATOOLS_INC_PATH/..".
#
# ATOOLS_LIB_PATH
# Optional. Path to atools static library. Default is "../build-atools-$${CONF_TYPE}"
# if not set.
#
# MARBLE_INC_PATH
# Required. Path to Marble include files of a Marble installation.
# Example: /home/YOU/Programs/Marble/include
#
# MARBLE_LIB_PATH
# Required. Path to Marble library files of a Marble installation.
# Example: /home/YOU/Programs/Marble/lib
# Also reads plugins from "$$MARBLE_LIB_PATH/../plugins" or "$$MARBLE_LIB_PATH/lib/plugins" depending on OS.
#
# INSTALL_MARBLE_DYLIB
# Required for macOS only. Path to the Marble shared library in the Marble build folder
# that was created by running "make all".
# Example: $HOME/Projekte/build-marble-$$CONF_TYPE/src/lib/marble/libmarblewidget-qt5.25.dylib
#
# OPENSSL_PATH
# Windows: Base path of WinSSL 1.1.1 installation which can optionally installed with the Qt Installer.
#          Defaults to "$$[QT_INSTALL_PREFIX])\..\..\Tools\OpenSSL\Win_x86\bin\" (e.g. "C:\Qt\Tools\OpenSSL\Win_x86\bin\") if empty.
#          Note that the Qt OpenSSL requires the vcredist_x86_2010.exe and vcredist_x64_2010.exe installation.
# Linux:   Not used.
# macOS:   Not used.
#
# ATOOLS_GIT_PATH
# Optional. Path to GIT executable. Revision will be set to "UNKNOWN" if not set.
# Uses "git" on macOS and Linux as default if not set.
# Example: "C:\Git\bin\git"
#
# ATOOLS_SIMCONNECT_PATH
# Path to SimConnect SDK. SimConnect support will be omitted in build if not set.
# Example: "C:\Program Files (x86)\Microsoft Games\Microsoft Flight Simulator X SDK\SDK\Core Utilities Kit\SimConnect SDK"
#
# DEPLOY_BASE
# Target folder for "make deploy". Optional. Default is "../deploy" plus project name ($$TARGET_NAME).
#
# DATABASE_BASE
# Source folder for database files that are copied into the distribution on deploy.
# Optional. Ignored if it does not exist. Default is "$$PWD/../little_navmap_db".
#
# HELP_BASE
# Source folder for additional help files that are copied into the distribution on deploy.
# Optional. Ignored if it does not exist. Default is "$$PWD/../little_navmap_help".
#
# ATOOLS_QUIET
# Optional. Set this to "true" to avoid qmake messages.
#
# =============================================================================
# End of configuration documentation
# =============================================================================

# Define program version here VERSION_NUMBER_TODO
VERSION_NUMBER=2.9.0.develop

QT += core gui sql xml network svg printsupport

CONFIG += build_all c++14
CONFIG -= debug_and_release debug_and_release_target

TARGET = littlenavmap
TEMPLATE = app

win32 { contains(QT_ARCH, i386) { WINARCH = win32 } else { WINARCH = win64 } }

TARGET_NAME=Little Navmap

# =======================================================================
# Copy environment variables into qmake variables
ATOOLS_INC_PATH=$$(ATOOLS_INC_PATH)
ATOOLS_LIB_PATH=$$(ATOOLS_LIB_PATH)
MARBLE_INC_PATH=$$(MARBLE_INC_PATH)
MARBLE_LIB_PATH=$$(MARBLE_LIB_PATH)
INSTALL_MARBLE_DYLIB=$$(INSTALL_MARBLE_DYLIB)
GIT_PATH=$$(ATOOLS_GIT_PATH)
OPENSSL_PATH_WIN32=$$(OPENSSL_PATH_WIN32)
OPENSSL_PATH_WIN64=$$(OPENSSL_PATH_WIN64)
SIMCONNECT_PATH_WIN32=$$(ATOOLS_SIMCONNECT_PATH_WIN32)
SIMCONNECT_PATH_WIN64=$$(ATOOLS_SIMCONNECT_PATH_WIN64)
DEPLOY_BASE=$$(DEPLOY_BASE)
DATABASE_BASE=$$(DATABASE_BASE)
HELP_BASE=$$(HELP_BASE)
QUIET=$$(ATOOLS_QUIET)

# =======================================================================
# Fill defaults for unset


CONFIG(debug, debug|release) : CONF_TYPE=debug
CONFIG(release, debug|release) : CONF_TYPE=release

isEmpty(DEPLOY_BASE) : DEPLOY_BASE=$$PWD/../deploy
isEmpty(DATABASE_BASE) : DATABASE_BASE=$$PWD/../little_navmap_db
isEmpty(HELP_BASE) : HELP_BASE=$$PWD/../little_navmap_help

isEmpty(ATOOLS_INC_PATH) : ATOOLS_INC_PATH=$$PWD/../atools/src
isEmpty(ATOOLS_LIB_PATH) : ATOOLS_LIB_PATH=$$PWD/../build-atools-$$CONF_TYPE

win32 {
isEmpty(MARBLE_INC_PATH) : MARBLE_INC_PATH=$$PWD/../Marble-$$CONF_TYPE-$$WINARCH/include
isEmpty(MARBLE_LIB_PATH) : MARBLE_LIB_PATH=$$PWD/../Marble-$$CONF_TYPE-$$WINARCH/lib
} else {
isEmpty(MARBLE_INC_PATH) : MARBLE_INC_PATH=$$PWD/../Marble-$$CONF_TYPE/include
isEmpty(MARBLE_LIB_PATH) : MARBLE_LIB_PATH=$$PWD/../Marble-$$CONF_TYPE/lib
}

# QT_INSTALL_PREFIX: C:/Qt/5.15.2/mingw81_32
# C:\Qt\Tools\OpenSSL\Win_x86\bin\
# win32: isEmpty(OPENSSL_PATH) : OPENSSL_PATH=$$[QT_INSTALL_PREFIX])\..\..\Tools\OpenSSL\Win_x86\bin\

# =======================================================================
# Set compiler flags and paths

unix:!macx {
  isEmpty(GIT_PATH) : GIT_PATH=git

  QMAKE_LFLAGS += -no-pie

  # Makes the shell script and setting LD_LIBRARY_PATH redundant
  QMAKE_RPATHDIR=.
  QMAKE_RPATHDIR+=./lib

  # Search path for the Marble widget so while linking
  QMAKE_RPATHLINKDIR=$$MARBLE_LIB_PATH

  LIBS += -L$$MARBLE_LIB_PATH -lmarblewidget-qt5 -L$$ATOOLS_LIB_PATH -latools  -lz
}

win32 {
  contains(QT_ARCH, i386) {
    # FSX or P3D
    WINARCH = win32
    !isEmpty(SIMCONNECT_PATH_WIN32) {
      DEFINES += SIMCONNECT_BUILD_WIN32 WINARCH32
      INCLUDEPATH += $$SIMCONNECT_PATH_WIN32"\inc"
      LIBS += $$SIMCONNECT_PATH_WIN32"\lib\SimConnect.lib"
      OPENSSL_PATH_WIN=$$(OPENSSL_PATH_WIN32)
    }
  } else {
  # MSFS
  WINARCH = win64
  !isEmpty(SIMCONNECT_PATH_WIN64) {
    DEFINES += SIMCONNECT_BUILD_WIN64 WINARCH64
    INCLUDEPATH += $$SIMCONNECT_PATH_WIN64"\include"
    LIBS += $$SIMCONNECT_PATH_WIN64"\lib\SimConnect.lib"
    OPENSSL_PATH_WIN=$$(OPENSSL_PATH_WIN64)
  }
}

  WINDEPLOY_FLAGS = --compiler-runtime
  CONFIG(debug, debug|release) : WINDEPLOY_FLAGS += --debug

  DEFINES += _USE_MATH_DEFINES

  CONFIG(debug, debug|release) : LIBS += -L$$MARBLE_LIB_PATH -llibmarblewidget-qt5d
  CONFIG(release, debug|release) : LIBS += -L$$MARBLE_LIB_PATH -llibmarblewidget-qt5
  LIBS += -L$$ATOOLS_LIB_PATH -latools -lz
}

macx {
  isEmpty(GIT_PATH) : GIT_PATH=git

  MACDEPLOY_FLAGS = -always-overwrite
  CONFIG(debug, debug|release) : MACDEPLOY_FLAGS += -no-strip

  LIBS += -L$$MARBLE_LIB_PATH -lmarblewidget-qt5 -L$$ATOOLS_LIB_PATH -latools  -lz
}

isEmpty(GIT_PATH) {
  GIT_REVISION=UNKNOWN
  GIT_REVISION_FULL=UNKNOWN
} else {
  GIT_REVISION=$$system('$$GIT_PATH' rev-parse --short HEAD)
  GIT_REVISION_FULL=$$system('$$GIT_PATH' rev-parse HEAD)
}

PRE_TARGETDEPS += $$ATOOLS_LIB_PATH/libatools.a
DEPENDPATH += $$ATOOLS_INC_PATH $$MARBLE_INC_PATH
INCLUDEPATH += $$PWD/src $$ATOOLS_INC_PATH $$MARBLE_INC_PATH
DEFINES += VERSION_NUMBER_LITTLENAVMAP='\\"$$VERSION_NUMBER\\"'
DEFINES += GIT_REVISION_LITTLENAVMAP='\\"$$GIT_REVISION\\"'
DEFINES += QT_NO_CAST_FROM_BYTEARRAY
DEFINES += QT_NO_CAST_TO_ASCII

# =======================================================================
# Include build_options.pro with additional variables

exists($$PWD/../build_options.pro) {
   include($$PWD/../build_options.pro)

   !isEqual(QUIET, "true") {
     message($$PWD/../build_options.pro found.)
   }
} else {
   !isEqual(QUIET, "true") {
     message($$PWD/../build_options.pro not found.)
   }
}

# =======================================================================
# Print values when running qmake

!isEqual(QUIET, "true") {
message(-----------------------------------)
message(VERSION_NUMBER: $$VERSION_NUMBER)
message(GIT_REVISION: $$GIT_REVISION)
message(GIT_REVISION_FULL: $$GIT_REVISION_FULL)
message(GIT_PATH: $$GIT_PATH)
message(WINARCH: $$WINARCH)
message(OPENSSL_PATH_WIN: $$OPENSSL_PATH_WIN)
message(ATOOLS_INC_PATH: $$ATOOLS_INC_PATH)
message(ATOOLS_LIB_PATH: $$ATOOLS_LIB_PATH)
message(MARBLE_INC_PATH: $$MARBLE_INC_PATH)
message(SIMCONNECT_PATH_WIN32: $$SIMCONNECT_PATH_WIN32)
message(SIMCONNECT_PATH_WIN64: $$SIMCONNECT_PATH_WIN64)
message(DEPLOY_BASE: $$DEPLOY_BASE)
message(DEFINES: $$DEFINES)
message(INCLUDEPATH: $$INCLUDEPATH)
message(LIBS: $$LIBS)
message(TARGET_NAME: $$TARGET_NAME)
message(QT_INSTALL_PREFIX: $$[QT_INSTALL_PREFIX])
message(QT_INSTALL_LIBS: $$[QT_INSTALL_LIBS])
message(QT_INSTALL_PLUGINS: $$[QT_INSTALL_PLUGINS])
message(QT_INSTALL_TRANSLATIONS: $$[QT_INSTALL_TRANSLATIONS])
message(QT_INSTALL_BINS: $$[QT_INSTALL_BINS])
message(CONFIG: $$CONFIG)
message(DEFINES: $$DEFINES)
message(QT: $$QT)
message(-----------------------------------)
}

# =====================================================================
# Files

SOURCES += \
  src/airspace/airspacecontroller.cpp \
  src/airspace/airspacetoolbarhandler.cpp \
  src/app/commandline.cpp \
  src/app/dataexchange.cpp \
  src/app/navapp.cpp \
  src/common/abstractinfobuilder.cpp \
  src/common/aircrafttrail.cpp \
  src/common/airportfiles.cpp \
  src/common/constants.cpp \
  src/common/coordinateconverter.cpp \
  src/common/dialogrecordhelper.cpp \
  src/common/dirtool.cpp \
  src/common/elevationprovider.cpp \
  src/common/filecheck.cpp \
  src/common/formatter.cpp \
  src/common/fueltool.cpp \
  src/common/htmlinfobuilder.cpp \
  src/common/jsoninfobuilder.cpp \
  src/common/jumpback.cpp \
  src/common/mapcolors.cpp \
  src/common/mapflags.cpp \
  src/common/mapresult.cpp \
  src/common/maptools.cpp \
  src/common/maptypes.cpp \
  src/common/maptypesfactory.cpp \
  src/common/procflags.cpp \
  src/common/proctypes.cpp \
  src/common/settingsmigrate.cpp \
  src/common/symbolpainter.cpp \
  src/common/tabindexes.cpp \
  src/common/textplacement.cpp \
  src/common/unit.cpp \
  src/common/unitstringtool.cpp \
  src/common/updatehandler.cpp \
  src/common/vehicleicons.cpp \
  src/connect/connectclient.cpp \
  src/connect/connectdialog.cpp \
  src/db/airspacedialog.cpp \
  src/db/databasedialog.cpp \
  src/db/databaseloader.cpp \
  src/db/databasemanager.cpp \
  src/db/databaseprogressdialog.cpp \
  src/db/dbtools.cpp \
  src/db/dbtypes.cpp \
  src/db/undoredoprogress.cpp \
  src/export/csvexporter.cpp \
  src/export/exporter.cpp \
  src/gui/holddialog.cpp \
  src/gui/coordinatedialog.cpp \
  src/gui/mainwindow.cpp \
  src/gui/messagesettings.cpp \
  src/gui/runwayselection.cpp \
  src/gui/statusbareventfilter.cpp \
  src/gui/stylehandler.cpp \
  src/gui/textdialog.cpp \
  src/gui/texteditdialog.cpp \
  src/gui/timedialog.cpp \
  src/gui/trafficpatterndialog.cpp \
  src/gui/rangemarkerdialog.cpp \
  src/gui/updatedialog.cpp \
  src/info/aircraftprogressconfig.cpp \
  src/info/infocontroller.cpp \
  src/logbook/logdatacontroller.cpp \
  src/logbook/logdataconverter.cpp \
  src/logbook/logdatadialog.cpp \
  src/logbook/logstatisticsdialog.cpp \
  src/main.cpp \
  src/mapgui/aprongeometrycache.cpp \
  src/mapgui/imageexportdialog.cpp \
  src/mapgui/mapairporthandler.cpp \
  src/mapgui/mapcontextmenu.cpp \
  src/mapgui/mapdetailhandler.cpp \
  src/mapgui/mapfunctions.cpp \
  src/mapgui/maplayer.cpp \
  src/mapgui/maplayersettings.cpp \
  src/mapgui/mapmarkhandler.cpp \
  src/mapgui/mappaintwidget.cpp \
  src/mapgui/mapscale.cpp \
  src/mapgui/mapscreenindex.cpp \
  src/mapgui/mapthemehandler.cpp \
  src/mapgui/maptooltip.cpp \
  src/mapgui/mapvisible.cpp \
  src/mapgui/mapwidget.cpp \
  src/mappainter/mappainter.cpp \
  src/mappainter/mappainteraircraft.cpp \
  src/mappainter/mappainterairport.cpp \
  src/mappainter/mappainterairspace.cpp \
  src/mappainter/mappainteraltitude.cpp \
  src/mappainter/mappainterils.cpp \
  src/mappainter/mappaintermark.cpp \
  src/mappainter/mappaintermsa.cpp \
  src/mappainter/mappainternav.cpp \
  src/mappainter/mappainterroute.cpp \
  src/mappainter/mappaintership.cpp \
  src/mappainter/mappaintertop.cpp \
  src/mappainter/mappaintertrail.cpp \
  src/mappainter/mappainteruser.cpp \
  src/mappainter/mappaintervehicle.cpp \
  src/mappainter/mappainterweather.cpp \
  src/mappainter/mappainterwind.cpp \
  src/mappainter/mappaintlayer.cpp \
  src/online/onlinedatacontroller.cpp \
  src/options/optiondata.cpp \
  src/options/optionsdialog.cpp \
  src/perf/aircraftperfcontroller.cpp \
  src/perf/aircraftperfdialog.cpp \
  src/perf/perfmergedialog.cpp \
  src/print/printdialog.cpp \
  src/print/printsupport.cpp \
  src/profile/profilelabelwidgethoriz.cpp \
  src/profile/profilelabelwidgetvert.cpp \
  src/profile/profileoptions.cpp \
  src/profile/profilescrollarea.cpp \
  src/profile/profilewidget.cpp \
  src/query/airportquery.cpp \
  src/query/airspacequery.cpp \
  src/query/airwayquery.cpp \
  src/query/airwaytrackquery.cpp \
  src/query/infoquery.cpp \
  src/query/mapquery.cpp \
  src/query/procedurequery.cpp \
  src/query/querytypes.cpp \
  src/query/waypointquery.cpp \
  src/query/waypointtrackquery.cpp \
  src/route/customproceduredialog.cpp \
  src/route/flightplanentrybuilder.cpp \
  src/route/parkingdialog.cpp \
  src/route/route.cpp \
  src/route/routealtitude.cpp \
  src/route/routealtitudeleg.cpp \
  src/route/routecalcdialog.cpp \
  src/route/routecommand.cpp \
  src/route/routecontroller.cpp \
  src/route/routeextractor.cpp \
  src/route/routeflags.cpp \
  src/route/routelabel.cpp \
  src/route/routeleg.cpp \
  src/route/runwayselectiondialog.cpp \
  src/route/userwaypointdialog.cpp \
  src/routeexport/fetchroutedialog.cpp \
  src/routeexport/routeexport.cpp \
  src/routeexport/routeexportdata.cpp \
  src/routeexport/routeexportdialog.cpp \
  src/routeexport/routeexportflags.cpp \
  src/routeexport/routeexportformat.cpp \
  src/routeexport/routemultiexportdialog.cpp \
  src/routeexport/simbriefhandler.cpp \
  src/routestring/routestringdialog.cpp \
  src/routestring/routestringreader.cpp \
  src/routestring/routestringtypes.cpp \
  src/routestring/routestringwriter.cpp \
  src/search/abstractsearch.cpp \
  src/search/airporticondelegate.cpp \
  src/search/airportsearch.cpp \
  src/search/column.cpp \
  src/search/columnlist.cpp \
  src/search/logdatasearch.cpp \
  src/search/navicondelegate.cpp \
  src/search/navsearch.cpp \
  src/search/onlinecentersearch.cpp \
  src/search/onlineclientsearch.cpp \
  src/search/onlineserversearch.cpp \
  src/search/proceduresearch.cpp \
  src/search/querybuilder.cpp \
  src/search/randomdepartureairportpickingbycriteria.cpp \
  src/search/randomdestinationairportpickingbycriteria.cpp \
  src/search/searchbasetable.cpp \
  src/search/searchcontroller.cpp \
  src/search/sqlcontroller.cpp \
  src/search/sqlmodel.cpp \
  src/search/sqlmodeltypes.cpp \
  src/search/sqlproxymodel.cpp \
  src/search/userdatasearch.cpp \
  src/search/usericondelegate.cpp \
  src/track/trackcontroller.cpp \
  src/track/trackmanager.cpp \
  src/userdata/userdatacontroller.cpp \
  src/userdata/userdatadialog.cpp \
  src/userdata/userdataicons.cpp \
  src/weather/weathercontext.cpp \
  src/weather/weathercontexthandler.cpp \
  src/weather/weatherreporter.cpp \
  src/weather/windreporter.cpp \
  src/web/requesthandler.cpp \
  src/web/webapp.cpp \
  src/web/webcontroller.cpp \
  src/web/webflags.cpp \
  src/web/webmapcontroller.cpp \
  src/web/webtools.cpp \
  src/webapi/abstractactionscontroller.cpp \
  src/webapi/abstractlnmactionscontroller.cpp \
  src/webapi/actionscontrollerindex.cpp \
  src/webapi/airportactionscontroller.cpp \
  src/webapi/mapactionscontroller.cpp \
  src/webapi/simactionscontroller.cpp \
  src/webapi/uiactionscontroller.cpp \
  src/webapi/webapicontroller.cpp

HEADERS  += \
  src/airspace/airspacecontroller.h \
  src/airspace/airspacetoolbarhandler.h \
  src/app/commandline.h \
  src/app/dataexchange.h \
  src/app/navapp.h \
  src/common/abstractinfobuilder.h \
  src/common/aircrafttrail.h \
  src/common/airportfiles.h \
  src/common/constants.h \
  src/common/coordinateconverter.h \
  src/common/dialogrecordhelper.h \
  src/common/dirtool.h \
  src/common/elevationprovider.h \
  src/common/filecheck.h \
  src/common/formatter.h \
  src/common/fueltool.h \
  src/common/htmlinfobuilder.h \
  src/common/htmlinfobuilderflags.h \
  src/common/infobuildertypes.h \
  src/common/jsoninfobuilder.h \
  src/common/jumpback.h \
  src/common/mapcolors.h \
  src/common/mapflags.h \
  src/common/mapresult.h \
  src/common/maptools.h \
  src/common/maptypes.h \
  src/common/maptypesfactory.h \
  src/common/procflags.h \
  src/common/proctypes.h \
  src/common/settingsmigrate.h \
  src/common/symbolpainter.h \
  src/common/tabindexes.h \
  src/common/textplacement.h \
  src/common/unit.h \
  src/common/unitstringtool.h \
  src/common/updatehandler.h \
  src/common/vehicleicons.h \
  src/connect/connectclient.h \
  src/connect/connectdialog.h \
  src/db/airspacedialog.h \
  src/db/databasedialog.h \
  src/db/databaseloader.h \
  src/db/databasemanager.h \
  src/db/databaseprogressdialog.h \
  src/db/dbtools.h \
  src/db/dbtypes.h \
  src/db/undoredoprogress.h \
  src/export/csvexporter.h \
  src/export/exporter.h \
  src/gui/holddialog.h \
  src/gui/coordinatedialog.h \
  src/gui/mainwindow.h \
  src/gui/messagesettings.h \
  src/gui/runwayselection.h \
  src/gui/statusbareventfilter.h \
  src/gui/stylehandler.h \
  src/gui/textdialog.h \
  src/gui/texteditdialog.h \
  src/gui/timedialog.h \
  src/gui/trafficpatterndialog.h \
  src/gui/rangemarkerdialog.h \
  src/gui/updatedialog.h \
  src/info/aircraftprogressconfig.h \
  src/info/infocontroller.h \
  src/logbook/logdatacontroller.h \
  src/logbook/logdataconverter.h \
  src/logbook/logdatadialog.h \
  src/logbook/logstatisticsdialog.h \
  src/mapgui/aprongeometrycache.h \
  src/mapgui/imageexportdialog.h \
  src/mapgui/mapairporthandler.h \
  src/mapgui/mapcontextmenu.h \
  src/mapgui/mapdetailhandler.h \
  src/mapgui/mapfunctions.h \
  src/mapgui/maplayer.h \
  src/mapgui/maplayersettings.h \
  src/mapgui/mapmarkhandler.h \
  src/mapgui/mappaintwidget.h \
  src/mapgui/mapscale.h \
  src/mapgui/mapscreenindex.h \
  src/mapgui/mapthemehandler.h \
  src/mapgui/maptooltip.h \
  src/mapgui/mapvisible.h \
  src/mapgui/mapwidget.h \
  src/mappainter/mappainter.h \
  src/mappainter/mappainteraircraft.h \
  src/mappainter/mappainterairport.h \
  src/mappainter/mappainterairspace.h \
  src/mappainter/mappainteraltitude.h \
  src/mappainter/mappainterils.h \
  src/mappainter/mappaintermark.h \
  src/mappainter/mappaintermsa.h \
  src/mappainter/mappainternav.h \
  src/mappainter/mappainterroute.h \
  src/mappainter/mappaintership.h \
  src/mappainter/mappaintertop.h \
  src/mappainter/mappaintertrail.h \
  src/mappainter/mappainteruser.h \
  src/mappainter/mappaintervehicle.h \
  src/mappainter/mappainterweather.h \
  src/mappainter/mappainterwind.h \
  src/mappainter/mappaintlayer.h \
  src/online/onlinedatacontroller.h \
  src/options/optiondata.h \
  src/options/optionsdialog.h \
  src/perf/aircraftperfcontroller.h \
  src/perf/aircraftperfdialog.h \
  src/perf/perfmergedialog.h \
  src/print/printdialog.h \
  src/print/printsupport.h \
  src/profile/profilelabelwidgethoriz.h \
  src/profile/profilelabelwidgetvert.h \
  src/profile/profileoptions.h \
  src/profile/profilescrollarea.h \
  src/profile/profilewidget.h \
  src/query/airportquery.h \
  src/query/airspacequery.h \
  src/query/airwayquery.h \
  src/query/airwaytrackquery.h \
  src/query/infoquery.h \
  src/query/mapquery.h \
  src/query/procedurequery.h \
  src/query/querytypes.h \
  src/query/waypointquery.h \
  src/query/waypointtrackquery.h \
  src/route/customproceduredialog.h \
  src/route/flightplanentrybuilder.h \
  src/route/parkingdialog.h \
  src/route/route.h \
  src/route/routealtitude.h \
  src/route/routealtitudeleg.h \
  src/route/routecalcdialog.h \
  src/route/routecommand.h \
  src/route/routecommandflags.h \
  src/route/routecontroller.h \
  src/route/routeextractor.h \
  src/route/routeflags.h \
  src/route/routelabel.h \
  src/route/routeleg.h \
  src/route/runwayselectiondialog.h \
  src/route/userwaypointdialog.h \
  src/routeexport/fetchroutedialog.h \
  src/routeexport/routeexport.h \
  src/routeexport/routeexportdata.h \
  src/routeexport/routeexportdialog.h \
  src/routeexport/routeexportflags.h \
  src/routeexport/routeexportformat.h \
  src/routeexport/routemultiexportdialog.h \
  src/routeexport/simbriefhandler.h \
  src/routestring/routestringdialog.h \
  src/routestring/routestringreader.h \
  src/routestring/routestringtypes.h \
  src/routestring/routestringwriter.h \
  src/search/abstractsearch.h \
  src/search/airporticondelegate.h \
  src/search/airportsearch.h \
  src/search/column.h \
  src/search/columnlist.h \
  src/search/logdatasearch.h \
  src/search/navicondelegate.h \
  src/search/navsearch.h \
  src/search/onlinecentersearch.h \
  src/search/onlineclientsearch.h \
  src/search/onlineserversearch.h \
  src/search/proceduresearch.h \
  src/search/querybuilder.h \
  src/search/randomdepartureairportpickingbycriteria.h \
  src/search/randomdestinationairportpickingbycriteria.h \
  src/search/searchbasetable.h \
  src/search/searchcontroller.h \
  src/search/sqlcontroller.h \
  src/search/sqlmodel.h \
  src/search/sqlmodeltypes.h \
  src/search/sqlproxymodel.h \
  src/search/userdatasearch.h \
  src/search/usericondelegate.h \
  src/track/trackcontroller.h \
  src/track/trackmanager.h \
  src/userdata/userdatacontroller.h \
  src/userdata/userdatadialog.h \
  src/userdata/userdataicons.h \
  src/weather/weathercontext.h \
  src/weather/weathercontexthandler.h \
  src/weather/weatherreporter.h \
  src/weather/windreporter.h \
  src/web/requesthandler.h \
  src/web/webapp.h \
  src/web/webcontroller.h \
  src/web/webflags.h \
  src/web/webmapcontroller.h \
  src/web/webtools.h \
  src/webapi/abstractactionscontroller.h \
  src/webapi/abstractlnmactionscontroller.h \
  src/webapi/actionscontrollerindex.h \
  src/webapi/airportactionscontroller.h \
  src/webapi/mapactionscontroller.h \
  src/webapi/simactionscontroller.h \
  src/webapi/uiactionscontroller.h \
  src/webapi/webapicontroller.h \
  src/webapi/webapirequest.h \
  src/webapi/webapiresponse.h

FORMS += \
  src/connect/connectdialog.ui \
  src/db/airspacedialog.ui \
  src/db/databasedialog.ui \
  src/db/databaseprogressdialog.ui \
  src/gui/coordinatedialog.ui \
  src/gui/holddialog.ui \
  src/gui/mainwindow.ui \
  src/gui/rangemarkerdialog.ui \
  src/gui/textdialog.ui \
  src/gui/texteditdialog.ui \
  src/gui/timedialog.ui \
  src/gui/trafficpatterndialog.ui \
  src/gui/updatedialog.ui \
  src/logbook/logdatadialog.ui \
  src/logbook/logstatisticsdialog.ui \
  src/mapgui/imageexportdialog.ui \
  src/options/options.ui \
  src/perf/aircraftperfdialog.ui \
  src/perf/perfmergedialog.ui \
  src/print/printdialog.ui \
  src/route/customproceduredialog.ui \
  src/route/parkingdialog.ui \
  src/route/routecalcdialog.ui \
  src/route/runwayselectiondialog.ui \
  src/route/userwaypointdialog.ui \
  src/routeexport/fetchroutedialog.ui \
  src/routeexport/routeexportdialog.ui \
  src/routeexport/routemultiexportdialog.ui \
  src/routestring/routestringdialog.ui \
  src/userdata/userdatadialog.ui

RESOURCES += littlenavmap.qrc

# =====================================================================
# Add API key resource only if it exists
exists($$PWD/../little_navmap_keys) {
  RESOURCES += keys.qrc
}

ICON = resources/icons/littlenavmap.icns

TRANSLATIONS = littlenavmap_fr.ts \
               littlenavmap_it.ts \
               littlenavmap_nl.ts \
               littlenavmap_de.ts \
               littlenavmap_zh.ts \
               littlenavmap_pt_BR.ts

# littlenavmap_es.ts

OTHER_FILES += \
  $$files(build/*, true) \
  $$files(customize/*, true) \
  $$files(desktop/*, true) \
  $$files(etc/*, true) \
  $$files(help/*, true) \
  $$files(web/*, true) \
  $$files(marble/*, true) \
  $$files(.github/*, true) \
  $$files(simconnect/*, true) \
  $$files(etc/*, true) \
  .travis.yml \
  .gitignore \
  *.ts \
  BUILD.txt \
  CHANGELOG.txt \
  LICENSE.txt \
  README.txt \
  README.md \
  htmltidy.cfg \
  uncrustify.cfg

# =====================================================================
# Local deployment commands for development

# Linux - Copy help and Marble plugins and data
unix:!macx {
  copydata.commands = mkdir -p $$OUT_PWD/plugins &&
  copydata.commands += cp -avfu \
    $$MARBLE_LIB_PATH/marble/plugins/libCachePlugin.so \
    $$MARBLE_LIB_PATH/marble/plugins/libAtmospherePlugin.so \
    $$MARBLE_LIB_PATH/marble/plugins/libCompassFloatItem.so \
    $$MARBLE_LIB_PATH/marble/plugins/libGraticulePlugin.so \
    $$MARBLE_LIB_PATH/marble/plugins/libKmlPlugin.so \
    $$MARBLE_LIB_PATH/marble/plugins/libLatLonPlugin.so \
    $$MARBLE_LIB_PATH/marble/plugins/libPn2Plugin.so \
    $$MARBLE_LIB_PATH/marble/plugins/libMapScaleFloatItem.so \
    $$MARBLE_LIB_PATH/marble/plugins/libNavigationFloatItem.so \
    $$MARBLE_LIB_PATH/marble/plugins/libOverviewMap.so \
    $$OUT_PWD/plugins &&
  copydata.commands += mkdir -p $$OUT_PWD/translations &&
  copydata.commands += cp -avfu $$PWD/*.qm $$OUT_PWD/translations &&
  copydata.commands += cp -avfu $$ATOOLS_INC_PATH/../*.qm $$OUT_PWD/translations &&
  copydata.commands += cp -avfu $$PWD/help $$OUT_PWD &&
  copydata.commands += cp -avfu $$PWD/web $$OUT_PWD &&
  copydata.commands += cp -avfu $$PWD/customize $$OUT_PWD &&
  copydata.commands += cp -avfu $$PWD/marble/data $$OUT_PWD &&
  copydata.commands += cp -vf $$PWD/desktop/littlenavmap*.sh $$OUT_PWD &&
  copydata.commands += chmod -v a+x $$OUT_PWD/littlenavmap*.sh
}

# Mac OS X - Copy help and Marble plugins and data
macx {
  copydata.commands += cp -Rv $$PWD/help $$OUT_PWD/littlenavmap.app/Contents/MacOS &&
  copydata.commands += cp -Rv $$PWD/web $$OUT_PWD/littlenavmap.app/Contents/MacOS &&
  copydata.commands += cp -Rv $$PWD/customize $$OUT_PWD/littlenavmap.app/Contents/MacOS &&
  copydata.commands += cp -Rv $$PWD/marble/data $$OUT_PWD/littlenavmap.app/Contents/MacOS &&
  copydata.commands += cp -vf $$PWD/*.qm $$OUT_PWD/littlenavmap.app/Contents/MacOS &&
  copydata.commands += cp -vf $$ATOOLS_INC_PATH/../*.qm $$OUT_PWD/littlenavmap.app/Contents/MacOS
}

# =====================================================================
# Deployment commands

# Linux specific deploy target
unix:!macx {
  DEPLOY_DIR=\"$$DEPLOY_BASE/$$TARGET_NAME\"
  DEPLOY_DIR_LIB=\"$$DEPLOY_BASE/$$TARGET_NAME/lib\"

  deploy.commands += rm -Rfv $$DEPLOY_DIR &&
  deploy.commands += mkdir -pv $$DEPLOY_DIR_LIB &&
  deploy.commands += mkdir -pv $$DEPLOY_DIR_LIB/iconengines &&
  deploy.commands += mkdir -pv $$DEPLOY_DIR_LIB/imageformats &&
  deploy.commands += mkdir -pv $$DEPLOY_DIR_LIB/platforms &&
  deploy.commands += mkdir -pv $$DEPLOY_DIR_LIB/platformthemes &&
  deploy.commands += mkdir -pv $$DEPLOY_DIR_LIB/printsupport &&
  deploy.commands += mkdir -pv $$DEPLOY_DIR_LIB/sqldrivers &&
  deploy.commands += echo $$VERSION_NUMBER > $$DEPLOY_DIR/version.txt &&
  deploy.commands += echo $$GIT_REVISION_FULL > $$DEPLOY_DIR/revision.txt &&
  deploy.commands += cp -Rvf $$MARBLE_LIB_PATH/*.so* $$DEPLOY_DIR_LIB &&
  deploy.commands += patchelf --set-rpath \'\$\$ORIGIN/.\' $$DEPLOY_DIR_LIB/libmarblewidget-qt5.so* &&
  deploy.commands += patchelf --set-rpath \'\$\$ORIGIN/.\' $$DEPLOY_DIR_LIB/libastro.so* &&
  deploy.commands += cp -Rvf $$OUT_PWD/plugins $$DEPLOY_DIR &&
  deploy.commands += cp -Rvf $$OUT_PWD/data $$DEPLOY_DIR &&
  deploy.commands += cp -Rvf $$OUT_PWD/help $$DEPLOY_DIR &&
  deploy.commands += cp -Rvf $$OUT_PWD/web $$DEPLOY_DIR &&
  deploy.commands += cp -Rvf $$OUT_PWD/customize $$DEPLOY_DIR &&
  deploy.commands += cp -Rvf $$OUT_PWD/translations $$DEPLOY_DIR &&
  deploy.commands += cp -Rvf $$OUT_PWD/littlenavmap $$DEPLOY_DIR &&
  deploy.commands += cp -vfa $$[QT_INSTALL_TRANSLATIONS]/qt_??.qm  $$DEPLOY_DIR/translations &&
  deploy.commands += cp -vfa $$[QT_INSTALL_TRANSLATIONS]/qt_??_??.qm  $$DEPLOY_DIR/translations &&
  deploy.commands += cp -vfa $$[QT_INSTALL_TRANSLATIONS]/qtbase*.qm  $$DEPLOY_DIR/translations &&
  exists($$DATABASE_BASE) : deploy.commands += cp -Rvf $$DATABASE_BASE $$DEPLOY_DIR &&
  exists($$HELP_BASE) : deploy.commands += cp -Rvf $$HELP_BASE/* $$DEPLOY_DIR/help &&
  deploy.commands += cp -vf $$PWD/desktop/\"Little Navmap Portable Linux.sh\" $$DEPLOY_DIR/\"Little Navmap Portable.sh\" &&
  deploy.commands += cp -vf $$PWD/desktop/linux-qt.conf $$DEPLOY_DIR/qt.conf &&
  deploy.commands += cp -vf $$PWD/CHANGELOG.txt $$DEPLOY_DIR &&
  deploy.commands += cp -vf $$PWD/README.txt $$DEPLOY_DIR &&
  deploy.commands += cp -vf $$PWD/LICENSE.txt $$DEPLOY_DIR &&
  deploy.commands += cp -vf $$PWD/resources/icons/littlenavmap.svg $$DEPLOY_DIR &&
  deploy.commands += cp -vf \"$$PWD/desktop/Little Navmap.desktop\" $$DEPLOY_DIR &&
  exists(/usr/lib/x86_64-linux-gnu/libssl.so.1.1) : deploy.commands += cp -vfaL /usr/lib/x86_64-linux-gnu/libssl.so.1.1 $$DEPLOY_DIR_LIB &&
  exists(/usr/lib/x86_64-linux-gnu/libcrypto.so.1.1) : deploy.commands += cp -vfaL /usr/lib/x86_64-linux-gnu/libcrypto.so.1.1 $$DEPLOY_DIR_LIB &&
  deploy.commands += cp -vfa $$[QT_INSTALL_PLUGINS]/iconengines/libqsvgicon.so*  $$DEPLOY_DIR_LIB/iconengines &&
  deploy.commands += cp -vfa $$[QT_INSTALL_PLUGINS]/imageformats/libqgif.so*  $$DEPLOY_DIR_LIB/imageformats &&
  deploy.commands += cp -vfa $$[QT_INSTALL_PLUGINS]/imageformats/libqjpeg.so*  $$DEPLOY_DIR_LIB/imageformats &&
  deploy.commands += cp -vfa $$[QT_INSTALL_PLUGINS]/imageformats/libqsvg.so*  $$DEPLOY_DIR_LIB/imageformats &&
  deploy.commands += cp -vfa $$[QT_INSTALL_PLUGINS]/imageformats/libqwbmp.so*  $$DEPLOY_DIR_LIB/imageformats &&
  deploy.commands += cp -vfa $$[QT_INSTALL_PLUGINS]/imageformats/libqwebp.so*  $$DEPLOY_DIR_LIB/imageformats &&
  deploy.commands += cp -vfa $$[QT_INSTALL_PLUGINS]/platforms/libqeglfs.so*  $$DEPLOY_DIR_LIB/platforms &&
  deploy.commands += cp -vfa $$[QT_INSTALL_PLUGINS]/platforms/libqlinuxfb.so*  $$DEPLOY_DIR_LIB/platforms &&
  deploy.commands += cp -vfa $$[QT_INSTALL_PLUGINS]/platforms/libqminimal.so*  $$DEPLOY_DIR_LIB/platforms &&
  deploy.commands += cp -vfa $$[QT_INSTALL_PLUGINS]/platforms/libqminimalegl.so*  $$DEPLOY_DIR_LIB/platforms &&
  deploy.commands += cp -vfa $$[QT_INSTALL_PLUGINS]/platforms/libqoffscreen.so*  $$DEPLOY_DIR_LIB/platforms &&
  deploy.commands += cp -vfa $$[QT_INSTALL_PLUGINS]/platforms/libqxcb.so*  $$DEPLOY_DIR_LIB/platforms &&
  deploy.commands += cp -vfa $$[QT_INSTALL_PLUGINS]/platformthemes/libqgtk*.so*  $$DEPLOY_DIR_LIB/platformthemes &&
  deploy.commands += cp -vfa $$[QT_INSTALL_PLUGINS]/printsupport/libcupsprintersupport.so*  $$DEPLOY_DIR_LIB/printsupport &&
  deploy.commands += cp -vfa $$[QT_INSTALL_PLUGINS]/sqldrivers/libqsqlite.so*  $$DEPLOY_DIR_LIB/sqldrivers &&
  deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libicudata.so*  $$DEPLOY_DIR_LIB &&
  deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libicui18n.so*  $$DEPLOY_DIR_LIB &&
  deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libicuuc.so*  $$DEPLOY_DIR_LIB &&
  deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libQt5Concurrent.so*  $$DEPLOY_DIR_LIB &&
  deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libQt5Core.so*  $$DEPLOY_DIR_LIB &&
  deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libQt5DBus.so*  $$DEPLOY_DIR_LIB &&
  deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libQt5Gui.so*  $$DEPLOY_DIR_LIB &&
  deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libQt5Network.so*  $$DEPLOY_DIR_LIB &&
  deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libQt5PrintSupport.so*  $$DEPLOY_DIR_LIB &&
  deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libQt5Qml.so*  $$DEPLOY_DIR_LIB &&
  deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libQt5Quick.so*  $$DEPLOY_DIR_LIB &&
  deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libQt5Sql.so*  $$DEPLOY_DIR_LIB &&
  deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libQt5Svg.so*  $$DEPLOY_DIR_LIB &&
  deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libQt5Widgets.so*  $$DEPLOY_DIR_LIB &&
  deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libQt5X11Extras.so*  $$DEPLOY_DIR_LIB &&
  deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libQt5XcbQpa.so*  $$DEPLOY_DIR_LIB &&
  deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libQt5QmlModels.so*  $$DEPLOY_DIR_LIB &&
  deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libQt5Xml.so* $$DEPLOY_DIR_LIB
}

# Mac specific deploy target
macx {

  INSTALL_MARBLE_DYLIB_CMD=install_name_tool \
         -change  $$INSTALL_MARBLE_DYLIB \
          @executable_path/../Frameworks/libmarblewidget-qt5.25.dylib $$OUT_PWD/littlenavmap.app/Contents/PlugIns

#  INSTALL_MARBLE_DYLIB_CMD=install_name_tool \
#         -change $$clean_path($$MARBLE_BUILD_PATH/src/lib/marble/libmarblewidget-qt5.25.dylib) \
#          @executable_path/../Frameworks/libmarblewidget-qt5.25.dylib $$OUT_PWD/littlenavmap.app/Contents/PlugIns

  DEPLOY_APP=\"$$PWD/../deploy/Little Navmap.app\"
  DEPLOY_DIR=\"$$PWD/../deploy\"

  message(INSTALL_MARBLE_DYLIB_CMD: $$INSTALL_MARBLE_DYLIB_CMD)

  deploy.commands = rm -Rfv $$DEPLOY_APP &&
  deploy.commands += mkdir -p $$OUT_PWD/littlenavmap.app/Contents/PlugIns &&
  exists($$DATABASE_BASE) : deploy.commands += cp -Rvf $$DATABASE_BASE $$OUT_PWD/littlenavmap.app/Contents/MacOS &&
  exists($$HELP_BASE) : deploy.commands += cp -Rvf $$HELP_BASE/* $$OUT_PWD/littlenavmap.app/Contents/MacOS/help &&
  deploy.commands += cp -Rvf \
    $$MARBLE_LIB_PATH/plugins/libCachePlugin.so \
    $$MARBLE_LIB_PATH/plugins/libAtmospherePlugin.so \
    $$MARBLE_LIB_PATH/plugins/libCompassFloatItem.so \
    $$MARBLE_LIB_PATH/plugins/libGraticulePlugin.so \
    $$MARBLE_LIB_PATH/plugins/libKmlPlugin.so \
    $$MARBLE_LIB_PATH/plugins/libLatLonPlugin.so \
    $$MARBLE_LIB_PATH/plugins/libPn2Plugin.so \
    $$MARBLE_LIB_PATH/plugins/libMapScaleFloatItem.so \
    $$MARBLE_LIB_PATH/plugins/libNavigationFloatItem.so \
    $$MARBLE_LIB_PATH/plugins/libOverviewMap.so \
    $$OUT_PWD/littlenavmap.app/Contents/PlugIns &&
  deploy.commands +=  $$INSTALL_MARBLE_DYLIB_CMD/libCachePlugin.so &&
  deploy.commands +=  $$INSTALL_MARBLE_DYLIB_CMD/libAtmospherePlugin.so &&
  deploy.commands +=  $$INSTALL_MARBLE_DYLIB_CMD/libCompassFloatItem.so &&
  deploy.commands +=  $$INSTALL_MARBLE_DYLIB_CMD/libGraticulePlugin.so &&
  deploy.commands +=  $$INSTALL_MARBLE_DYLIB_CMD/libKmlPlugin.so &&
  deploy.commands +=  $$INSTALL_MARBLE_DYLIB_CMD/libLatLonPlugin.so &&
  deploy.commands +=  $$INSTALL_MARBLE_DYLIB_CMD/libPn2Plugin.so &&
  deploy.commands +=  $$INSTALL_MARBLE_DYLIB_CMD/libMapScaleFloatItem.so &&
  deploy.commands +=  $$INSTALL_MARBLE_DYLIB_CMD/libNavigationFloatItem.so &&
  deploy.commands +=  $$INSTALL_MARBLE_DYLIB_CMD/libOverviewMap.so &&
  deploy.commands += $$[QT_INSTALL_BINS]/macdeployqt littlenavmap.app $$MACDEPLOY_FLAGS &&
  deploy.commands += cp -vf $$PWD/desktop/\"Little Navmap Portable macOS.command\" $$DEPLOY_DIR/\"Little Navmap Portable.command\" &&
  deploy.commands += cp -rfv $$OUT_PWD/littlenavmap.app $$DEPLOY_APP &&
  deploy.commands += cp -fv $$[QT_INSTALL_TRANSLATIONS]/qt_??.qm  $$DEPLOY_APP/Contents/MacOS &&
  deploy.commands += cp -fv $$[QT_INSTALL_TRANSLATIONS]/qt_??_??.qm  $$DEPLOY_APP/Contents/MacOS &&
  deploy.commands += cp -fv $$[QT_INSTALL_TRANSLATIONS]/qtbase*.qm  $$DEPLOY_APP/Contents/MacOS &&
  deploy.commands += cp -fv $$PWD/build/mac/Info.plist $$ $$DEPLOY_APP/Contents &&
  deploy.commands += echo $$VERSION_NUMBER > $$DEPLOY_DIR/version-LittleNavmap.txt &&
  deploy.commands += echo $$GIT_REVISION_FULL > $$DEPLOY_DIR/revision-LittleNavmap.txt &&
  deploy.commands += cp -fv $$PWD/LICENSE.txt $$DEPLOY_DIR &&
  deploy.commands += cp -fv $$PWD/README.txt $$DEPLOY_DIR/README-LittleNavmap.txt &&
  deploy.commands += cp -fv $$PWD/CHANGELOG.txt $$DEPLOY_DIR/CHANGELOG-LittleNavmap.txt


# -verbose=3
}

# Windows specific deploy target
win32 {
  defineReplace(p){return ($$shell_quote($$shell_path($$1)))}
  RC_ICONS = resources/icons/littlenavmap.ico
  RC_ICONS += resources/icons/littlenavmapdoc.ico

  CONFIG(debug, debug|release) : DLL_SUFFIX=d
  CONFIG(release, debug|release) : DLL_SUFFIX=

  WIN_TARGET_NAME="$$TARGET_NAME $$WINARCH"

  deploy.commands = rmdir /s /q $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME) &
  deploy.commands += mkdir $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME/translations) &&
  deploy.commands += mkdir $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME/plugins) &&
  deploy.commands += echo $$WINARCH-$$VERSION_NUMBER > $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME/version.txt) &&
  deploy.commands += echo $$GIT_REVISION_FULL > $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME/revision.txt) &&
  deploy.commands += xcopy $$p($$MARBLE_LIB_PATH/../plugins/libCachePlugin$${DLL_SUFFIX}.dll) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME/plugins) &&
  deploy.commands += xcopy $$p($$MARBLE_LIB_PATH/../plugins/libAtmospherePlugin$${DLL_SUFFIX}.dll) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME/plugins) &&
  deploy.commands += xcopy $$p($$MARBLE_LIB_PATH/../plugins/libCompassFloatItem$${DLL_SUFFIX}.dll) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME/plugins) &&
  deploy.commands += xcopy $$p($$MARBLE_LIB_PATH/../plugins/libGraticulePlugin$${DLL_SUFFIX}.dll) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME/plugins) &&
  deploy.commands += xcopy $$p($$MARBLE_LIB_PATH/../plugins/libKmlPlugin$${DLL_SUFFIX}.dll) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME/plugins) &&
  deploy.commands += xcopy $$p($$MARBLE_LIB_PATH/../plugins/libLatLonPlugin$${DLL_SUFFIX}.dll) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME/plugins) &&
  deploy.commands += xcopy $$p($$MARBLE_LIB_PATH/../plugins/libPn2Plugin$${DLL_SUFFIX}.dll) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME/plugins) &&
  deploy.commands += xcopy $$p($$MARBLE_LIB_PATH/../plugins/libMapScaleFloatItem$${DLL_SUFFIX}.dll) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME/plugins) &&
  deploy.commands += xcopy $$p($$MARBLE_LIB_PATH/../plugins/libNavigationFloatItem$${DLL_SUFFIX}.dll) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME/plugins) &&
  deploy.commands += xcopy $$p($$MARBLE_LIB_PATH/../plugins/libOverviewMap$${DLL_SUFFIX}.dll) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME/plugins) &&
  deploy.commands += xcopy $$p($$OUT_PWD/littlenavmap.exe) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME) &&
  deploy.commands += xcopy $$p($$PWD/CHANGELOG.txt) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME) &&
  deploy.commands += xcopy $$p($$PWD/README.txt) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME) &&
  deploy.commands += xcopy $$p($$PWD/LICENSE.txt) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME) &&
  deploy.commands += copy $$p($$PWD/desktop/Little Navmap Portable Win.cmd) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME/Little Navmap Portable.cmd) &&
  deploy.commands += copy $$p($$PWD/desktop/win-qt.conf) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME/qt.conf) &&
  deploy.commands += xcopy $$p($$PWD/*.qm) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME/translations) &&
  deploy.commands += xcopy $$p($$ATOOLS_INC_PATH/../*.qm) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME/translations) &&
  exists($$DATABASE_BASE) : deploy.commands += xcopy /i /s /e /f /y $$p($$DATABASE_BASE) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME/little_navmap_db) &&
  exists($$HELP_BASE) : deploy.commands += xcopy /i /s /e /f /y $$p($$HELP_BASE) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME/help) &&
  deploy.commands += xcopy /i /s /e /f /y $$p($$PWD/help/*) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME/help/) &&
  deploy.commands += xcopy /i /s /e /f /y $$p($$PWD/web) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME/web) &&
  deploy.commands += xcopy /i /s /e /f /y $$p($$PWD/customize) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME/customize) &&
  deploy.commands += xcopy /i /s /e /f /y $$p($$PWD/marble/data) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME/data) &&
  deploy.commands += xcopy /i /s /e /f /y $$p($$PWD/etc) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME/etc) &&
  deploy.commands += xcopy /i /s /e /f /y $$p($$PWD/simconnect) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME/simconnect) &&
  contains(QT_ARCH, i386) { # 32 Bit build
    deploy.commands += move /Y $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME/simconnect/SimConnect.dll) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME) &&
  } else { # 64 Bit build
    deploy.commands += del /f /q $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME/simconnect/SimConnect.dll) &&
    deploy.commands += del /f /q $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME/simconnect/simconnect.manifest) &&
    deploy.commands += xcopy $$p($$SIMCONNECT_PATH_WIN64/lib/SimConnect.dll) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME) &&
  }
  deploy.commands += xcopy $$p($$MARBLE_LIB_PATH/../libmarblewidget-qt5$${DLL_SUFFIX}.dll) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME) &&
  deploy.commands += xcopy $$p($$MARBLE_LIB_PATH/../libastro$${DLL_SUFFIX}.dll) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME) &&
  deploy.commands += xcopy $$p($$[QT_INSTALL_BINS]/libgcc*.dll) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME) &&
  deploy.commands += xcopy $$p($$[QT_INSTALL_BINS]/libstdc*.dll) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME) &&
  deploy.commands += xcopy $$p($$[QT_INSTALL_BINS]/libwinpthread*.dll) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME) &&
  deploy.commands += xcopy $$p($$OPENSSL_PATH_WIN\libcrypto*.dll) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME) &&
  deploy.commands += xcopy $$p($$OPENSSL_PATH_WIN\libssl*.dll) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME) &&
  deploy.commands += xcopy $$p($$[QT_INSTALL_BINS]/Qt5Network.dll) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME) &&
  deploy.commands += xcopy $$p($$[QT_INSTALL_BINS]/Qt5PrintSupport.dll) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME) &&
  deploy.commands += xcopy $$p($$[QT_INSTALL_BINS]/Qt5Qml.dll) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME) &&
  deploy.commands += xcopy $$p($$[QT_INSTALL_BINS]/Qt5Sql.dll) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME) &&
  deploy.commands += xcopy $$p($$[QT_INSTALL_BINS]/Qt5Quick.dll) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME) &&
  deploy.commands += xcopy $$p($$[QT_INSTALL_BINS]/Qt5OpenGL.dll) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME) &&
  deploy.commands += xcopy $$p($$[QT_INSTALL_BINS]/Qt5QmlModels.dll) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME) &&
  deploy.commands += xcopy $$p($$[QT_INSTALL_BINS]/Qt5Xml.dll) $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME) &&
  deploy.commands += $$p($$[QT_INSTALL_BINS]/windeployqt) $$WINDEPLOY_FLAGS $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME) &&
  deploy.commands += del /f /q $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME/sqldrivers/qsqlpsql.dll) &&
  deploy.commands += del /f /q $$p($$DEPLOY_BASE/$$WIN_TARGET_NAME/sqldrivers/qsqlodbc.dll)
}

# =====================================================================
# Additional targets

# Need to copy data when compiling
all.depends = copydata

# Deploy needs compiling before
deploy.depends = all

QMAKE_EXTRA_TARGETS += deploy copydata all


