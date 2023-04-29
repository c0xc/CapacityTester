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

#include "volumetester.hpp"

#if defined(_WIN32) && !defined(NO_FSYNC)
int
fsync(int fd)
{
    /*
     * Emulate fsync on platforms which lack it, primarily Windows and
     * cross-compilers like MinGW.
     *
     * This is derived from sqlite3 sources and is in the public domain.
     *
     * Written by Richard W.M. Jones <rjones.at.redhat.com>
     */

    HANDLE h = (HANDLE)_get_osfhandle(fd);
    DWORD err;

    if (h == INVALID_HANDLE_VALUE)
    {
        errno = EBADF;
        return -1;
    }

    if (!FlushFileBuffers(h))
    {
        /*
         * Translate some Windows errors into rough approximations of Unix
         * errors.  MSDN is useless as usual - in this case it doesn't
         * document the full range of errors.
         */
        err = GetLastError();
        switch (err)
        {
            /* eg. Trying to fsync a tty. */
            case ERROR_INVALID_HANDLE:
            errno = EINVAL;
            break;

            default:
            errno = EIO;
        }
        return -1;
    }

    return 0;
}
#endif

/*!
 * Checks if the provided string is a valid mountpoint.
 */
bool
VolumeTester::isValid(const QString &mountpoint)
{
    //Mountpoint defined
    if (mountpoint.isEmpty()) return false; //no, don't default to cwd

    //Check if it's (still) a mountpoint
    QStorageInfo storage(mountpoint);
    if (!storage.isValid() || storage.rootPath() != mountpoint)
    {
        //Not a valid mountpoint
        //Might be an unmounted directory (/mnt/tmp), but no mountpoint
        return false;
    }
    if (!storage.isReady())
    {
        return false;
    }

    return true;
}

/*!
 * Returns a list of available mountpoints.
 */
QStringList
VolumeTester::availableMountpoints()
{
    QStringList mountpoints;

    foreach (const QStorageInfo &storage, QStorageInfo::mountedVolumes())
    {
        QString mountpoint = storage.rootPath();
        if (!storage.isValid()) continue;
        mountpoints << mountpoint;
    }

    return mountpoints;
}

/*!
 * Constructs a VolumeTester for the specified mountpoint.
 *
 * Use availableMountpoints() to get a list of available mountpoints.
 */
VolumeTester::VolumeTester(const QString &mountpoint)
            : block_size_max(16 * MB),
              file_size_max(512 * MB),
              safety_buffer(1 * MB), //512 KB not enough for some filesystems
              req_remount(false),
              file_prefix("CAPACITYTESTER"),
              bytes_total(0),
              bytes_written(0),
              bytes_remaining(0),
              _canceled(false),
              success(true),
              error_type(Error::Unknown)
{
    //Default safety buffer
    #if defined(SAFETY_BUFFER)
    safety_buffer = SAFETY_BUFFER;
    #endif

    //Apply mountpoint if valid
    if (isValid(mountpoint))
    {
        _mountpoint = mountpoint;
    }

}

/*!
 * Changes the size of the safety buffer zone at the end.
 * This safety buffer is required for many filesystems.
 * A too small value may lead to a write error at the end
 * of the initialization phase.
 * The default value is 1 MB.
 *
 * For example, when testing a FAT32 filesystem,
 * this could be set to 0.
 * It's almost never necessary to decrease this value.
 */
bool
VolumeTester::setSafetyBuffer(int new_buffer)
{
    if (new_buffer < 0) return false;
    safety_buffer = new_buffer;
    return true;
}

void
VolumeTester::setReqRemount(bool enabled)
{
    req_remount = enabled;
}

/*!
 * Checks if this VolumeTester is still valid, i.e.,
 * if it still points to a valid mountpoint.
 */
bool
VolumeTester::isValid()
const
{
    return isValid(mountpoint());
}

/*!
 * Returns the mountpoint used by this VolumeTester.
 */
QString
VolumeTester::mountpoint()
const
{
    QString mountpoint;
    mountpoint = _mountpoint;
    return mountpoint;
}

/*!
 * Returns the total capacity of the filesystem.
 */
qint64
VolumeTester::bytesTotal()
const
{
    qint64 bytes = 0;

    QStorageInfo storage(mountpoint());
    if (storage.isValid() && storage.isReady())
    {
        bytes = storage.bytesTotal();
    }

    return bytes;
}

/*!
 * Returns the number of bytes used on the filesystem.
 */
qint64
VolumeTester::bytesUsed()
const
{
    qint64 bytes = 0;

    QStorageInfo storage(mountpoint());
    if (storage.isValid() && storage.isReady())
    {
        bytes = storage.bytesTotal() - storage.bytesFree();
    }

    return bytes;
}

/*!
 * Returns the available space on the filesystem, in bytes.
 */
qint64
VolumeTester::bytesAvailable()
const
{
    qint64 bytes = 0;

    QStorageInfo storage(mountpoint());
    if (storage.isValid() && storage.isReady())
    {
        bytes = storage.bytesAvailable();
    }

    return bytes;
}

/*!
 * Returns filesystem usage in percentage points, like 10(%).
 */
int
VolumeTester::usedPercentagePoints()
const
{
    long long used_k = bytesUsed() / 1024;
    long long total_k = bytesTotal() / 1024;
    return std::div(used_k * 100, total_k).quot;
}

/*!
 * Returns the name of the filesystem, if defined.
 * Returns an empty string otherwise.
 */
QString
VolumeTester::name()
const
{
    QString name;

    QStorageInfo storage(mountpoint());
    if (storage.isValid() && storage.isReady())
    {
        name = storage.name();
    }

    return name;
}

/*!
 * Returns a combination of mountpoint and filesystem name.
 */
QString
VolumeTester::label()
const
{
    QString label;

    if (isValid())
    {
        label = mountpoint();
        QString name = this->name();
        if (!name.isEmpty())
            label = label + " " + name;
    }

    return label;
}

/*!
 * Returns a list of QFileInfo objects, one for every file.
 * This list should be empty before a test is started.
 */
QFileInfoList
VolumeTester::entryInfoList()
const
{
    QFileInfoList files;
    if (!isValid()) return files;

    //List files in filesystem root (not recursively)
    QDir dir(mountpoint());
    files = dir.entryInfoList(
        QDir::NoDotAndDotDot | QDir::AllEntries |
        QDir::Hidden | QDir::System,
        QDir::Name | QDir::DirsFirst | QDir::IgnoreCase);

    return files;
}

/*!
 * Returns a string list of file names.
 * See entryInfoList().
 */
QStringList
VolumeTester::rootFiles()
const
{
    QStringList file_name_list;

    foreach (QFileInfo fi, entryInfoList())
    {
        QString name = fi.fileName();
        if (fi.isDir()) name += "/";
        file_name_list << name;
    }

    return file_name_list;
}

/*!
 * Returns a list of conflicting files.
 * It's not possible to start a test with those files present.
 *
 * If the program crashes during a test, the test files are not cleaned up
 * and are then considered conflict files.
 */
QStringList
VolumeTester::conflictFiles()
const
{
    QStringList conflict_files;

    assert(!file_prefix.isEmpty());

    foreach (QString name, rootFiles())
    {
        if (name.startsWith(file_prefix))
            conflict_files << name;
    }

    return conflict_files;
}

/*!
 * Starts a test.
 * A test consists of three phases:
 * 1. Initialization: The test files are created and
 * a quick test is performed.
 * 2. Write: A test pattern is written to the files.
 * 3. Verify: The files are read and compared with the pattern.
 */
void
VolumeTester::start()
{
    //Abort if mountpoint not valid anymore
    if (!isValid())
    {
        emit failed();
        emit finished();
        return;
    }

    //Test phases:
    //1 Initialization (write first and last block bytes)
    //2 Full write
    //3 Full read

    //Test files and blocks:
    //The available space is filled with test files.
    //Each file is grown to a size of file_size_max bytes
    //except for the last one, which may be smaller.
    //After the initialization, the files are filled with test data,
    //one block at a time.
    //The block size is a multiple of 1 MB (16 MB at the time of writing),
    //smaller than a file.
    //The last block in the last file may be smaller than block_size_max.

    //Block size multiple of 1024
    assert(block_size_max > 0 && block_size_max % MB == 0);

    //File size > block size, like 512 MB
    assert(file_size_max > 0 && file_size_max % MB == 0);
    assert(file_size_max > block_size_max);

    //Size of volume
    //Safety buffer used by default (e.g., 1M)
    //Some filesystems need this, otherwise write error at 100% (ENOSPC)
    bytes_total = bytesAvailable();
    bytes_total -= safety_buffer;
    bytes_written = 0;
    bytes_remaining = bytes_total;
    if (bytes_total <= 0)
    {
        //Volume full or error getting size
        emit failed(Error::Full);
        emit finished();
        return;
    }

    //Test pattern
    generateTestPattern();
    assert(pattern.size() == block_size_max);

    //Calculate file and block sizes
    QDir dir(mountpoint());
    {
        int file_count = bytes_total / file_size_max;
        int last_file_size = bytes_total % file_size_max;
        if (last_file_size) file_count++;
        file_infos.clear();
        for (int i = 0; i < file_count; i++)
        {
            //File size
            int size = file_size_max; //e.g., 512 MB
            if (i == file_count - 1 && last_file_size)
                size = last_file_size;
            assert(size > 0);

            //File area
            //long long (not just int) to prevent overflows
            qint64 pos = i * file_size_max; //NOT times current size!
            assert(pos >= 0); //int overflow may lead to negative pos
            qint64 end = pos + size;

            //File path
            QString name = file_prefix + QString::number(i);
            QString path = dir.absoluteFilePath(name);

            //File ID
            QByteArray id_bytes = QString::number(i).toUtf8();
            id_bytes.append((char)'\1');

            //File information
            FileInfo file_info;
            file_info.filename = name;
            file_info.offset = pos;
            file_info.offset_mb = pos / MB;
            file_info.size = size;
            file_info.end = end;
            file_info.id = id_bytes;

            //Blocks
            int block_count = size / block_size_max;
            int last_block_size = size % block_size_max;
            if (last_block_size) block_count++;
            for (int j = 0; j < block_count; j++)
            {
                //Block size
                int block_size = block_size_max; //e.g., 16 MB
                if (j == block_count - 1 && last_block_size)
                    block_size = last_block_size;
                assert(block_size > 0);

                //Block position
                qint64 pos = j * block_size_max; //NOT times current size!
                assert(pos >= 0);
                qint64 end = pos + block_size;

                //Block ID
                QByteArray id_bytes = QString("%1:%2").arg(i).arg(j).toUtf8();
                id_bytes.append((char)'\1');

                //Block information
                BlockInfo block_info;
                block_info.rel_offset = pos; //relative offset within file
                block_info.abs_offset = file_info.offset + pos; //absolute
                block_info.abs_offset_mb = block_info.abs_offset / MB;
                block_info.size = block_size;
                block_info.rel_end = end;
                block_info.abs_end = file_info.offset + end;
                block_info.abs_end_mb = block_info.abs_end / MB;
                block_info.id = id_bytes;

                //Add to list
                file_info.blocks << block_info;
            }
            assert(file_info.blocks.size() == block_count);

            //Add to list
            file_infos << file_info;
        }
    }

    //File objects (on heap, auto-deleted)
    //Files deleted after destroyed() immediate deletion may fail (too early)
    files_parent = new QObject(this); //scope till finished
    for (int i = 0, ii = file_infos.size(); i < ii; i++)
    {
        //Create file
        QString name = file_infos[i].filename;
        QString path = dir.absoluteFilePath(name);
        QFile *file = new QFile(path, files_parent); //with parent
        file->setProperty("NAME", name);
        connect(file,
                SIGNAL(destroyed(QObject*)),
                SLOT(removeFile(QObject*)));
        file_infos[i].file = file;
    }

    //Run tests
    bool ok = initialize() && writeFull();
    if (!ok)
    {
        //Test failed
        success = false;
        emit failed(error_type);
    }
    //Remount
    if (req_remount)
    {
        emit remountRequested(_mountpoint);
    }
    else
    {
        runVerifyStep();
    }

    //Files deleted and finished() after last file object deleted

}

/*!
 * Requests the currently running test to be aborted gracefully.
 * This will wait for the current file operation to complete.
 * The test files will be deleted normally.
 */
void
VolumeTester::cancel()
{
    error_type |= Error::Aborted;
    _canceled = true;
}

void
VolumeTester::handleRemounted(const QString &new_mountpoint)
{
    //Request completed: volume remounted at new_mountpoint
    if (new_mountpoint.isEmpty())
    {
        //Remount failed!
        emit verifyFailed(0);
        return;
    }

    //UDiskManager/DBus call fails in this thread:
    //UDiskManager diskmanager;
    //diskmanager.umount(device)...
    //QObject: Cannot create children for a parent that is in a different thread.
    //(Parent is UDiskManager(), parent's thread is QThread(), current thread is QThread()

    //Fix file paths
    if (!QFileInfo(new_mountpoint).isDir())
        return;
    _mountpoint = new_mountpoint;
    for (int i = 0, ii = file_infos.size(); i < ii; i++)
    {
        FileInfo file_info = file_infos[i];
        QFile *file = file_info.file;
        QFileInfo fi(QDir(new_mountpoint), file_info.filename);
        file->setFileName(fi.filePath());
    }

    //Continue test, which is waiting for remount
    runVerifyStep();
}

void
VolumeTester::runVerifyStep()
{
    bool ok = verifyFull();
    if (ok)
    {
        //Test succeeded
        success = true;
        emit succeeded();
    }
    else
    {
        //Test failed
        success = false;
        emit failed(error_type);
    }

    files_parent->deleteLater();
}

bool
VolumeTester::initialize()
{
    //Start
    qint64 total_mb = bytes_total / MB;
    emit initializationStarted(total_mb);

    //Create test files to fill available space
    //Last test file usually smaller (to fill space)
    uchar byte_fe = 254;
    QElapsedTimer timer_initializing;
    double initialized_mb = 0;
    double initialized_sec = 0;
    for (int i = 0, ii = file_infos.size(); i < ii; i++)
    {
        FileInfo file_info = file_infos[i];
        QFile *file = file_info.file;
        assert(file);

        //File must not exist
        if (file->exists())
        {
            //File conflict
            error_type |= Error::Create;
            emit createFailed(i, file_info.offset);
            return false;
        }

        //Create file
        if (!file->open(QIODevice::ReadWrite))
        {
            //Creating test file failed
            error_type |= Error::Create;
            if (file->error() & QFileDevice::PermissionsError)
                error_type |= Error::Permissions;
            emit createFailed(i, file_info.offset);
            return false;
        }

        //Write id
        //Usually 1 byte but could be longer
        if (!file->seek(0) ||
            file->write(file_info.id) != file_info.id.size())
        {
            //Writing id failed
            error_type |= Error::Write;
            emit writeFailed(file_info.offset_mb);
            return false;
        }

        //Grow file incrementally
        for (int j = 0, jj = file_info.blocks.size(); j < jj; j++)
        {
            BlockInfo block_info = file_info.blocks[j];

            //Start timer
            timer_initializing.start();

            //Grow file
            if (!file->resize(block_info.rel_end))
            {
                //Growing file failed
                error_type |= Error::Write;
                error_type |= Error::Resize;
                emit writeFailed(block_info.abs_offset_mb);
                return false;
            }

            //No fsync() because the initialization should be fast.
            //This may lead to high (wrong) write speed being reported.

            //Last block
            bool is_last = j == jj - 1;
            if (is_last)
            {
                //File size
                assert(file->size() == file_info.size);

                //Write last byte
                if (!file->seek(file_info.size - 1) ||
                    !file->putChar(byte_fe))
                {
                    //Writing last byte failed
                    error_type |= Error::Write;
                    emit writeFailed(block_info.abs_offset_mb);
                    return false;
                }
            }

            //Block initialized, get time
            initialized_sec += (double)timer_initializing.elapsed() / 1000;
            initialized_mb += block_info.size / MB;
            double avg_speed =
                initialized_sec ? initialized_mb / initialized_sec : 0;
            emit initialized(block_info.abs_end_mb, avg_speed);

            //Cancel gracefully
            if (abortRequested()) return false;
        }

        //Verify this file right away to speed things up
        //Don't wait for last file to be written if second already corrupted

        //Verify last byte
        char c;
        if (!file->seek(file_info.size - 1) ||
            !file->getChar(&c) || (uchar)c != byte_fe)
        {
            //Verifying last byte failed
            error_type |= Error::Verify;
            emit verifyFailed(file_info.offset_mb); //TODO possible bug, position at 0
            return false;
        }

        //Verify id
        if (!file->seek(0) ||
            file->read(file_info.id.size()) != file_info.id)
        {
            //Verifying id failed
            error_type |= Error::Verify;
            emit verifyFailed(file_info.offset_mb); //TODO possible bug, position at 0
            return false;
        }

        //Cancel gracefully
        if (abortRequested()) return false;
    }

    //Verify all files (quick test, just first and last few bytes)
    for (int i = 0, ii = file_infos.size(); i < ii; i++)
    {
        FileInfo file_info = file_infos[i];
        QFile *file = file_info.file;

        //Verify last byte
        char c;
        if (!file->seek(file_info.size - 1) ||
            !file->getChar(&c) || (uchar)c != byte_fe)
        {
            //Verifying last byte failed
            error_type |= Error::Verify;
            emit verifyFailed(file_info.offset_mb); //TODO possible bug, position at 0
            return false;
        }

        //Verify id
        if (!file->seek(0) ||
            file->read(file_info.id.size()) != file_info.id)
        {
            //Verifying id failed
            error_type |= Error::Verify;
            emit verifyFailed(file_info.offset_mb); //TODO possible bug, position at 0
            return false;
        }

        //Cancel gracefully
        if (abortRequested()) return false;
    }

    return true;
}

bool
VolumeTester::writeFull()
{
    //Start
    emit writeStarted();

    //Write test pattern
    QElapsedTimer timer_writing;
    double written_mb = 0;
    double written_sec = 0;
    for (int i = 0, ii = file_infos.size(); i < ii; i++)
    {
        FileInfo file_info = file_infos[i];
        QFile *file = file_info.file;
        int fd = file->handle();

        //Flush cache
        //Might block for a while if initialized files not on disk yet (cache)
        #ifdef USE_FSYNC
        fsync(fd);
        #endif

        //Write blocks
        for (int j = 0, jj = file_info.blocks.size(); j < jj; j++)
        {
            BlockInfo block_info = file_info.blocks[j];

            //Block data (based on pattern, with unique id)
            QByteArray block = blockData(i, j);

            //Start timer
            timer_writing.start();

            //Write block
            if (!file->seek(block_info.rel_offset) ||
                file->write(block) != block_info.size)
            {
                //Writing chunk failed
                error_type |= Error::Write;
                emit writeFailed(block_info.abs_offset_mb);
                return false;
            }

            //Flush cache
            #ifdef USE_FSYNC
            fsync(fd);
            #endif

            //Block written
            written_sec += (double)timer_writing.elapsed() / 1000;
            written_mb += block_info.size / MB;
            double avg_speed = written_sec ? written_mb / written_sec : 0;
            emit written(block_info.abs_end_mb, avg_speed);

            //Cancel gracefully
            if (abortRequested()) return false;
        }

        //Tell kernel to discard cache
        #if _XOPEN_SOURCE >= 600 || _POSIX_C_SOURCE >= 200112L
        posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
        #endif

        file->close();
    }

    return true;
}

bool
VolumeTester::verifyFull()
{
    //Read test pattern
    emit verifyStarted();
    QElapsedTimer timer_verifying;
    double verified_mb = 0;
    double verified_sec = 0;
    for (int i = 0, ii = file_infos.size(); i < ii; i++)
    {
        FileInfo file_info = file_infos[i];
        QFile *file = file_info.file;

        //Read pattern in small chunks
        for (int j = 0, jj = file_info.blocks.size(); j < jj; j++)
        {
            BlockInfo block_info = file_info.blocks[j];

            //Block data (based on pattern, with unique id)
            QByteArray block = blockData(i, j);

            //Start timer
            timer_verifying.start();

            //Re-open file
            if (!file->isOpen())
            {
                if (!file->open(QIODevice::ReadOnly))
                {
                    error_type |= Error::Verify;
                    emit verifyFailed(block_info.abs_offset_mb); //TODO possible bug, position at 0
                    return false;
                }
            }

            //Read block
            if (!file->seek(block_info.rel_offset) ||
                file->read(block_info.size) != block)
            {
                //Verifying chunk failed
                error_type |= Error::Verify;
                emit verifyFailed(block_info.abs_offset_mb); //TODO possible bug, position at 0
                return false;
            }

            //Block verified
            verified_sec += (double)timer_verifying.elapsed() / 1000;
            verified_mb += block_info.size / MB;
            double avg_speed = verified_sec ? verified_mb / verified_sec : 0;
            emit verified(block_info.abs_end_mb, avg_speed);

            //Cancel gracefully
            if (abortRequested()) return false;
        }
    }

    return true;
}

void
VolumeTester::generateTestPattern()
{
    //Test pattern
    //Pattern size < block size
    int pattern_size = block_size_max; //for example 16 MB
    assert(pattern_size > 0);
    QByteArray new_pattern(pattern_size, (char)0);
    srand(time(0));
    for (int i = 0; i < pattern_size; i++)
    {
        //Random byte except 0
        uchar byte = rand() % 254 + 1; //0 < byte < 255
        new_pattern[i] = byte;
    }
    this->pattern = new_pattern;
    assert(this->pattern.size() == pattern_size);

}

void
VolumeTester::removeFile(QObject *file_obj)
{
    //File is QFile and may be removed now
    QFile *file = qobject_cast<QFile*>(file_obj);
    //assert(file);

    //Find index of file info object
    QString name = file_obj->property("NAME").toString();
    assert(!name.isEmpty());
    int index = -1;
    for (int i = 0, ii = file_infos.size(); i < ii; i++)
    {
        if (file_infos[i].filename == name) index = i;
    }
    assert(index != -1);

    //Delete file
    bool removed = false;
    if (file) removed = file->remove();
    else removed = QFile::remove(QDir(_mountpoint).absoluteFilePath(name));
    if (!removed)
    {
        //Deleting file failed
        //Should not happen (maybe slow drive...)
        emit removeFailed(name);
    }

    //Remove info
    file_infos.removeAt(index);

    //Finish after last file
    if (file_infos.isEmpty())
    {
        emit finished(success, error_type);
    }
}

QByteArray
VolumeTester::blockData(int file_index, int block_index)
const
{
    FileInfo file_info = file_infos.at(file_index);
    BlockInfo block_info = file_info.blocks.at(block_index);

    //Block data based on test pattern
    QByteArray block = pattern;
    assert(!block.isEmpty()); //pattern must have been generated previously

    //Shrink last block
    if (block.size() > block_info.size)
        block.resize(block_info.size);
    assert(block.size() == block_info.size);
    assert(!block.isEmpty());

    //Put unique id sequence at beginning (if possible)
    if (block_info.size >= block_info.id.size())
    {
        block.replace(0, block_info.id.size(), block_info.id);
    }
    assert(block.size() == block_info.size);

    return block;
}

bool
VolumeTester::abortRequested()
const
{
    return _canceled;
}

