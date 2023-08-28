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

UsbDiskSelectionWidget::UsbDiskSelectionWidget(bool show_title, bool show_buttons, QWidget *parent)
                      : QDialog(parent),
                        m_only_usb(true)
{
    QVBoxLayout *vbox = new QVBoxLayout;
    setLayout(vbox);

    //Title, separator line
    if (show_title)
    {
        //: Storage device (selection list) / usb thumb drive / disk
        QLabel *lbl_title = new QLabel(tr("Select a storage device"));
        lbl_title->setStyleSheet("font-weight:bold; color:darkgray; font-size:18pt;");
        vbox->addWidget(lbl_title);
        QFrame *hline1 = new QFrame;
        hline1->setFrameShape(QFrame::HLine);
        vbox->addWidget(hline1);
    }

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
    //Filter
    QCheckBox *chk_filter_usb = new QCheckBox(tr("USB devices only"));
    chk_filter_usb->setChecked(true);
    connect(chk_filter_usb, SIGNAL(stateChanged(int)),
            SLOT(setUsbFilter(int)));
    vbox->addWidget(chk_filter_usb);

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
    m_button_box = 0;
    if (show_buttons)
    {
        m_button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(m_button_box, SIGNAL(accepted()), this, SLOT(confirmSelection()));
        connect(m_button_box, SIGNAL(rejected()), this, SIGNAL(rejected()));
        vbox->addWidget(m_button_box);
    }

    reloadList();
}

void
UsbDiskSelectionWidget::accept()
{
    confirmSelection();
}

void
UsbDiskSelectionWidget::reloadList()
{
    m_selection_list->blockSignals(true);
    m_selection_list->clear(); //clear list widget
    if (m_button_box)
        m_button_box->button(QDialogButtonBox::Ok)->setDisabled(true);
    m_device_selection = StorageDiskSelection(); //reset cached devices

    foreach (auto device, m_device_selection.blockDevices())
    {
        QString dev_path = device.path();

        //Only USB devices
        if (m_only_usb)
        {
            bool is_usb = device.isUsbDevice();
            if (!is_usb) continue;
        }

        //Skip unnamed devices
        QString dev_title = device.identifier();
        if (dev_title.isEmpty()) continue;

        //Add device item to list
        //dev_path = "/dev/sda"; dev_title is some id string usually + serial
        //dev_path is the (internal) path to the block device on the system
        if (dev_title.isEmpty()) dev_title = dev_path;
        QListWidgetItem *item = new QListWidgetItem(dev_title, m_selection_list);
        item->setData(Qt::UserRole, dev_path);
        item->setData(Qt::UserRole + 1, dev_title);
        item->setToolTip(dev_path);
        m_selection_list->addItem(item);

        //Highlight if mounted //TODO not fully implemented
        //if (device.isMounted())
        //{
        //    item->setBackground("red");
        //}

    }
    m_selection_list->blockSignals(false);

}

void
UsbDiskSelectionWidget::setUsbFilter(int state)
{
    m_only_usb = state;
    reloadList();
}

void
UsbDiskSelectionWidget::itemSelected(QListWidgetItem *current, QListWidgetItem *previous)
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
    m_txt_dev_size->setText(capacity.formatted());
    m_txt_dev_size->setToolTip(QString("%1 GB (%2 B)").
        arg(capacity.gb()).
        arg((qint64)capacity));
    m_txt_drive_vendor->setText(device.vendorName());

    if (m_button_box)
        m_button_box->button(QDialogButtonBox::Ok)->setDisabled(false);

}

void
UsbDiskSelectionWidget::confirmSelection(QListWidgetItem *item)
{
    QString dev_path = item->data(Qt::UserRole).toString();
    QString dev_title = item->data(Qt::UserRole + 1).toString();

    emit deviceSelected(dev_path, dev_title);
    done(QDialog::Accepted);
}

void
UsbDiskSelectionWidget::confirmSelection()
{
    if (!m_selection_list->currentItem()) return;
    confirmSelection(m_selection_list->currentItem());
}

UsbDiskSelectionDialog::UsbDiskSelectionDialog(QWidget *parent)
                      : UsbDiskSelectionWidget(true, true, parent)
{
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint | Qt::WindowTitleHint);
    //Qt::WindowTitleHint | Qt::WindowSystemMenuHint
    //Qt::WindowMinMaxButtonsHint

    connect(this, SIGNAL(rejected()), this, SLOT(close()));
}

