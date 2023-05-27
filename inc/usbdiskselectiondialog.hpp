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

#ifndef USBDISKSELECTIONDIALOG_HPP
#define USBDISKSELECTIONDIALOG_HPP

#include <QDialog>
#include <QPointer>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QMessageBox>
#include <QTimer>

#include "size.hpp"
#include "storagediskselection.hpp"

class StorageDiskSelection;

class UsbDiskSelectionDialog : public QDialog
{
    Q_OBJECT

signals:

    void
    deviceSelected(const QString &dev_path, const QString &dev_title = "");

public:

    UsbDiskSelectionDialog(QWidget *parent);

private slots:

    void
    reloadList();

    void
    itemSelected(QListWidgetItem *current, QListWidgetItem *previous);

    void
    confirmSelection(QListWidgetItem *item);

    void
    confirmSelection();

private:

    StorageDiskSelection
    m_device_selection;

    QListWidget
    *m_selection_list;

    QLineEdit
    *m_txt_drive_path;

    QLineEdit
    *m_txt_drive_name;

    QLineEdit
    *m_txt_drive_vendor;

    QLineEdit
    *m_txt_dev_size;

    QDialogButtonBox
    *m_button_box;

};

#endif
