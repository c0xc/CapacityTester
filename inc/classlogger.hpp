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
#include <QTextStream>
#include <QProcessEnvironment>

#define CSTR(x) QStr(x).c_str()
#define CSTR0(x) qUtf8Printable(x)
#define CSTR2(d) QStr(d).c_str()

#define QS(...) QString::asprintf(__VA_ARGS__)

#define Debug(x) ClassLogger::logMsg(x, 10, __FILE__, __LINE__, Q_FUNC_INFO)

//NOTE use Debug(QS(...)), don't forget QS() or else...
//TODO DEBUG() << "text" << var << ptr; (stream op)

class QStr
{
public:
    QStr(const QString &qstring) { m_qstr = qstring; }
    QStr(QObject *qobject_ptr) { QTextStream(&m_qstr) << qobject_ptr; }
    QString
    str() { return m_qstr; }
    const char*
    c_str() { m_qba = m_qstr.toUtf8(); return m_qba.constData(); }
    //operator const char*()

private:
    QString m_qstr;
    QByteArray m_qba;
    QVariant m_v;
};

class ClassLogger
{
public:

    static void
    logMsg(const QString &msg, int level, const char *file, int line, const char *func);

    void
    log(const QString &msg, int level, const char *file, int line, const char *func, const QString &class_name);

    static ClassLogger*
    instance();

    ClassLogger();

private:

    void
    initFile(const QString &filepath);

    QProcessEnvironment
    m_env;

    QFile
    m_dst_file;

    QTextStream
    m_txt_stream_default, m_txt_stream_out, m_txt_stream_file;
    QTextStream
    *m_txt_stream_ptr = 0;
    //QTextStream
    //m_txt_stream;

    QRegExp
    m_rx_class;

    QRegExp
    m_rx_func;

};

#endif
