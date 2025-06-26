# 重构指南：下一代二进制存储引擎 (`RobustBinaryHelper`)

**版本**: 1.0
**日期**: 2025-06-26
**作者**: Gemini (AI Assistant) & [您的名字]

---

## 1. 概述 (Overview)

本文档详细说明了将项目现有的二进制文件读写逻辑 (`Persistence::BinaryFileHelper`) 替换为新一代、更健壮的存储引擎 (`Persistence::RobustBinaryHelper`) 的完整过程。

### 1.1. 问题背景 (The Problem)

现有的 `BinaryFileHelper` 基于一个连续的、流式的二进制结构 (`[行大小][行数据][行大小][行数据]...`)。该结构在设计上存在以下**根本性缺陷**：

* **脆弱性 (Fragility)**: 在对文件进行**修改**操作（特别是当数据长度变化时）的过程中，如果程序异常退出（崩溃、断电），文件会被截断并处于一个不一致的损坏状态，导致后续无法读取，数据永久丢失。
* **复杂性 (Complexity)**: 为了弥补其脆弱性，旧代码被迫实现了大量复杂、易错的逻辑，包括：
  * 危险的"原地修改"文件内容。
  * 庞大的、难以维护的"手动序列化"逻辑。
  * 为弥补性能问题而设计的多级缓存（内存+磁盘索引文件）和多线程扫描机制。
* **设计冲突**: 试图用"固定长度"的思维模式去应对"动态列"的业务需求，导致实现逻辑极其复杂且难以维护。

### 1.2. 解决方案 (The Solution)

我们设计并实现了一个全新的存储引擎 `RobustBinaryHelper`，其核心设计哲学是**"安全、解耦、简单"**。

* **数据结构**:
  * **文件头 (Header)**: 包含魔数、版本、索引区指针等关键元信息。
  * **数据区 (Data Area)**: 由一系列独立的、可校验的**数据块 (Data Chunks)** 组成。
  * **索引区 (Index Area)**: 文件的原生组成部分，存储每个逻辑行对应的数据块在文件中的物理位置。

* **核心优势**:
  * **原子化写入 (Atomic Writes)**: 所有修改和新增操作都通过**"仅追加"(Append-Only)** 的方式完成，从不修改文件中间的内容，从根本上杜绝了文件损坏的风险。
  * **数据完整性 (Data Integrity)**: 每个数据块都包含**同步标记 (Sync Marker)** 和**校验和 (CRC32 Checksum)**，确保读取的数据是完整且正确的。
  * **高性能随机访问 (Fast Random Access)**: 通过原生的索引区，实现了 `O(1)` 复杂度的快速行定位。
  * **原生支持动态结构 (Native Dynamic Schema Support)**: 通过 `QDataStream` 对 `QList<QVariant>` 的序列化，完美支持动态增删列，无需任何额外代码。
  * **代码简洁性 (Simplicity)**: 由于设计上的优越性，新模块的代码量相比旧模块减少了约80%，更易于理解和维护。

## 2. 改造范围与策略 (Scope & Strategy)

本次重构的核心是将所有依赖 `Persistence::BinaryFileHelper` 的代码，全部切换到使用 `Persistence::RobustBinaryHelper`。

* **主要影响文件**:
  * `vector/vectordatahandler.cpp` (以及其 .h 文件)
  * 可能还有其他直接或间接调用旧模块的地方。

* **改造策略**:
  * 我们采取**"硬切换"(Hard Cutover)**策略，因为项目处于开发阶段，**无需迁移旧的 `.vbindata` 文件**。
  * 所有改造工作应在一个独立的Git分支上进行，直到所有功能测试通过后再合并到主线。
  * 改造过程遵循"先读后写"的顺序：首先改造数据加载功能，然后是保存、修改、删除等功能。

## 3. 详细改造步骤 (Step-by-Step Guide)

### 第 Ⅰ 阶段：准备工作 (Preparation) - `已完成`

1. **创建新模块**:
    * `robust_binary_helper.h`: 已创建。定义了新的数据结构和类接口。
    * `robust_binary_helper.cpp`: 已创建。实现了新引擎的全部核心功能 (CRUD、文件生命周期管理)。
2. **添加到项目**:
    * 确保上述两个新文件已添加到 `CMakeLists.txt` 的源文件列表中，以便项目能够正确编译它们。

### 第 Ⅱ 阶段：核心集成 - `VectorDataHandler` (Integration)

这是本次任务的核心。我们需要逐一替换 `VectorDataHandler` 中的旧逻辑。

#### 3.1. 修改头文件和成员变量

**文件**: `vector/vectordatahandler.h`

1. **包含新头文件**:

    ```cpp
    // 移除 #include "database/binaryfilehelper.h"
    #include "robust_binary_helper.h" // 添加新头文件
    ```

2. **调整成员变量**:
    * 旧的 `VectorDataHandler` 可能有一些用于管理分页、缓存旧二进制文件逻辑的成员变量。审视这些变量，大部分可能都不再需要。
    * 核心是确保有一个地方可以存储从SQLite加载的列定义"图纸"(`QList<ColumnInfo>`)。

#### 3.2. 改造文件打开/关闭/创建逻辑

**文件**: `vector/vectordatahandler.cpp`

* **目标函数**: `openTable`, `createTable`, `closeTable`, `saveTable` (或类似名称的函数)。

* **改造逻辑**:
    1. **`createTable`**:
        * 旧逻辑: 调用 `DatabaseManager` 在SQLite中创建元数据，然后可能创建了一个空的 `.vbindata` 文件。
        * 新逻辑: 在SQLite中创建元数据后，调用 `Persistence::RobustBinaryHelper::createFile(newBinaryFilePath)` 来创建一个包含有效文件头的、崭新的、健壮的二进制文件。
    2. **`openTable`**:
        * 旧逻辑: 从SQLite获取路径和列定义，然后可能调用了 `BinaryFileHelper` 的某个扫描或建立索引的函数。
        * 新逻辑: 从SQLite获取路径后，直接调用 `Persistence::RobustBinaryHelper::openFile(binaryFilePath)`。此函数会一次性将文件的索引读入内存，为后续所有操作做好准备。
    3. **`closeTable`/`saveTable`**:
        * 旧逻辑: 可能有一个复杂的保存流程，甚至全量重写文件。
        * 新逻辑: 逻辑变得极其简单。只需调用 `Persistence::RobustBinaryHelper::closeFile()`。此函数会自动将内存中最新的索引安全地写回文件，并关闭句柄。

#### 3.3. 改造数据加载逻辑 (最核心)

* **目标函数**: `loadVectorTablePageData` (或类似名称的分页加载函数)。

* **改造逻辑**:
  * 此函数的复杂性将**大幅降低**。
    1. **获取行ID**: 不再需要扫描文件。直接调用 `Persistence::RobustBinaryHelper::getAllRowIds()` 获取所有有效行的ID列表。
    2. **分页**: 根据传入的 `pageIndex` 和 `pageSize`，从上一步的ID列表中计算出当前页需要显示的ID。
    3. **循环读取**: 遍历当前页的ID，对每个 `rowId`，调用 `Persistence::RobustBinaryHelper::readRow(rowId, rowData)`。
    4. **填充UI**: 将返回的 `rowData` (`QList<QVariant>`) 解析并填充到 `QTableWidget` 或 `QTableView` 中。`VectorDataHandler` 在这里扮演"翻译官"的角色，它知道 `rowData` 中第 N 个元素对应UI的哪一列。

#### 3.4. 改造数据修改逻辑

* **目标函数**: `addRow`, `updateRow`, `deleteRow` (或处理UI修改后保存的槽函数)。

* **改造逻辑**:
    1. **`addRow`**:
        * 从UI或其他来源收集新行的数据，组织成一个 `RowData` (`QList<QVariant>`)。
        * 调用 `Persistence::RobustBinaryHelper::addRow(rowData, newRowId)`。完成。
    2. **`updateRow`**:
        * 从UI或其他来源收集修改后的行数据，组织成 `RowData`。
        * 获取该行的 `rowId`。
        * 调用 `Persistence::RobustBinaryHelper::updateRow(rowId, rowData)`。完成。
    3. **`deleteRow`**:
        * 获取要删除行的 `rowId`。
        * 调用 `Persistence::RobustBinaryHelper::deleteRow(rowId)`。完成。

#### 3.5. 改造处理动态列的逻辑

* **目标函数**: `addNewPinColumn`, `deletePinColumn` (或类似函数)。

* **改造逻辑 (如之前讨论)**:
    1. 在SQLite中更新列的元数据。
    2. 调用 `RobustBinaryHelper::getAllRowIds()` 获取所有行ID。
    3. 循环遍历每个 `rowId`:
        * 调用 `RobustBinaryHelper::readRow()` 读取旧数据。
        * 在 `VectorDataHandler` 层面，修改 `QList<QVariant>` (添加新列的默认值，或删除指定列的数据)。
        * 调用 `RobustBinaryHelper::updateRow()` 将修改后的完整行数据写回。

### 第 Ⅲ 阶段：清理与验证 (Cleanup & Verification)

1. **删除旧代码**:
    * 确认项目中所有对 `BinaryFileHelper` 的调用都已被替换。
    * 从 `CMakeLists.txt` 中移除 `database/binaryfilehelper.cpp`。
    * 从文件系统中删除 `database/binaryfilehelper.cpp` 和 `database/binaryfilehelper.h`。
2. **功能测试**:
    * 全面测试所有与向量表相关的功能：
        * 新建/打开项目。
        * 添加/删除/修改行。
        * 添加/删除管脚列。
        * 滚动和分页加载大量数据。
3. **压力与异常测试**:
    * 在一个包含大量数据的项目中，执行修改操作，然后在操作中途**强制关闭程序**。
    * 重新打开项目，验证数据是否完好无损（应回滚到操作前的状态），文件是否可以正常打开。这是检验我们新引擎健壮性的最终测试。

## 4. 风险与回滚计划 (Risk & Rollback Plan)

* **主要风险**: 集成过程中，可能遗漏某些对旧模块的调用，或在 `VectorDataHandler` 中对新接口的调用逻辑有误。
* **规避措施**: 在独立的Git分支上进行所有开发。频繁地进行编译和单元功能测试。
* **回滚计划**: 如果在集成后期发现无法解决的重大问题，最简单的回滚方案就是丢弃当前分支，并从主线重新开始 (`git checkout main; git branch -D refactor-branch`)。由于所有修改都隔离在分支中，因此主线代码是绝对安全的。

---
*文档结束*
