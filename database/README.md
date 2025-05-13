# Database Module README

## Feature Overview

This module handles all database interactions for the VecEdit application, primarily managing vector metadata and collection information stored in an SQLite database. It provides Data Access Objects (DAOs) for CRUD (Create, Read, Update, Delete) operations on vectors and vector collections. Vector binary data is stored separately in the file system, with the database holding references (filenames).

## Usage Instructions

Include the necessary DAO headers (e.g., `vectordao.h`, `vectorcollectiondao.h`) and use the provided methods to interact with the database. Ensure `DatabaseManager::initialize()` is called once at application startup.

**Example (Adding a Vector):**

```cpp
#include "database/vectordao.h"
#include "database/vectorcollectiondao.h"
#include "vector/vector.h"
#include "vector/vectorcollection.h"
#include <QByteArray>

// Assuming collection exists or is created
VectorCollection collection = VectorCollectionDAO::getCollectionByName("MyCollection");
if (collection.id <= 0) {
    collection = VectorCollectionDAO::addCollection("MyCollection", "Description");
}

VectorDAO vectorDAO;
QByteArray data = ...; // Load or generate binary data
QString metadata = "{ \"key\": \"value\" }";
Vector newVector = vectorDAO.addVector("MyVector", metadata, data, &collection);

if (newVector.id > 0) {
    // Vector added successfully (DB record created and file saved)
} else {
    // Failed to add vector (rollback attempted if file save failed)
}
```

## Input/Output Specification

- **Input:** Function parameters as defined in DAO headers (e.g., names, metadata strings, `QByteArray` for binary data, object references).
- **Output:** DAO methods typically return `Vector` or `VectorCollection` objects, lists (`QList`) of these objects, or boolean success/failure indicators. Vector objects returned by list methods (`getAllVectors`, `getAllVectorsByCollectionId`) do *not* contain binary data; use `getVectorById` to load binary data for a specific vector.
- **Database:** Interacts with the `VecEdit.db` SQLite database file located in the application's data directory (`QStandardPaths::AppDataLocation`).
- **Files:** Reads/writes `.bin` files containing vector binary data in the application's data directory.

## Backward Compatibility

- Database schema changes require migration logic (handled by `DatabaseManager`).
- Binary file format is currently raw `QByteArray` data. Versioning is not yet implemented within the binary files themselves but should be considered for future enhancements.

## Versioning and Change History

- **Version:** 1.1
- **Changes:**
  - **v1.1:**
    - Implemented rollback logic in `VectorDAO::addVector`. If saving the binary file fails, the corresponding database record is now deleted to prevent orphaned records. Added checks and rollback for directory creation failure.
    - Added detailed `qDebug` logs to all DAO methods for enhanced traceability.
    - Refined path handling using `QStandardPaths` and `QDir::separator()`.
    - Stored only filenames (not full paths) in the database `vectors` table. Full paths are constructed on demand.
    - Added handling for potential failures during binary file loading in `getVectorById`.
    - Improved deletion logic in `deleteVector` to retrieve the filename before deleting the DB record and attempting file deletion afterward.
  - **v1.0:** Initial implementation of `VectorDAO` and `VectorCollectionDAO` with basic CRUD operations.

---

# 数据库模块 (持久化层) 更新 - 版本 2

## 特性概述

本次更新 (数据库版本 2) 引入了对数据库 Schema 的重要修改，以支持一种新的混合存储方案，旨在优化大量向量表数据的存储效率和性能。此方案遵循了《项目存储优化：SQLite 与二进制文件混合方案实施指南》中定义的设计。

核心变化包括：

1. **新表添加**:
    - `VectorTableMasterRecord`: 用于存储每个"向量表"的元数据，包括其名称、关联的原始表ID（用于迁移）、以及指向外部二进制数据文件的引用、文件和数据 Schema 版本信息、行列数统计和时间戳。
    - `VectorTableColumnConfiguration`: 用于详细定义每个二进制数据文件内部的列结构，包括列名、顺序、数据类型（如 `TEXT`, `INSTRUCTION_ID`, `TIMESET_ID`, `PIN_STATE_ID`）以及用于管脚列的额外属性（以JSON格式存储，如 `pin_list_id`, `channel_count`, `type_option_id`）。

2. **存储逻辑变更 (针对 `VectorDataHandler` 处理的向量表)**:
    - 原有的 `vector_table_data` 和 `vector_table_pin_values` 表将不再用于存储新的或经过迁移的向量表的大量行数据。它们在 Schema (`schema.sql`) 中保留，主要目的是支持对旧项目数据的读取和迁移过程。
    - 新的向量表数据将被序列化并存储在与主 `.db` 文件分离的独立二进制文件中 (通常在 `[数据库文件名]_vbindata/` 子目录下，文件名如 `table_[original_table_id]_data.vbindata`)。

## 数据库版本管理

- 数据库版本通过 `DatabaseManager` 类进行管理，并记录在 SQLite 数据库内部的 `db_version` 表中（包含 `version` 和 `updated_at` 字段）。
- **当前最新数据库 Schema 版本**: 2。此版本由 `DatabaseManager::LATEST_DB_SCHEMA_VERSION` 常量定义。
- **升级机制**:
  - 当应用程序打开一个旧版本（例如版本1）的数据库文件时，`DatabaseManager::openExistingDatabase()` 会检测到版本差异。
  - 它会调用 `DatabaseManager::performSchemaUpgradeToV2()` 方法（对于从v1到v2的升级）。
  - 该升级方法首先执行 `resources/db/updates/update_to_v2.sql` 脚本来创建新表结构。
  - 随后，它在C++中执行元数据迁移：
    - 遍历旧的 `vector_tables`。
    - 为每个旧表在 `VectorTableMasterRecord` 中创建对应条目。
    - 根据旧表的固定列和 `vector_table_pins` 定义，为每个主记录填充 `VectorTableColumnConfiguration`。
    - 创建空的二进制数据文件。
  - 成功完成所有步骤后，`db_version` 表中的版本号将更新为2。
- **新数据库**:
  - 新创建的数据库将通过执行 `resources/db/schema.sql` 直接建立版本2的结构。
  - `schema.sql` 文件已包含 `VectorTableMasterRecord` 和 `VectorTableColumnConfiguration` 的定义。

## 对现有功能的影响

- 此 Schema 变更为 `VectorDataHandler` 的重构奠定了基础，使其能够采用新的二进制文件存储方式替换对 `vector_table_data` 和 `vector_table_pin_values` 表的直接依赖。
- 上层业务逻辑和UI层对此 Schema 变更应保持透明。数据加载和保存接口的签名计划保持不变，但其内部实现将被重构。

## 向后兼容性与数据迁移

- **Schema 向后兼容性**: 新版本的应用程序将能够打开旧版本（版本1）的数据库文件，并自动将其 Schema 升级到版本2。
- **数据迁移**:
  - `DatabaseManager::performSchemaUpgradeToV2()` **已完成元数据迁移**，包括创建 `VectorTableMasterRecord` 条目、填充 `VectorTableColumnConfiguration` 以及创建空的二进制文件。
  - **实际行数据的迁移** (从旧的 `vector_table_data` 和 `vector_table_pin_values` 表读取数据，序列化并写入新的二进制文件，同时更新 `VectorTableMasterRecord` 中的 `row_count`) **是后续步骤**，将在 `VectorDataHandler` 的重构中实现。
  - 当前 Schema 更改和元数据迁移是完整数据迁移的前提。

## 使用说明 (针对开发者)

- 当需要进一步修改数据库 Schema 时（例如添加/修改表、视图、索引）：
    1. **更新 `resources/db/schema.sql`**: 此文件应始终反映最新的、完整的数据库结构。
    2. **创建升级脚本**: 如果更改需要应用于现有的较旧版本的数据库，则创建一个新的升级SQL脚本 (例如 `update_to_v(N+1).sql`) 并将其放置在 `resources/db/updates/` 目录下 (建议也添加到项目的qrc资源文件中)。脚本应使用 `IF NOT EXISTS` 等幂等操作。
    3. **更新 `DatabaseManager.h`**: 增加 `LATEST_DB_SCHEMA_VERSION` 的值。
    4. **实现升级函数**: 在 `DatabaseManager.cpp` 中添加一个新的私有方法，例如 `performSchemaUpgradeToV(N+1)()`。此方法应：
        - 执行新的SQL升级脚本。
        - 执行任何必要的C++数据迁移或转换逻辑。
        - 成功后调用 `setCurrentDbFileVersionInTable(N+1)`。
    5. **更新调用链**: 在 `DatabaseManager::openExistingDatabase()` 的升级逻辑部分，添加对新的 `performSchemaUpgradeToV(N+1)()` 方法的调用，确保按版本顺序执行升级。
    6. **更新此 `README.md`**: 记录变更内容、新的数据库版本号以及升级步骤。

---

# 二进制文件辅助模块 (BinaryFileHelper)

## 功能概述

`database/binaryfilehelper.h` 和 `database/binaryfilehelper.cpp` 文件定义并实现了 `Persistence::BinaryFileHelper` 类。这个类提供了一系列静态辅助函数，用于处理项目自定义二进制数据文件（例如 `.vbindata`）的底层读写操作。它主要负责：

1. **文件头处理**：读取和写入在 `common/binary_file_format.h` 中定义的 `BinaryFileHeader` 结构。
2. **数据序列化/反序列化**：提供（目前为占位符的）接口，用于将内存中的行数据对象与二进制字节流进行相互转换。

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
    BinaryFileHeader newHeader;
    newHeader.magic_number = VBIN_MAGIC_NUMBER;
    newHeader.file_format_version = 1;
    newHeader.data_schema_version = 1; // Match DB schema version for this data
    newHeader.row_count_in_file = 0;   // Initially empty
    newHeader.column_count_in_file = 5; // Example column count
    newHeader.timestamp_created = static_cast<uint64_t>(QDateTime::currentSecsSinceEpoch());
    newHeader.timestamp_updated = newHeader.timestamp_created;

    if (Persistence::BinaryFileHelper::writeBinaryHeader(&binFile, newHeader)) {
        // Header written successfully
    } else {
        // Failed to write header
    }
    binFile.close();
}
```

### 行数据序列化/反序列化 (接口占位符)

`BinaryFileHelper` 类中定义了 `serializeRow` 和 `deserializeRow` 的静态方法签名，但其实现目前是占位符。这些方法将在确定了项目中代表单行数据 (`MemoryRowObject`) 和列定义 (`ColumnSchema`) 的具体C++类型后完成。

```cpp
// Placeholder - actual usage will depend on MemoryRowObject and ColumnSchema definitions
// static bool serializeRow(const MemoryRowObject& row, const ColumnSchema& schema, QByteArray& serializedRow);
// static bool deserializeRow(const QByteArray& bytes, const ColumnSchema& schema, int fileVersion, MemoryRowObject& row);
```

## 主要方法

- `static bool readBinaryHeader(QIODevice* device, BinaryFileHeader& header);`
  - 从 `device` 读取并验证 `BinaryFileHeader`。
  - 使用 `QDataStream` 并指定字节序 (LittleEndian) 以确保一致性。
  - 包含详细的错误检查和日志记录。
- `static bool writeBinaryHeader(QIODevice* device, const BinaryFileHeader& header);`
  - 将 `header` 写入 `device`。
  - 使用 `QDataStream` 并指定字节序 (LittleEndian)。
  - 包含详细的错误检查和日志记录。

## 输入/输出规范

- **`readBinaryHeader`**:
  - 输入: 打开的、可读的 `QIODevice*`，指向二进制文件的起始位置（或头部位置）。
  - 输出: 填充的 `BinaryFileHeader` 对象；函数返回 `true` 表示成功，`false` 表示失败。
- **`writeBinaryHeader`**:
  - 输入: 打开的、可写的 `QIODevice*`；有效的 `BinaryFileHeader` 对象。
  - 输出: 函数返回 `true` 表示成功，`false` 表示失败。

## 向后兼容性

文件头的 `file_format_version` 和 `data_schema_version` 字段由 `BinaryFileHelper` 读写，这些版本信息将由调用方（如 `VectorDataHandler`）用于处理不同版本文件格式的兼容性问题。`BinaryFileHelper` 本身仅负责忠实地读写头部结构。

## 版本和变更历史

- **版本**: 1.0
- **变更历史**:
  - 1.0 (YYYY-MM-DD): 初始创建 `BinaryFileHelper` 类。实现 `readBinaryHeader` 和 `writeBinaryHeader` 方法，用于处理 `BinaryFileHeader` 的读写。添加了 `serializeRow` 和 `deserializeRow` 的占位符接口。包含QDebug日志记录。
