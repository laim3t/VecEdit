# TimeSet 模块

## 功能概述

TimeSet（时间集）模块是VecEdit应用程序的核心组件之一，负责管理和配置测试向量中的时间参数。该模块提供了完整的TimeSet创建、编辑、填充和替换功能，支持用户灵活定义测试时序。

## 组件结构

TimeSet模块包含以下主要组件：

### 核心组件

- **TimeSetModel**：TimeSet数据模型，定义了TimeSet的数据结构和属性
- **TimeSetDataAccess**：提供TimeSet数据的数据库访问层，实现CRUD操作
- **TimeSetUI**：TimeSet相关的UI组件和显示逻辑

### 对话框组件

- **TimeSetDialog**：TimeSet创建和编辑对话框
- **FillTimeSetDialog**：填充TimeSet功能对话框
- **ReplaceTimeSetDialog**：替换TimeSet功能对话框
- **TimeSetEdgeDialog**：编辑TimeSet边沿参数的对话框

### 辅助组件

- **TimeSetEdgeManager**：管理TimeSet的边沿定义和操作

## 功能说明

### TimeSet创建与编辑

TimeSet创建和编辑功能允许用户定义测试向量的时间参数，包括：

- 时钟周期
- 边沿时序定义
- 信号延迟和宽度设置
- 单位和分辨率配置

### TimeSet填充功能

TimeSet填充功能允许用户将指定的TimeSet应用到向量表的选定行或整个表格中：

1. **选择目标**：用户可以选择特定行或整个表格
2. **选择TimeSet**：从可用TimeSet列表中选择一个应用
3. **执行填充**：系统会更新数据库和二进制文件中的TimeSet引用

**技术实现**：

- 通过`FillTimeSetDialog`类提供用户界面
- 使用`TimeSetDataAccess`读取可用的TimeSet列表
- `MainWindow::fillTimeSetForVectorTable`方法执行实际的填充操作，同时更新数据库和二进制文件

### TimeSet替换功能

TimeSet替换功能允许用户在向量表中将一个TimeSet替换为另一个：

1. **选择目标**：用户可以选择特定行或整个表格
2. **选择源和目标TimeSet**：指定要替换的源TimeSet和替换后的目标TimeSet
3. **执行替换**：系统会更新数据库和二进制文件中的TimeSet引用

**技术实现**：

- 通过`ReplaceTimeSetDialog`类提供用户界面
- 使用`TimeSetDataAccess`读取可用的TimeSet列表
- `MainWindow::replaceTimeSetForVectorTable`方法执行实际的替换操作，同时更新数据库和二进制文件

## 最近改进

### 填充TimeSet功能修复 (v1.6.0)

在最新的更新中，我们修复了"填充TimeSet"功能未能正确更新二进制文件数据的问题：

- **问题**：使用"填充TimeSet"功能时，虽然数据库中的TimeSet引用得到更新，但二进制文件中的TimeSet数据未被同步修改
- **解决方案**：完善了`fillTimeSetForVectorTable`方法，使其同时更新数据库和二进制文件中的TimeSet数据
- **实现细节**：
  - 增加了二进制文件路径解析和验证
  - 完善了列定义加载和TimeSet列识别
  - 添加了向量数据的TimeSet ID填充逻辑
  - 实现了二进制文件的数据回写

### 替换TimeSet功能修复 (v1.5.5)

此前，我们也修复了"替换TimeSet"功能的类似问题：

- **问题**：点击"替换TimeSet"按钮后，虽然显示成功消息，但二进制文件中的TimeSet数据没有被替换
- **解决方案**：修改了`replaceTimeSetForVectorTable`方法，使其同步更新数据库和二进制文件
- **实现细节**：
  - 添加了二进制文件元数据获取功能
  - 实现了列定义解析与TimeSet列定位
  - 添加了对内存中向量数据的TimeSet ID替换
  - 实现了数据回写到二进制文件的功能

## 使用示例

### 创建新的TimeSet

```cpp
// 创建新的TimeSet对象
TimeSet timeSet;
timeSet.setName("标准时序");
timeSet.setPeriod(100);  // 100ns周期

// 添加边沿定义
timeSet.addEdge("上升沿", 10, Edge::Rising);
timeSet.addEdge("下降沿", 90, Edge::Falling);

// 保存TimeSet
TimeSetDataAccess dataAccess;
int newTimeSetId = dataAccess.saveTimeSet(timeSet);
```

### 填充TimeSet

```cpp
// 获取选中的行
QList<int> selectedRows = getSelectedRows();

// 填充TimeSet
int timeSetId = 2;  // 要填充的TimeSet ID
mainWindow->fillTimeSetForVectorTable(timeSetId, selectedRows);
```

### 替换TimeSet

```cpp
// 获取选中的行
QList<int> selectedRows = getSelectedRows();

// 替换TimeSet
int fromTimeSetId = 1;  // 源TimeSet ID
int toTimeSetId = 2;    // 目标TimeSet ID
mainWindow->replaceTimeSetForVectorTable(fromTimeSetId, toTimeSetId, selectedRows);
```

## 版本历史

### v1.6.0 (2025-05-21)

- 修复"填充TimeSet"功能在二进制文件中不生效的问题

### v1.5.5 (2025-05-18)

- 修复"替换TimeSet"功能在二进制文件中不生效的问题

### v1.5.0 (2025-05-10)

- 添加TimeSet替换功能
- 优化TimeSet对话框UI

### v1.4.0 (2025-05-01)

- 添加TimeSet填充功能
- 改进TimeSet边沿定义编辑器

### v1.3.0 (2025-04-20)

- 初始版本，包含基本的TimeSet创建和编辑功能
