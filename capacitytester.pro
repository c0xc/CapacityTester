DEFINES += PROGRAM=\\\"CapacityTester\\\"

TARGET = capacity-tester
DESTDIR = bin/
OBJECTS_DIR = obj/
INCLUDEPATH = inc/
HEADERS = inc/*
SOURCES = src/*
QT += widgets
QT += concurrent

!win32 {
    QT += dbus
}

win32 {
    LIBS += -lsetupapi
}

# udev + libusb (optional)
QT_CONFIG -= no-pkg-config
CONFIG += link_pkgconfig
!win32 {
    LIBS += -ludev
    DEFINES += HAVE_LIBUDEV
}
# libusb1, libusb0
# LIBS += -lusb (or SUBLIBS=-lusb make)
isEmpty(NO_LIBUSB) {
    !win32 {
        PKGCONFIG += libusb-1.0 libusb

        #contains(PKGCONFIG, libusb-1.09) {
        contains(PKGCONFIG, libusb-1.0) {
            LIBS += -lusb-1.0
            DEFINES += HAVE_LIBUSB1
        } else:contains(PKGCONFIG, libusb) {
            LIBS += -lusb
            DEFINES += HAVE_LIBUSB0
        }
    }
}

RESOURCES += res/res.qrc
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

