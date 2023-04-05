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

#include "udiskformatdialog.hpp"

UDiskFormatDialog::UDiskFormatDialog(QString dev, QWidget *parent)
                 : QDialog(parent)
{
    //Usually dev should be a block device but it could be a mountpoint
    //in that case we determine the device and ask to unmount it
    if (dev.isEmpty())
    {
        QMessageBox::critical(this, tr("Error"),
            tr("No drive selected."));
        done(QDialog::Rejected);
        return;
    }
    else if (QFileInfo(dev).isDir())
    {
        QString actual_dev = udisk.mountedDevice(dev);
        if (actual_dev.isEmpty())
        {
            QMessageBox::critical(this, tr("Error"),
                tr("This is not a valid drive: %1").arg(dev));
            done(QDialog::Rejected);
            return;
        }
        dev = actual_dev;
        QString parent_dev = udisk.underlyingBlockDevice(actual_dev);
        if (!parent_dev.isEmpty())
            dev = parent_dev;
    }
    m_dev = dev;

    //Layout
    QVBoxLayout *vbox = new QVBoxLayout;
    setLayout(vbox);

    //Title, separator line
    QLabel *lbl_title = new QLabel(tr("Format drive: %1").arg(dev));
    lbl_title->setStyleSheet("font-weight:bold; color:darkgray; font-size:18pt;");
    vbox->addWidget(lbl_title);
    QFrame *hline1 = new QFrame;
    hline1->setFrameShape(QFrame::HLine);
    vbox->addWidget(hline1);

    //Details form
    QFormLayout *form = new QFormLayout;
    vbox->addLayout(form);
    m_txt_drive_name = new QLineEdit;
    m_txt_drive_name->setReadOnly(true);
    form->addRow(tr("Drive name"), m_txt_drive_name);
    m_txt_dev_size = new QLineEdit;
    m_txt_dev_size->setReadOnly(true);
    form->addRow(tr("Size"), m_txt_dev_size);
    m_txt_partitions = new QLineEdit;
    m_txt_partitions->setReadOnly(true);
    form->addRow(tr("Partitions"), m_txt_partitions); //current partition layout
    m_cmb_fs = new QComboBox;
    m_cmb_fs->setEditable(false);
    form->addRow(tr("New filesystem"), m_cmb_fs); //filesystem for new partition
    QStringList fs_list_2 = udisk.supportedFilesystems();
    foreach (QString type, QStringList() << "exfat" << "ext4")
    {
        int i = fs_list_2.indexOf(type);
        if (i == -1) continue;
        m_cmb_fs->addItem(fs_list_2.takeAt(i));
    }
    m_cmb_fs->insertSeparator(m_cmb_fs->count());
    m_cmb_fs->addItems(fs_list_2);
    vbox->addStretch();

    //Warning label for when drive is still mounted (any of its partitions)
    m_lbl_warn_mounted = new QLabel(tr("This drive is currently mounted! You need to unmount it before formatting it. Make sure you don't have any valuable data on it."));
    m_lbl_warn_mounted->setWordWrap(true);
    m_lbl_warn_mounted->setStyleSheet("border:3px dashed red; background-color:orange; color:black;");
    m_lbl_warn_mounted->setVisible(false);
    vbox->addWidget(m_lbl_warn_mounted);

    //Control buttons
    QDialogButtonBox *button_box = new QDialogButtonBox(QDialogButtonBox::Cancel);
    connect(button_box, SIGNAL(rejected()), this, SLOT(close()));
    m_btn_umount = new QPushButton(tr("Unmount"));
    m_btn_umount->setDisabled(true);
    connect(m_btn_umount, SIGNAL(clicked()), SLOT(umount()));
    button_box->addButton(m_btn_umount, QDialogButtonBox::ActionRole);
    m_btn_format = new QPushButton(tr("Format")); //start formatting
    m_btn_format->setToolTip(tr("Format this drive now, wiping all data on it."));
    m_btn_format->setDisabled(true);
    connect(m_btn_format, SIGNAL(clicked()), SLOT(formatNow()));
    button_box->addButton(m_btn_format, QDialogButtonBox::ApplyRole);
    vbox->addWidget(button_box);

    updateInfo();

}

void
UDiskFormatDialog::updateInfo()
{
    QVariantMap data = udisk.deviceData(m_dev);
    QString drive_name = data.value("Drive").toString().section('/', -1);
    m_txt_drive_name->setText(drive_name);
    Size capacity = data.value("Size").toLongLong();
    //TODO fix 2048408248320B shown as 1TB
    double gb = std::div(capacity.bytes(), (qint64)1024*1024*1024).quot;
    m_txt_dev_size->setText(QString("%1 GB").arg(gb, 0, 'f', 1));

    QString parts_txt = "-";
    QStringList part_devs = udisk.partitionDevices(m_dev);
    if (!part_devs.isEmpty())
    {
        parts_txt = part_devs.join(", ");
    }
    m_txt_partitions->setText(parts_txt);

    QStringList mounted_parts = udisk.mountedPartitionsOf(m_dev);
    m_lbl_warn_mounted->setToolTip(mounted_parts.join(", "));
    if (mounted_parts.isEmpty())
    {
        m_lbl_warn_mounted->setVisible(false);
        m_btn_umount->setDisabled(true);
        m_btn_format->setDisabled(false);
    }
    else
    {
        m_lbl_warn_mounted->setVisible(true);
        m_btn_umount->setDisabled(false);
        m_btn_format->setDisabled(true);
    }

}

void
UDiskFormatDialog::umount()
{
    setDisabled(true);

    foreach (QString part_dev, udisk.mountedPartitionsOf(m_dev))
    {
        //Call umount method
        QString message;
        if (!udisk.umount(part_dev, &message))
        {
            QMessageBox::critical(this, tr("Unmount device"),
                tr("The selected device could not be unmounted. %1").
                arg(message));
            return;
        }
    }

    setDisabled(false);
    updateInfo();
}

void
UDiskFormatDialog::formatNow()
{
    QString fs_type = m_cmb_fs->currentText();
    if (fs_type.isEmpty()) return;

    setDisabled(true);

    if (QMessageBox::question(this, tr("Format now?"),
        tr("Are you sure you want to format this drive now? All data on this drive will be lost.")) != QMessageBox::Yes)
    {
        setDisabled(false);
        return;
    }

    udisk.makeDiskLabel(m_dev);

    QString message;
    if (!udisk.createPartition(m_dev, fs_type, &message))
    {
        QMessageBox::critical(this, tr("Error"),
            tr("Failed to format drive.\n%1").arg(message));
    }

    QMessageBox::information(this, tr("Drive formatted"),
        tr("The drive has been formatted."));
    done(QDialog::Accepted);
}

UDiskSelectionDialog::UDiskSelectionDialog(QWidget *parent)
                    : QDialog(parent)
{
    QVBoxLayout *vbox = new QVBoxLayout;
    setLayout(vbox);

    //Title, separator line
    QLabel *lbl_title = new QLabel(tr("Select a drive"));
    lbl_title->setStyleSheet("font-weight:bold; color:darkgray; font-size:18pt;");
    vbox->addWidget(lbl_title);
    QFrame *hline1 = new QFrame;
    hline1->setFrameShape(QFrame::HLine);
    vbox->addWidget(hline1);

    //Selection list with available devices
    m_selection_list = new QListWidget;
    m_selection_list->setStyleSheet("font-weight:bold; font-size:16pt;");
    m_selection_list->setCursor(Qt::PointingHandCursor);
    connect(m_selection_list, SIGNAL(itemActivated(QListWidgetItem*)),
        SLOT(confirmSelection(QListWidgetItem*)));
    connect(m_selection_list,
        SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)),
        SLOT(itemSelected(QListWidgetItem*, QListWidgetItem*)));
    vbox->addWidget(m_selection_list);

    //Details form
    QWidget *wid_details = new QWidget;
    vbox->addWidget(wid_details);
    QFormLayout *form = new QFormLayout;
    wid_details->setLayout(form);
    m_txt_drive_name = new QLineEdit;
    m_txt_drive_name->setReadOnly(true);
    form->addRow(tr("Drive name"), m_txt_drive_name);
    m_txt_dev_size = new QLineEdit;
    m_txt_dev_size->setReadOnly(true);
    form->addRow(tr("Size"), m_txt_dev_size);

    //Control buttons
    QDialogButtonBox *button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(button_box, SIGNAL(accepted()), this, SLOT(confirmSelection()));
    //m_btn_umount = new QPushButton(tr("Unmount"));
    //m_btn_umount->setDisabled(true);
    //button_box->addButton(m_btn_umount, QDialogButtonBox::ActionRole);
    vbox->addWidget(button_box);

    //QTimer::singleShot(0, this, SLOT(reloadList()));
    reloadList();
}

void
UDiskSelectionDialog::reloadList()
{
    m_selection_list->clear();

    foreach (QString dev, udisk.blockDevices())
    {
        //Skip devices which have parents (partitions)
        if (!udisk.underlyingBlockDevice(dev).isEmpty()) continue;
        //Skip blank drives
        if (udisk.isBlankDevice(dev)) continue;

        //Only USB devices
        bool is_usb = udisk.isUsbDevice(dev);
        if (!is_usb) continue;

        //Add device
        m_selection_list->addItem(dev);
    }

}

void
UDiskSelectionDialog::itemSelected(QListWidgetItem *current, QListWidgetItem *previous)
{
    QString dev = current->text();

    QVariantMap data = udisk.deviceData(dev);
    QString drive_name = data.value("Drive").toString().section('/', -1);
    m_txt_drive_name->setText(drive_name);
    Size capacity = data.value("Size").toLongLong();
    m_txt_dev_size->setText(QString("%1 / %2 B").
        arg(capacity.formatted()). //TODO fix 2048408248320B shown as 1TB
        arg((qint64)capacity));


}

void
UDiskSelectionDialog::confirmSelection(QListWidgetItem *item)
{
    QString dev = item->text();
    emit deviceSelected(dev);
    done(QDialog::Accepted);
}

void
UDiskSelectionDialog::confirmSelection()
{
    confirmSelection(m_selection_list->currentItem());
}

