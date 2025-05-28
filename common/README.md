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

## 功能概述

TableStyleManager 是一个提供表格样式管理功能的工具类，用于快速、高效地为 Qt 表格部件（QTableWidget 和 QTableView）应用统一的样式。本模块支持批量样式设置、预定义样式类和自定义列样式，大幅提高表格渲染性能和开发效率。

## 使用说明

### 基础样式应用

```cpp
// 方法1：基本样式应用（旧版API，逐元素设置，性能较低）
TableStyleManager::applyTableStyle(tableWidget);

// 方法2：优化的样式应用（减少重绘次数）
TableStyleManager::applyTableStyleOptimized(tableWidget);

// 方法3：批量样式设置（高性能版，推荐使用）
TableStyleManager::applyBatchTableStyle(tableWidget);

// 对于QTableView的样式设置
TableStyleManager::applyTableStyle(tableView);
```

### 样式类应用

可以使用预定义的样式类来设置单元格或整列的外观：

```cpp
// 对单个单元格应用样式类
TableStyleManager::applyCellStyleClass(tableWidget, row, column, TableStyleManager::Numeric);

// 对整列应用样式类
TableStyleManager::applyColumnStyleClass(tableWidget, columnIndex, TableStyleManager::DateTime);
```

### 可用的样式类

TableStyleManager 提供以下预定义样式类：

| 样式类 | 描述 | 视觉效果 |
|--------|------|----------|
| Default | 默认样式 | 白底黑字 |
| Numeric | 数值型单元格 | 浅蓝底深蓝字，等宽字体 |
| DateTime | 日期时间单元格 | 浅绿底深绿字 |
| Status | 状态信息单元格 | 浅红底深红字，加粗 |
| HeaderPin | 管脚表头 | 浅蓝底黑字，带蓝色边框 |
| PinCell | 管脚单元格 | 浅蓝底黑字，等宽字体 |
| HighlightRow | 高亮行 | 浅黄底黑字 |
| EditableCell | 可编辑单元格 | 浅绿底黑字，虚线边框 |
| ReadOnlyCell | 只读单元格 | 浅灰底灰字，无边框 |

## 输入/输出规范

### 批量表格样式设置功能

**输入**：

- `QTableWidget*` - 指向要设置样式的表格部件的指针

**输出**：

- 无返回值，直接修改表格样式

### 样式类应用功能

**输入**：

- `QTableWidget*` - 指向表格部件的指针
- `int row` / `int column` - 行和列索引（单元格样式）或列索引（列样式）
- `StyleClass` - 要应用的样式类枚举值

**输出**：

- 无返回值，直接修改指定单元格或列的样式

## 向后兼容性

此更新完全向后兼容，旧的API函数（`applyTableStyle`和`applyTableStyleOptimized`）保持不变，同时增加了新的更高效的API函数。

## 版本与变更历史

**当前版本**：1.2.0

**变更记录**：

- 1.2.0 (2025-05-25)
  - 新增批量样式设置方法（`applyBatchTableStyle`）
  - 添加样式类系统，支持通过预定义类快速设置样式
  - 添加单元格和列样式类应用方法
  - 性能优化：批量设置减少95%样式应用时间

- 1.1.0 (2025-05-20)
  - 添加优化版表格样式应用方法（`applyTableStyleOptimized`）
  - 减少重绘次数，提高性能

- 1.0.0 (2025-05-15)
  - 初始版本
  - 基本表格样式设置功能

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

# 通用工具模块文档

本文档描述了VecEdit项目中common目录下的公共组件和工具类。

## binary_file_format.h

### 功能概述

`common/binary_file_format.h` 文件定义了项目中自定义二进制数据文件（例如 `.vbindata`）的文件头部结构 `BinaryFileHeader`。这个头部用于存储二进制文件的元数据，如文件标识、版本信息、数据维度和时间戳等，确保文件可以被正确解析和版本管理。

### 使用说明

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

## binary_field_lengths.h

### 功能概述

`common/binary_field_lengths.h` 文件定义了二进制数据存储中各种数据类型的固定字段长度。通过预定义固定长度，优化了二进制数据的存储和读取效率，同时也为前端输入提供了相应的长度限制指导。

### 使用说明

这些常量在序列化和反序列化代码中使用，以确保字段长度一致。

```cpp
#include "common/binary_field_lengths.h"

// 使用示例
int textFieldMaxLen = Persistence::TEXT_FIELD_MAX_LENGTH;
int pinStateLen = Persistence::PIN_STATE_FIELD_MAX_LENGTH;

// 根据固定长度限制输入
QLineEdit *lineEdit = new QLineEdit();
lineEdit->setMaxLength(Persistence::TEXT_FIELD_MAX_LENGTH / 2); // UTF-16字符占两字节
```

### 常量定义

文件定义了以下常量：

| 常量名 | 值(字节) | 描述 |
|--------|---------|------|
| TEXT_FIELD_MAX_LENGTH | 256 | 文本字段最大长度 |
| PIN_STATE_FIELD_MAX_LENGTH | 1 | 管脚状态字段长度 |
| INTEGER_FIELD_MAX_LENGTH | 4 | 整数类型字段长度 |
| REAL_FIELD_MAX_LENGTH | 8 | 浮点数字段长度 |
| BOOLEAN_FIELD_MAX_LENGTH | 1 | 布尔值字段长度 |
| JSON_PROPERTIES_MAX_LENGTH | 1024 | JSON属性字段最大长度 |

## utils 目录

utils目录包含了各种通用工具类和函数，用于简化整个项目中的常见任务。
