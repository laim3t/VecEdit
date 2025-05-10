#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMutex>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

/**
 * @brief 日志处理类，用于处理Qt日志消息并输出到控制台和文件
 */
class Logger : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取Logger单例实例
     * @return Logger实例引用
     */
    static Logger &instance();

    /**
     * @brief 初始化日志系统
     * @param logToFile 是否将日志写入文件
     * @param logFilePath 日志文件路径，默认为"application.log"
     */
    void initialize(bool logToFile = false, const QString &logFilePath = "application.log");

private:
    explicit Logger(QObject *parent = nullptr);
    ~Logger();

    /**
     * @brief Qt消息处理函数
     */
    static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);

    QFile m_logFile;
    QTextStream m_logStream;
    bool m_logToFile;
    static QMutex m_mutex;
    static Logger *m_instance;
};

#endif // LOGGER_H