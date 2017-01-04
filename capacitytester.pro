TARGET = CapacityTester
DESTDIR = bin/
OBJECTS_DIR = obj/
INCLUDEPATH = inc/
#HEADERS += inc/main.hpp \
#           inc/size.hpp \
#           inc/capacitytestercli.hpp \
#           inc/capacitytestergui.hpp \
#           inc/volumetester.hpp
#SOURCES += src/main.cpp \
#           src/size.cpp \
#           src/capacitytestercli.cpp \
#           src/capacitytestergui.cpp \
#           src/volumetester.cpp
HEADERS = inc/*
SOURCES = src/*
QT += widgets

DEFINES += PROGRAM=\\\"CapacityTester\\\"

