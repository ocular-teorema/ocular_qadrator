#ifndef ERRORHANDLER_H
#define ERRORHANDLER_H

#include <QFile>
#include <QMutex>
#include <QObject>

#define ERROR_MESSAGE0(T,O,M) \
    do { \
        char             str[1024]; \
        ErrorHandler*    pErrHandler = ErrorHandler::instance(); \
        snprintf(str, 1024, "%s", M); \
        pErrHandler->ErrorMessage(T, O, str); \
    } while (0)

#define ERROR_MESSAGE1(T,O,M,P1) \
    do { \
        char             str[1024]; \
        ErrorHandler*    pErrHandler = ErrorHandler::instance(); \
        snprintf(str, 1024, M, P1); \
        pErrHandler->ErrorMessage(T, O, str); \
    } while (0)

#define ERROR_MESSAGE2(T,O,M,P1,P2) \
    do { \
        char             str[1024]; \
        ErrorHandler*    pErrHandler = ErrorHandler::instance(); \
        snprintf(str, 1024, M, P1, P2); \
        pErrHandler->ErrorMessage(T, O, str); \
    } while (0)

#define ERROR_MESSAGE3(T,O,M,P1,P2,P3) \
    do { \
        char             str[1024]; \
        ErrorHandler*    pErrHandler = ErrorHandler::instance(); \
        snprintf(str, 1024, M, P1, P2, P3); \
        pErrHandler->ErrorMessage(T, O, str); \
    } while (0)

#define ERROR_MESSAGE4(T,O,M,P1,P2,P3,P4) \
    do { \
        char             str[1024]; \
        ErrorHandler*    pErrHandler = ErrorHandler::instance(); \
        snprintf(str, 1024, M, P1, P2, P3, P4); \
        pErrHandler->ErrorMessage(T, O, str); \
    } while (0)

#define ERROR_MESSAGE5(T,O,M,P1,P2,P3,P4,P5) \
    do { \
        char             str[1024]; \
        ErrorHandler*    pErrHandler = ErrorHandler::instance(); \
        snprintf(str, 1024, M, P1, P2, P3, P4, P5); \
        pErrHandler->ErrorMessage(T, O, str); \
    } while (0)

#ifdef _DEBUG

#define DEBUG_MESSAGE0(O,M) \
    do { \
        char             str[1024]; \
        ErrorHandler*    pErrHandler = ErrorHandler::instance(); \
        snprintf(str, 1024, "%s", M); \
        pErrHandler->DebugMessage(O, str); \
    } while (0)

#define DEBUG_MESSAGE1(O,M,P1) \
    do { \
        char             str[1024]; \
        ErrorHandler*    pErrHandler = ErrorHandler::instance(); \
        snprintf(str, 1024, M, P1); \
        pErrHandler->DebugMessage(O, str); \
    } while (0)

#define DEBUG_MESSAGE2(O,M,P1,P2) \
    do { \
        char             str[1024]; \
        ErrorHandler*    pErrHandler = ErrorHandler::instance(); \
        snprintf(str, 1024, M, P1, P2); \
        pErrHandler->DebugMessage(O, str); \
    } while (0)

#define DEBUG_MESSAGE3(O,M,P1,P2,P3) \
    do { \
        char             str[1024]; \
        ErrorHandler*    pErrHandler = ErrorHandler::instance(); \
        snprintf(str, 1024, M, P1, P2, P3); \
        pErrHandler->DebugMessage(O, str); \
    } while (0)


#define DEBUG_MESSAGE4(O,M,P1,P2,P3,P4) \
    do { \
        char             str[1024]; \
        ErrorHandler*    pErrHandler = ErrorHandler::instance(); \
        snprintf(str, 1024, M, P1, P2, P3, P4); \
        pErrHandler->DebugMessage(O, str); \
    } while (0)

#else
    #define DEBUG_MESSAGE0(O,M)
    #define DEBUG_MESSAGE1(O,M,P1)
    #define DEBUG_MESSAGE2(O,M,P1,P2)
    #define DEBUG_MESSAGE3(O,M,P1,P2,P3)
    #define DEBUG_MESSAGE4(O,M,P1,P2,P3,P4)
#endif

enum ErrorCode
{
    CAMERA_PIPELINE_OK,
    CAMERA_PIPELINE_ERROR
};

enum ErrorType
{
    ERR_TYPE_MESSAGE,       // Just log message
    ERR_TYPE_WARNING,       // Warning
    ERR_TYPE_DISPOSABLE,    // Kind of small error that doesn't affect whole pipeline
    ERR_TYPE_ERROR,         // Usual error
    ERR_TYPE_CRITICAL,      // Critical error that requires signalling to higher-level app
    MAX_ERR_TYPES
};

/*
 * Class for handling all types of errors and debug log messages
 * It can not only log these messages to file, but also we can add some kind of
 * signalling here, such error notification to other apllications (SWB, Admin panel, etc)
 * For example signalling can be implemented via http protocol or sockets
 */
class ErrorHandler : public QObject
{
    Q_OBJECT
public:
    static const char* DefaultLogFileName;

    static ErrorHandler* instance();
    ~ErrorHandler();

    ErrorCode  SetLogFile(QString logFileName);                                     /// Open file for dumping messages

    void ErrorMessage(ErrorType type, const char* ownerName, const char* message);  /// Error messages handling
    void DebugMessage(const char* ownerName, const char* message);                  /// Debug messages handling

signals:
    void CriticalError(QString err); /// Critical errors sometimes needs to be handled in other places

private:
    static ErrorHandler* m_instance;

    ErrorHandler();

    QMutex  m_mutex;
    QFile   m_logFile;
};

#endif // ERRORHANDLER_H
