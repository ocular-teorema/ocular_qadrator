
#include <QTime>
#include "errorHandler.h"

ErrorHandler* ErrorHandler::m_instance = NULL;

const char* ErrorHandler::DefaultLogFileName = "ErrorLog.txt";

const char* ErrorTypeString[MAX_ERR_TYPES] = {"MESSAGE", "WARNING", "DISPOSABLE", "ERROR", "CRITICAL"};

ErrorHandler::ErrorHandler() :
    QObject(NULL),
    m_logFile(QString(DefaultLogFileName))
{
    if (!m_logFile.open(QIODevice::WriteOnly))
    {
        ErrorMessage(ERR_TYPE_ERROR, "ErrorHandler", "Unable to open error log file for writing");
    }
}

ErrorHandler::~ErrorHandler()
{
    m_logFile.close();
}

ErrorHandler* ErrorHandler::instance()
{
    if(!m_instance)
    {
        m_instance = new ErrorHandler();
    }
    return m_instance;
}

ErrorCode ErrorHandler::SetLogFile(QString logFileName)
{
    m_logFile.close();
    m_logFile.setFileName(logFileName);
    if (!m_logFile.open(QIODevice::WriteOnly))
    {
        ErrorMessage(ERR_TYPE_ERROR, "ErrorHandler", "Unable to open error log file for writing");
        return CAMERA_PIPELINE_ERROR;
    }
    return CAMERA_PIPELINE_OK;
}

void ErrorHandler::ErrorMessage(ErrorType errType, const char* ownerName, const char* message)
{
    QString errMsg;

    errMsg.sprintf("[%s %s]:%s: %s\n",
           ErrorTypeString[errType],
           QTime::currentTime().toString().toUtf8().data(),
           ownerName,
           message);

    m_mutex.lock();

    printf("%s", errMsg.toUtf8().data());
    fflush(stdout);

    if(m_logFile.isOpen())
    {
        m_logFile.write(errMsg.toUtf8().data());
        m_logFile.flush();
    }

    m_mutex.unlock();

    if (errType == ERR_TYPE_CRITICAL)
    {
        emit CriticalError(errMsg);
    }
}

void ErrorHandler::DebugMessage(const char* ownerName, const char* message)
{
    QString errMsg;

    errMsg.sprintf("[%s DEBUG]:%s: %s\n",
           QTime::currentTime().toString().toUtf8().data(),
           ownerName,
           message);

    m_mutex.lock();

    printf("%s", errMsg.toUtf8().data());
    fflush(stdout);

    if(m_logFile.isOpen())
    {
        m_logFile.write(errMsg.toUtf8().constData());
        m_logFile.flush();
    }

    m_mutex.unlock();
}

