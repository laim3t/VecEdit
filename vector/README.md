# 向量数据处理模块

## 功能概述

向量数据处理模块负责管理测试向量数据的读取、保存、编辑和显示功能。该模块提供了对向量表的完整操作支持，包括表格初始化、数据加载、字段编辑、行增删、数据保存等核心功能。

## 数据结构

### 核心数据结构

- **ColumnInfo**: 描述向量表的列结构，包含列ID、名称、类型、顺序等信息
- **RowData**: 表示一行向量数据，是一个QVariant列表
- **VectorTableData**: 表示完整的向量表数据，是RowData的列表
- **ColumnDataType**: 定义了列的数据类型，如TEXT、INTEGER、INSTRUCTION_ID、TIMESET_ID、PIN_STATE_ID等

### 数据存储

向量数据使用混合存储方案：

- 元数据（表结构、列配置等）存储在SQLite数据库中
- 实际的行数据存储在二进制文件中，提高大型数据集的读写效率

## 关键功能

### 数据加载

`loadVectorTableData`函数负责从数据库和二进制文件加载向量表数据到QTableWidget中：

1. 从数据库加载表结构元数据
2. 解析二进制文件路径
3. 读取二进制数据
4. 设置表格表头
5. 填充数据到表格中

对于特殊类型的列，如下拉框列（INSTRUCTION_ID和TIMESET_ID），会执行特殊处理：

- 将存储的ID值查询对应的文本名称
- 将结果显示在表格中
- 保存原始ID值到表格项的UserRole中，便于保存时使用

### 数据保存

`saveVectorTableData`函数负责将QTableWidget中的数据保存到数据库和二进制文件中：

1. 获取表结构元数据
2. 解析二进制文件路径
3. 建立表格与数据库列的映射关系
4. 收集表格中的所有行数据
5. 对于下拉框列（如INSTRUCTION_ID和TIMESET_ID），将显示文本转换为对应的ID值
6. 使用事务写入二进制文件和更新数据库

## 下拉框处理

### 下拉框定义

向量表中的以下列类型使用下拉框编辑：

- `INSTRUCTION_ID`: 指令列，使用instruction_options表中的值
- `TIMESET_ID`: 时序列，使用timeset_list表中的值
- `BOOLEAN`: 布尔列（如Capture），使用Y/N选项

### 数据保存流程

1. 用户在表格中选择下拉框选项
2. 点击保存按钮触发saveVectorTableData函数
3. 函数首先查询所有指令和TimeSet的名称到ID的映射关系
4. 从表格中读取选中的文本值
5. 将文本值转换为对应的ID值
6. 保存ID值到二进制文件中

### 数据加载流程

1. loadVectorTableData函数从二进制文件读取ID值
2. 通过ID查询数据库获取对应的文本名称
3. 在表格中显示文本名称
4. 在表格项的UserRole中保存原始ID值

## 使用说明

### 向量表操作

1. 创建新向量表：使用MainWindow::addNewVectorTable函数
2. 加载向量表：使用VectorDataHandler::loadVectorTableData函数
3. 保存向量表：使用VectorDataHandler::saveVectorTableData函数
4. 删除向量表：使用VectorDataHandler::deleteVectorTable函数

### 行操作

1. 添加行：使用VectorDataHandler::addVectorRow函数
2. 删除行：使用VectorDataHandler::deleteVectorRows函数
3. 插入行：使用VectorDataHandler::insertVectorRows函数

## 版本历史

### 2023-xx-xx - 修复下拉框值保存问题

- 修复了Instruction和TimeSet下拉框选择后无法正确保存的问题
- 改进了向量表数据的保存和加载逻辑
  - 保存时正确将下拉框选中的文本转换为对应的ID值
  - 加载时正确将数据库中的ID值转换为对应的文本显示
- 增强了日志记录，方便追踪调试可能的问题
- 数据保存时使用事务处理，确保数据一致性

## 注意事项

1. 表格结构修改（如列添加、删除）需要同时更新数据库和二进制文件
2. 处理大型向量表时，注意内存使用和性能优化
3. 二进制文件路径解析和表ID保持一致性，避免数据混乱

## 性能优化

向量数据处理模块针对大规模数据集进行了专门优化：

1. **批量向量行添加**：可在20秒内添加500万行数据
2. **内存管理优化**：减少40%以上峰值内存占用
3. **磁盘I/O优化**：减少文件写入次数，提高数据处理效率
4. **智能缓存机制**：针对重复模式数据进行预缓存

> 详细优化内容请参阅[优化文档](./README_优化.md)

## 关键类和接口

### VectorDataHandler

核心数据处理类，提供数据访问和操作的接口。

主要方法：

- `loadVectorTableData(int tableId, QTableWidget *tableWidget)`: 加载向量表数据到UI控件
- `saveVectorTableData(int tableId, QTableWidget *tableWidget, QString &errorMessage)`: 保存表格数据到数据库
- `insertVectorRows(int tableId, int startIndex, int rowCount, int timesetId, QTableWidget *dataTable, bool appendToEnd, const QList<QPair<int, QPair<QString, QPair<int, QString>>>> &selectedPins, QString &errorMessage)`: 批量插入向量行数据
- `deleteVectorRowsInRange(int tableId, int fromRow, int toRow, QString &errorMessage)`: 删除指定范围内的向量行

### VectorDataManager

提供向量数据管理的高级接口，用于UI层与数据层之间的交互。

主要方法：

- `addVectorRow(QTableWidget *table, const QStringList &pinOptions, int rowIdx)`: 向表格添加单行向量数据
- `addVectorRows(QTableWidget *table, const QStringList &pinOptions, int startRowIdx, int count)`: 批量添加向量行

### 数据类型

- `Vector::ColumnInfo`: 表示向量表中单个列的元数据
- `Vector::RowData`: 表示单行向量数据，实际上是`QList<QVariant>`的别名
- `Vector::VectorTableData`: 表示整个向量表数据，实际上是`QList<RowData>`的别名

## 文件存储格式

向量数据采用混合存储模式：

1. **元数据**: 存储在SQLite数据库的`VectorTableMasterRecord`和`VectorTableColumnConfiguration`表中
2. **行数据**: 存储在二进制文件(.vbindata)中，使用自定义的二进制格式

二进制文件格式:

- **文件头**: 包含魔数标识、版本信息、行数和列数等基本信息
- **行数据块**: 每行数据先写入块大小，然后是序列化后的数据

## 使用示例

```cpp
// 加载向量表数据
QTableWidget *tableWidget = new QTableWidget();
VectorDataHandler::instance().loadVectorTableData(tableId, tableWidget);

// 批量添加向量行
QStringList pinOptions = {"1", "0", "X", "Z", "H", "L"};
VectorDataHandler::addVectorRows(tableWidget, pinOptions, 0, 5000000); // 添加500万行

// 保存表格数据
QString errorMessage;
bool success = VectorDataHandler::instance().saveVectorTableData(tableId, tableWidget, errorMessage);
```

## 错误处理

大多数公共方法都通过返回布尔值和填充errorMessage参数的方式进行错误处理。在使用这些方法时，建议始终检查返回值并处理可能的错误消息。

```cpp
QString errorMessage;
if (!VectorDataHandler::instance().saveVectorTableData(tableId, tableWidget, errorMessage)) {
    QMessageBox::critical(this, "保存失败", errorMessage);
}
```

## 性能最佳实践

1. **批量操作**：尽可能使用批量添加/删除方法而非逐行操作
2. **追加模式**：对于大规模数据添加，优先使用追加模式而非插入模式
3. **内存管理**：对于超大数据集，考虑分批次加载和处理数据
4. **预缓存数据**：对于重复性模式数据，利用预缓存机制提高性能
5. **合理批次大小**：根据系统内存和硬盘性能调整批次大小（默认50,000行/批）

## 版本历史

- v2.0: 大幅优化向量行添加性能，支持20秒内添加500万行
- v1.5: 添加批量删除和范围删除功能
- v1.0: 初始版本，实现基本的向量数据处理功能
