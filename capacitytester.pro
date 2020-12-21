TARGET = CapacityTester
DESTDIR = bin/
OBJECTS_DIR = obj/
INCLUDEPATH = inc/
HEADERS = inc/*
SOURCES = src/*
QT += widgets

DEFINES += PROGRAM=\\\"CapacityTester\\\"

CONFIG += lrelease embed_translations
RESOURCES += res/lang.qrc

TRANSLATIONS = \
languages/capacitytester_de.ts

