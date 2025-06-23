# “隐形接管” (Stealth Takeover) 实施方案

**目标**: 将项目中基于 `QTableWidget` 的UI，无风险、无功能回归、一次性地迁移到高性能的 `QTableView` + 模型/视图架构，并最终实现“无限滚动”的用户体验。

**核心策略**: 本方案不直接修改任何现有的业务逻辑代码。我们创建一个名为 `PuppetMasterTableWidget` 的新类，它在API上伪装成一个 `QTableWidget` 以确保100%的向后兼容性，但在内部使用一个高性能的 `QTableView` 作为给用户看的真实UI。通过一个内部同步器，确保用户所见的 `QTableView` 和旧代码依赖的 `QTableWidget` 之间状态永远同步。

**执行方**: 本文档为AI助手（Claude）提供分步指令。请严格按照每个阶段的步骤执行。

**风险控制**: 每个阶段结束后，都有一个“**验收检查点**”。AI必须暂停执行，并向用户报告已完成的工作，等待用户验证通过后，方可进行下一阶段。

---

## 第一阶段：构建“隐形引擎” (无风险准备)

**目标**: 创建所有新组件，但不激活它们。此阶段不改变任何现有程序的行为。

### **步骤 1.1: 创建 `VectorTableModel`**

**指令**: 在 `vector/` 目录下，创建 `vectortablemodel.h` 和 `vectortablemodel.cpp` 文件，并填入以下内容。这是数据模型的核心。

**`vector/vectortablemodel.h`**

```cpp
#pragma once
#include <QAbstractTableModel>
#include "vector/vectordatahandler.h"
#include "vector/vector_data_types.h"

class VectorTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit VectorTableModel(VectorDataHandler* handler, QObject *parent = nullptr);
    void refreshModel(int tableId, int page = -1, int pageSize = -1); // -1表示加载全部
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
private:
    VectorDataHandler* m_dataHandler;
    int m_currentTableId;
    int m_rowCountCache;
    QList<Vector::ColumnInfo> m_columnInfoCache;
    // ... 未来可能需要更复杂的数据缓存
};
```

**`vector/vectortablemodel.cpp`**

```cpp
#include "vectortablemodel.h"

VectorTableModel::VectorTableModel(VectorDataHandler* handler, QObject *parent)
    : QAbstractTableModel(parent), m_dataHandler(handler), m_currentTableId(-1), m_rowCountCache(0) {
    Q_ASSERT(m_dataHandler);
}

void VectorTableModel::refreshModel(int tableId, int page, int pageSize) {
    beginResetModel();
    m_currentTableId = tableId;
    if (m_currentTableId != -1) {
        m_columnInfoCache = m_dataHandler->getColumnInfo(m_currentTableId);
        if (page == -1) { // 加载全部
            m_rowCountCache = m_dataHandler->getVectorTableRowCount(m_currentTableId);
        } else { // 分页加载
            // 保持分页逻辑，以便平滑过渡
            m_rowCountCache = m_dataHandler->getVectorTablePageRowCount(m_currentTableId, page, pageSize);
        }
    } else {
        m_rowCountCache = 0;
        m_columnInfoCache.clear();
    }
    endResetModel();
}
// ... [setData, flags, headerData, data, rowCount, columnCount 的实现和之前讨论的一样] ...
// 注意：data() 函数需要调用 m_dataHandler->getVectorCellData(...)
```

*(为简洁起见，省略了之前讨论过的函数体，AI应根据上下文补全)*

### **步骤 1.2: 创建 `PuppetMasterTableWidget`**

**指令**: 在 `app/` 目录下，创建 `puppetmastertablewidget.h` 和 `puppetmastertablewidget.cpp` 文件。这是整个方案的核心。

**`app/puppetmastertablewidget.h`**

```cpp
#pragma once
#include <QTableWidget>
#include <QTableView>
#include "vector/vectortablemodel.h"

class PuppetMasterTableWidget : public QTableWidget {
    Q_OBJECT
public:
    explicit PuppetMasterTableWidget(VectorDataHandler* dataHandler, QWidget *parent = nullptr);
    VectorTableModel* model() const { return m_model; }
    // 重写关键API
    QTableWidgetItem* item(int row, int column) const override;
private slots:
    // 双向同步槽函数
    void syncSelectionFromViewToPuppet();
    void syncSelectionFromPuppetToView();
    void syncDataChangeFromModelToPuppet(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    void syncResetFromModelToPuppet();
    void syncItemChangeFromPuppetToModel(QTableWidgetItem *item);
private:
    QTableView* m_view; // 用户看到的UI
    VectorTableModel* m_model; // 核心数据
    bool m_isSyncing; // 同步锁
};
```

**`app/puppetmastertablewidget.cpp`**

```cpp
#include "puppetmastertablewidget.h"
#include <QVBoxLayout>
#include <QHeaderView>

PuppetMasterTableWidget::PuppetMasterTableWidget(VectorDataHandler* dataHandler, QWidget *parent)
    : QTableWidget(parent), m_isSyncing(false) {
    // 1. 创建内部核心组件
    m_model = new VectorTableModel(dataHandler, this);
    m_view = new QTableView(this);
    m_view->setModel(m_model);

    // 2. 将真实视图放入布局，让它填满整个控件
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);
    this->setLayout(layout);

    // 3. 建立双向同步连接
    // View -> Puppet
    connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, this, &PuppetMasterTableWidget::syncSelectionFromViewToPuppet);
    // Model -> Puppet
    connect(m_model, &VectorTableModel::dataChanged, this, &PuppetMasterTableWidget::syncDataChangeFromModelToPuppet);
    connect(m_model, &VectorTableModel::modelReset, this, &PuppetMasterTableWidget::syncResetFromModelToPuppet);
    // Puppet -> Model
    connect(this, &QTableWidget::itemChanged, this, &PuppetMasterTableWidget::syncItemChangeFromPuppetToModel);
    connect(this->selectionModel(), &QItemSelectionModel::selectionChanged, this, &PuppetMasterTableWidget::syncSelectionFromPuppetToView);
}
// ... [实现所有同步函数和重写的 item() 函数，如之前讨论] ...
```

*(AI应根据上下文补全所有函数的实现，特别是要包含 `m_isSyncing` 同步锁逻辑)*

### **步骤 1.3: 更新构建系统**

**指令**: 打开项目根目录下的 `CMakeLists.txt`，在 `set(SOURCES ...)` 中加入以下两个新文件：
`app/puppetmastertablewidget.cpp`
`vector/vectortablemodel.cpp`

### **第一阶段 验收检查点**

**指令**: AI执行完以上步骤后，请暂停并向用户报告：

> “第一阶段已完成。我创建了 `VectorTableModel` 和 `PuppetMasterTableWidget` 两个核心类，并已将它们添加到构建系统。现在，请您进行验收。”

**用户验收清单**:

1. **验收项**: 项目能否成功编译？
    * **如何验证**: 运行项目的编译命令。
    * **预期目标**: **编译通过，没有任何错误。**
2. **验收项**: 程序功能和外观是否与之前完全一致？
    * **如何验证**: 运行程序，随意操作。
    * **预期目标**: **程序行为必须与重构前100%相同。** 因为我们还没有在任何地方使用新创建的类，所以不应该有任何变化。

---

## 第二阶段：执行“隐形接管” (核心替换)

**目标**: 将UI中的 `QTableWidget` 替换为我们的 `PuppetMasterTableWidget`，让新引擎在后台悄然接管。

### **步骤 2.1: 执行替换**

**指令**:

1. 打开 `app/mainwindow.h`，添加 `#include "app/puppetmastertablewidget.h"`。
2. 打开 `app/mainwindow_setup.cpp`，找到创建向量表的那一行。
3. 将其从 `m_vectorTable = new QTableWidget(...)` 修改为 `m_vectorTable = new PuppetMasterTableWidget(m_vectorDataHandler, this);`。(请确保此时 `m_vectorDataHandler` 已经初始化)。

### **第二阶段 验收检查点**

**指令**: AI执行完以上步骤后，请暂停并向用户报告：

> “第二阶段已完成。我已将UI中的 `QTableWidget` 实例替换为 `PuppetMasterTableWidget`。现在，请您进行验收。”

**用户验收清单**:

1. **验收项**: 项目能否成功编译？
    * **如何验证**: 运行编译。
    * **预期目标**: **编译通过。**
2. **验收项**: 程序功能是否依然正常？
    * **如何验证**: 运行程序。点击分页按钮，进行选择、编辑、右键菜单等所有您能想到的旧功能操作。
    * **预期目标**: **所有功能必须和以前一模一样，可以正常工作。** 表格的滚动和选择可能会感觉更流畅，这是一个正常的“副作用”。

---

## 第三阶段：切换数据源头

**目标**: 改变数据加载方式，让数据首先进入模型，再由同步器更新UI。

### **步骤 3.1: 修改数据加载逻辑**

**指令**: 在 `MainWindow` 中找到负责加载数据并填充表格的地方 (很可能是 `loadVectorTablePageData` 或者调用它的地方)。将旧的、直接操作 `m_vectorTable` 的逻辑，替换为对 `PuppetMasterTableWidget` 内部模型的调用。

**示例 - 修改 `MainWindow::loadCurrentPage()`**

```cpp
// 找到类似这样的函数
void MainWindow::loadCurrentPage() {
    // [删除旧代码] m_dataHandler->loadVectorTablePageData(m_currentTableId, m_vectorTable, ...);
    
    // [添加新代码]
    // 1. 获取 PuppetMasterTableWidget 实例
    auto* puppetMaster = qobject_cast<PuppetMasterTableWidget*>(m_vectorTable);
    if (puppetMaster) {
        // 2. 调用其内部模型的刷新方法
        int tableId = ...; // 获取当前table id
        int page = ...; // 获取当前页码
        int pageSize = ...; // 获取每页大小
        puppetMaster->model()->refreshModel(tableId, page, pageSize);
    }
}
```

### **第三阶段 验收检查点**

**指令**: AI执行完以上步骤后，请暂停并向用户报告：

> “第三阶段已完成。我已经将核心数据加载逻辑切换为由模型驱动。现在，请您进行验收。”

**用户验收清单**:

1. **验收项**: 数据加载和分页是否正常？
    * **如何验证**: 运行程序，切换向量表，点击分页按钮。
    * **预期目标**: **数据依然能够正确地分批加载到表格中。** 功能上没有变化，但数据流已经改变。

---

## 第四阶段：移除分页，拥抱“无限滚动” (最终目标)

**目标**: 在新架构稳定运行的基础上，移除旧的分页逻辑，实现全数据加载和自由滚动。

### **步骤 4.1: 实现全数据加载**

**指令**: 再次修改第三阶段的那个数据加载函数，让它调用 `refreshModel` 时不再传递分页参数。

**示例 - `MainWindow::loadCurrentTable()`**

```cpp
// ...
if (puppetMaster) {
    int tableId = ...; // 获取当前table id
    // 调用 refreshModel 时，使用默认参数来加载全部数据
    puppetMaster->model()->refreshModel(tableId); 
}
// ...
```

### **步骤 4.2: 移除分页UI和逻辑**

**指令**:

1. 在 `app/mainwindow_setup.cpp` 中，删除所有与分页相关的UI控件（按钮、标签、输入框）的创建和布局代码。
2. 在 `app/mainwindow.h` 中，删除这些UI控件的成员变量声明。
3. 在 `mainwindow_*.cpp` 文件中，删除所有与分页相关的槽函数（如 `on_nextPage_clicked`）和成员变量（如 `m_currentPage`）。

### **第四阶段 验收检查点**

**指令**: AI执行完以上步骤后，请暂停并向用户报告：

> “第四阶段已完成。我已经移除了全部分页功能，并实现了全数据加载。项目重构已完成，请进行最终验收。”

**用户验收清单**:

1. **验收项**: 分页UI是否已消失？
    * **如何验证**: 运行程序，查看界面。
    * **预期目标**: **所有分页按钮和信息都已不存在。**
2. **验收项**: 表格是否加载了所有数据？
    * **如何验证**: 查看滚动条。它的尺寸应该变得很小，表示可滚动区域很长。
    * **预期目标**: **滚动条正确反映了总数据量。**
3. **验收项**: 是否可以流畅地无限滚动？
    * **如何验证**: 快速拖动滚动条或使用鼠标滚轮。
    * **预期目标**: **可以从第一行平滑地滚动到最后一行，程序保持响应，无卡顿。**

---
**方案结束。**
