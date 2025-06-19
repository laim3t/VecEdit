#include "mainwindow.h"
#include "common/logger.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QFile>

int main(int argc, char *argv[])
{
    // 检测并修正工作目录
    QString appPath = QFileInfo(argv[0]).absolutePath();
    QDir currentDir = QDir::current();
    QString currentPath = currentDir.absolutePath();

    // 检查当前目录是否是 build 目录
    if (currentPath.endsWith("build") || !QFile::exists("resources/qt/icons.qrc"))
    {
        // 尝试切换到上一级目录
        if (currentDir.cdUp())
        {
            QDir::setCurrent(currentDir.absolutePath());
            qDebug() << "main - 已将工作目录从" << currentPath << "切换到" << currentDir.absolutePath();
        }
    }

    Q_INIT_RESOURCE(icons); // 显式初始化 Qt 资源文件

    QApplication a(argc, argv);

    // 初始化日志系统
    Logger::instance().initialize(true, "logs/application.log", Logger::LogLevel::Debug, true); // Logger::LogLevel::Debug true为开启，false为关闭
    // Logger::instance().initialize(true, "logs/application.log");
    qDebug() << "main - 应用程序启动";
    qInfo() << "main - 欢迎使用VecEdit";

    MainWindow w;
    w.show();

    qDebug() << "main - 进入应用程序主循环";
    int result = a.exec();
    qDebug() << "main - 应用程序退出，返回码:" << result;

    return result;
}
