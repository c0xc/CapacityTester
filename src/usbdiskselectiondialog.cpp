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

#include "usbdiskselectiondialog.hpp"

UsbDiskSelectionDialog::UsbDiskSelectionDialog(QWidget *parent)
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
    //Path field
    m_txt_drive_path = new QLineEdit;
    m_txt_drive_path->setReadOnly(true);
    //: Internal device address, path to device file.
    form->addRow(tr("Address"), m_txt_drive_path);
    //Model field
    m_txt_drive_name = new QLineEdit;
    m_txt_drive_name->setReadOnly(true);
    //: Name of storage device, i.e.: [model] name (e.g., of USB stick; not user-defined)
    form->addRow(tr("Name"), m_txt_drive_name); //make/model
    //Vendor field
    m_txt_drive_vendor = new QLineEdit;
    m_txt_drive_vendor->setReadOnly(true);
    form->addRow(tr("Vendor"), m_txt_drive_vendor);
    //Size field
    m_txt_dev_size = new QLineEdit;
    m_txt_dev_size->setReadOnly(true);
    form->addRow(tr("Size"), m_txt_dev_size);

    //Control buttons
    m_button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(m_button_box, SIGNAL(accepted()), this, SLOT(confirmSelection()));
    connect(m_button_box, SIGNAL(rejected()), this, SLOT(close()));
    vbox->addWidget(m_button_box);

    reloadList();
}

void
UsbDiskSelectionDialog::reloadList()
{
    m_selection_list->blockSignals(true);
    m_selection_list->clear(); //clear list widget
    m_button_box->button(QDialogButtonBox::Ok)->setDisabled(true);
    m_device_selection = StorageDiskSelection(); //reset cached devices

    foreach (auto device, m_device_selection.blockDevices())
    {
        QString dev_path = device.path();

        //Only USB devices
        bool is_usb = device.isUsbDevice();
        if (!is_usb) continue;

        //Add device item to list
        //dev_path = "/dev/sda"; dev_title is some id string usually + serial
        //dev_path is the (internal) path to the block device on the system
        QString dev_title = device.identifier();
        if (dev_title.isEmpty()) dev_title = dev_path;
        QListWidgetItem *item = new QListWidgetItem(dev_title, m_selection_list);
        item->setData(Qt::UserRole, dev_path);
        item->setData(Qt::UserRole + 1, dev_title);
        item->setToolTip(dev_path);
        m_selection_list->addItem(item);

    }
    m_selection_list->blockSignals(false);

}

void
UsbDiskSelectionDialog::itemSelected(QListWidgetItem *current, QListWidgetItem *previous)
{
    Q_UNUSED(previous);
    QString dev_path = current->data(Qt::UserRole).toString();
    QString dev_title = current->data(Qt::UserRole + 1).toString();

    StorageDiskSelection::Device device = m_device_selection.blockDevice(dev_path);
    if (device.isNull())
    {
        //Invalid selection, device is not cached, shouldn't happen, block
        setDisabled(true);
        return;
    }

    m_txt_drive_path->setText(dev_path);
    m_txt_drive_name->setText(device.displayModelName());
    QString serial = device.serialNumber();
    if (serial.isEmpty())
        m_txt_drive_name->setToolTip("");
    else
        //: Serial number of storage device.
        m_txt_drive_name->setToolTip(tr("Serial") + ": " + serial);
    Size capacity = device.capacity();
    m_txt_dev_size->setText(QString("%1 / %2 B").
        arg(capacity.formatted()). //TODO fix 2048408248320B shown as 1TB
        arg((qint64)capacity));
    m_txt_drive_vendor->setText(device.vendorName());

    m_button_box->button(QDialogButtonBox::Ok)->setDisabled(false);

}

void
UsbDiskSelectionDialog::confirmSelection(QListWidgetItem *item)
{
    QString dev_path = item->data(Qt::UserRole).toString();
    QString dev_title = item->data(Qt::UserRole + 1).toString();

    emit deviceSelected(dev_path, dev_title);
    done(QDialog::Accepted);
}

void
UsbDiskSelectionDialog::confirmSelection()
{
    confirmSelection(m_selection_list->currentItem());
}

