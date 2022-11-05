TARGET = CapacityTester
DESTDIR = bin/
OBJECTS_DIR = obj/
INCLUDEPATH = inc/
HEADERS = inc/*
SOURCES = src/*
QT += widgets
QT += dbus

DEFINES += PROGRAM=\\\"CapacityTester\\\"

CONFIG += lrelease embed_translations
RESOURCES += res/lang.qrc

#LIBS += -lparted

TRANSLATIONS = \
languages/capacitytester_en.ts \
languages/capacitytester_de.ts \
languages/capacitytester_ru.ts \
languages/capacitytester_es.ts

target.path = /opt/capacitytester
INSTALLS += target

#!versionAtLeast(QT_VERSION, 5.15.0) {
#    message("Cannot use Qt $${QT_VERSION}")
#    error("Use Qt 5.15 or newer")
#}

