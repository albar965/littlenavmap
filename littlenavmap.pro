#-------------------------------------------------
#
# Project created by QtCreator 2016-02-09T16:39:58
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

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

SOURCES += main.cpp\
        mainwindow.cpp

HEADERS  += mainwindow.h

FORMS    += mainwindow.ui

win32:CONFIG(release, debug|release): LIBS += -L$$MARBLE_BASE/lib/release/ -lmarblewidget-qt5
else:win32:CONFIG(debug, debug|release): LIBS += -L$$MARBLE_BASE/lib/debug/ -lmarblewidget-qt5
else:unix: LIBS += -L$$MARBLE_BASE/lib/ -lmarblewidget-qt5
INCLUDEPATH += $$MARBLE_BASE/include
DEPENDPATH += $$MARBLE_BASE/include

DISTFILES += \
    uncrustify.cfg
