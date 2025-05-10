# 日志系统模块

## 功能概述

日志系统模块提供了一个全局的日志处理机制，用于捕获和格式化Qt应用程序中的所有日志消息（qDebug, qInfo, qWarning, qCritical, qFatal），并将其输出到控制台和/或日志文件。

## 使用说明

### 初始化日志系统

在应用程序启动时（通常在main.cpp中）初始化日志系统：

```cpp
#include "common/logger.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    
    // 初始化日志系统，参数1：是否写入文件，参数2：日志文件路径
    Logger::instance().initialize(true, "logs/application.log");
    
    // 应用程序其他初始化代码...
    return a.exec();
}
```

### 输出日志

在代码中使用Qt标准日志函数输出日志：

```cpp
// 调试信息
qDebug() << "函数名 - 这是一条调试信息" << someVariable;

// 一般信息
qInfo() << "函数名 - 这是一条普通信息" << someVariable;

// 警告信息
qWarning() << "函数名 - 这是一条警告信息" << someVariable;

// 严重错误
qCritical() << "函数名 - 这是一条严重错误信息" << someVariable;

// 致命错误（会导致程序终止）
qFatal() << "函数名 - 这是一条致命错误信息";
```

## 输入/输出规范

- **输入**：Qt标准日志函数的调用（qDebug, qInfo, qWarning, qCritical, qFatal）
- **输出**：
  - 控制台输出：格式化的日志消息
  - 文件输出（如启用）：相同格式的日志消息写入指定文件

## 向后兼容性

该日志系统与Qt标准日志机制完全兼容，不会影响现有代码的行为，只是增强了日志的显示和存储功能。

## 版本和变更历史

- **版本**：1.0
- **变更历史**：
  - 1.0 - 初始版本，支持控制台和文件日志输出，包含时间戳、日志级别和源代码位置信息
