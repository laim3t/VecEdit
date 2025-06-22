# 100%成功率 - QTableWidget 到 QTableView 重构AI执行手册 (最终版v3)

## 1. 核心目标与AI执行原则

**最终目标:** 将项目中导致性能瓶颈的 `QTableWidget` 彻底替换为高性能的 `QTableView` 和自定义的 `VectorTableModel` 模型。完成重构后，应用必须保持所有原有功能，并且在处理大量数据时 UI 流畅、无卡顿。

**AI执行原则 (必读):**
- **严格遵循步骤:** 你必须严格按照本文档定义的顺序，一步一步执行。禁止跳过、合并或改变任何步骤的顺序。
- **原子化操作:** 每个步骤都是一个不可再分的最小工作单元。在完成一个步骤的所有指令和验收标准之前，绝不开始下一步。
- **以验收为准:** 每个步骤都附有“验收标准”。这是你判断任务是否完成的唯一依据。必须 100% 满足所有验收条件才能认为该步骤已成功。
- **禁止创造:** 你的任务是精确执行指令，而不是提出替代方案或进行创造性修改。所有必要的决策都已在本手册中做出。

---
## 阶段一：奠定数据层基础 (非破坏性修改)

### **步骤 1: 创建 `VectorTableModel` 的文件框架**
*   **目标:** 创建新数据模型 `VectorTableModel` 的 `.h` 和 `.cpp` 文件。
*   **AI指令:**
    1.  创建文件 `vector/vectortablemodel.h`，内容如下:
        ```cpp
        #ifndef VECTORTABLEMODEL_H
        #define VECTORTABLEMODEL_H
        #include <QAbstractTableModel>
        #include "vector/vector_data_types.h" // 包含依赖
        
        // 前置声明
        class VectorDataHandler;
        
        class VectorTableModel : public QAbstractTableModel
        {
            Q_OBJECT
        
        public:
            explicit VectorTableModel(QObject *parent = nullptr);
            ~VectorTableModel();
        
            int rowCount(const QModelIndex &parent = QModelIndex()) const override;
            int columnCount(const QModelIndex &parent = QModelIndex()) const override;
            QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
            QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
        
        private:
            VectorDataHandler* m_dataHandler = nullptr;
            QList<Vector::ColumnInfo> m_columnInfoList;
            int m_rowCount = 0;
            int m_tableId = -1; // 提前添加tableId成员
        };
        #endif // VECTORTABLEMODEL_H
        ```
    2.  创建文件 `vector/vectortablemodel.cpp`，内容如下:
        ```cpp
        #include "vectortablemodel.h"
        #include "vector/vectordatahandler.h" // 包含依赖

        VectorTableModel::VectorTableModel(QObject *parent) : QAbstractTableModel(parent) {}
        VectorTableModel::~VectorTableModel() {}
        int VectorTableModel::rowCount(const QModelIndex &parent) const { return 0; }
        int VectorTableModel::columnCount(const QModelIndex &parent) const { return 0; }
        QVariant VectorTableModel::data(const QModelIndex &index, int role) const { return QVariant(); }
        QVariant VectorTableModel::headerData(int section, Qt::Orientation orientation, int role) const { return QVariant(); }
        ```
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

### **步骤 3: 为 `VectorDataHandler` 添加独立的数据接口**
*   **目标:** 为 `VectorDataHandler` 添加不依赖 `QTableWidget` 的新方法，为后续解耦做准备。
*   **AI指令:**
    1.  打开 `vector/vectordatahandler.h` 文件。
    2.  在前置声明区域，添加 `class VectorTableModel;`。
    3.  在 `public:` 部分，添加以下三个新的函数声明：
        ```cpp
        // 新增：获取单行数据，以QVariantList形式返回，供模型使用
        QList<QVariant> getRowForModel(int tableId, int rowIndex);

        // 新增：更新单个单元格数据
        bool updateCellValue(int tableId, int rowIndex, int columnIndex, const QVariant &value);

        // 新增：保存来自模型的数据（替代旧的saveVectorTableDataPaged）
        bool saveVectorTableModelData(int tableId, VectorTableModel *tableModel, int currentPage, int pageSize, int totalRows, QString &errorMessage);
        ```
    4. 打开 `vector/vectordatahandler.cpp` 文件。
    5. **(重要)** 添加新头文件包含 `#include "vector/vectortablemodel.h"`。
    6. 在文件末尾，为上述三个新函数添加空的实现体，确保能够编译通过。示例如下：
        ```cpp
        QList<QVariant> VectorDataHandler::getRowForModel(int tableId, int rowIndex) {
            // TODO: 在后续步骤中实现
            Q_UNUSED(tableId); Q_UNUSED(rowIndex);
            return QList<QVariant>();
        }

        bool VectorDataHandler::updateCellValue(int tableId, int rowIndex, int columnIndex, const QVariant &value) {
            // TODO: 在后续步骤中实现
            Q_UNUSED(tableId); Q_UNUSED(rowIndex); Q_UNUSED(columnIndex); Q_UNUSED(value);
            return false;
        }
        
        bool VectorDataHandler::saveVectorTableModelData(int tableId, VectorTableModel *tableModel, int currentPage, int pageSize, int totalRows, QString &errorMessage) {
            // TODO: 在后续步骤中实现
            Q_UNUSED(tableId); Q_UNUSED(tableModel); Q_UNUSED(currentPage); Q_UNUSED(pageSize); Q_UNUSED(totalRows); Q_UNUSED(errorMessage);
            return false;
        }
        ```
*   **验收标准:**
    *   [ ] `VectorDataHandler` 类已拥有三个新的空方法。
    *   [ ] 项目 **必须** 能够成功编译。

### **步骤 4: 实现 `getRowForModel`**
*   **目标:** 填充 `getRowForModel` 函数的逻辑，使其能按需从二进制文件中读取单行数据。
*   **AI指令:**
    1. 打开 `vector/vectordatahandler.cpp`。
    2. 找到 `getAllVectorRows` 函数，复制其内部关于打开文件、读取元数据和列信息的逻辑。
    3. 用复制的逻辑重写 `getRowForModel` 的空实现。
    4. **核心逻辑:**
        *   不要一次性读取所有行。
        *   必须通过循环读取每一行的字节长度，累加偏移量，直到找到 `rowIndex` 对应的行的起始位置。**（注意：行的长度是可变的，不能通过 `行号 * 固定长度` 来计算偏移量）**
        *   `seek()` 到目标位置，只读取那一单行的数据。
        *   反序列化该行数据为 `Vector::RowData`。
        *   将 `Vector::RowData` 转换为 `QList<QVariant>` 并返回。
        *   **必须** 正确处理缓存（`m_tableDataCache`），如果缓存命中则直接从缓存返回。
*   **验收标准:**
    *   [ ] `getRowForModel` 已被正确实现。
    *   [ ] 项目 **必须** 能够成功编译。

### **步骤 5: 实现 `VectorTableModel` 的核心只读逻辑**
*   **目标:** 填充模型类的逻辑，使其能够通过 `VectorDataHandler` 加载数据。
*   **AI指令:**
    1.  在 `vector/vectortablemodel.h` 中添加两个新公有函数声明:
        ```cpp
        void loadTable(int tableId, VectorDataHandler* dataHandler);
        void clear();
        ```
    2.  打开 `vector/vectortablemodel.cpp` 文件，用以下内容替换所有现有函数实现：
        ```cpp
        #include "vectortablemodel.h"
        #include "vector/vectordatahandler.h"

        VectorTableModel::VectorTableModel(QObject *parent) : QAbstractTableModel(parent) {}
        VectorTableModel::~VectorTableModel() {}

        void VectorTableModel::loadTable(int tableId, VectorDataHandler* dataHandler) {
            beginResetModel();
            m_dataHandler = dataHandler;
            m_tableId = tableId;
            if (m_dataHandler) {
                m_rowCount = m_dataHandler->getVectorTableRowCount(m_tableId);
                m_columnInfoList = m_dataHandler->getAllColumnInfo(m_tableId);
            } else {
                m_rowCount = 0;
                m_columnInfoList.clear();
            }
            endResetModel();
        }

        void VectorTableModel::clear() {
            beginResetModel();
            m_dataHandler = nullptr;
            m_tableId = -1;
            m_rowCount = 0;
            m_columnInfoList.clear();
            endResetModel();
        }
        
        int VectorTableModel::rowCount(const QModelIndex &parent) const {
            return parent.isValid() ? 0 : m_rowCount;
        }
        
        int VectorTableModel::columnCount(const QModelIndex &parent) const {
            return parent.isValid() ? 0 : m_columnInfoList.size();
        }
        
        QVariant VectorTableModel::data(const QModelIndex &index, int role) const {
            if (!index.isValid() || !m_dataHandler || role != Qt::DisplayRole) {
                return QVariant();
            }
            // 使用新函数 getRowForModel
            QList<QVariant> rowData = m_dataHandler->getRowForModel(m_tableId, index.row());
            if (index.column() < rowData.size()) {
                return rowData.at(index.column());
            }
            return QVariant();
        }
        
        QVariant VectorTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
            if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
                if (section >= 0 && section < m_columnInfoList.size()) {
                    return m_columnInfoList.at(section).name;
                }
            }
            return QVariant();
        }
        ```
*   **验收标准:**
    *   [ ] `VectorTableModel` 类的所有方法都已按上述指令实现。
    *   [ ] 项目 **必须** 能够成功编译。

---
## 阶段二：核心切换与适配

### **步骤 6: 在 `MainWindow` 中用 `QTableView` 替换 `QTableWidget`**
*   **目标:** 在 `MainWindow` 的声明中，将表格控件类型从 `QTableWidget` 更换为 `QTableView`。
*   **AI指令:**
    1.  打开 `app/mainwindow.h` 文件:
        *   找到并删除 `#include <QTableWidget>`。
        *   在头文件包含区，添加 `#include <QTableView>`。
        *   在头文件包含区，添加 `#include "vector/vectortablemodel.h"`。
        *   找到成员变量 `QTableWidget *m_vectorTableWidget;` 并将其修改为 `QTableView *m_vectorTableView;`。
        *   在其下方添加一个新的私有成员: `VectorTableModel *m_vectorTableModel;`。
*   **验收标准:**
    *   [ ] `MainWindow` 类成员已更新为 `QTableView*` 和 `VectorTableModel*`。
    *   [ ] **(关键)** 此时，项目将 **大量报错且无法编译**。这是正常的预期结果。

### **步骤 7: 适配UI初始化代码 (`mainwindow_setup.cpp`)**
*   **目标:** 修改UI创建代码，实例化新控件和模型。
*   **AI指令:**
    1.  打开 `app/mainwindow_setup.cpp`。
    2.  在 `MainWindow::MainWindow` 构造函数中，紧随 `setupUI()` 之后，添加模型实例化代码：`m_vectorTableModel = new VectorTableModel(this);`。
    3.  在 `setupVectorTableUI` 函数中，将 `m_vectorTableWidget = new QTableWidget(...)` 修改为 `m_vectorTableView = new QTableView(...)`。
    4.  紧随其后，添加 `m_vectorTableView->setModel(m_vectorTableModel);`。
    5.  将该函数中所有对 `m_vectorTableWidget` 的引用全部更新为 `m_vectorTableView`。
    6.  **重要：** `QTableView` 没有 `cellChanged` 信号。找到 `connect(m_vectorTableWidget, &QTableWidget::cellChanged, ...)` 这行，将其**完全删除或注释掉**。我们将在后续步骤中用模型信号替代它。
    7.  **重要：** `QTableView` 没有 `cellWidget` 信号或方法。找到所有与 `cellWidget` 相关的 `connect` 语句，将其**完全删除或注释掉**。
*   **验收标准:**
    *   [ ] `setupVectorTableUI` 中的实例化和 connect 代码已按指令更新。
    *   [ ] 项目**仍然无法编译**，但与此文件相关的部分错误可能已解决。

### **步骤 8: 适配数据加载逻辑 (`mainwindow_dialogs_events.cpp`)**
*   **目标:** 使用模型加载数据，替代旧的 `QTableWidget` 数据填充方式。
*   **AI指令:**
    1.  打开 `app/mainwindow_dialogs_events.cpp`。
    2.  定位到 `openVectorTable` 函数。
    3.  **彻底删除** 其中所有关于 `setRowCount`, `setColumnCount` 的调用，以及循环创建 `QTableWidgetItem` 并 `setItem` 的整个代码块。
    4.  用一行代码取而代之: `m_vectorTableModel->loadTable(tableId, &VectorDataHandler::instance());`。
    5.  将此函数中所有对 `m_vectorTableWidget` 的引用全部更新为 `m_vectorTableView`。将 `m_vectorTableWidget->rowCount()` 或 `columnCount()` 替换为 `m_vectorTableModel->rowCount()` 或 `columnCount()`。
    6.  定位到 `onVectorTableSelectionChanged` 函数，执行与 `openVectorTable` 相同的替换操作。
    7.  将 `TableStyleManager::applyBatchTableStyle(m_vectorTableWidget);` 替换为 `TableStyleManager::applyBatchTableStyle(m_vectorTableView);`。
*   **验收标准:**
    *   [ ] 数据加载逻辑已更新为 `m_vectorTableModel->loadTable()`。
    *   [ ] 项目**仍然无法编译**。

### **步骤 9: 全局适配其余API调用**
*   **目标:** 逐一修复剩下的、分散在项目中的 `QTableWidget` API调用。
*   **AI指令:**
    1.  在 `app/mainwindow_project_tables.cpp` 的 `deleteCurrentVectorTable` 函数中，将 `m_vectorTableWidget->setRowCount(0);` 和 `setColumnCount(0)` 两行替换为 `m_vectorTableModel->clear();`。
    2.  在 `app/mainwindow_pins_timeset.cpp` 的 `showFillTimeSetDialog` 和 `showReplaceTimeSetDialog` 函数中，将 `m_vectorTableWidget->selectionModel()` 替换为 `m_vectorTableView->selectionModel()`。
    3.  在 `app/mainwindow_data_operations.cpp` 的 `deleteSelectedVectorRows` 和 `gotoLine` 函数中，进行相同的替换。
    4.  在 `app/mainwindow_vector_fill.cpp` 的 `fillVectorForVectorTable` 函数中，进行相同的替换。
    5.  **全局搜索** `m_vectorTableWidget`，将所有剩余的简单引用（非函数调用）替换为 `m_vectorTableView`。
    6.  **全局搜索** `->rowCount()`，如果其调用者是 `m_vectorTableView`，则修正为 `m_vectorTableModel->rowCount()`。对 `columnCount` 做同样处理。
    7.  **全局搜索** `->selectedItems()`，将其替换为 `->selectionModel()->selectedIndexes()`。并注意处理返回类型的差异。
*   **验收标准:**
    *   [ ] 所有 `m_vectorTableWidget` 的引用已被替换。
    *   [ ] **(关键)** 项目此时 **必须** 能够成功编译。
    *   [ ] 运行应用，打开一个向量表。表格 **必须** 能正确显示数据，且滚动流畅。所有只读功能应该正常。

---
## 阶段三：恢复交互功能

### **步骤 10: 实现单元格数据编辑 (`setData`)**
*   **目标:** 让用户能够再次编辑数据，并将修改持久化。
*   **AI指令:**
    1.  打开 `vector/vectortablemodel.h`，添加以下两个 `override` 函数声明：
        ```cpp
        Qt::ItemFlags flags(const QModelIndex &index) const override;
        bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
        ```
    2.  打开 `vector/vectortablemodel.cpp`，添加 `flags` 和 `setData` 的实现：
        ```cpp
        Qt::ItemFlags VectorTableModel::flags(const QModelIndex &index) const {
            if (!index.isValid()) return Qt::NoItemFlags;
            return QAbstractTableModel::flags(index) | Qt::ItemIsEditable;
        }

        bool VectorTableModel::setData(const QModelIndex &index, const QVariant &value, int role) {
            if (role == Qt::EditRole && m_dataHandler) {
                if (m_dataHandler->updateCellValue(m_tableId, index.row(), index.column(), value)) {
                    emit dataChanged(index, index, {role});
                    return true;
                }
            }
            return false;
        }
        ```
    3. 打开 `vector/vectordatahandler.cpp`，找到之前添加的空的 `updateCellValue` 函数，并为其添加真实逻辑：通过SQL `UPDATE` 语句更新数据库中的特定字段，并更新二进制文件中的对应数据。
*   **验收标准:**
    *   [ ] `flags` 和 `setData` 已实现。 `updateCellValue` 已实现。
    *   [ ] **(关键)** 在UI中双击单元格，进入编辑模式。修改数据后，数据被成功保存。

*(后续步骤，如行/列操作、清理等，也应遵循同样极度精细化的原则编写，但为简洁起见，此处省略。核心的最难部分已经覆盖。)*

---
**重构继续。**