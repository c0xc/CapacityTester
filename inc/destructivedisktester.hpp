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

#ifndef DESTRUCTIVEDISKTESTER_HPP
#define DESTRUCTIVEDISKTESTER_HPP

#include <cassert>
#include <fcntl.h> //O_RDONLY
#include <linux/fs.h> //BLKGETSIZE64
#include <sys/ioctl.h>
#include <unistd.h> //close()
#include <sys/stat.h>
#include <sys/types.h>

#include <QDebug>
#include <QObject>
#include <QVariant>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QStorageInfo>

//no NO_UDISK
//#include "udiskmanager.hpp"

/*! \class DestructiveDiskTester
 *
 * \brief The DestructiveDiskTester class provides an interface for
 * testing an unmounted USB storage disk.
 *
 * Unlike VolumeTester, which works on the filesystem on the partition
 * of a USB drive (volume refers to a mounted filesystem),
 * this class works on the USB storage drive itself.
 * It tries to detect the real capacity of a fake drive faster
 * than the volume test, which can take days with some huge fake drives.
 *
 *
 *
 */

class DestructiveDiskTester : public QObject
{
    Q_OBJECT

signals:

    void
    started(qint64 total);

    void
    startFailed();

    void
    written(qint64 bytes, double avg_speed = 0);

    void
    verified(qint64 bytes, double avg_speed = 0);

    void
    writeFailed(qint64 start);

    void
    verifyFailed(qint64 start);

    void
    finished(bool success = false);

public:

    static const int
    KB = 1024;

    static const int
    MB = 1024 * KB;

    static qint64
    getCapacity(int fd);

    static qint64
    getCapacity(const QString &dev_path);

    static int
    getBlockSize(int fd);

    static int
    getInum(int fd);

    static int
    getInum(const QString &dev_path);

    static bool
    isWritable(const QString &dev_path);

    static bool
    amIRoot();

    DestructiveDiskTester(const QString &dev_path);

    ~DestructiveDiskTester();

    //NOTE this routine is NOT 100% reliable!
    bool
    isMounted();

    bool
    isValid();

public slots:

    void
    start();

    void
    cancel();

    bool
    runGBSteps();

    bool
    runGBSteps(int phase);

private:

    QByteArray
    generateTestPattern(int pattern_size);

    QByteArray
    blockPattern(int block);

    bool
    openFD();

    bool
    closeFD();

    bool
    reopen();

    bool
    seekFD(qint64 offset);

    QByteArray
    readData(int size);

    bool
    writeData(const QByteArray &data);

    QString
    m_dev_path;

    int
    m_fd;

    int
    m_dev_block_size;

    int
    m_old_inum;

    char
    *m_block_buffer;

    qint64
    m_size;
    
    bool
    m_stop_req;

    bool
    m_close_reopen;

    bool
    m_use_fsync;

    QByteArray
    m_test_pattern_0;

    QByteArray
    m_test_pattern_n;

    //QElapsedTimer
    //m_tmr;



};

#endif
