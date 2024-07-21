/****************************************************************************
**
** Copyright (C) 2023 Philip Seeger
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

#ifndef DESTRUCTIVEDISKTESTERWRAPPER_HPP
#define DESTRUCTIVEDISKTESTERWRAPPER_HPP

#include <QDebug>
#include <QProcess>
#include <QCoreApplication>
#include <QTimer>
#include <QPointer>
#include <QFile>

#include "destructivedisktester.hpp"
#include "classlogger.hpp"

class DestructiveDiskTesterWrapper : public DestructiveDiskTester
{
    Q_OBJECT

signals:

    void
    started(qint64 total);

    void
    startFailed();

    void
    written(qint64 bytes, double avg_speed);

    void
    verified(qint64 bytes, double avg_speed);

    void
    writeFailed(qint64 start);

    void
    verifyFailed(qint64 start);

    //void
    //finished(bool success = false);
    void
    finished(int result);

    void
    gotError(const QString &err);

public:

    DestructiveDiskTesterWrapper(const QString &dev_path);

    ~DestructiveDiskTesterWrapper();

    void
    setFullMode();

public slots:

    void
    start();

    void
    cancel(bool force_quit = false);

private slots:

    void
    checkStatus(const QString &line);

    void
    checkStatus();

    void
    checkStateFinished(int rc, QProcess::ExitStatus status);

    void
    checkProcError(QProcess::ProcessError error);

private:

    QPointer<QProcess>
    m_proc;

    QString
    m_dev_path;

    bool
    m_started;

    bool
    m_finished;

    int
    m_result = -1;

    QTimer
    *m_tmr_status;

    QRegExp
    m_rx_start;

    QRegExp
    m_rx_start_err;

    QRegExp
    m_rx_progress;

    QRegExp
    m_rx_progress_err;

    QRegExp
    m_rx_finished;

    int
    m_rc;

    QString
    m_err;

};

#endif
