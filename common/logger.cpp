#include "logger.h"
#include <QDebug>
#include <QCoreApplication>
#include <iostream>
#include <QDir>
#include <windows.h>

QMutex Logger::m_mutex;
Logger *Logger::m_instance = nullptr;

Logger::Logger(QObject *parent) : QObject(parent), m_logToFile(false)
{
    qDebug() << "Logger::Logger - 日志处理器初始化";
}

Logger::~Logger()
{
    if (m_logFile.isOpen())
    {
        m_logStream.flush();
        m_logFile.close();
    }
    qDebug() << "Logger::~Logger - 日志处理器销毁";
}

Logger &Logger::instance()
{
    if (!m_instance)
    {
        QMutexLocker locker(&m_mutex);
        if (!m_instance)
        {
            m_instance = new Logger();
        }
    }
    return *m_instance;
}

void Logger::initialize(bool logToFile, const QString &logFilePath)
{
    m_logToFile = logToFile;

    if (m_logToFile)
    {
        // 确保日志目录存在
        QFileInfo fileInfo(logFilePath);
        QDir dir;
        if (!dir.exists(fileInfo.absolutePath()))
        {
            dir.mkpath(fileInfo.absolutePath());
        }

        m_logFile.setFileName(logFilePath);
        if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        {
            m_logStream.setDevice(&m_logFile);
            qDebug() << "Logger::initialize - 日志文件已打开:" << logFilePath;
        }
        else
        {
            qWarning() << "Logger::initialize - 无法打开日志文件:" << logFilePath;
            m_logToFile = false;
        }
    }

    // 在Windows上分配控制台
#ifdef Q_OS_WIN
    // 尝试附加到父进程的控制台
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
    {
        // 如果没有父控制台，则创建新的控制台
        AllocConsole();
    }

    // 设置控制台代码页为UTF-8
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // 重定向标准输出到控制台
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);

    // 设置控制台标题
    SetConsoleTitleW(L"VecEdit 日志控制台");

    // 设置控制台缓冲区大小
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    COORD bufferSize;
    bufferSize.X = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    bufferSize.Y = 9999; // 增大缓冲区高度，以便保存更多日志
    SetConsoleScreenBufferSize(hConsole, bufferSize);
#endif

    // 安装消息处理器
    qInstallMessageHandler(Logger::messageHandler);
    qDebug() << "Logger::initialize - 日志系统已初始化";
}

void Logger::messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // 获取当前时间
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");

    // 获取消息类型
    QString typeStr;
    switch (type)
    {
    case QtDebugMsg:
        typeStr = "Debug";
        break;
    case QtInfoMsg:
        typeStr = "Info";
        break;
    case QtWarningMsg:
        typeStr = "Warning";
        break;
    case QtCriticalMsg:
        typeStr = "Critical";
        break;
    case QtFatalMsg:
        typeStr = "Fatal";
        break;
    }

    // 格式化消息
    QString logMessage;

    // 如果启用了消息上下文（通过 QT_MESSAGELOGCONTEXT 宏）
    if (context.file && context.line && context.function)
    {
        QString file = QString(context.file).section('\\', -1); // 仅获取文件名，不包含路径
        logMessage = QString("[%1] [%2] %3 (%4:%5, %6)")
                         .arg(timestamp)
                         .arg(typeStr)
                         .arg(msg)
                         .arg(file)
                         .arg(context.line)
                         .arg(context.function);
    }
    else
    {
        logMessage = QString("[%1] [%2] %3")
                         .arg(timestamp)
                         .arg(typeStr)
                         .arg(msg);
    }

    QMutexLocker locker(&m_mutex);

    // 输出到控制台
#ifdef Q_OS_WIN
    // 在Windows上，使用控制台API直接输出
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    // 根据消息类型设置不同的颜色
    WORD attributes = FOREGROUND_INTENSITY;

    switch (type)
    {
    case QtDebugMsg:
        attributes |= FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // 白色
        break;
    case QtInfoMsg:
        attributes |= FOREGROUND_GREEN | FOREGROUND_BLUE; // 青色
        break;
    case QtWarningMsg:
        attributes |= FOREGROUND_RED | FOREGROUND_GREEN; // 黄色
        break;
    case QtCriticalMsg:
    case QtFatalMsg:
        attributes |= FOREGROUND_RED; // 红色
        break;
    }

    SetConsoleTextAttribute(hConsole, attributes);

    // 直接使用UTF-8输出
    std::cout << logMessage.toStdString() << std::endl;

    // 恢复默认颜色
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#else
    // 其他平台使用标准输出
    std::cout << logMessage.toStdString() << std::endl;
#endif

    // 如果启用了文件日志，也写入文件
    Logger &logger = Logger::instance();
    if (logger.m_logToFile && logger.m_logFile.isOpen())
    {
        logger.m_logStream << logMessage << Qt::endl;
        logger.m_logStream.flush();
    }

    // 如果是致命错误，退出应用程序
    if (type == QtFatalMsg)
    {
        abort();
    }
}