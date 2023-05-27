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

#ifndef CAPACITYTESTERCLI_HPP
#define CAPACITYTESTERCLI_HPP

#include <cassert>
#include <iostream>

#include <QCoreApplication>
#include <QTimer>
#include <QTextStream>
#include <QCommandLineParser>
#include <QSignalMapper>
#include <QThread>
#if !defined(Q_OS_WIN)
#include <QSocketNotifier>
#else
#include <QWinEventNotifier>
#endif

#ifndef NO_UDISK
#include "udiskmanager.hpp"
#endif

#include "size.hpp"
#include "volumetester.hpp"
#include "destructivedisktester.hpp"

class CapacityTesterCli : public QObject
{
    Q_OBJECT

signals:

public:

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    static constexpr const auto endl = Qt::endl;
#endif

    CapacityTesterCli(QObject *parent = 0);

private:

    QCoreApplication
    &app;

    QTextStream
    out;

    QTextStream
    err;

    //QTextStream
    //in;
    QFile
    *in_file;

    bool
    is_yes;

    int
    safety_buffer;

    QPointer<VolumeTester>
    worker;

    qint64
    total_mb;

    QElapsedTimer
    tmr_total_test_time;

    QString
    str_write_speed;

    QString
    str_verify_speed;

    QPointer<QObject>
    stdin_notifier;

private slots:

    void
    exit(int code);

    void
    close(int code, int delay = 0);

    void
    close();

    void
    watchStdinSignal();

    void
    readStdinSignal();

    void
    checkStdinSignal();

    bool
    confirm();

    void
    showVolumeList();

    void
    showVolumeInfo(const QString &mountpoint);

    void
    startVolumeTest(const QString &mountpoint);

    bool
    stopVolumeTest();

    void
    succeededVolumeTest();

    void
    failedVolumeTest(int error_type);

    void
    completedVolumeTest(bool success, int error_type);

    void
    initializationStarted(qint64 total);

    void
    initialized(qint64 size, double avg_speed);

    void
    writeStarted();

    void
    verifyStarted();

    void
    written(qint64 written, double avg_speed);

    void
    verified(qint64 read, double avg_speed);

    void
    startDiskTest(const QString &device);

    void
    startedDiskTest(qint64 total);

    void
    diskWritten(qint64 size, double avg);

    void
    diskWriteFailed(qint64 size);

    void
    diskVerified(qint64 size, double avg);

    void
    diskVerifyFailed(qint64 size);

    void
    completedDiskTest(bool success);

};

#endif
