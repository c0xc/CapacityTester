#include "classlogger.hpp"

//__func__
//__PRETTY_FUNCTION__ is a GCC feature

/*
 * The joy of logging with sprintf.
 * My CSTR macro used to work fine (see also qUtf8Printable),
 * but as soon as I added one additional call/layer
 * by changing the macro to ClassLogger::c_str(x),
 * random characters would be logged instead:
 *
 * QString key = "902680f4a21842d2c18a4b4a69f4eb9d";
 * ...
 * c_str : qstring (902680f4a21842d2c18a4b4a69f4eb9d)c_str : qstring (psdblog)logging to: psdblog
 * qTOEST "TOEST1 record with key <T>..."
 * ...
 * The behavior appears to be random. After some additional tests,
 * the same code prints the full string correctly,
 * yet in the same call in the real module still only prints garbage.
 * It seems as if the temporary QByteArray object is destroyed
 * (invalidating the char* pointer) before the data pointer reaches QS()...
 *
 * <void Gui::FileRecord::loadForm()> : loading form for table u
 * ...
 * <void Gui::FileRecord::loadRecord()> : load key 902680f4a21842d2c18a4b4a69f4eb9d for file: /mnt/Share/_TEST_PICTURES/1/104-0477_IMG.JPG
 * <Model::FileRecord Model::FileTable::loadFileRecord(const QString&)> : searching for file record with key <s>...
 * <Model::FileRecord Model::FileTable::loadFileRecord(const QString&)> : found file record with key <f>
 * ...
 * It's all the same key variable.
 *
 * Test, this == QObject derived pointer:
 * qInfo()<<" in main"<< QString::asprintf("qTEST2(%s)", QStr(this).c_str());
 * qInfo()<<" in main"<< QStr(this).str();
 * >>>
 *  in main "qTEST2(0\u0000\u0000\u0000\u0000\u0000\u0000\u0000)"
 *  in main "0xc007e0"
 *
 * Could be they made the qPrintable macro to work inline for a reason...
 *
 * It works when storing the temporary QByteArray object in the QStr wrapper:
 * c_str() { m_qba = m_qstr.toUtf8(); return m_qba.constData();
 * ->
 * Debug(QS("TEST1 key <%s>...<%s>/<%s>... in main(%s)", CSTR(QString(key)), QStr(key).c_str(), CSTR2(key), CSTR2(this))); // main("0")
 * qInfo() << " in main"<< QString::asprintf("qTEST2(%s)", QStr(this).c_str());
 * ->
 * TEST1 key <902680f4a21842d2c18a4b4a69f4eb9d>...<902680f4a21842d2c18a4b4a69f4eb9d>/<902680f4a21842d2c18a4b4a69f4eb9d>... in main(0x11cd830)
 *  in main "qTEST2(0x11cd830)"
 * => So it really is a matter of life and death (of the QByteArray object).
 *
 * => Recommendation: Do not define any macro as func call to wrapper.
 * Use simple, direct macros - and migrate to QTextStream debugging!
 *
 */

/*
 * Do not use:
 *
 * static const char*
 * qptr_c_str(QObject *qobject_ptr); //BUG broken "0"
 *
 * //works, but prints "0" when passed as argument to asprintf()
 * QString str;
 * QTextStream stream(&str);
 * stream << qobject_ptr;
 *
 * See commentary above, use QStr wrapper class or stream op.
 */

// === === ===

void
ClassLogger::logMsg(const QString &msg, int level, const char *file, int line, const char *func)
{
    QString class_name = "::";
    if (QString(func).contains(class_name))
    {
        class_name = QString(func).section(class_name, 0, -2);
    }

    instance()->log(msg, level, file, line, func, class_name);

}

void
ClassLogger::log(const QString &msg, int level, const char *file, int line, const char *func, const QString &class_name)
{
    QString func_name = QString(func).section("::", -1);

    //Check log level
    QString level_name;
    switch (level)
    {
        case 10: level_name = "DEBUG"; break;
        case 20: level_name = "INFO"; break;
        case 30: level_name = "WARNING"; break;
        case 40: level_name = "ERROR"; break;
        case 50: level_name = "CRITICAL"; break;
    }

    //Filter (only accept log messages from specific areas)
    if (!m_rx_class.isEmpty())
    {
        if (!m_rx_class.exactMatch(class_name))
            return;
    }
    if (!m_rx_func.isEmpty())
    {
        if (!m_rx_func.exactMatch(func_name))
            return;
    }

    //Construct log string
    QString final_entry;
    if (!level_name.isEmpty())
        final_entry = QString("[%1]").arg(level_name);
    if (!final_entry.isEmpty())
        final_entry += " ";
    final_entry += QString("(L%1) <%2>").arg(line).arg(func); //TODO func vs. pretty
    final_entry += " : " + msg;
    if (!msg.endsWith('\n'))
        final_entry += '\n';

    //Write log entry
    //if (m_dst_file.isOpen())
    //{
    //    m_dst_file.write(final_entry.toUtf8());
    //    m_dst_file.flush();
    //}
    *m_txt_stream_ptr << final_entry.toUtf8();
    m_txt_stream_ptr->flush();

}

ClassLogger*
ClassLogger::instance()
{
    //ClassLogger *logger = 0;

    static std::unique_ptr<ClassLogger>
    _global_log;

    if (!_global_log)
    {
        //Initialize logger
        _global_log.reset(new ClassLogger);
        _global_log->m_env = QProcessEnvironment::systemEnvironment();
        //TODO public init() method

        if (_global_log->m_env.contains("CLOG_FILE"))
        {
            QString filepath = _global_log->m_env.value("CLOG_FILE");
            _global_log->initFile(filepath);
            //TODO CLOG_APPEND
        }

        if (_global_log->m_env.contains("CLOG_RX_CLASS"))
        {
            QString search = _global_log->m_env.value("CLOG_RX_CLASS");
            _global_log->m_rx_class.setPattern(search);
        }
        if (_global_log->m_env.contains("CLOG_RX_FUNC"))
        {
            QString search = _global_log->m_env.value("CLOG_RX_FUNC");
            _global_log->m_rx_func.setPattern(search);
        }

    }

    return _global_log.get();
}

ClassLogger::ClassLogger()
           : m_txt_stream_out(stdout), 
             m_txt_stream_ptr(&m_txt_stream_default)
{
}

void
ClassLogger::initFile(const QString &filepath)
{
    m_dst_file.close();
    //m_txt_stream_out = QTextStream(stdout);
    if (filepath == "-")
    {
        //Special file stdout, point stream writer directly to stdout
        m_txt_stream_ptr = &m_txt_stream_out;
    }
    else
    {
        //Open log file for writing
        m_dst_file.setFileName(filepath);
        if (m_dst_file.open(
            QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text |
            QIODevice::Unbuffered
        ))
        {
            printf("logging to: %s\n", CSTR(filepath));
        }
        else
        {
            printf("failed to open log file: %s\n", CSTR(filepath));
        }
        //Point stream writer to file
        m_txt_stream_ptr = &m_txt_stream_file;
        m_txt_stream_file.setDevice(&m_dst_file);
    }

}

