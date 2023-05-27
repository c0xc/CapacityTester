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

#ifndef SELECTIONWINDOW_HPP
#define SELECTIONWINDOW_HPP

#include <QObject>
#include <QPointer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QListWidget>
#include <QStorageInfo>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QDialog>
#include <QTimer>
#include <QMessageBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QButtonGroup>

#include "size.hpp"
#include "udiskmanager.hpp"

class SelectionWindow : public QDialog
{
    Q_OBJECT

public:

    typedef QPair<QString, QString> QStringPair;

    SelectionWindow(QWidget *parent = 0);


signals:

    void
    volumeSelected(const QString&);

public slots:

    void
    refresh();

    void
    showSelection(int index);

    void
    applySelection();

    void
    updateContentsList(const QString &file);

    void
    updateContentsList(QListWidgetItem *item);

private slots:

    void
    filterVolChanged(int id);

    void
    mount();

    void
    umount();

private:

    int
    m_list_filter;

    int
    sel_index;

    QList<QStringPair>
    listed_volumes;

    QListWidget
    *lst_contents;

    QWidget
    *wid_main;

    QPointer<QLineEdit>
    txt_mountpoint;

    QPointer<QLineEdit>
    txt_label;

    QPointer<QLineEdit>
    txt_capacity;

    QPointer<QPushButton>
    btn_mount;

    QPointer<QPushButton>
    btn_umount;

    QPointer<QPushButton>
    btn_select;

    QString
    currentMountpoint();

    QString
    currentDevice();

};

#endif
