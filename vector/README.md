# 向量模块

本模块用于实现向量表的创建、编辑、保存和删除等功能，是VecEdit应用的核心功能模块之一。

## 功能概览

- 向量表创建和管理
- 向量数据编辑和保存
- 管脚状态设置
- 分页显示大型数据集
- 二进制数据存储和读取

## 主要文件和组件

- **vectordatahandler.h/cpp**: 向量数据处理核心类，负责向量数据的加载、保存和处理
- **vectordatamanager.h/cpp**: 向量数据管理类，提供用户界面相关功能
- **pinvaluelineedit.h/cpp**: 管脚值编辑控件，用于编辑管脚状态

## 性能优化历史

### 1.0.0 基础实现

- 基本的向量表创建和管理
- 通过UI表格编辑和保存数据
- 分页浏览功能支持大型向量表

### 1.1.0 数据处理优化 (当前版本)

- `VectorDataHandler::saveVectorTableDataPaged`：新增的优化版保存函数，避免创建临时表格
- `MainWindow::saveVectorTableData`：处理用户保存操作的优化入口

#### 优化亮点

1. **避免临时表格创建**：原先的实现需要创建完整的临时表格用于合并数据，新实现直接在内存中处理数据，减少了UI组件创建和销毁的开销
2. **减少数据复制**：直接从二进制文件读取数据，然后合并当前页面的修改，减少了数据复制次数
3. **跳过未修改的行**：通过比较修改前后的值，只处理被修改的行，提高处理效率
4. **降低日志开销**：减少不必要的日志输出，进一步提升性能
5. **内存使用优化**：预分配内存空间，减少动态分配次数

## 使用方式

向量数据处理主要由`VectorDataHandler`类提供，该类是单例模式，可以通过以下方式使用：

```cpp
// 获取实例
VectorDataHandler &handler = VectorDataHandler::instance();

// 加载向量表数据
handler.loadVectorTableData(tableId, tableWidget);

// 保存向量表数据
QString errorMsg;
bool success = handler.saveVectorTableData(tableId, tableWidget, errorMsg);

// 使用优化版本的保存函数 (适用于分页模式)
bool successPaged = handler.saveVectorTableDataPaged(tableId, currentPageWidget, currentPage, pageSize, totalRows, errorMsg);
```

## 数据格式

向量表数据存储采用混合模式：

- 元数据和表结构存储在SQLite数据库中
- 行数据存储在二进制文件中，提高读写效率
- 二进制文件采用标准格式，支持版本控制和向后兼容

## 问题排查

保存操作失败时常见原因：

1. 数据库连接问题
2. 二进制文件写入权限问题
3. 数据格式不匹配
4. 列映射关系错误

可通过查看日志文件了解详细错误信息。

## 版本历史

- **1.0.0**: 初始版本，实现基本功能
- **1.1.0**: 性能优化，实现无临时表的数据保存功能
