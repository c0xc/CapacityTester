/****************************************************************************
**
** Copyright (C) 2023 Philip Seeger (p@c0xc.net)
** This file is part of CapacityTester.
** This module was developed for CapacityTester.
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

#define DEFINE_SDS_INC
#include "storagediskselection.hpp"

//The compiler macros are not indented to match the coding style of the project
//and because they're interpreted by the preprocessor, not the compiler.

StorageDiskSelection::Access::Access()
{
#if defined(Q_OS_WIN)
    //Look how ugly Windows is
    //Apparently, this function is hidden and
    //you need to pull it out of the sink wearing gloves.
    SetupDiGetDevicePropertyW = (FN_SetupDiGetDevicePropertyW)GetProcAddress(GetModuleHandle(TEXT("Setupapi.dll")), "SetupDiGetDevicePropertyW");
    //Micro Soft... it's funny, right? "micro soft"
#endif
}

StorageDiskSelection::Access::~Access()
{
#if !defined(Q_OS_WIN)
#elif defined(Q_OS_WIN)

    while (m_d_handles.size())
    {
        auto dev_handle = m_d_handles.back();
        m_d_handles.pop_back();
        CloseHandle(dev_handle);
    }

    SetupDiDestroyDeviceInfoList(m_h_dev_info_set);

#endif

}

#if defined(Q_OS_WIN)

//Normally, all classes are declared in the header file, but this is a special case
struct DRIVE_LAYOUT_INFORMATION_EX_FIXED : DRIVE_LAYOUT_INFORMATION_EX
{
    PARTITION_INFORMATION_EX PartitionEntry[99]; //base class has too few entries
};

QString
StorageDiskSelection::Access::toString(const wchar_t *str_wchar)
{
    //LPTSTR is MS's version of the w_char_t* data type, i.e., wide characters
    //LPCWSTR = const wchar_t*

    QString text;

    text = QString::fromWCharArray(str_wchar);

    return text;
}

QString
StorageDiskSelection::Access::sometimesWideCharToString(LPCTSTR str_tchar)
{
    //LPTSTR is MS's version of the char* data type
    //which may sometimes be wchar_t* depending on UNICODE or _UNICODE
    //LPCTSTR = const LPTSTR = const char* or const wchar_t*
    //https://softwareengineering.stackexchange.com/a/194768

    QString text;

#ifdef UNICODE
    text = QString::fromWCharArray(str_tchar);
#else
    text = QString::fromLocal8Bit(str_tchar)
#endif

    return text;
}

QString
StorageDiskSelection::Access::formatWinError(DWORD dw)
{
    LPVOID win_msg_buf; //char* or wchar_t*

    if (!dw) dw = GetLastError(); 

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&win_msg_buf,
        0, NULL
    );
    QString msg = sometimesWideCharToString((LPCTSTR)win_msg_buf);
    LocalFree(win_msg_buf);

    return msg;
}

#endif

StorageDiskSelection::Device::Device()
{
    m_fd = -1;
#if defined(Q_OS_WIN)
    m_index = -1;
    m_win_handle = INVALID_HANDLE_VALUE;
#endif
}

StorageDiskSelection::Device::Device(QSharedPointer<Access> selection, QString addr)
                            : Device() //no owner other than shared pointer
{
    //addr is an internal address
    //in the case of our UDiskManager it's "sda" for the device /dev/sda,
    //which would have the udisks address:
    // /org/freedesktop/UDisks2/block_devices/sda.
    m_int_addr = addr;

    //Enumerator (which created this Device instance) is used to access
    //central handles.
    //Therefore we store a shared pointer to it to ensure its existence.
    //It is required that the Enumerator that created this object
    //remains alive as long as this object exists.
    m_enumerator = selection;

}

StorageDiskSelection::Device::Device(const Device &other)
                            : Device()
{
    //Copy constructor (we tried to keep the number of member variables
    //that would have to be copied to a minimum)
    m_enumerator = other.m_enumerator;
    m_int_addr = other.m_int_addr;
    m_fd = other.m_fd;
#if defined(Q_OS_WIN)
    m_index = other.m_index;
    m_win_handle = other.m_win_handle;
#endif
}

StorageDiskSelection::Device::~Device()
{
#if defined(Q_OS_WIN)
    if (m_win_handle != INVALID_HANDLE_VALUE)
        CloseHandle(m_win_handle);
#endif
}

bool
StorageDiskSelection::Device::isNull()
{
    return !m_enumerator || m_int_addr.isEmpty();
}

bool
StorageDiskSelection::Device::isValid()
{
    bool is_valid = false;
    if (isNull()) return false;

#if !defined(Q_OS_WIN)

    //Unlike lstat(), stat() follows links (/dev/by-id/...)
    QString filepath = path();
    struct stat st;
    bool success = stat(filepath.toLocal8Bit().constData(), &st) == 0;
    if (success)
    {
        is_valid = S_ISBLK(st.st_mode);
    }

#elif defined(Q_OS_WIN)

    is_valid = driveHandle() != INVALID_HANDLE_VALUE;

#endif

    return is_valid;
}

QString
StorageDiskSelection::Device::path()
{
    //Get the real path to the device file
    //so if we use "sda" internally, resolve it to /dev/sda

    QString dev_path = m_int_addr;

    assert(m_enumerator);

#if !defined(Q_OS_WIN) //normal OS

    UDiskManager &udisk = m_enumerator->m_udisk;
    dev_path = udisk.deviceFilePath(m_int_addr);

#elif defined(Q_OS_WIN)

    //dev_path

#endif

    return dev_path;
}

QString
StorageDiskSelection::Device::identifier()
{
    //Get identifier string
    //Id: "SDK-PSSD-SSD-3.0-3457661003149006514"
    QString id;

    auto a = m_enumerator;
    assert(a);

#if !defined(Q_OS_WIN) //normal OS

    UDiskManager &udisk = m_enumerator->m_udisk;
    id = udisk.driveId(m_int_addr);

#elif defined(Q_OS_WIN)

    char inst_id_str[MAX_DEVICE_ID_LEN]; //<cfgmgr32.h>
    CONFIGRET cm_status;

    assert(a->m_interfaces.size() > m_index);
    Access::Interface if_container = a->m_interfaces.at(m_index);
    auto dev_info_ptr = if_container.m_devinfo_data;

    //Device Instance ID
    cm_status = CM_Get_Device_IDA(dev_info_ptr->DevInst, inst_id_str, MAX_PATH, 0);
    if (cm_status == CR_SUCCESS)
    {
        id = inst_id_str;
    }

#endif

    return id;
}

bool
StorageDiskSelection::Device::isUsbDevice(bool *error_ref)
{
    bool is_usb = false;

    assert(m_enumerator);

#if !defined(Q_OS_WIN) //normal OS

    UDiskManager &udisk = m_enumerator->m_udisk;
    is_usb = udisk.isUsbDevice(m_int_addr);

#elif defined(Q_OS_WIN)

    HANDLE dev_handle = driveHandle();

    //Normally, we'd use TYPE x = {} to zero-initialize a struct
    //but GCC4 produces misleading warnings when this syntax is used:
    //missing initializer
    //It might make sense if only part of the struct would be initialized.
    //To reduce the number of warnings, we do not initialize the structs
    //and use memset() to overwrite/clear it.
    //This is a bit risky. But when the struct basically only
    //contains members of type int, it shouldn't matter.
    STORAGE_DESCRIPTOR_HEADER struct_header; // = {}; "missing initializer"
    memset(&struct_header, 0, sizeof(STORAGE_DESCRIPTOR_HEADER));
    STORAGE_DEVICE_DESCRIPTOR struct_desc; // = {};
    memset(&struct_desc, 0, sizeof(STORAGE_DEVICE_DESCRIPTOR));
    DWORD rsize = 0;
    STORAGE_PROPERTY_QUERY stor_prop_query; // = {};
    memset(&stor_prop_query, 0, sizeof(STORAGE_PROPERTY_QUERY));
    stor_prop_query.QueryType = PropertyStandardQuery;
    stor_prop_query.PropertyId = StorageDeviceProperty;

    bool success = false;
    success = DeviceIoControl(dev_handle,
        IOCTL_STORAGE_QUERY_PROPERTY,
        &stor_prop_query, sizeof(STORAGE_PROPERTY_QUERY),
        &struct_header, sizeof(struct_header),
        &rsize,
        NULL
    );
    if (!success)
    {
        //if (error_ref) //TODO
    }
    DWORD out_size = struct_header.Size;

    //std::unique_ptr, std::shared_ptr: -std=c++11 required (GCC8+ recommended)
    std::unique_ptr<BYTE[]> out_buffer{new BYTE[out_size]{}};
    memset(out_buffer.get(), 0, out_size);
    success = DeviceIoControl(dev_handle,
        IOCTL_STORAGE_QUERY_PROPERTY,
        &stor_prop_query, sizeof(STORAGE_PROPERTY_QUERY),
        out_buffer.get(), out_size,
        &rsize,
        NULL
    );
    if (!success)
    {
        //error //TODO formatWinError
        return is_usb;
    }
    STORAGE_DEVICE_DESCRIPTOR *stor_dev_descriptor = (STORAGE_DEVICE_DESCRIPTOR*)out_buffer.get();
    bool is_removable = stor_dev_descriptor->RemovableMedia;

    //STORAGE_BUS_TYPE enumeration (winioctl.h)
    is_usb = stor_dev_descriptor->BusType == BusTypeUsb;

#endif

    return is_usb;
}

qint64
StorageDiskSelection::Device::capacity(bool *error_ref)
{
    qint64 size = 0;

    assert(m_enumerator);

#if !defined(Q_OS_WIN) //normal OS

    UDiskManager &udisk = m_enumerator->m_udisk;
    size = udisk.capacity(m_int_addr);

#elif defined(Q_OS_WIN)

    HANDLE dev_handle = driveHandle();

    //using = {}, which is allowed in C++11 and shorter than = {0}, causes:
    //warning: missing initializer
    DISK_GEOMETRY_EX struct_geom = {}; //"missing initializer" in GCC4
    bool success = false;
    DWORD rsize = 0;
    success = DeviceIoControl(
        dev_handle,
        IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
        0, 0,
        &struct_geom, sizeof(struct_geom),
        &rsize,
        0
    );
    if (!success)
    {
        //if (error_ref)
        return size;
    }

    size = struct_geom.DiskSize.QuadPart;

#endif

    return size;
}

QString
StorageDiskSelection::Device::displayModelName()
{
    QString name;

    auto a = m_enumerator;
    assert(a);

#if !defined(Q_OS_WIN) //normal OS

    UDiskManager &udisk = a->m_udisk;
    name = udisk.driveModelName(m_int_addr);

#elif defined(Q_OS_WIN)

    assert((int)a->m_interfaces.size() > m_index);
    auto dev_info_ptr = a->m_interfaces.at(m_index).m_devinfo_data;

    QString default_description;
    if (!checkWinDevDescProps(&default_description))
    {
        //Error: Device description properties not available
        name = default_description; //generic description or blank
        return name;
    }

    //Device Friendly Name (preferred display name, if available)
    unsigned char out_buffer[4096];
    DEVPROPTYPE prop_type = 0;
    DWORD rsize = 0;
    bool success = a->SetupDiGetDevicePropertyW(
        a->m_h_dev_info_set, dev_info_ptr.get(),
        &DEVPKEY_Device_FriendlyName,
        &prop_type, (BYTE*)out_buffer, sizeof(out_buffer),
        &rsize, 0
    );
    if (success)
    {
        name = Access::toString((const wchar_t*)out_buffer);
    }

#endif

    return name;
}

QString
StorageDiskSelection::Device::vendorName()
{
    QString name;

    auto a = m_enumerator;
    assert(a);

#if !defined(Q_OS_WIN) //normal OS

    UDiskManager &udisk = a->m_udisk;
    name = udisk.driveVendorName(m_int_addr);

#elif defined(Q_OS_WIN)

    assert((int)a->m_interfaces.size() > m_index);
    auto dev_info_ptr = a->m_interfaces.at(m_index).m_devinfo_data;

    if (!checkWinDevDescProps()) //description properties not available
        return name;

    //This may return "(Standard disk drives)" instead of the vendor name
    unsigned char out_buffer[4096];
    DEVPROPTYPE prop_type = 0;
    DWORD rsize = 0;
    bool success = a->SetupDiGetDevicePropertyW(
        a->m_h_dev_info_set, dev_info_ptr.get(),
        &DEVPKEY_Device_Manufacturer,
        &prop_type, (BYTE*)out_buffer, sizeof(out_buffer),
        &rsize, 0
    );
    if (success)
    {
        name = Access::toString((const wchar_t*)out_buffer);
    }

#endif

    return name;
}

QString
StorageDiskSelection::Device::serialNumber()
{
    QString serialno;

    auto a = m_enumerator;
    assert(a);

#if !defined(Q_OS_WIN) //normal OS

    UDiskManager &udisk = a->m_udisk;
    serialno = udisk.driveSerialNumber(m_int_addr);

#elif defined(Q_OS_WIN)

    //The struct returned by this function has invalid data instead of a serial
    //DeviceIoControl(dev_handle, IOCTL_STORAGE_QUERY_PROPERTY, ...
    //It's used in isUsbDevice() above
    //
    //And IOCTL_STORAGE_GET_MEDIA_SERIAL_NUMBER does not work either:
    //Error 50: The request is not supported.
    //
    //TODO return error of type not_implemented or something like that

    //Alternatively, the so-called instance id may contain the serial number.
    //It would be between the last backslash and the following &.
    if (serialno.isEmpty())
    {
        QString id_str = identifier();
        QRegExp rx(".*[\\\\](\\w+)[&][^&\\\\]*$");
        if (rx.indexIn(id_str) != -1)
        {
            //This may or may not be the serial number, extracted from the id
            serialno = rx.cap(1);
        }
    }

#endif

    return serialno;
}

int
StorageDiskSelection::Device::blockSize()
{
    int logical_bs = 0;

#if !defined(Q_OS_WIN) //normal OS

    //This does not work without root privileges.
    QString dev_path = path();
    int fd = ::open(dev_path.toLocal8Bit().data(), O_RDONLY);
    if (fd > -1)
    {
        ioctl(fd, BLKSSZGET, &logical_bs); //logical block size
        ::close(fd);
    }

#elif defined(Q_OS_WIN)

    HANDLE dev_handle = driveHandle();

    DISK_GEOMETRY struct_geom = {}; //"missing initializer" in GCC4
    bool success = false;
    DWORD rsize = 0;
    success = DeviceIoControl(
        dev_handle,
        IOCTL_DISK_GET_DRIVE_GEOMETRY,
        0, 0,
        &struct_geom, sizeof(struct_geom),
        &rsize,
        0
    );
    if (!success)
    {
        //if (error_ref)
        return logical_bs;
    }

    logical_bs = struct_geom.BytesPerSector;

#endif

    return logical_bs;
}

int
StorageDiskSelection::Device::open(int flags)
{
    int fd = -1;
    if (m_fd > -1)
    {
        return m_fd;
    }

    //Open in READ+WRITE mode by default
    //other mode flags are only partially supported, see below

    //Open block device
    QString dev_path = path();

#if !defined(Q_OS_WIN) //normal OS

    if (!flags)
        flags = O_RDWR;
    fd = ::open(dev_path.toLocal8Bit().constData(), flags);

#elif defined(Q_OS_WIN)

    //The ownership of the OS HANDLE is transferred to this file descriptor.
    //So it won't be necessary to call CloseHandle() after close().

    DWORD access_mode = 0;
    if (!flags)
    {
        access_mode = GENERIC_READ | GENERIC_WRITE; //CreateFileA()
        flags = _O_RDWR; //open()
    }
    DWORD share_mode = access_mode ? 0 : FILE_SHARE_READ | FILE_SHARE_WRITE;
    DWORD flags_attrs = 0;
    HANDLE handle = CreateFileA(
        m_int_addr.toLocal8Bit().constData(),
        access_mode,
        share_mode,
        NULL, //security attributes
        OPEN_EXISTING,
        flags_attrs, //flags and attributes
        NULL
    );
    if (handle == INVALID_HANDLE_VALUE)
        return -1;

    fd = _open_osfhandle((intptr_t)handle, flags); // _O_RDWR

#endif

    //Save fd for later (but it's not closed automatically by default)
    m_fd = fd;

    return fd;
}

int
StorageDiskSelection::Device::openSync()
{
    int fd = -1;
    if (m_fd > -1)
    {
        return m_fd;
    }

    //Open block device, use synchronized I/O
    QString dev_path = path();

    int flags = 0;

#if !defined(Q_OS_WIN) //normal OS

    flags = O_RDWR | O_DIRECT | O_SYNC;
    fd = ::open(dev_path.toLocal8Bit().constData(), flags);

#elif defined(Q_OS_WIN)

    //The ownership of the OS HANDLE is transferred to this file descriptor.
    //So it won't be necessary to call CloseHandle() after close().

    DWORD access_mode = 0;
    access_mode = GENERIC_READ | GENERIC_WRITE; //CreateFileA()
    flags = _O_RDWR; //open()
    DWORD share_mode = access_mode ? 0 : FILE_SHARE_READ | FILE_SHARE_WRITE;
    DWORD flags_attrs = FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH;
    HANDLE handle = CreateFileA(
        m_int_addr.toLocal8Bit().constData(),
        access_mode,
        share_mode,
        NULL, //security attributes
        OPEN_EXISTING,
        flags_attrs, //flags and attributes
        NULL
    );
    if (handle == INVALID_HANDLE_VALUE)
        return -1;

    fd = _open_osfhandle((intptr_t)handle, flags); // _O_RDWR

#endif

    //Save fd for later (not auto-closed by default)
    m_fd = fd;

    return fd;
}

bool
StorageDiskSelection::Device::close()
{
    int fd = m_fd;
    bool success = false;

#if !defined(Q_OS_WIN) //normal OS
    success = ::close(fd) == 0;
#elif defined(Q_OS_WIN)
    success = _close(fd) == 0;
#endif

    if (success)
        m_fd = -1;

    return success;
}

bool
StorageDiskSelection::Device::seek(qint64 offset, qint64 *pos_ptr)
{
    qint64 pos = -1;

#if !defined(Q_OS_WIN)
    pos = ::lseek(m_fd, offset, SEEK_SET);
#elif defined(Q_OS_WIN)
    pos = _lseeki64(m_fd, offset, SEEK_SET);
    //The operation completed successfully.
#endif

    if (pos_ptr)
        *pos_ptr = pos;

    return pos != -1;
}

qint64
StorageDiskSelection::Device::write(const char *bytes, qint64 size)
{
    qint64 bytes_written = 0;

#if !defined(Q_OS_WIN)
    bytes_written = ::write(m_fd, bytes, size);
#elif defined(Q_OS_WIN)
    bytes_written = ::_write(m_fd, bytes, size);
#endif

    return bytes_written;
}

qint64
StorageDiskSelection::Device::read(char *bytes, qint64 size)
{
    qint64 bytes_read = 0;

#if !defined(Q_OS_WIN)
    bytes_read = ::read(m_fd, bytes, size);
#elif defined(Q_OS_WIN)
    bytes_read = ::_read(m_fd, bytes, size);
#endif

    return bytes_read;
}

bool
StorageDiskSelection::Device::writeBlock(const char *bytes)
{
    qint64 bytes_written = 0;

    static qint64 bs = blockSize();

    //memcpy(m_block_buffer, data.constData() + write_count, chunk_size); //sync = true
    bytes_written = write(bytes, bs);

    return bytes_written == bs;
}

bool
StorageDiskSelection::Device::readBlock(char *bytes)
{
    qint64 bytes_read = 0;

    static qint64 bs = blockSize();

    bytes_read = read(bytes, bs);

    return bytes_read == bs;
}

StorageDiskSelection::PartitionTableType
StorageDiskSelection::Device::partitionTableType()
{
    PartitionTableType table_type = PartitionTableType::Unknown;

    auto a = m_enumerator;
    assert(a);

#if !defined(Q_OS_WIN)

    UDiskManager &udisk = a->m_udisk;
    UDiskManager::PartitionTableInfo table_info{};
    udisk.partitionDevices(m_int_addr, false, &table_info);
    if (table_info.type == "dos")
        table_type = PartitionTableType::MBR;
    else if (table_info.type == "gpt")
        table_type = PartitionTableType::GPT;

#elif defined(Q_OS_WIN)

    //IOCTL_DISK_GET_DRIVE_LAYOUT_EX
    HANDLE knob = driveHandle();
    DRIVE_LAYOUT_INFORMATION_EX_FIXED st_parts_layout;
    DWORD rsize = 0;
    DeviceIoControl(
        knob,
        IOCTL_DISK_GET_DRIVE_LAYOUT_EX,
        0, 0,
        &st_parts_layout, sizeof(st_parts_layout),
        &rsize,
        NULL
    );
    if (rsize)
    {
        if (st_parts_layout.PartitionStyle == PARTITION_STYLE_MBR)
            table_type = PartitionTableType::MBR;
        else if (st_parts_layout.PartitionStyle == PARTITION_STYLE_GPT)
            table_type = PartitionTableType::GPT;
    }

#endif

    return table_type;
}

QList<StorageDiskSelection::PartitionGeometry>
StorageDiskSelection::Device::partitions()
{
    QList<PartitionGeometry> partitions;

    auto a = m_enumerator;
    assert(a);

#if !defined(Q_OS_WIN)

    UDiskManager &udisk = a->m_udisk;
    UDiskManager::PartitionTableInfo table_info{};
    udisk.partitionDevices(m_int_addr, false, &table_info);

    foreach (UDiskManager::PartitionInfo table_part, table_info.partitions)
    {
        PartitionGeometry part{};

        part.start = table_part.start;
        part.size = table_part.size;
        part.number = table_part.number;
        partitions << part;
    }

#elif defined(Q_OS_WIN)

    //IOCTL_DISK_GET_DRIVE_LAYOUT_EX
    HANDLE knob = driveHandle();
    DRIVE_LAYOUT_INFORMATION_EX_FIXED st_parts_layout;
    DWORD rsize = 0;
    DeviceIoControl(
        knob,
        IOCTL_DISK_GET_DRIVE_LAYOUT_EX,
        0, 0,
        &st_parts_layout, sizeof(st_parts_layout),
        &rsize,
        NULL
    );
    if (!rsize)
    {
        //error //TODO formatWinError
        return partitions;
    }

    int parts_count_max = st_parts_layout.PartitionCount; //MBR: 4
    for (int j = 0; j < parts_count_max; j++)
    {
        PartitionGeometry part{};

        if (st_parts_layout.PartitionStyle == PARTITION_STYLE_MBR)
        {
            if (st_parts_layout.PartitionEntry[j].Mbr.PartitionType == PARTITION_ENTRY_UNUSED)
                continue;
        }

        part.start = st_parts_layout.PartitionEntry[j].StartingOffset.QuadPart;
        part.size = st_parts_layout.PartitionEntry[j].PartitionLength.QuadPart;
        part.number = st_parts_layout.PartitionEntry[j].PartitionNumber;
        partitions << part;
    }

#endif

    return partitions;
}

StorageDiskSelection::PartitionGeometry
StorageDiskSelection::Device::partition(int number)
{
    PartitionGeometry part{};

    foreach (PartitionGeometry part, partitions())
    {
        if (part.number == number)
        {
            return part;
        }
    }

    return part; //empty
}

QList<StorageDiskSelection::FilesystemInfo>
StorageDiskSelection::Device::filesystems()
{
    QList<FilesystemInfo> filesystems;

    auto a = m_enumerator;
    assert(a);

#if !defined(Q_OS_WIN)

    UDiskManager &udisk = a->m_udisk;
    UDiskManager::PartitionTableInfo table_info{};
    udisk.partitionDevices(m_int_addr, false, &table_info);

    foreach (UDiskManager::PartitionInfo table_part, table_info.partitions)
    {
        //Skip partitions which have no valid filesystem interface
        //Unfortunately, we cannot check for a size of 0 because some (exfat) report 0
        if (!table_part.fs) continue;

        //Unfortunately, we cannot determine the size of the filesystem
        //because this information is not available in the UDiskManager (via udisks).
        //It will be available in the future, with libparted, but for now we have to skip this.
        //Use the partition size instead. The filesystem usually isn't significantly smaller.

        FilesystemInfo fs_info{};

        fs_info.path = table_part.path;
        fs_info.mountpoint = udisk.mountpoint(table_part.path);
        fs_info._part_start = table_part.start;
        filesystems << fs_info;
    }

#elif defined(Q_OS_WIN)

    //List "volumes", i.e., known and mounted filesystems (letter-mounted)
    int our_dev_num = deviceNumber();
    int required_size = MAX_PATH;
    std::vector<wchar_t> vol_name_bytes(required_size);
    wchar_t *vol_name_data = (wchar_t*)vol_name_bytes.data();
    HANDLE vol_query_knob = FindFirstVolumeW(vol_name_data, required_size);
    bool vol_query_ok = vol_query_knob != INVALID_HANDLE_VALUE;
    while (vol_query_ok)
    {
        //path example:
        //\\?\ide#diskqemu_harddisk___________________________2.5+____#5&3a2a5854&0&1.0.0#{53f56307-b6bf-11d0-94f2-00a0c91efb8b}
        QString vol_path = Access::toString(vol_name_data);

        //Get volume letter (mount point)
        std::vector<wchar_t> vol_letter_bytes(required_size);
        DWORD retlen = 0;
        GetVolumePathNamesForVolumeNameW((const wchar_t*)vol_name_data, vol_letter_bytes.data(), required_size, &retlen);
        QString letter_str = Access::toString((const wchar_t*)vol_letter_bytes.data()); //array?

        //Prepare/fix path
        if (letter_str.length() == 3)
            letter_str = letter_str.mid(0, 2); //remove trailing backslash
        if (vol_path.endsWith("\\"))
            vol_path.chop(1); //remove trailing backslash! otherwise the following call fails
        //Filesystem found, but not necessarily on the current partition
        FilesystemInfo fs_info{}; //zero-initialize because of partition offset member
        fs_info.path = vol_path;
        fs_info.mountpoint = letter_str;

        //Open volume, get info on disk
        DWORD access_mode = 0;
        DWORD share_mode = access_mode ? 0 : FILE_SHARE_READ | FILE_SHARE_WRITE;
        HANDLE knob = CreateFileA(vol_path.toUtf8().constData(),
            0, //access_mode
            FILE_SHARE_READ | FILE_SHARE_WRITE, //share_mode
            NULL, //security attributes
            OPEN_EXISTING,
            0, //flags and attributes
            NULL
        );
        if (knob != INVALID_HANDLE_VALUE)
        {
            int required_size = 1000 + sizeof(VOLUME_DISK_EXTENTS);
            std::vector<char> extents_bytes(required_size);
            VOLUME_DISK_EXTENTS *extents_ptr = reinterpret_cast<VOLUME_DISK_EXTENTS*>(extents_bytes.data());
            long unsigned int retlen = 0;
            bool ok = DeviceIoControl(knob,
                IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                0, 0,
                extents_ptr, required_size,
                &retlen, 0
            );
            if (ok && extents_ptr->NumberOfDiskExtents)
            {
                auto st_extent = extents_ptr->Extents[0];
                int disknum = st_extent.DiskNumber;
                qint64 ext_start = st_extent.StartingOffset.QuadPart;
                qint64 ext_length = st_extent.ExtentLength.QuadPart;

                if (disknum == our_dev_num)
                {
                    //filesystem is on this device, one of its partitions
                    fs_info._part_start = ext_start;
                    filesystems << fs_info;
                }
            }
        }
        else
        {
            //TODO store error using Access::formatWinError()
        }
        CloseHandle(knob);

        vol_query_ok = FindNextVolumeW(vol_query_knob, vol_name_data, required_size);
        if (!vol_query_ok)
        {
            FindVolumeClose(vol_query_knob);
        }
    }

#endif

    return filesystems;
}

int
StorageDiskSelection::Device::partitionNumberOfFilesystem(const QString &filesystem_path)
{
    int number = -1;
    FilesystemInfo fs_info;

    //Get filesystem info
    foreach (FilesystemInfo fs, filesystems())
    {
        if (fs.path == filesystem_path)
        {
            fs_info = fs;
            break;
        }
    }
    if (fs_info.path.isEmpty())
        return number;
    
    foreach (PartitionGeometry part, partitions())
    {
        if (fs_info._part_start == part.start)
        {
            number = part.number;
            break;
        }
    }

    return number;
}

QString
StorageDiskSelection::Device::mountpoint(QStringList *mountpoints_ptr)
{
    QString mountpoint;
    QStringList mountpoints; //list of mountpoints but only 1 each

    foreach (auto fs, filesystems())
    {
        if (fs.mountpoint.isEmpty()) continue;

        if (mountpoint.isEmpty())
        {
            mountpoint = fs.mountpoint; //first one
        }
        mountpoints << fs.mountpoint;
    }

    if (mountpoints_ptr)
        *mountpoints_ptr = mountpoints;
    return mountpoint;
}

bool
StorageDiskSelection::Device::mount(const QString &dev_path, QString *message_ref)
{
    bool success = false;

    //TODO check if dev_path is (a partition of) this device, also accept partition number

#if !defined(Q_OS_WIN)

    //Mount filesystem in specified partition device
    //Note UDiskManager expects a valid (udev) path
    //e.g., for /dev/sda1 it would be /org/freedesktop/UDisks2/block_devices/sda1 or sda1
    QString udisk_name = dev_path;
    udisk_name = m_enumerator->m_udisk.findDeviceDBusPath(dev_path);
    if (udisk_name.isEmpty())
    {
        if (message_ref)
            *message_ref = "Invalid device path";
    }
    else
    {
        success = m_enumerator->m_udisk.mount(udisk_name, message_ref);
    }

#elif defined(Q_OS_WIN)

    //Does Windows even have a mount command?

#endif

    return success;
}

bool
StorageDiskSelection::Device::unmount(const QString &dev_path, QString *message_ref)
{
    bool success = false;

#if !defined(Q_OS_WIN)

    //Unmount filesystem in specified partition device
    //Note UDiskManager expects a valid (udev) path
    //e.g., for /dev/sda1 it would be /org/freedesktop/UDisks2/block_devices/sda1 or sda1
    //A device path is required, not a mountpoint!
    QString udisk_name;
    udisk_name = m_enumerator->m_udisk.findDeviceDBusPath(dev_path);
    if (udisk_name.isEmpty())
    {
        if (message_ref)
            *message_ref = "Invalid device path";
    }
    else
    {
        success = m_enumerator->m_udisk.umount(udisk_name, message_ref);
    }

#elif defined(Q_OS_WIN)

    //Does Windows even have an umount command?

#endif

    return success;
}

#if defined(Q_OS_WIN)

HANDLE
StorageDiskSelection::Device::driveHandle(bool temporary)
{
    HANDLE knob = m_win_handle;

    if (knob != INVALID_HANDLE_VALUE && !temporary)
    {
        return knob;
    }

    DWORD access_mode = 0;
    DWORD share_mode = access_mode ? 0 : FILE_SHARE_READ | FILE_SHARE_WRITE;
    knob = CreateFileA(m_int_addr.toLocal8Bit().constData(),
        access_mode,
        share_mode,
        NULL, //security attributes
        OPEN_EXISTING,
        0, //flags and attributes
        NULL
    );

    if (knob != INVALID_HANDLE_VALUE && !temporary)
    {
        m_win_handle = knob;
    }

    return knob;
}

bool
StorageDiskSelection::Device::checkWinDevDescProps(QString *description_ptr)
{
    auto a = m_enumerator;
    assert((int)a->m_interfaces.size() > m_index);
    auto dev_info_ptr = a->m_interfaces.at(m_index).m_devinfo_data;

    //Device description, whatever that means
    //If this returns false, further calls will likely fail as well. I think.
    unsigned char out_buffer[1024]; //the Internet says 1024 is good
    DWORD rsize = 0;
    DWORD prop_reg_data_type = 0;
    bool success = SetupDiGetDeviceRegistryPropertyA(
        a->m_h_dev_info_set, dev_info_ptr.get(),
        SPDRP_DEVICEDESC,
        &prop_reg_data_type, (BYTE*)out_buffer, sizeof(out_buffer),
        &rsize
    );
    if (success && description_ptr)
    {
        *description_ptr = (char*)out_buffer;
    }
    if (!a->SetupDiGetDevicePropertyW) success = false; //see ctor

    return success;
}

int
StorageDiskSelection::Device::deviceNumber()
{
    int dev_num = -1;

    //Get device number, required for linking filesystems to this disk device
    HANDLE dev_handle = driveHandle();

    //STORAGE_DEVICE_NUMBER
    STORAGE_DEVICE_NUMBER st_devnum;
    DWORD rsize = 0;
    DeviceIoControl(
        dev_handle,
        IOCTL_STORAGE_GET_DEVICE_NUMBER,
        0, 0,
        &st_devnum, sizeof(st_devnum),
        &rsize,
        NULL
    );
    if (rsize)
    {
        dev_num = st_devnum.DeviceNumber;
    }

    return dev_num;
}

#endif //Q_OS_WIN

//QSharedPointer<StorageDiskSelection::Enumerator>
//StorageDiskSelection::enumerator()
//{
//    //Create shared pointer to new Enumerator object
//    //Do not return raw pointer, ctor is private:
//    //Enumerator must not be used on the stack or without this shared pointer!
//    //That's because it creates Device objects and passes a copy of
//    //the shared pointer to each of them in order to stay alive
//    //until the last one is deleted.
//    //For example, the caller might acquire a new Enumerator,
//    //look for a specific device, keep a reference to that Device
//    //and discard its reference to the Enumerator.
//    //As long as that Device object exists, it might have to access
//    //the Enumerator, so the existence of the Enumerator is guaranteed
//    //by the shared pointer.
//    QSharedPointer<Enumerator> selection(new Enumerator);
//    selection->setSharedPointer(selection);
//
//    return selection;
//}

QList<StorageDiskSelection::Device>
StorageDiskSelection::blockDevices()
{
    QList<Device> devices;

    QSharedPointer<Access> a(new Access);
    assert(a);

    //Collect block devices (not partitions), e.g., hdds, ssds, usb sticks

#if !defined(Q_OS_WIN)

    foreach (QString dev, a->m_udisk.blockDevices())
    {
        QString dev_path = a->m_udisk.deviceFilePath(dev);
        //Skip devices which have parents (i.e., partitions)
        if (!a->m_udisk.underlyingBlockDevice(dev).isEmpty()) continue;
        //Skip blank drives
        if (a->m_udisk.isBlankDevice(dev)) continue;
        //Skip loop devices (e.g., /dev/loop0)
        if (a->m_udisk.isLoopDevice(dev)) continue;
        //Skip swap devices (e.g., /dev/zram0)
        if (a->m_udisk.isSwapDevice(dev)) continue;

        //Create Device object
        Device device(a, dev);
        devices << device;
    }

#elif defined(Q_OS_WIN)

    //Storage disk interface...
    a->m_h_dev_info_set = SetupDiGetClassDevs(&GUID_DEVINTERFACE_DISK, 0, 0, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
    if (a->m_h_dev_info_set == INVALID_HANDLE_VALUE)
    {
        //TODO Error
        return devices;
    }

    //Collect interface items
    for (int i = 0; true; i++)
    {
        std::shared_ptr<SP_DEVINFO_DATA> dev_info_ptr(new SP_DEVINFO_DATA);
        dev_info_ptr->cbSize = sizeof(SP_DEVINFO_DATA);
        bool success = SetupDiEnumDeviceInfo(a->m_h_dev_info_set, i, dev_info_ptr.get());
        if (GetLastError() == ERROR_NO_MORE_ITEMS)
            break;
        if (!success) continue;

        std::shared_ptr<SP_INTERFACE_DEVICE_DATA> if_data_ptr(new SP_INTERFACE_DEVICE_DATA);
        if_data_ptr->cbSize = sizeof(SP_INTERFACE_DEVICE_DATA);
        if (!SetupDiEnumDeviceInterfaces(a->m_h_dev_info_set, dev_info_ptr.get(), &(GUID_DEVINTERFACE_DISK), 0, if_data_ptr.get()))
        {
            if (GetLastError() != ERROR_NO_MORE_ITEMS)
                break;
        }
        Access::Interface if_container;
        if_container.m_devinfo_data = dev_info_ptr;
        if_container.m_interface_device_data = if_data_ptr;
        a->m_interfaces.push_back(if_container);
    }

    //interface -> device path, create and store Device objects
    for (int i = 0; i < (int)a->m_interfaces.size(); i++)
    {
        Access::Interface if_container = a->m_interfaces.at(i);
        std::shared_ptr<SP_INTERFACE_DEVICE_DATA> if_data_ptr = if_container.m_interface_device_data;

        //Get device path    
        DWORD required_size = 0;
        SetupDiGetDeviceInterfaceDetailA(a->m_h_dev_info_set, if_data_ptr.get(), 0, (DWORD)0, &required_size, 0);
        SP_INTERFACE_DEVICE_DETAIL_DATA_A *dev_path_data = 0; //deleted below
        dev_path_data = (SP_INTERFACE_DEVICE_DETAIL_DATA_A*)malloc(required_size);
        assert(dev_path_data);
        dev_path_data->cbSize = sizeof(SP_INTERFACE_DEVICE_DETAIL_DATA);
        if (!SetupDiGetDeviceInterfaceDetailA(a->m_h_dev_info_set, if_data_ptr.get(), dev_path_data, required_size, 0, 0))
        {
            continue;
        }
        QString dev_path = dev_path_data->DevicePath;
        free(dev_path_data); //SP_INTERFACE_DEVICE_DETAIL_DATA

        //Get device instance id
        //Example:
        //IDE\DISKQEMU_HARDDISK___________________________2.5+____\5&3A2A5854&0&1.1.0
        QString inst_id_str;
        SetupDiGetDeviceInstanceIdW(a->m_h_dev_info_set, if_container.m_devinfo_data.get(), 0, (DWORD)0, &required_size);
        std::vector<wchar_t> dev_id_chars(required_size);
        wchar_t *dev_id_data = (wchar_t*)dev_id_chars.data();
        if (SetupDiGetDeviceInstanceIdW(a->m_h_dev_info_set, if_container.m_devinfo_data.get(), dev_id_data, required_size, 0))
        {
            inst_id_str = Access::toString((const wchar_t*)dev_id_data);
        }

        //Create Device object
        Device device(a, dev_path);
        device.m_index = i;
        device.m_win_instance = inst_id_str;
        devices << device;
    }

#endif

    //Store copies of Device objects in cache
    foreach (Device device, devices)
        m_dev_cache << QSharedPointer<Device>(new Device(device));

    return devices;
}

QStringList
StorageDiskSelection::blockDevicePaths()
{
    QStringList dev_paths;

    QList<Device> devices;
    if (m_dev_cache.isEmpty())
        devices = blockDevices();
    else
        foreach (QSharedPointer<Device> device, m_dev_cache)
            devices << Device(*device);

    foreach (Device device, devices)
    {
        dev_paths << device.path();
    }

    return dev_paths;
}

StorageDiskSelection::Device
StorageDiskSelection::blockDevice(const QString &path)
{
    Device matching_device;

    QList<Device> devices;
    if (m_dev_cache.isEmpty())
        devices = blockDevices();
    else
        foreach (QSharedPointer<Device> device, m_dev_cache)
            devices << Device(*device);

    foreach (Device device, devices)
    {
        if (device.path() == path)
        {
            matching_device = device;
            break;
        }
    }

    return matching_device;
}

