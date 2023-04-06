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

#ifndef UDISKFORMATDIALOG_HPP
#define UDISKFORMATDIALOG_HPP

#include <QDebug>
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

#include "udiskmanager.hpp"
#include "size.hpp"

class UDiskFormatDialog : public QDialog
{
    Q_OBJECT

signals:

public:

    UDiskFormatDialog(QString dev, QWidget *parent);

private slots:

    void
    updateInfo();

    void
    umount();

    void
    formatNow();

private:

    UDiskManager
    udisk;

    QString
    m_dev;

    QLineEdit
    *m_txt_drive_name;

    QLineEdit
    *m_txt_dev_size;

    QLineEdit
    *m_txt_partitions;

    QComboBox
    *m_cmb_fs;

    QLabel
    *m_lbl_warn_mounted;

    QPushButton
    *m_btn_umount;

    QPushButton
    *m_btn_format;

    QPushButton
    *m_btn_chmod;

};

class UDiskSelectionDialog : public QDialog
{
    Q_OBJECT

signals:

    void
    deviceSelected(const QString &dev);

public:

    UDiskSelectionDialog(QWidget *parent);

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

    QListWidget
    *m_selection_list;

    QLineEdit
    *m_txt_drive_name;

    QLineEdit
    *m_txt_dev_size;

    QDialogButtonBox
    *m_button_box;

    UDiskManager
    udisk;

};

#endif
