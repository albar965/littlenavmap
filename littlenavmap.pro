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
  MARBLE_BASE=c:\\Program Files (x86)\\Marble
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
win32:CONFIG(release, debug|release): LIBS += -L$$MARBLE_BASE/lib/release/ -lmarblewidget-qt5
else:win32:CONFIG(debug, debug|release): LIBS += -L$$MARBLE_BASE/lib/debug/ -lmarblewidget-qt5
else:unix: LIBS += -L$$MARBLE_BASE/lib/ -lmarblewidget-qt5
INCLUDEPATH += $$MARBLE_BASE/include
DEPENDPATH += $$MARBLE_BASE/include

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

# Windows specific deploy target only for release builds
win32 {
  RC_ICONS = resources/icons/navroute.ico

  # Create backslashed path
  WINPWD=$${PWD}
  WINPWD ~= s,/,\\,g
  WINOUT_PWD=$${OUT_PWD}
  WINOUT_PWD ~= s,/,\\,g
  DEPLOY_DIR_NAME=Little Navmap
  DEPLOY_DIR=\"$${WINPWD}\\deploy\\$${DEPLOY_DIR_NAME}\"

  copydata.commands = xcopy /i /s /e /f /y $${WINPWD}\\help $${WINOUT_PWD}\\help

  cleandata.commands = del /s /q $${WINOUT_PWD}\\help

  deploy.commands = rmdir /s /q $${DEPLOY_DIR} &
  deploy.commands += mkdir $${DEPLOY_DIR} &
  deploy.commands += copy $${WINOUT_PWD}\\littlenavmap.exe $${DEPLOY_DIR} &&
  deploy.commands += copy $${WINPWD}\\CHANGELOG.txt $${DEPLOY_DIR} &&
  deploy.commands += copy $${WINPWD}\\README.txt $${DEPLOY_DIR} &&
  deploy.commands += copy $${WINPWD}\\LICENSE.txt $${DEPLOY_DIR} &&
  deploy.commands += xcopy /i /s /e /f /y $${WINPWD}\\help $${DEPLOY_DIR}\\help &&
  deploy.commands += copy $${QT_BIN}\\libgcc*.dll $${DEPLOY_DIR} &&
  deploy.commands += copy $${QT_BIN}\\libstdc*.dll $${DEPLOY_DIR} &&
  deploy.commands += copy $${QT_BIN}\\libwinpthread*.dll $${DEPLOY_DIR} &&
  deploy.commands += $${QT_BIN}\\windeployqt --compiler-runtime $${DEPLOY_DIR} &&
  # Delete some unneeded files copied by windeployqt
  deploy.commands += del /s /q $${DEPLOY_DIR}\\imageformats\\qdds.dll $${DEPLOY_DIR}\\imageformats\\qjp2.dll $${DEPLOY_DIR}\\imageformats\\qtga.dll $${DEPLOY_DIR}\\imageformats\\qtiff.dll &
  deploy.commands += del /s /q $${DEPLOY_DIR}\\sqldrivers\qsqlmysql.dll $${DEPLOY_DIR}\\sqldrivers\qsqlodbc.dll $${DEPLOY_DIR}\\sqldrivers\qsqlpsql.dll
}

QMAKE_EXTRA_TARGETS += deploy

first.depends = $(first) copydata
QMAKE_EXTRA_TARGETS += first copydata

clean.depends = $(clean) cleandata
QMAKE_EXTRA_TARGETS += clean cleandata

