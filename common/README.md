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

---

# 二进制文件格式定义 (BinaryFileHeader)

## 功能概述

`common/binary_file_format.h` 文件定义了项目中自定义二进制数据文件（例如 `.vbindata`）的文件头部结构 `BinaryFileHeader`。这个头部用于存储二进制文件的元数据，如文件标识、版本信息、数据维度和时间戳等，确保文件可以被正确解析和版本管理。

## 使用说明

当持久化层需要读取或写入二进制数据文件时，会首先处理这个 `BinaryFileHeader`。

### 结构体定义

```cpp
#include "common/binary_file_format.h"

// 示例：创建一个新的文件头
BinaryFileHeader header;
header.magic_number = VBIN_MAGIC_NUMBER; // "VBIN"
header.file_format_version = 1;
header.data_schema_version = 1; // 应与数据库中表的 schema_version 对应
header.row_count_in_file = 0;   // 实际行数
header.column_count_in_file = 0; // 实际列数
header.timestamp_created = static_cast<uint64_t>(QDateTime::currentSecsSinceEpoch());
header.timestamp_updated = header.timestamp_created;
// ... 其他字段根据需要设置

// 示例：从文件读取头部后进行校验
// (假设 'readHeader' 是一个读取文件头并填充 BinaryFileHeader 对象的函数)
// BinaryFileHeader loadedHeader = readHeader(fileDevice);
// if (loadedHeader.isValid()) {
//     qDebug() << "Binary file header is valid.";
//     loadedHeader.logDetails("ReadFile");
// } else {
//     qWarning() << "Invalid binary file header!";
// }
```

`BinaryFileHeader` 结构体包含以下主要字段：

- `magic_number`: 用于快速识别文件类型的魔数 (例如 `0x5642494E` 代表 "VBIN")。
- `file_format_version`: 二进制文件本身的结构版本。
- `data_schema_version`: 文件中存储的数据的模式版本，应与数据库中对应的元数据版本同步。
- `row_count_in_file`: 文件中存储的数据行数。
- `column_count_in_file`: 每行数据的列数。
- `timestamp_created`: 文件创建时的时间戳。
- `timestamp_updated`: 文件最后更新时的时间戳。
- `compression_type`: 数据压缩类型（当前默认为0，无压缩）。
- `reserved_bytes`: 保留字节，用于未来扩展。

结构体还提供了默认构造函数、`isValid()` 方法用于校验魔数，以及 `logDetails()` 方法用于输出头部信息以供调试。

## 输入/输出规范

- **输入**：当从二进制文件读取时，输入是文件开头的字节流，用于填充 `BinaryFileHeader` 结构。
- **输出**：当写入二进制文件时，`BinaryFileHeader` 结构的内容被序列化为字节流并写入文件开头。

## 向后兼容性

`file_format_version` 和 `data_schema_version` 字段用于支持版本控制和向后兼容。在读取旧版本文件时，持久化逻辑需要检查这些版本号并执行相应的适配操作。

## 版本和变更历史

- **版本**：1.0 (对应 `binary_file_format.h` 中 `BinaryFileHeader::file_format_version` 的初始值)
- **变更历史**：
  - 1.0 (YYYY-MM-DD): 初始定义 `BinaryFileHeader` 结构，包含魔数、版本号、行列计数、时间戳、压缩类型和保留字节。提供构造函数、校验和日志方法。

# 公共组件模块文档

## 功能概述

公共组件模块提供了整个应用程序中共享的实用工具和功能，包括表格样式管理、路径工具、数据格式处理等。

## 表格样式管理器 (TableStyleManager)

### 功能概述

表格样式管理器负责统一设置和维护应用程序中所有表格的视觉样式和行为。主要功能包括：

1. 设置表格的基本样式（边框、颜色、选择效果等）
2. 根据内容类型自动设置列对齐方式
3. 设置统一的行高
4. 管理管脚列的宽度分配

### 管脚列宽度优化

最新版本添加了管脚列宽度优化功能，解决了管脚列宽度不一致的问题：

- 自动识别管脚列（通过列标题中的换行符）
- 固定前六列为标准列（地址、标签、TimeSet、I_DUT、O_DUT、IO_DUT）
- 计算表格中剩余可用空间
- 将剩余空间平均分配给所有管脚列
- 确保当窗口大小变化时自动调整管脚列宽度

### 使用说明

在加载表格数据并希望应用统一样式时，调用以下函数：

```cpp
// 应用全部样式（包括管脚列宽度优化）
TableStyleManager::applyTableStyle(tableWidget);

// 仅优化管脚列宽度
TableStyleManager::setPinColumnWidths(tableWidget);
```

为确保窗口大小变化时管脚列宽度能够自动调整，请在窗口类中添加以下代码：

```cpp
// 在窗口大小变化时更新管脚列宽度
connect(this, &YourWindow::windowResized, [this]() {
    if (tableWidget && tableWidget->isVisible() && tableWidget->columnCount() > 6) {
        TableStyleManager::setPinColumnWidths(tableWidget);
    }
});

// 添加窗口大小变化处理
void YourWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    emit windowResized();
}
```

## 二进制文件格式 (BinaryFileFormat)

该模块定义了存储向量数据的二进制文件格式规范，包括文件头、版本信息和数据区。

## 路径工具 (PathUtils)

提供了处理和转换文件路径的实用函数，确保在不同操作系统中保持路径的兼容性。

## 向后兼容性

本模块保持完全向后兼容。所有新功能都是以增量方式添加，不会影响现有功能。

## 版本和变更历史

### 版本 2.5.0 (最新)

- 添加管脚列宽度优化功能，确保所有管脚列宽度一致
- 添加窗口大小变化时自动调整管脚列宽度的功能

### 版本 2.4.0

- 增强表格样式设置，支持根据列内容类型自动对齐
- 优化表格刷新机制，减少闪烁

### 版本 2.3.0

- 添加二进制文件格式定义
- 实现路径工具，支持跨平台路径处理
