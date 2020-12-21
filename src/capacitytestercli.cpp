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
                   out(stdout),
                   err(stderr),
                   in(stdin),
                   is_yes(false),
                   safety_buffer(-1),
                   total_mb(0)
{
    //Heading
    out << "CapacityTester" << endl
        << "==============" << endl
        << endl;

    //Command line argument parser
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.setSingleDashWordOptionMode(
        QCommandLineParser::ParseAsLongOptions);

    //Declare basic arguments
    parser.addOption(QCommandLineOption(QStringList() << "l" << "list",
        tr("Lists available volumes.")));
    parser.addOption(QCommandLineOption(QStringList() << "i" << "info",
        tr("Shows volume information.")));
    parser.addOption(QCommandLineOption(QStringList() << "t" << "test",
        tr("Starts volume test.")));
    parser.addOption(QCommandLineOption(QStringList() << "y" << "yes",
        tr("Answers questions with yes.")));
    parser.addOption(QCommandLineOption(QStringList() << "safety-buffer",
        tr("Changes the size of the safety buffer zone."),
        "safety-buffer"));
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
        yn = in.readLine().toLower();

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
    //Mounted filesystems
    QStringList mountpoints;
    foreach (QString mountpoint, VolumeTester::availableMountpoints())
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
        out << tr(
            "%1 old test file(s) have been found.\n"
            "Cannot test with these files present, please delete them!").
            arg(conflict_files.count())
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
        out << tr(
            "%1 file(s) have been found.\n"
            "The volume should be completely empty.").
            arg(root_files.count())
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
            out << tr(
                "The volume is not empty: %1 file(s) have been found.\n"
                "You should delete all those files first.\n"
                "Are you really sure you want to continue?").
                arg(root_files.count())
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
    out << "Starting volume test... " << flush;

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
    total_mb = total / VolumeTester::MB;

    out << endl;
    out << "Initializing...\t";
    out << QString(4, 32);
    out << flush;

}

void
CapacityTesterCli::initialized(qint64 bytes, double avg_speed)
{
    Q_UNUSED(avg_speed);

    //Initialized MB (progress)
    int initialized_mb = bytes / VolumeTester::MB;
    int p = ((double)initialized_mb / total_mb) * 100;

    //Print progress
    QString str_p = QString("%1%").arg(p).rightJustified(4);
    out << QString(4, 8);
    out << str_p;
    out << flush;

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
    out << flush;

}

void
CapacityTesterCli::verifyStarted()
{
    out << endl;
    out << "Verifying...\t";
    out << QString(4, 32); //100%
    out << QString(1, 32);
    str_verify_speed.clear();
    out << flush;

}

void
CapacityTesterCli::written(qint64 written, double avg_speed)
{
    //MB
    int written_mb = written / VolumeTester::MB;
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
    out << flush;

}

void
CapacityTesterCli::verified(qint64 read, double avg_speed)
{
    //MB
    int read_mb = read / VolumeTester::MB;
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
    out << flush;

}

