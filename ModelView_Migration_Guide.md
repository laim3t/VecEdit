# 将 QTableWidget 重构为 QTableView + 模型/视图架构的详细指南

本文档为 AI 助手提供了一套完整、明确的指令，用于将项目中基于 `QTableWidget` 的大数据量显示重构为使用 `QTableView` 和自定义数据模型（`QAbstractTableModel`）的高性能架构。

**项目背景分析:**

1. **纯 C++ UI**: 本项目的用户界面完全由 C++ 代码在 `app/mainwindow_setup.cpp` 等文件中构建，并未使用 Qt Designer 或 `.ui` 文件。所有 UI 控件都是 `MainWindow` 类的成员变量。
2. **现有数据层**: 项目中已存在一个名为 `VectorDataHandler` 的类，它负责所有与后端数据（数据库、文件）的交互。本次重构将最大限度地复用此类，仅作为数据适配层。
3. **目标**: 解决因一次性向 `QTableWidget` 添加大量 `QTableWidgetItem` 对象而导致的性能瓶颈和 UI 无响应问题。

---

## 重构执行步骤

请严格按照以下步骤顺序执行。

### 第1步：创建新的数据模型类 `VectorTableModel`

首先，我们需要创建一个新的 C++ 类，它继承自 `QAbstractTableModel`，作为 `QTableView` 和 `VectorDataHandler` 之间的桥梁。

**操作 1.1: 创建 `vector/vectortablemodel.h` 文件**

```cpp
#pragma once

#include <QAbstractTableModel>
#include "vector/vectordatahandler.h" // 依赖现有的数据处理器
#include "vector/vector_data_types.h" // 依赖现有的数据类型

class VectorTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit VectorTableModel(VectorDataHandler* handler, QObject *parent = nullptr);

    // 对外接口：用于设置当前要显示的向量表ID
    void setTableId(int tableId);

    // --- QAbstractTableModel 必须实现的接口 ---
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // --- 用于实现编辑功能的接口 ---
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;


public slots:
    // 公共槽函数：用于在外部数据发生变化后，刷新整个模型和视图
    void refreshModel();

private:
    VectorDataHandler* m_dataHandler;       // 指向现有数据处理器的指针（复用）
    int m_currentTableId;                   // 当前显示的向量表ID
    int m_rowCountCache;                    // 缓存的行数，避免重复查询
    QList<Vector::ColumnInfo> m_columnInfoCache; // 缓存的列定义，避免重复查询
};
```

**操作 1.2: 创建 `vector/vectortablemodel.cpp` 文件**

```cpp
#include "vectortablemodel.h"
#include <QDebug>

VectorTableModel::VectorTableModel(VectorDataHandler* handler, QObject *parent)
    : QAbstractTableModel(parent),
      m_dataHandler(handler),
      m_currentTableId(-1),
      m_rowCountCache(0)
{
    // 确保传入了有效的 data handler
    Q_ASSERT(m_dataHandler);
}

void VectorTableModel::setTableId(int tableId)
{
    if (m_currentTableId == tableId && tableId != -1) {
        return; // 如果ID未变，则无需操作
    }
    m_currentTableId = tableId;
    refreshModel();
}

void VectorTableModel::refreshModel()
{
    // beginResetModel/endResetModel 是刷新模型的标准做法。
    // 它会通知所有连接的视图：模型结构即将发生重大变化，请完全重置。
    // 这是最简单、最安全的刷新方式。
    beginResetModel();

    if (m_currentTableId != -1) {
        // 从 data handler 获取最新的数据信息并缓存
        m_rowCountCache = m_dataHandler->getVectorTableRowCount(m_currentTableId);
        m_columnInfoCache = m_dataHandler->getColumnInfo(m_currentTableId);
    } else {
        // 如果没有有效的表ID，则清空模型
        m_rowCountCache = 0;
        m_columnInfoCache.clear();
    }
    
    endResetModel();
}

int VectorTableModel::rowCount(const QModelIndex & /*parent*/) const
{
    return m_rowCountCache;
}

int VectorTableModel::columnCount(const QModelIndex & /*parent*/) const
{
    return m_columnInfoCache.count();
}

// 这是模型/视图架构的核心：视图只在需要显示数据时才调用此函数
QVariant VectorTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || m_currentTableId == -1) {
        return QVariant();
    }

    // 只处理"显示"和"编辑"角色，其它角色（如样式、工具提示等）暂时忽略
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        // 直接委托给 data handler 获取单个单元格的数据
        // 注意: VectorDataHandler 必须提供 getVectorCellData 方法
        return m_dataHandler->getVectorCellData(m_currentTableId, index.row(), index.column());
    }

    return QVariant();
}

QVariant VectorTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    if (orientation == Qt::Horizontal) {
        // 返回水平表头（列名）
        if (section >= 0 && section < m_columnInfoCache.count()) {
            return m_columnInfoCache.at(section).name;
        }
    } else if (orientation == Qt::Vertical) {
        // 返回垂直表头（行号），从1开始
        return section + 1;
    }

    return QVariant();
}

// --- 编辑功能实现 ---

Qt::ItemFlags VectorTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    
    // 允许所有单元格都是可选择、可启用、可编辑的
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
}

bool VectorTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role == Qt::EditRole) {
        // 委托给 data handler 保存数据
        bool success = m_dataHandler->saveVectorCellData(m_currentTableId, index.row(), index.column(), value);
        
        if (success) {
            // 如果保存成功，必须发射 dataChanged 信号通知视图更新此单元格
            emit dataChanged(index, index, {role});
            return true;
        }
    }
    return false;
}
```

### 第2步：确保 `VectorDataHandler` 提供所需接口

`VectorTableModel` 的实现依赖于 `VectorDataHandler` 提供的几个关键方法。请确保 `VectorDataHandler` 类（在 `vector/vectordatahandler.h` 和 `.cpp` 中）拥有并实现了以下公共接口：

```cpp
// 1. 获取总行数 (应已存在)
int getVectorTableRowCount(int tableId) const;

// 2. 获取列的定义信息 (必须添加)
// 返回值 QList<Vector::ColumnInfo> 假设 Vector::ColumnInfo 是已存在的结构体
QList<Vector::ColumnInfo> getColumnInfo(int tableId) const;

// 3. 获取单个单元格的数据 (必须添加)
// 这是最高效的方式，避免一次性加载整页数据
QVariant getVectorCellData(int tableId, int row, int column) const;

// 4. 保存单个单元格的数据 (为编辑功能必须添加)
bool saveVectorCellData(int tableId, int row, int column, const QVariant &data);
```

### 第3步：更新构建系统 `CMakeLists.txt`

新创建的 `vector/vectortablemodel.cpp` 文件需要被添加到编译列表中。

**操作**: 打开项目根目录下的 `CMakeLists.txt` 文件，找到 `set(SOURCES ...)` 部分，在其中加入新文件的路径。

```cmake
# ... 其他源文件 ...
vector/vectordatahandler.cpp
vector/vectordatamanager.cpp
vector/vectortabledelegate.cpp
vector/vectortablemodel.cpp  # <--- 在这里添加新文件
# ... 其他源文件 ...
```

### 第4步：修改 `MainWindow` 类定义

现在，我们需要更新 `MainWindow` 来使用 `QTableView` 和我们新创建的模型。

**操作**: 打开 `app/mainwindow.h` 文件。

1. 将 `<QTableWidget>` 的包含改为 `<QTableView>`。
2. 将 `QTableWidget*` 成员变量类型改为 `QTableView*`。
3. 添加一个指向 `VectorTableModel` 的新成员变量。
4. 添加 `#include "vector/vectortablemodel.h"`。

```diff
--- a/app/mainwindow.h
+++ b/app/mainwindow.h
@@ -10,13 +10,15 @@
 #include <QSplitter>
 #include <QTableView>
 #include <QTextEdit>
-#include <QTableWidget> // [REMOVE]
+#include <QTableView>   // [ADD]
 #include "database/databasemanager.h"
 #include "vector/vectordatahandler.h"
 #include "common/qcustomplot.h"
+#include "vector/vectortablemodel.h" // [ADD]
 
 class MainWindow : public QMainWindow
 {
 // ...
 private:
-    QTableWidget *m_vectorTable; // [REMOVE]
+    QTableView *m_vectorTableView; // [ADD]
+    VectorTableModel *m_vectorTableModel; // [ADD]
     // ... other member variables
 };
```

### 第5步：修改 `MainWindow` 的 UI 创建逻辑

**操作**: 打开 `app/mainwindow_setup.cpp` 文件。

1. 将 `new QTableWidget(...)` 修改为 `new QTableView(...)`。
2. **删除**所有针对 `m_vectorTable` 的、与数据行/列直接相关的配置代码。这包括但不限于 `setRowCount`, `setColumnCount`, `insertRow`, `setItem`, `clearContents` 等。这些职责现在完全由模型管理。
3. 保留通用的样式和行为设置，例如 `setSelectionBehavior`, `setContextMenuPolicy` 等。

```diff
--- a/app/mainwindow_setup.cpp
+++ b/app/mainwindow_setup.cpp
@@ -XX,13 +XX,12 @@
     // ...
 
-    m_vectorTable = new QTableWidget(this);
-    m_vectorTable->setColumnCount(5); // [REMOVE]
-    // ... 更多对 m_vectorTable 的旧配置 ... [REMOVE]
+    m_vectorTableView = new QTableView(this); // [MODIFY]
+    m_vectorTableView->setSelectionBehavior(QAbstractItemView::SelectRows); // [KEEP or ADD]
+    m_vectorTableView->horizontalHeader()->setStretchLastSection(true); // [KEEP or ADD]
+    // ... 保留任何通用的样式或行为设置 ...
 
-    mainLayout->addWidget(m_vectorTable);
+    mainLayout->addWidget(m_vectorTableView); // [MODIFY]
 
     // ...
 }
```

### 第6步：在 `MainWindow` 中集成模型和视图

**操作**: 修改 `MainWindow` 的构造函数和相关槽函数（主要在 `app/mainwindow.cpp` 和其他 `mainwindow_*.cpp` 文件中）。

1. **构造函数**: 实例化模型，并将其与视图关联。

    ```cpp
    // 在 MainWindow::MainWindow(...) 构造函数中
    
    // ... 在 m_vectorDataHandler 初始化之后 ...
    m_vectorTableModel = new VectorTableModel(m_vectorDataHandler, this);
    m_vectorTableView->setModel(m_vectorTableModel);
    ```

2. **切换向量表**: 当用户切换下拉框中的向量表时，不再手动加载数据到 `QTableWidget`，而是简单地通知模型更新其 `tableId`。

    ```cpp
    // 在 on_m_vectorTableSelector_currentIndexChanged(...) 槽函数中
    
    void MainWindow::on_m_vectorTableSelector_currentIndexChanged(int index)
    {
        int tableId = ui->m_vectorTableSelector->itemData(index).toInt();
        
        // 旧代码：循环、setItem 等... [REMOVE ALL]
        // loadVectorTablePageData(...); [REMOVE]
    
        // 新代码：
        m_vectorTableModel->setTableId(tableId);
    }
    ```

3. **数据变更操作**: 当执行添加、删除、修改行等操作后，不再直接操作UI控件，而是：
    a. 调用 `VectorDataHandler` 的方法更新后端数据。
    b. 调用 `m_vectorTableModel->refreshModel();` 通知模型刷新。

    ```cpp
    // 示例：添加10000行数据的操作
    void MainWindow::addTenThousandRows()
    {
        // 1. 调用 data handler 在后台添加数据
        // 这部分逻辑保持不变
        bool success = m_vectorDataHandler->addRows(m_currentTableId, 10000);
    
        // 2. 如果成功，刷新模型
        if (success) {
            m_vectorTableModel->refreshModel();
        }
    }
    ```

---

**重构完成。**
执行完以上所有步骤后，项目就已经成功迁移到了模型/视图架构。无论后端有多少数据，`QTableView` 都只会请求并渲染当前屏幕上可见的几十行，从而彻底解决了性能问题，并使得UI始终保持响应。
