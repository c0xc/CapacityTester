#include "destructivedisktester.hpp"

qint64
DestructiveDiskTester::getCapacity(int fd)
{
    qint64 capacity = 0;

    ioctl(fd, BLKGETSIZE64, &capacity);

    return capacity;
}

qint64
DestructiveDiskTester::getCapacity(const QString &dev_path)
{
    qint64 capacity = 0;

    int fd = ::open(dev_path.toLocal8Bit().data(), O_RDONLY);
    if (fd < 0) return fd; //failed to open -> return negative value
    capacity = getCapacity(fd);
    close(fd);

    //TODO support Windows?
    //statfs on Linux and GetDiskFreeSpaceEx on Windows
    //I hate Windows.

    return capacity;
}

int
DestructiveDiskTester::getBlockSize(int fd)
{
    int bs = 0;

    ioctl(fd, BLKSSZGET, &bs); //logical block size

    return bs;
}

int
DestructiveDiskTester::getInum(int fd)
{
    struct stat file_stat;
    int ret = fstat(fd, &file_stat);
    if (ret < 0) return -1;

    return file_stat.st_ino;
}

int
DestructiveDiskTester::getInum(const QString &dev_path)
{
    int inum = 0;

    int fd = ::open(dev_path.toLocal8Bit().data(), O_RDONLY);
    if (fd < 0) return fd; //failed to open -> return negative value
    inum = getInum(fd);
    close(fd);

    return inum;
}

bool
DestructiveDiskTester::isWritable(const QString &dev_path)
{
    bool is_writable = false;

    //Check if device file is writable
    //NOTE this could be a Unix/Linux alternative to checking for root
    //just in case someone has a non-root user with write access to /dev
    if (QFileInfo(dev_path).isWritable()) is_writable = true;

    return is_writable;
}

bool
DestructiveDiskTester::amIRoot()
{
    bool am_i_root = false;

    //geteuid() requires unistd.h
    if (geteuid() == 0) am_i_root = true;

    return am_i_root;
}

DestructiveDiskTester::DestructiveDiskTester(const QString &dev_path)
                     : QObject(),
                       m_fd(0),
                       m_dev_block_size(0),
                       m_old_inum(0),
                       m_block_buffer(0),
                       m_size(0),
                       m_stop_req(false),
                       m_close_reopen(true),
                       m_use_fsync(true)
{
    //Path to storage disk that will be opened, e.g., /dev/sdf
    //will be opened by path, so a new fd will be created
    m_dev_path = dev_path;
    //Open now to check if it can be opened at all
    //it will be reopened later, see that function for how we deal with that
    openFD();

    //Check if device is a USB device - it's not really the right place
    //UDiskManager udisk; //TODO replace this
    //bool is_usb = udisk.isUsbDevice(m_dev_path);
    //TODO is_usb is false in some cases?
    //Check if device is mounted //TODO
    //...

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
    m_old_inum = getInum(m_fd);

}

DestructiveDiskTester::~DestructiveDiskTester()
{
    closeFD();

    //delete[] m_block_buffer;
    if (m_block_buffer)
        free(m_block_buffer);
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
            qInfo() << "device appears to be mounted!" << mounted_dev;
            return true;
        }
    }

    return is_mounted;
}

bool
DestructiveDiskTester::isValid()
{
    if (m_dev_path.isEmpty()) return false;
    if (!QFile(m_dev_path).exists()) return false;

    //Check if device has non-zero capacity
    if (getCapacity(m_dev_path) <= 0) return false;

    return true;
}

QByteArray
DestructiveDiskTester::generateTestPattern(int pattern_size)
{
    assert(pattern_size > 0);
    QByteArray new_pattern(pattern_size, (char)0);
    srand(time(0));
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
    if (m_fd) return false;

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

    int fd = ::open(m_dev_path.toLocal8Bit().data(), O_RDWR | O_DIRECT | O_SYNC);
    if (fd <= 0) return false;
    m_fd = fd;

    //underlying block size is required when using O_DIRECT flag
    m_dev_block_size = getBlockSize(m_fd);

    return true;
}

bool
DestructiveDiskTester::closeFD()
{
    if (!m_fd) return true;

    if (::close(m_fd) == 0)
    {
        m_fd = 0;
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
    if (!ok) return false;
    //after reopening, check that it's still the same device
    if (m_old_inum && getInum(m_fd) != m_old_inum)
    {
        return false;
    }
    if (getCapacity(m_fd) != m_size)
    {
        return false;
    }

    return true;
}

bool
DestructiveDiskTester::seekFD(qint64 offset)
{
    //lseek() requires unistd.h
    //fseek() requires stdio.h - and takes a FILE*

    return ::lseek(m_fd, offset, SEEK_SET) != -1;
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

        int size_received = read(m_fd, m_block_buffer, chunk_size);
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

    //Write data to device - in block-sized chunks
    //Like readData(), this is not supposed to work with unaligned chunks.
    //As above, the buffer itself is aligned as well.
    int size = data.size();
    assert(size % m_dev_block_size == 0);
    int write_count = 0;
    while (write_count < size)
    {
        int remaining_count = size - write_count; //numer of bytes left total
        int chunk_size = remaining_count; //number of bytes to write now
        if (chunk_size > block_size) //if requested size > buffer
            chunk_size = block_size; //write only what fits in buffer

        memcpy(m_block_buffer, data.constData() + write_count, chunk_size);
        int written = write(m_fd, m_block_buffer, chunk_size);
        if (written < 0) return false; //error
        if (written < chunk_size) return false;

        write_count += chunk_size;
    }

    return write_count == size;
}

void
DestructiveDiskTester::start()
{
    //Open block device
    openFD();
    if (!m_fd) return;

    //Abort if device not accessible / invalid (empty)
    if (m_size <= 0)
    {
        emit startFailed();
        return;
    }

    //Initialize block buffer
    void *buf = 0;
    if (posix_memalign(&buf, m_dev_block_size, m_dev_block_size) != 0)
    {
        emit startFailed();
        return;
    }
    m_block_buffer = (char*)buf;

    //Generate test pattern
    m_test_pattern_n = generateTestPattern(m_dev_block_size);

    // ---
    bool ok = false;
    qint64 total_mb = m_size / MB;
    emit started(total_mb);

    //IDEA faster algorithm: binary search, check 0% (ok) 100% (nok), next 50%
    //IDEA or start by checking known limits like 64G

    ok = runGBSteps();

    emit finished(ok);
}

bool
DestructiveDiskTester::runGBSteps()
{
    bool ok = false;

    ok = runGBSteps(1);
    fsync(m_fd);
    ok = ok && reopen();
    ok = ok && runGBSteps(2);

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
    for (qint64 g = 0; g < mb_count / 1024; g++)
    {
        //Now checking first MB at g GB
        qint64 m = g * 1024;
        qint64 b = m * MB; //exact position
        if (m_stop_req) return false;

        QByteArray block_pattern = blockPattern(b);

        if (phase == 0 || phase == 1)
        {
            //Write pattern at position
            if (!seekFD(b))
            {
                //seek failed
                qDebug() << "write seek failed at" << b;
                emit writeFailed(m);
                return false;
            }
            if (writeData(block_pattern) == false)
            {
                qDebug() << "write failed at" << b;
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
                qDebug() << "read seek failed at" << b;
                emit verifyFailed(m);
                return false;
            }
            check_pattern = readData(block_pattern.size());
            if (check_pattern.isEmpty())
            {
                //empty read
                qDebug() << "read failed at" << b;
                emit verifyFailed(m);
                return false;
            }
            if (block_pattern != check_pattern)
            {
                //pattern on disk differs from test pattern => bad disk detected
                qDebug() << "pattern mismatch at" << b;
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

