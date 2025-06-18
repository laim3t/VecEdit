# UI层虚拟化重构任务简报 (UI Virtualization Refactoring Brief)

**致 AI 编程助手 (Claude 3.7 Sonnet):**

你好！接下来你需要执行一项针对本 Qt C++ 项目的UI层性能重构任务。本文档旨在为你提供所有必要的背景信息、目标和关键接触点，以帮助你高效地开展工作。

---

### 1. 任务背景与痛点 (Task Background & Pain Points)

- **当前实现**: 目前，项目的主数据显示界面位于 `MainWindow`，它使用一个 `QTableWidget` (`m_vectorTableWidget`) 来展示大量的向量数据。
- **核心问题**: `QTableWidget` 是一个非虚拟化的控件。当加载大量数据时，它会为每一个单元格创建并持有一个 `QTableWidgetItem`，这会导致严重的性能问题：
    1. **内存占用过高**: 加载数十万行数据就会消耗G级的内存，极易导致程序崩溃。
    2. **UI渲染冻结**: 一次性渲染大量控件会造成主线程长时间阻塞。
- **现有对策**: 为了规避此问题，项目实现了一套手动的**分页系统**（以 `m_paginationWidget` 为主的UI控件集）。这套系统通过每次只向 `QTableWidget` 提供一小部分（如100-500行）数据，暂时缓解了上述问题。
- **根本矛盾**: 分页方案仅仅是一个"创可贴"，它没有解决`QTableWidget`本身是性能瓶SN颈的根本事实。我们的目标是根除这一瓶颈。

---

### 2. 核心目标 (Core Objective)

本次重构的核心目标是：**用一个完全虚拟化的视图方案，取代现有的 `QTableWidget` 和手动分页系统。**

具体来说：

1. **替换控件**: 将 `QTableWidget` (`m_vectorTableWidget`) 替换为 `QTableView`。
2. **引入模型**: 创建一个新的数据模型类（例如，`VectorTableModel`），继承自 `QAbstractTableModel`，作为 `QTableView` 的数据源。
3. **移除分页**: 彻底移除手动分页相关的全部UI控件 (`m_paginationWidget`) 及其所有后台逻辑。
4. **最终效果**: 实现一个拥有单一、平滑滚动条的数据视图，它能够理论上处理百万甚至千万级的行数，而UI保持流畅，内存占用极低。

---

### 3. 前期信息与关键接触点 (Prep-Work & Key Touchpoints)

为了节省你的探索时间，以下是本次重构涉及的关键类和文件：

- **需要被修改的核心类**: `MainWindow`

- **需要被替换/移除的UI组件** (主要在 `MainWindow` 中):
  - `m_vectorTableWidget` (`QTableWidget`): **替换**为 `QTableView`。
  - `m_paginationWidget` (`QWidget`): **彻底移除**。其所有子控件 (如 `m_prevPageButton`, `m_nextPageButton`, `m_pageInfoLabel`, `m_pageSizeSelector` 等) 和相关槽函数也应一并移除。

- **需要进行修改的关键文件**:
  - `app/mainwindow.h`: 修改UI组件的成员变量声明。
  - `app/mainwindow_setup.cpp`: `setupVectorTableUI()` 函数是修改的重点，这里是UI组件创建和布局的地方。
  - `app/mainwindow_dialogs_events.cpp`: 这里包含了响应用户操作（如切换向量表）并触发数据加载的核心逻辑，例如 `onVectorTableSelectionChanged()` 和 `onTabChanged()`。你需要将这些逻辑修改为与新的数据模型交互。

- **数据源**:
  - `vector/vectordatahandler.h` (`VectorDataHandler` 类): 这是当前的数据访问层。新创建的 `VectorTableModel` 将会需要调用此类中的方法来按需获取数据。
  - 你可能会发现需要对 `VectorDataHandler` 添加新的接口（或微调现有接口），使其更适合按需、按范围（而不是直接操作 `QTableWidget`）地返回行数据。例如，创建一个返回 `QList<Vector::RowData>` 的函数。

---

### 4. 对实施方案的建议（非强制）

你的任务是设计并实现最佳的方案，以下仅为启动思路的提示，你可以自由发挥：

- **`VectorTableModel` 的职责**:
  - 它应作为 `QTableView` 和 `VectorDataHandler` 之间的桥梁。
  - 它**不应该**在内存中持有所有数据。
  - 它需要重写 `QAbstractTableModel` 的核心虚函数，如 `rowCount()`, `columnCount()`, `data()`, `headerData()`。
  - `rowCount()` 应直接返回数据集的总行数（可从 `VectorDataHandler` 获取）。
  - `data()` 是性能的关键。当 `QTableView` 请求特定单元格的数据时，此函数应高效地从缓存或磁盘中获取该数据。

- **按需加载与缓存**:
  - `VectorTableModel` 需要实现一套自己的数据缓存机制。当视图滚动时，按需从 `VectorDataHandler` 获取一定范围的行数据（例如，比当前可见范围稍大一些的"一页"）放入内部缓存，以优化滚动体验。
  - 缓存的设计（大小、预取策略等）由你来决定。

- **与 `MainWindow` 的集成**:
  - `MainWindow` 将创建并持有一个 `VectorTableModel` 的实例，并将其设置给新的 `QTableView`。
  - `MainWindow` 中所有的数据加载逻辑，现在都应委托给 `VectorTableModel` 的公共接口（例如，一个 `loadTable(tableId)` 的方法）。

### 5. 验收标准 (Definition of Done)

- `QTableWidget` 和分页系统已被完全移除。
- 应用界面现在使用 `QTableView`，并有一个能反映整个数据集长度的滚动条。
- 即使加载包含数百万行的数据表，应用启动和视图滚动也必须保持流畅，无明显卡顿。
- 所有与数据表格交互的旧有功能（例如，单元格编辑、右键菜单、行选择、数据填充等）必须在新方案下依然能够正常工作。

祝你顺利！
