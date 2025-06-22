#include "mainwindow.h"
#include "common/logger.h"

#include <QApplication>
#include <QDebug>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    // 初始化Qt应用
    QApplication a(argc, argv);

    // 设置应用程序信息
    QApplication::setApplicationName("VecEdit");
    QApplication::setOrganizationName("VecEdit");
    
    // 初始化日志系统
    try {
        Logger::instance().initialize(true, "logs/application.log", Logger::LogLevel::Debug, true);
        qDebug() << "main - 日志系统初始化成功";
    } catch (const std::exception& e) {
        QMessageBox::critical(nullptr, "错误", QString("日志系统初始化失败: %1").arg(e.what()));
        return 1;
    }
    
    qDebug() << "main - 应用程序启动";
    qInfo() << "main - 欢迎使用VecEdit";

    try {
        qDebug() << "main - 准备创建MainWindow对象";
        MainWindow* w = new MainWindow();
        qDebug() << "main - MainWindow对象创建完成";
        
        qDebug() << "main - 准备显示MainWindow";
        w->show();
        qDebug() << "main - MainWindow已显示";

        qDebug() << "main - 进入应用程序主循环";
        int result = a.exec();
        qDebug() << "main - 应用程序退出，返回码:" << result;

        return result;
    } catch (const std::exception& e) {
        qCritical() << "main - 发生异常:" << e.what();
        QMessageBox::critical(nullptr, "错误", QString("程序发生异常: %1").arg(e.what()));
        return 1;
    } catch (...) {
        qCritical() << "main - 发生未知异常";
        QMessageBox::critical(nullptr, "错误", "程序发生未知异常");
        return 1;
    }
}
