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
#include <QDebug>
#include <QTimer>
#include <QTextStream>
#include <QCommandLineParser>
#include <QSignalMapper>
#include <QThread>

#include "udiskmanager.hpp"

#include "size.hpp"
#include "volumetester.hpp"

class CapacityTesterCli : public QObject
{
    Q_OBJECT

signals:

public:

    CapacityTesterCli(QObject *parent = 0);

private:

    QCoreApplication
    &app;

    QTextStream
    out;

    QTextStream
    err;

    QTextStream
    in;

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

private slots:

    void
    exit(int code);

    void
    close(int code, int delay = 0);

    void
    close();

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
    initialized(qint64 bytes, double avg_speed);

    void
    writeStarted();

    void
    verifyStarted();

    void
    written(qint64 written, double avg_speed);

    void
    verified(qint64 read, double avg_speed);

};

#endif
