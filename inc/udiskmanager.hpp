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

#ifndef UDISKMANAGER_HPP
#define UDISKMANAGER_HPP

#include <QDebug>
#include <QObject>
#include <QPointer>
#include <QDir>
#include <QDBusInterface>
#include <QDBusReply>
#include <QXmlStreamReader>
#include <QDBusObjectPath>

class UDiskManager : public QObject
{
    Q_OBJECT

signals:

public:

    typedef QPair<QString, QString> QStringPair;

    /**
     * Constructor.
     * A parent object can be specified to enable garbage collection.
     */
    UDiskManager(QObject *parent = 0);

    /**
     * The class instance cannot be used if this returns false.
     */
    bool
    isValid() const;

    /**
     * Returns the full file path to the device file.
     * For example, sda => /dev/sda.
     */
    QString
    deviceFilePath(const QString &device);

    /**
     * Returns true if this is a blank device with a size of zero,
     * for example an empty card reader.
     */
    bool
    isBlankDevice(const QString &device);

    /**
     * Returns a list of all mountpoints of the specified device.
     * if the device (partition) is not mounted, the list will be empty.
     */
    QList<QDir>
    mountpoints(const QString &device);

    /**
     * Returns the location where the specified device is currently mounted.
     * if the device (partition) is not mounted,
     * an empty string will be returned.
     */
    QString
    mountpoint(const QString &device);

    /**
     * Returns the device name which is mounted at mountpoint.
     */
    QString
    mountedDevice(const QString &mountpoint);

    QStringList
    mountedPartitionsOf(const QString &device);

    bool
    isAnyPartitionMountedOf(const QString &device);

    bool
    isSystemDevice(const QString &device);

    bool
    isHiddenDevice(const QString &device);

    bool
    isRemovable(const QString &device);

    bool
    isUsbDevice(const QString &device);

    QString
    idLabel(const QString &device);

    QVariantMap
    deviceData(const QString &device);

    /**
     * Returns a list of names of all block devices.
     * For example: "sda", "sda1", "sda2", "sdxa1"
     *
     * If the argument called dbus_path is true,
     * each path will be a fully qualified dbus path, for example:
     * "/org/freedesktop/UDisks2/block_devices/sda1"
     */
    QStringList
    blockDevices(bool dbus_path = false);

    QStringList
    getBlockDevices(bool dbus_path = false);

    /**
     * Returns a list of names of block devices that are partitions.
     * For example: "sda1", "sda2", "sdxa1"
     *
     * This is a sub list of the list of block devices.
     * In other words: This function only returns those devices
     * that contain a filesystem and are meant to be mounted.
     * Technically, a block device does not need to be partitioned,
     * so an hdd called sdn could be formatted with a filesystem like btrfs
     * and mounted. However, a block device is usually partitioned
     * and the individual partitions are formatted.
     * So, if the device contains a partition table,
     * it is not included in the returned list.
     *
     * If the first argument (dbus_path) is true,
     * full dbus paths will be returned instead of "sda1".
     * If the second argument (fs_only) is true,
     * only those partitions will be returned that contain
     * a known filesystem which can be mounted.
     * This switch can be used to exclude swap partitions.
     */
    QStringList
    partitions(bool dbus_path = false, bool fs_only = false);

    /**
     * Returns the list of partitions from the partition table of device.
     * With dbus_path = true, the internal dbus identifiers will be returned
     * instead of sda1. Otherwise it might be [sda1, sda2] (for device = sda).
     */
    QStringList
    partitionDevices(const QString &device, bool dbus_path = false);

    QStringList
    usbPartitions();

    /**
     * Returns the block device that contains the specified partition.
     * This will be device name (without /dev/), like "sda".
     */
    QString
    underlyingBlockDevice(const QString &device, bool dbus_path = false);

    /**
     * Returns the drive that contains the specified block device.
     * This will be a D-Bus path. Not "sda".
     * It may not be all that useful.
     */
    QString
    drive(const QString &device);

    /**
     * Returns a list of known storage drives,
     * including drives that do not contain any partitions.
     *
     * Note that as of yet, this does might not return all drives
     * including optical drives. Instead, goes through the block devices
     * and returns their drives.
     */
    QStringList
    drives();

    QStringList
    supportedFilesystems();

    //void
    //watch...
    //watchDrives...

public slots:

    /**
     * Attempts to mount the device.
     * The mountpoint will be automatically selected (usually from fstab).
     * The second argument is optional, it can be a string pointer.
     * On success, returns true and writes the mountpoint to message_ref.
     * On error, returns false and writes the error message to message_ref.
     *
     * As mounting requires root privileges (except for USB devices),
     * a password prompt will appear in a new window,
     * where the user is expected to type in his sudo password.
     * If the user does not have a sudo password (i.e., not in the wheel group
     * or otherwise in sudoers config), he is not allowed to mount devices.
     */
    bool
    mount(const QString &device, QString *message_ref = 0);

    /**
     * Attempts to unmount the device.
     * Returns true on success.
     * On error, the error message is written to message_ref.
     */
    bool
    umount(const QString &device, QString *message_ref = 0);

    /**
     * Reset disk label with new partition table.
     * This will wipe the device!
     * Do not call this without asking the user first.
     *
     * DANGER DANGER DANGER
     * IF YOU CALL THIS FUNCTION WITH THE WRONG DEVICE NAME,
     * YOU WILL NEED RECOVERY TOOLS (OR A BACKUP PLAN WITH SNAPSHOTS).
     * YOU HAVE BEEN WARNED.
     */
    void
    makeDiskLabel(const QString &device, const QString &type = "dos");

    /**
     * Create new partition on device.
     */
    bool
    createPartition(const QString &device, const QString &type, QString *message_ref = 0);

private:

    static constexpr const char *UDISKS2_SERVICE = "org.freedesktop.UDisks2";

    struct SpecInterface
    {
        QString path;
        QString interface;
        QPointer<QDBusInterface> iface;
    };

    QList<SpecInterface>
    m_dbus_ifc_conns;

    QString
    deviceDbusPath(const QString &name);

    QDBusInterface&
    dbusInterface(const QString &path, const QString &interface);

    QVariant
    getVariant(const QString &path, const QString &interface, const QString &prop);

    QVariantMap
    getAllVariantMap(const QString &path, const QString &interface);

    QByteArrayList
    getByteArrayList(const QString &path, const QString &interface, const QString &prop);

    QVariant
    getValue(const QVariant &var);

    QVariantMap
    introspect(const QString &path);

    QStringList
    methodNames(const QString &path);

    QStringList
    propertyNames(const QString &path);



};

#endif
