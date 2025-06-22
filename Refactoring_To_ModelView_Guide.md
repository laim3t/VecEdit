# 100% 成功率 - QTableWidget 到 QTableView 重构AI执行手册 (优化版)

## 1. 核心目标与AI执行原则

**最终目标:** 将项目中导致性能瓶颈的 `QTableWidget` 彻底替换为高性能的 `QTableView` 和自定义的 `VectorTableModel` 模型。完成重构后，应用必须保持所有原有功能，并且在处理大量数据时 UI 流畅、无卡顿。

**AI执行原则 (必读):**
- **严格遵循步骤:** 你必须严格按照本文档定义的顺序，一步一步执行。禁止跳过、合并或改变任何步骤的顺序。
- **原子化操作:** 每个步骤都是一个不可再分的最小工作单元。在完成一个步骤的所有指令和验收标准之前，绝不开始下一步。
- **以验收为准:** 每个步骤都附有“验收标准”。这是你判断任务是否完成的唯一依据。必须 100% 满足所有验收条件才能认为该步骤已成功。
- **禁止创造:** 你的任务是精确执行指令，而不是提出替代方案或进行创造性修改。所有必要的决策都已在本手册中做出。

---

## 2. 总体规划

本次重构将分四个阶段进行，以确保过程的稳定和可控：

- **阶段一：奠定基础 (非破坏性)** - 创建新的模型类和扩展数据接口，但暂不触及现有 UI。
- **阶段二：核心切换 (只读模式)** - 用 `QTableView` 替换 `QTableWidget`，并连接新模型以显示数据。
- **阶段三：恢复交互功能** - 重新实现数据编辑、行/列操作等所有交互功能。
- **阶段四：最终清理与验证** - 移除所有废弃代码并进行最终测试。

---

## 阶段一：奠定基础 (非破坏性)

### **步骤 1: 创建 `VectorTableModel` 的文件框架**

*   **目标:** 创建新数据模型 `VectorTableModel` 的 `.h` 和 `.cpp` 文件。
*   **AI指令:**
    1.  创建文件 `vector/vectortablemodel.h`。
    2.  在文件中定义一个名为 `VectorTableModel` 的新类，它必须公开继承自 `QAbstractTableModel`。
    3.  在类定义上方，前置声明 `class VectorDataHandler;` 和 `namespace Vector { struct ColumnInfo; }`。
    4.  在类中，声明构造函数和析构函数。
    5.  声明并重写 (override) `QAbstractTableModel` 的四个核心虚函数：
        ```cpp
        int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        int columnCount(const QModelIndex &parent = QModelIndex()) const override;
        QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
        ```
    6.  创建文件 `vector/vectortablemodel.cpp`。
    7.  在该文件中包含 `"vectortablemodel.h"`，并为上述所有声明的函数提供最基础的空实现（例如，`rowCount` 返回 `0`，`data` 返回 `QVariant()`）。
*   **验收标准:**
    *   [ ] 文件 `vector/vectortablemodel.h` 和 `vector/vectortablemodel.cpp` 已按指令创建。
    *   [ ] **(关键)** 此时，项目 **必须** 仍然能够成功编译并通过。

### **步骤 2: 将新模型集成到构建系统**

*   **目标:** 让项目能够识别并编译 `VectorTableModel` 文件。
*   **AI指令:**
    1.  打开项目根目录下的 `CMakeLists.txt` 文件。
    2.  定位到 `target_sources(VecEdit PRIVATE ...)` 命令。
    3.  在该文件列表中，添加 `vector/vectortablemodel.h` 和 `vector/vectortablemodel.cpp`。
*   **验收标准:**
    *   [ ] `CMakeLists.txt` 文件已被修改。
    *   [ ] **(关键)** 项目 **必须** 能够再次成功编译。

### **步骤 3: 扩展 `VectorDataHandler` 以支持按需加载**

*   **目标:** 为 `VectorDataHandler` 添加一个新方法，使其能一次只获取一行数据。
*   **AI指令:**
    1.  打开 `vector/vectordatahandler.h` 文件。
    2.  在 `VectorDataHandler` 类的 `public` 部分，声明一个新的 `const` 成员函数：
        ```cpp
        QList<QVariant> getRow(int rowIndex) const;
        ```
    3.  打开 `vector/vectordatahandler.cpp` 文件。
    4.  实现 `getRow` 函数。
    5.  **实现逻辑:** 该函数的逻辑必须基于现有 `getAllVectorRows` 中的二进制文件读取逻辑。但它不应使用循环来读取所有行。相反，它需要计算出 `rowIndex` 对应的行在二进制文件中的准确字节偏移量，直接 `seek` 到该位置，只读取并解析那一单行的数据，然后将其作为 `QList<QVariant>` 返回。
*   **验收标准:**
    *   [ ] `VectorDataHandler` 类现在拥有 `getRow(int rowIndex) const` 方法。
    *   [ ] 项目 **必须** 能够成功编译。

### **步骤 4: 实现 `VectorTableModel` 的核心只读逻辑**

*   **目标:** 填充模型类的逻辑，使其能够通过 `VectorDataHandler` 加载数据。
*   **AI指令:**
    1.  打开 `vector/vectortablemodel.h` 文件:
        *   添加头文件包含: `#include "vector/vectordatahandler.h"` 和 `#include "vector/vector_data_types.h"`。
        *   在 `private` 部分添加成员变量：
            ```cpp
            VectorDataHandler* m_dataHandler = nullptr;
            QList<Vector::ColumnInfo> m_columnInfoList;
            int m_rowCount = 0;
            ```
        *   添加一个新的公共方法 `void loadTable(int tableId, VectorDataHandler* dataHandler);` 和 `void clear();`。
    2.  打开 `vector/vectortablemodel.cpp` 文件:
        *   **实现 `loadTable`:**
            1.  在函数顶部调用 `beginResetModel()`。
            2.  保存 `dataHandler`。
            3.  **重要:** 调用 `m_dataHandler->getVectorTableRowCount(tableId)` 来获取并保存行数到 `m_rowCount`。
            4.  **重要:** 调用 `m_dataHandler->getAllColumnInfo(tableId)` 来获取并保存列信息到 `m_columnInfoList`。
            5.  在函数末尾调用 `endResetModel()`。
        *   **实现 `clear`:** 调用 `beginResetModel()`, 清空成员变量, 然后调用 `endResetModel()`。
        *   **实现 `rowCount`:** `return m_rowCount;`。
        *   **实现 `columnCount`:** `return m_columnInfoList.size();`。
        *   **实现 `headerData`:** 如果 `orientation` 是 `Qt::Horizontal` 且 `role` 是 `Qt::DisplayRole`，返回 `m_columnInfoList.at(section).name`。否则返回 `QVariant()`。
        *   **实现 `data`:** 如果 `role` 是 `Qt::DisplayRole` 并且 `index` 有效：
            1.  调用 `m_dataHandler->getRow(index.row())` 来获取整行数据。
            2.  从返回的 `QList<QVariant>` 中，返回位于 `index.column()` 位置的 `QVariant`。
*   **验收标准:**
    *   [ ] `VectorTableModel` 类的所有方法都已按上述指令实现。
    *   [ ] 项目 **必须** 能够成功编译。

---

## 阶段二：核心切换 (只读模式)

### **步骤 5: 在 `MainWindow` 中用 `QTableView` 替换 `QTableWidget`**

*   **目标:** 在 `MainWindow` 的声明和 UI 构建代码中，将表格控件类型从 `QTableWidget` 更换为 `QTableView`。
*   **AI指令:**
    1.  打开 `app/mainwindow.h` 文件:
        *   移除 `#include <QTableWidget>`。
        *   添加 `#include <QTableView>`。
        *   添加头文件: `#include "vector/vectortablemodel.h"`。
        *   将成员变量 `QTableWidget *m_vectorTableWidget;` 修改为 `QTableView *m_vectorTableView;`。
        *   添加一个新的私有成员: `VectorTableModel *m_vectorTableModel;`。
    2.  打开 `app/mainwindow_setup.cpp` 文件，进入 `setupVectorTableUI` 函数:
        *   将 `m_vectorTableWidget = new QTableWidget(...)` 修改为 `m_vectorTableView = new QTableView(...)`。
        *   将该函数中所有对 `m_vectorTableWidget` 的引用全部更新为 `m_vectorTableView`。
        *   **注意:** `QTableView` 没有 `setRowCount`, `setColumnCount` 等方法。暂时注释掉所有不兼容的行。
*   **验收标准:**
    *   [ ] `MainWindow` 类成员已更新为 `QTableView*` 和 `VectorTableModel*`。
    *   [ ] `setupVectorTableUI` 中的实例化代码已更新。
    *   [ ] **(关键)** 此时，项目将 **无法编译**。这是正常的预期结果。

### **步骤 6: 关联模型与视图，并重构数据加载逻辑**

*   **目标:** 实例化模型，设置给 `QTableView`，并用模型驱动数据加载。
*   **AI指令:**
    1.  在 `MainWindow` 的构造函数 (`MainWindow::MainWindow` in `app/mainwindow_setup.cpp`) 中，实例化模型: `m_vectorTableModel = new VectorTableModel(this);`。
    2.  在 `app/mainwindow_setup.cpp` 的 `setupVectorTableUI` 函数末尾，设置模型: `m_vectorTableView->setModel(m_vectorTableModel);`。
    3.  **定位到 `app/mainwindow_dialogs_events.cpp` 文件中的 `openVectorTable` 函数。**
    4.  **彻底删除** 其中所有关于 `setRowCount`, `setColumnCount` 的调用，以及循环创建 `QTableWidgetItem` 并 `setItem` 的整个代码块。
    5.  用一行代码取而代之: `m_vectorTableModel->loadTable(tableId, &VectorDataHandler::instance());`。
*   **验收标准:**
    *   [ ] 旧的表格填充代码已被移除，并替换为对 `m_vectorTableModel->loadTable()` 的调用。
    *   [ ] **(关键)** 修复所有因 `QTableWidget` API 被移除而导致的编译错误，直到项目再次编译成功。
    *   [ ] 运行应用，打开一个项目，并选择一个向量表。
    *   [ ] **(验收核心)** 表格视图中 **必须** 正确地显示出所有数据，包括正确的行数、列数和表头。
    *   [ ] **(验收核心)** 上下滚动表格 **必须** 流畅，无任何卡顿。

### **步骤 7: 适配所有读取表格状态的代码**

*   **目标:** 修复所有之前从 `QTableWidget` 读取状态的代码。
*   **AI指令:** 在整个项目中搜索 `m_vectorTableWidget->` 定位并修改所有用法。
    1.  **获取选中项:** `m_vectorTableWidget->selectedItems()` 替换为 `m_vectorTableView->selectionModel()->selectedIndexes()`。 **注意:** 后者返回 `QModelIndexList`，你需要遍历它并使用 `.row()` 和 `.column()`。
    2.  **获取单元格数据:** `m_vectorTableWidget->item(row, col)->text()` 替换为 `m_vectorTableModel->data(m_vectorTableModel->index(row, col)).toString()`。
    3.  **获取行/列数:** `m_vectorTableWidget->rowCount()` 替换为 `m_vectorTableModel->rowCount()`。