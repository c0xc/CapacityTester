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

#define DEFINE_GLOBALS
#include "main.hpp"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(PROGRAM);
    app.setApplicationVersion(GITVERSION);
    app.setWindowIcon(QPixmap(":/USB_flash_drive.png"));

    bool run_cli = false;
    #ifdef NO_GUI
    run_cli = true;
    #endif

    CapacityTesterCli *cli = 0;
    CapacityTesterGui *gui = 0;
    if (argc > 1 || run_cli)
    {
        cli = new CapacityTesterCli;
    }
    else
    {
        gui = new CapacityTesterGui;
        gui->show();
    }

    int code = app.exec();
    delete cli;
    delete gui;
    return code;
}

