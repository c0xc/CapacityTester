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

#include "capacitytestergui.hpp"

CapacityTesterGui::CapacityTesterGui(QWidget *parent)
                 : QMainWindow(parent),
                   closing(false),
                   req_remount(false),
                   disable_precheck(false)
{
    //Layout items
    QVBoxLayout *vbox_main = new QVBoxLayout;
    QFrame *hline = 0;
    QWidget *main_widget = new QWidget;
    main_widget->setLayout(vbox_main);
    setCentralWidget(main_widget);

    //Font, icon
    QFont monospace_font =
        QFontDatabase::systemFont(QFontDatabase::FixedFont);

    //Volume text box, selection button
    QHBoxLayout *hbox_volume = new QHBoxLayout;
    vbox_main->addLayout(hbox_volume);
    //: Volume refers to a (mounted) filesystem throughout this application,
    //: so the word volume may be translated as filesystem.
    QLabel *lbl_volume = new QLabel(tr("Volume"));
    hbox_volume->addWidget(lbl_volume);
    txt_volume = new QLineEdit;
    txt_volume->setReadOnly(true);
    txt_volume->setFont(monospace_font);
    //: Volume, filesystem.
    txt_volume->setPlaceholderText(tr("Volume"));
    txt_volume->setToolTip(tr("Mounted volume to be tested"));
    txt_volume->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    hbox_volume->addWidget(txt_volume);
    //: Button action: Select volume/filesystem on a USB flash drive,
    //: the dialog lists drives but the user needs to select a volume,
    //: a mounted filesystem, so the word filesystem can be used here as well.
    btn_select_volume = new QPushButton(tr("Select &drive..."));
    hbox_volume->addWidget(btn_select_volume);
    connect(btn_select_volume,
            SIGNAL(clicked()),
            SLOT(showDriveWindow()));

    //Horizontal line
    hline = new QFrame;
    hline->setFrameShape(QFrame::HLine);
    vbox_main->addWidget(hline);
    hline = 0;

    //Volume form layout
    QFormLayout *volume_form = new QFormLayout;
    vbox_main->addLayout(volume_form);

    //Capacity
    txt_capacity = new QLineEdit;
    txt_capacity->setReadOnly(true);
    txt_capacity->setFont(monospace_font);
    volume_form->addRow(tr("Capacity"), txt_capacity);

    //Used space
    txt_used = new QLineEdit;
    txt_used->setReadOnly(true);
    txt_used->setFont(monospace_font);
    //: Label: Used storage (of capacity).
    volume_form->addRow(tr("Used"), txt_used);

    //Available space
    txt_available = new QLineEdit;
    txt_available->setReadOnly(true);
    txt_available->setFont(monospace_font);
    //: Label: Unused storage (of capacity).
    volume_form->addRow(tr("Available"), txt_available);

    //Horizontal line
    hline = new QFrame;
    hline->setFrameShape(QFrame::HLine);
    volume_form->addRow(hline);
    hline = 0;

    //Progress label ("...testing...")
    lbl_pro_testing = new QLabel;
    lbl_pro_testing->setFont(monospace_font);
    lbl_pro_testing->setSizePolicy(
        QSizePolicy::Expanding, QSizePolicy::Preferred);
    lbl_pro_testing->setProperty("DOTS", 0); //"..." == 3
    lbl_pro_testing->setAlignment(Qt::AlignHCenter);
    {
        QFont font = lbl_pro_testing->font();
        font.setPointSize(26);
        font.setBold(true);
        lbl_pro_testing->setFont(font);
    }
    lbl_pro_left_light = new QLabel;
    lbl_pro_left_light->setPixmap(progressLightPixmap("yellow"));
    lbl_pro_right_light = new QLabel;
    lbl_pro_left_light->setPixmap(progressLightPixmap("red"));
    QHBoxLayout *hbox_progress = new QHBoxLayout;
    hbox_progress->addWidget(lbl_pro_left_light);
    hbox_progress->addWidget(lbl_pro_testing);
    hbox_progress->addWidget(lbl_pro_right_light);
    volume_form->addRow(hbox_progress);

    //Progress timer
    tmr_pro_testing = new QTimer(this);
    tmr_pro_testing->setInterval(1000);
    connect(tmr_pro_testing,
            SIGNAL(timeout()),
            SLOT(updateProgressLabel()));

    //Blinking lights effect
    eff_pro_left = new QGraphicsOpacityEffect;
    lbl_pro_left_light->setGraphicsEffect(eff_pro_left);
    eff_pro_left->setOpacity(0);
    ani_pro_left = new QPropertyAnimation(eff_pro_left, "opacity");
    ani_pro_left->setDuration(tmr_pro_testing->interval() / 2);
    eff_pro_right = new QGraphicsOpacityEffect;
    lbl_pro_right_light->setGraphicsEffect(eff_pro_right);
    eff_pro_right->setOpacity(0);
    ani_pro_right = new QPropertyAnimation(eff_pro_right, "opacity");
    ani_pro_right->setDuration(tmr_pro_testing->interval() / 2);

    //Progress
    pro_initializing = new QProgressBar;
    //: Label for status of this phase - the init phase of the test.
    volume_form->addRow(tr("Initializing"), pro_initializing);
    pro_writing = new QProgressBar;
    //: Label for status of this test phase.
    volume_form->addRow(tr("Writing"), pro_writing);
    pro_verifying = new QProgressBar;
    //: Label for status of this test phase.
    volume_form->addRow(tr("Verifying"), pro_verifying);

    //Write speed
    txt_write_speed = new QLineEdit;
    txt_write_speed->setReadOnly(true);
    txt_write_speed->setFont(monospace_font);
    volume_form->addRow(tr("Write speed"), txt_write_speed);

    //Read speed
    txt_read_speed = new QLineEdit;
    txt_read_speed->setReadOnly(true);
    txt_read_speed->setFont(monospace_font);
    volume_form->addRow(tr("Read speed"), txt_read_speed);

    //Horizontal line
    hline = new QFrame;
    hline->setFrameShape(QFrame::HLine);
    volume_form->addRow(hline);
    hline = 0;

    //Time
    txt_time = new QLineEdit;
    txt_time->setReadOnly(true);
    txt_time->setFont(monospace_font);
    volume_form->addRow(tr("Time"), txt_time);

    //Result
    txt_result = new QPlainTextEdit;
    txt_result->setReadOnly(true);
    txt_result->setFont(monospace_font);
    volume_form->addRow(tr("Result"), txt_result);

    //Space
    vbox_main->addStretch(1);

    //Horizontal line
    hline = new QFrame;
    hline->setFrameShape(QFrame::HLine);
    vbox_main->addWidget(hline);
    hline = 0;

    //Buttons
    btn_start_volume_test = new QPushButton(tr("Start &Test")); //start VOLUME test
    connect(btn_start_volume_test,
            SIGNAL(clicked()),
            SLOT(startVolumeTest()));
    btn_stop_volume_test = new QPushButton(tr("&Stop Test"));
    connect(btn_stop_volume_test,
            SIGNAL(clicked()),
            SLOT(stopTest()));
    btn_quit = new QPushButton(tr("&Quit"));
    connect(btn_quit,
            SIGNAL(clicked()),
            SLOT(close()));
    //: Menu button (hence ...): Advanced functions, more functions...
    btn_advanced = new QPushButton(tr("&Advanced...")); //advanced functions...
    QMenu *mnu_advanced = new QMenu;
    btn_advanced->setMenu(mnu_advanced);
    //: On/off switch: Pre-check means quick test before the real test,
    //: to detect a fake earlier, can be translated as superficial check.
    act_disable_precheck = mnu_advanced->addAction(tr("Run pre-check during initialization"));
    act_disable_precheck->setCheckable(true);
    act_disable_precheck->setChecked(true);
    connect(act_disable_precheck,
            SIGNAL(toggled(bool)),
            SLOT(toggleInitPrecheck(bool)));
    //: On/off switch: Try to remount between write and read phases.
    //: (This only affects the volume test, not the disk test.)
    act_toggle_remount = mnu_advanced->addAction(tr("Unmount and remount during test"));
    act_toggle_remount->setCheckable(true);
    connect(act_toggle_remount,
            SIGNAL(toggled(bool)),
            SLOT(toggleReqRemount(bool)));
    mnu_advanced->addSeparator();
    //: Button: Format dialog where user can reformat his USB drive.
    act_show_format_window = mnu_advanced->addAction(tr("Format drive"));
    connect(act_show_format_window,
            SIGNAL(triggered()),
            SLOT(showFormatDialog()));
    //: Button: Show list of USB drives to run disk test on selected drive.
    //: The word destructive could probably be left out in the translation.
    //: Disk test means the whole flash disk is tested, not just the volume,
    //: and destructive means it'll overwrite it, destroying all data on it.
    //: Another relevant property of this disk test is that it's very fast,
    //: so in doubt, it could be translated as Fast disk test.
    act_destructive_test = mnu_advanced->addAction(tr("Destructive disk test"));
    connect(act_destructive_test,
            SIGNAL(triggered()),
            SLOT(startDiskTest()));
    //Disable format feature on unsupported platforms
    bool udisk_support = false;
#ifdef UDISKMANAGER_HPP
    udisk_support = UDiskManager().isValid();
#endif
    if (!udisk_support)
    {
        act_toggle_remount->setEnabled(false);
        act_show_format_window->setEnabled(false);
    }

    //Initiate/unload view
    QTimer::singleShot(0, this, SLOT(unloadVolume()));

    //Button row
    QHBoxLayout *hbox_buttons = new QHBoxLayout;
    vbox_main->addLayout(hbox_buttons);
    hbox_buttons->addWidget(btn_start_volume_test);
    hbox_buttons->addWidget(btn_stop_volume_test);
    hbox_buttons->addWidget(btn_advanced);
    hbox_buttons->addWidget(btn_quit);

    //Window position
    setGeometry(
        QStyle::alignedRect(
            Qt::LeftToRight,
            Qt::AlignCenter,
            this->size(),
            QApplication::desktop()->availableGeometry())
    );

    //Set focus on select button
    QTimer::singleShot(0, btn_select_volume, SLOT(setFocus()));

}

QPixmap
CapacityTesterGui::progressLightPixmap(const QColor &color)
{
    QSize progress_light_size(50, 50);
    QPixmap progress_pixmap(progress_light_size);

    QColor bg(255, 255, 255, 0);
    progress_pixmap.fill(bg);
    QPainter painter;
    painter.begin(&progress_pixmap);
    int w = progress_light_size.width();
    int h = progress_light_size.height();
    int d = 10; //distance
    QPointF topleftd(d, d);
    QPointF bottomrightd(w - 1 - d, h - 1 - d);
    QRectF rect(topleftd, bottomrightd);
    QRadialGradient gradient(w / 2, h / 2, h / 2, w / 2, h / 2);
    gradient.setColorAt(0, color);
    gradient.setColorAt(1, bg);
    QBrush brush(gradient);
    painter.setBrush(brush);
    QPen pen(brush, 5, Qt::SolidLine, Qt::RoundCap, Qt::BevelJoin);
    painter.setPen(pen);
    painter.drawEllipse(rect);

    return progress_pixmap;
}

void
CapacityTesterGui::closeEvent(QCloseEvent *event)
{
    //Don't close while test running
    if (volume_worker || dd_worker)
    {
        //Don't close immediately
        event->ignore();

        //Check if already closing
        if (closing)
            return;

        //Ask user what to do
        if (QMessageBox::question(this,
            tr("Abort test?"),
            tr("Do you want to abort the running test?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No) != QMessageBox::Yes)
        {
            //Continue, don't close
            return;
        }

        //Set closing flag
        closing = true;

        //Abort test (if still running) and close GUI after test
        if (volume_worker)
        {
            connect(volume_worker, SIGNAL(destroyed()), SLOT(close()));
        }
        else if (dd_worker)
        {
            connect(dd_worker, SIGNAL(destroyed()), SLOT(close()));
        }
        setEnabled(false);
        stopTest();

    }
    else
    {
        event->accept();
    }
}

void
CapacityTesterGui::showDriveWindow()
{
    SelectionWindow *window = new SelectionWindow(this);
    window->setModal(true);
    window->show();
    connect(window, SIGNAL(volumeSelected(const QString&)), SLOT(loadVolume(const QString&)));

}

void
CapacityTesterGui::showFormatDialog(const QString &target)
{
#ifdef UDISKMANAGER_HPP
    //Show format dialog for target which usually is a (block) device
    //It could also be a mountpoint in which case the device will be determined
    UDiskFormatDialog *format_dialog = new UDiskFormatDialog(target, this);
    format_dialog->setAttribute(Qt::WA_DeleteOnClose);
    format_dialog->open();
#endif
}

void
CapacityTesterGui::showFormatDialog()
{
#ifdef UDISKMANAGER_HPP
    //If mountpoint selected, use it as target and clear selection
    //because after formatting it will not be a valid mountpoint anymore
    if (!selected_mountpoint.isEmpty())
    {
        QString mp = selected_mountpoint;
        unloadVolume();
        showFormatDialog(mp);
        return;
    }

    //Show device selection dialog
    UsbDiskSelectionDialog *selection_dialog = new UsbDiskSelectionDialog(this);
    selection_dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(selection_dialog, SIGNAL(deviceSelected(const QString&)), this, SLOT(showFormatDialog(const QString&)));
    selection_dialog->open();
#else
#endif
}

void
CapacityTesterGui::toggleReqRemount(bool checked)
{
    if (!checked)
    {
        req_remount = false;
    }
    else
    {
        QMessageBox::information(this, tr("Information"),
            tr("You have requested the volume to be remounted during the test."
            " Normally, this should not be necessary."
            " You might be asked for a password."
        ));
        req_remount = true;
    }
}

void
CapacityTesterGui::toggleInitPrecheck(bool checked)
{
    disable_precheck = !checked;
}

void
CapacityTesterGui::startDiskTest(const QString &device)
{
    if (device.isEmpty()) return;

    //DESTRUCTIVE DISK TEST - unmounted disk will be overwritten

    //: Destructive / fast disk test, see hint above.
    if (QMessageBox::question(this, tr("Destructive disk test"),
        tr("Do you want to run a destructive test on the selected device?\n%1\nPlease note that this test routine will overwrite the device, destroying all data on it. After running this test, you will have to format it before you're able to use it again.\nIf you have any valuable files on this device, cancel NOW (press ESC).").arg(device),
        QMessageBox::Ok | QMessageBox::Cancel) != QMessageBox::Ok)
        return;

    //Check if selected device is in use (quick check)
    //note that this is not 100% reliable
    DestructiveDiskTester checker(device);
    if (checker.isMounted())
    {
        QMessageBox::critical(this, tr("Cannot start test"),
            tr("This device is in use: %1. Please unmount it. The destructive disk test requires that the device is not in use because it will overwrite it, destroying all data on it.").arg(device));
        return;
    }
    //Check if selected device is valid, exists
    if (!checker.exists())
    {
        QMessageBox::critical(this, tr("Cannot start test"),
            //: Invalid device selected, device not found.
            tr("This device is not valid: %1.").arg(device));
        return;
    }

    //Sanity checks above
    //---
    //Test starts here

    //Disable volume selection and other stuff
    btn_select_volume->setEnabled(false);
    btn_advanced->setEnabled(false);

    //Start/stop buttons
    btn_start_volume_test->setEnabled(false);
    btn_stop_volume_test->setEnabled(true);

    //Reset fields
    txt_write_speed->clear();
    txt_read_speed->clear();
    txt_time->clear();
    txt_result->clear();
    txt_result->setStyleSheet(" ");

    //Initialize worker
    dd_worker = new DestructiveDiskTester(device);

    //Use sudo wrapper if not running as root
    if (!dd_worker->isWritable())
    {
#if !defined(Q_OS_WIN)
        QMessageBox::warning(this, tr("Disk test"),
            //: This hint is to inform the user that a password prompt will be triggered, usually asking for the sudo password (i.e., the user's password if s/he has sufficient sudo permissions to run this program as root). sudo does not have to be translated, it's a detail.
            tr("This program is running with limited privileges. An attempt will now be made to gain root privileges for this test, you may be asked for your sudo password."));
        dd_worker = new DestructiveDiskTesterWrapper(device);
#elif defined(Q_OS_WIN)
        QMessageBox::critical(this, tr("Disk test"),
            //: This warning is shown on Windows, so the term administrator is used. Alternative: Please start this program with elevated privileges.
            tr("This program is running with limited privileges. Try restarting the program as administrator."));
        dd_worker->deleteLater();
        return;
#endif
    }

    //Thread for worker
    //... after the check above, otherwise dd_worker (+ GUI)  would get stuck.
    QThread *thread = new QThread;
    dd_worker->moveToThread(thread);

    //Start worker when thread starts
    connect(thread,
            SIGNAL(started()),
            dd_worker,
            SLOT(start()));

    //Initialization started
    connect(dd_worker,
            SIGNAL(started(qint64)),
            this,
            SLOT(startedDiskTest(qint64)));

    //Written
    connect(dd_worker,
            SIGNAL(written(qint64, double)),
            this,
            SLOT(written(qint64, double)));
    connect(dd_worker,
            SIGNAL(writeFailed(qint64)),
            this,
            SLOT(writeFailed(qint64)));

    //Verified
    connect(dd_worker,
            SIGNAL(verified(qint64, double)),
            this,
            SLOT(verified(qint64, double)));
    connect(dd_worker,
            SIGNAL(verifyFailed(qint64)),
            this,
            SLOT(verifyFailed(qint64)));

    //Test completed handler (successful or not)
    connect(dd_worker,
            SIGNAL(finished(bool)),
            this,
            SLOT(completedDiskTest(bool)));

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

    //Enable progress label
    tmr_pro_testing->setProperty("BLINK_STATE", 0);
    lbl_pro_testing->setProperty("PHASE", tr("STARTING"));
    lbl_pro_left_light->setPixmap(progressLightPixmap("yellow"));
    lbl_pro_right_light->setPixmap(progressLightPixmap("yellow"));
    tmr_pro_testing->start();

    //Enable fields
    txt_capacity->setEnabled(true);
    pro_initializing->setEnabled(true);
    pro_writing->setEnabled(true);
    pro_verifying->setEnabled(true);
    txt_time->setEnabled(true);
    txt_result->setEnabled(true);

    //Start test in background
    thread->start();

    //Start timer
    tmr_total_test_time.start();

}

void
CapacityTesterGui::startDiskTest()
{
    //Show device selection dialog
    UsbDiskSelectionDialog *selection_dialog = new UsbDiskSelectionDialog(this);
    selection_dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(selection_dialog, &UsbDiskSelectionDialog::deviceSelected, this, [this]
    (const QString &dev_path, const QString &dev_title)
    {
        startDiskTest(dev_path);
    });
    selection_dialog->open();
}

void
CapacityTesterGui::startedDiskTest(qint64 total)
{
    initializationStarted(total);
    lbl_pro_testing->setProperty("PHASE", tr("TESTING"));

    //Size capacity = total ...; //Size can't init MB yet
    txt_capacity->setText(QString("%1 GB (%2 MB)").
        arg(total / 1024).
        arg(total));

}

void
CapacityTesterGui::completedDiskTest(bool success)
{

    //Phase
    tmr_pro_testing->stop();
    lbl_pro_testing->clear();
    //Stop animation
    ani_pro_left->stop();
    eff_pro_left->setOpacity(1);
    lbl_pro_left_light->setVisible(true);
    ani_pro_right->stop();
    eff_pro_right->setOpacity(1);
    lbl_pro_right_light->setVisible(true);

    if (success)
    {
        //SUCCESS
        lbl_pro_testing->setText(tr("TEST SUCCEEDED"));
        txt_result->setPlainText(tr(
            "TEST COMPLETED SUCCESSFULLY, NO ERRORS FOUND."));
        txt_result->setStyleSheet("background-color:#DFF0D8; color:#437B43;");

        lbl_pro_left_light->setPixmap(progressLightPixmap("green"));
        lbl_pro_right_light->setPixmap(progressLightPixmap("green"));

    }
    else
    {
        //ERROR
        lbl_pro_testing->setText(tr("TEST FAILED"));

        lbl_pro_left_light->setPixmap(progressLightPixmap("red"));
        lbl_pro_right_light->setPixmap(progressLightPixmap("red"));

        //The position has already been written to txt_result by the slot
        //writeFailed / verifyFailed
        //TODO use/get gotError signal for unexpected errors
        QString comment = "";
        QMessageBox::critical(this,
            tr("Test failed"),
            tr("Test failed. ") + QString("%1").arg(comment));
        //txt_result->appendPlainText(comment);

    }

    //Start/stop buttons
    btn_start_volume_test->setEnabled(true);
    btn_stop_volume_test->setEnabled(false);

    //Enable volume selection and other stuff
    btn_select_volume->setEnabled(true);
    btn_advanced->setEnabled(true);

    //Stop timer
    tmr_total_test_time.invalidate();

}

void
CapacityTesterGui::unloadVolume()
{
    //Clear/reset fields
    txt_capacity->clear();
    txt_used->clear();
    txt_available->clear();
    pro_initializing->setValue(0);
    pro_writing->setValue(0);
    pro_verifying->setValue(0);
    txt_write_speed->clear();
    txt_read_speed->clear();

    //Reset status field
    lbl_pro_testing->clear();
    lbl_pro_testing->setProperty("PHASE", QString());
    txt_time->clear();
    txt_result->clear();
    txt_result->setStyleSheet(" ");

    //Disable fields
    txt_capacity->setEnabled(false);
    txt_used->setEnabled(false);
    txt_available->setEnabled(false);
    pro_initializing->setEnabled(false);
    pro_writing->setEnabled(false);
    pro_verifying->setEnabled(false);
    txt_write_speed->setEnabled(false);
    txt_read_speed->setEnabled(false);
    txt_time->setEnabled(false);
    txt_result->setEnabled(false);

    //Forget mountpoint
    selected_mountpoint.clear();
    txt_volume->clear();

    //Reset test buttons
    btn_start_volume_test->setEnabled(false);
    btn_stop_volume_test->setEnabled(false);

    //Hide progress lights
    lbl_pro_left_light->setVisible(false);
    lbl_pro_right_light->setVisible(false);

}

void
CapacityTesterGui::setCapacityFields(VolumeTester &tester)
{
    Size capacity = tester.bytesTotal();
    txt_capacity->setText(QString("%1 / %2 B").
        arg(capacity.formatted()).
        arg((qint64)capacity));
    Size used = tester.bytesUsed();
    int used_percentage =
        capacity ? ((double)used / capacity) * 100 : 0;
    txt_used->setText(QString("%1 / %2 B / %3%").
        arg(used.formatted()).
        arg((qint64)used).
        arg(used_percentage));
    Size available = tester.bytesAvailable();
    int available_percentage =
        capacity ? ((double)available / capacity) * 100 : 0;
    txt_available->setText(QString("%1 / %2 B / %3%").
        arg(available.formatted()).
        arg((qint64)available).
        arg(available_percentage));

}

void
CapacityTesterGui::loadVolume(const QString &mountpoint)
{
    //Unload
    unloadVolume();
    if (mountpoint.isEmpty()) return;

    //Check if mountpoint is still a mountpoint
    //Set mountpoint if valid
    VolumeTester tester(mountpoint);
    if (!tester.isValid())
    {
        //Don't load if storage object invalid
        //Don't load if old mp of "/foo" now "/" (/foo unmounted)
        QMessageBox::critical(this,
            tr("Volume invalid"),
            tr("The selected volume is not valid."));
        return;
    }

    //Warn if the volume is large and offer alternative test routine
    qint64 gb = tester.bytesTotal() / VolumeTester::GB;
    if (gb > 15)
    {
        if (QMessageBox::information(this, tr("Volume selected"),
            //: Disk test is the name of the (alternative) test routine found in the menu. Advanced is the label of the button that opens that menu, so the translation of Advanced in this text must match that of the button label.
            tr("It will take a long time to check %1 GB. Alternatively, you can try the disk test, which is much faster (minutes). It can be found under Advanced.").arg(gb), QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Cancel)
        {
            return;
        }
    }

    //Remember selected mountpoint
    txt_volume->setText(mountpoint);
    selected_mountpoint = mountpoint;

    //Enable fields
    txt_capacity->setEnabled(true);
    txt_used->setEnabled(true);
    txt_available->setEnabled(true);
    pro_initializing->setEnabled(true);
    pro_writing->setEnabled(true);
    pro_verifying->setEnabled(true);
    txt_write_speed->setEnabled(true);
    txt_read_speed->setEnabled(true);
    txt_time->setEnabled(true);
    txt_result->setEnabled(true);

    //Volume information
    setCapacityFields(tester);

    //Check for old test files that have not been removed (crash?)
    //Cannot test if those are present
    bool ok_to_test = true;
    QStringList conflict_files = tester.conflictFiles();
    if (!conflict_files.isEmpty())
    {
        //Old test files found (shouldn't happen)
        //Cannot start test
        QMessageBox msgbox(this);
        msgbox.setIcon(QMessageBox::Critical);
        msgbox.setWindowTitle(tr("Old test files found"));
        int n = conflict_files.count();
        msgbox.setText(tr(
            "%n old test file(s) have been found! "
            "Cannot test with these files present, please delete them! "
            "The volume should be completely empty.", "", n)
        );
        msgbox.setDetailedText(conflict_files.join("\n"));
        msgbox.exec();
        return;
    }

    //Check filesystem usage (should be empty)
    if (tester.usedPercentagePoints() > 10)
    {
        //Warn user, filesystem should be empty
        QMessageBox msgbox(this);
        msgbox.setIcon(QMessageBox::Warning);
        msgbox.setWindowTitle(tr("Volume not empty"));
        msgbox.setText(tr(
            "This volume is %1% full. "
            "The volume should be completely empty.").arg(tester.usedPercentagePoints())
        );
        msgbox.setStandardButtons(QMessageBox::Abort);
        if (msgbox.exec() != QMessageBox::Ignore)
        {
            return;
        }

        //Don't allow user to start test
        ok_to_test = false;
    }

    //Check for files in selected filesystem (should be empty)
    QStringList root_files = tester.rootFiles();
    if (!root_files.isEmpty())
    {
        //Warn user, filesystem should be empty
        QMessageBox msgbox(this);
        msgbox.setIcon(QMessageBox::Warning);
        msgbox.setWindowTitle(tr("Volume not empty"));
        int n = root_files.count();
        msgbox.setText(tr(
            "%n file(s) have been found. "
            "The volume should be completely empty.", "", n)
        );
        msgbox.setDetailedText(root_files.join("\n"));
        msgbox.setStandardButtons(QMessageBox::Ignore | QMessageBox::Cancel);
        msgbox.setDefaultButton(QMessageBox::Ignore);
        if (msgbox.exec() != QMessageBox::Ignore)
        {
            return;
        }
    }

    //Enable start button (if no problems found)
    if (ok_to_test)
    {
        btn_start_volume_test->setEnabled(true);
    }

}

void
CapacityTesterGui::startVolumeTest()
{
    //Check if selected mountpoint still valid (still a mountpoint)
    QString mountpoint = selected_mountpoint;
    VolumeTester tester(mountpoint);
    if (!tester.isValid())
    {
        //Don't load if storage object invalid
        //Most important check: storage.rootPath() == mountpoint
        //Don't load if old mp of "/foo" now "/" (/foo unmounted)
        QMessageBox::critical(this,
            tr("Volume invalid"),
            tr("The selected volume is not valid anymore."));
        QTimer::singleShot(0, this, SLOT(unloadVolume()));
        return;
    }

    //Update volume information
    setCapacityFields(tester);

    //Check if full
    if (!tester.bytesAvailable())
    {
        //Volume full or quota exhausted
        QMessageBox::critical(this,
            tr("Volume full"),
            tr("The selected volume is full."));
        return;
    }

    //Ask again if volume not empty
    //TODO ignore default ****: lost+found, System Volume Information
    {
        QStringList root_files = tester.rootFiles();
        if (!root_files.isEmpty())
        {
            QMessageBox msgbox(this);
            msgbox.setIcon(QMessageBox::Warning);
            msgbox.setWindowTitle(tr("Volume not empty"));
            int n = root_files.count();
            msgbox.setText(tr(
                "The volume is not empty: %n file(s) have been found. "
                "You should cancel, delete all those files and try again. "
                "Are you really sure you want to continue?", "", n)
            );
            msgbox.setStandardButtons(
                QMessageBox::Yes | QMessageBox::Cancel);
            msgbox.setDefaultButton(QMessageBox::Yes);
            msgbox.setDetailedText(root_files.join("\n"));
            if (msgbox.exec() != QMessageBox::Yes) return;
        }
    }

    //Sanity checks above
    //Test starts here

    //Disable volume selection and other stuff
    btn_select_volume->setEnabled(false);
    btn_advanced->setEnabled(false);

    //Start/stop buttons
    btn_start_volume_test->setEnabled(false);
    btn_stop_volume_test->setEnabled(true);

    //Reset fields
    txt_write_speed->clear();
    txt_read_speed->clear();
    txt_time->clear();
    txt_result->clear();
    txt_result->setStyleSheet(" ");

    //Worker
    volume_worker = new VolumeTester(mountpoint);
    if (req_remount) volume_worker->setReqRemount(true);
    if (disable_precheck) volume_worker->disableInitPrecheck(true);

    //Thread for worker
    QThread *thread = new QThread;
    volume_worker->moveToThread(thread);

    //Start worker when thread starts
    connect(thread,
            SIGNAL(started()),
            volume_worker,
            SLOT(start()));

    //Initialization started
    connect(volume_worker,
            SIGNAL(initializationStarted(qint64)),
            this,
            SLOT(initializationStarted(qint64)));

    //Block initialized
    connect(volume_worker,
            SIGNAL(initialized(qint64, double)),
            this,
            SLOT(initialized(qint64, double)));

    //Written
    connect(volume_worker,
            SIGNAL(written(qint64, double)),
            this,
            SLOT(written(qint64, double)));

    //Verified
    connect(volume_worker,
            SIGNAL(verified(qint64, double)),
            this,
            SLOT(verified(qint64, double)));

    //Write started
    connect(volume_worker,
            SIGNAL(writeStarted()),
            this,
            SLOT(writeStarted()));

    //Verify started
    connect(volume_worker,
            SIGNAL(verifyStarted()),
            this,
            SLOT(verifyStarted()));

    //Create failed
    connect(volume_worker,
            SIGNAL(createFailed(int, qint64)),
            this,
            SLOT(createFailed(int, qint64)));

    //Write failed
    connect(volume_worker,
            SIGNAL(writeFailed(qint64)),
            this,
            SLOT(writeFailed(qint64)));

    //Verify failed
    connect(volume_worker,
            SIGNAL(verifyFailed(qint64)),
            this,
            SLOT(verifyFailed(qint64)));

    //Test failed handler (after write/verify failed, with delay)
    connect(volume_worker,
            SIGNAL(failed(int)),
            this,
            SLOT(failedVolumeTest(int)));

    //Test succeeded handler
    connect(volume_worker,
            SIGNAL(succeeded()),
            this,
            SLOT(succeededVolumeTest()));

    //Test completed handler (successful or not)
    connect(volume_worker,
            SIGNAL(finished()),
            this,
            SLOT(completedVolumeTest()));

    //Stop thread when worker done (stops event loop -> thread->finished())
    connect(volume_worker,
            SIGNAL(finished()),
            thread,
            SLOT(quit()));

    //Delete worker when done
    connect(volume_worker,
            SIGNAL(finished()),
            volume_worker,
            SLOT(deleteLater()));

    //Delete thread when thread done (event loop stopped)
    connect(thread,
            SIGNAL(finished()),
            thread,
            SLOT(deleteLater()));

    //Handle remount request in this (GUI) thread
    connect(volume_worker,
            SIGNAL(remountRequested(const QString&)),
            this,
            SLOT(executeRemount(const QString&)));
    //connect(this,
    //        SIGNAL(remountExecuted(const QString&)),
    //        volume_worker,
    //        SLOT(handleRemounted(const QString&))); //slot is not called...
    //see executeRemount() which directly calls worker to continue

    //Stop worker on request
    //set stop flag immediately (DirectConnection acts in this thread!)
    //otherwise (AutoConnection) not canceled for a long time as thread blocks
    connect(this,
            SIGNAL(reqStop()),
            volume_worker,
            SLOT(cancel()), Qt::DirectConnection);
    //when canceled, test failed is shown with all buttons disabled
    //until all the files are cleaned up and the worker is gone

    //Start test in background
    thread->start();

    //Enable progress label
    tmr_pro_testing->setProperty("BLINK_STATE", 0);
    lbl_pro_testing->setProperty("PHASE", tr("STARTING"));
    lbl_pro_left_light->setPixmap(progressLightPixmap("yellow"));
    lbl_pro_right_light->setPixmap(progressLightPixmap("yellow"));
    tmr_pro_testing->start();

    //Start timer
    tmr_total_test_time.start();

}

void
CapacityTesterGui::stopTest()
{
    //No action if no test running
    if (!volume_worker && !dd_worker) return;

    //Update progress label
    lbl_pro_testing->setProperty("PHASE", tr("STOPPING"));

    //Disable button
    btn_stop_volume_test->setEnabled(false);

    //Request worker to stop
    emit reqStop();
    //this essentially does:
    //if (volume_worker)
    //    volume_worker->cancel();
    //else if (dd_worker)
    //    dd_worker->cancel();

}

void
CapacityTesterGui::succeededVolumeTest()
{
    //Phase
    tmr_pro_testing->stop();
    lbl_pro_testing->clear();
    lbl_pro_testing->setText(tr("TEST SUCCEEDED"));

    //Result - SUCCESS
    txt_result->setPlainText(tr(
        "TEST COMPLETED SUCCESSFULLY, NO ERRORS FOUND."));
    txt_result->setStyleSheet("background-color:#DFF0D8; color:#437B43;");

    //Stop animation
    lbl_pro_left_light->setPixmap(progressLightPixmap("green"));
    lbl_pro_right_light->setPixmap(progressLightPixmap("green"));
    ani_pro_left->stop();
    eff_pro_left->setOpacity(1);
    lbl_pro_left_light->setVisible(true);
    ani_pro_right->stop();
    eff_pro_right->setOpacity(1);
    lbl_pro_right_light->setVisible(true);

    //Show message
    QMessageBox::information(this,
        tr("Test succeeded"),
        tr("Test completed successfully, no errors found."));

}

void
CapacityTesterGui::failedVolumeTest(int error_type)
{
    //Phase
    tmr_pro_testing->stop();
    lbl_pro_testing->clear();
    lbl_pro_testing->setText(tr("TEST FAILED"));

    //Stop animation
    lbl_pro_left_light->setPixmap(progressLightPixmap("red"));
    lbl_pro_right_light->setPixmap(progressLightPixmap("red"));
    ani_pro_left->stop();
    eff_pro_left->setOpacity(1);
    lbl_pro_left_light->setVisible(true);
    ani_pro_right->stop();
    eff_pro_right->setOpacity(1);
    lbl_pro_right_light->setVisible(true);

    //No message if aborted (by user)
    if (error_type & VolumeTester::Error::Aborted) return;

    //Show message
    bool probably_bad = true;
    if (error_type & VolumeTester::Error::Create ||
        error_type & VolumeTester::Error::Permissions)
        probably_bad = false;
    QString comment = probably_bad ?
        tr("The volume might be bad.") :
        tr("An error has occurred.");
    if (error_type & VolumeTester::Error::Create)
        comment += " " + tr("Error creating test file.");
    if (error_type & VolumeTester::Error::Permissions)
        comment += " " + tr("Permission denied.");
    if (error_type & VolumeTester::Error::Resize)
        comment += " " + tr("Resizing test file failed.");
    if (error_type & VolumeTester::Error::Write)
        comment += " " + tr("Write failed.");
    if (error_type & VolumeTester::Error::Verify)
        comment += " " + tr("Verification failed.");
    QMessageBox::critical(this,
        tr("Test failed"),
        tr("Test failed. ") + QString("%1").arg(comment));
    txt_result->appendPlainText(comment);

}

void
CapacityTesterGui::completedVolumeTest()
{
    //Reset fields and values
    total_test_size = 0;

    //Start/stop buttons
    btn_start_volume_test->setEnabled(true);
    btn_stop_volume_test->setEnabled(false);

    //Enable volume selection and other stuff
    btn_select_volume->setEnabled(true);
    btn_advanced->setEnabled(true);

    //Stop timer
    tmr_total_test_time.invalidate();

}

void
CapacityTesterGui::updateProgressLabel()
{
    //Update progress text (if set)
    QString text = lbl_pro_testing->property("PHASE").toString();
    if (!text.isEmpty())
    {
        int old_dot_count = lbl_pro_testing->property("DOTS").toInt();
        int new_dot_count = (old_dot_count + 1) % 4;
        lbl_pro_testing->setProperty("DOTS", new_dot_count);
        QString dots(new_dot_count, '.');
        lbl_pro_testing->setText(dots + " " + text + " " + dots);
    }

    //Animation
    int blink_state = tmr_pro_testing->property("BLINK_STATE").toInt();
    if (blink_state == 0)
    {
        //Fade in left one, keep right one hidden
        ani_pro_left->setStartValue(0);
        ani_pro_left->setEndValue(1);
        ani_pro_right->setStartValue(0);
        ani_pro_right->setEndValue(0);
        blink_state = 2;
    }
    else if (blink_state == 1)
    {
        //Fade in left one, fade out right one
        ani_pro_left->setStartValue(0);
        ani_pro_left->setEndValue(1);
        ani_pro_right->setStartValue(1);
        ani_pro_right->setEndValue(0);
        blink_state = 2;
    }
    else if (blink_state == 2)
    {
        //Fade out left one, fade in right one
        ani_pro_left->setStartValue(1);
        ani_pro_left->setEndValue(0);
        ani_pro_right->setStartValue(0);
        ani_pro_right->setEndValue(1);
        blink_state = 1;
    }
    tmr_pro_testing->setProperty("BLINK_STATE", blink_state);
    ani_pro_left->start();
    ani_pro_right->start();
    if (!lbl_pro_left_light->isVisible())
        lbl_pro_left_light->setVisible(true);
    if (!lbl_pro_right_light->isVisible())
        lbl_pro_right_light->setVisible(true);

    //Show elapsed time
    if (tmr_total_test_time.isValid())
    {
        qint64 total_seconds = tmr_total_test_time.elapsed() / 1000;
        qlonglong elapsed_minutes = total_seconds / 60;
        qlonglong elapsed_seconds = total_seconds % 60;
        QString str_m_s = QString("%1:%2").
            arg(elapsed_minutes, 2, 10, QChar('0')).
            arg(elapsed_seconds, 2, 10, QChar('0'));
        txt_time->setText(str_m_s);
    }

}

void
CapacityTesterGui::initializationStarted(qint64 total)
{
    //Remember test size
    total_test_size = total;

    //Progress max
    int total_mb = total; //all units in MB, bytes would overflow int
    pro_initializing->setMaximum(total_mb);
    pro_writing->setMaximum(total_mb);
    pro_verifying->setMaximum(total_mb);

    //Reset progress bars
    pro_initializing->setValue(0);
    pro_writing->setValue(0);
    pro_verifying->setValue(0);

    //Test phase
    lbl_pro_testing->setProperty("PHASE", tr("INITIALIZING"));

}

void
CapacityTesterGui::initialized(qint64 size, double avg_speed)
{
    Q_UNUSED(avg_speed);

    //Initialized MB (progress)
    int initialized_mb = size;
    pro_initializing->setValue(initialized_mb);

}

void
CapacityTesterGui::writeStarted()
{
    //Test phase
    lbl_pro_testing->setProperty("PHASE", tr("WRITING"));
    lbl_pro_left_light->setPixmap(progressLightPixmap("orange"));
    lbl_pro_right_light->setPixmap(progressLightPixmap("orange"));

}

void
CapacityTesterGui::verifyStarted()
{
    //Test phase
    lbl_pro_testing->setProperty("PHASE", tr("VERIFYING"));
    lbl_pro_left_light->setPixmap(progressLightPixmap("blue"));
    lbl_pro_right_light->setPixmap(progressLightPixmap("blue"));

}

void
CapacityTesterGui::createFailed(int index, qint64 start)
{
    Q_UNUSED(index);
    Q_UNUSED(start);

    //Result - ERROR
    txt_result->setPlainText(tr("ACCESS ERROR!"));
    txt_result->setStyleSheet("background-color:#F2DEDE; color:#A94442;");

}

void
CapacityTesterGui::writeFailed(qint64 start)
{
    //MB
    int start_mb = start;

    //Result - ERROR
    txt_result->setPlainText(tr("WRITE ERROR AFTER %1 MB!").arg(start_mb));
    txt_result->setStyleSheet("background-color:#F2DEDE; color:#A94442;");

}

void
CapacityTesterGui::verifyFailed(qint64 start)
{
    //MB
    int start_mb = start;

    //Result - ERROR
    txt_result->setPlainText(tr("READ ERROR AFTER %1 MB!").arg(start_mb));
    txt_result->setStyleSheet("background-color:#F2DEDE; color:#A94442;");

}

void
CapacityTesterGui::written(qint64 written, double avg_speed)
{
    //MB
    int written_mb = written;
    pro_writing->setValue(written_mb);

    //Speed
    txt_write_speed->setText(QString("%1 MB/s").arg(avg_speed, 0, 'f', 1));

}

void
CapacityTesterGui::verified(qint64 read, double avg_speed)
{
    //MB
    int read_mb = read;
    pro_verifying->setValue(read_mb);

    //Speed
    txt_read_speed->setText(QString("%1 MB/s").arg(avg_speed, 0, 'f', 1));

}

void
CapacityTesterGui::executeRemount(const QString &mountpoint)
{
#ifdef UDISKMANAGER_HPP
    //Unmount, mount
    UDiskManager diskmanager;
    QString old_mountpoint = mountpoint;
    QString device = diskmanager.mountedDevice(mountpoint);
    //Unmount
    if (!diskmanager.umount(device))
    {
        emit remountExecuted("");
        return;
    }
    //Mount
    QString new_mountpoint;
    if (!diskmanager.mount(device, &new_mountpoint))
    {
        emit remountExecuted("");
        return;
    }
    //Confirm remount, continue
    //emit remountExecuted(new_mountpoint); does not arrive
    volume_worker->handleRemounted(new_mountpoint);

#else
    return;
#endif
}

