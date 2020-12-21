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
    app.setApplicationVersion(APP_VERSION);
    QString program = QString(PROGRAM).toLower();

    //Initialize QTranslator, load default translation
    //To change the language manually, either set the environment variable
    //LANGUAGE, e.g., LANGUAGE=de ./bin/CapacityTester
    //or set the i18n variable: i18n=German ./bin/CapacityTester
    QTranslator translator_locale;
    QProcessEnvironment env(QProcessEnvironment::systemEnvironment());
    //capacitytester_german.qm
    if (env.contains("i18n") && translator_locale.load(program + "_" + env.value("i18n").toLower(), ":/i18n"))
        QCoreApplication::installTranslator(&translator_locale);
    //capacitytester_de.qm (qm files with locale suffix, e.g., _de)
    else if (translator_locale.load(QLocale(), program, "_", ":/i18n"))
        QCoreApplication::installTranslator(&translator_locale);
    //capacitytester_german.qm
    else if (translator_locale.load(program + "_" + QLocale::languageToString(QLocale().language()).toLower(), ":/i18n"))
        QCoreApplication::installTranslator(&translator_locale);

    //Decide whether to run in cli mode (headless) or not
    bool run_cli = false;
    if (qApp->platformName() == "offscreen") run_cli = true;
    #ifndef NO_GUI
    app.setWindowIcon(QPixmap(":/USB_flash_drive.png"));
    #else
    run_cli = true;
    #endif

    //Load application instance
    CapacityTesterCli *cli = 0;
    #ifndef NO_GUI
    CapacityTesterGui *gui = 0;
    #endif
    if (argc > 1 || run_cli)
    {
        cli = new CapacityTesterCli;
    }
    else
    {
        #ifndef NO_GUI
        gui = new CapacityTesterGui;
        gui->show();
        #endif
    }

    int code = app.exec();
    delete cli;
    #ifndef NO_GUI
    delete gui;
    #endif
    return code;
}

