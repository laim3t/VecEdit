# 数据库模块 (持久化层) 更新 - 版本 2

## 特性概述

本次更新 (数据库版本 2) 引入了对数据库 Schema 的重要修改，以支持一种新的混合存储方案，旨在优化大量向量表数据的存储效率和性能。此方案遵循了《项目存储优化：SQLite 与二进制文件混合方案实施指南》中定义的设计。

核心变化包括：

1. **新表添加**:
    * `VectorTableMasterRecord`: 用于存储每个"向量表"的元数据，包括其名称、关联的原始表ID（用于迁移）、以及指向外部二进制数据文件的引用、文件和数据 Schema 版本信息、行列数统计和时间戳。
    * `VectorTableColumnConfiguration`: 用于详细定义每个二进制数据文件内部的列结构，包括列名、顺序、数据类型（如 `TEXT`, `INSTRUCTION_ID`, `TIMESET_ID`, `PIN_STATE_ID`）以及用于管脚列的额外属性（以JSON格式存储，如 `pin_list_id`, `channel_count`, `type_option_id`）。

2. **存储逻辑变更 (针对 `VectorDataHandler` 处理的向量表)**:
    * 原有的 `vector_table_data` 和 `vector_table_pin_values` 表将不再用于存储新的或经过迁移的向量表的大量行数据。它们在 Schema (`schema.sql`) 中保留，主要目的是支持对旧项目数据的读取和迁移过程。
    * 新的向量表数据将被序列化并存储在与主 `.db` 文件分离的独立二进制文件中 (通常在 `[数据库文件名]_vbindata/` 子目录下，文件名如 `table_[original_table_id]_data.vbindata`)。

## 数据库版本管理

* 数据库版本通过 `DatabaseManager` 类进行管理，并记录在 SQLite 数据库内部的 `db_version` 表中（包含 `version` 和 `updated_at` 字段）。
* **当前最新数据库 Schema 版本**: 2。此版本由 `DatabaseManager::LATEST_DB_SCHEMA_VERSION` 常量定义。
* **升级机制**:
  * 当应用程序打开一个旧版本（例如版本1）的数据库文件时，`DatabaseManager::openExistingDatabase()` 会检测到版本差异。
  * 它会调用 `DatabaseManager::performSchemaUpgradeToV2()` 方法（对于从v1到v2的升级）。
  * 该升级方法首先执行 `resources/db/updates/update_to_v2.sql` 脚本来创建新表结构。
  * 随后，它在C++中执行元数据迁移和完整数据迁移：
    * 遍历旧的 `vector_tables`。
    * 为每个旧表在 `VectorTableMasterRecord` 中创建对应条目。
    * 根据旧表的固定列和 `vector_table_pins` 定义，为每个主记录填充 `VectorTableColumnConfiguration`。
    * 调用 `migrateVectorDataToBinaryFiles()` 创建二进制数据文件并将旧表中的所有行数据迁移到这些文件中。
  * 成功完成所有步骤后，`db_version` 表中的版本号将更新为2。
* **新数据库**:
  * 新创建的数据库将通过执行 `resources/db/schema.sql` 直接建立版本2的结构。
  * `schema.sql` 文件已包含 `VectorTableMasterRecord` 和 `VectorTableColumnConfiguration` 的定义。

## 对现有功能的影响

* 此 Schema 变更为 `VectorDataHandler` 的重构奠定了基础，使其能够采用新的二进制文件存储方式替换对 `vector_table_data` 和 `vector_table_pin_values` 表的直接依赖。
* 上层业务逻辑和UI层对此 Schema 变更保持透明。数据加载和保存接口的签名保持不变，只有内部实现发生变化。

## 向后兼容性与数据迁移

* **Schema 向后兼容性**: 新版本的应用程序能够打开旧版本（版本1）的数据库文件，并自动将其 Schema 和数据升级到版本2。
* **数据迁移**:
  * `DatabaseManager::performSchemaUpgradeToV2()` 完成了元数据迁移，包括创建 `VectorTableMasterRecord` 条目、填充 `VectorTableColumnConfiguration`。
  * `DatabaseManager::migrateVectorDataToBinaryFiles()` 实现了行数据迁移，将旧表中的数据读取、序列化并写入新的二进制文件。
  * 数据迁移过程在事务中执行，确保原子性，任何步骤失败都会回滚整个迁移。
* **版本兼容性处理**:
  * `VectorDataHandler` 和 `BinaryFileHelper` 添加了版本兼容性检查和处理逻辑：
    * 当文件版本高于应用程序支持的版本时，会拒绝加载文件（防止损坏数据）。
    * 当文件版本低于应用程序支持的版本时，会尝试使用向后兼容逻辑加载数据。
    * `deserializeRow` 方法根据传入的 `fileVersion` 参数采用不同的反序列化策略。

## 性能与存储优化

* **文件大小减少**: 二进制存储比SQLite表存储更紧凑，预计文件大小减少约30-50%。
* **加载性能**: 大型向量表的加载性能显著提升，尤其是对于包含数千行的表。
* **保存性能**: 保存操作更高效，尤其是增量更新时。

## 使用说明 (针对开发者)

* 当需要进一步修改数据库 Schema 时（例如添加/修改表、视图、索引）：
    1. **更新 `resources/db/schema.sql`**: 此文件应始终反映最新的、完整的数据库结构。
    2. **创建升级脚本**: 如果更改需要应用于现有的较旧版本的数据库，则创建一个新的升级SQL脚本 (例如 `update_to_v(N+1).sql`) 并将其放置在 `resources/db/updates/` 目录下 (建议也添加到项目的qrc资源文件中)。脚本应使用 `IF NOT EXISTS` 等幂等操作。
    3. **更新 `DatabaseManager.h`**: 增加 `LATEST_DB_SCHEMA_VERSION` 的值。
    4. **实现升级函数**: 在 `DatabaseManager.cpp` 中添加一个新的私有方法，例如 `performSchemaUpgradeToV(N+1)()`。此方法应：
        * 执行新的SQL升级脚本。
        * 执行任何必要的C++数据迁移或转换逻辑。
        * 成功后调用 `setCurrentDbFileVersionInTable(N+1)`。
    5. **更新调用链**: 在 `DatabaseManager::openExistingDatabase()` 的升级逻辑部分，添加对新的 `performSchemaUpgradeToV(N+1)()` 方法的调用，确保按版本顺序执行升级。
    6. **更新此 `README.md`**: 记录变更内容、新的数据库版本号以及升级步骤。

* 在扩展二进制文件格式时：
    1. 更新 `BinaryFileHeader` 结构以包含新的元数据字段。
    2. 增加 `file_format_version` 的值。
    3. 在 `BinaryFileHelper` 中添加相应版本的序列化/反序列化处理逻辑。

---

# 完整实施步骤概述

混合存储方案的实施已经完成以下所有步骤：

1. **数据库Schema调整**:
   * 创建新的 `VectorTableMasterRecord` 和 `VectorTableColumnConfiguration` 表
   * 添加创建这些表的SQL脚本到 `schema.sql` 和 `update_to_v2.sql`

2. **二进制文件格式设计**:
   * 定义 `BinaryFileHeader` 结构
   * 实现二进制文件的文件命名和路径管理策略

3. **持久化层核心逻辑重构**:
   * 重构 `VectorDataHandler` 类，使其使用二进制文件存储而非SQLite表
   * 实现二进制数据序列化与反序列化

4. **数据迁移实现**:
   * 添加 `migrateVectorDataToBinaryFiles()` 方法迁移现有数据
   * 实现从旧格式到新格式的自动转换

5. **版本兼容性处理**:
   * 添加文件版本检查机制
   * 实现不同版本数据格式的兼容性逻辑

这一系列改进极大地优化了项目的存储效率和性能，同时保持了与现有代码的完全兼容性。

---

# 二进制文件辅助模块 (BinaryFileHelper)

## 功能概述

`database/binaryfilehelper.h` 和 `database/binaryfilehelper.cpp` 文件定义并实现了 `Persistence::BinaryFileHelper` 类。这个类提供了一系列静态辅助函数，用于处理项目自定义二进制数据文件（例如 `.vbindata`）的底层读写操作。它主要负责：

1. **文件头处理**：读取和写入在 `common/binary_file_format.h` 中定义的 `BinaryFileHeader` 结构。
2. **数据序列化/反序列化**：提供对各种数据类型的序列化和反序列化功能，支持将内存中的行数据对象与二进制字节流进行相互转换。

该模块是持久化层（例如 `VectorDataHandler`）在采用混合存储方案（SQLite + 二进制文件）时的关键内部组件。

## 使用说明

`BinaryFileHelper` 的方法是静态的，可以直接通过类名调用。

### 读取文件头

```cpp
#include "database/binaryfilehelper.h"
#include "common/binary_file_format.h"
#include <QFile>

// ...
QFile binFile("path/to/your/file.vbindata");
if (binFile.open(QIODevice::ReadOnly)) {
    BinaryFileHeader header;
    if (Persistence::BinaryFileHelper::readBinaryHeader(&binFile, header)) {
        // Header read successfully, header.logDetails("ReadFileHeader");
    } else {
        // Failed to read or validate header
    }
    binFile.close();
}
```

### 写入文件头

```cpp
#include "database/binaryfilehelper.h"
#include "common/binary_file_format.h"
#include <QFile>
#include <QDateTime>

// ...
QFile binFile("path/to/your/new_file.vbindata");
if (binFile.open(QIODevice::WriteOnly)) {
    BinaryFileHeader header;
    header.row_count_in_file = 100;  // 例如
    header.column_count_in_file = 5; // 例如
    header.timestamp_created = QDateTime::currentSecsSinceEpoch();
    header.timestamp_updated = header.timestamp_created;
    
    if (Persistence::BinaryFileHelper::writeBinaryHeader(&binFile, header)) {
        // Header written successfully
    } else {
        // Failed to write header
    }
    
    // Now continue to write row data...
    binFile.close();
}
```

### 序列化和反序列化行数据

```cpp
#include "database/binaryfilehelper.h"
#include "vector/vector_data_types.h"
#include <QByteArray>

// 序列化一行数据
Vector::RowData rowData; // 假设此处已填充数据
QList<Vector::ColumnInfo> columns; // 假设此处已定义列结构
QByteArray serializedData;

if (Persistence::BinaryFileHelper::serializeRow(rowData, columns, serializedData)) {
    // 序列化成功，可以写入文件
    // file.write(serializedData);
}

// 反序列化一行数据
QByteArray rawBytes; // 假设此处已从文件读取
Vector::RowData resultRow;
int fileVersion = 1; // 文件的schema版本

if (Persistence::BinaryFileHelper::deserializeRow(rawBytes, columns, fileVersion, resultRow)) {
    // 反序列化成功，可以处理resultRow
}
```

## 输入/输出规范

* **`readBinaryHeader`**:
  * 输入: 打开的、可读的 `QIODevice*`，指向二进制文件的起始位置（或头部位置）。
  * 输出: 填充的 `BinaryFileHeader` 对象；函数返回 `true` 表示成功，`false` 表示失败。
* **`writeBinaryHeader`**:
  * 输入: 打开的、可写的 `QIODevice*`；有效的 `BinaryFileHeader` 对象。
  * 输出: 函数返回 `true` 表示成功，`false` 表示失败。
* **`serializeRow`**:
  * 输入: 一个 `Vector::RowData` (数据行)；一个 `QList<Vector::ColumnInfo>` (列定义)。
  * 输出: 一个填充的 `QByteArray`；函数返回 `true` 表示成功，`false` 表示失败。
* **`deserializeRow`**:
  * 输入: 一个 `QByteArray`(二进制数据)；一个 `QList<Vector::ColumnInfo>` (列定义)；一个 `int` 文件版本。
  * 输出: 一个填充的 `Vector::RowData`；函数返回 `true` 表示成功，`false` 表示失败。

## 向后兼容性

文件头的 `file_format_version` 和 `data_schema_version` 字段由 `BinaryFileHelper` 读写，这些版本信息将由调用方（如 `VectorDataHandler`）用于处理不同版本文件格式的兼容性问题。现在已实现：

* 当文件版本高于应用程序支持的版本时，会拒绝加载文件，防止数据损坏。
* 当文件版本低于应用程序支持的版本时，会采用向后兼容策略，尝试适配旧数据。

## 版本和变更历史

* **版本**: 1.0
* **变更历史**:
  * 1.0 (2023-06-30): 初始创建 `BinaryFileHelper` 类。实现 `readBinaryHeader`、`writeBinaryHeader`、`serializeRow` 和 `deserializeRow` 方法，支持版本兼容性检查和不同格式数据的序列化/反序列化。
