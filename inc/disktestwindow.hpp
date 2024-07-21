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
#include <QProgressBar>
#include <QGroupBox>

#include "size.hpp"
#include "usbdiskselectiondialog.hpp" //old
#include "destructivedisktester.hpp"
#include "destructivedisktesterwrapper.hpp"
#include "performancegraph.hpp"

//class DiskTestWindow : public QDialog //do we need this?

class DiskTestWidget : public QWidget
{
    Q_OBJECT

signals:

    void
    backToMain();

    void
    testFinished(bool success);

public:

    static QFrame*
    newHLine();

    DiskTestWidget(QString dev_path = "", QWidget *parent = 0);

public slots:

    void
    back();

    void
    confirm();

    void
    showDevSelect();

    void
    showDevTestSelect(const QString &dev_path, const QString &dev_title = "");

    void
    showStartTest();

private slots:

    DestructiveDiskTester*
    createWorker(bool schedule_start = true);

    void
    startTest();

    void
    startTestPhase(int phase);

    void
    written(qint64 pos, double avg_speed = 0, double current_speed = 0);

    void
    verified(qint64 pos, double avg_speed = 0, double current_speed = 0);

    void
    writeFailed(qint64 pos);

    void
    readFailed(qint64 pos);

    void
    handleTestFinished(bool success);

private:

    QVBoxLayout*
    resetMainLayout();

    QWidget
    *wid_main;

    //QDialogButtonBox
    //*m_button_box;

    int
    m_step;

    QString
    m_dev_path;

    QString
    m_dev_title;

    //QScopedPointer<StorageDiskSelection::Device>
    //m_selected_device;

    qint64
    m_claimed_capacity;

    qint64
    m_wr_err;

    qint64
    m_rd_err;

    int
    m_sel_type;

    QPointer<UsbDiskSelectionWidget> //old
    m_disk_selection_widget;

    QPointer<DestructiveDiskTester>
    m_dd_worker;

    PerformanceGraph
    *m_graph_write;

    PerformanceGraph
    *m_graph_read;

    QProgressBar
    *m_progress;

    QLabel
    *m_lbl_time_elapsed;

    QLabel
    *m_lbl_time_estimate;

    QLabel
    *m_lbl_phase;

    int
    m_ddt_phase;

    QLabel
    *m_lbl_finish_desc;

    QGroupBox
    *m_grp_graph_write;

    QGroupBox
    *m_grp_graph_read;


};

#endif
