#include "destructivedisktesterwrapper.hpp"

DestructiveDiskTesterWrapper::DestructiveDiskTesterWrapper(const QString &dev_path)
                            : DestructiveDiskTester(dev_path),
                              m_started(false),
                              m_finished(false)
{
    m_dev_path = dev_path;

    m_tmr_status = new QTimer(this);
    m_tmr_status->setInterval(100);
    connect(m_tmr_status, SIGNAL(timeout()), SLOT(checkStatus()));
    connect(this, SIGNAL(finished()), m_tmr_status, SLOT(stop()));

    //We expect the status output from the CLI in a specific format
    //progress: either match "progress" or leave out percentage elsewhere
    //to avoid confusion (progress rx matching error line)
    m_rx_start = QRegExp("^\\[start\\].*(total=(\\d+)M)");
    m_rx_start_err = QRegExp("^\\[start\\].*(failed)");
    m_rx_progress = QRegExp("^\\[(\\w+)\\].*progress.*(\\d+[.]\\d+)%.*\\s@(\\d+)M");
    m_rx_progress_err = QRegExp("^\\[(\\w+)\\].*failed.*@(\\d+)M");
    m_rx_finished = QRegExp("^\\[done\\] (success|failed)");

    //NOTE this wrapper is Linux-only, it uses pkexec (sudo)

}

DestructiveDiskTesterWrapper::~DestructiveDiskTesterWrapper()
{
    if (m_proc)
    {
        m_proc->kill();
    }
}

void
DestructiveDiskTesterWrapper::start()
{
#ifdef Q_OS_WIN
    emit startFailed();
    return;
#endif

    if (!QFile::exists(m_dev_path))
    {
        emit startFailed();
        return;
    }

    //Use pkexec which is the recommended wrapper tool to run something as root
    //pkexec is the official PolicyKit wrapper on Linux
    //for Win, see QProcess::startDetachedUacPrompt()
    m_proc = new QProcess(this);
    m_proc->setProgram("pkexec");

    //Set up new QProcess instance
    m_proc->setReadChannel(QProcess::StandardOutput);
    connect(m_proc,
            SIGNAL(started()),
            m_tmr_status,
            SLOT(start()));
    connect(m_proc,
            SIGNAL(finished(int, QProcess::ExitStatus)),
            SLOT(checkStateFinished(int, QProcess::ExitStatus)));
    connect(m_proc,
            SIGNAL(errorOccurred(QProcess::ProcessError)),
            SLOT(checkProcError(QProcess::ProcessError)));

    //Get file path to this executable - usually works
    //NOTE no guarantee, the file could've been removed!
    QString exe_path = QCoreApplication::applicationFilePath();
    if (!QFile::exists(m_dev_path))
    {
        emit startFailed();
        return;
    }
    //NOTE in theory, an attacker could replace the executable
    //which would then be started as root

    //Arguments for selected device
    QStringList args;
    args << exe_path;
    args << "--destructive-test";
    args << "--device" << m_dev_path;
    m_proc->setArguments(args);

    //Start process
    m_proc->start();
}

void
DestructiveDiskTesterWrapper::cancel()
{
    if (!m_proc) return;

    //Before terminating the process gracefully, we send a custom signal
    //This signal will make it close, quit
    //We cannot send SIGTERM/SIGKILL to the process if it's running as root!
    m_proc->write("QUIT\n"); //send custom quit signal
    m_proc->close(); //this alone may not work while the process is busy

}

void
DestructiveDiskTesterWrapper::checkStatus(const QString &line)
{

    QRegExp &rx_start = m_rx_start;
    QRegExp &rx_start_err = m_rx_start_err;
    QRegExp &rx_progress = m_rx_progress;
    QRegExp &rx_progress_err = m_rx_progress_err;
    QRegExp &rx_finished = m_rx_finished;
    if (rx_start.indexIn(line) != -1)
    {
        QString size_m_str = rx_start.cap(2);
        qint64 size_m = size_m_str.toLongLong();
        emit started(size_m);
    }
    else if (rx_start_err.indexIn(line) != -1)
    {
        QString failed_str = rx_start_err.cap(1);
        if (!failed_str.isEmpty())
        {
            emit startFailed();
        }
    }
    if (rx_progress.indexIn(line) != -1)
    {
        QString phase = rx_progress.cap(1);
        QString percent_str = rx_progress.cap(2);
        QString pos_m_str = rx_progress.cap(3);
        qint64 pos_m = pos_m_str.toLongLong();
        bool ok;
        double p = percent_str.toDouble(&ok);
        if (phase == "write")
        {
            emit written(pos_m, 0);
        }
        else if (phase == "verify")
        {
            emit verified(pos_m, 0);
        }
    }
    else if (rx_progress_err.indexIn(line) != -1)
    {
        QString phase = rx_progress_err.cap(1);
        QString pos_m_str = rx_progress_err.cap(2);
        qint64 pos_m = pos_m_str.toLongLong();
        if (phase == "write")
        {
            emit writeFailed(pos_m);
        }
        else if (phase == "verify")
        {
            emit verifyFailed(pos_m);
        }
    }
    else if (rx_finished.indexIn(line) != -1)
    {
        bool success = rx_finished.cap(1) == "success";
        //finish signal is not forwarded yet because process still running
        //instead we mark it as received, it will be forwarded later
        //see checkStateFinished()
        m_finished = success;
    }

    //Typical behavior with caching:
    //The process runs through 100% within seconds and says it's finished,
    //then blocks for a long time when closing.
    //When this happens, all write and read commands worked on the cache
    //which is flushed on close/exit. That flushing would fail
    //in case of a bad drive but then it's too late, it's asynchronous.

}

void
DestructiveDiskTesterWrapper::checkStatus()
{
    if (!m_proc->canReadLine()) return;

    if (m_started) m_started = true;

    QString line(m_proc->readLine());
    checkStatus(line);
}

void
DestructiveDiskTesterWrapper::checkStateFinished(int rc, QProcess::ExitStatus status)
{
    //Stop status timer which continuously reads from the process
    m_tmr_status->stop();

    //Read stderr and all the rest (in case we need it, in a destroyed/slot)
    //Actually, stderr might be useful for the user (GUI)
    //as it would contain whatever error message we got from the system
    //if the process could not be started
    m_err = m_proc->readAllStandardError();
    if (!m_err.isEmpty())
        emit gotError(m_err);

    //Process all remaining events, there's usually something left in stdout
    QByteArray raw_out_rest = m_proc->readAllStandardOutput();
    if (!raw_out_rest.isEmpty())
    {
        QTextStream reader(&raw_out_rest);
        while (!reader.atEnd())
        {
            checkStatus(reader.readLine());
        }
    }

    //Now we're ready to forward the finished signal
    //We don't want to forward it earlier because it's connected
    //to worker->deleteLater() which means the process instance
    //could be terminated before this finished slot is executed:
    //Destroyed while process is still running.
    bool proc_ok = status == QProcess::NormalExit && rc == 0;
    emit finished(proc_ok && m_finished);
}

void
DestructiveDiskTesterWrapper::checkProcError(QProcess::ProcessError error)
{
    if (error == QProcess::FailedToStart)
        emit startFailed();
}

