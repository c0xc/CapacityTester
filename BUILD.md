This program should be built with and linked against Qt 5.15 (or backported/patched).
The following signal was only added in Qt 5.15, according to the documentation:

    QObject::connect: No such signal QButtonGroup::idClicked(int) in src/selectionwindow.cpp

However, I've built it with Qt 5.11.2 and it works.

Either way, to be sure, an AppImage file could be created from the binary
to include all relevant Qt libraries in the right version.
Such an AppImage file would run on almost any Linux system.

Debian 7 is not supported anymore: Debian 7 has GCC 4.7.2.

Even Qt 5.11 requires GCC >= 5:
/mnt/Media/opt/Qt/qt-everywhere-src-5.11.2/qtbase/src/corelib/global/qrandom.h:202:9: error: ‘is_trivially_destructible’ is not a member of ‘std’



