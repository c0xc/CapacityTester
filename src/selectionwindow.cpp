/****************************************************************************
**
** Copyright (C) 2022 Philip Seeger
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

#include "selectionwindow.hpp"

SelectionWindow::SelectionWindow(QWidget *parent)
               : QDialog(parent)
{
    if (parent) setParent(parent);
    m_list_filter = 2;

    QVBoxLayout *vbox = new QVBoxLayout;
    setLayout(vbox);
    QLabel *lbl_top = new QLabel(tr("Select the USB drive to be tested."));
    QPushButton *btn_refresh = new QPushButton(tr("Refresh"));
    connect(btn_refresh, SIGNAL(clicked()), SLOT(refresh()));
    QHBoxLayout *hbox_top = new QHBoxLayout;
    vbox->addLayout(hbox_top);
    hbox_top->addWidget(lbl_top);
    hbox_top->addStretch();
    hbox_top->addWidget(btn_refresh);

    wid_main = new QWidget;
    vbox->addWidget(wid_main);

    QHBoxLayout *hbox_bottom = new QHBoxLayout;
    vbox->addLayout(hbox_bottom);
    QPushButton *btn_close = new QPushButton(tr("Close"));
    connect(btn_close, SIGNAL(clicked()), SLOT(close()));
    hbox_bottom->addStretch();
    hbox_bottom->addWidget(btn_close);

    QTimer::singleShot(0, this, SLOT(refresh()));

}

void
SelectionWindow::refresh()
{
    //Clear main widget
    foreach (QObject *obj, wid_main->children())
        obj->deleteLater();
    QVBoxLayout *vbox = new QVBoxLayout;
    if (wid_main->layout()) delete wid_main->layout();
    wid_main->setLayout(vbox);
    //Reset list of known volumes etc.
    listed_volumes.clear();
    sel_index = -1;

    //Show warning if UDiskManager unavailable
    bool have_manager = false;
#ifdef UDISKMANAGER_HPP
    UDiskManager manager;
    have_manager = manager.isValid();
#endif
    if (!have_manager)
    {
        QLabel *lbl_warn = new QLabel(
            tr("The USB device selection is not available on this platform. Instead, all available mountpoints are shown here. Please find your USB drive in the list below and confirm your selection (it must be mounted)."));
        lbl_warn->setWordWrap(true);
        vbox->addWidget(lbl_warn);
        m_list_filter = 0;
    }

    //Hbox
    QHBoxLayout *hbox = new QHBoxLayout;
    vbox->addLayout(hbox);

    //Volume selection list (left side)
    QVBoxLayout *vbox_left = new QVBoxLayout;
    QLabel *lbl_top_list = new QLabel(tr("Drives"));
    hbox->addLayout(vbox_left);
    QListWidget *list = new QListWidget;
    connect(list, SIGNAL(currentRowChanged(int)), SLOT(showSelection(int)));
    list->setToolTip(tr("Drives"));

    //Options under list
    QButtonGroup *chk_group = new QButtonGroup(wid_main);
    QRadioButton *chk_mountpoints = new QRadioButton(tr("All mountpoints"));
    chk_mountpoints->setToolTip(tr("All mountpoints are listed, including system mountpoints."));
    chk_group->addButton(chk_mountpoints, 0);
    if (m_list_filter == 0) chk_mountpoints->setChecked(true);
    QRadioButton *chk_all_devs = new QRadioButton(tr("All filesystems"));
    chk_all_devs->setToolTip(tr("All detected storage devices are listed."));
    chk_group->addButton(chk_all_devs, 1);
    if (!have_manager) chk_all_devs->setDisabled(true);
    if (m_list_filter == 1) chk_all_devs->setChecked(true);
    QRadioButton *chk_usb_devs = new QRadioButton(tr("USB filesystems"));
    chk_usb_devs->setToolTip(tr("Only USB storage devices are listed."));
    chk_group->addButton(chk_usb_devs, 2);
    if (!have_manager) chk_usb_devs->setDisabled(true);
    if (m_list_filter == 2) chk_usb_devs->setChecked(true);
    connect(chk_group, SIGNAL(idClicked(int)), SLOT(filterVolChanged(int)));
    QVBoxLayout *box_option = new QVBoxLayout;
    box_option->addWidget(chk_mountpoints);
    box_option->addWidget(chk_all_devs);
    box_option->addWidget(chk_usb_devs);

    //Left vbox
    vbox_left->addWidget(lbl_top_list);
    vbox_left->addWidget(list);
    vbox_left->addLayout(box_option);

#ifdef UDISKMANAGER_HPP
    //Fill list, get items from manager or Qt
    if (have_manager && m_list_filter != 0)
    {
        //Use UDiskManager to get list of connected USB devices
        QStringList devs = manager.usbPartitions();
        if (m_list_filter == 1) devs = manager.partitions();
        foreach (QString device, devs)
        {
            QString mountpoint = manager.mountpoint(device);
            QString label = manager.idLabel(device);
            QString caption = device; //show device name by default
            if (!label.isEmpty()) caption = label; //show label if available
            if (!mountpoint.isEmpty()) //add mountpoint
                caption += " [" + mountpoint + "]";

            QStringPair sp(mountpoint, device);
            listed_volumes << sp;
            list->addItem(caption);
        }
    }
    else
#endif
    {
        //Fallback: Use QStorageInfo to get mountpoints, USB or not (unknown)
        foreach (const QStorageInfo &storage, QStorageInfo::mountedVolumes())
        {
            QString mountpoint = storage.rootPath();
            QString caption = mountpoint;

            QStringPair sp(mountpoint, "");
            listed_volumes << sp;
            list->addItem(caption);
        }
    }

    //Right details area layout
    QGridLayout *grid = new QGridLayout;
    QVBoxLayout *vbox_right = new QVBoxLayout;
    QLabel *lbl_top_details = new QLabel(tr("Details"));
    vbox_right->addWidget(lbl_top_details);
    vbox_right->addLayout(grid);
    hbox->addLayout(vbox_right);

    //Details form, top part
    QLabel *lbl;
    int row = 0;
    lbl = new QLabel(tr("Mountpoint"));
    txt_mountpoint = new QLineEdit;
    txt_mountpoint->setReadOnly(true);
    grid->addWidget(lbl, row, 0);
    grid->addWidget(txt_mountpoint, row, 1);
    row++;
    //: Label means user-defined name/title (of the selected drive).
    lbl = new QLabel(tr("Label"));
    txt_label = new QLineEdit;
    txt_label->setReadOnly(true);
    grid->addWidget(lbl, row, 0);
    grid->addWidget(txt_label, row, 1);
    row++;
    lbl = new QLabel(tr("Capacity"));
    txt_capacity = new QLineEdit;
    txt_capacity->setReadOnly(true);
    grid->addWidget(lbl, row, 0);
    grid->addWidget(txt_capacity, row, 1);
    row++;

    //File contents list
    lst_contents = new QListWidget;
    connect(lst_contents, SIGNAL(itemDoubleClicked(QListWidgetItem*)), SLOT(updateContentsList(QListWidgetItem*)));
    lst_contents->setToolTip(tr("Contents of selected drive"));
    grid->addWidget(lst_contents, row, 0, 1, -1);
    row++;

    grid->setRowStretch(row, 1);

    //Buttons: confirm selection; mount...
    QHBoxLayout *hbox_mount = new QHBoxLayout;
    //: Button: Mount filesystem (action).
    btn_mount = new QPushButton(tr("Mount"));
    btn_mount->setDisabled(true);
    connect(btn_mount, SIGNAL(clicked()), SLOT(mount()));
    hbox_mount->addWidget(btn_mount);
    //: Button: Unmount filesystem (action).
    btn_umount = new QPushButton(tr("Unmount"));
    btn_umount->setDisabled(true);
    connect(btn_umount, SIGNAL(clicked()), SLOT(umount()));
    hbox_mount->addWidget(btn_umount);
    //: Button: Confirm selection, selected filesystem (action).
    btn_select = new QPushButton(tr("Select and Continue"));
    btn_select->setDisabled(true);
    connect(btn_select, SIGNAL(clicked()), SLOT(applySelection()));
    grid->addLayout(hbox_mount, row, 0, 1, -1);
    row++;
    grid->addWidget(btn_select, row, 0, 1, -1);

    //If only one drive found, select it
    if (listed_volumes.size() == 1)
    {
        list->item(0)->setSelected(true);
        showSelection(0);
    }
    else
    {
        QTimer::singleShot(0, list, SLOT(setFocus()));
    }
}

void
SelectionWindow::showSelection(int index)
{
    if (index < 0) return;
    sel_index = index;
    QStringPair sp = listed_volumes.at(index);
    QString mountpoint = sp.first;

    //Update form (right, next to the list) to show details of selected volume

    txt_mountpoint->setText(mountpoint);

    QStorageInfo storage(mountpoint);
    Size capacity = storage.bytesTotal();
    txt_capacity->setText(QString("%1 / %2 B").
        arg(capacity.formatted()).
        arg((qint64)capacity));

#ifdef UDISKMANAGER_HPP
    UDiskManager manager;
    if (manager.isValid())
    {
        QString device;
        foreach (QString cur_dev, manager.blockDevices())
        {
            if (manager.mountpoint(cur_dev) == mountpoint)
                device = cur_dev;
        }
        QString label = manager.idLabel(device); //label can be blank
        txt_label->setText(label);
    }
    else
#endif
    {
        QStorageInfo storage(mountpoint);
        txt_label->setText(storage.name());
    }

    //Load preview list (files in selected volume)
    updateContentsList("");

    //Enable mount, unmount buttons
#ifdef UDISKMANAGER_HPP
    btn_mount->setDisabled(!mountpoint.isEmpty());
    btn_umount->setDisabled(mountpoint.isEmpty());
#endif
    btn_select->setDisabled(false);
    QTimer::singleShot(0, btn_select, SLOT(setFocus()));

}

void
SelectionWindow::applySelection()
{
    QString mountpoint = currentMountpoint();
    if (mountpoint.isEmpty())
    {
        QMessageBox::warning(this, tr("Invalid selection"),
            tr("No mountpoint selected. The drive you want to select must be mounted."));
        return;
    }

    emit volumeSelected(mountpoint);
    close();
}

void
SelectionWindow::updateContentsList(const QString &file)
{
    //Reload list of files in selected volume, if mounted

    //Current mountpoint
    QString mountpoint = currentMountpoint();
    if (mountpoint.isEmpty())
    {
        lst_contents->setDisabled(true);
        return;
    }

    //Assemble path to new subdirectory (or file)
    QString cur_subdir;
    if (!file.isEmpty())
        cur_subdir = lst_contents->property("subdir").toString();
    QString path = QDir(mountpoint).filePath(cur_subdir); //old directory path
    path = QDir::cleanPath(path);
    if (!QDir(path).exists()) return; //current dir gone, don't update
    //Info on selected file or directory
    QFileInfo fi(path, file);
    path = fi.canonicalFilePath();
    if (path.isEmpty()) return; //file gone, don't update
    if (QFileInfo(path).isFile())
    {
        //Ignore file, no action
        return;
    }
    if (!QFileInfo(path).isDir()) return; //just to be explicit
    //Check if selected subdirectory points outside of the mountpoint
    QStorageInfo mount(path);
    if (QDir(mount.rootPath()) != QDir(mountpoint))
    {
        QMessageBox::warning(this, tr("Cannot show directory"),
            tr("The subdirectory %1 is outside of the selected mountpoint %2.").
            arg(file).arg(mountpoint));
        return;
    }

    //List files in clicked (sub)directory
    lst_contents->clear();
    QDir dir(path);
    lst_contents->setProperty("subdir", dir.path()); //save new subdirectory
    QStringList sublist;
    QFileInfoList fi_list = dir.entryInfoList(QDir::NoDot | QDir::AllEntries, QDir::DirsFirst);
    foreach (QFileInfo fi, fi_list)
    {
        QString name = fi.fileName();
        if (fi.isDir()) name += "/";
        sublist << name;
        lst_contents->addItem(name);
    }
    lst_contents->setProperty("file_list", sublist); //save new file list
    lst_contents->setDisabled(false);

}

void
SelectionWindow::updateContentsList(QListWidgetItem *item)
{
    //Reload list of files in selected volume, if mounted

    //Current mountpoint
    QString mountpoint = currentMountpoint();

    //Name of selected item
    int new_index = lst_contents->row(item);
    QStringList cur_files = lst_contents->property("file_list").toStringList();
    QString new_item;
    if (new_index >= 0 && new_index < cur_files.size())
        new_item = cur_files[new_index];
    updateContentsList(new_item);

}

void
SelectionWindow::filterVolChanged(int id)
{
    m_list_filter = id;
    refresh();
}

void
SelectionWindow::mount()
{
    QString mountpoint = currentMountpoint();
    QString device = currentDevice();

#ifdef UDISKMANAGER_HPP

    //Need UDiskManager and selected device
    UDiskManager manager;
    if (device.isEmpty() || !manager.isValid()) return;

    //Ask
    if (QMessageBox::question(this, tr("Mount device?"),
        tr("Do you want to mount the selected device %1?").
        arg(device),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    //Call mount method
    QString message;
    if (manager.mount(device, &message))
    {
        QMessageBox::information(this, tr("Mount device"),
            tr("The selected device has been mounted here: %1").
            arg(message));
    }
    else
    {
        QMessageBox::critical(this, tr("Mount device"),
            tr("The selected device could not be mounted. %1").
            arg(message));
    }

#else

    return;

#endif

    //Refresh view
    QTimer::singleShot(0, this, SLOT(refresh()));
}

void
SelectionWindow::umount()
{
    QString mountpoint = currentMountpoint();
    QString device = currentDevice();

#ifdef UDISKMANAGER_HPP

    //Need UDiskManager and selected device
    UDiskManager manager;
    if (device.isEmpty() || !manager.isValid()) return;

    //Ask
    if (QMessageBox::question(this, tr("Unmount device?"),
        tr("Do you want to unmount the selected device, currently mounted at %1?").
        arg(mountpoint),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    //Call umount method
    QString message;
    if (manager.umount(device, &message))
    {
        QMessageBox::information(this, tr("Unmount device"),
            tr("The selected device has been unmounted."));
    }
    else
    {
        QMessageBox::critical(this, tr("Unmount device"),
            tr("The selected device could not be unmounted. %1").
            arg(message));
    }

#else

    return;

#endif

    //Refresh view
    QTimer::singleShot(0, this, SLOT(refresh()));
}

QString
SelectionWindow::currentMountpoint()
{
    QString mountpoint;
    if (sel_index >= 0)
    {
        QStringPair sp = listed_volumes.at(sel_index);
        mountpoint = sp.first;
    }

    return mountpoint;
}

QString
SelectionWindow::currentDevice()
{
    QString device;
    if (sel_index >= 0)
    {
        QStringPair sp = listed_volumes.at(sel_index);
        device = sp.second;
    }

    return device;
}

