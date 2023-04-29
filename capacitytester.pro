TARGET = capacity-tester
DESTDIR = bin/
OBJECTS_DIR = obj/
INCLUDEPATH = inc/
HEADERS = inc/*
SOURCES = src/*
QT += widgets
QT += dbus

DEFINES += PROGRAM=\\\"CapacityTester\\\"

# missing return statement should be fatal
QMAKE_CXXFLAGS += -Werror=return-type

CONFIG += lrelease embed_translations
RESOURCES += res/lang.qrc

#LIBS += -lparted

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
languages/capacitytester_es.ts

