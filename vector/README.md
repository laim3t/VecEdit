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
