#-------------------------------------------------
#
# Project created by QtCreator 2016-02-09T16:39:58
#
#-------------------------------------------------

QT       += core gui sql xml network svg

# axcontainer axserver concurrent core dbus declarative designer gui help multimedia
# multimediawidgets network opengl printsupport qml qmltest x11extras quick script scripttools
# sensors serialport sql svg testlib uitools webkit webkitwidgets widgets winextras xml xmlpatterns

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

#CONFIG *= debug_and_release debug_and_release_target

TARGET = littlenavmap
TEMPLATE = app

# =======================================================================
# Adapt these paths for each operating system
# =======================================================================

# Windows ==================
win32 {
  QT_BIN=C:\\Qt\\5.5\\mingw492_32\\bin
  GIT_BIN='C:\\Git\\bin\\git'
  CONFIG(debug, debug|release):MARBLE_BASE="c:\\Program Files (x86)\\marble-debug"
  CONFIG(release, debug|release):MARBLE_BASE="c:\\Program Files (x86)\\marble-release"
}

# Linux ==================
unix:!macx {
  CONFIG(debug, debug|release):MARBLE_BASE=/home/alex/Programme/Marble-debug
  CONFIG(release, debug|release):MARBLE_BASE=/home/alex/Programme/Marble-release
}

# Mac OS X ==================
macx {
  CONFIG(debug, debug|release):MARBLE_BASE=/Users/alex/Programme/Marble-debug
  CONFIG(release, debug|release):MARBLE_BASE=/Users/alex/Programme/Marble-release
}

# End of configuration section
# =======================================================================

# =====================================================================
# Dependencies
# =====================================================================

# Marble dependencies
win32 {
  INCLUDEPATH += $$MARBLE_BASE/include
  CONFIG(debug, debug|release):LIBS += -L$$MARBLE_BASE/ -llibmarblewidget-qt5d
  CONFIG(release, debug|release):LIBS += -L$$MARBLE_BASE/ -llibmarblewidget-qt5
  DEPENDPATH += $$MARBLE_BASE/include
}

# Add dependencies to atools project and its static library to ensure relinking on changes
DEPENDPATH += $$PWD/../atools/src
INCLUDEPATH += $$PWD/../atools/src $$PWD/src

CONFIG(debug, debug|release) {
  LIBS += -L $$PWD/../atools/debug -l atools
  PRE_TARGETDEPS += $$PWD/../build-atools-debug/libatools.a
}
CONFIG(release, debug|release) {
  LIBS += -L $$PWD/../atools/release -l atools
  PRE_TARGETDEPS += $$PWD/../build-atools-release/libatools.a
}

unix:!macx {
  INCLUDEPATH += $$MARBLE_BASE/include
  LIBS += -L$$MARBLE_BASE/lib -lmarblewidget-qt5 -lz
  DEPENDPATH += $$MARBLE_BASE/include
}

macx {
  INCLUDEPATH += $$MARBLE_BASE/include
  LIBS += -L$$MARBLE_BASE/lib -lmarblewidget-qt5 -lz
  DEPENDPATH += $$MARBLE_BASE/include
}

CONFIG += c++11

# =====================================================================
# Files
# =====================================================================

# Get the current GIT revision to include it into the code
win32:DEFINES += GIT_REVISION='\\"$$system($${GIT_BIN} rev-parse --short HEAD)\\"'
unix:DEFINES += GIT_REVISION='\\"$$system(git rev-parse --short HEAD)\\"'

SOURCES += src/main.cpp\
        src/gui/mainwindow.cpp \
    src/search/columnlist.cpp \
    src/search/sqlmodel.cpp \
    src/search/column.cpp \
    src/search/sqlproxymodel.cpp \
    src/search/searchcontroller.cpp \
    src/search/airportsearch.cpp \
    src/search/navsearch.cpp \
    src/mapgui/mapquery.cpp \
    src/mapgui/mappaintlayer.cpp \
    src/mapgui/maplayer.cpp \
    src/mapgui/maplayersettings.cpp \
    src/mapgui/mappainter.cpp \
    src/mapgui/mappainterairport.cpp \
    src/mapgui/mappaintermark.cpp \
    src/mapgui/mapscale.cpp \
    src/search/airporticondelegate.cpp \
    src/common/maptypes.cpp \
    src/common/mapcolors.cpp \
    src/mapgui/mappainternav.cpp \
    src/search/navicondelegate.cpp \
    src/mapgui/mappainterils.cpp \
    src/common/maptools.cpp \
    src/route/routecontroller.cpp \
    src/route/routemapobject.cpp \
    src/mapgui/mappainterroute.cpp \
    src/route/routeicondelegate.cpp \
    src/mapgui/maptooltip.cpp \
    src/common/formatter.cpp \
    src/common/coordinateconverter.cpp \
    src/common/maptypesfactory.cpp \
    src/db/databasedialog.cpp \
    src/route/parkingdialog.cpp \
    src/route/routecommand.cpp \
    src/route/routefinder.cpp \
    src/mapgui/mapwidget.cpp \
    src/route/routenetworkradio.cpp \
    src/route/routenetworkairway.cpp \
    src/route/routenetwork.cpp \
    src/common/weatherreporter.cpp \
    src/connect/connectdialog.cpp \
    src/connect/connectclient.cpp \
    src/mapgui/mappainteraircraft.cpp \
    src/profile/profilewidget.cpp \
    src/route/routemapobjectlist.cpp \
    src/common/aircrafttrack.cpp \
    src/info/infocontroller.cpp \
    src/common/symbolpainter.cpp \
    src/info/infoquery.cpp \
    src/db/databasemanager.cpp \
    src/db/dbtypes.cpp \
    src/common/constants.cpp \
    src/export/csvexporter.cpp \
    src/export/exporter.cpp \
    src/export/htmlexporter.cpp \
    src/common/htmlinfobuilder.cpp \
    src/mapgui/mapscreenindex.cpp \
    src/options/optionsdialog.cpp \
    src/options/optiondata.cpp \
    src/common/settingsmigrate.cpp \
    src/search/searchbase.cpp \
    src/search/sqlcontroller.cpp

HEADERS  += src/gui/mainwindow.h \
    src/search/columnlist.h \
    src/search/sqlmodel.h \
    src/search/column.h \
    src/search/sqlproxymodel.h \
    src/search/searchcontroller.h \
    src/search/airportsearch.h \
    src/search/navsearch.h \
    src/mapgui/mapquery.h \
    src/mapgui/mappaintlayer.h \
    src/mapgui/maplayer.h \
    src/mapgui/maplayersettings.h \
    src/mapgui/mappainter.h \
    src/mapgui/mappainterairport.h \
    src/mapgui/mappaintermark.h \
    src/mapgui/mapscale.h \
    src/search/airporticondelegate.h \
    src/common/maptypes.h \
    src/common/mapcolors.h \
    src/mapgui/mappainternav.h \
    src/search/navicondelegate.h \
    src/mapgui/mappainterils.h \
    src/common/maptools.h \
    src/route/routecontroller.h \
    src/route/routemapobject.h \
    src/mapgui/mappainterroute.h \
    src/route/routeicondelegate.h \
    src/mapgui/maptooltip.h \
    src/common/formatter.h \
    src/common/coordinateconverter.h \
    src/common/maptypesfactory.h \
    src/db/databasedialog.h \
    src/route/parkingdialog.h \
    src/route/routecommand.h \
    src/route/routefinder.h \
    src/mapgui/mapwidget.h \
    src/route/routenetworkradio.h \
    src/route/routenetworkairway.h \
    src/route/routenetwork.h \
    src/common/weatherreporter.h \
    src/connect/connectdialog.h \
    src/connect/connectclient.h \
    src/mapgui/mappainteraircraft.h \
    src/profile/profilewidget.h \
    src/route/routemapobjectlist.h \
    src/common/aircrafttrack.h \
    src/info/infocontroller.h \
    src/common/symbolpainter.h \
    src/info/infoquery.h \
    src/db/databasemanager.h \
    src/db/dbtypes.h \
    src/common/constants.h \
    src/export/csvexporter.h \
    src/export/exporter.h \
    src/export/htmlexporter.h \
    src/common/htmlinfobuilder.h \
    src/mapgui/mapscreenindex.h \
    src/options/optionsdialog.h \
    src/options/optiondata.h \
    src/common/settingsmigrate.h \
    src/search/searchbase.h \
    src/search/sqlcontroller.h

FORMS    += src/gui/mainwindow.ui \
    src/db/databasedialog.ui \
    src/route/parkingdialog.ui \
    src/connect/connectdialog.ui \
    src/options/options.ui

DISTFILES += \
    uncrustify.cfg \
    BUILD.txt \
    CHANGELOG.txt \
    htmltidy.cfg \
    LICENSE.txt \
    README.txt \
    help/en/index.html \
    help/en/images/gpl-v3-logo.svg \
    help/en/legend.html \
    help/en/legend_inline.html \
    help/en/features.html \
    help/en/images/legend/highlight_route.png \
    help/en/images/legend/highlight_search.png \
    help/en/images/legend/waypoint_invalid.png \
    help/en/images/legend/apron_transparent.png \
    help/en/images/legend/vordme_large.png \
    help/en/images/legend/vor_large.png \
    help/en/images/legend/aircraft_ground.png \
    help/en/images/legend/aircraft_text.png \
    help/en/images/legend/aircraft.png \
    help/en/images/legend/distance_rhumb.png \
    help/en/images/legend/distance_gc.png \
    help/en/images/legend/distance_vor.png \
    help/en/images/legend/range_ndb.png \
    help/en/images/legend/range_vor.png \
    help/en/images/legend/range_rings.png \
    help/en/images/legend/route_leg.png \
    help/en/images/legend/aircraft_track.png \
    help/en/images/legend/waypoint.png \
    help/en/images/legend/vordme_small.png \
    help/en/images/legend/vor_small.png \
    help/en/images/legend/tower_inactive.png \
    help/en/images/legend/tower_active.png \
    help/en/images/legend/taxiway.png \
    help/en/images/legend/runway_threshold.png \
    help/en/images/legend/runway_overrun.png \
    help/en/images/legend/runway_end.png \
    help/en/images/legend/runway_blastpad.png \
    help/en/images/legend/runway.png \
    help/en/images/legend/profile_start.png \
    help/en/images/legend/profile_end.png \
    help/en/images/legend/profile_route.png \
    help/en/images/legend/profile_aircraft.png \
    help/en/images/legend/parking_ramp_cargo.png \
    help/en/images/legend/parking_mil.png \
    help/en/images/legend/parking_gate_no_jetway.png \
    help/en/images/legend/parking_gate.png \
    help/en/images/legend/parking_ga_ramp.png \
    help/en/images/legend/parking_fuel.png \
    help/en/images/legend/ndb_small.png \
    help/en/images/legend/ndb_large.png \
    help/en/images/legend/marker_outer.png \
    help/en/images/legend/marker_middle.png \
    help/en/images/legend/marker_inner.png \
    help/en/images/legend/mark.png \
    help/en/images/legend/ils_small.png \
    help/en/images/legend/ils_large.png \
    help/en/images/legend/ils_gs_small.png \
    help/en/images/legend/ils_gs.png \
    help/en/images/legend/home.png \
    help/en/images/legend/heliport.png \
    help/en/images/legend/helipad.png \
    help/en/images/legend/dme.png \
    help/en/images/legend/airway_victor.png \
    help/en/images/legend/airway_jet.png \
    help/en/images/legend/airport_water.png \
    help/en/images/legend/airport_tower_water.png \
    help/en/images/legend/airport_tower_soft.png \
    help/en/images/legend/airport_tower_mil.png \
    help/en/images/legend/airport_tower_fuel.png \
    help/en/images/legend/airport_tower_8000.png \
    help/en/images/legend/airport_tower.png \
    help/en/images/legend/airport_text.png \
    help/en/images/legend/airport_soft_fuel.png \
    help/en/images/legend/airport_soft.png \
    help/en/images/legend/airport_mil.png \
    help/en/images/legend/airport_empty_soft.png \
    help/en/images/legend/airport_empty.png \
    help/en/images/legend/airport_closed.png \
    help/en/images/legend/airport_8000.png \
    help/en/images/legend/airport.png \
    help/en/images/legend/airport_tower_closed.png \
    help/en/images/legend/profile_track.png \
    help/en/images/legend/profile_safe_alt.png \
    help/en/images/statusbar.png \
    help/en/images/profile.png \
    help/en/images/fpedit2.png \
    help/en/images/infoac.png \
    help/en/images/infoacprogress.png \
    help/en/images/infonavaid.png \
    help/en/images/inforunway.png \
    help/en/images/infoairport.png \
    help/en/images/options.png \
    help/en/images/connect.png \
    help/en/images/selectstartposition.png \
    help/en/images/flightplan.png \
    help/en/images/navaidsearchtable.png \
    help/en/images/airportsearchtable.png \
    help/en/images/complexsearch.png \
    help/en/images/fpedit.png \
    help/en/images/airportdiagram2.png \
    help/en/images/airportdiagram1.png \
    help/en/images/tooltip.png \
    help/en/images/otm.png \
    help/en/images/osmhillshading.png \
    help/en/images/sphericalpolitical.png \
    help/en/images/menutoolbar.png \
    help/en/images/firststart.png \
    help/en/images/all.png \
    help/en/images/loadsceneryprogress.png \
    help/en/images/loadscenery.png \
    help/en/images/littlenavconnect.png \
    help/en/images/littlenavmap.svg \
    help/en/images/legend/route_start.png \
    help/en/css/style.css

RESOURCES += \
    littlenavmap.qrc

ICON=resources/icons/littlenavmap.icns

# =====================================================================
# Copy data commands
# =====================================================================

# Linux - Copy help and Marble plugins and data
unix:!macx {
  copydata.commands = mkdir -p $$OUT_PWD/plugins &&
  copydata.commands += cp -avfu \
    $${MARBLE_BASE}/lib/marble/plugins/libCachePlugin.so \
    $${MARBLE_BASE}/lib/marble/plugins/libCompassFloatItem.so \
    $${MARBLE_BASE}/lib/marble/plugins/libGraticulePlugin.so \
    $${MARBLE_BASE}/lib/marble/plugins/libKmlPlugin.so \
    $${MARBLE_BASE}/lib/marble/plugins/libLatLonPlugin.so \
    $${MARBLE_BASE}/lib/marble/plugins/libLicense.so \
    $${MARBLE_BASE}/lib/marble/plugins/libMapScaleFloatItem.so \
    $${MARBLE_BASE}/lib/marble/plugins/libNavigationFloatItem.so \
    $${MARBLE_BASE}/lib/marble/plugins/libOsmPlugin.so \
    $${MARBLE_BASE}/lib/marble/plugins/libOverviewMap.so \
    $${MARBLE_BASE}/lib/marble/plugins/libPn2Plugin.so \
    $${MARBLE_BASE}/lib/marble/plugins/libPntPlugin.so \
    $$OUT_PWD/plugins &&
  copydata.commands += cp -avfu $$PWD/help $$OUT_PWD &&
  copydata.commands += cp -avfu $$PWD/marble/data $$OUT_PWD

  cleandata.commands = rm -Rvf $$OUT_PWD/help $$OUT_PWD/data $$OUT_PWD/plugins
}

# Mac OS X - Copy help and Marble plugins and data
macx {
  copydata.commands = mkdir -p $$OUT_PWD/littlenavmap.app/Contents/MacOS/plugins &&
  copydata.commands += cp -Rvf \
    $${MARBLE_BASE}/lib/plugins/libCachePlugin.so \
    $${MARBLE_BASE}/lib/plugins/libCompassFloatItem.so \
    $${MARBLE_BASE}/lib/plugins/libGraticulePlugin.so \
    $${MARBLE_BASE}/lib/plugins/libKmlPlugin.so \
    $${MARBLE_BASE}/lib/plugins/libLatLonPlugin.so \
    $${MARBLE_BASE}/lib/plugins/libLicense.so \
    $${MARBLE_BASE}/lib/plugins/libMapScaleFloatItem.so \
    $${MARBLE_BASE}/lib/plugins/libNavigationFloatItem.so \
    $${MARBLE_BASE}/lib/plugins/libOsmPlugin.so \
    $${MARBLE_BASE}/lib/plugins/libOverviewMap.so \
    $${MARBLE_BASE}/lib/plugins/libPn2Plugin.so \
    $${MARBLE_BASE}/lib/plugins/libPntPlugin.so \
    $$OUT_PWD/littlenavmap.app/Contents/MacOS/plugins &&
  copydata.commands += cp -Rv $$PWD/help $$OUT_PWD/littlenavmap.app/Contents/MacOS &&
  copydata.commands += cp -Rv $$PWD/marble/data $$OUT_PWD/littlenavmap.app/Contents/MacOS

  cleandata.commands = rm -Rvf $$OUT_PWD/help $$OUT_PWD/data $$OUT_PWD/plugins
}

# =====================================================================
# Deployment commands
# =====================================================================

# Linux specific deploy target
unix:!macx {
  deploy.commands = rm -Rfv $$PWD/deploy/LittleNavmap &&
  deploy.commands += mkdir -pv $$PWD/deploy/LittleNavmap &&
  deploy.commands += cp -Rvf $${MARBLE_BASE}/lib/*.so* $$PWD/deploy/LittleNavmap &&
  deploy.commands += cp -Rvf $${OUT_PWD}/plugins $$PWD/deploy/LittleNavmap &&
  deploy.commands += cp -Rvf $${OUT_PWD}/data $$PWD/deploy/LittleNavmap &&
  deploy.commands += cp -Rvf $${OUT_PWD}/help $$PWD/deploy/LittleNavmap &&
  deploy.commands += cp -Rvf $${OUT_PWD}/littlenavmap $$PWD/deploy/LittleNavmap &&
  deploy.commands += cp -vf $$PWD/desktop/littlenavmap.sh $$PWD/deploy/LittleNavmap &&
  deploy.commands += chmod -v a+x $$PWD/deploy/LittleNavmap/littlenavmap.sh &&
  deploy.commands += cp -vf $${PWD}/CHANGELOG.txt $$PWD/deploy/LittleNavmap &&
  deploy.commands += cp -vf $${PWD}/README.txt $$PWD/deploy/LittleNavmap &&
  deploy.commands += cp -vf $${PWD}/LICENSE.txt $$PWD/deploy/LittleNavmap &&
  deploy.commands += cp -vf $${PWD}/resources/icons/littlenavmap.svg $$PWD/deploy/LittleNavmap
}

# Mac specific deploy target
macx {
  deploy.commands = mkdir -p $$OUT_PWD/littlenavmap.app/Contents/MacOS/lib &&
  deploy.commands += cp -Rvf $${MARBLE_BASE}/lib/*.dylib littlenavmap.app/Contents/MacOS/lib/ &&
  deploy.commands += macdeployqt littlenavmap.app -always-overwrite -dmg -executable=littlenavmap.app/Contents/MacOS/lib/libmarblewidget-qt5.dylib -executable=littlenavmap.app/Contents/MacOS/lib/libastro.dylib
#-verbose=3
}

# Windows specific deploy target
win32 {
  RC_ICONS = resources/icons/littlenavmap.ico

  # Create backslashed path
  WINPWD=$${PWD}
  WINPWD ~= s,/,\\,g
  WINOUT_PWD=$${OUT_PWD}
  WINOUT_PWD ~= s,/,\\,g
  DEPLOY_DIR_NAME=Little Navmap
  DEPLOY_DIR_WIN=\"$${WINPWD}\\deploy\\$${DEPLOY_DIR_NAME}\"
  MARBLE_BASE_WIN=\"$${MARBLE_BASE}\"

  copydata.commands = xcopy /i /s /e /f /y $${WINPWD}\\help $${WINOUT_PWD}\\help &&
  copydata.commands =+ xcopy /i /s /e /f /y $${WINPWD}\\marble\\data $${WINOUT_PWD}\\data
  cleandata.commands = del /s /q $${WINOUT_PWD}\\help

  CONFIG(debug, debug|release):DLL_SUFFIX=d
  CONFIG(release, debug|release):DLL_SUFFIX=

  deploy.commands = rmdir /s /q $${DEPLOY_DIR_WIN} &
  deploy.commands += mkdir $${DEPLOY_DIR_WIN} &&
  deploy.commands += mkdir $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libCachePlugin$${dll_suffix}.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libCompassFloatItem$${dll_suffix}.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libGraticulePlugin$${dll_suffix}.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libKmlPlugin$${dll_suffix}.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libLatLonPlugin$${dll_suffix}.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libLicense$${dll_suffix}.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libMapScaleFloatItem$${dll_suffix}.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libNavigationFloatItem$${dll_suffix}.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libOsmPlugin$${dll_suffix}.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libOverviewMap$${dll_suffix}.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libPn2Plugin$${dll_suffix}.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libPntPlugin$${dll_suffix}.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${WINOUT_PWD}\\littlenavmap.exe $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${WINPWD}\\CHANGELOG.txt $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${WINPWD}\\README.txt $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${WINPWD}\\LICENSE.txt $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy /i /s /e /f /y $${WINPWD}\\help $${DEPLOY_DIR_WIN}\\help &&
  deploy.commands += xcopy /i /s /e /f /y $${WINPWD}\\marble\\data $${DEPLOY_DIR_WIN}\\data &&
  deploy.commands += xcopy $${QT_BIN}\\libgcc*.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${QT_BIN}\\libstdc*.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${QT_BIN}\\libwinpthread*.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\libmarblewidget-qt5$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\libastro$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\Qt5DBus$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\Qt5Network$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\Qt5PrintSupport$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\Qt5Qml$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\Qt5Quick$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\Qt5Script$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\Qt5WebKit$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\icudt54.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\icuin54.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\icuuc54.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\Qt5Positioning$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\Qt5Multimedia$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\Qt5Sensors$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\Qt5WebChannel$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\Qt5WebKitWidgets$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\Qt5OpenGL$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\Qt5MultimediaWidgets$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\Qt5Xml$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += $${QT_BIN}\\windeployqt --compiler-runtime $${DEPLOY_DIR_WIN}
}

QMAKE_EXTRA_TARGETS += deploy

first.depends = $(first) copydata
QMAKE_EXTRA_TARGETS += first copydata

clean.depends = $(clean) cleandata
QMAKE_EXTRA_TARGETS += clean cleandata

