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
****************************************************************************/

#include "usbstoragedevice.hpp"

UsbStorageDevice::UsbStorageDevice(const QString &path)
                : m_path(path),
                  m_init_ok(false),
                  m_err_flag(0)
{
    //Check if the path is a block device
    struct stat dev_stat;
    if (stat(m_path.toUtf8().constData(), &dev_stat) == 0)
    {
        if (S_ISBLK(dev_stat.st_mode))
        {
            //path is a block device
            //like /dev/sda
            QFileInfo fi(m_path);
            m_int_name = fi.fileName();
            m_init_ok = true;
        }
        else
        {
            //not a block device
            m_err_flag = 1;
            return;
        }
    }
    else
    {
        //Error getting device status
        m_err_flag = -1;
        return;
    }

    //Get major and minor id
    //also available in the udev map: "MAJOR" and "MINOR"
    m_major_minor.first = major(dev_stat.st_rdev);
    m_major_minor.second = minor(dev_stat.st_rdev);

    Debug(QS("initialized UsbStorageDevice for device %s (major/minor ids: %d/%d)", CSTR(m_path), m_major_minor.first, m_major_minor.second));
#if defined(HAVE_LIBUSB1)
    Debug(QS("with libusb1 api"));
#elif defined(HAVE_LIBUSB0)
    Debug(QS("with libusb0 api"));
#endif
}

UsbStorageDevice::~UsbStorageDevice()
{
#if defined(HAVE_LIBUDEV)
    //Free udev device
    if (m_udev_device)
    {
        udev_device_unref(m_udev_device);
    }
#endif

#if defined(LIBUSB_API_VERSION)
    //Free libusb1 device
    if (m_usb1_device)
    {
        libusb_unref_device(m_usb1_device);
    }
    if (m_usb1_ctx)
    {
        libusb_exit(m_usb1_ctx);
    }
#endif
#if defined(HAVE_LIBUSB0)
    //libusb0 - device not opened, usb_dev_handle not acquired, so no need to close
#endif
}

QString
UsbStorageDevice::usbSpeedName(UsbSpeed speed)
{
    return usb_speed_names[speed];
}

UsbStorageDevice::UsbSpeed
UsbStorageDevice::toUsbSpeed(int speed)
{
    UsbSpeed usb_speed = UsbSpeed::Unknown;
    //Check if the speed is in the map, avoid casting -1 or other invalid values
    foreach (const UsbStorageDevice::UsbSpeed key, usb_speed_names.keys())
    {
        if (static_cast<int>(key) == speed)
        {
            usb_speed = key;
            break;
        }
    }
    return usb_speed;
}

UsbStorageDevice::Api
UsbStorageDevice::availableApi() const
{
    int api_availability = static_cast<int>(Api::NONE);

#ifdef HAVE_LIBUDEV
    api_availability |= static_cast<int>(Api::LIBUDEV);
#endif
#ifdef HAVE_LIBUSB1
    api_availability |= static_cast<int>(Api::LIBUSB1);
#endif
#ifdef HAVE_LIBUSB0
    api_availability |= static_cast<int>(Api::LIBUSB0);
#endif

    return static_cast<Api>(api_availability);
}

bool
UsbStorageDevice::isValid() const
{
    return m_init_ok;
}

QVariantMap
UsbStorageDevice::getUdevParameters()
{
    QVariantMap info;

#ifdef HAVE_LIBUDEV

    //Get udev device
    struct udev_device *udev_device = getUdevDevice();
    if (!udev_device)
    {
        return info;
    }

    //Get the device properties
    struct udev_list_entry *properties, *entry;
    properties = udev_device_get_properties_list_entry(udev_device);
    udev_list_entry_foreach(entry, properties)
    {
        const char *name = udev_list_entry_get_name(entry);
        const char *value = udev_list_entry_get_value(entry);
        info.insert(QString::fromUtf8(name), QString::fromUtf8(value));
    }

#endif

    return info;
}

QString
UsbStorageDevice::getUdevDevPath()
{
    QString devpath;

#ifdef HAVE_LIBUDEV

    //Get udev device
    struct udev_device *udev_device = getUdevDevice();
    if (!udev_device)
    {
        return devpath;
    }

    //Get the parent udev device (the USB device)
    udev_device = udev_device_get_parent_with_subsystem_devtype(udev_device, "usb", "usb_device");
    if (!udev_device)
    {
        Debug(QS("Unable to find parent usb device."));
        return devpath;
    }

    //Get the DEVPATH property
    const char *devpath_bytes = udev_device_get_devpath(udev_device);
    if (devpath_bytes)
    {
        devpath = QString::fromUtf8(devpath_bytes);
    }

#endif

    return devpath;
}

uint8_t
UsbStorageDevice::getBusNumber()
{
    uint8_t bus_number = -1;

#ifdef HAVE_LIBUDEV

    //Get udev device
    struct udev_device *udev_device = getUdevDevice();
    if (!udev_device)
    {
        Debug(QS("Can't get udev device"));
        return bus_number;
    }

    //Get the parent udev device (the USB device)
    udev_device = udev_device_get_parent_with_subsystem_devtype(udev_device, "usb", "usb_device");
    if (!udev_device)
    {
        Debug(QS("Unable to find parent usb device."));
        return bus_number;
    }

    //Get the DEVPATH property
    const char *devpath = udev_device_get_devpath(udev_device);
    if (!devpath)
    {
        Debug(QS("Unable to get DEVPATH property."));
        return bus_number;
    }

    //Extract the bus number and device number from the DEVPATH property
    QRegExp rx("usb(\\d+)/(\\d+)-(\\d+)");
    int pos = rx.indexIn(devpath);
    if (pos > -1)
    {
        QString bus_num_str = rx.cap(1);
        bus_number = bus_num_str.toInt();
        Debug(QS("determined usb bus number to be %d based on udev devpath <%s>", bus_number, devpath));
    }
    else
    {
        Debug(QS("failed to determine usb bus number; devpath: %s", CSTR(devpath)));
    }

#endif

    return bus_number;
}

QVector<uint8_t>
UsbStorageDevice::getBusPortPath()
{
    QVector<uint8_t> port_path; //port numbers starting from the root bus
    QString devpath = getUdevDevPath(); //provided by udev
    //ex: /devices/pci0000:00/0000:00:14.0/usb1/1-3
    //ex: /devices/pci0000:00/0000:00:14.0/usb1/1-1/1-1.2
    Debug(QS("devpath: %s", CSTR(devpath)));
    if (devpath.isEmpty())
    {
        Debug(QS("No devpath found."));
        return {};
    }

    //Find where the root usb path begins    
    int start = devpath.indexOf("/usb");
    if (start == -1)
    {
        return {};
    }

    //Extract the root bus number
    int root_bus_number = -1;
    QStringList path_segments = devpath.split('/');
    int root_bus_index = -1;
    QRegExp rx_root("^usb([0-9]+)$"); //match "usbN" and capture "N"
    for (int i = 0; i < path_segments.size(); i++)
    {
        if (rx_root.indexIn(path_segments[i]) != -1)
        {
            root_bus_index = i;
            root_bus_number = rx_root.cap(1).toInt();
            port_path.push_back(static_cast<uint8_t>(root_bus_number));
            break; //found the root bus
        }
    }

    //If no root bus was found, return; devpath is invalid or not a usb device
    if (root_bus_index == -1)
    {
        Debug(QS("No root bus found in devpath: %s", CSTR(devpath)));
        return {};
    }

    //Iterate over all segments starting after the root bus
    for (int i = root_bus_index + 1; i < path_segments.size(); i++)
    {
        QString segment = path_segments[i];
        //Check if it starts with a digit, otherwise it would not be part of the port path anymore
        if (!segment[0].isDigit()) //should be the bus number
        {
            break;
        }
        //Extract the port number from the end of the segment
        //This works for segments like "1-1" and "1-1.2", where the port number is at the end
        //and for each port in the chain, there is a segment with the port number.
        //For example, if the path is "1-1/1-1.2", the port numbers are 1 and 2.
        QRegExp rx("[.-]([0-9]+)$"); //match the integer at the end of the segment
        if (segment.contains(':'))
        {
            //Skip segments with colons, they are not part of the port path
            //This would be for the usb interface, which is not part of the port path
            Debug(QS("Skipping interface segment with colon: %s", CSTR(segment)));
            break;
        }
        if (rx.indexIn(segment) != -1)
        {
            bool ok = false;
            uint8_t port_number = static_cast<uint8_t>(rx.cap(1).toInt(&ok));
            if (ok)
            {
                port_path.push_back(port_number);
            }
            else
            {
                Debug(QS("Failed to convert port number to integer: %s", CSTR(rx.cap(1))));
                port_path.clear();
                break;
            }
        }
        else
        {
            Debug(QS("Failed to find port number in segment: %s", CSTR(segment)));
            port_path.clear();
            break;
        }
    }

    return port_path;
}

void
UsbStorageDevice::getBusPortNumbers(int *bus_ptr, QVector<uint8_t> *ports_ptr, QStringList *ports_str_ptr, QString *port_str_ptr)
{
    int bus_number = -1;
    QVector<uint8_t> port_path = getBusPortPath();
    if (port_path.isEmpty())
    {
        return;
    }
    bus_number = port_path[0];
    QVector<uint8_t> port_numbers = port_path.mid(1); //skip the root bus number
    if (bus_ptr)
    {
        *bus_ptr = bus_number;
    }
    if (ports_ptr)
    {
        *ports_ptr = port_numbers;
    }
    if (ports_str_ptr)
    {
        QStringList ports_str;
        for (int i = 0; i < port_numbers.size(); i++)
        {
            ports_str.append(QString::number(port_numbers[i]));
        }
        *ports_str_ptr = ports_str;
    }
    if (port_str_ptr)
    {
        QString port_numbers_str;
        if (!port_numbers.isEmpty())
        {
            port_numbers_str = QString::number(port_numbers[0]);
            for (int i = 1; i < port_numbers.size(); i++)
            {
                port_numbers_str += "-" + QString::number(port_numbers[i]);
            }
        }
        *port_str_ptr = port_numbers_str;
    }
}

int
UsbStorageDevice::getUdevSpeed()
{
    int speed = -1;

#ifdef HAVE_LIBUDEV

    //Get udev device
    struct udev_device *udev_device = getUdevDevice();
    if (!udev_device) return speed;

    //Get the parent udev device (the USB device)
    udev_device = udev_device_get_parent_with_subsystem_devtype(udev_device, "usb", "usb_device");
    if (!udev_device)
    {
        Debug(QS("Unable to find parent usb device."));
        return speed;
    }

    //Get the speed attribute
    const char *speed_bytes;
    speed_bytes = udev_device_get_sysattr_value(udev_device, "speed");
    if (!speed_bytes)
    {
        Debug(QS("Unable to get device speed."));
        return speed;
    }
    speed = std::stoi(speed_bytes);

#endif

    return speed;
}

UsbStorageDevice::UsbSpeed
UsbStorageDevice::getSpeed(int *rc_ptr, int *ver_major_ptr, int *ver_minor_ptr)
{
    //Determines the negotiated speed of the USB device
    //and returns it, setting rc to 0 if (reliably) reported by libusb1.
    //If only libusb0 is available, the speed is guessed based on the USB version

    //rc codes:
    //-1: Speed could not be determined at all
    //0: Speed was determined reliably using libusb1
    //1: Speed was guessed based on the USB version using libusb0
    //2: libusb1/0 is available but could not be used due to a lack of permissions, so udev was used instead
    //3: libusb1/0 is not available at all and udev had to be used

    UsbSpeed speed = UsbSpeed::Unknown;
    int rc = -1;
    int v_major = 0, v_minor = 0; //USB version used for speed guessing
    bool libusb_perm_error = false; //used if libusb1/0 unable to open device due to lack of permissions

    //We use different code paths for libusb0 and libusb1,
    //depending on the availability of the respective API.
    //If both are available, libusb1 is preferred.
    //I wrote this code before I realized that I could also use libudev.
    //-- Philip (2024-04-21)

    if (rc_ptr) *rc_ptr = -1; //rc set to -1 but will be set below if speed is determined

#if LIBUSB_API_VERSION
    //libusb1

    libusb_device *dev = getUsb1Device();
    if (dev)
    {
        if (rc_ptr) *rc_ptr = 0; //indicate reliable result    
        switch (libusb_get_device_speed(dev))
        {
            case LIBUSB_SPEED_LOW:
            speed = UsbSpeed::Low;
            break;

            case LIBUSB_SPEED_FULL:
            speed = UsbSpeed::Full;
            break;

            case LIBUSB_SPEED_HIGH:
            speed = UsbSpeed::High;
            break;

            case LIBUSB_SPEED_SUPER:
            speed = UsbSpeed::Super;
            break;

            case LIBUSB_SPEED_SUPER_PLUS:
            speed = UsbSpeed::SuperPlus;
            break;

            default:
            speed = UsbSpeed::Unknown;
            rc = -1;
        }
        Debug(QS("USB speed (libusb1): %d", (int)speed));
        //USB version
        uint16_t version;
        libusb_device_descriptor desc;
        int r = libusb_get_device_descriptor(dev, &desc);
        if (r == 0)
        {
            uint16_t version = desc.bcdUSB;
            v_major = version >> 8;
            v_minor = version & 0xff;
            Debug(QS("USB version: %d.%d", v_major, v_minor));
        }
    }
    else
    {
        //libusb1 could not open the device due to a lack of permissions
        libusb_perm_error = true;
        //rc already set to -1 but udev will be tried below
    }

#elif defined(HAVE_LIBUSB0)
    //libusb0

    rc = 1; //indicate uncertainty, educated guess
    usb_dev *dev = getUsb0Device();
    if (dev)
    {
        if (rc_ptr) *rc_ptr = 1; //indicate educated guess
        switch (dev->descriptor.bcdUSB)
        {
            case 0x0100:
            //USB 1.0 (Low Speed)
            speed = UsbSpeed::Low;
            break;

            case 0x0110:
            //USB 1.1 (Full Speed)
            speed = UsbSpeed::Full;
            break;

            case 0x0200:
            //USB 2.0 (High Speed)
            speed = UsbSpeed::High;
            break;

            case 0x0300:
            //USB 3.0 (SuperSpeed)
            speed = UsbSpeed::Super;
            break;

            default:
            //unknown
            rc = -1;
        }
        v_major = dev->descriptor.bcdUSB >> 8;
        v_minor = dev->descriptor.bcdUSB & 0xff;
        Debug(QS("USB version: %d.%d; speed (libusb0): %d", v_major, v_minor, (int)speed));
    }
    else
    {
        //libusb0 could not open the device due to a lack of permissions
        libusb_perm_error = true;
        //rc already set to -1 but udev will be tried below
    }

#endif

    //If libusb1/0 is not available, try to get the speed from udev;
    //and also use udev if libusb1/0 could not be used due to a lack of permissions.

#if defined(HAVE_LIBUDEV)
//udev

    //As the saying goes: Alle Wege führen nach 127.0.0.1.
    if (speed == UsbSpeed::Unknown)
    {
        //If the speed is still unknown, try to get it from udev
        //This is a last resort, as libusb requires sudo permissions
        //to open the device and get the speed, which is not always available,
        //and udev does not, but we don't consider the reported speed to be reliable.

        speed = toUsbSpeed(getUdevSpeed());
        if (speed != UsbSpeed::Unknown)
        {
            //Speed was determined by udev
            if (rc_ptr) *rc_ptr = (libusb_perm_error) ? 2 : 3;
        }
        else
        {
            //Speed could not be determined at all
            //rc == -1; not: if (rc_ptr) *rc_ptr = 4;
        }
        Debug(QS("USB speed (udev): %d", (int)speed));
    }

#endif

    if (ver_major_ptr) *ver_major_ptr = v_major;
    if (ver_minor_ptr) *ver_minor_ptr = v_minor;

    return speed;
}

void
UsbStorageDevice::getUdevVendorAndModelId(QString &vendor_id_ref, QString &model_id_ref)
{
#ifdef HAVE_LIBUDEV

    //Get udev device
    struct udev_device *udev_device = getUdevDevice();
    if (!udev_device)
    {
        Debug(QS("Can't get udev device"));
        return;
    }

    //Get the parent udev device (the USB device)
    udev_device = udev_device_get_parent_with_subsystem_devtype(udev_device, "usb", "usb_device");
    if (!udev_device)
    {
        Debug(QS("Unable to find parent usb device."));
        return;
    }

    //Get the vendor and model id
    const char *vendor_id = udev_device_get_sysattr_value(udev_device, "idVendor");
    const char *model_id = udev_device_get_sysattr_value(udev_device, "idProduct");
    if (vendor_id && model_id)
    {
        vendor_id_ref = QString::fromUtf8(vendor_id);
        model_id_ref = QString::fromUtf8(model_id);
    }

#endif
}

void
UsbStorageDevice::getUsbVendorAndModelId(QString &vendor_id_ref, QString &model_id_ref)
{
    //We use different code paths for libusb0 and libusb1,
    //depending on the availability of the respective API.
    //If both are available, libusb1 is preferred.
    //I wrote this code before I realized that I could also use libudev.
    //-- Philip (2024-04-21)

#if defined(LIBUSB_API_VERSION)
    //libusb1

    libusb_device *dev = getUsb1Device();
    if (dev)
    {
        libusb_device_descriptor desc;
        libusb_get_device_descriptor(dev, &desc);
        QString vendor_id = QString::number(desc.idVendor, 16);
        QString model_id = QString::number(desc.idProduct, 16);
        //zero-pad to 4 characters
        while (model_id.length() < 4)
            vendor_id = "0" + vendor_id;
        vendor_id_ref = vendor_id;
        model_id_ref = model_id;
    }

#elif defined(HAVE_LIBUSB0)
    //libusb0

    usb_dev *dev = getUsb0Device();
    if (dev)
    {
        QString vendor_id = QString::number(dev->descriptor.idVendor, 16);
        QString model_id = QString::number(dev->descriptor.idProduct, 16);
        //zero-pad to 4 characters
        //while (model_id.length() < 4)
        //{
        //    vendor_id = "0" + vendor_id;
        //}
        vendor_id_ref = vendor_id;
        model_id_ref = model_id;
    }

#endif
}

QString
UsbStorageDevice::getUdevModelLabel()
{
    QString model_label;

    model_label = getUdevParameters().value("ID_MODEL").toString();
    //const char *model_label_bytes = udev_device_get_sysattr_value(udev_device, "product");

    return model_label;
}

QString
UsbStorageDevice::getUdevSerialNumber()
{
    QString serial_number;

    serial_number = getUdevParameters().value("ID_SERIAL_SHORT").toString();
    //const char *serial_number_bytes = udev_device_get_sysattr_value(udev_device, "serial");

    return serial_number;
}

//I thought about fetching the serial number once when the device is found,
//and then caching it, but I decided against it, keeping the option
//to fetch it again later, in case it changes.
//The serial number is not always available, so we need to be able to handle that.
//-- Philip (2024-04-21)
QString
UsbStorageDevice::getUsbSerialNumber()
{
    QString serial_number;

    //Get the serial number using libusb
    //This requires opening the device, which requires sudo.

#if defined(HAVE_LIBUSB1)
    //libusb1

    libusb_device *dev = getUsb1Device();
    serial_number = getUsbSerialNumber(dev);

#elif defined(HAVE_LIBUSB0)
    //libusb0

    usb_dev *dev = getUsb0Device();
    serial_number = getUsbSerialNumber(dev);
    //usb_dev_handle *handle = dev ? usb_open(dev) : 0;
    //if (handle)
    //{
    //    char serial_number_bytes[256];
    //    int r = usb_get_string_simple(handle, dev->descriptor.iSerialNumber, serial_number_bytes, sizeof(serial_number_bytes));
    //    if (r > 0)
    //    {
    //        serial_number = QString::fromUtf8(serial_number_bytes);
    //        used_api = static_cast<int>(Api::LIBUSB0); //set flag to indicate that libusb0 was used
    //    }
    //    else
    //    {
    //        Debug(QS("failed to get serial number: %s", usb_strerror()));
    //    }
    //    usb_close(handle);
    //}
    //else
    //{
    //    Debug(QS("failed to open device: %s", usb_strerror()));
    //}

#endif

    return serial_number;
}

QString
UsbStorageDevice::getSerialNumber()
{
    QString serial_number;
    int used_api = 0;

    //libusb would require opening the device to get the serial number
    //and for that, sudo is required.
    //udev is more convenient for this purpose.
    //So we try to use libusb1 first, and if that fails, we fall back to udev.

#if defined(HAVE_LIBUSB1) || defined(HAVE_LIBUSB0)
    //libusb1 or libusb0

    serial_number = getUsbSerialNumber();
    bool ok = !serial_number.isEmpty();
    if (ok)
        used_api = static_cast<int>(Api::LIBUSB1); //set flag to indicate that libusb1 was used
    //note that we don't distinguish between libusb1 and libusb0 here

#endif

#if defined(HAVE_LIBUDEV)
    //udev

    if (serial_number.isEmpty())
    {
        serial_number = getUdevSerialNumber();
        bool ok = !serial_number.isEmpty();
        if (ok)
            used_api = static_cast<int>(Api::LIBUDEV); //set flag to indicate that udev was used
    }

#endif

    //Debug log which api was used
    if (used_api == static_cast<int>(Api::LIBUSB1))
    {
        Debug(QS("got serial number <%s> via libusb", CSTR(serial_number))); //note that we don't distinguish between libusb1 and libusb0 here
    }
    else if (used_api == static_cast<int>(Api::LIBUDEV))
    {
        Debug(QS("got serial number <%s> via udev", CSTR(serial_number)));
    }
    else
    {
        Debug(QS("failed to get serial number"));
    }
    //if (used_api_ptr)
    //    *used_api_ptr = static_cast<Api>(used_api);

    return serial_number;
}

#if !defined(Q_OS_WIN)
udev_device*
UsbStorageDevice::getUdevDevice()
{
    if (m_udev_device)
    {
        return m_udev_device;
    }

    struct udev *udev;
    struct udev_device *dev;

    udev = udev_new();
    if (!udev)
    {
        //set error flag
        return 0;
    }

    dev = udev_device_new_from_subsystem_sysname(udev, "block", m_int_name.toStdString().c_str());
    if (!dev)
    {
        //set error flag
        udev_unref(udev);
        return 0;
    }
    m_udev_device = dev;
    //udev_device_unref() is called in the destructor

    udev_unref(udev);
    return dev;
}
#endif

#ifdef HAVE_LIBUSB1
//libusb1

libusb_device*
UsbStorageDevice::getUsb1Device()
{
    if (m_usb1_device)
    {
        return m_usb1_device;
    }

    int bus_number = -1;
    QVector<uint8_t> port_numbers;
    QString port_numbers_str;
    getBusPortNumbers(&bus_number, &port_numbers, 0, &port_numbers_str);

    libusb_device **devs;
    libusb_context *ctx = 0;

    //Initialize libusb1 context (on demand, first use)
    int r = 0;
    if (!m_usb1_ctx)
    {
#if LIBUSB_API_VERSION >= 0x0100010A
        # Since version 1.0.27
        r = libusb_init_context(&ctx); //libusb_init is deprecated
#else
        r = libusb_init(&ctx);
#endif
        if (r < 0)
        {
            Debug(QS("libusb1 init error %d", r));
            return 0;
        }
        m_usb1_ctx = ctx;
    }

    //Device list
    ssize_t cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0)
    {
        Debug(QS("libusb1 failed to get device list (%ld)", cnt));
        return 0;
    }

    //Find the device in the list
    Debug(QS("Looking for device on bus %d %s", bus_number, CSTR(port_numbers_str)));
    ssize_t i = 0;
    for (i = 0; i < cnt; i++)
    {
        libusb_device *dev = devs[i];
        int cur_bus_num = libusb_get_bus_number(dev);
        int cur_dev_num = libusb_get_device_address(dev);
        uint8_t cur_ports[8];
        int port_count = libusb_get_port_numbers(dev, cur_ports, sizeof(cur_ports));

        //Fetch vendor and model id for debugging and to match the device
        libusb_device_descriptor desc = {};
        libusb_get_device_descriptor(dev, &desc);
        QString vendor_id = QString::number(desc.idVendor, 16);
        QString model_id = QString::number(desc.idProduct, 16);

        //Compare bus, port numbers (path)
        //Wir identifizieren das Gerät anhand der Bus- und Portnummern,
        //denn am Ende einer solchen Kette befindet sich genau ein Gerät.
        if (cur_bus_num != bus_number) continue; //skip device, bus number does not match
        if (port_count != port_numbers.count()) continue; //skip, port count doesn't match
        bool match = (port_count == port_numbers.count());
        for (int j = 0; match && j < port_count; j++)
        {
            if (cur_ports[j] != port_numbers[j])
                match = false;
        }
        if (!match) continue; //skip device, port numbers don't match
        //Matched bus and port numbers

        //Found the device (udev-reported device_number is not the current device number cur_dev_num)
        //There should be only one device with a given bus/port path
        if (m_usb1_device)
        {
            //Found duplicate device with the same bus/port path
            //logic error - found duplicate device, reset and abort    
            Debug(QS("duplicate device on bus %d %s, device number %d [%s:%s] (libusb1)", cur_bus_num, CSTR(port_numbers_str), cur_dev_num, CSTR(vendor_id), CSTR(model_id)));
            libusb_unref_device(m_usb1_device); //decrement reference count
            m_usb1_device = 0;
            break;
        }
        Debug(QS("Found device on bus %d %s, device number %d [%s:%s] (libusb1)", cur_bus_num, CSTR(port_numbers_str), cur_dev_num, CSTR(vendor_id), CSTR(model_id)));
        m_usb1_device = libusb_ref_device(dev); //increment reference count
        //do not break here, double-check

    }
    if (!m_usb1_device)
    {
        Debug(QS("Failed to find device on bus %d %s (libusb1)", bus_number, CSTR(port_numbers_str)));
    }

    //Delete the device list and unref all devices except for m_usb1_device
    //context is not deleted here, see destructor
    libusb_free_device_list(devs, 1); //unref all devices (except the one we want to keep)

    return m_usb1_device;
}

QString
UsbStorageDevice::getUsbSerialNumber(libusb_device *dev)
{
    QString serial_number;

    libusb_device_handle *handle = 0;
    int r = dev ? libusb_open(dev, &handle) : -1;
    if (r == 0)
    {
        char serial_number_bytes[256];
        libusb_device_descriptor desc;
        r = libusb_get_device_descriptor(dev, &desc);
        if (r == LIBUSB_SUCCESS)
        {
            r = libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, (unsigned char *)serial_number_bytes, sizeof(serial_number_bytes));
            if (r > 0)
            {
                serial_number = QString::fromUtf8(serial_number_bytes);
            }
            else
            {
                Debug(QS("failed to get serial number: %s", libusb_error_name(r)));
            }
        }
        libusb_close(handle);
    }
    else
    {
        Debug(QS("failed to open device: %s", libusb_error_name(r)));
    }

    return serial_number;
}

#endif //HAVE_LIBUSB1

#ifdef HAVE_LIBUSB0
//libusb0

usb_dev*
UsbStorageDevice::getUsb0Device()
{
    //Return cached device if already found
    if (m_usb0_device)
    {
        return m_usb0_device;
    }

    //Get known bus number and known ids, serial for matching
    int known_bus_number = getBusNumber();
    QString known_vendor_id, known_model_id;
    getUdevVendorAndModelId(known_vendor_id, known_model_id);
    QString known_serial_number = getUdevSerialNumber();
    Debug(QS("Looking for device with known bus number: %d, vendor id: %s, model id: %s, serial number: %s", known_bus_number, CSTR(known_vendor_id), CSTR(known_model_id), CSTR(known_serial_number)));
    if (known_bus_number == -1 || known_vendor_id.isEmpty() || known_model_id.isEmpty())
    {
        Debug(QS("Cannot find device with unknown bus number, vendor id or model id (libusb0)"));
        return 0;
    }
    if (known_serial_number.isEmpty())
    {
        Debug(QS("Cannot find device with unknown serial number (libusb0)"));
        return 0;
    }

    //Initialize libusb0
    struct usb_bus *busses;
    struct usb_bus *bus;
    usb_init();
    usb_find_busses();
    usb_find_devices();

    //Iterate over all devices
    //Without access to the full port path of a device, this algorithm is unreliable
    //as we are unable to distinguish between devices on the same hub.
    //The device number is not unique, as it is only unique within a hub.
    busses = usb_get_busses();
    for (bus = busses; bus; bus = bus->next)
    {
        struct usb_device *dev;
        for (dev = bus->devices; dev; dev = dev->next)
        {
            //Get bus number, device number
            //Full port path is not available in libusb0
            int devnum = dev->devnum;
            int busnum = atoi(bus->dirname);
            //Instead get vendor, model id and serial number
            //get model id as numeric type (hex) to avoid "0715" not matching "715"
            uint16_t vendor_id_num = dev->descriptor.idVendor;
            uint16_t model_id_num = dev->descriptor.idProduct;
            QString vendor_id = QString::number(vendor_id_num, 16);
            QString model_id = QString::number(model_id_num, 16);
            Debug(QS("Found some device on bus %d, device number %d, vendor id: %s, model id: %s (libusb0)", busnum, devnum, CSTR(vendor_id), CSTR(model_id)));

            //Match known bus number and vendor id, model id
            //convert string with id to numeric type (hex) to avoid "0715" not matching "715"
            uint16_t known_vendor_id_num = static_cast<uint16_t>(known_vendor_id.toInt(0, 16));
            uint16_t known_model_id_num = static_cast<uint16_t>(known_model_id.toInt(0, 16));
            if (busnum != known_bus_number) continue;
            if (vendor_id_num != known_vendor_id_num) continue;
            if (model_id_num != known_model_id_num) continue;
            Debug(QS("Found device of same type on bus %d, device number %d, vendor id: %s, model id: %s (libusb0)", busnum, devnum, CSTR(vendor_id), CSTR(model_id)));
            //Also compare serial number, but not before we have found a matching device type/model
            QString serial_number = getUsbSerialNumber(dev);
            if (serial_number != known_serial_number) continue;

            //All parameters match, consider the device found
            //But if we had already found it with these parameters, it is a duplicate
            if (m_usb0_device)
            {
                Debug(QS("duplicate device on bus number %d, device number %d, vendor id %s, model id %s, serial number %s (libusb0)", busnum, devnum, CSTR(vendor_id), CSTR(model_id), CSTR(serial_number)));
                m_usb0_device = 0;
                break;
            }
            Debug(QS("Found device on bus number %d, vendor id %s, model id %s, serial number %s (libusb0)", known_bus_number, CSTR(known_vendor_id), CSTR(known_model_id), CSTR(known_serial_number)));
            m_usb0_device = dev;
            //break; //TODO

        }
    }
    if (!m_usb0_device)
    {
        Debug(QS("Failed to find device with bus number %d, vendor id %s, model id %s, serial number %s (libusb0)", known_bus_number, CSTR(known_vendor_id), CSTR(known_model_id), CSTR(known_serial_number)));
    }

    return m_usb0_device;
}

QString
UsbStorageDevice::getUsbSerialNumber(usb_dev *dev)
{
    QString serial_number;

    usb_dev_handle *handle = dev ? usb_open(dev) : 0;
    if (handle)
    {
        char serial_number_bytes[256];
        int r = usb_get_string_simple(handle, dev->descriptor.iSerialNumber, serial_number_bytes, sizeof(serial_number_bytes));
        if (r > 0)
        {
            serial_number = QString::fromUtf8(serial_number_bytes);
        }
        else
        {
            Debug(QS("failed to get serial number: %s", usb_strerror()));
        }
        usb_close(handle);
    }
    else
    {
        Debug(QS("failed to open device: %s", usb_strerror()));
    }

    return serial_number;
}

#endif //HAVE_LIBUSB0


