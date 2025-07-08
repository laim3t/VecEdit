#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMutex>
#include <QMap>

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
     * @brief 日志级别枚举
     */
    enum class LogLevel
    {
        Debug,    // 调试级别，记录所有日志
        Info,     // 信息级别，不记录调试日志
        Warning,  // 警告级别，只记录警告、错误和致命错误
        Critical, // 错误级别，只记录错误和致命错误
        Fatal     // 致命错误级别，只记录致命错误
    };

    /**
     * @brief 获取Logger单例实例
     * @return Logger实例引用
     */
    static Logger &instance();

    /**
     * @brief 初始化日志系统
     * @param logToFile 是否将日志写入文件
     * @param logFilePath 日志文件路径，默认为"application.log"
     * @param level 日志级别，默认为Debug
     * @param enableConsole 是否创建日志控制台窗口，默认为true
     */
    void initialize(bool logToFile = false, const QString &logFilePath = "application.log", LogLevel level = LogLevel::Debug, bool enableConsole = true);
    // void initialize(bool logToFile = false, const QString &logFilePath = "application.log", LogLevel level = LogLevel::Debug, bool enableConsole = false);
    /**
     * @brief 设置日志级别
     * @param level 要设置的日志级别
     */
    void setLogLevel(LogLevel level);

    /**
     * @brief 获取当前日志级别
     * @return 当前设置的日志级别
     */
    LogLevel logLevel() const;

    /**
     * @brief 检查指定的消息类型是否应该被记录
     * @param type 消息类型
     * @return 如果应该记录则返回true，否则返回false
     */
    bool shouldLog(QtMsgType type) const;

    /**
     * @brief 设置指定功能的日志级别
     * @param modulePattern 模块名称或功能名称的匹配模式
     * @param level 要设置的日志级别
     */
    void setModuleLogLevel(const QString &modulePattern, LogLevel level);

    /**
     * @brief 获取模块的日志级别
     * @param moduleName 模块名称
     * @return 模块的日志级别
     */
    LogLevel moduleLogLevel(const QString &moduleName) const;

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
    LogLevel m_logLevel;
    QMap<QString, LogLevel> m_moduleLogLevels; // 模块特定的日志级别

    static QMutex m_mutex;
    static Logger *m_instance;
};

#endif // LOGGER_H