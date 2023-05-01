
Build
-----

This software requires Qt 5.

Build using qmake:

    $ qmake
    $ make

Cleanup:

    $ make distclean

Build without qmake (alternative):
Make sure that the QTDIR environment variable points
to your Qt build directory.

    $ export QTDIR=~/Builds/Qt/5.5.1-GCC5.3.1-FEDORA22
    $ cd build
    $ make

Windows build:

    $ SET QTDIR=...
    $ cd build
    $ make PLATFORM=win32-g++

32-bit build:

    $ export QTDIR=... # 32-bit Qt build (-platform linux-g++-32)
    $ cd build
    $ make rebuild-32bit

Cleanup:

    $ cd build
    $ make clean



Call
----

Use the libraries in your own Qt build directory if they're not installed.
If you don't have the Qt 5 libraries installed but you have a custom Qt build:

    $ LD_LIBRARY_PATH=~/Builds/Qt/5.5.1-GCC4.7.2-DEBIAN7/qtbase/lib \
      bin/capacity-tester

Call with plugin directory explicitly set (e.g., if xcb plugin missing):

    $ export QTDIR=~/Builds/Qt/5.5.1-GCC5.3.1-FEDORA22 \
      LD_LIBRARY_PATH=$QTDIR/qtbase/lib \
      QT_PLUGIN_PATH=$QTDIR/qtbase/plugins \
      bin/capacity-tester

If the program still won't start because the xcb plugin can't be loaded,
you're probably missing a library or two.
Check the dependencies of $QTDIR/qtbase/plugins/platforms/libqxcb.so
and install the missing libraries or add them manually.

Command line mode (no gui):

    $ export LD_LIBRARY_PATH=~/Builds/Qt/5.5.1-GCC4.7.2-DEBIAN7/qtbase/lib
    $ bin/capacity-tester -platform offscreen
    $ bin/capacity-tester -platform offscreen -list



Misc
----

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



