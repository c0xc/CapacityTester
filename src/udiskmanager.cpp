/****************************************************************************
**
** Copyright (C) 2022 Philip Seeger (p@c0xc.net)
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

#include "udiskmanager.hpp"

#ifdef UDISKMANAGER_HPP

/**
 * This class uses D-Bus to access UDisks2.
 * It won't help you if your operating system lacks these features.
 *
 * UDisks2 allows us to access storage devices without having to be root.
 * It is used on modern Linux distributions to allow the user to
 * see and mount USB drives with a click in the filemanager (like Caja)
 * without having to type in a password.
 * However, it's limited to USB drives by default.
 * If the user has a 3.5 inch hotswap bay for floppy drives (2.5 inch HDD/SSD)
 * in the chassis, mounting such a floppy drive would show a password prompt
 * because it is usually connected to the S-ATA bus, not to the USB bus.
 *
 * D-Bus allows us to communicate with system services like UDisks2.
 * It is a great invention. But trying to find out how it works
 * and how it is supposed to be used by reading the documentation
 * can lead to tears in the eyes and negative emotions.
 * If you want to understand how to use a specific service, read properties,
 * and cannot find the part of the documentation that describes
 * how to get from A to B, use tools to visualize D-Bus.
 * Use qdbus to list all available objects paths of this service:
 * $ qdbus --system org.freedesktop.UDisks2
 * Use busctl to see all properties and everything of a specific object:
 * $ busctl introspect org.freedesktop.UDisks2 /org/freedesktop/UDisks2/block_devices/sda1
 * Use the graphical tool D-Feet to browse the D-Bus
 * and send test queries.
 *
 * This program does make use of Qt's XML voodoo
 * to automatically create classes and unmarshal D-Bus responses.
 */

UDiskManager::UDiskManager(QObject *parent)
            : QObject(parent)
{
}

bool
UDiskManager::isValid() const
{
    return QDBusConnection::systemBus().isConnected();
}

QString
UDiskManager::deviceFilePath(const QString &device)
{
    QString block_if = "org.freedesktop.UDisks2.Block"; //interface for block section

    QVariant var = getVariant(deviceDbusPath(device), block_if, "Device");

    return var.toByteArray();
}

bool
UDiskManager::isBlankDevice(const QString &device)
{
    QVariantMap data = deviceData(device);
    qint64 size = data.value("Size").toLongLong();
    return size == 0;
}

QList<QDir>
UDiskManager::mountpoints(const QString &device)
{
    //> An array of filesystems paths for where the file system on the device is mounted. If the device is not mounted, this array is empty.
    //http://storaged.org/doc/udisks2-api/latest/gdbus-org.freedesktop.UDisks2.Filesystem.html#gdbus-property-org-freedesktop-UDisks2-Filesystem.MountPoints
    //Type is aay, that's an array of byte arrays, for example:
    //> [[47, 98, 111, 111, 116, 0]]

    QString filesystem_if = "org.freedesktop.UDisks2.Filesystem"; //interface for filesystem section

    QList<QDir> mountpoints;

    QString path = deviceDbusPath(device);
    foreach (QByteArray ba, getByteArrayList(path, filesystem_if, "MountPoints"))
    {
        QString mountpoint(ba);
        if (mountpoint.isEmpty()) continue;
        QDir dir(mountpoint);

        mountpoints << dir;
    }

    return mountpoints;
}

QString
UDiskManager::mountpoint(const QString &device)
{
    QList<QDir> mountpoints = this->mountpoints(device);
    if (!mountpoints.isEmpty())
        return mountpoints[0].path();
    return "";
}

QString
UDiskManager::mountedDevice(const QString &mountpoint)
{
    //Determine which device is mounted at mountpoint
    //by iterating over all devices and checking their mountpoints
    //Another option might be the GetManagedObjects method
    //but it returns a complex data structure with nested maps

    QString matching_device;

    foreach (QString some_dev, getBlockDevices())
    {
        QList<QDir> mp_dirs = mountpoints(some_dev);
        bool dev_matches = false;
        foreach (const QDir &dir, mp_dirs)
        {
            if (dir.path() == mountpoint)
                dev_matches = true;
        }
        if (!dev_matches) continue;
        //Found matching device
        matching_device = some_dev;
    }

    return matching_device;
}

QStringList
UDiskManager::mountedPartitionsOf(const QString &device)
{
    QStringList mounted_devs;

    QStringList all_parts;
    all_parts << device; //TODO
    foreach (QString dev, partitionDevices(device))
        all_parts << dev;
    foreach (QString part, all_parts)
    {
        if (!mountpoints(part).isEmpty())
            mounted_devs << part;
    }

    return mounted_devs;
}

bool
UDiskManager::isAnyPartitionMountedOf(const QString &device)
{
    return !mountedPartitionsOf(device).isEmpty();
}

bool
UDiskManager::isSystemDevice(const QString &device)
{
    QString block_if = "org.freedesktop.UDisks2.Block"; //interface for block section

    QVariant var = getVariant(deviceDbusPath(device), block_if, "HintSystem");

    return var.toBool();
}

bool
UDiskManager::isHiddenDevice(const QString &device)
{
    QString block_if = "org.freedesktop.UDisks2.Block"; //interface for block section

    QVariant var = getVariant(deviceDbusPath(device), block_if, "HintHidden");

    return var.toBool();
}

bool
UDiskManager::isRemovable(const QString &device)
{
    //> Note that this is only a guess.
    //http://storaged.org/doc/udisks2-api/latest/gdbus-org.freedesktop.UDisks2.Drive.html

    QString drive_path = drive(device);
    QString drive_if = "org.freedesktop.UDisks2.Drive"; //interface for drive section

    QVariant var = getVariant(drive_path, drive_if, "Removable");

    return var.toBool();
}

bool
UDiskManager::isUsbDevice(const QString &device)
{
    QString drive_path = drive(device);
    QString drive_if = "org.freedesktop.UDisks2.Drive"; //interface for drive section

    QString connection_bus;
    connection_bus = getVariant(drive_path, drive_if, "ConnectionBus").toString();

    return connection_bus == "usb";
}

bool
UDiskManager::isLoopDevice(const QString &device)
{
    QString loop_if = "org.freedesktop.UDisks2.Loop"; //interface for loop section

    bool failed = false;
    getVariant(deviceDbusPath(device), loop_if, "SetupByUID", &failed);

    return !failed;
}

bool
UDiskManager::isSwapDevice(const QString &device)
{
    QString swap_if = "org.freedesktop.UDisks2.Swapspace"; //interface for swap section

    bool failed = false;
    getVariant(deviceDbusPath(device), swap_if, "Active", &failed);

    return !failed;
}

QString
UDiskManager::idLabel(const QString &device)
{
    QString block_if = "org.freedesktop.UDisks2.Block"; //interface for block section

    //IdLabel is the label of the block device / partition (not the drive)
    //see driveModelName()
    QVariant var = getVariant(deviceDbusPath(device), block_if, "IdLabel");

    return var.toString();
}

qint64
UDiskManager::capacity(const QString &device)
{
    QVariantMap data = deviceData(device);
    qint64 size = data.value("Size").toLongLong();
    return size;
}

QVariantMap
UDiskManager::deviceData(const QString &device)
{
    QVariantMap dev_dict;

    //Get list of all available properties
    QString dbus_path = deviceDbusPath(device);
    dev_dict["dbus_path"] = dbus_path;
    QVariantMap data = introspect(dbus_path);

    //Get all properties
    QString prop_if_name = "org.freedesktop.DBus.Properties"; //interface for properties
    QString block_if_name = "org.freedesktop.UDisks2.Block"; //interface for block device section
    QDBusInterface &iface = dbusInterface(dbus_path, prop_if_name);
    QDBusReply<QVariantMap> reply = iface.call("GetAll", block_if_name);
    QVariantMap map = reply.value();
    foreach (QString key, map.keys())
    {
        //Strings and numbers are returned as strings, which is what we want
        //Properties which represent D-Bus object paths
        //(complex because string with ".") come out as "".
        QString attr_type = data["prop." + key].toString();
        bool is_object_path = attr_type == "o";
        QString str_value = map[key].toString();

        //Resolve complex object path
        if (is_object_path && str_value.isEmpty())
        {
            QVariant var = map[key];
            if (var.canConvert<QDBusObjectPath>())
            {
                str_value = var.value<QDBusObjectPath>().path();
            }
        }

        dev_dict[key] = str_value;
    }

    return dev_dict;
}

QString
UDiskManager::driveId(const QString &device)
{
    QString drive_path = drive(device);
    QString drive_if = "org.freedesktop.UDisks2.Drive"; //interface for drive section

    QVariant var = getVariant(drive_path, drive_if, "Id");

    return var.toString();
}

QString
UDiskManager::driveModelName(const QString &device)
{
    QString drive_path = drive(device);
    QString drive_if = "org.freedesktop.UDisks2.Drive"; //interface for drive section

    QVariant var = getVariant(drive_path, drive_if, "Model");

    return var.toString();
}

QString
UDiskManager::driveVendorName(const QString &device)
{
    QString drive_path = drive(device);
    QString drive_if = "org.freedesktop.UDisks2.Drive"; //interface for drive section

    QVariant var = getVariant(drive_path, drive_if, "Vendor");

    return var.toString();
}

QString
UDiskManager::driveSerialNumber(const QString &device)
{
    QString drive_path = drive(device);
    QString drive_if = "org.freedesktop.UDisks2.Drive"; //interface for drive section

    QVariant var = getVariant(drive_path, drive_if, "Serial");

    return var.toString();
}

QStringList
UDiskManager::blockDevices(bool dbus_path)
{
    QStringList devices;

    QString block_path = "/org/freedesktop/UDisks2/block_devices";
    foreach (QString device, introspect(block_path)["node"].toStringList())
    {
        devices << (dbus_path ? block_path + "/": "") + device;
    }

    return devices;
}

QStringList
UDiskManager::getBlockDevices(bool dbus_path)
{
    QStringList dev_list;

    QString method_path = "/org/freedesktop/UDisks2/Manager";
    QString method_if = "org.freedesktop.UDisks2.Manager";
    QDBusInterface &iface = dbusInterface(method_path, method_if);
    QDBusReply<QList<QDBusObjectPath>> reply = iface.call("GetBlockDevices", QVariantMap());
    //TODO qdbus_cast<QList<QDBusObjectPath>>(reply.value())
    foreach (const QDBusObjectPath &o, reply.value())
    {
        QString dev = o.path();
        if (!dbus_path) dev = dev.section('/', -1);
        dev_list << dev;
    }

    return dev_list;
}

QStringList
UDiskManager::partitions(bool dbus_path, bool fs_only)
{
    QStringList devices;

    QString prop_if = "org.freedesktop.DBus.Properties"; //interface for properties
    QString block_if = "org.freedesktop.UDisks2.Block"; //interface for block section
    QString filesystem_if = "org.freedesktop.UDisks2.Filesystem"; //interface for filesystem section
    QString partition_if = "org.freedesktop.UDisks2.Partition"; //interface for partition section
    QString partitiontable_if = "org.freedesktop.UDisks2.PartitionTable"; //interface for partitiontable section

    foreach (QString device, blockDevices(dbus_path))
    {
        QString path = deviceDbusPath(device);

        //Skip if device contains partitions (== is partitioned == has partition table)
        //With this check, we skip drives like "sda".
        {
            if (getVariant(path, partitiontable_if, "Type").isValid())
                continue;
        }

        //Skip if device contains no partition info
        {
            if (!getVariant(path, partition_if, "Number").isValid())
                continue;
        }

        //Skip if device does not contain a (known) filesystem
        //This switch can be used to skip swap partitions.
        if (fs_only)
        {
            //org.freedesktop.UDisks2.Filesystem.Size must be defined
            if (!getVariant(path, filesystem_if, "Size").isValid())
                continue;
        }

        devices << device;
    }

    return devices;
}

QStringList
UDiskManager::partitionDevices(const QString &device, bool dbus_path, PartitionTableInfo *table_info)
{
    QStringList sub_devices; //list of partitions in device
    QString partitiontable_if = "org.freedesktop.UDisks2.PartitionTable"; //interface for partitiontable section
    QString partition_if = "org.freedesktop.UDisks2.Partition"; //interface for partition section
    QString filesystem_if = "org.freedesktop.UDisks2.Filesystem"; //interface for filesystem section

    //Check partition table of device
    //If table_info is a valid pointer to a PartitionTableInfo object (owned by the caller, on stack),
    //the function will fill in the type of the partition table and the partitions.
    if (table_info)
    {
        //Return if this device is not partitioned
        if (getVariant(device, partitiontable_if, "Type").isValid())
            return sub_devices;
        table_info->type = getVariant(device, partitiontable_if, "Type").toString();
    }

    //The property called Partitions contains an array of dbus path objects
    //For example: /org/freedesktop/UDisks2/block_devices/sda1
    QDBusInterface &iface = dbusInterface(deviceDbusPath(device), partitiontable_if);
    QVariant v_partitions = iface.property("Partitions");
    QList<QDBusObjectPath> obj_list = qvariant_cast<QList<QDBusObjectPath>>(v_partitions);
    foreach (const QDBusObjectPath &o, obj_list)
    {
        //Resolve dbus path to device name
        QString dev_path = o.path();
        QString dev = dev_path;
        if (!dbus_path) dev = dev.section('/', -1);
        sub_devices << dev;

        //Fill in partition info, if requested
        if (table_info)
        {
            PartitionInfo part_info{}; //zero-initialize
            part_info.path = dev_path;
            part_info.number = getVariant(dev_path, partition_if, "Number").toUInt();
            part_info.start = getVariant(dev_path, partition_if, "Offset").toULongLong();
            part_info.size = getVariant(dev_path, partition_if, "Size").toULongLong();

            //Check if partition has a filesystem
            bool has_fs_ifc = false; //dbusInterface(deviceDbusPath(dev), filesystem_if).isValid();
            bool failed = false;
            //If it contains no filesystem, it won't have a Filesystem interface, causing this error:
            //"No such interface “org.freedesktop.UDisks2.Filesystem”"
            //error().type() == QDBusError::InvalidArgs
            getVariant(dev_path, filesystem_if, "Size", &failed);
            if (!failed) has_fs_ifc = true;
            part_info.fs = has_fs_ifc;
            if (has_fs_ifc)
            {
                //Note that some filesystems like exfat may report a size of 0
                //> Currently limited to xfs and ext filesystems only. 
                part_info.fs_size = getVariant(dev_path, filesystem_if, "Size").toULongLong();
            }
            table_info->partitions << part_info;
        }
    }

    //Sort partitions by number (unfortunately, they are not sorted by default)
    if (table_info)
    {
        std::sort(table_info->partitions.begin(), table_info->partitions.end(), [](const PartitionInfo &a, const PartitionInfo &b)
        {
            return a.number < b.number;
        });
    }

    return sub_devices;
}

QStringList
UDiskManager::usbPartitions()
{
    QStringList devices;

    foreach (QString device, partitions())
    {
        if (!isUsbDevice(device)) continue;

        devices << device;
    }

    return devices;
}

QString
UDiskManager::underlyingBlockDevice(const QString &device, bool dbus_path)
{
    QString partition_if = "org.freedesktop.UDisks2.Partition"; //interface for partition section

    QString parent_device;

    //Table property points to underlying storage device
    QVariant var = getVariant(deviceDbusPath(device), partition_if, "Table");
    //check if device is a partition: var.canConvert<QDBusObjectPath>();
    if (!var.canConvert<QDBusObjectPath>()) return "";
    QDBusObjectPath parent_obj;
    parent_obj = var.value<QDBusObjectPath>();
    parent_device = parent_obj.path();
    //Remove DBus path leaving only the device name //TODO use types / objects
    if (!dbus_path) parent_device = parent_device.section('/', -1);

    return parent_device;
}

QString
UDiskManager::drive(const QString &device)
{
    QString prop_if = "org.freedesktop.DBus.Properties"; //D-Bus interface for properties
    QString block_if = "org.freedesktop.UDisks2.Block";

    QVariant var = getVariant(deviceDbusPath(device), block_if, "Drive");

    QString drive_path;
    if (var.canConvert<QDBusObjectPath>())
    {
        drive_path = var.value<QDBusObjectPath>().path();
    }

    return drive_path;
}

QStringList
UDiskManager::drives()
{
    QStringList drives;

    foreach (QString device, blockDevices())
    {
        QString drive_path = this->drive(device);
        if (!drives.contains(drive_path))
            drives << drive_path;
    }

    return drives;
}

QStringList
UDiskManager::supportedFilesystems()
{
    QString manager_path = "/org/freedesktop/UDisks2/Manager";
    QString manager_if = "org.freedesktop.UDisks2.Manager";
    QDBusInterface &iface = dbusInterface(manager_path, manager_if);
    QVariant v_filesystems = iface.property("SupportedFilesystems");
    QStringList filesystems = qvariant_cast<QStringList>(v_filesystems);

    return filesystems;
}

bool
UDiskManager::mount(const QString &device, QString *message_ref)
{
    QString filesystem_if = "org.freedesktop.UDisks2.Filesystem"; //interface for filesystem section
    QString path = deviceDbusPath(device);

    //Call mount method
    QDBusInterface &iface = dbusInterface(path, filesystem_if);
    QVariantMap options_dict;
    QVariant options(options_dict);
    QDBusMessage msg = iface.call("Mount", options);
    if (msg.type() == QDBusMessage::ReplyMessage)
    {
        QDBusReply<QString> reply(msg);
        if (message_ref) *message_ref = reply.value();
        return true;
    }
    else if (msg.type() == QDBusMessage::ErrorMessage)
    {
        if (message_ref) *message_ref = msg.errorMessage();
        return false;
    }

    return false;
}

bool
UDiskManager::umount(const QString &device, QString *message_ref)
{
    QString filesystem_if = "org.freedesktop.UDisks2.Filesystem"; //interface for filesystem section
    QString path = deviceDbusPath(device);

    //Call umount method
    //If successful, type will be ReplyMessage.
    //On error, type will be ErrorMessage and a human-readable error message
    //is returned.
    QDBusInterface &iface = dbusInterface(path, filesystem_if);
    QVariantMap options_dict;
    QVariant options(options_dict);
    QDBusMessage msg = iface.call("Unmount", options);
    if (msg.type() == QDBusMessage::ReplyMessage)
    {
        return true;
    }
    else if (msg.type() == QDBusMessage::ErrorMessage)
    {
        if (message_ref) *message_ref = msg.errorMessage();
        return false;
    }

    return false;
}

bool
UDiskManager::makeDiskLabel(const QString &device, QString type, bool rescan)
{
    QString block_if = "org.freedesktop.UDisks2.Block"; //interface for block section
    QString path = deviceDbusPath(device);

    // If the option erase is used then the underlying device will be erased. Valid values include “zero” to write zeroes over the entire device before formatting, “ata-secure-erase” to perform a secure erase or “ata-secure-erase-enhanced” to perform an enhanced secure erase. 

    //Call Format method
    if (type.isEmpty()) type = "gpt"; //default if argument explicitly set to ""
    QDBusInterface &iface = dbusInterface(path, block_if);
    QVariantMap options_dict;
    QVariant options(options_dict);
    QDBusMessage msg = iface.call("Format", type, options);
    bool ok = msg.type() == QDBusMessage::ReplyMessage;

    //Rescan device (optional!)
    // This is usually not needed since the OS automatically does this when the last process with a writable file descriptor for the device closes it. 
    if (rescan)
    {
        msg = iface.call("Rescan", options);
        ok = ok && msg.type() == QDBusMessage::ReplyMessage;
    }

    return ok;
}

bool
UDiskManager::createPartition(const QString &device, const QString &type, QString *message_ref)
{
    QString partitiontable_if = "org.freedesktop.UDisks2.PartitionTable";
    QString path = deviceDbusPath(device);

    // http://storaged.org/doc/udisks2-api/latest/gdbus-org.freedesktop.UDisks2.PartitionTable.html#gdbus-method-org-freedesktop-UDisks2-PartitionTable.CreatePartitionAndFormat
    //offset 0 will be rounded up to 1M

    QDBusInterface &iface = dbusInterface(path, partitiontable_if);
    QVariantMap options_dict;
    QVariant opt(options_dict);
    QDBusMessage msg = iface.call("CreatePartitionAndFormat", (quint64)0, (quint64)0, "", "", opt, type, opt);

    if (msg.type() == QDBusMessage::ErrorMessage)
    {
        if (message_ref) *message_ref = msg.errorMessage();
        return false;
    }

    return true;
}

bool
UDiskManager::fixPartitionPermissions(const QString &device)
{
    QString filesystem_if = "org.freedesktop.UDisks2.Filesystem"; //interface for filesystem section
    QString path = deviceDbusPath(device);
    bool ok = false;

    //Mount it to fix permissions
    //NOTE device (formatted, same arg as fir createPartition) is not a partition path
    //e.g., createPartition("/dev/sde", "ext4") -> mount("/dev/sde1")
    QString mountpoint;
    QString partition;
    foreach (QString dev, partitionDevices(device))
        if (partition.isEmpty()) partition = dev;
        else { partition.clear(); break; }
    QString message;
    if (!mount(partition, &message) || message.isEmpty())
    {
        qWarning() << "Failed to mount partition" << partition << message;
        return false;
    }
    mountpoint = message;
    //QList<QDir> mountpoints = this->mountpoints(device);
    //if (mountpoints.isEmpty())
    //{
    //    ok = false;
    //}
    //else if (mountpoints.size() == 1)
    //{
    //    mountpoint = mountpoints[0].path();
    //    ok = true;
    //}
    if (!QFileInfo(mountpoint).isDir())
    {
        qWarning() << "Mountpoint is not a directory" << mountpoint;
        ok = false;
    }
    //Wait for a few seconds before unmounting
    QThread::sleep(3); //prevent: target is busy

    //chmod 777 mountpoint - must be run as root
    qInfo() << "Fixing permissions for" << mountpoint;
    QFile file(mountpoint);
    ok = ok && file.setPermissions( //not working (but not failing either)
        QFileDevice::ReadUser | QFileDevice::WriteUser | QFileDevice::ExeUser
        | QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup
        | QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther);
    if (chmod(mountpoint.toStdString().c_str(), 0777) != 0)
    {
        qWarning() << "Failed to change permissions for" << mountpoint;
        ok = false;
    }

    //Unmount
    if (!umount(partition, &message))
    {
        qWarning() << "Failed to unmount partition" << partition << message;
        return false;
    }

    return ok;
}

QString
UDiskManager::findDeviceDBusPath(const QString &device)
{
    //Dbus does not use the real path to the device file
    //For example, the device "sda" (which would be at /dev/sda) has the dbus path:
    // /org/freedesktop/UDisks2/block_devices/sda
    //The other function accepts sda1 only because it happens to be the same base name
    //as the corresponding dbus path, but doesn't resolve /dev/sda or anything.
    //This function is meant to be used with a device path like /dev/sda or sda1.
    if (device.isEmpty()) return "";

    //Check if it's already an internal path, handle that too
    bool is_dbus_path = device.startsWith("/org/freedesktop/UDisks2/block_devices/");

    //If relative, prepend /dev assuming device is a real block device
    QString path = device;
    if (!path.contains("/"))
        path = "/dev/" + path;

    //Resolve path if it's a symlink
    if (QFileInfo(path).isSymLink())
        path = QFileInfo(path).symLinkTarget();

    //Make sure it's a device file
    //Note this isn't really necessary because we only return a valid matching dbus path
    //struct stat path_stat;
    //if (stat(path.toStdString().c_str(), &path_stat) != 0)
    //    return ""; // stat failed, return empty string
    //if (!S_ISBLK(path_stat.st_mode))
    //    return ""; // not a block device, return empty string

    //Find device in list of block devices
    foreach (QString dev, blockDevices(true))
    {
        if (is_dbus_path && dev == device)
            return dev;
        if (deviceFilePath(dev) == path)
            return dev;
    }

    return "";
}

QString
UDiskManager::deviceDbusPath(const QString &name)
{
    //Dbus does not use the real path to the device file
    //For example, the device "sda" (which would be at /dev/sda):
    // /org/freedesktop/UDisks2/block_devices/sda

    //Turn "sda" into dbus path
    //NOTE that a full path is not resolved and fixed! TODO
    QString path = name;
    if (!path.contains("/"))
        path = "/org/freedesktop/UDisks2/block_devices/" + path;
    return path;
}

QDBusInterface&
UDiskManager::dbusInterface(const QString &path, const QString &interface)
{
    //QDBusInterface for specified path
    //with interface for a specific set of methods/properties
    //Find and return matching interface object
    SpecInterface args;
    args.path = path;
    args.interface = interface;
    for (int i = 0, ii = m_dbus_ifc_conns.size(); i < ii; i++)
    {
        SpecInterface item = m_dbus_ifc_conns[i];
        if (item.path == path && item.interface == interface)
        {
            return *item.iface;
        }
    }
    //Create new interface object
    QDBusInterface *bus = new QDBusInterface(UDISKS2_SERVICE, path, interface, QDBusConnection::systemBus(), this);
    assert(bus);
    args.iface = bus;
    m_dbus_ifc_conns << args;
    return *bus;
}

QVariant
UDiskManager::getVariant(const QString &path, const QString &interface, const QString &prop, bool *failed_ptr, QDBusError *dbus_error_ptr)
{
    //Call "Get" method through D-Bus interface
    //on object with specified D-Bus path (path)
    //requesting object property in section specified by interface name (if)
    //returning specified object property (prop)
    //path argument == full dbus path (device argument == incomplete "sda")
    QString prop_if = "org.freedesktop.DBus.Properties"; //D-Bus interface for properties
    QDBusInterface &iface = dbusInterface(path, prop_if);
    QDBusReply<QVariant> reply = iface.call("Get", interface, prop);
    if (failed_ptr)
        *failed_ptr = reply.error().isValid();
    if (dbus_error_ptr)
        *dbus_error_ptr = reply.error();
    return reply.value();
}

QVariantMap
UDiskManager::getAllVariantMap(const QString &path, const QString &interface)
{
    QString prop_if = "org.freedesktop.DBus.Properties"; //D-Bus interface for properties
    QDBusInterface &iface = dbusInterface(path, prop_if);
    QDBusReply<QVariantMap> reply = iface.call("GetAll", interface);
    return reply.value();
}

QByteArrayList
UDiskManager::getByteArrayList(const QString &path, const QString &interface, const QString &prop)
{
    QVariant var = getVariant(path, interface, prop);

    QByteArrayList list;
    QDBusArgument arg = var.value<QDBusArgument>();
    //arg.currentType() or raise ...
    //if QDBusArgument::UnknownType:
    //QDBusArgument: read from a write-only object
    if (arg.currentType() == QDBusArgument::UnknownType)
        return list;

    arg.beginArray();
    while (!arg.atEnd())
    {
        QByteArray bytes;
        arg >> bytes;
        list << bytes;
    }
    //arg.endArray():
    //QDBusArgument: read from a write-only object

    return list;
}

QVariant
UDiskManager::getValue(const QVariant &var)
{
    //Experimental
    //This function is meant to auto-detect the data type of a property
    //returned by getVariant().
    if (var.canConvert<QString>())
    {
        return var;
    }
    //path object? array? array of objects?
    return QVariant();
}

QVariantMap
UDiskManager::introspect(const QString &path)
{
    QVariantMap data;
    data["path"] = path;

    //If the specified path contains "property" elements,
    //data["property"] will contain a list of their names.

    //This function uses the complicated XML approach:
    //It uses the Introspectable of D-Bus to get a list of nodes,
    //formatted as XML.
    QString interface = "org.freedesktop.DBus.Introspectable";
    QDBusInterface &iface = dbusInterface(path, interface);

    //Call the introspect function (of the system's D-Bus interface)
    QDBusReply<QString> reply = iface.call("Introspect");

    //Parse XML document
    // <node name=\"sda\"/>\n  <node name=\"sda3\"/>\n ...
    // <interface name=\"org.freedesktop.DBus.Properties\">\n    <method name=\"Get\">\n      <arg type=\"s\" name=\"interface_name\" direction=\"in\"/>\n      <arg type=\"s\" name=\"property_name\" direction=\"in\"/>\n      <arg type=\"v\" name=\"value\" direction=\"out\"/>\n    </method>\n ...
    QVector<QVariantMap> elements;
    QXmlStreamReader xml(reply.value());
    while (!xml.atEnd())
    {
        xml.readNext();

        //Update element list
        if (xml.isEndElement())
        {
            QString element = xml.name().toString();
            //Pop element from list (remove last <arg> before adding new <arg)
            if (!elements.isEmpty() && elements.constLast()["element"].toString() == element)
            {
                elements.removeLast();
            }
            continue;
        }

        //Read new element
        if (!xml.isStartElement()) continue;
        //Element: <method>, <property>
        QString element = xml.name().toString();
        QString name = xml.attributes().value("name").toString();
        //Elements without attributes are skipped
        if (xml.attributes().isEmpty()) continue; //container element
        if (name.isEmpty()) continue; //element without name attribute
        //Add element.name to list
        {
            QVariantMap vm;
            vm["element"] = element;
            vm["name"] = name;
            elements << vm;
        }

        //List names in map (if property elements found, map.property = [...])
        QStringList name_list = data.value(element).toStringList();
        name_list << name;
        data[element] = name_list;

        //Read other attributes: type
        QString attr_type = xml.attributes().value("type").toString();
        if (!attr_type.isEmpty())
        {
            data["prop." + name] = attr_type;
            QString flat_path; //interface=X.property=foo.
            foreach (const QVariantMap &vm, elements)
            {
                if (!flat_path.isEmpty()) flat_path += "|";
                flat_path += vm["element"].toString() + "=" + vm["name"].toString();
            }
            if (!data[flat_path].toString().isEmpty())
            {
                qInfo() << "programmer made a mistake!" << flat_path << data[flat_path];
            }
            data[flat_path] = attr_type;
        }

        //maybe collect "arg" elements separately...
        //data.prop_NAME_arg ...
    }

    return data;
}

QStringList
UDiskManager::methodNames(const QString &path)
{
    QStringList methods;
    methods = introspect(deviceDbusPath(path)).value("method").toStringList();
    return methods;
}

QStringList
UDiskManager::propertyNames(const QString &path)
{
    QStringList properties;
    properties = introspect(deviceDbusPath(path)).value("property").toStringList();
    return properties;
}

#endif
