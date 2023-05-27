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

DestructiveDiskTester::DestructiveDiskTester(const QString &dev_path)
                     : QObject(),
                       m_fd(-1),
                       m_dev_block_size(0),
                       m_old_inum(-1),
                       m_block_buffer(0),
                       m_size(0),
                       m_stop_req(false),
                       m_close_reopen(true),
                       m_use_fsync(true)
{
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

    //Check if device is a USB device - it's not really the right place
    //UDiskManager udisk; //TODO replace this
    //bool is_usb = udisk.isUsbDevice(m_dev_path);
    //TODO is_usb is false in some cases?

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

    //delete[] m_block_buffer;
    if (m_block_buffer)
        free(m_block_buffer);
}

qint64
DestructiveDiskTester::getCapacity()
{
    qint64 capacity = 0;

#if !defined(Q_OS_WIN)

    //Within this class instance, we prefer to query by file descriptor,
    //specifically to prevent something like /dev/sdx now pointing
    //to another device than it did earlier.
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

    //TODO StorageDiskSelection::Device isWritable / try open
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
    //TODO
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

QByteArray
DestructiveDiskTester::generateTestPattern(int pattern_size)
{
    assert(pattern_size > 0);
    QByteArray new_pattern(pattern_size, (char)0);
#if !defined(Q_OS_WIN)
    srand(time(0));
#elif defined(Q_OS_WIN)
    //TODO
#endif
    for (int i = 0; i < pattern_size; i++)
    {
        //Random byte except 0
        uchar byte = rand() % 254 + 1; //0 < byte < 255
        new_pattern[i] = byte;
    }

    assert(new_pattern.size() == pattern_size);

    return new_pattern;
}

QByteArray
DestructiveDiskTester::blockPattern(int block)
{
    QByteArray pattern = m_test_pattern_n;
    
    //Put unique id sequence at beginning
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

bool
DestructiveDiskTester::openFD()
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

#if !defined(Q_OS_WIN)

    fd = m_device->open(O_RDWR | O_DIRECT | O_SYNC);

#elif defined(Q_OS_WIN)

    fd = m_device->open();

#endif

    if (fd <= 0) return false;
    m_fd = fd; //also stored in m_device

    //underlying block size is required when using O_DIRECT flag
    m_dev_block_size = getBlockSize();

    return true;
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

bool
DestructiveDiskTester::seekFD(qint64 offset)
{
    //lseek() requires unistd.h
    //fseek() requires stdio.h - and takes a FILE*

    qint64 pos = -1;

    return m_device->seek(offset, &pos);
}

QByteArray
DestructiveDiskTester::readData(int size)
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
    assert(size % m_dev_block_size == 0);
    int read_count = 0;
    while (read_count < size)
    {
        int remaining_count = size - read_count; //numer of bytes left total
        int chunk_size = remaining_count; //number of bytes to read now
        if (chunk_size > m_dev_block_size) //if requested size > buffer
            chunk_size = m_dev_block_size; //read only what fits in buffer

#if !defined(Q_OS_WIN)
        int size_received = read(m_fd, m_block_buffer, chunk_size);
#elif defined(Q_OS_WIN)
        int size_received = _read(m_fd, m_block_buffer, chunk_size);
#endif
        if (!size_received) break; //EOF
        if (size_received < 0) return QByteArray(); //error

        result_array += QByteArray(m_block_buffer, chunk_size);
        read_count += chunk_size;
    }

    return result_array;
}

bool
DestructiveDiskTester::writeData(const QByteArray &data)
{
    //write() requires unistd.h

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
    int size = data.size();
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

        memcpy(m_block_buffer, data.constData() + write_count, chunk_size);
#if !defined(Q_OS_WIN)
        int written = write(m_fd, m_block_buffer, chunk_size);
#elif defined(Q_OS_WIN)
        int written = _write(m_fd, m_block_buffer, chunk_size);
#endif
        if (written < 0) return false; //error
        if (written < chunk_size) return false;

        write_count += chunk_size;
    }

    return write_count == size;
}

void
DestructiveDiskTester::start()
{
    Debug(QS("starting test..."));
    //Open block device
    openFD();
    Debug(QS("opened dev, fd = %d", m_fd));
    if (m_fd == -1) return;
#if !defined(Q_OS_WIN)
    m_old_inum = getInum(m_fd);
#endif

    //Abort if device not accessible / invalid (empty)
    if (m_size <= 0)
    {
        Debug(QS("cannot start test - disk has a size of zero or unknown"));
        emit startFailed();
        return;
    }

    //Initialize block buffer
    void *buf = 0;
#if !defined(Q_OS_WIN)
    if (posix_memalign(&buf, m_dev_block_size, m_dev_block_size) != 0)
    {
        emit startFailed();
        return;
    }
#else
    if (!(buf = malloc(m_dev_block_size)))
    {
        emit startFailed();
        return;
    }
#endif
    m_block_buffer = (char*)buf;

    //Generate test pattern
    m_test_pattern_n = generateTestPattern(m_dev_block_size);

    // ---
    bool ok = false;
    qint64 total_mb = m_size / MB;
    Debug(QS("total size: %lld MB", total_mb));
    emit started(total_mb);

    //IDEA faster algorithm: binary search, check 0% (ok) 100% (nok), next 50%
    //IDEA or start by checking known limits like 64G

    ok = runGBSteps();

    Debug(QS("completed, ok = %d", ok));
    emit finished(ok);
}

bool
DestructiveDiskTester::runGBSteps()
{
    bool ok = false;

    ok = runGBSteps(1);
    Debug(QS("write phase done, ok = %d", ok));
#if !defined(Q_OS_WIN)
    fsync(m_fd);
#elif defined(Q_OS_WIN)
    //TODO see VolumeTester::fsync()
#endif
    ok = ok && reopen();
    Debug(QS("write reopened (flushed), ok = %d", ok));
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
    //TODO check last bytes, last block
    for (qint64 g = 0; g < gb_count - 1; g++) //last GB not checked
    {
        //Now checking first MB at g GB
        qint64 m = g * 1024;
        qint64 b = m * MB; //exact position
        if (m_stop_req)
        {
            Debug(QS("stop requested at %llu / %lld GB", b, g));
            return false;
        }

        QByteArray block_pattern = blockPattern(b);

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
            if (writeData(block_pattern) == false)
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
            check_pattern = readData(block_pattern.size());
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

void
DestructiveDiskTester::cancel()
{
    m_stop_req = true;
}

