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

#include "disktestwindow.hpp"

QFrame*
DiskTestWidget::newHLine()
{
    QFrame *hline = new QFrame;
    hline->setFrameShape(QFrame::HLine);
    return hline;
}

//DiskTestWindow::DiskTestWindow(QWidget *parent)
//              : QDialog(parent)
//{
//    QVBoxLayout *vbox = new QVBoxLayout;
//    setLayout(vbox);
//
//    //Top title
//    //font: Prompt (free to use); included as resource
//    //or VT323
//    //not supported: font-variant-caps: small-caps;
//    QHBoxLayout *hbox_top = new QHBoxLayout;
//    vbox->addLayout(hbox_top);
//    QLabel *lbl_title = new QLabel("Capacity Tester");
//    lbl_title->setStyleSheet("font-weight:800; font-family:'Prompt', sans-serif; color:black; font-size:30pt; font-style:italic;");
//    //"font-family:'Prompt', sans-serif; color:black; font-size:30pt; font-style:italic;"
//    hbox_top->addWidget(lbl_title);
//    QLabel *lbl_title_by = new QLabel("by c0xc");
//    lbl_title_by->setStyleSheet("font-weight:italic; font-family:roboto; color:lightgray; font-size:8pt;");
//    hbox_top->addWidget(lbl_title_by, 0, Qt::AlignBottom);
//    QFrame *hline1 = new QFrame;
//    hline1->setFrameShape(QFrame::HLine);
//    vbox->addWidget(hline1);
//
//    //Sub header with selection
//    wid_dev_sel = new QWidget;
//    vbox->addWidget(wid_dev_sel);
//
//    //Content
//    wid_main = new QWidget;
//    vbox->addWidget(wid_main);
//
//    QVBoxLayout *vbox_main = new QVBoxLayout;
//    wid_main->setLayout(vbox_main);
//    wid_main->setContentsMargins(QMargins()); //TODO doesn't help
//
//    QHBoxLayout *hbox_main_warn = new QHBoxLayout;
//    vbox_main->addLayout(hbox_main_warn);
//    QFrame *vline1 = new QFrame;
//    vline1->setFrameStyle(QFrame::VLine | QFrame::Plain);
//    vline1->setLineWidth(5);
//    QPalette pal_line_warn = vline1->palette();
//    pal_line_warn.setColor(QPalette::WindowText, QColor("red"));
//    vline1->setPalette(pal_line_warn);
//    hbox_main_warn->addWidget(vline1);
//    QLabel *lbl_main_warn = new QLabel(tr("A disk test writes to the storage device - not to a filesystem on it. Any filesystem on it will be overwritten, all data on the selected storage device will be lost. Only select a storage device that does not contain any valuable data. Use this test on your own risk. After the test is completed, you will have to format the disk again."));
//    //lbl_main_warn->setStyleSheet("border-left:5px solid red");
//        //tr("Do you want to run a disk test on the selected device?\n%1\nPlease note that this test routine will overwrite the device, destroying all data on it. After running this test, you will have to format it before you're able to use it again.\nIf you have any valuable files on this device, cancel NOW (press ESC).")
//    lbl_main_warn->setWordWrap(true);
//    hbox_main_warn->addWidget(lbl_main_warn);
//    vbox->addStretch();
//
//    //Control buttons
//    m_button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
//    connect(m_button_box, SIGNAL(accepted()), this, SLOT(confirm()));
//    connect(m_button_box, SIGNAL(rejected()), this, SLOT(cancel()));
//    vbox->addWidget(m_button_box);
//
//    m_step = 0;
//}

DiskTestWidget::DiskTestWidget(QString dev_path, QWidget *parent)
              : QWidget(parent)
{
    QVBoxLayout *vbox = new QVBoxLayout;
    setLayout(vbox);

    //Content
    wid_main = new QWidget;
    vbox->addWidget(wid_main);
    vbox->addStretch();

    QVBoxLayout *vbox_main = new QVBoxLayout;
    wid_main->setLayout(vbox_main);
    wid_main->setContentsMargins(QMargins()); //doesn't help

    m_step = 0;
    if (!dev_path.isEmpty())
    {
        showDevTestSelect(dev_path);
    }
}

void
DiskTestWidget::back()
{
    //TODO send reqStop, maybe ask first, something like that

    if (m_step == 0)
    {
        emit backToMain();
    }
    else if (m_step == 1)
    {
        showDevSelect();
    }
    else if (m_step == 2)
    {
        showDevTestSelect(m_dev_path, m_dev_title);
    }
    else if (m_step == 3)
    {
        showDevTestSelect(m_dev_path, m_dev_title);
    }
}

void
DiskTestWidget::confirm()
{
    Debug(QS("DT step: %d", m_step)); //TODO
    if (m_step == 0)
    {
        showDevSelect();
    }
    else if (m_step == 1)
    {
        m_disk_selection_widget->accept();
    }
    else if (m_step == 2)
    {
        //Test selection -> show start confirmation
        showStartTest();
    }
    else if (m_step == 3)
    {
        //Start confirmation -> start test
        startTest();
    }
    else if (m_step >= 10 && m_step < 20)
    {
        //Test in progress
    }
    else if (m_step >= 20)
    {
        bool ok = true;
        emit testFinished(ok);
    }

}

void
DiskTestWidget::showDevSelect()
{
    QVBoxLayout *vbox = resetMainLayout();
    vbox->setSpacing(0);

    m_disk_selection_widget = new UsbDiskSelectionWidget(true, false, this);
    m_disk_selection_widget->setContentsMargins(QMargins()); //doesn't help
    vbox->addWidget(m_disk_selection_widget.data());

    connect(m_disk_selection_widget.data(), SIGNAL(deviceSelected(const QString&, const QString&)),
        SLOT(showDevTestSelect(const QString&, const QString&)));

    m_step = 1;
}

void
DiskTestWidget::showDevTestSelect(const QString &dev_path, const QString &dev_title)
{
    //Check if selected device is in use (quick check)
    //note that this is not 100% reliable
    //DestructiveDiskTester checker(dev_path);
    //if (checker.isMounted())
    //{
    //    QMessageBox::critical(this, tr("Cannot start test"),
    //        tr("This device is in use: %1. Please unmount it. The disk test requires that the device is not in use because it will overwrite it, destroying all data on it.").arg(dev_path));
    //    showDevSelect();
    //    return;
    //}

    //Set selected device
    StorageDiskSelection::Device device = StorageDiskSelection().blockDevice(dev_path);
    m_dev_path = dev_path;
    m_dev_title = !dev_title.isEmpty() ? dev_title : device.displayModelName();
    m_claimed_capacity = device.capacity();
    m_wr_err = -1;
    m_rd_err = -1;
    Debug(QS("set storage device: %s (%s)", dev_path.toUtf8().constData(), Size(m_claimed_capacity).formatted().toUtf8().constData()));

    //Show test type selection
    QVBoxLayout *vbox = resetMainLayout();
    QLabel *lbl_top_info = new QLabel(tr("Which type of test do you want to run? Click OK or hit ENTER to use the recommended one."));
    lbl_top_info->setWordWrap(true);
    vbox->addWidget(lbl_top_info);
    vbox->addWidget(newHLine());
    //---
    //: Fast, as in quick - takes only ~minutes.
    QRadioButton *btn_fast_test = new QRadioButton(tr("Quick disk test"));
    btn_fast_test->setChecked(true);
    btn_fast_test->setStyleSheet("font-weight:bold; font-family:'Prompt', sans-serif; font-size:14pt;");
    vbox->addWidget(btn_fast_test);
    QLabel *lbl_fast_test_desc = new QLabel(tr("Writes to the disk with certain intervals to quickly determine if the device has less capacity than it claims to have. This type of test works for all common fakes and is very fast. It will not test every byte. Recommended."));
    lbl_fast_test_desc->setWordWrap(true);
    vbox->addWidget(lbl_fast_test_desc);
    QRadioButton *btn_full_test = new QRadioButton(tr("Full disk test"));
    btn_full_test->setStyleSheet("font-weight:bold; font-family:'Prompt', sans-serif; font-size:14pt;");
    vbox->addWidget(btn_full_test);
    QLabel *lbl_full_test_desc = new QLabel(tr("Fills the whole disk, testing every block. This will take a long time. Use this type of test if you suspect that parts of your disk are faulty."));
    lbl_full_test_desc->setWordWrap(true);
    vbox->addWidget(lbl_full_test_desc);
    //---
    vbox->addWidget(newHLine());

    //Select action
    QTimer::singleShot(0, btn_fast_test, SLOT(setFocus()));
    m_sel_type = 1;
    connect(btn_fast_test, &QAbstractButton::toggled, this, [=](bool checked)
    {
        Q_UNUSED(checked);
        m_sel_type = 1;
    });
    connect(btn_full_test, &QAbstractButton::toggled, this, [=](bool checked)
    {
        Q_UNUSED(checked);
        m_sel_type = 2;
    });

    m_step = 2;
}

void
DiskTestWidget::showStartTest()
{
    //Reset widget
    QVBoxLayout *vbox = resetMainLayout();

    //TEST
    QLabel *lbl_test = new QLabel(tr("Disk test for: %1").arg(m_dev_title));
    vbox->addWidget(lbl_test);
    //add label after delay
    QTimer::singleShot(0, this, [=] () //TODO TEST
    {
    QLabel *lbl_test2 = new QLabel(tr("TEST2 "));
    vbox->addWidget(lbl_test2);
    }); //TODO
    QTimer::singleShot(5000, this, [=] () //TODO TEST
    {
        Debug(QS("showing test3...")); //TODO
        showDevSelect();
    }); //TODO


    //Get storage device
    StorageDiskSelection::Device device = StorageDiskSelection().blockDevice(m_dev_path);

    Debug(QS("show start test: %s (%s)", m_dev_path.toUtf8().constData(), Size(m_claimed_capacity).formatted().toUtf8().constData())); //TODO
    //Ask for final confirmation
    QLabel *lbl_info = new QLabel(tr("Do you want to start the test now?"));
    QFont h_font = lbl_info->font();
    h_font.setWeight(QFont::Bold);
    lbl_info->setFont(h_font);
    lbl_info->setWordWrap(true);
    vbox->addWidget(lbl_info);
    //add italic info label
    QLabel *lbl_info_italic = new QLabel(tr("This test will overwrite the device, destroying all data on it."));
    lbl_info_italic->setStyleSheet("color:darkred;");
    lbl_info_italic->setStyleSheet("font-style:italic;");
    lbl_info_italic->setWordWrap(true);
    vbox->addWidget(lbl_info_italic);

    //List mounted filesystems, if any (they will be unmounted)
    QStringList mountpoints;
    device.mountpoint(&mountpoints);
    if (!mountpoints.isEmpty())
    {
        QLabel *lbl_mounted_fs = new QLabel(tr("The following filesystems are mounted on this device: %1. They will be unmounted before the test starts.").arg(mountpoints.join(", ")));
        lbl_mounted_fs->setWordWrap(true);
        vbox->addWidget(lbl_mounted_fs);
    }

    m_step = 3;
}

DestructiveDiskTester*
DiskTestWidget::createWorker(bool schedule_start)
{

    //TODO auto_unmount_init flag is on by default, otherwise we might check if in use

    //Initialize worker
    DestructiveDiskTester *dd_worker = new DestructiveDiskTester(m_dev_path);
    //dd_worker->setFullMode() see startTest()

    //Check if selected device is valid, exists
    if (!dd_worker->exists())
    {
        QMessageBox::critical(this, tr("Cannot start test"),
            //: Invalid device selected, device not found.
            tr("This device is not valid: %1.").arg(m_dev_path));
        dd_worker->deleteLater();
        return 0;
    }

    //Use sudo wrapper if not running as root
    //This is for convenience, so the user doesn't have to restart the program as root
    //But it's not 100% reliable, because the user may not have sudo permissions
    if (!dd_worker->isWritable())
    {
#if !defined(Q_OS_WIN)
        QMessageBox::warning(this, tr("Disk test"),
            //: This hint is to inform the user that a password prompt will be triggered, usually asking for the sudo password (i.e., the user's password if s/he has sufficient sudo permissions to run this program as root). sudo does not have to be translated, it's a detail.
            tr("This program is running with limited privileges. An attempt will now be made to gain root privileges for this test, you may be asked for your sudo password."));
        //TODO check for success! else warn user
        dd_worker->deleteLater();
        dd_worker = new DestructiveDiskTesterWrapper(m_dev_path);
#elif defined(Q_OS_WIN)
        QMessageBox::critical(this, tr("Disk test"),
            //: This warning is shown on Windows, so the term administrator is used. Alternative: Please start this program with elevated privileges.
            tr("This program is running with limited privileges. Try restarting the program as administrator."));
        dd_worker->deleteLater();
        return 0;
#endif
    }

    //Thread for worker
    //... after the check above, otherwise dd_worker (+ GUI) would get stuck.
    QThread *thread = new QThread;
    dd_worker->moveToThread(thread);

    //Start worker when thread starts
    connect(thread,
            SIGNAL(started()),
            dd_worker,
            SLOT(start()));

    //Initialization started
    //connect(dd_worker,
    //        SIGNAL(started(qint64)),
    //        this,
    //        SLOT(startedDiskTest(qint64)));

    //Stop thread when worker done (stops event loop -> thread->finished())
    connect(dd_worker,
            SIGNAL(finished()),
            thread,
            SLOT(quit()));

    //Delete worker when done
    //This signal can be connected to QObject::deleteLater(), to free objects in that thread.
    //https://doc.qt.io/qt-5/qthread.html#finished
    connect(thread,
            SIGNAL(finished()),
            dd_worker,
            SLOT(deleteLater())); //delete worker after loop

    //Delete thread when thread done (event loop stopped)
    connect(dd_worker,
            SIGNAL(destroyed()),
            thread,
            SLOT(deleteLater())); //delete thread after worker

    //Stop worker on request
    connect(this,
            SIGNAL(reqStop()),
            dd_worker,
            SLOT(cancel()));

    //Delete on quit (GUI) //TODO ?
    connect(this,
            SIGNAL(destroyed()),
            dd_worker,
            SLOT(deleteLater())); //delete worker on quit

    //Start test in background
    if (schedule_start)
        QTimer::singleShot(0, thread, SLOT(start()));

    return dd_worker;
}

void
DiskTestWidget::startTest()
{
    //Create all widgets we need to show the test progress
    //but hide some while we prepare the device etc.

    //Reset widget
    QVBoxLayout *vbox = resetMainLayout();

    //Frame with overall test progress
    QFrame *frame_test = new QFrame;
    vbox->addWidget(frame_test);
    QVBoxLayout *vbox_test = new QVBoxLayout;
    frame_test->setLayout(vbox_test);
    QGridLayout *grid_test_frame = new QGridLayout;
    vbox_test->addLayout(grid_test_frame);
    QLabel *lbl_test_title = new QLabel(tr("Test Progress"));
    QFont h_font = lbl_test_title->font();
    h_font.setWeight(QFont::Bold);
    lbl_test_title->setFont(h_font);
    grid_test_frame->addWidget(lbl_test_title, 0, 0);
    m_lbl_phase = new QLabel();
    h_font = m_lbl_phase->font();
    h_font.setWeight(QFont::Bold);
    m_lbl_phase->setFont(h_font);
    grid_test_frame->addWidget(m_lbl_phase, 1, 0);
    QLabel *lbl_test_time = new QLabel(tr("Time elapsed:"));
    grid_test_frame->addWidget(lbl_test_time, 2, 0);
    m_lbl_time_elapsed = new QLabel;
    grid_test_frame->addWidget(m_lbl_time_elapsed, 2, 1);
    QLabel *lbl_test_estimate = new QLabel(tr("Estimated time remaining:"));
    grid_test_frame->addWidget(lbl_test_estimate, 3, 0);
    m_lbl_time_estimate = new QLabel;
    grid_test_frame->addWidget(m_lbl_time_estimate, 3, 1);
    m_progress = new QProgressBar;
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    grid_test_frame->addWidget(m_progress, 4, 0, 1, 2);
    m_lbl_phase->setText(tr("TEST IN PROGRESS"));

    //Prepare frame with write speed graph
    m_grp_graph_write = new QGroupBox(tr("Write Speed"));
    vbox->addWidget(m_grp_graph_write);
    QVBoxLayout *vbox_graph_write = new QVBoxLayout;
    m_grp_graph_write->setLayout(vbox_graph_write);
    m_graph_write = new PerformanceGraph("MB", "MB/s");
    vbox_graph_write->addWidget(m_graph_write);

    //Prepare frame with read speed graph
    m_grp_graph_read = new QGroupBox(tr("Read Speed"));
    vbox->addWidget(m_grp_graph_read);
    QVBoxLayout *vbox_graph_read = new QVBoxLayout;
    m_grp_graph_read->setLayout(vbox_graph_read);
    m_graph_read = new PerformanceGraph("MB", "MB/s");
    vbox_graph_read->addWidget(m_graph_read);

    //Hide both graphs for now until we start the test
    m_grp_graph_write->setVisible(false);
    m_grp_graph_read->setVisible(false);

    //Initialize worker, not started immediately
    m_dd_worker = createWorker();
    if (!m_dd_worker)
    {
        //Error
        //m_step = 0;
        return;
    }
    m_wr_err = -1;
    m_rd_err = -1;
    m_ddt_phase = 0;
    connect(m_dd_worker.data(), SIGNAL(writeFailed(qint64)),
            this, SLOT(writeFailed(qint64)));
    connect(m_dd_worker.data(), SIGNAL(verifyFailed(qint64)),
            this, SLOT(readFailed(qint64)));
    //Show speed in graph
    //Display graph while writing/reading
    //Lambda callbacks may never be called if worker is wrapper!
    //connect(m_dd_worker.data(), &DestructiveDiskTester::written...
    connect(m_dd_worker.data(), SIGNAL(written(qint64, double)),
            this, SLOT(written(qint64, double)));
    connect(m_dd_worker.data(), SIGNAL(verified(qint64, double)),
            this, SLOT(verified(qint64, double)));



    //Configure worker
    if (m_sel_type == 1)
    {
        //Quick mode, no performance graph
    }
    else if (m_sel_type == 2)
    {
        //Set test option
        Debug(QS("setting full mode..."));
        m_dd_worker->setFullMode(); //not executed in wrapper

    }
    //NOTE speed is not transmitted through wrapper

    //Connect finished signal
    connect(m_dd_worker.data(), SIGNAL(finished(bool)),
            this, SLOT(handleTestFinished(bool)));



    m_step = 10;
}

void
DiskTestWidget::startTestPhase(int phase)
{
    if (phase == 1)
    {
        m_lbl_phase->setText(tr("WRITING"));
        m_grp_graph_write->setVisible(true);
    }
    else if (phase == 2)
    {
        m_grp_graph_write->setVisible(false);
        m_lbl_phase->setText(tr("VERIFYING"));
        m_grp_graph_read->setVisible(true);
    }
    else if (phase == 3 || phase == 4)
    {
        if (phase == 3)
        {
            m_lbl_phase->setText(tr("TEST SUCCEEDED"));
        }
        else if (phase == 4)
        {
            m_lbl_phase->setText(tr("TEST FAILED"));
        }
    }

    m_ddt_phase = phase;
}

void
DiskTestWidget::written(qint64 pos, double avg_speed, double current_speed)
{
    if (m_ddt_phase == 0)
    {
        startTestPhase(1);
    }
    m_graph_write->addPoint(pos, current_speed);
}

void
DiskTestWidget::verified(qint64 pos, double avg_speed, double current_speed)
{
    if (m_ddt_phase == 1)
    {
        startTestPhase(2);
    }
    m_graph_read->addPoint(pos, current_speed);
}

void
DiskTestWidget::writeFailed(qint64 pos)
{
    m_wr_err = pos;
}

void
DiskTestWidget::readFailed(qint64 pos)
{
    m_rd_err = pos;
}

void
DiskTestWidget::handleTestFinished(bool success)
{
    double size_g = Size(m_claimed_capacity).gb();

    //TODO hide cancel button
    //QPushButton *btn_cancel = m_button_box->button(QDialogButtonBox::Cancel);
    //if (btn_cancel) btn_cancel->hide();

    if (success)
    {
        //No error found
        m_lbl_phase->setText(tr("DONE - NO ERRORS"));

        //Different summary text depending on test type
        if (m_sel_type == 1)
        {
            //Fast mode
            m_lbl_finish_desc->setText(tr("This storage device appears to be genuine. This storage device has a capacity of %1 GB, all of which is usable.").arg(size_g, 0, 'f', 1));
        }
        else
        {
            //Full mode
            //may be used to test a bad drive which does not lie about its capacity
            m_lbl_finish_desc->setText(tr("This storage device appears to be genuine. This storage device has a capacity of %1 GB, all of which is usable.").arg(size_g, 0, 'f', 1));
        }
    }
    else
    {
        //Bad disk detected
        m_lbl_phase->setText(tr("FAKE DETECTED"));
        qint64 err_at_m = m_wr_err > -1 ? m_wr_err : m_rd_err; //> -1 M (pos)
        qint64 err_at_g = 0;
        if (err_at_m > -1) //if -1, failed signal with position didn't work
        {
            err_at_g = err_at_m / 1024; //std::div(m, 1024).quot
        }

        //Different summary text depending on test type
        //because fast mode detects fake capacity
        //and full mode detects any damage(d region)
        if (m_sel_type == 1)
        {
            //Fast mode
            m_lbl_finish_desc->setText(tr("This storage device claims to have a capacity of %1 GB, but only the first %2 GB are usable.").arg(size_g, 0, 'f', 1).arg(err_at_g));
        }
        else
        {
            //Full mode
            m_lbl_finish_desc->setText(tr("This storage device returned corrupted data after %1 GB.").arg(err_at_g));
        }

    }

    m_step = 19;
}

QVBoxLayout*
DiskTestWidget::resetMainLayout()
{
    //Reset widget
    foreach (QObject *obj, wid_main->children())
    {
        QWidget *wid = qobject_cast<QWidget*>(obj);
        if (wid) wid->hide(); //because delete will be delayed
        obj->deleteLater();
    }
    if (wid_main->layout()) delete wid_main->layout();
    QVBoxLayout *vbox = new QVBoxLayout;
    wid_main->setLayout(vbox);

    return vbox;
}

