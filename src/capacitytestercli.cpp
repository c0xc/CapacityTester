/****************************************************************************
**
** Copyright (C) 2016 Philip Seeger
** This file is part of CapacityTester.
**
** CapacityTester is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** CapacityTester is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with CapacityTester. If not, see <http://www.gnu.org/licenses/>.
**
****************************************************************************/

#include "capacitytestercli.hpp"

CapacityTesterCli::CapacityTesterCli(QObject *parent)
                 : QObject(parent),
                   app(*qApp),
                   out(stdout, QIODevice::WriteOnly | QIODevice::Unbuffered | QIODevice::Text),
                   err(stderr, QIODevice::WriteOnly | QIODevice::Unbuffered | QIODevice::Text),
                   is_yes(false),
                   safety_buffer(-1),
                   total_mb(0),
                   full_ddt_mode(false)
{
    //Heading
    out << "CapacityTester" << endl
        << "==============" << endl
        << endl;

    //Custom signal handler
    //stdin - also used for custom signal, see watchStdinSignal()
    in_file = new QFile(this);
    in_file->open(stdin, QIODevice::ReadOnly | QIODevice::Text);

    //Command line argument parser
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.setSingleDashWordOptionMode(
        QCommandLineParser::ParseAsLongOptions);

    //Declare basic arguments
    //IDEA alternatively, list partitions (libparted) if running as root
    parser.addOption(QCommandLineOption(QStringList() << "l" << "list",
        tr("Lists available volumes.")));
    parser.addOption(QCommandLineOption(QStringList() << "i" << "info",
        tr("Shows volume information.")));
    parser.addOption(QCommandLineOption(QStringList() << "t" << "test",
        tr("Starts volume test.")));
    parser.addOption(QCommandLineOption(QStringList() << "y" << "yes",
        tr("Answers questions with yes.")));
    parser.addOption(QCommandLineOption(QStringList() << "destructive-test",
        tr("Run destructive test (+ -device).")));
    parser.addOption(QCommandLineOption(QStringList() << "full-test-mode",
        tr("Set disk test mode to full instead of fast mode.")));
    parser.addOption(QCommandLineOption(QStringList() << "safety-buffer",
        tr("Changes the size of the safety buffer zone."), "safety-buffer"));
    parser.addOption(QCommandLineOption(QStringList() << "device",
        tr("Path to device to be tested (disk test)."), "device"));
    parser.addOption(QCommandLineOption(QStringList() << "mountpoint",
        tr("Volume to be tested."), "mountpoint"));
    parser.addPositionalArgument("mountpoint",
        tr("Volume to be tested."), "[mountpoint]");

    //Parse arguments
    parser.process(app);

    //Abort if too many positional arguments
    const QStringList args = parser.positionalArguments();
    QString mountpoint;
    if (!args.isEmpty()) mountpoint = args.at(0);
    if (args.size() > 1)
    {
        err << "Too many arguments." << endl;
        close(1);
        return;
    }
    if (parser.isSet("mountpoint"))
        mountpoint = parser.value("mountpoint");
    QString device;
    if (parser.isSet("device"))
        device = parser.value("device");

    //Custom safety buffer
    QString str_safety_buffer = parser.value("safety-buffer");
    if (!str_safety_buffer.isEmpty())
    {
        bool ok;
        int number = str_safety_buffer.toInt(&ok);
        if (ok) safety_buffer = number;
    }

    //Answer with yes
    if (parser.isSet("yes"))
    {
        is_yes = true;
    }
    //Options
    if (parser.isSet("full-test-mode"))
    {
        full_ddt_mode = true;
    }

    //Run command
    if (parser.isSet("list"))
    {
        showVolumeList();
        close();
    }
    else if (parser.isSet("info"))
    {
        showVolumeInfo(mountpoint);
        close();
    }
    else if (parser.isSet("test"))
    {
        startVolumeTest(mountpoint);
    }
    else if (parser.isSet("destructive-test"))
    {
        startDiskTest(device);
    }
    else
    {
        parser.showHelp();
    }

}

void
CapacityTesterCli::exit(int code)
{
    //Quit immediately
    app.exit(code); //QCoreApplication::exit()
}

void
CapacityTesterCli::close(int code, int delay)
{
    QTimer *timer = new QTimer;
    timer->setSingleShot(true);
    QSignalMapper *mapper = new QSignalMapper(timer); //deleted with timer
    mapper->setMapping(timer, code);
    connect(timer,
            SIGNAL(timeout()),
            mapper,
            SLOT(map()));           //timer -> mapper
    connect(mapper,
            SIGNAL(mapped(int)),
            this,
            SLOT(exit(int)));       //mapper -> exit()
    connect(timer,
            SIGNAL(timeout()),
            timer,
            SLOT(deleteLater()));   //timer deletes itself (and mapper)
    timer->start(delay);
}

void
CapacityTesterCli::close()
{
    //Close (success)
    close(0);
}

void
CapacityTesterCli::watchStdinSignal()
{
    //Watch stdin, handle custom signal
    //We used to use QTextStream(stdin) which worked great to read lines
    //but only on demand, it does not have canReadLine()
    //and it does not have a readLine() with a timeout either (it would block).
    //QFile has canReadLine(), but that does not work with stdin.
    //So we try to use QSocketNotifier ... it may not work on Windows
    //
    //It's unbelievable how difficult it is in Qt to do the most basic
    //file operation - conditionally reading from stdin...
    //
    //We read custom (text) signals from stdin instead of using signals
    //because this may run as a subprocess as root
    //so the caller (the GUI) would not be allowed to send signals to it.

    if (!stdin_notifier)
    {

#if !defined(Q_OS_WIN)

        stdin_notifier = new QSocketNotifier(0, QSocketNotifier::Read, this);

        // https://doc.qt.io/qt-5/qsocketnotifier.html#activated
        connect(stdin_notifier, SIGNAL(activated(int)), SLOT(readStdinSignal()));

#else

        //Windows routine - experimental (I don't have a Windows vm running now)
        //the Internet says: QWinEventNotifier(GetStdHandle(STD_INPUT_HANDLE));
        stdin_notifier = new QWinEventNotifier(GetStdHandle(STD_INPUT_HANDLE), this);
        connect(stdin_notifier, SIGNAL(activated(int)), SLOT(readStdinSignal()));

#endif

    }
}

void
CapacityTesterCli::readStdinSignal()
{
    QString line = in_file->readLine().trimmed();

    if (line.startsWith("QUIT"))
    {
        out << "ABORT - canceled..." << endl;
        close();
    }
}

void
CapacityTesterCli::checkStdinSignal()
{
    //Try to check if anything is available / waiting in stdin
    //class QTextStream’ has no member named ‘canReadLine’
    //QTextStream cannot be used to wait for input asynchronously
    //QFile has canReadLine() but it does not work with stdin
    //QSocketNotifier can be used (::Read), but it may not work on Windows
    if (!in_file->canReadLine()) return;
    //again, this does not work, the code is left here so we don't try it again
    //see above for the routine using the socket notifier

    readStdinSignal();
}

bool
CapacityTesterCli::confirm()
{
    if (is_yes)
    {
        out << "yes" << endl;
        return true;
    }

    QString yn;
    do
    {
        yn = in_file->readLine().trimmed().toLower();

        if (yn == "yes")
            return true;
        else if (yn == "no")
            return false;
        else
            yn.clear();

        out << "yes or no?" << endl;
    }
    while (yn.isEmpty());

    return false;
}

void
CapacityTesterCli::showVolumeList()
{
    QStringList mountpoints;
    bool fetched_list = false;
#ifdef UDISKMANAGER_HPP
    //Mounted USB devices
    UDiskManager manager;
    if (manager.isValid())
    {
        foreach (QString device, manager.usbPartitions())
        {
            QString mountpoint = manager.mountpoint(device);
            if (!mountpoint.isEmpty())
                mountpoints << mountpoint;
        }
        fetched_list = true;
    }
#endif

    //All mounted filesystems (fallback)
    if (!fetched_list)
        mountpoints = VolumeTester::availableMountpoints();
    foreach (QString mountpoint, mountpoints)
    {
        //Gather information
        VolumeTester tester(mountpoint);
        if (!tester.isValid()) continue;
        QString label = tester.label();
        Size capacity = tester.bytesTotal();

        //Add to list
        mountpoints << mountpoint;

        //Print
        QString str_size =
            capacity.formatted(Size::Condensed).leftJustified(5);
        out << "* " << str_size << "\t" << label << endl;

    }

    //Nothing
    if (mountpoints.isEmpty())
    {
        out << "No mountpoints found." << endl;
    }

}

void
CapacityTesterCli::showVolumeInfo(const QString &mountpoint)
{
    //Volume
    VolumeTester tester(mountpoint);
    if (!tester.isValid())
    {
        err << "The specified volume is not valid." << endl;
        return close(1);
    }

    //Print volume information
    out << "Volume:\t\t" << mountpoint << endl;
    out << endl;
    Size capacity = tester.bytesTotal();
    Size used = tester.bytesUsed();
    int used_percentage =
        capacity ? ((double)used / capacity) * 100 : 0;
    Size available = tester.bytesAvailable();
    int available_percentage =
        capacity ? ((double)available / capacity) * 100 : 0;
    out << tr("Capacity:") << "\t"
        << capacity.formatted()
        << " / "
        << capacity.toLongLong() << " B"
        << endl;
    out << tr("Used:") << "\t\t"
        << used.formatted()
        << " / "
        << used.toLongLong() << " B"
        << " / "
        << used_percentage << "%"
        << endl;
    out << tr("Available:") << "\t"
        << available.formatted()
        << " / "
        << available.toLongLong() << " B"
        << " / "
        << available_percentage << "%"
        << endl;
    out << endl;

    //Check for old test files that have not been removed (crash?)
    //Cannot test if those are present
    QStringList conflict_files = tester.conflictFiles();
    if (!conflict_files.isEmpty())
    {
        int n = conflict_files.count();
        out << tr(
            "%n old test file(s) have been found.\n"
            "Cannot test with these files present, please delete them!", "", n)
            << endl;
        foreach (QString file, conflict_files)
        {
            out << "!\t" << file << endl;
        }
        out << endl;
    }

    //Check for files in selected filesystem (should be empty)
    QStringList root_files = tester.rootFiles();
    if (!root_files.isEmpty())
    {
        int n = root_files.count();
        out << tr(
            "%n file(s) have been found.\n"
            "The volume should be completely empty.", "", n)
            << endl;
        foreach (QString file, root_files)
        {
            out << "X\t" << file << endl;
        }
        out << endl;
    }

}

void
CapacityTesterCli::startVolumeTest(const QString &mountpoint)
{
    //Volume
    VolumeTester tester(mountpoint);
    tester.setSafetyBuffer(safety_buffer); //for calculation only
    if (!tester.isValid())
    {
        err << "The specified volume is not valid." << endl;
        return close(1);
    }

    //Check if full
    if (!tester.bytesAvailable())
    {
        //Volume full or quota exhausted
        err << tr("The selected volume is full.") << endl;
        return close(1);
    }

    //Ask again if volume not empty
    {
        QStringList root_files = tester.rootFiles();
        if (!root_files.isEmpty())
        {
            int n = root_files.count();
            out << tr(
                "The volume is not empty: %n file(s) have been found.\n"
                "You should delete all those files first.\n"
                "Are you really sure you want to continue?", "", n)
                << endl;
            if (!confirm()) return close(2);
        }
    }

    //Worker
    worker = new VolumeTester(mountpoint);
    worker->setSafetyBuffer(safety_buffer);

    //Thread for worker
    QThread *thread = new QThread;
    worker->moveToThread(thread);

    //Start worker when thread starts
    connect(thread,
            SIGNAL(started()),
            worker,
            SLOT(start()));

    //Initialization started
    connect(worker,
            SIGNAL(initializationStarted(qint64)),
            this,
            SLOT(initializationStarted(qint64)));

    //Block initialized
    connect(worker,
            SIGNAL(initialized(qint64, double)),
            this,
            SLOT(initialized(qint64, double)));

    //Written
    connect(worker,
            SIGNAL(written(qint64, double)),
            this,
            SLOT(written(qint64, double)));

    //Verified
    connect(worker,
            SIGNAL(verified(qint64, double)),
            this,
            SLOT(verified(qint64, double)));

    //Write started
    connect(worker,
            SIGNAL(writeStarted()),
            this,
            SLOT(writeStarted()));

    //Verify started
    connect(worker,
            SIGNAL(verifyStarted()),
            this,
            SLOT(verifyStarted()));

    //Test failed handler (after write/verify failed, with delay)
    connect(worker,
            SIGNAL(failed(int)),
            this,
            SLOT(failedVolumeTest(int)));

    //Test succeeded handler
    connect(worker,
            SIGNAL(succeeded()),
            this,
            SLOT(succeededVolumeTest()));

    //Test completed handler (successful or not)
    connect(worker,
            SIGNAL(finished(bool, int)),
            this,
            SLOT(completedVolumeTest(bool, int)));

    //Stop thread when worker done (stops event loop -> thread->finished())
    connect(worker,
            SIGNAL(finished()),
            thread,
            SLOT(quit()));

    //Delete worker when done
    connect(worker,
            SIGNAL(finished()),
            worker,
            SLOT(deleteLater()));

    //Delete thread when thread done (event loop stopped)
    connect(thread,
            SIGNAL(finished()),
            thread,
            SLOT(deleteLater()));

    //Get started
    out << "Starting volume test... " << endl;

    //Start test in background
    thread->start();

    //Start timer
    tmr_total_test_time.start();

}

bool
CapacityTesterCli::stopVolumeTest()
{
    //No action if no test running
    if (!worker) return false;

    //Set stop flag
    worker->cancel();

    return true; //stop requested
}

void
CapacityTesterCli::succeededVolumeTest()
{
}

void
CapacityTesterCli::failedVolumeTest(int error_type)
{
    Q_UNUSED(error_type);
}

void
CapacityTesterCli::completedVolumeTest(bool success, int error_type)
{
    //Result
    out << endl;
    out << endl;
    if (success)
    {
        out << tr("Test completed successfully, no errors found.") << endl;
    }
    else
    {
        bool probably_bad = true;
        if (error_type & VolumeTester::Error::Create ||
            error_type & VolumeTester::Error::Permissions)
            probably_bad = false;
        QString comment = probably_bad ?
            tr("The volume might be bad.") :
            tr("An error has occurred.");
        if (error_type & VolumeTester::Error::Create)
            comment += "\n" + tr("Error creating test file.");
        if (error_type & VolumeTester::Error::Permissions)
            comment += "\n" + tr("Permission denied.");
        if (error_type & VolumeTester::Error::Resize)
            comment += "\n" + tr("Resizing test file failed.");
        if (error_type & VolumeTester::Error::Write)
            comment += "\n" + tr("Write failed.");
        if (error_type & VolumeTester::Error::Verify)
            comment += "\n" + tr("Verification failed.");
        out << tr("Test failed.") << "\n" << comment << endl;
    }

    //Time
    out << endl;
    qint64 total_seconds = tmr_total_test_time.elapsed() / 1000;
    qlonglong elapsed_minutes = total_seconds / 60;
    qlonglong elapsed_seconds = total_seconds % 60;
    QString str_m_s = QString("%1:%2").
        arg(elapsed_minutes, 2, 10, QChar('0')).
        arg(elapsed_seconds, 2, 10, QChar('0'));
    out << "Time:\t\t" << str_m_s << endl;

    if (success)
        close(); //success (code 0)
    else
        close(9); //error
}

void
CapacityTesterCli::initializationStarted(qint64 total)
{
    total_mb = total;

    out << endl;
    out << "Initializing...\t";
    out << QString(4, 32);
    out << endl;

}

void
CapacityTesterCli::initialized(qint64 size, double avg_speed)
{
    Q_UNUSED(avg_speed);

    //Initialized MB (progress)
    int initialized_mb = size;
    int p = ((double)initialized_mb / total_mb) * 100;

    //Print progress
    QString str_p = QString("%1%").arg(p).rightJustified(4);
    out << QString(4, 8);
    out << str_p;
    out << endl;

}

void
CapacityTesterCli::writeStarted()
{
    out << endl;
    out << "Writing...\t";
    out << QString(1, 32);
    out << "...";
    out << QString(1, 32);
    str_write_speed.clear();
    out << endl;

}

void
CapacityTesterCli::verifyStarted()
{
    out << endl;
    out << "Verifying...\t";
    out << QString(4, 32); //100%
    out << QString(1, 32);
    str_verify_speed.clear();
    out << endl;

}

void
CapacityTesterCli::written(qint64 written, double avg_speed)
{
    //MB
    int written_mb = written;
    int p = ((double)written_mb / total_mb) * 100;

    //Print progress
    QString str_p = QString("%1%").arg(p).rightJustified(4);
    QString str_avg =
        QString("%1 MB/s").
        arg((int)avg_speed);
    out << QString(4, 8);
    out << QString(1 + str_write_speed.size(), 8);
    str_write_speed = str_avg;
    out << str_p;
    out << " " << str_avg;
    out << endl;

}

void
CapacityTesterCli::verified(qint64 read, double avg_speed)
{
    //MB
    int read_mb = read;
    int p = ((double)read_mb / total_mb) * 100;

    //Print progress
    QString str_p = QString("%1%").arg(p).rightJustified(4);
    QString str_avg =
        QString("%1 MB/s").
        arg((int)avg_speed);
    out << QString(4, 8);
    out << QString(1 + str_verify_speed.size(), 8);
    str_verify_speed = str_avg;
    out << str_p;
    out << " " << str_avg;
    out << endl;

}

void
CapacityTesterCli::startDiskTest(const QString &device)
{
    //Check device
    DestructiveDiskTester tester(device);
    if (!tester.isValid())
    {
        err << "The specified device is not valid." << endl;
        return close(1);
    }
    //Abort if in use / mounted - NOTE not 100% reliable
    if (tester.isMounted())
    {
        err << "The specified device is in use." << endl;
        return close(1);
    }
    //bool is_usb = udisk.isUsbDevice(dev); //TODO

    //Warn user
    out << "This program will overwrite this device, destroying all data on it: " + device << endl;
    out << "In doubt, press CTRL+C to abort NOW..." << endl;
    QThread::sleep(5);
    out << endl;

    //---
    //Test starts here
    watchStdinSignal(); //handle quit signal

    //Initialize worker
    QPointer<DestructiveDiskTester> worker;
    worker = new DestructiveDiskTester(device);
    if (full_ddt_mode) worker->setFullMode();

    //Thread for worker
    QThread *thread = new QThread;
    worker->moveToThread(thread);

    //Start worker when thread starts
    connect(thread,
            SIGNAL(started()),
            worker,
            SLOT(start()));

    //Initialization started
    connect(worker,
            SIGNAL(started(qint64)),
            this,
            SLOT(startedDiskTest(qint64)));

    //Written
    connect(worker,
            SIGNAL(written(qint64, double)),
            this,
            SLOT(diskWritten(qint64, double)));
    connect(worker,
            SIGNAL(writeFailed(qint64)),
            this,
            SLOT(diskWriteFailed(qint64)));

    //Verified
    connect(worker,
            SIGNAL(verified(qint64, double)),
            this,
            SLOT(diskVerified(qint64, double)));
    connect(worker,
            SIGNAL(verifyFailed(qint64)),
            this,
            SLOT(diskVerifyFailed(qint64)));

    //Test completed handler (successful or not)
    connect(worker,
            SIGNAL(finished(bool)),
            this,
            SLOT(completedDiskTest(bool)));

    //Stop thread when worker done (stops event loop -> thread->finished())
    connect(worker,
            SIGNAL(finished()),
            thread,
            SLOT(quit()));
    //make sure thread->quit() is called even on error
    connect(worker,
            SIGNAL(destroyed()),
            thread,
            SLOT(quit()));

    //TODO make sure worker is destroyed on error
    //connect(worker, &QObject::destroyed, this, [this]()
    //{
    //    //worker destroyed
    //});

    //Delete worker when done
    //This signal can be connected to QObject::deleteLater(), to free objects in that thread.
    //https://doc.qt.io/qt-5/qthread.html#finished
    //connect(worker,
    //        SIGNAL(finished()),
    //        worker,
    //        SLOT(deleteLater()));
    //TODO it appears this is not called in some cases
    connect(thread,
            SIGNAL(finished()),
            worker,
            SLOT(deleteLater())); //delete worker after loop

    //Delete thread when thread done (event loop stopped)
    connect(worker,
            SIGNAL(destroyed()),
            thread,
            SLOT(deleteLater())); //delete thread after worker

    //Stop thread and delete worker when this CLI program is closed
    //Note that this process may be running as another user (root),
    //so it may not be possible for the client (GUI) to kill it.
    //So instead, a quit command is sent via stdin (on GUI stop/close).
    //see watchStdinSignal()
    connect(this,
            SIGNAL(destroyed()),
            worker,
            SLOT(deleteLater()));

    //Get started
    out << "Starting disk test... " << endl;

    //Start test in background
    thread->start();

    //Start timer
    tmr_total_test_time.start();

}

void
CapacityTesterCli::startedDiskTest(qint64 total)
{
    total_mb = total;

    out << QString("[start] total=%1M").arg(total_mb);
    out << endl;

}

void
CapacityTesterCli::diskWritten(qint64 size, double avg)
{
    qint64 progress_mb = size;
    double percentage = std::div(progress_mb * 100, total_mb).quot;

    //Print progress
    //this format will be parsed by the DestructiveDiskTesterWrapper
    QString str_p = QString("%1%").arg(percentage, 0, 'f', 2);
    out << QString("[write] progress: %1; @%2M; %3M/s avg").arg(str_p).arg(progress_mb).arg(avg, 0, 'f', 2); //NOTE do not translate this
    out << endl;

}

void
CapacityTesterCli::diskWriteFailed(qint64 size)
{
    qint64 progress_mb = size;

    out << QString("[write] failed @%1M").arg(progress_mb);
    out << endl;
}

void
CapacityTesterCli::diskVerified(qint64 size, double avg)
{
    qint64 progress_mb = size;
    double percentage = std::div(progress_mb * 100, total_mb).quot;

    //see diskWritten()
    QString str_p = QString("%1%").arg(percentage, 0, 'f', 2);
    out << QString("[verify] progress: %1; @%2M; %3M/s avg").arg(str_p).arg(progress_mb).arg(avg, 0, 'f', 2); //NOTE do not translate this
    out << endl;

}

void
CapacityTesterCli::diskVerifyFailed(qint64 size)
{
    qint64 progress_mb = size;

    out << QString("[verify] failed @%1M").arg(progress_mb);
    out << endl;
}

void
CapacityTesterCli::completedDiskTest(bool success)
{
    if (success)
    {
        out << "[done] success";
        out << endl;

        close(); //success (code 0)
    }
    else
    {
        out << "[done] failed";
        out << endl;

        close(9); //error
    }
}

