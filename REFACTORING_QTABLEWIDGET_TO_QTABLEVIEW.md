# 重构指南：从 QTableWidget 迁移到 QTableView

## 引言

本文档是为AI助手准备的一份详细、安全的重构操作手册。

- **最终目标**: 使用高性能的 `QTableView` + `QAbstractTableModel` 架构，彻底取代现有的 `QTableWidget`。
- **核心原则**: 增量、安全、可验证。每完成一步，我们都会进行验证。
- **首要任务**: **显示优先**。我们将以最快速度让新表格把数据显示出来，即便它最初是只读的。

### **重要架构说明：关于"实现-包含模式"**

**AI请注意**：在开始重构前，你需要了解本项目的一个核心设计模式。

为了避免单个实现文件过长，本项目对多个核心类（如 `MainWindow`, `VectorDataHandler` 等）广泛采用了"**实现-包含模式**"（Implementation-Inclusion Pattern）。具体来说：

1. 一个类的成员函数实现，被按功能拆分到了多个 `_<feature>.cpp` 辅助文件中（例如 `mainwindow_data_operations.cpp`）。
2. 该类的主实现文件（例如 `mainwindow.cpp`）会作为"编译集线器"，在末尾负责 `#include` 所有这些辅助实现文件。
3. 因此，**任何为现有类新建的辅助实现文件（如 `classname_feature.cpp`）都不需要、也不应该被添加到 `CMakeLists.txt` 中**。
4. 所有需要的头文件都在主实现文件中统一管理。

在执行本指南的任务时，请遵循这一通用模式。

### **核心挑战与迁移难点分析**

**AI请注意**：在开始前，你必须了解当前 `QTableWidget` 实现中的几个关键复杂点。在迁移过程中，这些点不能被直接复制，而必须被"翻译"成符合Model/View架构的正确实现。

1. **【最重要】自定义单元格控件 (`setCellWidget`)**:
    - **现状**: 项目中多处使用 `table->setCellWidget(...)` 将自定义控件（如用于选择管脚状态的`QComboBox`）直接放入单元格。
    - **迁移陷阱**: **绝对不能**在新的架构中尝试做同样的事情。Model/View架构禁止将Widget存入模型。
    - **正确路径**: 所有使用 `setCellWidget` 的功能，都必须通过在 `QTableView` 上设置一个自定义的 `QStyledItemDelegate` 来重新实现。这个Delegate将负责：
        - `createEditor()`: 在用户编辑时，按需创建那个`QComboBox`。
        - `setEditorData()` / `setModelData()`: 在编辑器和模型之间同步数据。
        - `paint()`: 在单元格未被编辑时，将其内容正确地绘制出来。
    - *这在步骤2.3中有详细说明，但你需要从一开始就意识到这是我们的核心转换任务之一。*

2. **右键上下文菜单**:
    - **现状**: 右键菜单的逻辑很可能依赖于 `itemAt(pos)` 来获取一个 `QTableWidgetItem`，然后根据这个Item的属性来决定显示哪些菜单项。
    - **迁移陷阱**: 新的视图没有 `itemAt`。直接复制粘贴代码会失败。
    - **正确路径**: 相应的槽函数必须被重构。它需要使用 `indexAt(pos)` 来获取一个 `QModelIndex`，然后通过 `model->data(index, ...)` 来查询模型，从而获取判断所需的数据。

3. **依赖于Item的逻辑**:
    - **现状**: 项目中可能存在直接操作 `QTableWidgetItem` 的代码（例如，`item->setText(...)`, `item->setBackground(...)`）。
    - **迁移陷阱**: 所有对 `QTableWidgetItem` 的直接引用都将失效。
    - **正确路径**: 所有这些操作都必须转变为对**模型**的操作。例如，修改文本应调用 `model->setData(...)`，修改背景色应在模型的 `data()` 函数中，根据索引返回一个 `QBrush` 给 `Qt::BackgroundRole`。

---

## 第一阶段：奠定基石 —— 创建模型并显示数据

*此阶段的核心目标：在不破坏现有功能的前提下，让一个新的`QTableView`能够成功加载并显示向量表的数据。*

### 步骤1.1：创建数据模型的文件框架

**任务**: 创建 `VectorTableModel` 类所需的基础文件。

**AI请注意**:

1. 在 `app/` 目录下创建两个新文件：`vectortablemodel.h` 和 `vectortablemodel.cpp`。
2. 在 `.h` 文件中，定义一个名为 `VectorTableModel` 的新类，它必须公开继承自 `QAbstractTableModel`。
3. **非常重要**：找到项目的 `CMakeLists.txt` 文件，在 `add_executable` 或 `target_sources` 部分，将 `app/vectortablemodel.cpp` 添加进去。如果遗漏此步骤，项目将无法编译。

### 步骤1.2：实现一个最小化的数据模型

**任务**: 为 `VectorTableModel` 实现 `QAbstractTableModel` 的核心虚函数，使其能够被编译和实例化。

**AI请注意**:

1. 在 `vectortablemodel.h` 中，声明 `rowCount()`, `columnCount()`, `data()` 和 `headerData()` 这四个函数的重写。
2. 在 `vectortablemodel.cpp` 中，为这些函数提供最简单的实现。此时，`rowCount()` 和 `columnCount()` 可以直接返回 `0`，`data()` 可以返回一个空的 `QVariant()`。目标仅仅是让这个类变得完整和可用。

### 步骤1.3：在界面中引入新的TableView（与旧的并存）

**任务**: 在主窗口中创建新的`QTableView`和`VectorTableModel`实例，并将新视图与旧视图并排安放，以便随时对比。

**AI请注意**:

1. 本项目UI为纯C++代码，没有 `.ui` 文件。请首先在 `app/mainwindow.h` 的私有成员区，声明两个新的指针成员变量：`QTableView* m_vectorTableView;` 和 `VectorTableModel* m_vectorTableModel;`。
2. 然后，你需要找到UI的初始化代码。请在 `app/` 目录中搜索 `m_vectorTableWidget` 的创建位置，这很可能在名为 `mainwindow_setup... .cpp` 的文件中。
3. 在找到的UI初始化函数中，实例化这两个新成员：`m_vectorTableModel = new VectorTableModel(this);` 和 `m_vectorTableView = new QTableView(this);`。
4. **关键步骤**: 找到容纳旧 `m_vectorTableWidget` 的布局(Layout)。创建一个 `QStackedWidget` 实例，将**旧的 `m_vectorTableWidget`** 作为第一个页面、**新的 `m_vectorTableView`** 作为第二个页面，都添加到这个 `QStackedWidget` 中。然后将 `QStackedWidget` 放入原先的布局中。这样就实现了新旧共存。
5. 最后，将新模型设置给新视图：`m_vectorTableView->setModel(m_vectorTableModel);`。

### 步骤1.4：让模型具备加载数据的能力

**任务**: 赋予`VectorTableModel`从`VectorDataHandler`获取一页数据并管理它的能力。这是实现Model/View分离的核心。

**AI请注意**:

1. 在 `vectortablemodel.h` 中，为 `VectorTableModel` 添加私有成员变量，用于存储分页状态（如当前页、总行数）和最重要的数据容器：`QList<Vector::RowData> m_pageData;` 以及列信息 `QList<Vector::ColumnInfo> m_visibleColumns;`。
2. **创建新的数据接口**: 在 `database/vectordatahandler.h` 和相应的`.cpp`文件中，创建一个**新的**公共函数，例如 `getPageData(int tableId, int page, int pageSize)`。这个新函数与现有的 `loadVectorTablePageData` 非常相似，但有一个关键区别：它不接受 `QTableWidget*` 参数，而是应该**返回**一个包含该页数据的 `QList<Vector::RowData>` 对象。
3. **实现模型加载逻辑**: 回到 `VectorTableModel`，创建一个公共函数 `loadPage(int tableId, int page)`。在这个函数内部，它应该：
    a. 调用 `VectorDataHandler` 的相关函数获取总行数和列定义。
    b. 调用我们上一步在 `VectorDataHandler` 中创建的 `getPageData(...)` 函数来获取当前页的行数据。
    c. 将获取到的数据和元信息存储到 `VectorTableModel` 的成员变量中。
    d. **必须调用** `beginResetModel()` 和 `endResetModel()`。这会通知所有连接的视图（我们的`QTableView`）数据已完全重置，需要彻底刷新。
4. **连接数据到视图**: 现在，回去修改 `VectorTableModel` 的 `rowCount()`, `columnCount()`, `data()` 等函数的实现，让它们从 `m_pageData` 和 `m_visibleColumns` 成员变量中返回真实的数据。

### 步骤1.5：触发数据加载并提供预览切换

**任务**: 将UI的表格选择事件与模型的加载功能连接起来，并提供一个方法让我们能看到新表格的预览。

**AI请注意**:

1. 找到 `MainWindow::onVectorTableSelectionChanged(int index)` 函数。在这个函数现有逻辑的**末尾**，添加一行代码，调用 `m_vectorTableModel->loadPage(tableId, 0);` 来加载新选定表格的首页数据。
2. 为了方便我们预览重构的成果，请在 `MainWindow` 的UI中（例如，在一个`QToolBar`里）添加一个临时的 `QAction` 或 `QPushButton`，标签为"切换新旧视图"。它的作用是切换 `QStackedWidget` 的 `currentIndex()`，从而让我们可以在旧的`QTableWidget`和新的`QTableView`之间来回切换查看。

---

### **第一阶段验证**

此时，在执行完以上所有步骤后，项目应该能成功编译并运行。

**请进行以下操作并确认结果：**

1. 选择一个向量表。
2. 点击新增的"切换新旧视图"按钮。
3. **预期结果**: 新的 `QTableView` 应该被显示出来，并且其中正确地填充了所选向量表第一页的数据。数据此时是只读的，样式也可能是默认的，但数据显示的正确性是本阶段成功的关键标志。

**AI请在继续下一步前，使用你的"代码审查者"角色，对以上所有修改进行一次全面的自我审查，并向我报告结果。**

---

## 第二阶段：迁移核心功能

*此阶段的目标：在数据显示正确的基础上，让新表格变得可交互，逐步实现编辑、删除、分页等核心用户功能。*

### **重要概念：为什么我们仍然需要分页？**

**AI请注意**：这是一个关键的架构决策点。

- **在旧架构中**，我们使用分页是为了避免`QTableWidget`因创建过多`QTableWidgetItem`而崩溃。分页是为了拯救**视图(View)**。
- **在新架构中**，`QTableView`本身是虚拟的，它不关心数据总量。但是，我们必须避免让**模型(Model)** 一次性加载所有数据到内存中，否则会造成巨大的内存消耗。

因此，我们的目标是创建一个"**分页的模型**"（Paginated Model）。`VectorTableModel`将永远只在内存中保留一页数据。UI上的分页控件将不再直接操作视图，而是告诉**模型**去切换它内部的数据页。

所以，接下来的"迁移分页功能"步骤是完全正确且至关重要的。我们不是在重复造轮子，而是在将分页逻辑迁移到它在Model/View架构中真正应该在的位置。

### 步骤2.1：迁移分页功能

**任务**: 将主界面的"上一页"、"下一页"、"跳转"等分页控件，与新的 `VectorTableModel` 连接起来。

**AI请注意**:

1. 在 `VectorTableModel` 类中，添加新的公共成员函数，如 `nextPage()`, `previousPage()` 和 `jumpToPage(int pageNum)`。
2. 这些新函数的逻辑很简单：它们只负责修改模型内部的 `m_currentPage` 成员变量，然后调用模型自己的 `loadPage()` 函数来加载新页的数据并刷新视图。
3. 找到 `MainWindow` 中处理分页逻辑的槽函数（例如 `loadNextPage`, `loadPrevPage`, `jumpToPage`）。
4. 修改这些槽函数，让它们不再执行旧的逻辑，而是直接调用 `m_vectorTableModel` 上对应的新分页函数。

### 步骤2.2：实现数据编辑

**任务**: 让用户可以直接在新的 `QTableView` 中编辑单元格内容，并将修改状态同步到底层数据。

**AI请注意**:

1. 在 `VectorTableModel` 中，重写 `flags(const QModelIndex &index) const` 虚函数。根据列的类型和 `index`，决定该单元格是否可编辑。如果可编辑，就返回 `Qt::ItemIsEditable` 与默认标志的组合。
2. 在 `VectorTableModel` 中，重写 `setData(const QModelIndex &index, const QVariant &value, int role)` 虚函数。这是编辑功能的核心。
3. 在 `setData` 的实现中：
    a. 首先检查 `role` 是否为 `Qt::EditRole`。
    b. 更新模型内部的数据容器 `m_pageData` 中的值。
    c. **重要**：调用 `VectorDataHandler` 的一个方法（如果不存在，你可能需要添加一个新的），将这一行标记为"已修改"。这是为了确保后续的"保存"操作能够识别出哪些数据变脏了。
    d. 最后，发射 `dataChanged(index, index, {role})` 信号，这是通知视图和其他组件"这个单元格的数据已经变了"的标准方式。

### 步骤2.3：通过Delegate实现自定义显示

**任务**: 许多单元格（如"Y/N", "0/1/X"）可能有特殊的显示要求（如居中、不同颜色）。使用 `QStyledItemDelegate` 来复现这些自定义样式。

**AI请注意**:

1. 创建新文件 `app/vectortabledelegate.h` 和 `app/vectortabledelegate.cpp`。定义一个 `VectorTableDelegate` 类，继承自 `QStyledItemDelegate`。记得将其 `.cpp` 文件添加到 `CMakeLists.txt`。
2. 在 `VectorTableDelegate` 中，重写 `paint(...)` 方法。在 `paint` 方法内部，你可以检查 `index.data()` 的值，并据此修改 `QStyleOptionViewItem` 对象（例如，`option.displayAlignment = Qt::AlignCenter;`），然后调用基类的 `paint` 方法来完成绘制。这样既可以自定义样式，又可以复用大部分默认绘制逻辑。
3. 在 `MainWindow` 的UI初始化代码中，创建 `VectorTableDelegate` 的一个实例，并使用 `m_vectorTableView->setItemDelegate(...)` 将其设置给新的视图。

### 步骤2.4：迁移"删除行"功能

**任务**: 将"删除选中行"的功能适配到新的 `QTableView` 上。

**AI请注意**:

1. 在 `VectorTableModel` 中，重写 `removeRows(int row, int count, const QModelIndex &parent)` 虚函数。
2. `removeRows` 的实现逻辑应该是：
    a. 调用 `beginRemoveRows(...)`。
    b. 循环 `count` 次，调用 `VectorDataHandler` 中的方法，从后端存储中删除对应的行。
    c. 如果后端删除成功，则调用 `endRemoveRows()`。
3. 找到 `MainWindow` 中负责删除行的槽函数（如 `deleteSelectedVectorRows`）。
4. 修改这个槽函数，让它从 `m_vectorTableView->selectionModel()` 获取选中的行，然后调用 `m_vectorTableModel->removeRows(...)` 来执行删除操作。

---

### **第二阶段验证**

**请进行以下操作并确认结果：**

1. 分页按钮现在应该能够正确地控制新 `QTableView` 的数据显示。
2. 双击可编辑的单元格，应该可以输入新值。
3. 单元格的特殊样式（如居中对齐）应该和旧表格一样。
4. 在新表格中选中若干行并点击删除，这些行应该会消失。

**AI请在继续下一步前，使用你的"代码审查者"角色，对以上所有修改进行一次全面的自我审查，并向我报告结果。**

---

## 第三阶段：迁移剩余动作

*此阶段的目标：将依赖于表格交互的次要功能，如右键菜单、状态栏信息更新等，全部迁移到新架构上。*

### 步骤3.1：迁移右键菜单

**任务**: 让右键点击 `QTableView` 时，能弹出与之前功能相同的上下文菜单。

**AI请注意**:

1. 找到 `MainWindow` 中为旧表格设置上下文菜单的代码（通常是连接 `customContextMenuRequested` 信号）。
2. 将 `m_vectorTableView` 的 `customContextMenuRequested` 信号也连接到同一个槽函数。
3. 修改该槽函数：它需要使用 `m_vectorTableView->indexAt(pos)` 来获取被点击处的 `QModelIndex`。后续的逻辑需要从依赖 `QTableWidgetItem` 改为依赖 `QModelIndex` 来判断应该显示哪些菜单项。

### 步骤3.2：迁移基于选择的交互

**任务**: 适配那些需要根据当前选中项来更新的UI组件（例如，一个显示选中行16进制值的状态栏）。

**AI请注意**:

1. 找到 `MainWindow` 中响应旧表格选择变化的逻辑（通常是连接 `itemSelectionChanged` 信号）。
2. 现在，连接 `m_vectorTableView->selectionModel()` 的 `selectionChanged` 信号到相应的槽函数。
3. 修改槽函数内部的逻辑。它现在需要通过 `selectionModel()` 来获取一个 `QModelIndexList`，然后通过 `m_vectorTableModel->data(index)` 来获取每个选中单元格的数据。

---

## 第四阶段：清理与收尾

*此阶段的目标：在所有功能都已成功迁移并验证后，彻底移除旧的`QTableWidget`，完成整个重构。*

### 步骤4.1：移除旧的QTableWidget

**任务**: 从项目中完全删除 `QTableWidget` 及其相关的所有代码。

**AI请注意**:

1. 在 `MainWindow` 的UI初始化代码中，找到我们之前创建的 `QStackedWidget`。将它从中布局中移除，然后将 `m_vectorTableView` 直接添加到布局中。
2. 在 `mainwindow.h` 中，删除 `m_vectorTableWidget` 成员变量的声明。
3. 在 `MainWindow` 中，删除临时的"切换新旧视图"按钮及其相关槽函数。
4. 在整个项目中搜索 `m_vectorTableWidget`，确保所有对它的引用都已被删除或重构。
5. 在 `VectorDataHandler` 中，现在可以安全地删除旧的、接受 `QTableWidget*` 作为参数的 `loadVectorTablePageData` 函数了。

### 步骤4.2：最终代码审查

**任务**: 对所有与本次重构相关的修改进行最后一次代码审查，确保代码的整洁和最终质量。

**AI请注意**:
请对所有你修改过的文件进行一次最终检查。移除所有用于调试的临时代码、注释或 `qDebug` 输出。确保新添加的代码遵循项目现有的编码风格和规范。

**至此，重构完成！**
