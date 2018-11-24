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

CapacityTesterGui::CapacityTesterGui(QWidget *parent, Qt::WindowFlags flags)
                 : QMainWindow(parent, flags),
                   closing(false)
{
    //Layout items
    QVBoxLayout *vbox_main = new QVBoxLayout;
    QFrame *hline = 0;
    QWidget *main_widget = new QWidget;
    main_widget->setLayout(vbox_main);
    setCentralWidget(main_widget);

    //Font
    QFont monospace_font =
        QFontDatabase::systemFont(QFontDatabase::FixedFont);

    //Volume selection
    QHBoxLayout *hbox_volume = new QHBoxLayout;
    vbox_main->addLayout(hbox_volume);
    QLabel *lbl_volume = new QLabel(tr("Volume"));
    hbox_volume->addWidget(lbl_volume);
    cmb_volume = new QComboBox;
    cmb_volume->setFont(monospace_font);
    cmb_volume->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    cmb_volume->setToolTip(tr("Mounted volume to be tested"));
    connect(cmb_volume,
            SIGNAL(currentIndexChanged(int)),
            SLOT(loadVolume(int)));
    hbox_volume->addWidget(cmb_volume);
    btn_refresh_volumes = new QPushButton(tr("&Refresh"));
    hbox_volume->addWidget(btn_refresh_volumes);
    connect(btn_refresh_volumes,
            SIGNAL(clicked()),
            SLOT(refreshVolumeList()));

    //Load list of volumes
    QTimer::singleShot(0, this, SLOT(refreshVolumeList()));

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
    volume_form->addRow(tr("Used"), txt_used);

    //Available space
    txt_available = new QLineEdit;
    txt_available->setReadOnly(true);
    txt_available->setFont(monospace_font);
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
    volume_form->addRow(tr("Initializing"), pro_initializing);
    pro_writing = new QProgressBar;
    volume_form->addRow(tr("Writing"), pro_writing);
    pro_verifying = new QProgressBar;
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
    btn_start_volume_test = new QPushButton(tr("Start &Test"));
    connect(btn_start_volume_test,
            SIGNAL(clicked()),
            SLOT(startVolumeTest()));
    btn_stop_volume_test = new QPushButton(tr("&Stop Test"));
    connect(btn_stop_volume_test,
            SIGNAL(clicked()),
            SLOT(stopVolumeTest()));
    btn_quit = new QPushButton(tr("&Quit"));
    connect(btn_quit,
            SIGNAL(clicked()),
            SLOT(close()));

    //Initiate/unload view
    QTimer::singleShot(0, this, SLOT(unloadVolume()));

    //Button row
    QHBoxLayout *hbox_buttons = new QHBoxLayout;
    vbox_main->addLayout(hbox_buttons);
    hbox_buttons->addWidget(btn_start_volume_test);
    hbox_buttons->addWidget(btn_stop_volume_test);
    hbox_buttons->addWidget(btn_quit);

    //Window position
    setGeometry(
        QStyle::alignedRect(
            Qt::LeftToRight,
            Qt::AlignCenter,
            this->size(),
            QApplication::desktop()->availableGeometry())
    );

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
    if (worker)
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

        //Abort test (if still running)
        if (!worker) return;
        connect(worker,
                SIGNAL(destroyed()),
                SLOT(close()));
        setEnabled(false);
        stopVolumeTest();

    }
    else
    {
        event->accept();
    }
}

void
CapacityTesterGui::refreshVolumeList()
{
    //Remember previously selected item
    QString selected_mountpoint = cmb_volume->currentData().toString();
    int selected_index = -1;

    //Clear
    cmb_volume->clear();
    cmb_volume->addItem("");

    //The user is provided with a list of mounted filesystems.
    //So finding the right drive should be easy.
    //Letting the user select any random directory on the system
    //would not be user-friendly at all.

    //Fill list with mounted filesystems
    foreach (QString mountpoint, VolumeTester::availableMountpoints())
    {
        //Gather information
        VolumeTester tester(mountpoint);
        if (!tester.isValid()) continue;
        QString label = tester.label();

        //Check if selected in list
        if (mountpoint == selected_mountpoint)
            selected_index = cmb_volume->count();

        //Add to list
        cmb_volume->addItem(label, QVariant(mountpoint));

    }

    //Select previously selected item
    if (selected_index != -1)
    {
        cmb_volume->setCurrentIndex(selected_index);
        loadVolume(selected_index);
    }

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
    txt_capacity->setText(tr("%1 / %2 B").
        arg(capacity.formatted()).
        arg((qint64)capacity));
    Size used = tester.bytesUsed();
    int used_percentage =
        capacity ? ((double)used / capacity) * 100 : 0;
    txt_used->setText(tr("%1 / %2 B / %3%").
        arg(used.formatted()).
        arg((qint64)used).
        arg(used_percentage));
    Size available = tester.bytesAvailable();
    int available_percentage =
        capacity ? ((double)available / capacity) * 100 : 0;
    txt_available->setText(tr("%1 / %2 B / %3%").
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
    VolumeTester tester(mountpoint);
    if (!tester.isValid())
    {
        //Don't load if storage object invalid
        //Don't load if old mp of "/foo" now "/" (/foo unmounted)
        QMessageBox::critical(this,
            tr("Volume invalid"),
            tr("The selected volume is not valid."));
        cmb_volume->setCurrentIndex(0); //unselect
        QTimer::singleShot(0, this, SLOT(refreshVolumeList())); //update
        return;
    }

    //Remember selected mountpoint
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
        msgbox.setText(tr(
            "%1 old test file(s) have been found! "
            "Cannot test with these files present, please delete them! "
            "The volume should be completely empty.").
            arg(conflict_files.count()));
        msgbox.setDetailedText(conflict_files.join("\n"));
        msgbox.exec();

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
        msgbox.setText(tr(
            "%1 file(s) have been found. "
            "The volume should be completely empty.").
            arg(root_files.count()));
        msgbox.setDetailedText(root_files.join("\n"));
        msgbox.setStandardButtons(QMessageBox::Ignore | QMessageBox::Cancel);
        msgbox.setDefaultButton(QMessageBox::Cancel);
        if (msgbox.exec() == QMessageBox::Cancel)
        {
            //Cancel (undo selection)
            cmb_volume->setCurrentIndex(0); //unselect
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
CapacityTesterGui::loadVolume(int index)
{
    QString mountpoint = cmb_volume->itemData(index).toString();
    loadVolume(mountpoint);
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
        cmb_volume->setCurrentIndex(0); //unselect
        QTimer::singleShot(0, this, SLOT(refreshVolumeList())); //update
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
    {
        QStringList root_files = tester.rootFiles();
        if (!root_files.isEmpty())
        {
            QMessageBox msgbox(this);
            msgbox.setIcon(QMessageBox::Warning);
            msgbox.setWindowTitle(tr("Volume not empty"));
            msgbox.setText(tr(
                "The volume is not empty: %1 file(s) have been found. "
                "You should cancel, delete all those files and try again. "
                "Are you really sure you want to continue?").
                arg(root_files.count()));
            msgbox.setStandardButtons(
                QMessageBox::Ignore | QMessageBox::Cancel);
            msgbox.setDefaultButton(QMessageBox::Cancel);
            msgbox.setDetailedText(root_files.join("\n"));
            if (msgbox.exec() != QMessageBox::Ignore) return;
        }
    }

    //Sanity checks above
    //Test starts here

    //Disable volume selection
    cmb_volume->setEnabled(false);
    btn_refresh_volumes->setEnabled(false);

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
    worker = new VolumeTester(mountpoint);

    //Thread for worker
    QThread *thread = new QThread;
    worker->moveToThread(thread);

    //Start worker when thread starts
    connect(thread,
            SIGNAL(started()),
            worker,
            SLOT(start()));

    //Initialization started
    connect(worker,
            SIGNAL(initializationStarted(qint64)),
            this,
            SLOT(initializationStarted(qint64)));

    //Block initialized
    connect(worker,
            SIGNAL(initialized(qint64, double)),
            this,
            SLOT(initialized(qint64, double)));

    //Written
    connect(worker,
            SIGNAL(written(qint64, double)),
            this,
            SLOT(written(qint64, double)));

    //Verified
    connect(worker,
            SIGNAL(verified(qint64, double)),
            this,
            SLOT(verified(qint64, double)));

    //Write started
    connect(worker,
            SIGNAL(writeStarted()),
            this,
            SLOT(writeStarted()));

    //Verify started
    connect(worker,
            SIGNAL(verifyStarted()),
            this,
            SLOT(verifyStarted()));

    //Create failed
    connect(worker,
            SIGNAL(createFailed(int, qint64)),
            this,
            SLOT(createFailed(int, qint64)));

    //Write failed
    connect(worker,
            SIGNAL(writeFailed(qint64, int)),
            this,
            SLOT(writeFailed(qint64, int)));

    //Verify failed
    connect(worker,
            SIGNAL(verifyFailed(qint64, int)),
            this,
            SLOT(verifyFailed(qint64, int)));

    //Test failed handler (after write/verify failed, with delay)
    connect(worker,
            SIGNAL(failed(int)),
            this,
            SLOT(failedVolumeTest(int)));

    //Test succeeded handler
    connect(worker,
            SIGNAL(succeeded()),
            this,
            SLOT(succeededVolumeTest()));

    //Test completed handler (successful or not)
    connect(worker,
            SIGNAL(finished()),
            this,
            SLOT(completedVolumeTest()));

    //Stop thread when worker done (stops event loop -> thread->finished())
    connect(worker,
            SIGNAL(finished()),
            thread,
            SLOT(quit()));

    //Delete worker when done
    connect(worker,
            SIGNAL(finished()),
            worker,
            SLOT(deleteLater()));

    //Delete thread when thread done (event loop stopped)
    connect(thread,
            SIGNAL(finished()),
            thread,
            SLOT(deleteLater()));

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
CapacityTesterGui::stopVolumeTest()
{
    //No action if no test running
    if (!worker) return;

    //Test phase
    lbl_pro_testing->setProperty("PHASE", tr("STOPPING"));

    //Disable button
    btn_stop_volume_test->setEnabled(false);

    //Set stop flag
    worker->cancel();

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
        comment += tr(" Error creating test file.");
    if (error_type & VolumeTester::Error::Permissions)
        comment += tr(" Permission denied.");
    if (error_type & VolumeTester::Error::Resize)
        comment += tr(" Resizing test file failed.");
    if (error_type & VolumeTester::Error::Write)
        comment += tr(" Write failed.");
    if (error_type & VolumeTester::Error::Verify)
        comment += tr(" Verification failed.");
    QMessageBox::critical(this,
        tr("Test failed"),
        tr("Test failed. %1").arg(comment));
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

    //Enable volume selection
    cmb_volume->setEnabled(true);
    btn_refresh_volumes->setEnabled(true);

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
    qint64 mb = 1024 * 1024;
    int total_mb = total / mb; //bytes would overflow int
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
CapacityTesterGui::initialized(qint64 bytes, double avg_speed)
{
    Q_UNUSED(avg_speed);

    //Initialized MB (progress)
    int initialized_mb = bytes / VolumeTester::MB;
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
CapacityTesterGui::writeFailed(qint64 start, int size)
{
    Q_UNUSED(size);

    //MB
    int start_mb = start / VolumeTester::MB;

    //Result - ERROR
    txt_result->setPlainText(tr("WRITE ERROR AFTER %1 MB!").arg(start_mb));
    txt_result->setStyleSheet("background-color:#F2DEDE; color:#A94442;");

}

void
CapacityTesterGui::verifyFailed(qint64 start, int size)
{
    Q_UNUSED(size);

    //MB
    int start_mb = start / VolumeTester::MB;

    //Result - ERROR
    txt_result->setPlainText(tr("READ ERROR AFTER %1 MB!").arg(start_mb));
    txt_result->setStyleSheet("background-color:#F2DEDE; color:#A94442;");

}

void
CapacityTesterGui::written(qint64 written, double avg_speed)
{
    //MB
    int written_mb = written / VolumeTester::MB;
    pro_writing->setValue(written_mb);

    //Speed
    txt_write_speed->setText(tr("%1 MB/s").arg(avg_speed, 0, 'g', 2));

}

void
CapacityTesterGui::verified(qint64 read, double avg_speed)
{
    //MB
    int read_mb = read / VolumeTester::MB;
    pro_verifying->setValue(read_mb);

    //Speed
    txt_read_speed->setText(tr("%1 MB/s").arg(avg_speed, 0, 'g', 2));

}

