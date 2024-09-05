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

QFrame*
CapacityTesterGui::newHLine()
{
    QFrame *hline = new QFrame;
    hline->setFrameShape(QFrame::HLine);
    return hline;
}

CapacityTesterGui::CapacityTesterGui(QWidget *parent)
                 : QMainWindow(parent)
{
    QVBoxLayout *vbox = new QVBoxLayout;
    //setLayout(vbox);
    QWidget *main_widget = new QWidget;
    main_widget->setLayout(vbox);
    setCentralWidget(main_widget);

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

    //Control buttons
    m_button_box = new QDialogButtonBox();
    QPushButton *btn_next = m_button_box->addButton(tr("&Next"), QDialogButtonBox::AcceptRole);
    m_btn_next = btn_next;
    QPushButton *btn_back = m_button_box->addButton(tr("&Back"), QDialogButtonBox::RejectRole);
    m_btn_back = btn_back;
    connect(btn_next, SIGNAL(clicked()), this, SLOT(nextStep()));
    connect(btn_back, SIGNAL(clicked()), this, SLOT(prevStep()));
    btn_back->setEnabled(false);
    vbox->addStretch();
    vbox->addWidget(m_button_box);

    //Show device selection - initial step
    //skip step 0 which is just text nobody wants to read
    m_step = 0;
    //QTimer::singleShot(0, this, SLOT(nextStep()));
    showWelcome(true); //show welcome text

    //Center window
    QTimer::singleShot(100, [this]()
    {
        if (this->width() < 500 || this->height() < 500)
            this->resize(500, 500);
        setGeometry(
            QStyle::alignedRect(
                Qt::LeftToRight,
                Qt::AlignCenter,
                this->size(),
                QGuiApplication::primaryScreen()->availableGeometry()
            )
        );
    });

}

void
CapacityTesterGui::showFilesystemTest(const QString &mountpoint)
{
    //Clear main layout
    QVBoxLayout *vbox = resetMainLayout();

    //Hide nav button, keep box
    m_btn_next->hide();
    m_step = 10;

    //Show volume test widget
    VolumeTestGui *volume_test = new VolumeTestGui(mountpoint);
    vbox->addWidget(volume_test);
    connect(volume_test, &VolumeTestGui::started, this, [=]()
    {
        //Disable nav button
        m_btn_back->setEnabled(false);
    });
    connect(volume_test, &VolumeTestGui::finished, this, [=]()
    {
        //Enable nav button
        //m_btn_next->show(); //not now, see back/prev event
        m_btn_back->setEnabled(true);
    });
    //debug log deletion of volume test widget
    connect(volume_test, &QObject::destroyed, this, [=]()
    {
        Debug("Volume test widget deleted");
    });

}

void
CapacityTesterGui::nextStep()
{
    if (m_step == 0)
    {
        //Show device selection
        showDevSelect();
        if (m_btn_back)
            m_btn_back->setEnabled(true);
    }
    else if (m_step == 1) //device selection
    {
        //Apply selected device, show device details and controls
        applyDev();
        //from here, the user may jump to the volume test instead of continuing with the disk test
    }
    else if (m_step == 2) //device controls
    {
        //Show disk test
        showDiskTestSelect();
    }
    else if (m_step == 3) //disk test selection
    {
        //Show start test
        showDiskTestStart();
    }
    else if (m_step == 4) //disk test start (question/confirmation)
    {
        //Start test now
        startDiskTest();
    }

}

void
CapacityTesterGui::prevStep()
{
    if (m_step <= 1) //intro text about quick/manual, nobody wants to read that
    {
        showWelcome();
        if (m_btn_back)
            m_btn_back->setEnabled(false);
    }
    else if (m_step == 2) //device selection
    {
        showDevSelect();
    }
    else if (m_step == 3) //disk test selection
    {
        showDevControls();
    }
    else if (m_step == 4) //disk test start (last question)
    {
        showDiskTestSelect();
    }
    else if (m_step == 6) //disk test completed
    {
        //Go back to device controls
        //showDevControls();
        m_btn_next->setEnabled(true);
        showDevSelect();
    }
    else if (m_step == 10) //volume (filesystem) test
    {
        //Hide/delete volume test widget
        m_btn_next->show();
        showDevSelect();
    }
    else if (m_step == 20) //format tool
    {
        //Hide/delete volume test widget
        m_btn_next->setEnabled(true);
        showDevControls();
    }
}

void
CapacityTesterGui::showWelcome(bool first_call)
{
    //Clear main layout
    QVBoxLayout *vbox = resetMainLayout();

    //Header
    QLabel *lbl_head = new QLabel(tr("Hello"));
    lbl_head->setStyleSheet("font-weight:bold; text-align:center;");
    vbox->addWidget(lbl_head);

    //Intro(duck)
    //: Introduction text for the program. Feel free to improve the wording. //Welcome to the USB Capacity Tester...
    QLabel *lbl_intro = new QLabel(tr("This program is designed to verify the reported capacity of USB storage devices. It checks if a device is reporting a fake capacity and attempts to determine the actual capacity. Please proceed with caution as this process will wipe all data on the device."));
    lbl_intro->setWordWrap(true);
    //format it blue and green to attract attention
    lbl_intro->setStyleSheet("color:blue; background-color:lightgreen; padding:10px; border:1px solid green; border-radius:5px;");
    vbox->addWidget(lbl_intro);

    //Warn if not running as root
    bool is_root = DestructiveDiskTester::amIRoot(); // (geteuid() == 0);
    if (!is_root)
    {
        //QLabel *lbl_root = new QLabel(tr("This program should be run as root."));
        //: Warning for Linux/Unix (sudo permissions - a Linux/Unix thing).
        QLabel *lbl_root = new QLabel(tr("Note: This program was started as a regular user. If this user lacks sudo permissions, the test will fail to start. It is recommended to run this program as root."));
#if defined(Q_OS_WIN)
        //: Warning for Windows, so almost the same test as above but "Administrator" instead of "root"/sudo.
        lbl_root->setText(tr("Note: This program was started as a regular user. If this user lacks administrative permissions, the test will fail to start. It is recommended to run this program as an administrator."));
#endif
        lbl_root->setStyleSheet("color:gray; font-size:8pt;");
        lbl_root->setWordWrap(true);
        vbox->addWidget(lbl_root);
    }

    /*
    //Note that this is the initial selection between quick and manual mode,
    //selecting quick mode will start the test immediately after selecting a device.
    //And the storage device will be wiped during the test.
    QLabel *lbl_head = new QLabel(tr("Select automatic or manual mode"));
    vbox->addWidget(lbl_head);

    //Quick mode
    //: Automatic mode; quick test [method] means a quick test of the storage device.
    QRadioButton *opt_auto = new QRadioButton(tr("Quick test (automatic)"));
    vbox->addWidget(opt_auto);
    //QLabel *lbl_auto1 = new QLabel(tr("After selecting a USB storage device, it will be formatted and a quick test will be run. That test is usually sufficient to detect fake devices.")); //too much text
    QLabel *lbl_auto1 = new QLabel(tr("This test is usually sufficient to detect fake devices."));
    lbl_auto1->setWordWrap(true);
    vbox->addWidget(lbl_auto1);
    QLabel *lbl_auto2 = new QLabel(tr("This will overwrite all data on the device. This is the recommended test for most users."));
    lbl_auto2->setWordWrap(true);
    vbox->addWidget(lbl_auto2);
    QLabel *lbl_auto3 = new QLabel(tr("This test will only take a few minutes."));
    lbl_auto3->setWordWrap(true);
    vbox->addWidget(lbl_auto3);
    connect(opt_auto, &QAbstractButton::toggled, this, [=](bool checked)
    {
        m_auto = checked;
    });
    opt_auto->setChecked(true);

    //Manual mode
    //: Manual test selection means the user can select the test type.
    QRadioButton *opt_manual = new QRadioButton(tr("Manual test"));
    vbox->addWidget(opt_manual);
    QLabel *lbl_manual1 = new QLabel(tr("Select which test to run and see details. You have the option to select a full disk test, which takes longer."));
    lbl_manual1->setWordWrap(true);
    vbox->addWidget(lbl_manual1);
    connect(opt_manual, &QAbstractButton::toggled, this, [=](bool checked)
    {
        m_auto = !checked;
    });
    */

    //Space
    vbox->addStretch();

    //On first call, schedule next step to be shown
    if (first_call && is_root)
    {
        QTimer::singleShot(1000, this, SLOT(nextStep()));
    }

    m_step = 0;
    m_btn_back->setEnabled(false);
}

void
CapacityTesterGui::showDevSelect()
{
    //Clear main layout
    QVBoxLayout *vbox = resetMainLayout();

    //Header
    QLabel *lbl_head = new QLabel(tr("Select the USB storage device to test"));
    lbl_head->setWordWrap(true);
    QHBoxLayout *hbox_head = new QHBoxLayout;
    vbox->addLayout(hbox_head);
    hbox_head->addWidget(lbl_head);
    hbox_head->addStretch();
    QToolButton *btn_menu = new QToolButton; //menu button with refresh option
    btn_menu->setPopupMode(QToolButton::MenuButtonPopup);
    hbox_head->addWidget(btn_menu);
    btn_menu->setIcon(QIcon::fromTheme("view-refresh"));
    QMenu *mnu = new QMenu(btn_menu);
    btn_menu->setMenu(mnu); //Ownership of the menu is not transferred to the push button.
    QAction *act_refresh = mnu->addAction(tr("Refresh"));
    connect(btn_menu, SIGNAL(clicked()), SLOT(updateDevSelectTable()));
    connect(act_refresh, SIGNAL(triggered()), SLOT(updateDevSelectTable()));
    QAction *act_set_usb_filter = mnu->addAction(tr("USB devices only"));
    //if (m_auto)
    //    act_set_usb_filter->setEnabled(false);
    act_set_usb_filter->setCheckable(true);
    act_set_usb_filter->setChecked(true);
    m_dev_filter = 1;
    connect(act_set_usb_filter, SIGNAL(toggled(bool)), SLOT(setUsbFilter(bool)));
    mnu->addSeparator();
    m_act_format = mnu->addAction(tr("Format storage device"));
    m_act_format->setEnabled(false);
    //connect(m_act_format, SIGNAL(triggered()), SLOT(showFormatTool())); //need applyDev()
    connect(m_act_format, &QAction::triggered, this, [=]()
    {
        applyDev(m_selected_dev, false, true); //just set header, don't continue
        showFormatTool(); //load format tool
    });
    QAction *act_select_volume = mnu->addAction(tr("Select custom location to test"));
    connect(act_select_volume, &QAction::triggered, this, [=]()
    {
        SelectionWindow *window = new SelectionWindow(btn_menu);
        window->setModal(true);
        window->show();
        connect(window, SIGNAL(volumeSelected(const QString&)), SLOT(showFilesystemTest(const QString&)));
    });

    //Device selection
    //Previous table will be deleted automatically, so it's replaced here
    //reusing the table might lead to a crash due to delayed deletion (reset above)
    m_dev_table = new QTableWidget;
    m_dev_table->setStyleSheet("font-weight:bold; font-size:16pt;");
    m_dev_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_dev_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_dev_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_dev_table->setAlternatingRowColors(true);
    //m_dev_table->setCursor(Qt::PointingHandCursor);
    vbox->addWidget(m_dev_table);

    //Handle double click
    connect(m_dev_table, &QTableWidget::cellDoubleClicked, this, [=](int row, int column)
    {
        Q_UNUSED(column);
        checkDevSelection(); //update selected device
        applyDev();
    });
    //Store dev path on select
    connect(m_dev_table, SIGNAL(itemSelectionChanged()), this, SLOT(checkDevSelection()));
    QTimer::singleShot(0, this, SLOT(updateDevSelectTable())); //load list of devices

    //Unload disk tester instance if it exists (after navigating back)
    if (m_dd_worker)
    {
        m_dd_worker->thread()->quit(); //TODO check if this is correct
        m_dd_worker.clear();
    }

    //Horizontal line
    vbox->addWidget(newHLine());

    //Show frame (group box with title on frame border) for quick test
    //label quick start, this test will only take a few minutes and be sufficient to detect fake devices
    QGroupBox *grp_quick = new QGroupBox(tr("Quick test"));
    vbox->addWidget(grp_quick);
    QVBoxLayout *vbox_quick = new QVBoxLayout;
    grp_quick->setLayout(vbox_quick);
    QLabel *lbl_quick1 = new QLabel(tr("This test is usually sufficient to detect fake devices. It will only take a few minutes."));
    lbl_quick1->setWordWrap(true);
    vbox_quick->addWidget(lbl_quick1);
    //show a huge quick start button, large font and possibly colorful to attract attention
    m_btn_quick = new QPushButton(tr("Quick start"));
    vbox_quick->addWidget(m_btn_quick);
    connect(m_btn_quick, &QAbstractButton::clicked, this, [=]()
    {
        applyDev(m_selected_dev, true);
    });
    //font / css (store default css as button property to reset it later)
    m_btn_quick->setStyleSheet("font-weight:bold; font-size:16pt;");
    m_btn_quick->setProperty("default_css", m_btn_quick->styleSheet());
    //make frame border orange to attract attention
    grp_quick->setStyleSheet("QGroupBox { border: 2px solid orange; border-radius: 5px; margin-top: 1.5em; }");


    //Info
    QLabel *lbl_info = new QLabel(tr("Continue to select the type of test to run."));
    vbox->addWidget(lbl_info);
    //I'm told I write too much text, so let's hide this one
    if (m_auto)
        lbl_info->hide();

    //Space
    vbox->addStretch();

    m_step = 1;
}

void
CapacityTesterGui::updateDevSelectTable()
{
    //Do nothing if called too late
    if (!m_dev_table) return;
    //Clear table
    m_dev_table->clearContents();
    m_dev_table->setRowCount(0);
    m_dev_table->setColumnCount(3);
    m_dev_table->setHorizontalHeaderLabels(QStringList() << tr("Capacity") << tr("Name") << "");
    m_dev_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_dev_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);

    //Get devices
    int count = 0;
    //loop through devices; device is copied as some functions are not const
    foreach (StorageDiskSelection::Device dev, StorageDiskSelection().blockDevices())
    {
        //Filter - USB devices only
        bool is_usb = dev.isUsbDevice();
        if (m_dev_filter == 1 && !is_usb)
            continue;

        //Add item
        m_dev_table->insertRow(count);
        QString path = dev.path();
        QString name = dev.displayModelName(); //TODO possible bug if SSK stick on hub!?
        QString extra;

        //Get additional USB info
        //UsbStorageDevice usb_dev(path);
        //if (usb_dev.isValid())
        //{
        //    //: USB speed of the device.
        //    int usb_speed_known = 0;
        //    auto usb_speed = usb_dev.getUdevSpeed(&usb_speed_known);
        //    if (usb_speed_known != -1)
        //        extra = tr("USB speed: %1").arg(usb_dev.usbSpeedName(usb_dev.getSpeed()));
        //}

        //Capacity (total size of device, not just partition)
        Size capacity = dev.capacity();
        QTableWidgetItem *size_item = new QTableWidgetItem(capacity.formatted(0));
        size_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        size_item->setData(Qt::UserRole, path);
        size_item->setToolTip(QString("Capacity reported by the device: %1 GB (%2 B)").
            arg(capacity.gb()).
            arg((qint64)capacity));
        m_dev_table->setItem(count, 0, size_item);

        //Name as reported by the device (some report something like 123456789)
        QTableWidgetItem *name_item = new QTableWidgetItem(name);
        name_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        name_item->setData(Qt::UserRole, path);
        m_dev_table->setItem(count, 1, name_item);

        //Show serial number as tooltip
        QString serial = dev.serialNumber();
        if (serial.isEmpty())
            name_item->setToolTip("");
        else
            //: Serial number of storage device.
            name_item->setToolTip(tr("Serial") + ": " + serial);

        //Show first mountpoint, if mounted (not 100% reliable, best effort)
        //It's not 100% reliable because there are different ways to mount a device,
        //like LVM or encrypted volumes, which are not detected here.
        //But this should not be relevant for USB flash drives.
        QTableWidgetItem *extra_item = new QTableWidgetItem();
        m_dev_table->setItem(count, 2, extra_item);
        QString mountpoint = dev.mountpoint();
        if (!mountpoint.isEmpty())
        {
            //: Mountpoint of the device.
            extra = tr("Mounted at: %1").arg(mountpoint);
        }
        extra_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        extra_item->setData(Qt::UserRole, path);
        extra_item->setToolTip(extra);

        Debug(QS("found device: %s (%s)", CSTR(name), CSTR(path)));
        if (!mountpoint.isEmpty())
            Debug(QS("mounted at: %s", CSTR(mountpoint)));
        count++;
    }

    //Clear selected device
    checkDevSelection(); //clear selected device
}

void
CapacityTesterGui::setUsbFilter(bool checked)
{
    if (checked)
        m_dev_filter = 1;
    else
        m_dev_filter = 0;
    updateDevSelectTable();
}

void
CapacityTesterGui::checkDevSelection()
{
    //Get selected device (from list)
    QString path; //selected device path
    if (!m_dev_table->selectedItems().isEmpty())
        path = m_dev_table->selectedItems().first()->data(Qt::UserRole).toString();

    //Set device path to selected list item
    m_selected_dev = path;
    if (path.isEmpty())
    {
        m_dev_title.clear();
        m_btn_quick->setEnabled(false);
        return;
    }

    //Enable format button
    m_act_format->setEnabled(true);

    //USB device? Otherwise keep button disabled
    StorageDiskSelection::Device device = StorageDiskSelection().blockDevice(path);
    if (!device.isUsbDevice())
    {
        m_btn_quick->setEnabled(false);
        return;
    }

    //Enable quick start button
    m_btn_quick->setEnabled(true);
    //Highlight button for a couple of seconds, to attract attention
    m_btn_quick->setStyleSheet("font-weight:bold; font-size:16pt; color:red;");
    QTimer::singleShot(2000, this, [=]()
    {
        if (!m_btn_quick) return;
        m_btn_quick->setStyleSheet(m_btn_quick->property("default_css").toString());
    });

    //Get device name (display name)
    QString name; // = m_dev_table->item(row, 1)->text();
    if (name.isEmpty())
        name = device.displayModelName();
    m_dev_title = name;
    Debug(QS("selected device: %s (%s)", CSTR(name), CSTR(path)));

}

void
CapacityTesterGui::applyDev()
{
    //Get selected device
    QString path = m_selected_dev; //set in checkDevSelection()
    if (path.isEmpty())
    {
        QMessageBox::critical(this, tr("No device selected"),
            tr("Please select a device to test."));
        return;
    }

    applyDev(path);
}

void
CapacityTesterGui::applyDev(const QString &path, bool quick, bool not_continue)
{
    if (path.isEmpty())
    {
        QMessageBox::critical(this, tr("No device selected"),
            tr("Please select a device to test."));
        return;
    }

    //Reset widget
    foreach (QObject *obj, wid_dev_sel->children())
        obj->deleteLater();
    if (wid_dev_sel->layout()) delete wid_dev_sel->layout();
    //TODO auto-clear dev header when dev object is deleted

    //Set selected device
    StorageDiskSelection::Device device = StorageDiskSelection().blockDevice(path);
    if (!device.isValid())
    {
        QMessageBox::critical(this, tr("Invalid device"),
            tr("The selected device is not valid."));
        m_dev_path.clear();
        return;
    }
    m_dev_path = path;
    if (m_dev_title.isEmpty())
        m_dev_title = device.displayModelName();
    if (m_dev_title.isEmpty())
        m_dev_title = device.path(); //fallback
    m_claimed_capacity = device.capacity();
    Debug(QS("set storage device: %s (%s)", CSTR(path), CSTR(Size(m_claimed_capacity).formatted())));

    //Show selected device in sub header
    QVBoxLayout *vbox = new QVBoxLayout;
    wid_dev_sel->setLayout(vbox);
    QHBoxLayout *hbox = new QHBoxLayout;
    vbox->addLayout(hbox);
    QLabel *lbl_label1 = new QLabel(tr("Device:"));
    lbl_label1->setStyleSheet("font-family:'Prompt', sans-serif; color:black;"); //or VT323
    hbox->addWidget(lbl_label1);
    QLabel *lbl_size = new QLabel(m_claimed_capacity.formatted(0));
    lbl_size->setStyleSheet("font-family:'Prompt', sans-serif; color:black; font-weight:bold;"); //or VT323
    hbox->addWidget(lbl_size);
    QLineEdit *lbl_name = new QLineEdit(m_dev_title);
    lbl_name->setStyleSheet("font-weight:bold; font-family:'Prompt', sans-serif; color:black; background-color:white;"); //or VT323
    lbl_name->setFrame(false);
    lbl_name->setReadOnly(true);
    lbl_name->setCursorPosition(0); //otherwise beginning of text truncated
    lbl_name->setContextMenuPolicy(Qt::NoContextMenu);
    lbl_name->setToolTip(tr("Device path: %1").arg(path));
    hbox->addWidget(lbl_name);
    vbox->addWidget(newHLine());

    //Show device controls and test selection
    //or do nothing (not_continue), caller will load new form
    m_step = 2;
    if (!quick)
    {
        m_test_type = 2;
        showDevControls();
    }
    else if (!not_continue)
    {
        //start quick test without any more questions
        m_auto = true;
        m_test_type = 1;
        showDiskTestStart();
    }
}

void
CapacityTesterGui::showDevControls()
{
    //Clear main layout
    QVBoxLayout *vbox = resetMainLayout();

    //Get storage device
    StorageDiskSelection::Device device = StorageDiskSelection().blockDevice(m_dev_path);

    //Show device info
    QGroupBox *grp_device = new QGroupBox(tr("Device information"));
    vbox->addWidget(grp_device);
    QVBoxLayout *vbox_device = new QVBoxLayout;
    grp_device->setLayout(vbox_device);
    //---
    QShortcut *shortcut = new QShortcut(QKeySequence("Escape"), grp_device);
    connect(shortcut, &QShortcut::activated, this, &CapacityTesterGui::prevStep);

    //Device name
    QLabel *lbl_dev_name = new QLabel(tr("Device name: %1").arg(m_dev_title));
    vbox_device->addWidget(lbl_dev_name);

    //Device path
    //QLabel *lbl_dev_path = new QLabel(tr("Device path: %1").arg(m_dev_path));
    //vbox->addWidget(lbl_dev_path);
    //This may be useful for debugging, but it's not user-friendly

    //Vendor is often not available; model is already shown above

    //Device serial
    QString serial = device.serialNumber();
    QLabel *lbl_dev_serial = new QLabel(tr("Device serial: %1").arg(serial));
    vbox_device->addWidget(lbl_dev_serial);

    //USB speed and info, if available
    UsbStorageDevice usb_dev(m_dev_path);
    if (usb_dev.isValid() && (int)usb_dev.availableApi())
    {
        //USB speed
        int usb_speed_known = 0;
        auto usb_speed = usb_dev.getSpeed(&usb_speed_known);
        if (usb_speed_known != -1)
        {
            QLabel *lbl_dev_speed = new QLabel(tr("Device speed: %1 (%2 Mbps)").arg(usb_dev.usbSpeedName(usb_speed)).arg((int)usb_speed));
            vbox_device->addWidget(lbl_dev_speed);
        }
        else if (usb_speed_known == -1)
        {
            QLabel *lbl_dev_speed = new QLabel(tr("Device speed could not be determined."));
            vbox_device->addWidget(lbl_dev_speed);
        }

        if (usb_speed_known == 2)
        {
            QLabel *lbl_dev_speed = new QLabel(tr("Possible permission problem trying to determine device speed."));
            vbox_device->addWidget(lbl_dev_speed);
        }

        //USB vendor and model
        QString usb_vendor_id, usb_model_id;
        usb_dev.getUsbVendorAndModelId(usb_vendor_id, usb_model_id);
        if (!usb_vendor_id.isEmpty() && !usb_model_id.isEmpty())
        {
            QLabel *lbl_dev_usb_id = new QLabel(tr("USB device id: %1:%2").arg(usb_vendor_id).arg(usb_model_id));
            vbox_device->addWidget(lbl_dev_usb_id);
        }
        else
        {
            usb_dev.getUdevVendorAndModelId(usb_vendor_id, usb_model_id);
            if (!usb_vendor_id.isEmpty() && !usb_model_id.isEmpty())
            {
                QLabel *lbl_dev_usb_id = new QLabel(tr("USB device id: %1:%2").arg(usb_vendor_id).arg(usb_model_id));
                vbox_device->addWidget(lbl_dev_usb_id);
            }
        }

    }
    else
    {
        QLabel *lbl_dev_speed = new QLabel(tr("USB device information not available on this system."));
        vbox_device->addWidget(lbl_dev_speed);
    }

    //Hline
    vbox->addWidget(newHLine());

    //Show filesystem box
    QGroupBox *grp_filesystems = new QGroupBox(tr("Filesystems"));
    vbox->addWidget(grp_filesystems);
    QVBoxLayout *vbox_filesystems = new QVBoxLayout;
    grp_filesystems->setLayout(vbox_filesystems);

    //Number of partitions
    int part_count = device.partitions().count();
    {
        //: This device has %1 partitions.
        QLabel *lbl_parts = new QLabel(tr("Number of partitions: %1").arg(part_count));
        vbox_filesystems->addWidget(lbl_parts);
    }

    int fs_count = 0;
    foreach (auto fs, device.filesystems())
    {
        fs_count++;

        //Layout
        vbox_filesystems->addWidget(newHLine());
        QGridLayout *grid_fs = new QGridLayout;
        vbox_filesystems->addLayout(grid_fs);
        int row = 0;

        //Partition number
        int part_num = device.partitionNumberOfFilesystem(fs.path);
        QLabel *lbl_part_num = new QLabel(tr("Partition"));
        grid_fs->addWidget(lbl_part_num, row, 0);
        QLabel *txt_part_num = new QLabel(QString("%1").arg(part_num));
        grid_fs->addWidget(txt_part_num, row, 1);

        //Size
        //Unfortunately, the size of the filesystem is not available
        //We use the size of the partition instead
        Size part_size = device.partition(part_num).size;
        QLabel *lbl_size = new QLabel(tr("Size"));
        grid_fs->addWidget(lbl_size, ++row, 0);
        QLabel *txt_size = new QLabel(part_size.formatted(0));
        grid_fs->addWidget(txt_size, row, 1);

        //Mountpoint
        QLabel *lbl_fs_mp = new QLabel(tr("Mountpoint"));
        grid_fs->addWidget(lbl_fs_mp, ++row, 0);
        QLineEdit *txt_fs_mp = new QLineEdit(fs.mountpoint);
        txt_fs_mp->setStyleSheet("font-family:'Prompt', sans-serif; color:black;"); //or VT323
        txt_fs_mp->setFrame(false);
        txt_fs_mp->setReadOnly(true);
        txt_fs_mp->setCursorPosition(0); //otherwise beginning of text truncated
        txt_fs_mp->setContextMenuPolicy(Qt::NoContextMenu);
        grid_fs->addWidget(txt_fs_mp, row, 1);

        //Mount buttons
        bool is_mounted = !fs.mountpoint.isEmpty();
        if (is_mounted)
        {
            //Unmount button    
            QPushButton *btn_unmount = new QPushButton(tr("Unmount"));
            grid_fs->addWidget(btn_unmount, ++row, 1);
            connect(btn_unmount, &QAbstractButton::clicked, this, [=]()
            {
                attemptUnmount(fs.path);
            });

            //: Filesystem test (old test method; not disk test, which is more thorough and recommended).
            QPushButton *btn_fs_test = new QPushButton(tr("Filesystem test"));
            grid_fs->addWidget(btn_fs_test, ++row, 1);
            connect(btn_fs_test, &QAbstractButton::clicked, this, [=]()
            {
                showFilesystemTest(fs.mountpoint);
            });
            btn_fs_test->setToolTip(tr("Test the filesystem on this partition. A filesystem test will take hours! A disk test is recommended instead."));
        }
        else
        {
            //Mount button
            QPushButton *btn_mount = new QPushButton(tr("Mount"));
            grid_fs->addWidget(btn_mount, ++row, 0);
            connect(btn_mount, &QAbstractButton::clicked, this, [=]()
            {
                attemptMount(fs.path);
            });
        }

    }

    //Hline
    vbox->addWidget(newHLine());

    QLabel *lbl_disk_test3 = new QLabel(tr("Continue to select a test type."));
    vbox->addWidget(lbl_disk_test3);

    //Space
    vbox->addStretch();

    m_btn_next->setEnabled(true);
    m_step = 2;
}

void
CapacityTesterGui::attemptMount(const QString &filesystem)
{
    StorageDiskSelection::Device device = StorageDiskSelection().blockDevice(m_dev_path);

    //Ask
    if (QMessageBox::question(this, tr("Mount filesystem?"),
        tr("Do you want to mount the selected filesystem?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    //Call mount method
    QString message;
    if (device.mount(filesystem, &message))
    {
        QMessageBox::information(this, tr("Mount filesystem"),
            tr("The selected filesystem has been mounted here: %1").
            arg(message));
        //Update
        showDevControls();
    }
    else
    {
        QMessageBox::critical(this, tr("Mount filesystem"),
            tr("The selected filesystem could not be mounted. %1").
            arg(message));
    }
}

void
CapacityTesterGui::attemptUnmount(const QString &filesystem)
{
    StorageDiskSelection::Device device = StorageDiskSelection().blockDevice(m_dev_path);

    //Ask
    if (QMessageBox::question(this, tr("Unmount filesystem?"),
        tr("Do you want to unmount the selected filesystem?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    //Call unmount method
    QString message;
    if (device.unmount(filesystem, &message))
    {
        QMessageBox::information(this, tr("Unmount filesystem"),
            tr("The selected filesystem has been unmounted."));
        //Update
        showDevControls();
    }
    else
    {
        QMessageBox::critical(this, tr("Unmount filesystem"),
            tr("The selected filesystem could not be unmounted. %1").
            arg(message));
    }
}

void
CapacityTesterGui::showDiskTestSelect()
{
    //Clear main layout
    QVBoxLayout *vbox = resetMainLayout();

    //Header
    //: Type of test can be translated as "test method"
    QLabel *lbl_head = new QLabel(tr("Select the type of test to run"));
    lbl_head->setWordWrap(true);
    vbox->addWidget(lbl_head);

    //Quick test
    //: Quick test [method] means a quick test of the storage device.
    QRadioButton *opt_quick = new QRadioButton(tr("Quick test"));
    vbox->addWidget(opt_quick);
    QLabel *lbl_quick1 = new QLabel(tr("This test is usually sufficient to detect fake devices."));
    //Der Schnelltest prüft gezielt, ob das Speichermedium eine falsche Kapazität angibt und versucht, die reale Kapazität zu ermitteln. Wenn Fake-Sticks, die mehr Kapazität angeben, als sie eigentlich haben, über ihre reale Kapazität hinaus beschrieben werden, gehen die geschriebenen Daten verloren und das Dateisystem auf dem Stick wird in manchen Fällen beschädigt. USB-Sticks, die eine zu große Kapazität angeben, können hiermit schnell erkannt werden, daher ist dies der empfohlene Testtyp. Statt das Speichermedium komplett zu füllen, wird dabei gezielt in die hinteren Bereiche geschrieben, um zu erkennen, ob die geschriebenen Daten wieder ausgelesen werden können.
    lbl_quick1->setWordWrap(true);
    vbox->addWidget(lbl_quick1);
    //QLabel *lbl_quick2 = new QLabel(tr("Test if the de. This is the recommended test for most users."));
    QLabel *lbl_quick2 = new QLabel(tr("The Quick Test specifically checks whether the storage medium is reporting a false capacity and attempts to determine the real capacity. USB sticks that claim too large a capacity can be quickly identified with this, hence this is the recommended test type."));
    lbl_quick2->setWordWrap(true);
    lbl_quick2->setToolTip(tr("If fake sticks, which claim more capacity than they actually have, are written beyond their real capacity, the written data is lost and the file system on the stick can even be damaged in some cases. Instead of completely filling the storage medium, it writes specifically at certain positions to determine whether the data can be read back."));
    vbox->addWidget(lbl_quick2);
    QLabel *lbl_quick3 = new QLabel(tr("This test will only take a few minutes."));
    lbl_quick3->setWordWrap(true);
    vbox->addWidget(lbl_quick3);
    QLabel *lbl_quick4 = new QLabel(tr("This test is specifically designed to detect fake USB sticks, which report the wrong capacity. It's not suited for genuine devices experiencing data corruption due to age or other defects."));
    lbl_quick4->setWordWrap(true);
    vbox->addWidget(lbl_quick4);
    connect(opt_quick, &QAbstractButton::toggled, this, [=](bool checked)
    {
        m_test_type = 1;
    });
    opt_quick->setChecked(true);

    //Full test
    //: Full test [method] means a full test of the storage device.
    QRadioButton *opt_full = new QRadioButton(tr("Full test"));
    vbox->addWidget(opt_full);
    QLabel *lbl_full1 = new QLabel(tr("This test is more thorough and takes longer. The entire storage medium is filled with data, and then the data is read back to confirm that it matches the original data."));
    lbl_full1->setWordWrap(true);
    vbox->addWidget(lbl_full1);
    //: "this" refers to this test method (full test) in the following sentence.
    QLabel *lbl_full2 = new QLabel(tr("Use this if your device is not a typical fake but may have some other defects."));
    lbl_full2->setWordWrap(true);
    vbox->addWidget(lbl_full2);
    connect(opt_full, &QAbstractButton::toggled, this, [=](bool checked)
    {
        m_test_type = 2;
    });

    //Space
    vbox->addStretch();

    m_step = 3;
}

void
CapacityTesterGui::showDiskTestStart()
{
    //Clear main layout
    QVBoxLayout *vbox = resetMainLayout();

    //Header
    QLabel *lbl_head = new QLabel(tr("Start the disk test now?"));
    lbl_head->setWordWrap(true);
    vbox->addWidget(lbl_head);
    //---
    QShortcut *shortcut = new QShortcut(QKeySequence("Escape"), lbl_head);
    connect(shortcut, &QShortcut::activated, this, &CapacityTesterGui::showDevControls);

    //Note
    QLabel *lbl_note = new QLabel(tr("The disk test will overwrite all data on the device."));
    lbl_note->setWordWrap(true);
    vbox->addWidget(lbl_note);

    //Space
    vbox->addStretch();

    m_step = 4;
}

void
CapacityTesterGui::startDiskTest()
{
    Debug(QS("starting disk test for device: %s", CSTR(m_dev_path)));

    //Clear main layout
    QVBoxLayout *vbox = resetMainLayout();

    //Add cancel button
    if (!m_btn_cancel)
    {
        m_btn_cancel = m_button_box->addButton(QDialogButtonBox::Cancel);
        connect(m_btn_cancel, SIGNAL(clicked()), this, SLOT(stopDiskTest()));
    }
    m_btn_cancel->setEnabled(true);
    m_btn_cancel->setVisible(true);

    //Frame with overall test progress
    QFrame *frame_test = new QFrame;
    vbox->addWidget(frame_test);
    QVBoxLayout *vbox_test = new QVBoxLayout;
    frame_test->setLayout(vbox_test);
    QGridLayout *grid_test_frame = new QGridLayout;
    vbox_test->addLayout(grid_test_frame);
    m_lbl_phase = new QLabel(tr("INITIALIZING"));
    QFont h_font;
    h_font = m_lbl_phase->font();
    h_font.setWeight(QFont::Bold);
    m_lbl_phase->setFont(h_font);
    m_lbl_icon = new QLabel;
    QHBoxLayout *hbox_phase_icon = new QHBoxLayout;
    hbox_phase_icon->addWidget(m_lbl_phase);
    //hbox_phase_icon->addStretch();
    hbox_phase_icon->addWidget(m_lbl_icon, 0, Qt::AlignRight);
    grid_test_frame->addLayout(hbox_phase_icon, 0, 0, 1, 2);
    //grid_test_frame->addWidget(m_lbl_phase, 0, 0, 1, 2);
    QLabel *lbl_test_time = new QLabel(tr("Time elapsed:"));
    grid_test_frame->addWidget(lbl_test_time, 2, 0);
    m_lbl_time_elapsed = new QLabel;
    grid_test_frame->addWidget(m_lbl_time_elapsed, 2, 1);
    QLabel *lbl_test_estimate = new QLabel(tr("Estimated time remaining:"));
    grid_test_frame->addWidget(lbl_test_estimate, 3, 0);
    m_lbl_time_estimate = new QLabel;
    grid_test_frame->addWidget(m_lbl_time_estimate, 3, 1);
    m_progress = new QProgressBar;
    QLabel *lbl_speed = new QLabel(tr("Speed:"));
    grid_test_frame->addWidget(lbl_speed, 4, 0);
    m_lbl_speed = new QLabel;
    grid_test_frame->addWidget(m_lbl_speed, 4, 1);
    QObject *scope_widget = m_progress;
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    grid_test_frame->addWidget(m_progress, 5, 0, 1, 2);
    //Prepare timer
    m_tmr_time = new QTimer(m_progress);
    connect(m_tmr_time, SIGNAL(timeout()), this, SLOT(updateTime()));
    m_tmr_time->start(1000); //1 second interval
    //Checkbox to format when done IDEA
    //QCheckBox *chk_format = new QCheckBox(tr("Format device when done"));
    //vbox_test->addWidget(chk_format);

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

    //Prepare frame with result information
    m_grp_result = new QGroupBox(tr("Test results"));
    vbox->addWidget(m_grp_result);
    QSizePolicy sp_retain = m_grp_result->sizePolicy();
    sp_retain.setRetainSizeWhenHidden(true);
    m_grp_result->setSizePolicy(sp_retain);
    QVBoxLayout *vbox_result = new QVBoxLayout;
    m_grp_result->setLayout(vbox_result);
    m_lbl_result = new QLabel;
    m_lbl_result->setWordWrap(true);
    vbox_result->addWidget(m_lbl_result);
    //make label bold, highlight result text and frame
    QFont bold_font;
    bold_font = m_lbl_result->font();
    bold_font.setWeight(QFont::Bold);
    m_lbl_result->setFont(bold_font);
    m_grp_result->setStyleSheet("QGroupBox { border: 2px solid orange; border-radius: 5px; margin-top: 1.5em; }");
    m_grp_result->hide();
    //Frame with format hint and format button
    m_grp_format = new QGroupBox();
    m_grp_format->setVisible(false);
    vbox_result->addWidget(m_grp_format);
    QHBoxLayout *hbox_format = new QHBoxLayout;
    m_grp_format->setLayout(hbox_format);
    QLabel *lbl_format = new QLabel(tr("You should format the device to make it usable again."));
    lbl_format->setWordWrap(true);
    hbox_format->addWidget(lbl_format);
    QPushButton *btn_format = new QPushButton(tr("Format device"));
    //btn_format->setEnabled(false);
    hbox_format->addWidget(btn_format);
    connect(btn_format, &QAbstractButton::clicked, this, [=]()
    {
        //formatDevice(); show dialog
        showFormatTool(true); //start formatting now
    });


    //Worker control logic below

    //Initialize worker, not started immediately
    //the create function connects the typical thread signals, our signals are connected below
    Debug(QS("setting up worker..."));
    m_dd_worker = createDiskTestWorker(); //schedule start
    //m_dd_worker = createDiskTestWorker(false); //TODO debugging
    if (!m_dd_worker)
    {
        //Error
        //m_step = 0;
        return;
    }
    Debug(QS("connecting worker..."));
    m_wr_err = -1;
    m_rd_err = -1;
    m_ddt_phase = 0;
    m_total = 0; //set via started signal
    m_wr = 0;
    m_rd = 0;
    m_cur_speed = 0;
    //Connect signals; init, write/read progress, finished
    //init/started signal tells us the worker is ready and about to start writing
    connect(m_dd_worker, SIGNAL(started(qint64)),
            this, SLOT(startedTest(qint64)));
    connect(m_dd_worker, SIGNAL(startFailed(QString)),
            this, SLOT(startFailed(QString)));

    connect(m_dd_worker.data(), SIGNAL(writeFailed(qint64)),
            this, SLOT(writeFailed(qint64)));
    connect(m_dd_worker.data(), SIGNAL(verifyFailed(qint64)),
            this, SLOT(readFailed(qint64)));
    //Show speed in graph
    //Display graph while writing/reading
    //Lambda callbacks may never be called if worker is wrapper!
    //connect(m_dd_worker.data(), &DestructiveDiskTester::written...
    //Therefore we use old style signal/slot syntax
    connect(m_dd_worker.data(), SIGNAL(written(qint64, double)),
            this, SLOT(written(qint64, double)));
    connect(m_dd_worker.data(), SIGNAL(verified(qint64, double)),
            this, SLOT(verified(qint64, double)));
    //Delete when navigating away (the progress widget serves as scope container)
    //this will also remove the cancel button, which is visible for the lifetime of the worker
    connect(m_progress, &QObject::destroyed, [=]()
    {
        if (m_dd_worker && m_dd_worker->thread() != this->thread())
        {
            Debug("widget closed - terminating worker thread...");
            m_dd_worker->thread()->quit();
            m_dd_worker->thread()->wait();
        }
        else if (m_dd_worker)
        {
            Debug("widget closed - deleting wrapper for worker process...");
            m_dd_worker->deleteLater();
        }
    });

    //Configure worker
    if (m_test_type == 1)
    {
        //Quick mode, no performance graph
    }
    else if (m_test_type == 2)
    {
        //Set test option
        Debug(QS("setting full mode..."));
        m_dd_worker->setFullMode(); //not executed in wrapper
    }
    //NOTE speed is not transmitted through wrapper

    //Connect finished signal
    connect(m_dd_worker.data(), SIGNAL(finished(int)),
            this, SLOT(handleTestFinished(int)));
    connect(m_dd_worker.data(), SIGNAL(destroyed()),
            m_btn_cancel, SLOT(hide())); //hide cancel button when worker is gone

    //Disable back, next buttons
    m_btn_back->setEnabled(false);
    m_btn_next->setEnabled(false);
    //NOTE if worker (wrapper) crashes, we're stuck with disabled buttons


    m_step = 5;
}

void
CapacityTesterGui::stopDiskTest()
{
    if (!m_dd_worker)
    {
        Debug("no worker to stop");
        return;
    }
    //Ask
    bool is_second_attempt = m_ddt_phase == 5; //first cancel gracefully, second kill
    if (!is_second_attempt)
    {
        if (QMessageBox::question(this, tr("Stop test process?"),
            tr("Do you want to stop the disk test?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
    }
    //else if (QMessageBox::question(this, tr("Stop disk test?"),
    //    tr("The disk test could not be stopped gracefully. Do you want to terminate the test?"),
    //    QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) //no yes until implemented
    //    return;
    else if (QMessageBox::question(this, tr("Stop test process?"),
        //: "gracefully" may be skipped in the translation or translated as "normally"; "terminate" means "to kill" in this context (to force the process to stop).
        tr("The disk test could not be stopped gracefully. Do you want to terminate the test?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    Debug("stopping disk test...");
    m_lbl_phase->setText(tr("STOPPING"));
    m_ddt_phase = 5;

    //Request stop
    //NOTE m_dd_worker is usually the worker but it may be a wrapper!
    //The wrapper merely starts the worker in a new process (as root)
    //and controls it via stdin/stdout, so if the worker (process) is stuck,
    //we cannot stop it directly. Stopping the wrapper will not stop the worker.
    //Instead, we should set up + enable a timeout in the worker to terminate itself.
    //NOTE BUG if cancel() is called on wrapper (which inherits from worker),
    //the wrong (base) method will be executed, which crashes the wrappers.
    //To avoid this horrible mixup, either use signal/slot connections (old style!)
    //or make sure the derived method is called.
    if (!is_second_attempt)
    {
        m_dd_worker->cancel();
        //start timeout (to terminate worker), but timer must be stopped if worker finishes
        //and it must be stopped if this dialog widget is closed (otherwise it will crash)
    }
    else
    {
        m_dd_worker->cancel(true);
        if (m_btn_cancel) m_btn_cancel->setEnabled(false);
    }
    //TODO add option to kill worker after timeout, maybe terminate thread

    //Progress widgets stay for now, but are disabled; cleared by event handler after worker finishes

    m_step = 6;
}

void
CapacityTesterGui::updateTime()
{
    if (!m_dd_worker)
    {
        Debug("no worker to update time");
        return;
    }
    if (m_ddt_phase == 0)
    {
        //Initialization
        m_lbl_time_elapsed->setText(tr("Initializing..."));
        m_lbl_time_estimate->setText(tr("Initializing..."));
        return;
    }

    //Display elapsed time
    //qint64 elapsed = m_dd_worker->elapsed(); //we'd rather not bother the worker while it's running
    if (!m_tmr_elapsed.isValid()) return; //timer not started yet
    qint64 elapsed = m_tmr_elapsed.elapsed() / 1000;
    int elapsed_days = elapsed / 86400;
    //QTime time = QTime::fromSecondsSinceStartOfDay(elapsed % 86400);
    //QString elapsed_str = (elapsed_days > 0 ? QString::number(elapsed_days) + " days " : "") + time.toString("hh:mm:ss");
    //QString elapsed_str = QTime(0, 0).addSecs(elapsed).toString("hh:mm:ss");
    //alternative
    QString elapsed_str = QString("%1:%2:%3")
        .arg((elapsed / 60 / 60), 2, 10, QChar('0'))
        .arg((elapsed / 60) % 60, 2, 10, QChar('0'))
        .arg(elapsed % 60, 2, 10, QChar('0'));
    m_lbl_time_elapsed->setText(elapsed_str);

    //Estimate and display remaining time
    //Speed is calculated based on the progress and elapsed time
    //We simply add written + read and double total size assuming the same speed
    qint64 processed_size = m_wr + m_rd; // MB
    double average_speed = processed_size / static_cast<double>(elapsed); // MB/s
    //It would be more accurate to calculate the speed based on the last few seconds, see handlers below
    average_speed = m_cur_speed;
    qint64 remaining_size = m_total * 2 - processed_size;
    qint64 estimated_time = remaining_size / average_speed; // seconds
    QString remaining_str = "";
    int remaining_days = estimated_time / 86400;
    //QTime remaining_time = QTime::fromSecondsSinceStartOfDay(estimated_time % 86400);
    //remaining_str = (remaining_days > 0 ? QString::number(remaining_days) + " days " : "") + remaining_time.toString("hh:mm:ss");
    remaining_str = QString("%1:%2:%3")
        .arg((estimated_time / 60 / 60), 2, 10, QChar('0'))
        .arg((estimated_time / 60) % 60, 2, 10, QChar('0'))
        .arg(estimated_time % 60, 2, 10, QChar('0'));
    if (estimated_time < 0)
        remaining_str = tr("?");
    m_lbl_time_estimate->setText(remaining_str);
    Debug(QS("elapsed: %lld s, speed: %f MB/s, remaining: %lld s", elapsed, average_speed, estimated_time));

    //Progress
    int progress = 100 * (processed_size) / (m_total * 2); //0-100
    m_progress->setValue(progress);
    //Debug(QS("progress: %d%% (processed: %lld MB, total: %lld MB)", progress, processed_size, m_total * 2));
}

void
CapacityTesterGui::startFailed(const QString &error_msg)
{
    Debug(QS("start failed: %s", CSTR(error_msg)));
    m_lbl_phase->setText(tr("FAILED TO START"));
    //m_lbl_time_elapsed->setText(tr("FAILED"));
    //m_lbl_time_estimate->setText(tr("FAILED"));
    QMessageBox::critical(this, tr("Failed to start disk test"),
        tr("The disk test could not be started: %1").arg(error_msg));

    //Enable back button
    m_btn_back->setEnabled(true);
    m_btn_cancel->setEnabled(false);

    //Delete worker by stopping its thread (if it's still running), making sure we're not stopping our own thread
    //if (m_dd_worker && m_dd_worker->thread() != thread())
    //{
    //    m_dd_worker->thread()->quit(); //see destructor lambda for cleanup
    //}
    //m_dd_worker.clear();
    m_step = 4;

}

void
CapacityTesterGui::startedTest(qint64 size)
{
    Debug(QS("started disk test for device: %s", CSTR(m_dev_path)));
    //Set counters
    //note there's also m_claimed_capacity but it is determined through another method
    //for the progress, we use the size as reported by the DDT worker
    m_total = size; //total size in MB
    m_wr = 0; //written / progress in MB
    m_rd = 0; //read / progress in MB
    m_wr_err = -1; //error position in MB
    m_rd_err = -1; //error position in MB
    m_ddt_phase = 0; //0=init, 1=write, 2=read, 3=done, 4=failed
    //Start timer
    if (m_tmr_elapsed.isValid()) m_tmr_elapsed.invalidate(); //reset timer
    m_tmr_elapsed.start();

    m_tmr_time->start(1000); //1 second interval
    m_progress->setValue(0);
    //m_graph_write->clear(); //TODO
    //m_graph_read->clear(); //TODO
    m_lbl_phase->setText(tr("INITIALIZING"));
    m_lbl_time_elapsed->setText("00:00:00");
}

void
CapacityTesterGui::startTestPhase(int phase)
{
    Debug(QS("start test phase: %d", phase));
    if (phase == 0)
    {
        m_lbl_phase->setText(tr("INITIALIZING"));
    }
    else if (phase == 1)
    {
        m_lbl_phase->setText(tr("WRITING"));
        m_grp_graph_write->setVisible(true);
        m_tmr_elapsed_current.invalidate();
        m_tmr_elapsed_current.start();
    }
    else if (phase == 2)
    {
        m_grp_graph_write->setVisible(false);
        m_lbl_phase->setText(tr("VERIFYING"));
        m_grp_graph_read->setVisible(true);
        m_tmr_elapsed_current.invalidate();
        m_tmr_elapsed_current.start();
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
CapacityTesterGui::written(qint64 pos, double avg_speed, double current_speed)
{
    //If current speed is 0, use average speed
    if (current_speed == 0)
        current_speed = avg_speed;

    //Set data point in graph
    if (m_ddt_phase == 0)
    {
        startTestPhase(1);
    }
    m_graph_write->addPoint(pos, current_speed);
    Debug(QS("written: %lld MB, speed: %f MB/s", pos, current_speed));
    if (current_speed > 0)
        m_lbl_speed->setText(QString("%1 MB/s").arg(current_speed, 0, 'f', 1));
    //NOTE we wanted to display the speed in a progress bar but some themes don't support it

    //Calculate current speed NOTE should be done by worker, see argument, but isn't...
    //3 second avg speed (TODO calculate in worker)
    double speed = 0;
    if (m_tmr_elapsed_current.isValid())
    {
        qint64 last_pos = m_wr;
        qint64 diff = pos - last_pos;
        double elapsed = m_tmr_elapsed_current.elapsed() / 1000;
        if (elapsed > 3)
        {
            speed = (pos - last_pos) / elapsed;
            m_tmr_elapsed_current.restart();
        }
    }
    if (speed > 0)
        m_cur_speed = speed;

    //Update progress
    m_wr = pos;

}

void
CapacityTesterGui::verified(qint64 pos, double avg_speed, double current_speed)
{
    //If current speed is 0, use average speed
    if (current_speed == 0)
        current_speed = avg_speed;

    //Set data point in graph
    if (m_ddt_phase == 1)
    {
        startTestPhase(2);
    }
    m_graph_read->addPoint(pos, current_speed);
    Debug(QS("verified: %lld MB, speed: %f MB/s", pos, current_speed));
    if (current_speed > 0)
        m_lbl_speed->setText(QString("%1 MB/s").arg(current_speed, 0, 'f', 1));

    //Calculate current read speed
    double speed = 0;
    if (m_tmr_elapsed_current.isValid())
    {
        qint64 last_pos = m_rd;
        double elapsed = m_tmr_elapsed_current.elapsed() / 1000;
        if (elapsed > 3)
        {
            speed = (pos - last_pos) / elapsed;
            m_tmr_elapsed_current.restart();
        }
    }
    if (speed > 0)
        m_cur_speed = speed;

    //Update progress
    m_rd = pos;

}

void
CapacityTesterGui::writeFailed(qint64 pos)
{
    m_wr_err = pos;
}

void
CapacityTesterGui::readFailed(qint64 pos)
{
    m_rd_err = pos;
}

void
CapacityTesterGui::handleTestFinished(int result)
{
    double size_g = Size(m_claimed_capacity).gb();
    bool success = result == 0;
    Debug(QS("test finished, success: %d", success));

    //Stop counting elapsed, remaining time etc.
    m_tmr_time->stop();
    m_lbl_time_estimate->clear();
    m_progress->setValue(100);
    m_grp_graph_write->setVisible(false);
    m_grp_graph_read->setVisible(false);

    //TODO hide cancel button
    //QPushButton *btn_cancel = m_button_box->button(QDialogButtonBox::Cancel);
    //if (btn_cancel) btn_cancel->hide();

    m_grp_result->show();
    if (success)
    {
        //No error found - test finished successfully
        m_lbl_phase->setText(tr("DONE - NO ERRORS"));
        m_lbl_icon->setPixmap(QPixmap(":/check-mark-96.png"));
        m_lbl_icon->setToolTip(tr("Test completed successfully"));
        //m_lbl_icon->setFixedSize(24, 24);
        m_lbl_icon->setScaledContents(true);

        //Different summary text depending on test type
        if (m_test_type == 1)
        {
            //Fast mode
            m_lbl_result->setText(tr("This storage device appears to be genuine. This storage device has a capacity of %1 GB, all of which is usable.").arg(size_g, 0, 'f', 1));
        }
        else
        {
            //Full mode
            //may be used to test a bad drive which does not lie about its capacity
            m_lbl_result->setText(tr("This storage device is genuine. This storage device has a capacity of %1 GB, all of which is usable.").arg(size_g, 0, 'f', 1));
        }

        //Add option to format device
        m_grp_format->setVisible(true);

    }
    else if (result < 0)
    {
        //Test was aborted, either by user or due to an unexpected error
        m_lbl_phase->setText(tr("TEST ABORTED"));
        m_lbl_icon->setPixmap(QPixmap(":/cross-mark-96.png"));
        m_lbl_icon->setToolTip(tr("Test aborted"));
        //m_lbl_icon->setFixedSize(24, 24);
        m_lbl_icon->setScaledContents(true);

        m_lbl_result->setText(tr("The disk test was aborted. No results available."));
    }
    else
    {
        //Bad disk detected - possibly fake or damaged
        //Most likely a fake if the verification showed a checksum mismatch
        bool verify_mismatch = result == DestructiveDiskTester::Error::VerifyMismatch;
        m_lbl_phase->setText(tr("DEFECTIVE STORAGE DEVICE")); //TODO rephrase?
        if (verify_mismatch)
            m_lbl_phase->setText(tr("POSSIBLE COUNTERFEIT STORAGE DEVICE"));
        m_lbl_icon->setPixmap(QPixmap(":/cross-mark-96.png"));
        m_lbl_icon->setToolTip(tr("Test failed"));
        //m_lbl_icon->setFixedSize(24, 24);
        m_lbl_icon->setScaledContents(true);
        //Error position in MB
        qint64 err_at_m = m_wr_err > -1 ? m_wr_err : m_rd_err; //> -1 M (pos)
        qint64 err_at_g = 0;
        if (err_at_m > -1) //if -1, failed signal with position didn't work
        {
            err_at_g = err_at_m / 1024; //std::div(m, 1024).quot
        }
        //err_at_g should be a positive offset, but see below

        //Different summary text depending on test type
        //because fast mode detects fake capacity
        //and full mode detects any damage(d region)
        QString err_msg;
        if (verify_mismatch) //VerifyMismatch
            //: Translate: "data that did not match..." may be translated as "wrong data" or "incorrect data". "silently corrupting data" means damaging data without notifying the user.
            err_msg = tr("After %1 GB, the device returned data that did not match what was written during the test. This indicates the device is silently corrupting data without reporting any errors.").arg(err_at_g);
        else if (result == DestructiveDiskTester::Error::Prep)
            err_msg = tr("This storage device could not be prepared for testing.");
        else if (result == DestructiveDiskTester::Error::Open)
            err_msg = tr("This storage device could not be opened.");
        else if (result == DestructiveDiskTester::Error::SeekWrite)
            //: Translator: Do not translate "seek" (technical term)
            err_msg = tr("Failed to write to device (seek error) after %1 GB.").arg(err_at_g);
        else if (result == DestructiveDiskTester::Error::SeekRead)
            //: Translator: Do not translate "seek" (technical term)
            err_msg = tr("Failed to read from device (seek error) after %1 GB.").arg(err_at_g);
        else if (result == DestructiveDiskTester::Error::WriteData)
            err_msg = tr("This storage device could not be written to after %1 GB.").arg(err_at_g);
        else if (result == DestructiveDiskTester::Error::ReadData)
            err_msg = tr("Failed to read from device after %1 GB.").arg(err_at_g);
        else if (result == DestructiveDiskTester::Error::ReadEmpty)
            //: Translator: It unexpectedly returned empty/nothing when reading a (non-empty) section of the device
            err_msg = tr("This storage device returned empty data after %1 GB.").arg(err_at_g);
        else //GenericFail
            err_msg = tr("An error occurred after %1 GB.").arg(err_at_g);

        if (m_test_type == 1)
        {
            //Fast mode
            m_lbl_result->setText(err_msg + " " + tr("This storage device claims to have a capacity of %1 GB, but only the first %2 GB are usable.").arg(size_g, 0, 'f', 1).arg(err_at_g));
        }
        else
        {
            //Full mode
            m_lbl_result->setText(err_msg);
        }
        //Different message if error at the beginning
        if (err_at_g == 0)
        {
            m_lbl_phase->setText(tr("TEST FAILED"));
            //: Translator: Two things may have lead to this error: Either bogus data was returned at the beginning of the test, or something prevented the test from running.
            m_lbl_result->setText(tr("This storage device returned corrupted data at the beginning or something prevented the test."));
        }

    }

    //Enable back, next buttons
    if (m_btn_cancel) m_btn_cancel->setEnabled(false);
    m_btn_back->setEnabled(true);
    m_btn_next->setEnabled(false);

    m_step = 6;
}

void
CapacityTesterGui::showFormatTool(bool format_now)
{
    if (m_selected_dev.isEmpty())
    {
        showDevSelect();
        return;
    }
    QString dev_path = m_selected_dev;
    //Clear main layout
    QVBoxLayout *vbox = resetMainLayout();

    //Header
    QLabel *lbl_head = new QLabel(tr("Select a filesystem type and confirm to format the device."));
    lbl_head->setWordWrap(true);
    vbox->addWidget(lbl_head);
    QFormLayout *form = new QFormLayout;
    vbox->addLayout(form);
    //---
    QShortcut *shortcut = new QShortcut(QKeySequence("Escape"), lbl_head);
    connect(shortcut, &QShortcut::activated, this, &CapacityTesterGui::showDevControls); //prevStep()

    //Clear partition table
    //: Translator: "Clear" may be translated as "reset".
    QPushButton *btn_clear = new QPushButton(tr("Clear partition table"));
    btn_clear->setToolTip(tr("Clear the partition table on this device, removing all partitions. Before you can use the device again, you will need to create a new partition table and format the device. This will erase all data on the device."));
    form->addRow(tr("Clear device"), btn_clear);
    connect(btn_clear, &QAbstractButton::clicked, this, [=]()
    {
        //Clear partition table //TODO return code, success...?
        StorageDiskSelection::Device device = StorageDiskSelection().blockDevice(dev_path);
        if (!device.isValid())
        {
            QMessageBox::critical(this, tr("Error"),
                tr("The device is not valid."));
            return;
        }
        Debug(QS("clearing partition table on device: %s", CSTR(dev_path)));
        device.makePartitionTable(StorageDiskSelection::PartitionTableType::Unknown); //clear (Linux/UDisks2 only); optional convenience function
        if (device.makePartitionTable())
        {
            QMessageBox::information(this, tr("Partition table cleared"),
                tr("The partition table on this device has been cleared."));
        }
        else
        {
            QMessageBox::critical(this, tr("Error"),
                tr("Failed to clear partition table."));
        }
    });

    //Filesystem type
    QComboBox *cmb_fs = new QComboBox;
    cmb_fs->setEditable(false);
    form->addRow(tr("New filesystem"), cmb_fs); //filesystem for new partition
    //Get available filesystems
    QStringList fs_list_2;
#ifdef UDISKMANAGER_HPP
    fs_list_2 = UDiskManager().supportedFilesystems();
#else
    QLabel *lbl_unsupported = new QLabel(tr("This function is not available on this platform."));
    vbox->addWidget(lbl_unsupported);
    lbl_unsupported->setStyleSheet("color: red;");
    lbl_unsupported->setWordWrap(true);
#endif //UDISKMANAGER_HPP
    Debug(QS("supported filesystems: %s", CSTR(fs_list_2.join(", "))));
    //Move exfat and ext4 to the top (default selection should be exfat)
    foreach (QString type, QStringList() << "exfat" << "ext4")
    {
        int i = fs_list_2.indexOf(type);
        if (i == -1) continue; //skip if type not available
        cmb_fs->addItem(fs_list_2.takeAt(i));
    }
    cmb_fs->insertSeparator(cmb_fs->count());
    cmb_fs->addItems(fs_list_2);
    //Checkbox to make it world-writable?
    //: Translator: "Writable for everyone" means that all users, not just root, will be able to write to the device. This option exists because in some cases, after formatting, accessing the storage device causes permission errors.
    QCheckBox *chk_world_writable = new QCheckBox(tr("Writable for everyone"));
    chk_world_writable->setToolTip(tr("This will allow all users to write to the device."));
    chk_world_writable->setChecked(true); //always on (why would anyone create a filesystem on a device that's not writable except for root?)
    bool is_root = DestructiveDiskTester::amIRoot(); // (geteuid() == 0);
#if !defined(UDISKMANAGER_HPP)
    chk_world_writable->setEnabled(false); //TODO not yet implemented
#endif
    if (!is_root)
    {
        chk_world_writable->setEnabled(false);
        chk_world_writable->setToolTip(tr("This option is not available because the program is not running as root."));
    }
    form->addRow("", chk_world_writable);
    //Button
    QPushButton *btn_format = new QPushButton(tr("Format"));
    btn_format->setToolTip(tr("Format this device now, wiping all data on it. Afterward, you should be able to mount and use it again."));
    bool can_continue = fs_list_2.count() > 0;
    btn_format->setDisabled(!can_continue);
    form->addRow("", btn_format);

    //Handler
    connect(btn_format, &QAbstractButton::clicked, this, [=]()
    {
        QString fs_type = cmb_fs->currentText();
        if (fs_type.isEmpty()) return;
        Debug(QS("formatting device %s with filesystem %s", CSTR(dev_path), CSTR(fs_type)));
        bool reset_mbr = true;
        bool fix_perms = chk_world_writable->isChecked();

#ifdef UDISKMANAGER_HPP
        UDiskManager udisk;
        QString udisk_path = udisk.findDeviceDBusPath(dev_path);
        Debug(QS("internal udisk device path: %s", CSTR(udisk_path)));
        QApplication::setOverrideCursor(Qt::WaitCursor);
        if (reset_mbr)
        {
            if (!udisk.makeDiskLabel(udisk_path, "dos", true))
            {
                QMessageBox::critical(this, tr("Error"),
                    tr("Failed to reset the partition table."));
                QApplication::restoreOverrideCursor();
                return;
            }
        }
        QString message;
        bool ok = udisk.createPartition(udisk_path, fs_type, &message);
        Debug(QS("create partition: %d, message: %s", ok, CSTR(message)));
        if (ok && fix_perms)
        {
            if (!udisk.fixPartitionPermissions(udisk_path))
            {
                Debug(QS("failed to fix permissions on device %s", CSTR(dev_path)));
            }
        }
        if (!ok)
        {
            QMessageBox::critical(this, tr("Error"),
                tr("Failed to format device.\n%1").arg(message));
        }
        else
        {
            QMessageBox::information(this, tr("Success"),
                tr("The device has been formatted. You should now be able to mount and use it."));
        }
        QApplication::restoreOverrideCursor();
#else
        QMessageBox::critical(this, tr("Error"),
            tr("This function is not available on this platform."));
#endif //UDISKMANAGER_HPP
    });
    if (format_now)
    {
        QTimer::singleShot(0, btn_format, SLOT(click()));
    }

    m_btn_next->setEnabled(false);
    m_step = 20;
}

QVBoxLayout*
CapacityTesterGui::resetMainLayout()
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

DestructiveDiskTester*
CapacityTesterGui::createDiskTestWorker(bool schedule_start)
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
    bool is_wrapper = false;
    //This is for convenience, so the user doesn't have to restart the program as root
    //But it's not 100% reliable, because the user may not have sudo permissions
    if (!dd_worker->isWritable())
    {
#if !defined(Q_OS_WIN)
        //Question for sudo - disabled because too much text to read
        //QMessageBox::warning(this, tr("Disk test"),
        //    //: This hint is to inform the user that a password prompt will be triggered, usually asking for the sudo password (i.e., the user's password if s/he has sufficient sudo permissions to run this program as root). sudo does not have to be translated, it's a detail.
        //    tr("This program is running with limited privileges. An attempt will now be made to gain root privileges for this test, you may be asked for your sudo password."));
        //NOTE wrapper process may fail to start!
        dd_worker->deleteLater();
        //dd_wrapper = new DestructiveDiskTesterWrapper(m_dev_path);
        //dd_worker = dd_wrapper;
        dd_worker = new DestructiveDiskTesterWrapper(m_dev_path);
        is_wrapper = true;
#elif defined(Q_OS_WIN)
        QMessageBox::critical(this, tr("Disk test"),
            //: This warning is shown on Windows, so the term administrator is used. Alternative: Please start this program with elevated privileges.
            tr("This program is running with limited privileges. Try restarting the program as administrator."));
        dd_worker->deleteLater();
        return 0;
#endif
    }

    //NOTE BUG dd_worker is now a wrapper, so be careful with any methods
    //and slots which are reimplemented, specifically cancel().
    //If called the wrong way, the base method will be executed,
    //which crashes the wrapper (its thread will be quit, its qprocess terminated).
    //Our initial workaround was to use old style signal/slot connections (no lambdas),
    //(also see the reqStop signal),
    //but for now we've made the cancel function virtual.

    //Thread for worker
    //... after the check above, otherwise dd_worker (+ GUI) would get stuck.
    //The wrapper (for Linux to start without root) is a special case,
    //it starts the worker in a new process, so it should not be moved to a thread;
    //it would not be able to send the stop signal to the worker if running in a thread.
    QThread *thread = 0;
    if (!is_wrapper) thread = new QThread;
    if (thread) dd_worker->moveToThread(thread);
    //Here we connect the typical signals between worker and thread, see caller for more
    if (thread)
    {

        //Start worker when thread starts
        connect(thread,
                SIGNAL(started()),
                dd_worker,
                SLOT(start()));

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

    }
    else
    {
        //Delete worker when done
        connect(dd_worker,
                SIGNAL(finished(int)),
                dd_worker,
                SLOT(deleteLater())); //delete worker when done
    }

    //Stop worker on request //unused, see cancel()
    //connect(this,
    //        SIGNAL(reqStop()),
    //        dd_worker,
    //        SLOT(cancel()));

    //Delete on quit (GUI) //TODO thread->wait() ?
    //connect(this,
    //        SIGNAL(destroyed()),
    //        dd_worker,
    //        SLOT(deleteLater())); //delete worker on quit
    //Delete on quit - see caller, m_progress widget holds m_dd_worker

    //Start test in background
    if (schedule_start && !is_wrapper)
        QTimer::singleShot(0, thread, SLOT(start()));
    else if (schedule_start)
        QTimer::singleShot(0, dd_worker, SLOT(start()));

    return dd_worker;
}

/* Legacy module below */

VolumeTestGui::VolumeTestGui(QWidget *parent)
             : QWidget(parent),
               closing(false),
               req_remount(false),
               disable_precheck(false)
{
    //Layout items
    QVBoxLayout *vbox_main = new QVBoxLayout;
    QFrame *hline = 0;
    setLayout(vbox_main);

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
    txt_volume->setPlaceholderText(tr("Filesystem"));
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
    btn_start_volume_test = new QPushButton(tr("&Start")); //start VOLUME test
    connect(btn_start_volume_test,
            SIGNAL(clicked()),
            SLOT(startVolumeTest()));
    btn_stop_volume_test = new QPushButton(tr("Stop"));
    connect(btn_stop_volume_test,
            SIGNAL(clicked()),
            SLOT(stopTest()));
    btn_quit = new QPushButton(tr("&Quit"));
    btn_quit->hide(); //parent should have its own quit button
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
    bool udisk_support = false;
#ifdef UDISKMANAGER_HPP
    udisk_support = UDiskManager().isValid();
#endif
    if (!udisk_support)
    {
        act_toggle_remount->setEnabled(false);
    }

    //Initiate/unload view
    unloadVolume();

    //Button row
    QHBoxLayout *hbox_buttons = new QHBoxLayout;
    vbox_main->addLayout(hbox_buttons);
    hbox_buttons->addWidget(btn_start_volume_test);
    hbox_buttons->addWidget(btn_stop_volume_test);
    hbox_buttons->addWidget(btn_advanced);
    hbox_buttons->addWidget(btn_quit);

}

VolumeTestGui::VolumeTestGui(const QString &mountpoint, QWidget *parent)
             : VolumeTestGui(parent)
{
    loadVolume(mountpoint);

    btn_select_volume->hide();

}

QPixmap
VolumeTestGui::progressLightPixmap(const QColor &color)
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
VolumeTestGui::closeEvent(QCloseEvent *event)
{
    //Don't close while test running
    if (volume_worker)
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
        setEnabled(false);
        stopTest();

    }
    else
    {
        event->accept();
    }
}

void
VolumeTestGui::showDriveWindow()
{
    SelectionWindow *window = new SelectionWindow(this);
    window->setModal(true);
    window->show();
    connect(window, SIGNAL(volumeSelected(const QString&)), SLOT(loadVolume(const QString&)));
}

void
VolumeTestGui::toggleReqRemount(bool checked)
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
VolumeTestGui::toggleInitPrecheck(bool checked)
{
    disable_precheck = !checked;
}

void
VolumeTestGui::unloadVolume()
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
VolumeTestGui::setCapacityFields(VolumeTester &tester)
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
VolumeTestGui::loadVolume(const QString &mountpoint)
{
    //Unload
    unloadVolume();
    if (mountpoint.isEmpty()) return;
    Debug(QS("loading volume: %s", CSTR(mountpoint)));

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
VolumeTestGui::startVolumeTest()
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
    //later emit started to disable parent's back button

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

    //Delete worker when thread done (event loop stopped)
    connect(thread,
            SIGNAL(finished()),
            volume_worker,
            SLOT(deleteLater()));

    //Delete thread when worker gone
    connect(volume_worker,
            SIGNAL(destroyed()),
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
    //Delete on quit (GUI)
    connect(this,
            SIGNAL(destroyed()),
            volume_worker,
            SLOT(deleteLater())); //delete worker on quit
    connect(volume_worker, &QObject::destroyed, this, [=]()
    {
        Debug("Volume worker destroyed");
    });

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
    emit started();

}

void
VolumeTestGui::stopTest()
{
    //No action if no test running
    if (!volume_worker) return;

    //Update progress label
    lbl_pro_testing->setProperty("PHASE", tr("STOPPING"));

    //Disable button
    btn_stop_volume_test->setEnabled(false);

    //Request worker to stop
    emit reqStop();
    volume_worker->thread()->requestInterruption();

}

void
VolumeTestGui::succeededVolumeTest()
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
VolumeTestGui::failedVolumeTest(int error_type)
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
VolumeTestGui::completedVolumeTest()
{
    //Reset fields and values
    total_test_size = 0;

    //Start/stop buttons
    btn_start_volume_test->setEnabled(true);
    btn_stop_volume_test->setEnabled(false);
    emit finished();

    //Enable volume selection and other stuff
    btn_select_volume->setEnabled(true);
    btn_advanced->setEnabled(true);

    //Stop timer
    tmr_total_test_time.invalidate();

}

void
VolumeTestGui::updateProgressLabel()
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
VolumeTestGui::initializationStarted(qint64 total)
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
VolumeTestGui::initialized(qint64 size, double avg_speed)
{
    Q_UNUSED(avg_speed);

    //Initialized MB (progress)
    qint64 initialized_mb = size;
    pro_initializing->setValue(initialized_mb);

}

void
VolumeTestGui::writeStarted()
{
    //Test phase
    lbl_pro_testing->setProperty("PHASE", tr("WRITING"));
    lbl_pro_left_light->setPixmap(progressLightPixmap("orange"));
    lbl_pro_right_light->setPixmap(progressLightPixmap("orange"));

}

void
VolumeTestGui::verifyStarted()
{
    //Test phase
    lbl_pro_testing->setProperty("PHASE", tr("VERIFYING"));
    lbl_pro_left_light->setPixmap(progressLightPixmap("blue"));
    lbl_pro_right_light->setPixmap(progressLightPixmap("blue"));

}

void
VolumeTestGui::createFailed(int index, qint64 start)
{
    Q_UNUSED(index);
    Q_UNUSED(start);

    //Result - ERROR
    txt_result->setPlainText(tr("ACCESS ERROR!"));
    txt_result->setStyleSheet("background-color:#F2DEDE; color:#A94442;");

}

void
VolumeTestGui::writeFailed(qint64 start)
{
    //MB
    qint64 start_mb = start;

    //Result - ERROR
    txt_result->setPlainText(tr("WRITE ERROR AFTER %1 MB!").arg(start_mb));
    txt_result->setStyleSheet("background-color:#F2DEDE; color:#A94442;");

}

void
VolumeTestGui::verifyFailed(qint64 start)
{
    //MB
    qint64 start_mb = start;

    //Result - ERROR
    txt_result->setPlainText(tr("READ ERROR AFTER %1 MB!").arg(start_mb));
    txt_result->setStyleSheet("background-color:#F2DEDE; color:#A94442;");

}

void
VolumeTestGui::written(qint64 written, double avg_speed)
{
    //MB
    qint64 written_mb = written;
    pro_writing->setValue(written_mb);

    //Speed
    txt_write_speed->setText(QString("%1 MB/s").arg(avg_speed, 0, 'f', 1));

}

void
VolumeTestGui::verified(qint64 read, double avg_speed)
{
    //MB
    qint64 read_mb = read;
    pro_verifying->setValue(read_mb);

    //Speed
    txt_read_speed->setText(QString("%1 MB/s").arg(avg_speed, 0, 'f', 1));

}

void
VolumeTestGui::executeRemount(const QString &mountpoint)
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

