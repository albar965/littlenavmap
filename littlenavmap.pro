#-------------------------------------------------
#
# Project created by QtCreator 2016-02-09T16:39:58
#
#-------------------------------------------------

QT       += core gui sql xml network svg printsupport

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

CONFIG(debug, debug|release):CONF_TYPE=debug
CONFIG(release, debug|release):CONF_TYPE=release

# Windows ==================
win32 {
  QT_HOME=C:\\Qt\\5.9\\mingw53_32
  OPENSSL=C:\\OpenSSL-Win32
  GIT_BIN='C:\\Git\\bin\\git'
  MARBLE_BASE="c:\\Projekte\\marble-$${CONF_TYPE}"
}

# Linux ==================
unix:!macx {
  QT_HOME=/home/alex/Qt/5.9/gcc_64
  MARBLE_BASE=/home/alex/Programme/Marble-$${CONF_TYPE}
}

# Mac OS X ==================
macx {
  MARBLE_BASE=/Users/alex/Programme/Marble-$${CONF_TYPE}
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

win32 {
DEFINES += _USE_MATH_DEFINES
  LIBS += -L $$PWD/../build-atools-$${CONF_TYPE}/$${CONF_TYPE} -l atools
  LIBS += -lz
  PRE_TARGETDEPS += $$PWD/../build-atools-$${CONF_TYPE}/$${CONF_TYPE}/libatools.a
  WINDEPLOY_FLAGS = --compiler-runtime
}

unix {
  LIBS += -L $$PWD/../build-atools-$${CONF_TYPE} -l atools
  PRE_TARGETDEPS += $$PWD/../build-atools-$${CONF_TYPE}/libatools.a
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
    src/mapgui/mappainterroute.cpp \
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
    src/common/aircrafttrack.cpp \
    src/info/infocontroller.cpp \
    src/common/symbolpainter.cpp \
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
    src/search/sqlcontroller.cpp \
    src/print/printsupport.cpp \
    src/print/printdialog.cpp \
    src/route/routestring.cpp \
    src/route/routestringdialog.cpp \
    src/route/flightplanentrybuilder.cpp \
    src/common/unit.cpp \
    src/route/userwaypointdialog.cpp \
    src/common/infoquery.cpp \
    src/common/textplacement.cpp \
    src/route/routeleg.cpp \
    src/route/route.cpp \
    src/common/procedurequery.cpp \
    src/search/abstractsearch.cpp \
    src/search/proceduresearch.cpp \
    src/common/proctypes.cpp \
    src/mapgui/mappainterairspace.cpp \
    src/db/databaseerrordialog.cpp \
    src/gui/airspacetoolbarhandler.cpp \
    src/navapp.cpp \
    src/common/mapflags.cpp \
    src/common/elevationprovider.cpp \
    src/mapgui/mappaintership.cpp \
    src/mapgui/mappaintervehicle.cpp \
    src/common/updatehandler.cpp

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
    src/mapgui/mappainterroute.h \
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
    src/common/aircrafttrack.h \
    src/info/infocontroller.h \
    src/common/symbolpainter.h \
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
    src/search/sqlcontroller.h \
    src/print/printsupport.h \
    src/print/printdialog.h \
    src/route/routestring.h \
    src/route/routestringdialog.h \
    src/route/flightplanentrybuilder.h \
    src/common/unit.h \
    src/route/userwaypointdialog.h \
    src/common/infoquery.h \
    src/common/textplacement.h \
    src/route/routeleg.h \
    src/route/route.h \
    src/common/procedurequery.h \
    src/search/abstractsearch.h \
    src/search/proceduresearch.h \
    src/common/proctypes.h \
    src/mapgui/mappainterairspace.h \
    src/db/databaseerrordialog.h \
    src/gui/airspacetoolbarhandler.h \
    src/navapp.h \
    src/common/mapflags.h \
    src/common/elevationprovider.h \
    src/mapgui/mappaintership.h \
    src/mapgui/mappaintervehicle.h \
    src/common/updatehandler.h

FORMS    += src/gui/mainwindow.ui \
    src/db/databasedialog.ui \
    src/route/parkingdialog.ui \
    src/connect/connectdialog.ui \
    src/options/options.ui \
    src/print/printdialog.ui \
    src/route/routestringdialog.ui \
    src/route/userwaypointdialog.ui \
    src/db/databaseerrordialog.ui

DISTFILES += \
    uncrustify.cfg \
    BUILD.txt \
    CHANGELOG.txt \
    htmltidy.cfg \
    LICENSE.txt \
    README.txt

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
  copydata.commands += cp -avfu $$PWD/magdec $$OUT_PWD &&
  copydata.commands += cp -avfu $$PWD/marble/data $$OUT_PWD &&
  copydata.commands += cp -vf $$PWD/desktop/littlenavmap*.sh $$OUT_PWD &&
  copydata.commands += chmod -v a+x $$OUT_PWD/littlenavmap*.sh

  cleandata.commands = rm -Rvf $$OUT_PWD/help $$OUT_PWD/magdec $$OUT_PWD/data $$OUT_PWD/plugins
}

# Mac OS X - Copy help and Marble plugins and data
macx {
  copydata.commands += cp -Rv $$PWD/help $$OUT_PWD/littlenavmap.app/Contents/MacOS &&
  copydata.commands += cp -Rv $$PWD/magdec $$OUT_PWD/littlenavmap.app/Contents/MacOS &&
  copydata.commands += cp -Rv $$PWD/marble/data $$OUT_PWD/littlenavmap.app/Contents/MacOS

  cleandata.commands = rm -Rvf $$OUT_PWD/help $$PWD/magdec $$OUT_PWD/data $$OUT_PWD/plugins
}

# =====================================================================
# Deployment commands
# =====================================================================

# Linux specific deploy target
unix:!macx {
  DEPLOY_DIR=\"$$PWD/../deploy/Little Navmap\"
  DEPLOY_DIR_LIB=\"$$PWD/../deploy/Little Navmap/lib\"

  deploy.commands = rm -Rfv $${DEPLOY_DIR} &&
  deploy.commands += mkdir -pv $${DEPLOY_DIR_LIB} &&
  deploy.commands += mkdir -pv $${DEPLOY_DIR}/iconengines &&
  deploy.commands += mkdir -pv $${DEPLOY_DIR}/imageformats &&
  deploy.commands += mkdir -pv $${DEPLOY_DIR}/platforms &&
  deploy.commands += mkdir -pv $${DEPLOY_DIR}/platformthemes &&
  deploy.commands += mkdir -pv $${DEPLOY_DIR}/printsupport &&
  deploy.commands += mkdir -pv $${DEPLOY_DIR}/sqldrivers &&
  deploy.commands += cp -Rvf $${MARBLE_BASE}/lib/*.so* $${DEPLOY_DIR_LIB} &&
  deploy.commands += cp -Rvf $${OUT_PWD}/plugins $${DEPLOY_DIR} &&
  deploy.commands += cp -Rvf $${OUT_PWD}/data $${DEPLOY_DIR} &&
  deploy.commands += cp -Rvf $${OUT_PWD}/help $${DEPLOY_DIR} &&
  deploy.commands += cp -Rvf $${OUT_PWD}/magdec $${DEPLOY_DIR} &&
  deploy.commands += cp -Rvf $${OUT_PWD}/littlenavmap $${DEPLOY_DIR} &&
  deploy.commands += cp -vf $$PWD/desktop/littlenavmap.sh $${DEPLOY_DIR} &&
  deploy.commands += chmod -v a+x $${DEPLOY_DIR}/littlenavmap.sh &&
  deploy.commands += cp -vf $${PWD}/CHANGELOG.txt $${DEPLOY_DIR} &&
  deploy.commands += cp -vf $${PWD}/README.txt $${DEPLOY_DIR} &&
  deploy.commands += cp -vf $${PWD}/LICENSE.txt $${DEPLOY_DIR} &&
  deploy.commands += cp -vf $${PWD}/resources/icons/littlenavmap.svg $${DEPLOY_DIR} &&
  deploy.commands += cp -vfa $${QT_HOME}/plugins/iconengines/libqsvgicon.so*  $${DEPLOY_DIR}/iconengines &&
  deploy.commands += cp -vfa $${QT_HOME}/plugins/imageformats/libqgif.so*  $${DEPLOY_DIR}/imageformats &&
  deploy.commands += cp -vfa $${QT_HOME}/plugins/imageformats/libqjp2.so*  $${DEPLOY_DIR}/imageformats &&
  deploy.commands += cp -vfa $${QT_HOME}/plugins/imageformats/libqjpeg.so*  $${DEPLOY_DIR}/imageformats &&
  deploy.commands += cp -vfa $${QT_HOME}/plugins/imageformats/libqsvg.so*  $${DEPLOY_DIR}/imageformats &&
  deploy.commands += cp -vfa $${QT_HOME}/plugins/imageformats/libqwbmp.so*  $${DEPLOY_DIR}/imageformats &&
  deploy.commands += cp -vfa $${QT_HOME}/plugins/imageformats/libqwebp.so*  $${DEPLOY_DIR}/imageformats &&
  deploy.commands += cp -vfa $${QT_HOME}/plugins/platforms/libqeglfs.so*  $${DEPLOY_DIR}/platforms &&
  deploy.commands += cp -vfa $${QT_HOME}/plugins/platforms/libqlinuxfb.so*  $${DEPLOY_DIR}/platforms &&
  deploy.commands += cp -vfa $${QT_HOME}/plugins/platforms/libqminimal.so*  $${DEPLOY_DIR}/platforms &&
  deploy.commands += cp -vfa $${QT_HOME}/plugins/platforms/libqminimalegl.so*  $${DEPLOY_DIR}/platforms &&
  deploy.commands += cp -vfa $${QT_HOME}/plugins/platforms/libqoffscreen.so*  $${DEPLOY_DIR}/platforms &&
  deploy.commands += cp -vfa $${QT_HOME}/plugins/platforms/libqxcb.so*  $${DEPLOY_DIR}/platforms &&
  deploy.commands += cp -vfa $${QT_HOME}/plugins/platformthemes/libqgtk*.so*  $${DEPLOY_DIR}/platformthemes &&
  deploy.commands += cp -vfa $${QT_HOME}/plugins/printsupport/libcupsprintersupport.so*  $${DEPLOY_DIR}/printsupport &&
  deploy.commands += cp -vfa $${QT_HOME}/plugins/sqldrivers/libqsqlite.so*  $${DEPLOY_DIR}/sqldrivers &&
  deploy.commands += cp -vfa $${QT_HOME}/lib/libicudata.so*  $${DEPLOY_DIR_LIB} &&
  deploy.commands += cp -vfa $${QT_HOME}/lib/libicui18n.so*  $${DEPLOY_DIR_LIB} &&
  deploy.commands += cp -vfa $${QT_HOME}/lib/libicuuc.so*  $${DEPLOY_DIR_LIB} &&
  deploy.commands += cp -vfa $${QT_HOME}/lib/libQt5Concurrent.so*  $${DEPLOY_DIR_LIB} &&
  deploy.commands += cp -vfa $${QT_HOME}/lib/libQt5Core.so*  $${DEPLOY_DIR_LIB} &&
  deploy.commands += cp -vfa $${QT_HOME}/lib/libQt5DBus.so*  $${DEPLOY_DIR_LIB} &&
  deploy.commands += cp -vfa $${QT_HOME}/lib/libQt5Gui.so*  $${DEPLOY_DIR_LIB} &&
  deploy.commands += cp -vfa $${QT_HOME}/lib/libQt5Network.so*  $${DEPLOY_DIR_LIB} &&
  deploy.commands += cp -vfa $${QT_HOME}/lib/libQt5PrintSupport.so*  $${DEPLOY_DIR_LIB} &&
  deploy.commands += cp -vfa $${QT_HOME}/lib/libQt5Qml.so*  $${DEPLOY_DIR_LIB} &&
  deploy.commands += cp -vfa $${QT_HOME}/lib/libQt5Quick.so*  $${DEPLOY_DIR_LIB} &&
  deploy.commands += cp -vfa $${QT_HOME}/lib/libQt5Script.so*  $${DEPLOY_DIR_LIB} &&
  deploy.commands += cp -vfa $${QT_HOME}/lib/libQt5Sql.so*  $${DEPLOY_DIR_LIB} &&
  deploy.commands += cp -vfa $${QT_HOME}/lib/libQt5Svg.so*  $${DEPLOY_DIR_LIB} &&
  deploy.commands += cp -vfa $${QT_HOME}/lib/libQt5Widgets.so*  $${DEPLOY_DIR_LIB} &&
  deploy.commands += cp -vfa $${QT_HOME}/lib/libQt5X11Extras.so*  $${DEPLOY_DIR_LIB} &&
  deploy.commands += cp -vfa $${QT_HOME}/lib/libQt5XcbQpa.so*  $${DEPLOY_DIR_LIB} &&
  deploy.commands += cp -vfa $${QT_HOME}/lib/libQt5Xml.so* $${DEPLOY_DIR_LIB}
}

# Mac specific deploy target
macx {
  INSTALL_MARBLE_DYLIB_CMD=install_name_tool \
         -change /Users/alex/Projekte/build-marble-$$CONF_TYPE/src/lib/marble/libmarblewidget-qt5.25.dylib \
          @executable_path/../Frameworks/libmarblewidget-qt5.25.dylib $$OUT_PWD/littlenavmap.app/Contents/PlugIns

  deploy.commands += mkdir -p $$OUT_PWD/littlenavmap.app/Contents/PlugIns &&
  deploy.commands += cp -Rvf \
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
    $$OUT_PWD/littlenavmap.app/Contents/PlugIns &&
  deploy.commands +=  $$INSTALL_MARBLE_DYLIB_CMD/libCachePlugin.so &&
  deploy.commands +=  $$INSTALL_MARBLE_DYLIB_CMD/libCachePlugin.so &&
  deploy.commands +=  $$INSTALL_MARBLE_DYLIB_CMD/libCompassFloatItem.so &&
  deploy.commands +=  $$INSTALL_MARBLE_DYLIB_CMD/libGraticulePlugin.so &&
  deploy.commands +=  $$INSTALL_MARBLE_DYLIB_CMD/libKmlPlugin.so &&
  deploy.commands +=  $$INSTALL_MARBLE_DYLIB_CMD/libLatLonPlugin.so &&
  deploy.commands +=  $$INSTALL_MARBLE_DYLIB_CMD/libLicense.so &&
  deploy.commands +=  $$INSTALL_MARBLE_DYLIB_CMD/libMapScaleFloatItem.so &&
  deploy.commands +=  $$INSTALL_MARBLE_DYLIB_CMD/libNavigationFloatItem.so &&
  deploy.commands +=  $$INSTALL_MARBLE_DYLIB_CMD/libOsmPlugin.so &&
  deploy.commands +=  $$INSTALL_MARBLE_DYLIB_CMD/libOverviewMap.so &&
  deploy.commands +=  $$INSTALL_MARBLE_DYLIB_CMD/libPn2Plugin.so &&
  deploy.commands +=  $$INSTALL_MARBLE_DYLIB_CMD/libPntPlugin.so &&
  deploy.commands += macdeployqt littlenavmap.app -appstore-compliant -always-overwrite -dmg

# -verbose=3
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
  DEPLOY_DIR_WIN=\"$${WINPWD}\\..\\deploy\\$${DEPLOY_DIR_NAME}\"
  MARBLE_BASE_WIN=\"$${MARBLE_BASE}\"

  CONFIG(debug, debug|release):DLL_SUFFIX=d
  CONFIG(release, debug|release):DLL_SUFFIX=

  deploy.commands = rmdir /s /q $${DEPLOY_DIR_WIN} &
  deploy.commands += mkdir $${DEPLOY_DIR_WIN} &&
  deploy.commands += mkdir $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libCachePlugin$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libCompassFloatItem$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libGraticulePlugin$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libKmlPlugin$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libLatLonPlugin$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libLicense$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libMapScaleFloatItem$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libNavigationFloatItem$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libOsmPlugin$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libOverviewMap$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libPn2Plugin$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\plugins\\libPntPlugin$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN}\\plugins &&
  deploy.commands += xcopy $${WINOUT_PWD}\\$${CONF_TYPE}\\littlenavmap.exe $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${WINPWD}\\CHANGELOG.txt $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${WINPWD}\\README.txt $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${WINPWD}\\LICENSE.txt $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${WINPWD}\\littlenavmap.exe.simconnect $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy /i /s /e /f /y $${WINPWD}\\help $${DEPLOY_DIR_WIN}\\help &&
  deploy.commands += xcopy /i /s /e /f /y $${WINPWD}\\magdec $${DEPLOY_DIR_WIN}\\magdec &&
  deploy.commands += xcopy /i /s /e /f /y $${WINPWD}\\etc $${DEPLOY_DIR_WIN}\\etc &&
  deploy.commands += xcopy /i /s /e /f /y $${WINPWD}\\marble\\data $${DEPLOY_DIR_WIN}\\data &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\libmarblewidget-qt5$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${MARBLE_BASE_WIN}\\libastro$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${QT_HOME}\\bin\\libgcc*.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${QT_HOME}\\bin\\libstdc*.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${QT_HOME}\\bin\\libwinpthread*.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${OPENSSL}\\bin\\libeay32.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${OPENSSL}\\bin\\ssleay32.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${OPENSSL}\\libssl32.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${QT_HOME}\\bin\\Qt5DBus$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${QT_HOME}\\bin\\Qt5Network$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${QT_HOME}\\bin\\Qt5PrintSupport$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${QT_HOME}\\bin\\Qt5Qml$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${QT_HOME}\\bin\\Qt5Quick$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${QT_HOME}\\bin\\Qt5Script$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${QT_HOME}\\bin\\Qt5Multimedia$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${QT_HOME}\\bin\\Qt5OpenGL$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${QT_HOME}\\bin\\Qt5MultimediaWidgets$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += xcopy $${QT_HOME}\\bin\\Qt5Xml$${DLL_SUFFIX}.dll $${DEPLOY_DIR_WIN} &&
  deploy.commands += $${QT_HOME}\\bin\\windeployqt $${WINDEPLOY_FLAGS} $${DEPLOY_DIR_WIN}
}

QMAKE_EXTRA_TARGETS += deploy

first.depends = $(first) copydata
QMAKE_EXTRA_TARGETS += first copydata

clean.depends = $(clean) cleandata
QMAKE_EXTRA_TARGETS += clean cleandata

#TRANSLATIONS = littlenavmap_de.ts \
#               littlenavmap_nl.ts \
#               littlenavmap_fr.ts
