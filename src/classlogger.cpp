#include "classlogger.hpp"

//__func__
//__PRETTY_FUNCTION__ is a GCC feature

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
    //date
    QString ts_str = QDateTime::currentDateTime().toString("hh:mm:ss");
    final_entry += ts_str + " ";
    //log level
    if (!level_name.isEmpty())
        final_entry += QString("[%1]").arg(level_name);
    if (!final_entry.isEmpty())
        final_entry += " ";
    //function name
    final_entry += QString("(L%1) <%2>").arg(line).arg(func); //TODO func vs. pretty
    //log message
    final_entry += " : " + msg;
    if (!msg.endsWith('\n'))
        final_entry += '\n';

    //Write log entry
    if (m_dst_file.isOpen())
    {
        m_dst_file.write(final_entry.toUtf8());
        m_dst_file.flush();
    }

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
            //TODO check if it exists and if it's a text file already
            _global_log->m_dst_file.setFileName(filepath);
            if (_global_log->m_dst_file.open(
                QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text |
                QIODevice::Unbuffered
            ))
            {
                printf("logging to: %s\n", filepath.toUtf8().constData());
            }
            else
            {
                printf("failed to open log file: %s\n", filepath.toUtf8().constData());
            }

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


