#include "mainwindow.h"
#include "common/logger.h"

#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 初始化日志系统
    Logger::instance().initialize(true, "logs/application.log", Logger::LogLevel::Debug, false); // Logger::LogLevel::Debug true为开启，false为关闭
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
