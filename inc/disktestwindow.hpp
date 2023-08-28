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

#ifndef DISKTESTWINDOW_HPP
#define DISKTESTWINDOW_HPP

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
#include <QCheckBox>
#include <QRadioButton>

#include "size.hpp"
#include "usbdiskselectiondialog.hpp"
#include "destructivedisktester.hpp"
#include "capacitytestergui.hpp" //TODO init disk test
#include "performancegraph.hpp"

class DiskTestWindow : public QDialog
{
    Q_OBJECT

signals:

public:

    static QFrame*
    newHLine();

    DiskTestWindow(QWidget *parent = 0);

private slots:

    void
    cancel();

    void
    confirm();

    void
    showDevSelect();

    void
    showDev(const QString &dev_path, const QString &dev_title);

    void
    showStartTest();

    void
    startTest();

    void
    startTestPhase(int phase);

    void
    handleTestFinished(bool success);

private:

    QVBoxLayout*
    resetMainLayout();

    QWidget
    *wid_dev_sel;

    QWidget
    *wid_main;

    QDialogButtonBox
    *m_button_box;

    int
    m_step;

    QString
    m_dev_path;

    QString
    m_dev_title;

    qint64
    m_claimed_capacity;

    qint64
    m_wr_err;

    qint64
    m_rd_err;

    int
    m_sel_type;

    QPointer<UsbDiskSelectionWidget>
    m_disk_selection_widget;

    QPointer<DestructiveDiskTester>
    m_dd_worker;

    PerformanceGraph
    *m_graph_write;

    PerformanceGraph
    *m_graph_read;

    QLabel
    *m_lbl_phase;

    //QLabel
    //*m_lbl_phase_writing;

    //QLabel
    //*m_lbl_phase_verifying;

    int
    m_ddt_phase;

    QLabel
    *m_lbl_finish_desc;



};

#endif
