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

# Adapt these variables to compile on Windows
win32 {
  QT_BIN=C:\\Qt\\5.5\\mingw492_32\\bin
  GIT_BIN='C:\\Git\\bin\\git'
  CONFIG(debug, debug|release):MARBLE_BASE="c:\\Program Files (x86)\\marble-debug"
  CONFIG(release, debug|release):MARBLE_BASE="c:\\Program Files (x86)\\marble-release"
}

unix {
  CONFIG(debug, debug|release):MARBLE_BASE=/home/alex/Programme/Marble-debug
  CONFIG(release, debug|release):MARBLE_BASE=/home/alex/Programme/Marble-release
}

CONFIG += c++11

# Get the current GIT revision to include it into the code
win32:DEFINES += GIT_REVISION='\\"$$system($${GIT_BIN} rev-parse --short HEAD)\\"'
unix:DEFINES += GIT_REVISION='\\"$$system(git rev-parse --short HEAD)\\"'

win32:DEFINES +=_USE_MATH_DEFINES

SOURCES += src/main.cpp\
        src/gui/mainwindow.cpp \
    src/table/columnlist.cpp \
    src/table/controller.cpp \
    src/table/sqlmodel.cpp \
    src/table/column.cpp \
    src/table/sqlproxymodel.cpp \
    src/table/searchcontroller.cpp \
    src/table/search.cpp \
    src/table/airportsearch.cpp \
    src/table/navsearch.cpp \
    src/mapgui/mapquery.cpp \
    src/mapgui/navmapwidget.cpp \
    src/mapgui/mappaintlayer.cpp \
    src/mapgui/maplayer.cpp \
    src/mapgui/maplayersettings.cpp \
    src/mapgui/mappainter.cpp \
    src/mapgui/mappainterairport.cpp \
    src/mapgui/mappaintermark.cpp \
    src/mapgui/mapscale.cpp \
    src/table/airporticondelegate.cpp \
    src/mapgui/symbolpainter.cpp \
    src/common/maptypes.cpp \
    src/common/mapcolors.cpp \
    src/mapgui/mappainternav.cpp \
    src/table/navicondelegate.cpp \
    src/mapgui/mappainterils.cpp \
    src/mapgui/mapposhistory.cpp \
    src/common/maptools.cpp \
    src/route/routecontroller.cpp \
    src/route/routemapobject.cpp \
    src/mapgui/mappainterroute.cpp \
    src/route/routeicondelegate.cpp \
    src/mapgui/maptooltip.cpp \
    src/common/formatter.cpp \
    src/common/coordinateconverter.cpp \
    src/common/maptypesfactory.cpp \
    src/common/morsecode.cpp

HEADERS  += src/gui/mainwindow.h \
    src/table/columnlist.h \
    src/table/controller.h \
    src/table/sqlmodel.h \
    src/table/column.h \
    src/table/sqlproxymodel.h \
    src/table/searchcontroller.h \
    src/table/search.h \
    src/table/airportsearch.h \
    src/table/navsearch.h \
    src/mapgui/mapquery.h \
    src/mapgui/navmapwidget.h \
    src/mapgui/mappaintlayer.h \
    src/mapgui/maplayer.h \
    src/mapgui/maplayersettings.h \
    src/mapgui/mappainter.h \
    src/mapgui/mappainterairport.h \
    src/mapgui/mappaintermark.h \
    src/mapgui/mapscale.h \
    src/table/airporticondelegate.h \
    src/mapgui/symbolpainter.h \
    src/common/maptypes.h \
    src/common/mapcolors.h \
    src/mapgui/mappainternav.h \
    src/table/navicondelegate.h \
    src/mapgui/mappainterils.h \
    src/mapgui/mapposhistory.h \
    src/common/maptools.h \
    src/route/routecontroller.h \
    src/route/routemapobject.h \
    src/mapgui/mappainterroute.h \
    src/route/routeicondelegate.h \
    src/mapgui/maptooltip.h \
    src/common/formatter.h \
    src/common/coordinateconverter.h \
    src/common/maptypesfactory.h \
    src/common/morsecode.h

FORMS    += src/gui/mainwindow.ui

# Marble dependencies
win32 {
  INCLUDEPATH += $$MARBLE_BASE/include
  CONFIG(debug, debug|release):LIBS += -L$$MARBLE_BASE/ -llibmarblewidget-qt5d
  CONFIG(release, debug|release):LIBS += -L$$MARBLE_BASE/ -llibmarblewidget-qt5
  DEPENDPATH += $$MARBLE_BASE/include
}

unix {
  INCLUDEPATH += $$MARBLE_BASE/include
  LIBS += -L$$MARBLE_BASE/lib -lmarblewidget-qt5
  DEPENDPATH += $$MARBLE_BASE/include
}

# Add dependencies to atools project and its static library to ensure relinking on changes
DEPENDPATH += $$PWD/../atools/src
INCLUDEPATH += $$PWD/../atools/src $$PWD/src

CONFIG(debug, debug|release) {
  LIBS += -L $$PWD/../atools/debug -l atools
  PRE_TARGETDEPS += $$PWD/../atools/debug/libatools.a
}
CONFIG(release, debug|release) {
  LIBS += -L $$PWD/../atools/release -l atools
  PRE_TARGETDEPS += $$PWD/../atools/release/libatools.a
}

DISTFILES += \
    uncrustify.cfg \
    BUILD.txt \
    CHANGELOG.txt \
    htmltidy.cfg \
    LICENSE.txt \
    README.txt \
    help/en/index.html \
    help/en/images/gpl-v3-logo.svg

RESOURCES += \
    littlenavmap.qrc

# Create additional makefile targets to copy help files
unix {
  copydata.commands = mkdir -p $$OUT_PWD/plugins &&
  copydata.commands += cp -avfu $${MARBLE_BASE}/lib/marble/plugins/libAprsPlugin.so \
                                $${MARBLE_BASE}/lib/marble/plugins/libCachePlugin.so \
                                $${MARBLE_BASE}/lib/marble/plugins/libCompassFloatItem.so \
                                $${MARBLE_BASE}/lib/marble/plugins/libElevationProfileFloatItem.so \
                                $${MARBLE_BASE}/lib/marble/plugins/libElevationProfileMarker.so \
                                $${MARBLE_BASE}/lib/marble/plugins/libGpxPlugin.so \
                                $${MARBLE_BASE}/lib/marble/plugins/libGraticulePlugin.so \
                                $${MARBLE_BASE}/lib/marble/plugins/libJsonPlugin.so \
                                $${MARBLE_BASE}/lib/marble/plugins/libKmlPlugin.so \
                                $${MARBLE_BASE}/lib/marble/plugins/libLatLonPlugin.so \
                                $${MARBLE_BASE}/lib/marble/plugins/libLicense.so \
                                $${MARBLE_BASE}/lib/marble/plugins/libLocalDatabasePlugin.so \
                                $${MARBLE_BASE}/lib/marble/plugins/libLogPlugin.so \
                                $${MARBLE_BASE}/lib/marble/plugins/libMapQuestPlugin.so \
                                $${MARBLE_BASE}/lib/marble/plugins/libMapScaleFloatItem.so \
                                $${MARBLE_BASE}/lib/marble/plugins/libMonavPlugin.so \
                                $${MARBLE_BASE}/lib/marble/plugins/libNavigationFloatItem.so \
                                $${MARBLE_BASE}/lib/marble/plugins/libOpenCachingComPlugin.so \
                                $${MARBLE_BASE}/lib/marble/plugins/libOpenDesktopPlugin.so \
                                $${MARBLE_BASE}/lib/marble/plugins/libOpenRouteServicePlugin.so \
                                $${MARBLE_BASE}/lib/marble/plugins/libOsmPlugin.so \
                                $${MARBLE_BASE}/lib/marble/plugins/libOSRMPlugin.so \
                                $${MARBLE_BASE}/lib/marble/plugins/libOverviewMap.so \
                                $${MARBLE_BASE}/lib/marble/plugins/libPn2Plugin.so \
                                $${MARBLE_BASE}/lib/marble/plugins/libPntPlugin.so \
                                $${MARBLE_BASE}/lib/marble/plugins/libPositionMarker.so \
                                $${MARBLE_BASE}/lib/marble/plugins/libProgressFloatItem.so  $$OUT_PWD/plugins &&
  copydata.commands += cp -avfu $$PWD/help $$OUT_PWD &&
  copydata.commands += cp -avfu $$PWD/marble/data $$OUT_PWD

  cleandata.commands = rm -Rvf $$OUT_PWD/help $$OUT_PWD/data $$OUT_PWD/plugins
}


# Windows specific deploy target
win32 {
  RC_ICONS = resources/icons/navroute.ico

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
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libCachePlugin.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libCompassFloatItem.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libElevationProfileFloatItem.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libElevationProfileMarker.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libGpxPlugin.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libGraticulePlugin.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libJsonPlugin.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libKmlPlugin.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libLatLonPlugin.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libLicense.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libLocalDatabasePlugin.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libLogPlugin.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libMapQuestPlugin.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libMapScaleFloatItem.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libMonavPlugin.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libNavigationFloatItem.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libOpenCachingComPlugin.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libOpenDesktopPlugin.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libOpenRouteServicePlugin.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libOsmPlugin.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libOSRMPlugin.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libOverviewMap.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libPn2Plugin.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libPntPlugin.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libPositionMarker.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libProgressFloatItem.dll $${DEPLOY_DIR_WIN}\\plugins &&
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

