#-------------------------------------------------
#
# Project created by QtCreator 2016-02-09T16:39:58
#
#-------------------------------------------------

QT       += core gui sql xml

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

#CONFIG *= debug_and_release debug_and_release_target

TARGET = littlenavmap
TEMPLATE = app

# Adapt these variables to compile on Windows
win32 {
  QT_BIN=C:\\Qt\\5.5\\mingw492_32\\bin
  GIT_BIN='C:\\Git\\bin\\git'
  MARBLE_BASE=\"c:\\Program Files (x86)\\Marble\"
}

unix {
  MARBLE_BASE=/home/alex/Programme/Marble-Debug
}

CONFIG += c++11

# Get the current GIT revision to include it into the code
win32:DEFINES += GIT_REVISION='\\"$$system($${GIT_BIN} rev-parse --short HEAD)\\"'
unix:DEFINES += GIT_REVISION='\\"$$system(git rev-parse --short HEAD)\\"'

SOURCES += src/main.cpp\
        src/gui/mainwindow.cpp \
    src/map/navmapwidget.cpp

HEADERS  += src/gui/mainwindow.h \
    src/map/navmapwidget.h

FORMS    += src/gui/mainwindow.ui

# Marble dependencies
win32 {
  INCLUDEPATH += $$MARBLE_BASE/include
  LIBS += -L$$MARBLE_BASE/ -llibmarblewidget-qt5d
  DEPENDPATH += $$MARBLE_BASE/include
}

unix {
  INCLUDEPATH += $$MARBLE_BASE/include
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
    README.txt

RESOURCES += \
    littlenavmap.qrc

# Create additional makefile targets to copy help files
unix {
  copydata.commands = cp -avf $$PWD/help $$OUT_PWD
  cleandata.commands = rm -Rvf $$OUT_PWD/help
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
  DEPLOY_DIR=\"$${WINPWD}\\deploy\\$${DEPLOY_DIR_NAME}\"

  CONFIG(debug, debug|release):DLL_SUFFIX=d
  CONFIG(release, debug|release):DLL_SUFFIX=

  deploy.commands = rmdir /s /q $${DEPLOY_DIR} &
  deploy.commands += mkdir $${DEPLOY_DIR} &
  deploy.commands += xcopy $${WINOUT_PWD}\\littlenavmap.exe $${DEPLOY_DIR} &&
  deploy.commands += xcopy $${WINPWD}\\CHANGELOG.txt $${DEPLOY_DIR} &&
  deploy.commands += xcopy $${WINPWD}\\README.txt $${DEPLOY_DIR} &&
  deploy.commands += xcopy $${WINPWD}\\LICENSE.txt $${DEPLOY_DIR} &&
  deploy.commands += xcopy $${QT_BIN}\\libgcc*.dll $${DEPLOY_DIR} &&
  deploy.commands += xcopy $${QT_BIN}\\libstdc*.dll $${DEPLOY_DIR} &&
  deploy.commands += xcopy $${QT_BIN}\\libwinpthread*.dll $${DEPLOY_DIR} &&
  deploy.commands += xcopy $${MARBLE_BASE}\\libmarblewidget-qt5$${DLL_SUFFIX}.dll $${DEPLOY_DIR} &&
  deploy.commands += xcopy $${MARBLE_BASE}\\libastro$${DLL_SUFFIX}.dll $${DEPLOY_DIR} &&
  deploy.commands += xcopy $${MARBLE_BASE}\\Qt5DBus$${DLL_SUFFIX}.dll $${DEPLOY_DIR} &&
  deploy.commands += xcopy $${MARBLE_BASE}\\Qt5Network$${DLL_SUFFIX}.dll $${DEPLOY_DIR} &&
  deploy.commands += xcopy $${MARBLE_BASE}\\Qt5PrintSupport$${DLL_SUFFIX}.dll $${DEPLOY_DIR} &&
  deploy.commands += xcopy $${MARBLE_BASE}\\Qt5Qml$${DLL_SUFFIX}.dll $${DEPLOY_DIR} &&
  deploy.commands += xcopy $${MARBLE_BASE}\\Qt5Quick$${DLL_SUFFIX}.dll $${DEPLOY_DIR} &&
  deploy.commands += xcopy $${MARBLE_BASE}\\Qt5Script$${DLL_SUFFIX}.dll $${DEPLOY_DIR} &&
  deploy.commands += xcopy $${MARBLE_BASE}\\Qt5WebKit$${DLL_SUFFIX}.dll $${DEPLOY_DIR} &&
  deploy.commands += xcopy $${MARBLE_BASE}\\icudt54.dll $${DEPLOY_DIR} &&
  deploy.commands += xcopy $${MARBLE_BASE}\\icuin54.dll $${DEPLOY_DIR} &&
  deploy.commands += xcopy $${MARBLE_BASE}\\icuuc54.dll $${DEPLOY_DIR} &&
  deploy.commands += xcopy $${MARBLE_BASE}\\Qt5Positioning$${DLL_SUFFIX}.dll $${DEPLOY_DIR} &&
  deploy.commands += xcopy $${MARBLE_BASE}\\Qt5Multimedia$${DLL_SUFFIX}.dll $${DEPLOY_DIR} &&
  deploy.commands += xcopy $${MARBLE_BASE}\\Qt5Sensors$${DLL_SUFFIX}.dll $${DEPLOY_DIR} &&
  deploy.commands += xcopy $${MARBLE_BASE}\\Qt5WebChannel$${DLL_SUFFIX}.dll $${DEPLOY_DIR} &&
  deploy.commands += xcopy $${MARBLE_BASE}\\Qt5WebKitWidgets$${DLL_SUFFIX}.dll $${DEPLOY_DIR} &&
  deploy.commands += xcopy $${MARBLE_BASE}\\Qt5OpenGL$${DLL_SUFFIX}.dll $${DEPLOY_DIR} &&
  deploy.commands += xcopy $${MARBLE_BASE}\\Qt5MultimediaWidgets$${DLL_SUFFIX}.dll $${DEPLOY_DIR} &&
  deploy.commands += xcopy $${MARBLE_BASE}\\Qt5Xml$${DLL_SUFFIX}.dll $${DEPLOY_DIR} &&
  deploy.commands += xcopy /i /s /e /f /y $${MARBLE_BASE}\\data $${DEPLOY_DIR}\\data &&
  deploy.commands += xcopy /i /s /e /f /y $${MARBLE_BASE}\\plugins $${DEPLOY_DIR}\\plugins &&
  deploy.commands += $${QT_BIN}\\windeployqt --compiler-runtime $${DEPLOY_DIR}
}

QMAKE_EXTRA_TARGETS += deploy

first.depends = $(first) copydata
QMAKE_EXTRA_TARGETS += first copydata

clean.depends = $(clean) cleandata
QMAKE_EXTRA_TARGETS += clean cleandata

