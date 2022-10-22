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

QList<QDir>
UDiskManager::mountpoints(const QString &device)
{
    //> An array of filesystems paths for where the file system on the device is mounted. If the device is not mounted, this array is empty.
    //http://storaged.org/doc/udisks2-api/latest/gdbus-org.freedesktop.UDisks2.Filesystem.html#gdbus-property-org-freedesktop-UDisks2-Filesystem.MountPoints
    //Type is aay, that's an array of byte arrays, for example:
    //> [[47, 98, 111, 111, 116, 0]]

    QString filesystem_if = "org.freedesktop.UDisks2.Filesystem"; //interface for filesystem section

    QList<QDir> mountpoints;

    QString path = devicePath(device);
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

bool
UDiskManager::isSystemDevice(const QString &device)
{
    QString block_if = "org.freedesktop.UDisks2.Block"; //interface for block section

    QVariant var = getVariant(devicePath(device), block_if, "HintSystem");

    return var.toBool();
}

bool
UDiskManager::isHiddenDevice(const QString &device)
{
    QString block_if = "org.freedesktop.UDisks2.Block"; //interface for block section

    QVariant var = getVariant(devicePath(device), block_if, "HintHidden");

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

QString
UDiskManager::idLabel(const QString &device)
{
    QString block_if = "org.freedesktop.UDisks2.Block"; //interface for block section

    QVariant var = getVariant(devicePath(device), block_if, "IdLabel");

    return var.toString();
}

QVariantMap
UDiskManager::deviceData(const QString &device)
{
    QVariantMap dev_dict;

    //Get list of all available properties
    QString dbus_path = devicePath(device);
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
        QString path = devicePath(device);

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
UDiskManager::underlyingBlockDevice(const QString &device)
{
    QString partition_if = "org.freedesktop.UDisks2.Partition"; //interface for partition section

    QString parent_device;

    //Table property points to underlying storage device
    QVariant var = getVariant(devicePath(device), partition_if, "Table");
    QDBusObjectPath parent_obj;
    //to check if device is a partition: var.canConvert<QDBusObjectPath>();
    parent_obj = var.value<QDBusObjectPath>();
    parent_device = parent_obj.path();

    return parent_device;
}

QString
UDiskManager::drive(const QString &device)
{
    QString prop_if = "org.freedesktop.DBus.Properties"; //D-Bus interface for properties
    QString block_if = "org.freedesktop.UDisks2.Block";

    QVariant var = getVariant(devicePath(device), block_if, "Drive");

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

bool
UDiskManager::mount(const QString &device, QString *message_ref)
{
    QString filesystem_if = "org.freedesktop.UDisks2.Filesystem"; //interface for filesystem section
    QString path = devicePath(device);

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
    QString path = devicePath(device);

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

QString
UDiskManager::devicePath(const QString &name)
{
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
    args.iface = bus;
    m_dbus_ifc_conns << args;
    return *bus;
}

QVariant
UDiskManager::getVariant(const QString &path, const QString &interface, const QString &prop)
{
    //Call "Get" method through D-Bus interface
    //on object with specified D-Bus path (path)
    //requesting object property in section specified by interface name (if)
    //returning specified object property (prop)
    //path argument == full dbus path (device argument == incomplete "sda")
    QString prop_if = "org.freedesktop.DBus.Properties"; //D-Bus interface for properties
    QDBusInterface &iface = dbusInterface(path, prop_if);
    QDBusReply<QVariant> reply = iface.call("Get", interface, prop);
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
    methods = introspect(devicePath(path)).value("method").toStringList();
    return methods;
}

QStringList
UDiskManager::propertyNames(const QString &path)
{
    QStringList properties;
    properties = introspect(devicePath(path)).value("property").toStringList();
    return properties;
}

