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

#ifndef VOLUMETESTER_HPP
#define VOLUMETESTER_HPP

#include <cassert>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <fcntl.h>

#if defined(_WIN32)
#include <io.h> /* _get_osfhandle */
#define WIN32_LEAN_AND_MEAN
#include <windows.h> /* FlushFileBuffers */
#include <errno.h>
#endif

#include <QDebug>
#include <QObject>
#include <QVariant>
#include <QPair>
#include <QFile>
#include <QFileInfo>
#include <QStorageInfo>
#include <QPointer>
#include <QElapsedTimer>

#define USE_FSYNC
#ifdef NO_FSYNC
#undef USE_FSYNC
#endif

#if defined(_WIN32) && !defined(NO_FSYNC)
int
fsync(int fd);
#endif

/*! \class VolumeTester
 *
 * \brief The VolumeTester class provides an interface for
 * testing a mounted filesystem.
 *
 * A VolumeTester can test the capacity of a mounted filesystem
 * by writing files to it and verifying those files.
 * The filesystem should be completely empty.
 * A filesystem that's already half full cannot be tested properly.
 *
 * The goal is to detect a fake USB thumbdrive (or memory card).
 * There are many counterfeit flash storage devices or memory cards
 * available online.
 * These claim to have a higher capacity than they really have.
 * For example, a thumbdrive sold as "16 GB USB flash drive"
 * will present itself as a storage device with a capacity of 16 GB,
 * but in reality, it might just have a 4 GB chip.
 * Writes beyond the 4 GB limit are sometimes ignored
 * without reporting an error, depending on the fake.
 * This class will fill the filesystem completely and then verify it
 * to see if the returned data is correct.
 *
 * This class works on a mounted filesystem rather than a storage device.
 * The filesystem should span the entire storage device,
 * i.e., it should be the only filesystem on that device.
 *
 * As a courtesy to the user, this tester will clean up after itself
 * and remove all test files afterwards.
 *
 */

class VolumeTester : public QObject
{
    Q_OBJECT

signals:

    void
    initializationStarted(qint64 total);

    void
    writeStarted();

    void
    verifyStarted();

    void
    initialized(qint64 size, double avg_speed);

    void
    written(qint64 bytes, double avg_speed);

    void
    verified(qint64 bytes, double avg_speed);

    void
    createFailed(int index, qint64 start);

    void
    writeFailed(qint64 start);

    void
    remountRequested(const QString &mountpoint);

    void
    verifyFailed(qint64 start);

    void
    failed(int error_type = Error::Unknown);

    void
    succeeded();

    void
    removeFailed(const QString &path);

    void
    finished(bool success = false, int error_type = Error::Unknown);

public:

    struct Error
    {
        enum Type
        {
            Unknown         = 0,
            Aborted         = 1 << 0,
            Full            = 1 << 1,
            Create          = 1 << 2,
            Permissions     = 1 << 3,
            Resize          = 1 << 4,
            Write           = 1 << 5,
            Verify          = 1 << 7,
        };
    };

    static const int
    KB = 1024;

    static const int
    MB = 1024 * KB;

    static bool
    isValid(const QString &mountpoint);

    static QStringList
    availableMountpoints();

    VolumeTester(const QString &mountpoint);

    bool
    setSafetyBuffer(int new_buffer);

    void
    setReqRemount(bool enabled);

    bool
    isValid() const;

    QString
    mountpoint() const;

    qint64
    bytesTotal() const;

    qint64
    bytesUsed() const;

    qint64
    bytesAvailable() const;

    int
    usedPercentagePoints() const;

    QString
    name() const;

    QString
    label() const;

    QFileInfoList
    entryInfoList() const;

    QStringList
    rootFiles() const;

    QStringList
    conflictFiles() const;

public slots:

    void
    start();

    void
    cancel();

    void
    handleRemounted(const QString &new_mountpoint);

private slots:

    void
    runVerifyStep();

    bool
    initialize();

    bool
    writeFull();

    bool
    verifyFull();

    void
    generateTestPattern();

    void
    removeFile(QObject *file);

private:

    struct BlockInfo
    {
        qint64
        rel_offset;

        qint64
        abs_offset;

        qint64
        abs_offset_mb;

        int
        size;

        qint64
        rel_end;

        qint64
        abs_end;

        qint64
        abs_end_mb;

        QByteArray
        id;

    };

    struct FileInfo
    {
        QString
        filename;

        QPointer<QFile>
        file;

        qint64
        offset;

        qint64
        offset_mb;

        int
        size;

        qint64
        end;

        QByteArray
        id;

        QList<BlockInfo>
        blocks;

    };

    QByteArray
    blockData(int file_index, int block_index) const;

    bool
    abortRequested() const;

    qint64
    block_size_max;

    qint64
    file_size_max;

    qint64
    safety_buffer;

    bool
    req_remount;

    QString
    _mountpoint;

    QString
    file_prefix;

    QByteArray
    pattern;

    qint64
    bytes_total;

    qint64
    bytes_written;

    qint64
    bytes_remaining;

    bool
    _canceled;

    bool
    success;

    int
    error_type;

    QPointer<QObject>
    files_parent;

    QList<FileInfo>
    file_infos;

};

#endif
