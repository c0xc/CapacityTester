/****************************************************************************
**
** Copyright (C) 2016 Philip Seeger
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

#ifndef CAPACITYTESTERGUI_HPP
#define CAPACITYTESTERGUI_HPP

#include <cassert>

#include <QApplication>
#include <QPixmap>
#include <QDesktopWidget>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QTimer>
#include <QMessageBox>
#include <QThread>
#include <QPainter>
#include <QBrush>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QMutexLocker>
#include <QMenu>
#include <QTableWidget>
#include <QHeaderView>
#include <QToolButton>
#include <QGroupBox>
#include <QScreen>
#include <QShortcut>

#include "size.hpp"
#include "volumetester.hpp"
#include "destructivedisktester.hpp"
#include "destructivedisktesterwrapper.hpp"
#include "selectionwindow.hpp"
#include "udiskformatdialog.hpp"
#include "usbdiskselectiondialog.hpp"
//#include "disktestwindow.hpp"
#include "usbstoragedevice.hpp"
#include "performancegraph.hpp"

#ifndef NO_UDISK
#include "udiskmanager.hpp"
#endif

class CapacityTesterGui : public QMainWindow
{
    Q_OBJECT

signals:

public:

    static QFrame*
    newHLine();

    CapacityTesterGui(QWidget *parent = 0);

public slots:

    void
    showFilesystemTest(const QString &mountpoint);

private slots:

    void
    nextStep();

    void
    prevStep();

    void
    showWelcome(bool first_call = false);

    void
    showDevSelect();

    void
    updateDevSelectTable();

    void
    setUsbFilter(bool checked);

    void
    checkDevSelection();

    void
    applyDev();

    void
    applyDev(const QString &path, bool quick = false);

    void
    showDevControls();

    void
    attemptMount(const QString &filesystem);

    void
    attemptUnmount(const QString &filesystem);

    void
    showDiskTestSelect();

    void
    showDiskTestStart();

    void
    startDiskTest();

    void
    stopDiskTest();

    void
    updateTime();

    void
    startFailed(const QString &error_msg = "");

    void
    startedTest(qint64 size);

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
    handleTestFinished(int result);

    void
    showFormatTool(bool format_now = false);

private:

    QVBoxLayout*
    resetMainLayout();

    DestructiveDiskTester*
    createDiskTestWorker(bool schedule_start = true);

    QWidget
    *wid_dev_sel;

    QWidget
    *wid_main;

    QDialogButtonBox
    *m_button_box;

    QPointer<QPushButton>
    m_btn_back;

    QPointer<QPushButton>
    m_btn_next;

    QPointer<QPushButton>
    m_btn_cancel;

    QAction
    *m_act_format;

    int
    m_step;

    bool
    m_auto;

    int
    m_test_type = 1;

    int
    m_dev_filter;

    QString
    m_dev_path;

    QString
    m_dev_title;

    Size
    m_claimed_capacity;

    QPointer<QTableWidget>
    m_dev_table;

    QPointer<QPushButton>
    m_btn_quick;

    QPointer<DestructiveDiskTester>
    m_dd_worker;

    qint64
    m_wr_err;

    qint64
    m_rd_err;

    qint64
    m_total;

    qint64
    m_wr, m_rd;

    //I'm using QPointer objects here because most of these widgets are created later on,
    //then deleted when the user goes back to the previous step and then recreated again.

    QPointer<QProgressBar>
    m_progress; //used as scope parent object (while the test progress widgets are shown)

    QPointer<PerformanceGraph>
    m_graph_write;

    QPointer<PerformanceGraph>
    m_graph_read;

    QPointer<QLabel>
    m_lbl_time_elapsed;

    QPointer<QLabel>
    m_lbl_time_estimate;

    QPointer<QTimer>
    m_tmr_time;

    QElapsedTimer
    m_tmr_elapsed;

    QElapsedTimer
    m_tmr_elapsed_current;

    double
    m_cur_speed;

    QPointer<QLabel>
    m_lbl_phase;

    int
    m_ddt_phase;

    QPointer<QGroupBox>
    m_grp_graph_write;

    QPointer<QGroupBox>
    m_grp_graph_read;

    QPointer<QGroupBox>
    m_grp_result;

    QPointer<QLabel>
    m_lbl_result;

    QPointer<QGroupBox>
    m_grp_format;



};

class VolumeTestGui : public QWidget
{
    Q_OBJECT

signals:

    void
    started();

    void
    finished();

    void
    remountExecuted(const QString &new_mountpoint);

    void
    reqStop();

public:

    VolumeTestGui(QWidget *parent = 0);

    VolumeTestGui(const QString &mountpoint, QWidget *parent = 0);

private:

    bool
    closing;

    QString
    selected_mountpoint;

    bool
    req_remount;

    bool
    disable_precheck;

    QLineEdit
    *txt_volume;

    QPushButton
    *btn_select_volume;

    QPushButton
    *btn_advanced;

    QAction
    *act_toggle_remount;

    QAction
    *act_disable_precheck;

    QAction
    *act_show_format_window;

    QAction
    *act_destructive_test;

    QPushButton
    *btn_start_volume_test;

    QPushButton
    *btn_stop_volume_test;

    QPushButton
    *btn_quit;

    QLineEdit
    *txt_capacity;

    QLineEdit
    *txt_used;

    QLineEdit
    *txt_available;

    QLabel
    *lbl_pro_testing;

    QLabel
    *lbl_pro_left_light;

    QLabel
    *lbl_pro_right_light;

    QTimer
    *tmr_pro_testing;

    QGraphicsOpacityEffect
    *eff_pro_left;

    QGraphicsOpacityEffect
    *eff_pro_right;

    QPropertyAnimation
    *ani_pro_left;

    QPropertyAnimation
    *ani_pro_right;

    Size
    total_test_size;

    QProgressBar
    *pro_initializing;

    QProgressBar
    *pro_writing;

    QProgressBar
    *pro_verifying;

    QLineEdit
    *txt_write_speed;

    QLineEdit
    *txt_read_speed;

    QElapsedTimer
    tmr_total_test_time;

    QLineEdit
    *txt_time;

    QPlainTextEdit
    *txt_result;

    QPixmap
    progressLightPixmap(const QColor &color);

    QPointer<VolumeTester>
    volume_worker;

public slots:

    void
    closeEvent(QCloseEvent *event);

    void
    showDriveWindow();

    void
    showFormatDialog(const QString &target);

    void
    showFormatDialog();

    void
    toggleReqRemount(bool checked);

    void
    toggleInitPrecheck(bool checked);

    void
    unloadVolume();

    void
    setCapacityFields(VolumeTester &tester);

    void
    loadVolume(const QString &mountpoint);

    void
    startVolumeTest();

    void
    stopTest();

private slots:

    void
    succeededVolumeTest();

    void
    failedVolumeTest(int error_type);

    void
    completedVolumeTest();

    void
    updateProgressLabel();

    void
    initializationStarted(qint64 total);

    void
    initialized(qint64 size, double avg_speed);

    void
    writeStarted();

    void
    verifyStarted();

    void
    createFailed(int index, qint64 start);

    void
    writeFailed(qint64 start);

    void
    verifyFailed(qint64 start);

    void
    written(qint64 written, double avg_speed);

    void
    verified(qint64 read, double avg_speed);

    void
    executeRemount(const QString &mountpoint);

};

#endif