#include "destructivedisktester.hpp"

qint64
DestructiveDiskTester::getCapacity(int fd)
{
    qint64 capacity = 0;

#if !defined(Q_OS_WIN)

    ioctl(fd, BLKGETSIZE64, &capacity);

#endif

    return capacity;
}

qint64
DestructiveDiskTester::getCapacity(const QString &dev_path)
{
    qint64 capacity = 0;

#if !defined(Q_OS_WIN)

    int fd = ::open(dev_path.toLocal8Bit().constData(), O_RDONLY);
    if (fd < 0) return fd; //failed to open -> return negative value
    capacity = getCapacity(fd);
    close(fd);

#elif defined(Q_OS_WIN)

    auto device = StorageDiskSelection().blockDevice(dev_path);
    if (!device.isNull())
        capacity = device.capacity();

#endif

    //I hate Windows.

    return capacity;
}

int
DestructiveDiskTester::getBlockSize(int fd)
{
    int bs = 0;

#if !defined(Q_OS_WIN)

    ioctl(fd, BLKSSZGET, &bs); //logical block size

#elif defined(Q_OS_WIN)

    //not implemented, see isWritable() method

#endif

    return bs;
}

int
DestructiveDiskTester::getInum(int fd)
{
#if !defined(Q_OS_WIN)

    struct stat file_stat;
    int ret = fstat(fd, &file_stat);
    if (ret < 0) return -1;

    return file_stat.st_ino;

#else

    return -1;

#endif
}

int
DestructiveDiskTester::getInum(const QString &dev_path)
{
    int inum = 0;

#if !defined(Q_OS_WIN)

    int fd = ::open(dev_path.toLocal8Bit().data(), O_RDONLY);
    if (fd < 0) return fd; //failed to open -> return negative value
    inum = getInum(fd);
    close(fd);

#endif

    return inum;
}

bool
DestructiveDiskTester::isWritable(const QString &dev_path)
{
    bool is_writable = false;

#if !defined(Q_OS_WIN)

    //Check if device file is writable
    //NOTE this could be a Unix/Linux alternative to checking for root
    //just in case someone has a non-root user with write access to /dev
    if (QFileInfo(dev_path).isWritable()) is_writable = true;

#elif defined(Q_OS_WIN)

    //not implemented, see isWritable() method

#endif

    return is_writable;
}

bool
DestructiveDiskTester::amIRoot()
{
    bool am_i_root = false;

#if !defined(Q_OS_WIN)

    //geteuid() requires unistd.h
    if (geteuid() == 0) am_i_root = true;

#endif

    return am_i_root;
}

QByteArray
DestructiveDiskTester::generateRandomBytes0(int pattern_size)
{
    assert(pattern_size > 0);
    QByteArray new_pattern(pattern_size, (char)0);

    std::srand(std::time(0));
    for (int i = 0; i < pattern_size; i++)
    {
        //Random byte except 0
        uchar byte = std::rand() % 254 + 1; //0 < byte < 255
        new_pattern[i] = byte;
    }

    assert(new_pattern.size() == pattern_size);

    return new_pattern;
}

DestructiveDiskTester::DestructiveDiskTester(const QString &dev_path)
                     : QObject(),
                       m_parent_pid(0),
                       m_mode_full(false),
                       m_fd(-1),
                       m_dev_block_size(0),
                       m_old_inum(-1),
                       m_block_buffer(0),
                       m_size(0),
                       m_num_blocks(0),
                       m_stop_req(false),
                       m_close_reopen(true),
                       m_use_fsync(true),
                       m_limit_gb(0)
{
    //RNG test
    initRandomBenchmark();

    //Path to storage disk that will be opened, e.g., /dev/sdf
    //will be opened by path, so a new fd will be created
    m_dev_path = dev_path;
    StorageDiskSelection::Device device = StorageDiskSelection().blockDevice(dev_path);
    if (!device.isNull())
        m_device.reset(new StorageDiskSelection::Device(device));
    else
        return;

    //Do *not* open device here because this class instance may
    //be created (by the GUI) just to check something
    //and/or before moving it to another thread.
    //So we shouldn't open (for writing) before actually starting the test.

    //Abort initialization if the device is in use (not 100% reliable)
    if (isMounted()) return;
    //or if otherwise inaccessible, invalid
    qint64 capacity = 0;
    if (isValid())
    {
        capacity = getCapacity(m_dev_path);
        if (capacity < 0) capacity = 0; //error
    }
    if (!capacity) return;
    //As long as m_size remains at 0, the test won't start, nothing is written

    //Store capacity
    m_size = capacity;
    //on start, dev will be opened, inum/info initially queried

}

DestructiveDiskTester::~DestructiveDiskTester()
{
    closeFD();

    if (m_block_buffer)
        free(m_block_buffer);
}

void
DestructiveDiskTester::setParentPid(unsigned int pid)
{
    m_parent_pid = pid;
}

qint64
DestructiveDiskTester::getCapacity()
{
    qint64 capacity = 0;

#if !defined(Q_OS_WIN)

    //Within this class instance, we prefer to query by file descriptor,
    //specifically to prevent something like /dev/sdx now pointing
    //to another device as it did earlier.
    capacity = getCapacity(m_fd);

#elif defined(Q_OS_WIN)

    if (m_device && !m_device->isNull())
        capacity = m_device->capacity();

#endif

    return capacity;
}

int
DestructiveDiskTester::getBlockSize()
{
    int block_size = 0; //usually 512

#if !defined(Q_OS_WIN)

    block_size = getBlockSize(m_fd);

#elif defined(Q_OS_WIN)

    if (m_device && !m_device->isNull())
        block_size = m_device->blockSize();

#endif

    return block_size;
}

bool
DestructiveDiskTester::isWritable()
{
    bool is_writable = false;

#if !defined(Q_OS_WIN)

    //Check if device file is writable
    //NOTE this could be a Unix/Linux alternative to checking for root
    //just in case someone has a non-root user with write access to /dev
    if (QFileInfo(m_dev_path).isWritable()) is_writable = true;

#elif defined(Q_OS_WIN)

    //TODO StorageDiskSelection::Device isWritable / tryOpen
    int fd = -1;
    if (m_device && !m_device->isNull())
        fd = m_device->open();
    if (fd > -1)
    {
        is_writable = true;
        m_device->close();
    }

#endif

    return is_writable;
}

bool
DestructiveDiskTester::isMounted()
{
    bool is_mounted = false;

    //TODO m_device->checkMounted() not implemented

    //NOTE this routine is NOT 100% reliable!
    //actually, it's quite hacky.
    //but we wanted to keep the dependency on UDiskManager out of here...
    //TODO reconsider importing UDiskManager?

    //Try to check if device is mounted, i.e., if device's partition mounted
    //Note that this is a hack because we're assuming that the mounted
    //partitions have the same base name as the device: sda -> sda1
    //This may not always be the case, see lvm mappings etc.
    foreach (const QStorageInfo &info, QStorageInfo::mountedVolumes())
    {
        QString mounted_dev = info.device();
        if (mounted_dev.contains(m_dev_path))
        {
            Debug(QS("device appears to be mounted! %s", mounted_dev.toUtf8().constData())); //Info/Warning //TODO
            return true;
        }
    }

#if defined(Q_OS_WIN)
    //TODO mounted check not implemented
#endif

    return is_mounted;
}

bool
DestructiveDiskTester::exists()
{
    if (m_dev_path.isEmpty()) return false;
    if (!m_device || m_device->isNull()) return false;

    //This returns true if the selected device exists.
    //No root privileges are required for this check.
    //It can be used to check if it makes sense to ask for a root password.
    return m_device->isValid();
}

bool
DestructiveDiskTester::isValid()
{
    if (m_dev_path.isEmpty()) return false;
#if !defined(Q_OS_WIN)
    if (!QFile(m_dev_path).exists()) return false;
#endif
    if (!m_device || m_device->isNull()) return false;

    //Check if device has non-zero capacity
    //This fails without root privileges.
    if (getCapacity(m_dev_path) <= 0) return false;

    return true;
}

bool
DestructiveDiskTester::isFastMode()
{
    return !m_mode_full;
}

bool
DestructiveDiskTester::isFullMode()
{
    return m_mode_full;
}

void
DestructiveDiskTester::setFullMode()
{
    m_mode_full = true;
    m_direct_io = isFastMode();
}

bool
DestructiveDiskTester::useDirectIO()
{
    return m_direct_io;
}

void
DestructiveDiskTester::setLimitGb(int limit)
{
    m_limit_gb = limit;
}

bool
DestructiveDiskTester::prepareDevice(QString *error_msg_ptr)
{
    //Prepare device for testing
    //Unless in manual mode, check if device or any of its partitions are mounted, unmount.
    //This is done for USB storage devices (which is the main use case).

    //All filesystems on the device should be unmounted (they would be overwritten)
    //There are different ways a filesystem can be mounted and we might not detect all,
    //but this program is used to check USB sticks and similar devices
    //which are usually mounted directly, so this shouldn't be an issue.
    //We also don't take care of LVM, RAID or filesystems mounted twice.

    //Try to find and unmount all filesystems on the device
    StorageDiskSelection::Device device = StorageDiskSelection().blockDevice(m_dev_path);
    Debug(QS("preparing device %s", CSTR(m_dev_path)));
    bool no_auto_unmount = false;
    foreach (auto fs, device.filesystems())
    {
        //Check if mounted
        if (fs.mountpoint.isEmpty()) continue; //skip unless mounted
        Debug(QS("filesystem is mounted: %s", CSTR(fs.path)));
        //TODO fs.path is dbus path, add attribute with device path

        //Check if we're allowed to unmount this filesystem
        if (no_auto_unmount)
        {
            if (error_msg_ptr)
                *error_msg_ptr = tr("Filesystem is mounted, cannot continue: %1").arg(fs.path);
            return false;
        }
        if (!device.isUsbDevice())
        {
            Debug(QS("filesystem is mounted, not unmounting automatically because this is not a USB device: %s", CSTR(fs.path)));
            if (error_msg_ptr)
                *error_msg_ptr = tr("Filesystem is mounted, not unmounting automatically because this is not a USB device: %1").arg(fs.path);
            return false;
        }

        //Unmount filesystem
        QString error_msg;
        if (!device.unmount(fs.path, &error_msg))
        {
            Debug(QS("failed to unmount %s: %s", CSTR(fs.path), CSTR(error_msg)));
            if (error_msg_ptr)
                *error_msg_ptr = tr("Failed to unmount %1: %2").arg(fs.path).arg(error_msg);
            return false;
        }
        Debug(QS("unmounted filesystem %s", CSTR(fs.path)));
        emit filesystemUnmounted(fs.path); //unused
    }

    //If configured and necessary, also rewrite the MBR to remove the partition table

    //Get the capacity again
    qint64 capacity = getCapacity(m_dev_path);
    m_size = capacity;
    Debug(QS("prepared device %s; size = %lld", CSTR(m_dev_path), m_size));

    return true;
}

bool
DestructiveDiskTester::postResetDevice()
{
    //Format device after test, create new partition table with filesystem
    //TODO not yet implemented by StorageDiskSelection

    return false;
}

qint64
DestructiveDiskTester::elapsed()
{
    return m_total_timer.elapsed();
}

void
DestructiveDiskTester::start()
{
    QString error_msg;
    if (!prepareDevice(&error_msg))
    {
        Debug(QS("failed to prepare device: %s", CSTR(error_msg)));
        emit startFailed(error_msg);
        return;
    }

    Debug(QS("starting test..."));
    //Open block device
    if (isFastMode())
    {
        Debug(QS("opening device for fast mode (d/sync i/o): %s", m_device->path().toUtf8().constData()));
        openFD(); //synced, direct i/o
    }
    else
    {
        Debug(QS("opening device (default i/o): %s", CSTR(m_device->path())));
        openFD(false);
    }
    Debug(QS("opened dev, fd = %d", m_fd));
    if (m_fd == -1)
    {
        Debug(QS("failed to open device"));
        emit startFailed();
        return;
    }
    //NOTE if we return anywhere without returning an appropriate signal,
    //the GUI (and/or wrapper process) will hang forever, waiting for a signal.
    //IDEA set up a timer, heartbeat

    //Abort if device not accessible / invalid (empty)
    if (m_size <= 0)
    {
        Debug(QS("cannot start test - disk has a size of zero or unknown"));
        emit startFailed();
        return;
    }

    //Initialize block buffer
    if (!initBlockBuffer()) emit startFailed();

    //Generate test pattern
    m_num_blocks = m_size / m_dev_block_size;
    if (m_size % m_dev_block_size)
    {
        Debug(QS("disk has unexpected size"));
        emit startFailed();
        return;
    }
    m_block_pattern_n = generateRandomBytes0(m_dev_block_size);

    // ---
    bool ok = false;
    qint64 total_mb = m_size / MB;
    Debug(QS("total size: %lld MB; block size: %d; blocks: %lld", total_mb, m_dev_block_size, m_num_blocks));
    m_total_timer.invalidate();
    m_total_timer.start();
    emit started(total_mb);

    //IDEA faster algorithm: binary search, check 0% (ok) 100% (nok), next 50%
    //IDEA or start by checking known limits like 64G

    if (isFastMode())
        ok = runGBSteps();
    else if (isFullMode())
        ok = runFullTest();

    Debug(QS("completed, ok = %d", ok));
    int res = ok ? 1 : 0;
    if (m_stop_req || !checkParentPid()) res = -1; //make sure to return abort if stop signal received
    emit finished(res);

    postResetDevice();
}

void
DestructiveDiskTester::cancel(bool force_quit)
{
    Debug(QS("stop request received - canceling test, force quit: %d", force_quit));

    //Set stop request flag, similar to requestInterruption()
    m_stop_req = true;

    //Force quit not really implemented (implemented in wrapper)
    if (force_quit)
    {
        //TODO force quit
        this->thread()->quit();
    }
}

bool
DestructiveDiskTester::runGBSteps()
{
    bool ok = false;

    //FAST test algorithm with GB precision
    Debug(QS("starting fast disk test..."));

    //Run write operations with GB intervals
    ok = runGBSteps(1);
    Debug(QS("write phase done, ok = %d", ok));

    //Reopen device file to avoid cache issues
    //(Otherwise, reusing fd might cause some reads to come from cache.)
    sync();
    ok = ok && reopen();
    Debug(QS("write reopened (flushed), ok = %d", ok));

    //Run read operations with GB intervals, verify
    ok = ok && runGBSteps(2);
    Debug(QS("read phase done, ok = %d", ok));

    return ok;
}

bool
DestructiveDiskTester::runGBSteps(int phase)
{
    bool ok = true;

    //phase: 1 = write; 2 = verify
    //phase 0 means verify after each write which is faster
    //but it can yield wrong results
    //(full test finishes within seconds even though OS caching off)
    //if/when there's a need, we'd later improve this algorithm,
    //one idea is to use a binary search approach
    //and overwrite some MB to prevent the device cache from making trouble
    //clearly, any type of caching can lead to wrong results
    //if the test data being verified is not read from the device
    //but from the cache that was filled when we wrote it.
    //To prevent that, we already use O_DIRECT and O_SYNC (open flags).
    //We could also offer a super-safe but slightly awkward alternative:
    //Eject the device (that's easy, our own UDiskManager can do it),
    //then wait for the device to reappear (not so easy: user needs to wake up)
    //and only then continue with the verification phase.

    //Loop over logical positions in GB steps
    QByteArray check_pattern;
    qint64 mb_count = m_size / MB;
    qint64 gb_count = mb_count / 1024;
    ok = gb_count > 0;
    for (qint64 g = 0; g < gb_count - 1; g++) //last GB not checked
    {
        //Now checking first MB at g GB
        qint64 m = g * 1024;
        qint64 b = m * MB; //exact position
        if (m_stop_req || !checkParentPid())
        {
            Debug(QS("stop requested at %llu / %lld GB", b, g));
            return false;
        }

        QByteArray block_pattern = indexedBlockPattern(b);

        if (phase == 0 || phase == 1)
        {
            //Write pattern at position
            if (!seekFD(b))
            {
                //seek failed
                Debug(QS("write seek failed (phase %d) at %llu / %lld GB (errno: %d)", phase, b, g, errno));
                emit writeFailed(m);
                return false;
            }
            if (writeBlocks(block_pattern) == false)
            {
                Debug(QS("write failed (phase %d, bs %d) at %llu / %lld GB (errno: %d)", phase, block_pattern.size(), b, g, errno));
                emit writeFailed(m);
                return false;
            }
            //fsync(m_fd);
            emit written(m, 0.0);
        }

        if (phase == 0 || phase == 2)
        {
            //Read pattern back
            if (!seekFD(b))
            {
                //seek failed
                Debug(QS("read seek failed (phase %d) at %llu / %lld GB (errno: %d)", phase, b, g, errno));
                emit verifyFailed(m);
                return false;
            }
            check_pattern = readBlocks(block_pattern.size());
            if (check_pattern.isEmpty())
            {
                //empty read
                Debug(QS("read failed (phase %d), empty result at %llu / %lld GB (errno: %d)", phase, b, g, errno));
                emit verifyFailed(m);
                return false;
            }
            if (block_pattern != check_pattern)
            {
                //pattern on disk differs from test pattern => bad disk detected
                Debug(QS("pattern mismatch at %llu / %lld GB", b, g));
                emit verifyFailed(m);
                return false;
            }
            emit verified(m, 0.0);
        }

    }

    return ok;
}

bool
DestructiveDiskTester::runFullTest()
{
    bool ok = false;

    //FULL test algorithm with block precision
    Debug(QS("starting full disk test..."));

    //Run write operations with GB intervals
    ok = runFullWrite();
    Debug(QS("write phase done, ok = %d", ok));

    //Reopen device file to avoid cache issues
    //(Otherwise, reusing fd might cause some reads to come from cache.)
    sync();
    ok = ok && reopen();
    Debug(QS("write reopened (flushed), ok = %d", ok));

    //Run read operations with GB intervals, verify
    ok = ok && runFullRead();
    Debug(QS("read phase done, ok = %d", ok));

    return ok;
}

bool
DestructiveDiskTester::runFullWrite()
{
    bool ok = true;
    //Seek beginning of block device
    assert(m_num_blocks != 0);
    if (!seekFD(0))
    {
        Debug(QS("write seek failed (errno: %d)", errno));
        emit writeFailed(0);
        return false;
    }

    //-------------------------------------------------------------------------
    //
    //NOTE synced, direct i/o is too slow for this type of test
    //We use direct i/o in the fast test, to really make sure
    //nothing is ever cached.
    //But that's neither necessary nor appropriate for a full test.
    //For example, writing with direct i/o to a usb2 drive
    //which should have write speeds of 7.5M/s, speeds are at 330K/s.
    //
    //However, disabling direct i/o causes the system to use caching,
    //so very high write speeds are reported (263 MB/s on a USB2 stick)
    //because we're filling the OS cache.
    //
    //Storing each block's hash isn't much better than
    //storing each block in memory:
    //Consider a 2 TB disk with 3907029168 blocks of 512 B.
    //Now, if one hash takes up 40 bytes (just an example, ignoring overhead),
    //that makes 3907029168*40 = 156281166720 B = 149041 MB = 146 GB.
    //
    //-------------------------------------------------------------------------

    //-------------------------------------------------------------------------
    //see notes in getFullChunkPattern()
    //if (hash_in_background)
    //    Debug(QS("using %d hash workers (min: %d)", thread_count, min_ready_patterns));
    //-------------------------------------------------------------------------

    //Reset hashes (of written chunks)
    qint64 m_count = m_size / MB;
    patterns_q.clear();
    m_chunk_hashes.clear();
    m_chunk_hashes.resize(m_count);

    //Write buffer
    QByteArray buffer_container(MB, (char)0);
    char *chunk_buffer = buffer_container.data();

    //Start performance timer
    QElapsedTimer timer;
    timer.start();
    qint64 last_mb_wr = 0; //MB written (since last time)
    qint64 last_msec_elapsed = 0;
    double current_speed = 0; //MB/sec (since last time)
    double avg_speed = 0; //MB/sec (since start)

    //Write loop - generate and write blocks, grouped in MB chunks
    bool check_uniq = true; //check, require unique pattern, for debugging
    check_uniq = false; //SLOW (< 1 MB/s)
    qint64 g = -1; //position in GB
    QElapsedTimer chunk_timer;
    Debug(QS("starting write loop; %lld chunks", m_count));
    for (qint64 m = 0; m < m_count && ok; m++) //m == position in MB
    {
        chunk_timer.start();
        g += (m % KB == 0 ? 1 : 0); //position in GB
        if (m_limit_gb && g == m_limit_gb) break;
        if (m_stop_req || !checkParentPid())
        {
            Debug(QS("stop requested at %lld MB", m));
            return false;
        }

        //Get block pattern to be written
        MChunkPattern pattern = getFullChunkPattern(m);
        assert(pattern.block.size() == MB);
        if (check_uniq && m_chunk_hashes.contains(pattern.hash))
        {
            Debug(QS("internal error or hash collision generating test pattern for chunk #%lld", m));
            emit writeFailed(m);
            return false;
        }
        m_chunk_hashes[m] = pattern.hash; //save hash of written chunk
        qint64 hash_time = chunk_timer.elapsed(); //msec
        //Copy chunk pattern to buffer
        memcpy(chunk_buffer, pattern.block.constData(), pattern.block.size());
        assert(QByteArray(chunk_buffer, pattern.block.size()) == pattern.block);
        //hash time 18 ms with original algorithm (not optimized, seq)

        //Write chunk to disk
        //Unless configured otherwise, commit each chunk (fsync)
        //in order to avoid filling the OS cache instead,
        //reporting bogus speeds.
        chunk_timer.start();
        bool wr_ok = false;
        bool write_blocks = false;
        if (write_blocks)
            wr_ok = writeBlocks(chunk_buffer, MB);
        else
            wr_ok = m_device->write(chunk_buffer, MB) == MB;
        if (!wr_ok)
        {
            Debug(QS("write failed at chunk #%lld [%lld MB] (errno: %d)", m, m + 1, errno));
            emit writeFailed(m);
            return false;
        }
        sync(); //TODO (double-check fsync implementation and no-buffer flags)
        qint64 wr_time = chunk_timer.elapsed(); //chunk written in msec

        //Measure performance, report status
        qint64 mb_wr = m + 1;
        qint64 msec_elapsed = timer.elapsed();
        if (msec_elapsed - last_msec_elapsed > 1000)
        {
            //1sec+ has elapsed
            qint64 msec_diff = msec_elapsed - last_msec_elapsed;
            last_msec_elapsed = msec_elapsed;
            double sec_diff = (double)msec_diff / 1000; //>= 1

            //Current speed = MB since last time
            qint64 mb_wr_diff = mb_wr - last_mb_wr;
            last_mb_wr = mb_wr;
            current_speed = mb_wr_diff / sec_diff;

            //Average speed = MB since start of test / time
            double sec_total = (double)msec_elapsed / 1000;
            avg_speed = mb_wr / sec_total;

            Debug(QS("+1sec: at %lld MB, %lld GB; +%lld/%lld msec + %lld MB in %f sec = %f M/s = %f M/s", m, g, msec_diff, msec_elapsed, mb_wr_diff, sec_diff, current_speed, avg_speed));

            emit written(mb_wr, avg_speed, current_speed);
        }
    }

    return ok;
}

bool
DestructiveDiskTester::runFullRead()
{
    bool ok = true;

    //Seek beginning of block device
    assert(m_num_blocks != 0);
    if (!seekFD(0))
    {
        Debug(QS("read seek failed (errno: %d)", errno));
        emit verifyFailed(0);
        return false;
    }

    //IDEA debugging hint - internal stats to check own performance
    //Do they come back in random order?
    //Then it could be a sign that using multiple threads improves performance.

    //Read buffer
    QByteArray buffer_container(MB, (char)0);
    char *chunk_buffer = buffer_container.data();

    //Start performance timer
    QElapsedTimer timer;
    timer.start();
    qint64 last_mb_rd = 0; //MB read (since last time)
    qint64 last_msec_elapsed = 0;
    double current_speed = 0; //MB/sec (since last time)
    double avg_speed = 0; //MB/sec (since start)

    //Read loop - read and verify chunks
    qint64 m_count = m_size / MB;
    QElapsedTimer chunk_timer;
    qint64 g = -1; //position in GB
    Debug(QS("starting read loop; %lld chunks", m_count));
    for (qint64 m = 0; m < m_count && ok; m++) //m == position in MB
    {
        chunk_timer.start();
        g += (m % KB == 0 ? 1 : 0); //position in GB
        if (m_limit_gb && g == m_limit_gb) break;
        if (m_stop_req || !checkParentPid())
        {
            Debug(QS("stop requested at %lld MB", m));
            return false;
        }

        //Get hash of chunk
        QByteArray expected_hash = m_chunk_hashes[m];

        //Read chunk from disk
        bool rd_ok = false;
        rd_ok = m_device->read(chunk_buffer, MB) == MB;
        if (!rd_ok)
        {
            Debug(QS("read failed at chunk #%lld [%lld MB] (errno: %d)", m, m + 1, errno));
            emit verifyFailed(m);
            return false;
        }
        qint64 rd_time = chunk_timer.elapsed(); //chunk read in msec

        //Verify block by calculating and comparing its hash
        QByteArray chunk_bytes(chunk_buffer, MB);
        QByteArray new_hash = calcRawHash(chunk_bytes);
        bool match = new_hash == expected_hash;
        if (!match)
        {
            Debug(QS("pattern mismatch at chunk #%lld [%lld MB]", m, m + 1));
            emit verifyFailed(m);
            return false;
        }

        //Measure performance, report status
        qint64 mb_rd = m + 1;
        qint64 msec_elapsed = timer.elapsed();
        if (msec_elapsed - last_msec_elapsed > 1000)
        {
            //1sec+ has elapsed
            qint64 msec_diff = msec_elapsed - last_msec_elapsed;
            last_msec_elapsed = msec_elapsed;
            double sec_diff = (double)msec_diff / 1000; //>= 1

            //Current speed = MB since last time
            qint64 mb_rd_diff = mb_rd - last_mb_rd;
            last_mb_rd = mb_rd;
            current_speed = mb_rd_diff / sec_diff;

            //Average speed = MB since start of test / time
            double sec_total = (double)msec_elapsed / 1000;
            avg_speed = mb_rd / sec_total;

            Debug(QS("+1sec: at %lld MB; +%lld/%lld msec + %lld MB in %f sec = %f M/s = %f M/s", m, msec_diff, msec_elapsed, mb_rd_diff, sec_diff, current_speed, avg_speed));

            emit verified(mb_rd, avg_speed, current_speed);
        }
    }

    return ok;
}

template <typename RNG>
QByteArray
DestructiveDiskTester::generateRandomBytes(int count, const QString &src)
{
    //Get random number generator, static (per thread)
    //Use default source for random bytes (/dev/urandom)
    //On AMD Phenom(tm) II X6 1100T, ctor arg "default": unsupported token
    //using default-constructed instance instead
    thread_local std::random_device rd; //src.toUtf8().constData()
    //std::mt19937 is a fast generator, but minstd_rand may be even faster
    //taus88 is said to be very fast
    thread_local RNG gen(rd());

    //Generate random bytes
    std::uniform_int_distribution<int> distrib(1, 255); 
    QByteArray bytes(count, (char)0);
    for (int i = 0; i < count; i++)
    {
        uchar byte = distrib(gen);
        bytes[i] = byte;
    }

    return bytes;
}

QByteArray
DestructiveDiskTester::generateRandomBytes(int count, RandomGenerator rng, const QString &src)
{
    QByteArray bytes;

    if (rng == minstd_rand0)
        bytes = generateRandomBytes<std::minstd_rand0>(count, src);
    else if (rng == minstd_rand)
        bytes = generateRandomBytes<std::minstd_rand>(count, src);
    else if (rng == mt19937)
        bytes = generateRandomBytes<std::mt19937>(count, src);
    else if (rng == mt19937_64)
        bytes = generateRandomBytes<std::mt19937_64>(count, src);
    else if (rng == ranlux24_base)
        bytes = generateRandomBytes<std::ranlux24_base>(count, src);
    else if (rng == ranlux48_base)
        bytes = generateRandomBytes<std::ranlux48_base>(count, src);
    else if (rng == ranlux24)
        bytes = generateRandomBytes<std::ranlux24>(count, src);
    else if (rng == ranlux48)
        bytes = generateRandomBytes<std::ranlux48>(count, src);
    else if (rng == knuth_b)
        bytes = generateRandomBytes<std::knuth_b>(count, src);
    else
        bytes = generateRandomBytes<std::default_random_engine>(count, src);

    return bytes;
}

QByteArray
DestructiveDiskTester::generateRandomBytes(int count)
{
    return generateRandomBytes(count, m_sel_rng, m_sel_src);
}

template <typename RNG>
qint64
DestructiveDiskTester::testRandomGenerator(const QString &name)
{
    RNG generator;

    std::uniform_int_distribution<> distribution(1, 255);

    int count = MB;

    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < count; i++)
        distribution(generator);
    const auto end = std::chrono::steady_clock::now();
    qint64 us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count(); //µs
    qint64 ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    qint64 ns_per_op = ns / static_cast<double>(count);

    Debug(QS("test result with %s generator (x%d): %lld ns / op (%lld us)", name.toUtf8().constData(), count, ns_per_op, us));

    return ns_per_op;
}

void
DestructiveDiskTester::initRandomBenchmark()
{
    Debug(QS("running rng benchmark..."));

    //Check availability of rng source
    //On AMD Phenom(tm) II X6 1100T, ctor arg "default": unsupported token
    //See generateRandomBytes()
    QString src;
    try
    {
        std::random_device("hw");
        src = "hw"; //use hardware source
        //Debug(QS("selecting hardware source for rng"));
    }
    catch (std::exception &e)
    {
        //continue with default, pass
    }
    m_sel_src = src;
    //Debug(QS("using random device src: %s", src.isEmpty() ? "default" : src.toUtf8().constData()));

    //Name mapping
    QMap<RandomGenerator, QString> rng_name_map;
    rng_name_map[minstd_rand0] = "minstd_rand0";
    rng_name_map[minstd_rand] = "minstd_rand";
    rng_name_map[mt19937] = "mt19937";
    rng_name_map[mt19937_64] = "mt19937_64";
    rng_name_map[ranlux24_base] = "ranlux24_base";
    rng_name_map[ranlux48_base] = "ranlux48_base";
    rng_name_map[ranlux24] = "ranlux24";
    rng_name_map[ranlux48] = "ranlux48";
    rng_name_map[knuth_b] = "knuth_b";

    //Test all generators, write measured time to map
    int count = MB;
    QMap<RandomGenerator, qint64> rng_time;
    QMultiMap<qint64, RandomGenerator> ns_rng_map;
    foreach (RandomGenerator rng, rng_name_map.keys())
    {
        QString name = rng_name_map[rng];
        //Run test (count bytes), measure time
        auto start = std::chrono::steady_clock::now();
        generateRandomBytes(count, rng, src);
        auto end = std::chrono::steady_clock::now();
        qint64 ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        qint64 us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count(); //µs
        qint64 ns_per_op = ns / static_cast<double>(count);
        rng_time[rng] = ns_per_op;
        ns_rng_map.insert(ns_per_op, rng);
        Debug(QS("test result with %s generator (x%d): %lld ns / op (%lld us)", name.toUtf8().constData(), count, ns_per_op, us));
    }

    //Save results
    m_rng_time = rng_time;
    m_sel_rng = ns_rng_map.first();
    Debug(QS("benchmark complete - using %s generator from now on", rng_name_map[m_sel_rng].toUtf8().constData()));

}

QByteArray
DestructiveDiskTester::indexedBlockPattern(qint64 block, QByteArray *pattern_ptr)
{
    QByteArray pattern;
    if (pattern_ptr) pattern = *pattern_ptr;
    else pattern = m_block_pattern_n; //default to pre-defined pattern

    //Put unique id sequence at beginning (block pos argument)
    //This is crucial because we need each block to be unique
    //if we were writing the same exact test pattern on all blocks,
    //we'd eventually reach a block beyond the real capacity that,
    //when reading, returns the content from the first block
    //so if the block patterns were not unique, that would be bad
    QByteArray id_bytes = QByteArray::number(block);
    assert(pattern.size() > id_bytes.size());
    pattern.replace(0, id_bytes.size(), id_bytes);
    pattern.replace(pattern.size() - id_bytes.size(), id_bytes.size(), id_bytes);

    return pattern;
}

DestructiveDiskTester::MChunkPattern
DestructiveDiskTester::fullChunkPattern(qint64 index)
{
    MChunkPattern pattern;

    //Random number generator
    //std::mt19937 is a fast generator, but minstd_rand may be even faster
    //taus88 is said to be very fast

    //Generate new random block pattern
    qint64 size = MB;
    QByteArray bytes = generateRandomBytes(size);

    //Write index into block
    if (index >= 0)
    {
        bytes = indexedBlockPattern(index, &bytes);
        assert(bytes.size() == size);
    }

    //Calculate hash
    QByteArray block_hash = calcRawHash(bytes);

    //pattern.index = index;
    pattern.block = bytes;
    pattern.hash = block_hash;
    return pattern;
}

DestructiveDiskTester::MChunkPattern
DestructiveDiskTester::fullChunkPattern1(qint64 index)
{
    MChunkPattern pattern;

    //Generate new random block pattern
    qint64 size = MB;
    QByteArray block = generateRandomBytes0(size);

    //Write block index into block
    block = indexedBlockPattern(index, &block);
    assert(block.size() == size);

    //Calculate hash
    QByteArray block_hash = calcRawHash(block);

    //pattern.index = index;
    pattern.block = block;
    pattern.hash = block_hash;
    return pattern;
}

DestructiveDiskTester::MChunkPattern
DestructiveDiskTester::fullChunkPattern2()
{
    MChunkPattern pattern;

    //Generate new random block pattern
    //Initialize mt19937 random number generator
    //using hardware device would be fastest but not always supported
    //random_device("hw") may throw exception:
    //random_device::random_device(const std::string&): device not available
    qint64 size = MB;
    thread_local std::random_device rd("default"); //"hw"
    thread_local std::mt19937 gen(rd());
    std::uniform_int_distribution<int> distrib(1, 255); 
    QByteArray bytes(size, (char)0);
    for (int i = 0; i < size; i++)
    {
        uchar byte = distrib(gen);
        bytes[i] = byte;
    }

    //Calculate hash
    QByteArray block_hash = calcRawHash(bytes);

    //pattern.index = index;
    pattern.block = bytes;
    pattern.hash = block_hash;
    return pattern;
}

QVariantList
DestructiveDiskTester::fullChunkPattern10VariantList(qint64 start_index)
{
    QVariantList vars;
    for (int i = 0; i < 10; i++)
        vars << fullChunkPattern(start_index + i).toByteArray();
    return vars;
}

/* 
 * TESTS WITH RANDOM NUMBERS
 *
 * Original function using rand(): 18 ms
 * Generating around 6 chunks in the background: 1810 ms
 *
    static std::random_device rd("hw");
    static std::uniform_int_distribution<> distrib(1, 255);
    uchar byte = distrib(rd);
    => 1713 ms without background jobs
 *
 *
    std::uniform_int_distribution<> distrib(1, 255);
    => 12 ms (single); 45-55 ms (max parallel)
    => 250 MB/s
 *
 * reducing overhead: 5x thread_max (here: 6) in parallel: 1309 ms
    => 310 MB/s (current speed) ... => 250-260 MB/s
 *
 * As for (choosing between) random number generators:
 * On some test vm, the fastest generator was minstd_rand.
 * However, on my P5 machine (which does not support hw source,
 * mt19937_64 appears to be the fastest one (<10ns per hash).
 *
 */

DestructiveDiskTester::MChunkPattern
DestructiveDiskTester::getFullChunkPattern(qint64 index)
{
    //Calculate several chunks, return one chunk pattern
    MChunkPattern pattern;

    //Background jobs can cause overhead, esp. with < 2 parallel jobs
    static int max_ready_patterns = QThread::idealThreadCount() * 5;
    static int min_ready_patterns = max_ready_patterns / 2 >= 2 ? max_ready_patterns / 2 : 0;
    //NOTE limit memory consumption - 1 chunk = 1 MB,
    //default max_threads (6) => 60 MB, x5 => 300 MB, x10 => 600 MB

    //Hash routine
    auto genPattern = [this](qint64 i)
    {
        return fullChunkPattern(i); //TODO 10List; use static genrandom thing with "name" param...
    };
    bool hash_in_background = true;

    //If queue half empty, request more patterns (async in background)
    static QQueue<QFuture<QByteArray>> hash_workers;
    if (hash_in_background && patterns_q.count() < min_ready_patterns)
    {
        int q_count_remaining = patterns_q.count() + hash_workers.count();
        for (int i = 0; i < max_ready_patterns; i++)
        {
            //Request 10M each in parallel //TODO
            qint64 next_i = i + q_count_remaining + index;
            QFuture<QByteArray> worker = QtConcurrent::run([=]()
            {
                QByteArray b;
                try
                {
                    b = genPattern(next_i).toByteArray();
                }
                catch (std::exception &e)
                {
                    qInfo() << "exception in worker thread!" << e.what();
                    Debug(QS("exception in hash worker thread: %s", e.what()));
                    //return nothing, max reduced below
                }
                return b;
            });
            hash_workers.enqueue(worker);
        }
    }

    //If queue empty, get next pattern now (blocking)
    if (patterns_q.isEmpty())
    {
        //Generate (at least) 1 pattern

        //Get results from background jobs, if any
        qint64 index_diff = 0;
        int worker_failed_count = 0;
        while (!hash_workers.isEmpty())
        {
            QByteArray b = hash_workers.dequeue().result();
            if (!b.isEmpty())
            {
                patterns_q.enqueue(b);
                index_diff++;
            }
            else
            {
                worker_failed_count++;
            }
        }
        if (worker_failed_count)
        {
            int max_old = max_ready_patterns;
            int max_new = max_ready_patterns - worker_failed_count;
            qDebug() << "failed hash workers" << worker_failed_count;
            Debug(QS("failed hash workers: %d; reducing max from %d to %d", worker_failed_count, max_old, max_new));
            max_ready_patterns = max_new;
            min_ready_patterns = max_ready_patterns / 2 >= 2 ? max_ready_patterns / 2 : 0;
        }

        //Generate at least one result now
        if (!index_diff)
            patterns_q.enqueue(genPattern(index));
    }
    assert(!patterns_q.isEmpty());
    pattern = patterns_q.dequeue();

    return pattern;
}

QByteArray
DestructiveDiskTester::calcRawHash(const QByteArray &block)
{
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(block);
    return hash.result();
}

bool
DestructiveDiskTester::initBlockBuffer()
{
    void *buf = 0;

    bool ok = true;

#if !defined(Q_OS_WIN)
    if (posix_memalign(&buf, m_dev_block_size, m_dev_block_size) != 0)
        ok = false;
#else
    if (!(buf = malloc(m_dev_block_size)))
        ok = false;
#endif

    if (m_block_buffer)
        free(m_block_buffer);
    m_block_buffer = (char*)buf;

    return ok;
}

bool
DestructiveDiskTester::openFD(bool sync)
{
    if (m_fd != -1) return false;

    /*
     * man open:
     *
     * O_DIRECT (since Linux 2.4.10)
     * Try to minimize cache effects of the I/O to and from this file.
     * In general this will degrade performance, but it is useful in special
     * situations, such as when applications do their own caching.
     * File I/O is done directly to/from user-space buffers.
     * The O_DIRECT flag on its own makes an effort
     * to transfer data synchronously, but does not give the guarantees
     * of the O_SYNC flag that data and necessary metadata are transferred.
     * To guarantee synchronous I/O, O_SYNC must be used in addition
     * to O_DIRECT.
     *
     * O_SYNC
     * Write operations on the file will complete according to the requirements
     * of synchronized I/O file integrity completion
     * (by contrast with the synchronized I/O data integrity
     * completion provided by O_DSYNC.)
     * By the time write(2) (or similar) returns, the output data and
     * associated file metadata have been transferred to the underlying
     * hardware (i.e., as though each write(2) was followed by a call
     * to fsync(2)).
     *
     */

    int fd = -1;

    m_direct_io = sync;
    if (sync)
        fd = m_device->openSync();
    else
        fd = m_device->open();

    if (fd <= 0) return false;
    m_fd = fd; //also stored in m_device

    //underlying block size is required when using O_DIRECT flag
    m_dev_block_size = getBlockSize();

#if !defined(Q_OS_WIN)
    m_old_inum = getInum(m_fd);
#endif

    return true;
}

bool
DestructiveDiskTester::openFD()
{
    return openFD(m_direct_io);
}

bool
DestructiveDiskTester::closeFD()
{
    if (m_fd == -1) return true; //already closed, that's ok (true)

    if (m_device->close())
    {
        m_fd = -1;
        return true;
    }
    else
    {
        return false;
    }
}

bool
DestructiveDiskTester::reopen()
{
    //Close and open device again
    bool ok = closeFD() && openFD();
    Debug(QS("reopened, fd = %d, ok = %d", m_fd, ok));
    if (!ok) return false;
    //after reopening, check that it's still the same device
    int new_inum = getInum(m_fd);
    if (m_old_inum != -1 && new_inum != m_old_inum)
    {
        Debug(QS("inum sanity check failed, old = %d, new = %d, have we opened another device?", m_old_inum, new_inum));
        return false;
    }
    if (getCapacity() != m_size)
    {
        Debug(QS("capacity sanity check failed, old = %lld, new = %lld", m_size, getCapacity()));
        return false;
    }

    return true;
}

void
DestructiveDiskTester::sync()
{
#if !defined(Q_OS_WIN)
    fsync(m_fd);
#elif defined(Q_OS_WIN)
    //TODO see VolumeTester::fsync()
#endif
}

bool
DestructiveDiskTester::seekFD(qint64 offset)
{
    //lseek() requires unistd.h
    //fseek() requires stdio.h - and takes a FILE*

    qint64 pos = -1;

    return m_device->seek(offset, &pos);
}

QByteArray
DestructiveDiskTester::readBlocks(int size)
{
    //read() requires unistd.h

    //C - a wonderful lang^C^C
    //So actually, the block size is usually 512 bytes:
    //static char common_buffer[512] __attribute__ ((__aligned__(512)));
    //But sometimes it's more, so we use a dynamically allocated buffer.

    QByteArray result_array;

    //Read requested size bytes - in full blocks because O_DIRECT
    //We expect that size is a positive multiple of the block size
    //because ... that's how direct i/o works and there's no reason
    //to read an unaligned number of bytes.
    //By default, read one block.
    if (!size) size = m_dev_block_size;
    assert(size % m_dev_block_size == 0);
    int read_count = 0;
    while (read_count < size)
    {
        int remaining_count = size - read_count; //numer of bytes left total
        int chunk_size = remaining_count; //number of bytes to read now
        if (chunk_size > m_dev_block_size) //if requested size > buffer
            chunk_size = m_dev_block_size; //read only what fits in buffer

        int size_received = m_device->read(m_block_buffer, chunk_size);
        if (!size_received) break; //EOF
        if (size_received < 0) return QByteArray(); //error

        result_array += QByteArray(m_block_buffer, chunk_size);
        read_count += chunk_size;
    }

    return result_array;
}

QByteArray
DestructiveDiskTester::readData(int size)
{
    //Read size number of bytes into QByteArray
    QByteArray bytes(size, (char)0);
    char *data = bytes.data();
    qint64 size_received = m_device->read(data, size);
    bytes.resize(size_received);

    return bytes;
}

bool
DestructiveDiskTester::writeBlocks(const char *bytes, qint64 size)
{
    /*
     * write()
     * EINVAL fd is attached to an object which is unsuitable for writing;
     * or the file was opened with the O_DIRECT flag, and either the address
     * specified in buf, the value specified in count,
     * or the file offset is not suitably aligned.
     *
     * man 2 open / open() | O_DIRECT
     * Under Linux 2.4, transfer sizes, the alignment of the user buffer,
     * and the file offset must all be multiples of the logical block size
     * of the filesystem.
     * Since Linux 2.6.0, alignment to the logical block size of the
     * underlying storage (typically 512 bytes) suffices.
     * The logical block size can be determined
     * using the ioctl(2) BLKSSZGET operation
     * or from the shell using the command:
     * blockdev --getss
     *
     * So if we really want to work with blocks (O_DIRECT),
     * we'd have to read a full block, update that and write it back.
     * If we first fill our internal buffer with zeros
     * and use that for writing, we could end up overwriting the area
     * following the payload size (until the end of the block).
     * But consider that we're doing a "destructive" test...
     *
     */

    int block_size = m_dev_block_size;
    assert(block_size);
    int write_count = 0;

    //Write data to device - in block-sized chunks
    //Like readData(), this is not supposed to work with unaligned chunks.
    //As above, the buffer itself is aligned as well.
    assert(size % m_dev_block_size == 0);
    while (write_count < size)
    {
        int remaining_count = size - write_count; //numer of bytes left total
        int chunk_size = remaining_count; //number of bytes to write now
        if (chunk_size > block_size) //if requested size > buffer
            chunk_size = block_size; //write only what fits in buffer

        memcpy(m_block_buffer, bytes + write_count, chunk_size);
        int written = m_device->write(m_block_buffer, chunk_size);
        if (written < 0) return false; //error
        if (written < chunk_size) return false;

        write_count += chunk_size;
    }

    return write_count == size;
}

bool
DestructiveDiskTester::writeBlocks(const QByteArray &data)
{
    //Write data to device, in blocks (direct i/o)
    return writeBlocks(data.constData(), data.size());
}

bool
DestructiveDiskTester::writeBlock(const char *bytes)
{
    //TODO SDS->writeBlock + memcpy

    const char *buffer = bytes;
    if (!buffer) buffer = m_block_buffer;
    if (useDirectIO() && (void*)buffer != (void*)m_block_buffer)
    {
        memcpy(m_block_buffer, buffer, m_dev_block_size);
        buffer = m_block_buffer;
    }

    return m_device->writeBlock(buffer);
}

bool
DestructiveDiskTester::writeData(const QByteArray &data)
{
    //Write QByteArray to device
    return m_device->write(data.constData(), data.size()) == data.size();
}

bool
DestructiveDiskTester::checkParentPid()
{
    bool parent_alive = m_parent_pid ? false : true;
#if !defined(Q_OS_WIN)
    if (m_parent_pid)
    {
        parent_alive = ::kill(m_parent_pid, 0) == 0;
    }
    if (!parent_alive)
        Debug(QS("parent process %d is dead", m_parent_pid));
#endif
    return parent_alive;
}

DestructiveDiskTester::MChunkPattern::MChunkPattern() {}
DestructiveDiskTester::MChunkPattern::MChunkPattern(const QByteArray &bytes)
{
    qint64 bs = MB;
    block = bytes.mid(0, bs);
    hash = bytes.mid(bs);
}

QByteArray
DestructiveDiskTester::MChunkPattern::toByteArray()
{
    qint64 bs = MB;
    QByteArray fixed_block = block;
    if (fixed_block.size() != bs)
        fixed_block.resize(bs);
    QByteArray bytes = fixed_block + hash;
    return bytes;
}

