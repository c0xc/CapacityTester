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
DiskTestWindow::newHLine()
{
    QFrame *hline = new QFrame;
    hline->setFrameShape(QFrame::HLine);
    return hline;
}

DiskTestWindow::DiskTestWindow(QWidget *parent)
              : QDialog(parent)
{
    QVBoxLayout *vbox = new QVBoxLayout;
    setLayout(vbox);

    //Top title
    //font: Prompt (free to use); included as resource
    //or VT323
    //not supported: font-variant-caps: small-caps;
    QHBoxLayout *hbox_top = new QHBoxLayout;
    vbox->addLayout(hbox_top);
    QLabel *lbl_title = new QLabel("Capacity Tester");
    lbl_title->setStyleSheet("font-weight:800; font-family:'Prompt', sans-serif; color:black; font-size:30pt; font-style:italic;");
    //"font-family:'Prompt', sans-serif; color:black; font-size:30pt; font-style:italic;"
    hbox_top->addWidget(lbl_title);
    QLabel *lbl_title_by = new QLabel("by c0xc");
    lbl_title_by->setStyleSheet("font-weight:italic; font-family:roboto; color:lightgray; font-size:8pt;");
    hbox_top->addWidget(lbl_title_by, 0, Qt::AlignBottom);
    QFrame *hline1 = new QFrame;
    hline1->setFrameShape(QFrame::HLine);
    vbox->addWidget(hline1);

    //Sub header with selection
    wid_dev_sel = new QWidget;
    vbox->addWidget(wid_dev_sel);

    //Content
    wid_main = new QWidget;
    vbox->addWidget(wid_main);

    QVBoxLayout *vbox_main = new QVBoxLayout;
    wid_main->setLayout(vbox_main);
    wid_main->setContentsMargins(QMargins()); //TODO doesn't help

    QHBoxLayout *hbox_main_warn = new QHBoxLayout;
    vbox_main->addLayout(hbox_main_warn);
    QFrame *vline1 = new QFrame;
    vline1->setFrameStyle(QFrame::VLine | QFrame::Plain);
    vline1->setLineWidth(5);
    QPalette pal_line_warn = vline1->palette();
    pal_line_warn.setColor(QPalette::WindowText, QColor("red"));
    vline1->setPalette(pal_line_warn);
    hbox_main_warn->addWidget(vline1);
    QLabel *lbl_main_warn = new QLabel(tr("A disk test writes to the storage device - not to a filesystem on it. Any filesystem on it will be overwritten, all data on the selected storage device will be lost. Only select a storage device that does not contain any valuable data. Use this test on your own risk. After the test is completed, you will have to format the disk again."));
    //lbl_main_warn->setStyleSheet("border-left:5px solid red");
        //tr("Do you want to run a disk test on the selected device?\n%1\nPlease note that this test routine will overwrite the device, destroying all data on it. After running this test, you will have to format it before you're able to use it again.\nIf you have any valuable files on this device, cancel NOW (press ESC).")
    lbl_main_warn->setWordWrap(true);
    hbox_main_warn->addWidget(lbl_main_warn);
    vbox->addStretch();

    //Control buttons
    m_button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(m_button_box, SIGNAL(accepted()), this, SLOT(confirm()));
    connect(m_button_box, SIGNAL(rejected()), this, SLOT(cancel()));
    vbox->addWidget(m_button_box);

    m_step = 0;
}

void
DiskTestWindow::cancel()
{
    //TODO send reqStop, maybe ask first, something like that
    close();
}

void
DiskTestWindow::confirm()
{
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
        showStartTest();
    }
    else if (m_step == 3)
    {
        startTest();
    }
    else if (m_step >= 10)
    {
        //if (parent())
        close();
    }
        //showTest();

}

void
DiskTestWindow::showDevSelect()
{
    foreach (QObject *obj, wid_dev_sel->children())
        obj->deleteLater();
    if (wid_dev_sel->layout()) delete wid_dev_sel->layout();

    QVBoxLayout *vbox = resetMainLayout();
    vbox->setSpacing(0);

    m_disk_selection_widget = new UsbDiskSelectionWidget(true, false, this);
    m_disk_selection_widget->setContentsMargins(QMargins()); //TODO doesn't help
    vbox->addWidget(m_disk_selection_widget.data());

    connect(m_disk_selection_widget.data(), SIGNAL(deviceSelected(const QString&, const QString&)),
        SLOT(showDev(const QString&, const QString&)));

    m_step = 1;
}

void
DiskTestWindow::showDev(const QString &dev_path, const QString &dev_title)
{
    //Check if selected device is in use (quick check)
    //note that this is not 100% reliable
    DestructiveDiskTester checker(dev_path);
    if (checker.isMounted())
    {
        QMessageBox::critical(this, tr("Cannot start test"),
            tr("This device is in use: %1. Please unmount it. The disk test requires that the device is not in use because it will overwrite it, destroying all data on it.").arg(dev_path));
        showDevSelect();
        return;
    }

    //Set selected device
    StorageDiskSelection::Device device = StorageDiskSelection().blockDevice(dev_path);
    m_dev_path = dev_path;
    m_dev_title = dev_title;
    m_claimed_capacity = device.capacity();
    m_wr_err = -1;
    m_rd_err = -1;
    Debug(QS("set storage device: %s (%s)", dev_path.toUtf8().constData(), Size(m_claimed_capacity).formatted().toUtf8().constData()));

    //Reset widget
    foreach (QObject *obj, wid_dev_sel->children())
        obj->deleteLater();
    if (wid_dev_sel->layout()) delete wid_dev_sel->layout();

    //Show selected device in sub header
    QVBoxLayout *vbox = new QVBoxLayout;
    wid_dev_sel->setLayout(vbox);
    QHBoxLayout *hbox = new QHBoxLayout;
    vbox->addLayout(hbox);
    QLabel *lbl_label1 = new QLabel(tr("Disk:"));
    lbl_label1->setStyleSheet("font-family:'Prompt', sans-serif; color:black;"); //or VT323
    hbox->addWidget(lbl_label1);
    QLineEdit *lbl_name = new QLineEdit(dev_title);
    lbl_name->setStyleSheet("font-weight:bold; font-family:'Prompt', sans-serif; color:black;"); //or VT323
    lbl_name->setFrame(false);
    lbl_name->setReadOnly(true);
    lbl_name->setCursorPosition(0); //otherwise beginning of text truncated
    lbl_name->setContextMenuPolicy(Qt::NoContextMenu);
    hbox->addWidget(lbl_name);
    vbox->addWidget(newHLine());

    //Show test type selection
    vbox = resetMainLayout();
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
DiskTestWindow::showStartTest()
{
    //Reset widget
    QVBoxLayout *vbox = resetMainLayout();

    //Ask for final confirmation
    QLabel *lbl_info = new QLabel(tr("Do you want to start the test now? All data on the selected storage device will be lost."));
    lbl_info->setWordWrap(true);
    vbox->addWidget(lbl_info);

    m_step = 3;
}

void
DiskTestWindow::startTest()
{
    //Reset widget
    QVBoxLayout *vbox = resetMainLayout();
    m_lbl_phase = new QLabel();
    QFont h_font = m_lbl_phase->font();
    h_font.setWeight(QFont::Bold);
    m_lbl_phase->setFont(h_font);
    vbox->addWidget(m_lbl_phase);
    m_lbl_finish_desc = new QLabel;
    m_lbl_finish_desc->setWordWrap(true);
    vbox->addWidget(m_lbl_finish_desc);

    //Start disk test worker
    //TODO ugly HACK using GUI
    //We want this worker to be shared, available to the old GUI as well,
    //so it can show the progress too and so this window can be closed.
    //This should be a temporary solution for now,
    //as we don't want to get rid of the old main window altogether.
    CapacityTesterGui *gui = qobject_cast<CapacityTesterGui*>(parent());
    if (!gui) return; //TODO hack
    QPointer<DestructiveDiskTester> dd_worker = gui->startDiskTest(m_dev_path);
    if (!dd_worker) return;
    m_dd_worker = dd_worker;
    m_wr_err = -1;
    m_rd_err = -1;
    m_ddt_phase = 0;
    connect(m_dd_worker.data(), SIGNAL(writeFailed(qint64)),
            this, SLOT(writeFailed(qint64)));
    connect(m_dd_worker.data(), SIGNAL(verifyFailed(qint64)),
            this, SLOT(readFailed(qint64)));

    //Configure worker
    m_graph_write = 0;
    m_graph_read = 0;
    if (m_sel_type == 1)
    {
    }
    else if (m_sel_type == 2)
    {
        //Set test option
        Debug(QS("setting mode..."));
        m_dd_worker->setFullMode(); //not executed in wrapper

        //Create performance graph
        m_graph_write = new PerformanceGraph("MB", "MB/s");
        //m_graph_write->setVisible(false);
        vbox->addWidget(m_graph_write);
        m_graph_read = new PerformanceGraph("MB", "MB/s");
        //m_graph_read->setVisible(false);
        vbox->addWidget(m_graph_read);

        //Show speed in graph
        //Display graph while writing/reading
        //Lambda callbacks may never be called if worker is wrapper!
        //connect(m_dd_worker.data(), &DestructiveDiskTester::written...
        connect(m_dd_worker.data(), SIGNAL(written(qint64, double)),
                m_graph_write, SLOT(addPoint(qint64, double)));
        connect(m_dd_worker.data(), SIGNAL(verified(qint64, double)),
                m_graph_read, SLOT(addPoint(qint64, double)));
    }

    //NOTE speed is not transmitted through wrapper

    connect(m_dd_worker.data(), SIGNAL(finished(bool)),
            this, SLOT(handleTestFinished(bool)));



    m_step = 10;
}

void
DiskTestWindow::startTestPhase(int phase)
{
    if (phase == 1)
    {
        m_lbl_phase->setText(tr("WRITING"));
        m_graph_write->setVisible(true);
    }
    else if (phase == 2)
    {
        m_graph_write->setVisible(false);
        m_lbl_phase->setText(tr("VERIFYING"));
        m_graph_read->setVisible(true);
    }
    else if (phase == 3 || phase == 4)
    {
        m_graph_write->setVisible(false);
        m_graph_read->setVisible(false);
        m_graph_write->setVisible(true);
        m_graph_read->setVisible(true);
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
DiskTestWindow::writeFailed(qint64 pos)
{
    m_wr_err = pos;
}

void
DiskTestWindow::readFailed(qint64 pos)
{
    m_rd_err = pos;
}

void
DiskTestWindow::handleTestFinished(bool success)
{
    double size_g = Size(m_claimed_capacity).gb();

    QPushButton *btn_cancel = m_button_box->button(QDialogButtonBox::Cancel);
    if (btn_cancel) btn_cancel->hide();

    if (success)
    {
        //No error found
        m_lbl_phase->setText(tr("DONE - NO ERRORS"));

        //Different summary text depending on test type
        if (m_sel_type == 1)
        {
            //Fast mode
            m_lbl_finish_desc->setText(tr("This storage device is genuine. This storage device has a capacity of %1 GB, all of which is usable.").arg(size_g, 0, 'f', 1));
        }
        else
        {
            //Full mode
            //may be used to test a bad drive which does not lie about its capacity
            m_lbl_finish_desc->setText(tr("This storage device has a capacity of %1 GB, all of which is usable.").arg(size_g, 0, 'f', 1));
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
DiskTestWindow::resetMainLayout()
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

