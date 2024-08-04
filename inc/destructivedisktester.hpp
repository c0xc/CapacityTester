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
#include <ctime>
#include <random>
#include <sys/stat.h>
#include <sys/types.h>
#include <exception>

#include <QtGlobal> //Q_OS_...

#if !defined(Q_OS_WIN)
#include <linux/fs.h> //BLKGETSIZE64
#include <sys/ioctl.h>
//#include <sys/types.h>
#include <signal.h> //kill()
#endif

#include <QObject>
#include <QVariant>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QStorageInfo>
#include <QSemaphore>
#include <QMutex>
#include <QThread>
#include <QTimer>
#include <QCryptographicHash>
#include <QtConcurrent>
#include <QFuture>

#include "storagediskselection.hpp"
#include "classlogger.hpp"

/*! \class DestructiveDiskTester
 *
 * \brief The DestructiveDiskTester class provides an interface for
 * testing an unmounted USB storage disk.
 *
 * Unlike VolumeTester, which works with the filesystem on the partition
 * of a USB drive (volume refers to a mounted filesystem),
 * this class works on the USB storage (block device) itself.
 * It tries to detect the real capacity of a fake drive faster
 * than the volume test, which can take days with some huge fake drives.
 * Faster means it should only take a couple of minutes.
 * That's possible because instead of slowly filling the filesystem
 * to reach the capacity limit, it can write at random offsets.
 *
 * This disk test must be run as root (or as superuser with full access
 * to the block device).
 *
 *
 *
 */

class DestructiveDiskTester : public QObject
{
    Q_OBJECT

    struct MChunkPattern
    {
        QByteArray block;
        QByteArray hash;

        MChunkPattern();
        MChunkPattern(const QByteArray &bytes);

        QByteArray
        toByteArray();
    };

signals:

    void
    started(qint64 total);

    void
    startFailed(const QString &error_msg = "");

    void
    filesystemUnmounted(const QString &mountpoint);

    void
    written(qint64 pos, double avg_speed = 0, double current_speed = 0);

    void
    verified(qint64 pos, double avg_speed = 0, double current_speed = 0);

    void
    writeFailed(qint64 start);

    void
    verifyFailed(qint64 start);

    void
    finished(int result = Error::GenericFail);

public:

    struct Error
    {
        enum Type
        {
            Aborted         = -1,
            None            = 0,
            GenericFail     = 1 << 0,
            Prep            = 1 << 1,
            Open            = 1 << 2,
            SeekWrite       = 1 << 3,
            WriteData       = 1 << 4,
            SeekRead        = 1 << 5,
            ReadData        = 1 << 6,
            ReadEmpty       = 1 << 7,
            VerifyMismatch  = 1 << 8,
        };
    };

    static const int
    KB = 1024;

    static const int
    MB = 1024 * KB;

    static const int
    GB = 1024 * MB;

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

    static QByteArray
    generateRandomBytes0(int pattern_size);

    DestructiveDiskTester(const QString &dev_path);

    ~DestructiveDiskTester();

    void
    setParentPid(unsigned int pid);

    qint64
    getCapacity();

    int
    getBlockSize();

    bool
    isWritable();

    //NOTE this routine is NOT 100% reliable!
    bool
    isMounted(); //TODO

    bool
    exists();

    bool
    isValid();

    bool
    isFastMode();

    bool
    isFullMode();

    void
    setFullMode();

    bool
    useDirectIO();

    void
    setLimitGb(int limit);

    bool
    prepareDevice(QString *error_msg_ptr = 0);

    bool
    postResetDevice();

    qint64
    elapsed();

public slots:

    void
    start();

    virtual void
    cancel(bool force_quit = false);

private slots:

    bool
    runGBSteps();

    bool
    runGBSteps(int phase);

    bool
    runFullTest();

    bool
    runFullWrite();

    bool
    runFullRead();

protected:

    bool
    m_mode_full;

private:

    enum RandomGenerator 
    {
        default_random_engine = 0,
        minstd_rand0 = 1,
        minstd_rand = 2,
        mt19937 = 3,
        mt19937_64 = 4,
        ranlux24_base = 5,
        ranlux48_base = 6,
        ranlux24 = 7,
        ranlux48 = 8,
        knuth_b = 9,
    };

    template <typename RNG>
    static QByteArray
    generateRandomBytes(int count, const QString &src);

    static QByteArray
    generateRandomBytes(int count, RandomGenerator rng, const QString &src = "default");

    QByteArray
    generateRandomBytes(int count);

    template <typename RNG>
    static qint64
    testRandomGenerator(const QString &name);

    void
    initRandomBenchmark();

    QByteArray
    indexedBlockPattern(qint64 block, QByteArray *pattern_ptr = 0);

    MChunkPattern
    fullChunkPattern(qint64 index = -1);

    MChunkPattern
    fullChunkPattern1(qint64 index);

    MChunkPattern
    fullChunkPattern2();

    QVariantList
    fullChunkPattern10VariantList(qint64 start_index);

    QList<MChunkPattern>
    reqFullChunkPatterns(int count);

    MChunkPattern
    getFullChunkPattern(qint64 index);

    static QByteArray
    calcRawHash(const QByteArray &block);

    bool
    initBlockBuffer();

    bool
    openFD(bool sync);

    bool
    openFD();

    bool
    closeFD();

    bool
    reopen();

    void
    sync();

    bool
    seekFD(qint64 offset);

    QByteArray
    readBlocks(int size = 0);

    QByteArray
    readData(int size = 0);

    bool
    writeBlocks(const char *bytes, qint64 size);

    bool
    writeBlocks(const QByteArray &data);

    bool
    writeBlock(const char *bytes = 0);

    bool
    writeData(const QByteArray &data);

    bool
    checkParentPid();

    unsigned int
    m_parent_pid = 0;

    QString
    m_dev_path;

    int
    m_fd;

    QSharedPointer<StorageDiskSelection::Device>
    m_device;

    int
    m_dev_block_size;

    int
    m_old_inum;

    char
    *m_block_buffer;

    qint64
    m_size;

    qint64
    m_num_blocks;

    std::atomic<bool>
    m_stop_req;

    bool
    m_close_reopen;

    bool
    m_use_fsync;

    bool
    m_direct_io;

    int
    m_limit_gb;

    QByteArray
    m_block_pattern_n;

    QQueue<MChunkPattern>
    patterns_q;

    QVector<QByteArray>
    m_chunk_hashes;

    QMap<RandomGenerator, qint64>
    m_rng_time;

    RandomGenerator
    m_sel_rng;

    QString
    m_sel_src;

    QElapsedTimer
    m_total_timer;

    bool
    m_verify_mismatch_detected = false;

    int
    m_err_result = Error::None;


};

#endif
