# VecEdit - 矢量测试编辑器

## 功能概述

VecEdit是一个用于创建和编辑矢量测试数据的专业工具。它支持管理测试向量、时间设置(TimeSet)以及向量表的创建和编辑。应用程序采用混合存储方案，结合SQLite数据库与二进制文件，以优化性能和存储效率。

## 最新更新：向量表显示增强

### 功能概览

现在VecEdit支持完整的向量表列显示，包括以下标准列：

- Label (标签)
- Instruction (指令)
- TimeSet (时间集)
- Capture (捕获)
- EXT (扩展)
- Comment (注释)
以及所有配置的管脚列。

### 更新内容

1. **完整列配置支持**
   - 新建向量表时自动创建6个标准列
   - 支持现有向量表的列配置自动修复

2. **表头显示优化**
   - 管脚列显示通道数和类型信息
   - 标准列使用清晰的标题

3. **编辑器增强**
   - 基于列类型提供不同的编辑器：
     - 下拉框用于Instruction、TimeSet和Capture列
     - 专用输入框用于管脚状态
     - 通用文本输入用于Label、EXT和Comment

4. **兼容性**
   - 向后兼容现有数据文件
   - 启动时自动检查和修复旧格式的向量表

## 使用说明

### 标准列功能

- **Label**: 为向量行添加标识标签
- **Instruction**: 选择指令类型 (INC等)
- **TimeSet**: 选择适用的时间设置
- **Capture**: 设置是否在此向量捕获数据 (Y/N)
- **EXT**: 存储附加的扩展信息
- **Comment**: 添加注释说明

### 管脚列操作

管脚列显示格式为：

```
管脚名
x通道数
类型
```

支持的管脚状态值：

- H: 高电平
- L: 低电平
- Z: 高阻
- X: 不关心
- 1: 逻辑1
- 0: 逻辑0

## 版本记录

### v1.5.0

- 增加标准列完整支持 (Label, Instruction, TimeSet, Capture, EXT, Comment)
- 添加向量表列配置自动修复功能
- 改进表头显示和编辑体验

### v1.4.0

- 实现二进制文件和SQLite混合存储
- 优化大量向量数据的读写性能

### v1.3.0

- 添加TimeSet配置功能
- 支持管脚分组和批量操作

### v1.2.0

- 实现向量表编辑和数据导入
- 添加管脚配置功能

### v1.1.0

- 基础项目创建和管理
- SQLite数据库集成

### v1.0.0

- 初始版本发布

## 向量表加载失败问题修复

### 问题描述

在创建新向量表后，系统无法成功加载向量表数据，控制台报错：

```
[2025-05-12 16:44:03.078] [Warning] "loadVectorTableMeta" - 查询主记录失败, 表ID: 1, 错误: ""
[2025-05-12 16:44:03.079] [Warning] "VectorDataHandler::loadVectorTableData" - 元数据加载失败
```

### 问题原因

1. 在创建新向量表时，只在`vector_tables`表中插入了记录，没有在`VectorTableMasterRecord`表中创建对应记录
2. 由于使用了二进制文件存储方案，向量表数据需要同时在SQL数据库和二进制文件中有对应记录
3. `VectorTableColumnConfiguration`表中的外键字段名与代码中的查询不匹配

### 修复内容

1. 修改了`MainWindow::addNewVectorTable()`方法，在创建向量表后同时：
   - 创建相应的`VectorTableMasterRecord`记录
   - 创建空的二进制数据文件并写入正确的文件头

2. 修正了`vectordatahandler.cpp`中的SQL查询，使用正确的外键列名：
   - 将`vector_table_id`修改为`master_record_id`

### 文件变更

- `app/mainwindow.cpp`: 添加创建`VectorTableMasterRecord`记录和二进制文件的代码
- `vector/vectordatahandler.cpp`: 修正SQL查询中的列名

### 向后兼容性

本次修复完全向后兼容，不会影响现有的数据格式和结构。修复只针对创建新向量表的流程，不影响已有向量表的加载和处理。

### 版本

- 修复版本: 1.0.1
- 日期: 2025-05-12

## 核心数据加载逻辑修复 (VectorDataHandler)

### 问题描述

在修复了路径问题后，向量表加载仍然失败。日志显示在`VectorDataHandler::loadVectorTableData`中，`loadVectorTableMeta`函数返回的列信息不正确，例如列名显示为"0"。

### 问题原因

在`vectordatahandler.cpp`的匿名命名空间中的`loadVectorTableMeta`函数内，当从`VectorTableColumnConfiguration`表查询列配置信息时，`QSqlQuery::value()`方法使用了错误的列索引来获取`column_name`, `column_order`, `column_type`, 和 `data_properties`。这导致`ColumnInfo`结构体被错误的数据填充。

### 修复内容

修改了`loadVectorTableMeta`函数中对`colQuery.value(index)`的调用，确保使用正确的数据库列索引来填充`Vector::ColumnInfo`结构体的各个字段（`name`, `order`, `original_type_str`, `data_properties`）。同时，将`ColumnInfo::vector_table_id`成员直接赋予传入的`tableId`参数（该参数对应于`VectorTableMasterRecord`的ID）。

### 文件变更

- `vector/vectordatahandler.cpp`: 修正了`loadVectorTableMeta`函数中数据库查询结果的索引错误。

### 向后兼容性

本次修复是针对代码逻辑的修正，不涉及数据格式或数据库结构的更改，因此完全向后兼容。

### 版本

- 修复版本: (建议根据您的版本控制系统递增，例如 1.0.2 或特定修复版本号)
- 日期: 2025-05-13

## 性能优化说明

为了提高应用程序处理大量向量行数据（如500万行）的性能，我们进行了以下优化：

### 向量数据处理优化

1. **批量处理模式**
   - 实现了分批处理大量数据的机制，每批最多处理10,000行
   - 减少了一次性加载大量数据到内存的问题
   - 显著降低了内存占用峰值

2. **IO操作优化**
   - 对于追加模式，现在只读取文件头而不是整个文件
   - 直接在文件末尾追加数据，无需重写整个文件
   - 定期刷新文件缓冲区，避免一次性大量IO操作

3. **序列化过程优化**
   - 使用模板行预先创建标准列的默认值
   - 缓存UI表格中的数据，避免重复访问UI控件
   - 减少了重复计算和数据复制

4. **UI响应性改进**
   - 在批处理过程中主动让出CPU时间，减轻UI冻结
   - 优化进度更新精度，提供更好的用户反馈
   - 为大数据集提供更准确的时间估计

### 使用建议

1. **向量数据处理**
   - 推荐一次添加的行数不超过100万行，尽管系统可以处理更多
   - 对于非常大的数据集，考虑分多次添加
   - 处理过程中请勿关闭程序

2. **内存管理**
   - 在处理大数据之前，建议关闭其他内存密集型应用程序
   - 运行程序的计算机至少应有8GB内存才能处理百万级数据

### 未来优化方向

1. **数据存储**
   - 考虑实现数据压缩，进一步减少文件大小
   - 探索列式存储格式，提高针对特定列的查询效率

2. **并行处理**
   - 实现多线程处理大批量数据
   - 使用工作线程进行IO操作，完全避免UI冻结

3. **用户体验**
   - 添加处理速度统计信息
   - 实现自动保存点，允许恢复中断的操作

## 更新日志

### 2023-xx-xx - 修复下拉框值保存问题

- 修复了Instruction和TimeSet下拉框选择后无法正确保存的问题
- 改进了向量表数据的保存和加载逻辑
  - 保存时正确将下拉框选中的文本转换为对应的ID值
  - 加载时正确将数据库中的ID值转换为对应的文本显示
- 增强了日志记录，方便跟踪调试可能的问题
- 数据保存时使用事务处理，确保数据一致性

## 窗口自由缩放功能

### 功能概述

添加了窗口自由缩放功能，使用户可以根据自己的需要调整程序窗口的大小。这项功能使得应用程序可以在不同分辨率的显示器上更好地工作，并允许用户根据自己的工作习惯自定义工作区域大小。

### 使用说明

1. **手动调整窗口大小**：
   - 直接拖动窗口边缘或角落进行大小调整
   - 窗口最小尺寸为 800×600 像素

2. **通过菜单控制窗口**：
   - "查看" → "窗口最大化(M)" 可在最大化和还原状态之间切换
   - "查看" → "设置窗口大小(S)" 可精确设置窗口的宽度和高度

3. **查看当前窗口大小**：
   - 状态栏右侧显示当前窗口的宽度和高度信息

### 输入/输出规范

- **输入**：鼠标拖动窗口边缘或菜单操作
- **输出**：窗口尺寸调整，状态栏显示窗口当前大小

### 向后兼容性

完全向后兼容原有功能，不影响现有任何数据结构和操作流程。

### 版本和变更历史

#### 版本 1.0

- 添加窗口自由缩放功能
- 增加窗口尺寸控制菜单选项
- 添加状态栏显示窗口大小信息
- 增加窗口状态保存和恢复功能

## 修复TimeSet替换功能

### 问题描述

在最新的更新中，我们修复了"替换TimeSet"功能未能正确更新二进制文件数据的问题：

**问题描述**：

- 点击"替换TimeSet"按钮后，虽然显示"TimeSet替换完成"的成功消息，但实际上二进制文件中的TimeSet数据没有被替换。

**解决方案**：

- 修改了`replaceTimeSetForVectorTable`方法，使其不仅更新数据库中的TimeSet引用，还同时更新二进制文件中的TimeSet数据。
- 添加了完整的二进制文件处理逻辑，包括文件路径解析、数据读取、内存中数据修改和数据回写功能。

**技术细节**：

- 添加了获取二进制文件元数据的功能，包括文件名、schema版本等信息。
- 实现了列定义加载和解析，以确定TimeSet列的位置。
- 对内存中加载的向量数据进行TimeSet ID替换。
- 将修改后的数据回写到二进制文件，保证持久化。

**使用说明**：

1. 在向量表界面选择需要修改的行（不选择则应用于全表）。
2. 点击"替换TimeSet"按钮。
3. 在弹出的对话框中选择源TimeSet和目标TimeSet。
4. 点击确认后，系统将自动更新所有相关数据。

## 修复TimeSet填充功能

### 问题描述

在最新的更新中，我们修复了"填充TimeSet"功能未能正确更新二进制文件数据的问题：

**问题描述**：

- 使用"填充TimeSet"功能时，虽然数据库中的TimeSet引用得到更新，但二进制文件中的TimeSet数据未被同步修改。
- 这导致数据库和二进制文件之间的数据不一致，重新加载表格后表现为TimeSet设置丢失。

**解决方案**：

- 参考"替换TimeSet"功能的实现，完善了`fillTimeSetForVectorTable`方法，使其同时更新数据库和二进制文件中的TimeSet数据。
- 添加了完整的二进制文件处理逻辑，包括文件路径解析、列信息检索、数据读写等核心步骤。

**技术细节**：

- 增加了二进制文件路径解析和验证功能，确保能够正确定位到表格对应的二进制文件。
- 实现了列定义加载和解析，以确定TimeSet列的位置。
- 添加了自动检测并修复缺失TimeSet列的功能，增强了程序的健壮性。
- 对内存中加载的向量数据进行TimeSet ID填充，并将修改后的数据回写到二进制文件。
- 在整个过程中添加了详细的日志记录，方便追踪操作过程和排查问题。

**使用说明**：

1. 在向量表界面选择需要填充TimeSet的行（不选择则应用于全表）。
2. 点击"填充TimeSet"按钮。
3. 在弹出的对话框中选择要应用的TimeSet。
4. 点击确认后，系统将自动更新所有相关数据，包括数据库记录和二进制文件内容。

**向后兼容性**：

本次更新完全向后兼容，不会影响已有数据结构和文件格式。对于历史数据，系统会自动检测并适配，无需用户进行额外操作。

**版本更新**：

- 版本: 1.6.0
- 日期: 2025-05-21
- 主要变更: 修复"填充TimeSet"功能在二进制文件中不生效的问题

## 分页加载功能

### 功能概述

分页加载功能解决了大量数据加载时界面响应慢的问题。通过将数据分页显示，减少一次性加载的数据量，显著提升了程序的性能和响应速度。

### 使用说明

- 数据自动分页显示，无需额外操作
- 使用分页控制栏导航:
  - 上一页/下一页按钮: 切换相邻页面
  - 页码信息: 显示当前页码/总页数及总行数
  - 每页行数: 可选择每页显示100/500/1000/5000行
  - 跳转功能: 输入页码直接跳转到指定页面

### 输入/输出规范

- 输入: 用户交互的页面导航操作
- 输出: 分页显示的向量表数据，包含当前页数据及分页控制信息

### 向后兼容性

该功能完全向后兼容，不影响任何现有数据结构或文件格式。数据存储方式未发生变化，仅改变了数据加载和显示方式。

### 版本和变更历史

- 版本: 1.0.0
- 变更:
  - 新增分页加载机制
  - 添加分页控制界面
  - 修改数据加载方式，支持按页读取
  - 优化二进制文件读取，支持指定范围读取
  - 添加页面跳转与导航功能
  - 与现有功能完全集成，确保无缝体验

## Feature Overview

本次更新修复了手动保存功能在分页模式下的一个BUG。该BUG导致用户在分页视图中对管脚列（使用 `PinValueLineEdit` 的列）所做的修改在保存后，切换页面再返回时会丢失，数据恢复为修改前的值。

## Usage Instructions

手动保存功能（通过"保存"按钮触发）现在应能正确处理分页数据：

- 当用户在分页视图中修改了任何单元格（包括标准单元格和使用如 `PinValueLineEdit` 的自定义编辑器单元格）。
- 点击"保存"按钮。
- 这些修改会被正确合并到完整数据集中并持久化。
- 之后切换到其他页面再返回该已修改并保存的页面时，用户应能看到他们保存的修改，数据不再丢失或恢复为默认值。

**注意**：在之前版本中，为解决另一分页BUG而暂时禁用的"页面切换时自动保存"功能仍然保持禁用状态。用户需要通过主菜单的"保存"按钮来保存对向量表数据的修改。

## Input/Output Specification

- **输入**: 用户在UI的分页表格视图中修改数据（特别是管脚列），然后点击"保存"按钮。
- **输出**: 修改后的数据应被正确写入后端的二进制存储文件。当用户后续重新加载该数据页时，表格应显示已保存的修改。

## Backward Compatibility

此更改是向后兼容的。它修复了现有保存功能的错误行为，没有改变数据格式或核心API。

## Versioning and Change History

- **Module Version**: N/A (Applies to MainWindow data saving logic)
- **Date**: 2023-10-27 (假设日期)
- **Change Description**:
  - **Fixed**: Resolved a bug in `MainWindow::saveVectorTableData` where modifications made to `cellWidget` based columns (e.g., `PinValueLineEdit` for pin states) in a paged view were not correctly merged into the full dataset before saving. This resulted in such changes being lost upon saving and subsequent page navigation.
  - **Details**: The data merging logic within `MainWindow::saveVectorTableData` was updated to explicitly check for and handle `sourceCellWidget` from the UI's current page view (`m_vectorTableWidget`). If a `PinValueLineEdit` is found, its text content is now correctly transferred to the corresponding `PinValueLineEdit` in the temporary full data table (`tempFullDataTable`) before the full data is passed to `VectorDataHandler` for serialization. Also improved handling for empty/cleared cells in the source view to ensure they correctly clear the corresponding cells in the full data table.
- **Author**: AI Assistant
- **Rationale**: The previous merging logic in `MainWindow::saveVectorTableData` primarily relied on `QTableWidget::item()`, which returns `nullptr` for cells managed by `QTableWidget::cellWidget()`. This oversight caused data from `PinValueLineEdit` (and potentially other cell widgets) to be ignored during the merge process when in paged-saving mode, leading to data loss for those specific cells.

## 版本更新说明

### 2023年7月更新 - 固定长度二进制存储格式

为了优化数据存储和提高读写效率，本次更新将二进制存储格式从可变长度改为固定长度。主要特性包括：

1. 定义了各种数据类型的固定存储长度，如TEXT(256字节)、INTEGER(4字节)等
2. 在前端实现了相应的输入长度限制，确保数据一致性
3. 保持向后兼容性，能够读取旧的二进制文件格式

详细信息请参考以下文档：

- 文件长度定义：`common/binary_field_lengths.h`
- 二进制文件处理：`database/binaryfilehelper.cpp`
- 前端输入限制：`vector/vectortabledelegate.cpp`
