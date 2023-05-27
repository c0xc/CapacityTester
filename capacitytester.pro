DEFINES += PROGRAM=\\\"CapacityTester\\\"

TARGET = capacity-tester
DESTDIR = bin/
OBJECTS_DIR = obj/
INCLUDEPATH = inc/
HEADERS = inc/*
SOURCES = src/*
QT += widgets

!win32 {
    QT += dbus
}

win32 {
    LIBS += -lsetupapi
}

RESOURCES += res/lang.qrc

# missing return statement should be fatal
QMAKE_CXXFLAGS += -Werror=return-type
QMAKE_CXXFLAGS += -std=c++11
CONFIG += lrelease embed_translations

target.path = /usr/bin
INSTALLS += target

#!versionAtLeast(QT_VERSION, 5.15.0) {
#    message("Cannot use Qt $${QT_VERSION}")
#    error("Use Qt 5.15 or newer")
#}

TRANSLATIONS = \
languages/capacitytester_en.ts \
languages/capacitytester_de.ts \
languages/capacitytester_ru.ts \
languages/capacitytester_es.ts \
languages/capacitytester_pt_br.ts

