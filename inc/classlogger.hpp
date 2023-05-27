/****************************************************************************
**
** Copyright (C) 2023 Philip Seeger
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

#ifndef CLASSLOGGER_HPP
#define CLASSLOGGER_HPP

#include <memory> //std::unique_ptr

//#include <QtGlobal> //Q_FUNC_INFO
#include <QString>
#include <QRegExp>
#include <QFile>
#include <QProcessEnvironment>

#define QS(...) QString::asprintf(__VA_ARGS__)

#define Debug(x) ClassLogger::logMsg(x, 10, __FILE__, __LINE__, Q_FUNC_INFO)

//NOTE use Debug(QS(...)), don't forget QS() or else...

class ClassLogger
{
public:

    static void
    logMsg(const QString &msg, int level, const char *file, int line, const char *func);

    void
    log(const QString &msg, int level, const char *file, int line, const char *func, const QString &class_name);

    static ClassLogger*
    instance();

private:

    QProcessEnvironment
    m_env;

    QFile
    m_dst_file;

    QRegExp
    m_rx_class;

    QRegExp
    m_rx_func;

};

#endif
