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

#ifndef STORAGEDISKSELECTION_HPP
#define STORAGEDISKSELECTION_HPP

/**
 * Windows is ugly, a sight for sore eyes
 * A jumbled mess of clicks and tries
 * Behind every door, waits a bluescreen
 * An error message, oh so obscene
 * 
 * No rhyme or reason to its malice
 * A frustration that borders on malice
 * It drags us down, makes us slow
 * No matter how high we aim to go
 * 
 * But in this darkness, there's a light
 * A glimmer of hope, shining bright
 * A promise of something new and clean
 * A world where bluescreens are just a dream
 * 
 * So let us look to the future with hope
 * And leave behind this clumsy old trope
 * With windows gone, we'll finally see
 * A world full of beauty, full of glee.
 *
 * -- Poetry Generator (boredhumans.com)
 */

#include <QtGlobal> //Q_OS_...

#if !defined(Q_OS_WIN) //system-dependent headers
//good systems

#include <sys/stat.h> //stat()
#include <fcntl.h> //O_RDONLY
#include <unistd.h> //close()
#include <sys/ioctl.h> //ioctl()
#include <linux/fs.h> //BLKGETSIZE64

#include "udiskmanager.hpp"

#elif defined(Q_OS_WIN)
//Windows

#include <windows.h>

//required: -std=c++11
// https://learn.microsoft.com/en-us/windows/win32/api/setupapi/nf-setupapi-setupdigetclassdevsw?redirectedfrom=MSDN
#ifdef DEFINE_SDS_INC
#define INITGUID
#include <initguid.h>
#include <ntddstor.h>
// GUID_DEVINTERFACE_DISK = {53F56307-B6BF-11D0-94F2-00A0C91EFB8B}
DEFINE_GUID(GUID_DEVINTERFACE_DISK, 0x53F56307, 0xB6BF, 0x11D0, 0x94, 0xf2, 0x0, 0xa0, 0xc9, 0x1e, 0xfb, 0x8b);
#else
//extern...
#endif
#include <devguid.h> //GUID_DEVCLASS_CDROM

#include <setupapi.h> //requires -lsetupapi

#include <cfgmgr32.h> //MAX_DEVICE_ID_LEN, CM_Get_Parent, CM_Get_Device_ID
#include <Devpropdef.h> //DEVPROPKEY
//DEFINE_DEVPROPKEY <Devpropdef.h>
DEFINE_DEVPROPKEY(DEVPKEY_Device_BusReportedDeviceDesc,  0x540b947e, 0x8b40, 0x45bc, 0xa8, 0xa2, 0x6a, 0x0b, 0x89, 0x4c, 0xbd, 0xa2, 4);     // DEVPROP_TYPE_STRING
DEFINE_DEVPROPKEY(DEVPKEY_Device_ContainerId,            0x8c7ed206, 0x3f8a, 0x4827, 0xb3, 0xab, 0xae, 0x9e, 0x1f, 0xae, 0xfc, 0x6c, 2);     // DEVPROP_TYPE_GUID
DEFINE_DEVPROPKEY(DEVPKEY_Device_FriendlyName,           0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 14);    // DEVPROP_TYPE_STRING
DEFINE_DEVPROPKEY(DEVPKEY_DeviceDisplay_Category,        0x78c34fc8, 0x104a, 0x4aca, 0x9e, 0xa4, 0x52, 0x4d, 0x52, 0x99, 0x6e, 0x57, 0x5a);  // DEVPROP_TYPE_STRING_LIST
DEFINE_DEVPROPKEY(DEVPKEY_Device_LocationInfo,           0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 15);    // DEVPROP_TYPE_STRING
DEFINE_DEVPROPKEY(DEVPKEY_Device_Manufacturer,           0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 13);    // DEVPROP_TYPE_STRING
DEFINE_DEVPROPKEY(DEVPKEY_Device_SecuritySDS,            0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 26);    // DEVPROP_TYPE_SECURITY_DESCRIPTOR_STRING

#include <io.h> //_open_osfhandle, _open
#include <fcntl.h> //_O_RDWR

#endif //end of system-dependent headers

#include <cassert>
#include <string>
#include <memory>

#include <QStringList>
#include <QSharedPointer>
#include <QRegExp>

//#include "classlogger.hpp"

/*! \class StorageDiskSelection
 *
 * \brief The StorageDiskSelection class provides an interface to
 * [USB] storage devices.
 *
 * This is a generic wrapper to simplify access to (the list of) disks.
 * It scans the system for [USB] block devices, i.e., disks.
 * It provides some of the most important, commonly needed attributes
 * of each storage device - the internal path to the block device,
 * the title or name of the drive, display name...
 *
 */

class StorageDiskSelection
{
public:

    struct Error // TODO
    {
        int code = 0;
        QString msg;
    };

    class Device;

    /*
     * The Access/Enumerator object contains the logic to scan
     * the system for block devices.
     * The Enumerator instance that was used to list the devices on the system
     * will remain in the background until the last Device object
     * goes out of scope.
     */
    struct Access
    {

        Access();

        ~Access();

#if !defined(Q_OS_WIN)

        UDiskManager
        m_udisk;

#elif defined(Q_OS_WIN)

        static QString
        toString(const wchar_t *str_wchar);

        static QString
        sometimesWideCharToString(LPCTSTR str_tchar);

        static QString
        formatWinError(DWORD dw = 0);

        std::list<HANDLE>
        m_d_handles;

        struct Interface
        {
            std::shared_ptr<SP_DEVINFO_DATA>
            m_devinfo_data;

            std::shared_ptr<SP_INTERFACE_DEVICE_DATA>
            m_interface_device_data;
        };

        HDEVINFO
        m_h_dev_info_set;

        std::vector<Interface>
        m_interfaces;

        typedef bool (*FN_SetupDiGetDevicePropertyW)
        (
            HDEVINFO DeviceInfoSet,
            PSP_DEVINFO_DATA DeviceInfoData,
            const DEVPROPKEY *PropertyKey, //<Devpropdef.h>
            DEVPROPTYPE *PropertyType,
            PBYTE PropertyBuffer,
            DWORD PropertyBufferSize,
            PDWORD RequiredSize,
            DWORD Flags
        );
        FN_SetupDiGetDevicePropertyW SetupDiGetDevicePropertyW;

#endif

    };

    /*
     * An instance of the Device class stores the internal device path
     * and a shared reference to the Enumerator
     * to access properties.
     * When the last instance goes out of scope, the shared pointer
     * will delete the Enumerator, freeing its memory.
     */
    class Device
    {
    private:

        Device();

        Device(QSharedPointer<Access> selection, QString addr);

    public:

        Device(const Device &other);

        Device& operator=(const Device &other) = default;

        ~Device();

        bool
        isNull();

        bool
        isValid();

        //actual path to device file, e.g., /dev/sda
        //TODO simplified form of path, e.g., sda
        QString
        path();

        QString
        identifier();

        bool
        isUsbDevice(bool *error_ref = 0);

        qint64
        capacity(bool *error_ref = 0);

        QString
        displayModelName();

        QString
        vendorName();

        QString
        serialNumber();

        int
        blockSize();

        int
        open(int flags = 0);

        bool
        close();

        bool
        seek(qint64 offset, qint64 *pos_ptr = 0);

    private:

        friend class StorageDiskSelection; //for Device() ctor

        QSharedPointer<Access>
        m_enumerator;

        QString
        m_int_addr;

        int
        m_fd;

        //copy ctor

#if defined(Q_OS_WIN)

        int
        m_index;

        HANDLE
        m_win_handle;

        HANDLE
        driveHandle();

        bool
        checkWinDevDescProps(QString *description_ptr = 0);

#endif

    };

    QList<Device>
    blockDevices();

    //convenience function
    QStringList
    blockDevicePaths();

    Device
    blockDevice(const QString &path);

private:

    QList<QSharedPointer<Device>>
    m_dev_cache;

};

#endif
