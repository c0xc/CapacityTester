/****************************************************************************
**
** Copyright (C) 2024 Philip Seeger (p@c0xc.net)
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
****************************************************************************
** NOTICE: The copyright notice including the name of the author (Philip Seeger)
** must be preserved in all copies or substantial portions of the software. 
** If you use this module in another project, an acknowledgment in the product 
** documentation or a visible attribution in the software itself would be 
** appreciated.
****************************************************************************/

#ifndef USBSTORAGEDEVICE_HPP
#define USBSTORAGEDEVICE_HPP

#include <QtGlobal> //Q_OS_...

#if !defined(Q_OS_WIN)
#include <libudev.h>
#endif

//except NO_LIBUSB:
//libusb-devel = version 0 - legacy, obsolete
//libusb-1-devel = version 1 - not available on Fedora 34 under this name, but:
//$ pkg-config --cflags libusb-1.0
//-I/usr/include/libusb-1.0 
//using HAVE_LIBUSB1 etc. defined in configure (qmake) file
#if defined(HAVE_LIBUSB1)
//defines LIBUSB_API_VERSION
#include <libusb-1.0/libusb.h>
#elif defined(HAVE_LIBUSB0)
#include <usb.h>
typedef struct usb_device usb_dev;
#endif

#include <sys/stat.h>
#include <regex>

#include <QString>
#include <QMap>
#include <QVector>
#include <QVariant>
#include <QFileInfo>
#include <QRegExp>

#include "classlogger.hpp"

class UsbStorageDevice
{
public:

    enum class Api
    {
        NONE = 0,           // 0b00000000
        LIBUDEV = 1 << 0,   // 0b00000001
        LIBUSB1 = 1 << 1,   // 0b00000010
        LIBUSB0 = 1 << 2    // 0b00000100
    };

    enum class UsbSpeed : int
    {
        Unknown = 0,
        Low = 1, //1.5
        Full = 12,
        High = 480,
        Super = 5000,
        SuperPlus = 10000,
        MegaPlus = 20000,
        MegaUltra = 40000,
        MegaUltraPlus = 80000,
        MegaUltraPlusPlus = 160000,
        MegaUltraPlusPlusPlus = 320000,
    };

    explicit
    UsbStorageDevice(const QString &path);

    ~UsbStorageDevice();

    QString
    usbSpeedName(UsbSpeed speed);

    UsbSpeed
    toUsbSpeed(int speed);

    /**
     * @brief Returns available api's or 0 (false) if none are available.
     * 
     * @return int 
     */
    Api
    availableApi() const;

    bool
    isValid() const;

    QVariantMap
    getUdevParameters();

    QString
    getUdevDevPath();

    uint8_t
    getBusNumber();

    QVector<uint8_t>
    getBusPortPath();

    void
    getBusPortNumbers(int *bus_ptr, QVector<uint8_t> *ports_ptr, QStringList *ports_str_ptr, QString *port_str_ptr);

    int
    getUdevSpeed();

    /**
     * @brief Attempts to determine the effective device speed.
     * 
     * If rc is set to 0, the speed was successfully determined (by libusb1) and
     * the return value is the speed in Mbps.
     * If rc is set to -1, the speed could not be determined.
     * 
     * -1: Speed could not be determined at all.
     * 0: Speed was determined reliably using libusb1.
     * 1: Speed was guessed based on the USB version using libusb0.
     * 2: libusb1/0 is available but could not be used due
     * to a lack of permissions, so udev was used instead.
     * 3: libusb1/0 is not available at all and udev had to be used.
     * 
     * @param rc will be set to indicate the success of the speed determination.
     * @return int
     */
    UsbSpeed
    getSpeed(int *rc_ptr = 0, int *ver_major_ptr = 0, int *ver_minor_ptr = 0);

    void
    getUdevVendorAndModelId(QString &vendor_id_ref, QString &model_id_ref);

    void
    getUsbVendorAndModelId(QString &vendor_id_ref, QString &model_id_ref);

    QString
    getUdevModelLabel();

    QString
    getUdevSerialNumber();

    QString
    getUsbSerialNumber();

    QString
    getSerialNumber();

#if !defined(Q_OS_WIN)
    udev_device*
    getUdevDevice();
#endif

#if defined(HAVE_LIBUSB1)
//libusb1
    libusb_device*
    getUsb1Device();
    QString
    getUsbSerialNumber(libusb_device *dev);
#elif defined(HAVE_LIBUSB0)
//libusb0
    usb_dev*
    getUsb0Device();
    QString
    getUsbSerialNumber(usb_dev *dev);
#endif

private:

    const QMap<UsbSpeed, QString> usb_speed_names
    {
        {UsbSpeed::Unknown, "Unknown"},
        {UsbSpeed::Low, "Low Speed"},
        {UsbSpeed::Full, "Full Speed"},
        {UsbSpeed::High, "High Speed"},
        {UsbSpeed::Super, "Super Speed"},
        {UsbSpeed::SuperPlus, "Super Plus Speed"},
    };

    QString
    m_path;

    QString
    m_int_name;

    bool
    m_init_ok;

    int
    m_err_flag;

    std::pair<int, int>
    m_major_minor = {0, 0};    

#if !defined(Q_OS_WIN)
    udev_device
    *m_udev_device = 0;
    udev
    *m_udev_context = 0;
#endif

#if defined(HAVE_LIBUSB1)
    libusb_context
    *m_usb1_ctx = 0;
    libusb_device
    *m_usb1_device = 0;
#elif defined(HAVE_LIBUSB0)
    usb_dev
    *m_usb0_device = 0;
#endif

};

#endif