huancun # Database Module README

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
2. **数据序列化/反序列化**：提供接口，用于将内存中的行数据对象与二进制字节流进行相互转换。

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
    header.magic_number = Persistence::VEC_BINDATA_MAGIC; // 自动设置 "VECB"
    header.file_format_version = Persistence::CURRENT_FILE_FORMAT_VERSION; // 使用最新格式版本
    header.data_schema_version = 1; // 根据实际情况设置
    header.row_count_in_file = 100; // 准备写入的行数
    header.column_count_in_file = 10; // 列数量
    header.timestamp_created = QDateTime::currentSecsSinceEpoch();
    header.timestamp_updated = header.timestamp_created;
    
    if (Persistence::BinaryFileHelper::writeBinaryHeader(&binFile, header)) {
        // Header written successfully
        // Continue with writing row data...
    } else {
        // Failed to write header
    }
    binFile.close();
}
```

### 序列化和反序列化行数据

```cpp
#include "database/binaryfilehelper.h"
#include "vector/vector_data_types.h"

// 序列化一行数据（使用固定长度格式）
QByteArray serializedRow;
if (Persistence::BinaryFileHelper::serializeRow(rowData, columns, serializedRow, true)) {
    // Serialization successful
    // serializedRow now contains the binary representation of the row
}

// 反序列化行数据（使用固定长度格式）
Vector::RowData rowData;
if (Persistence::BinaryFileHelper::deserializeRow(bytes, columns, fileVersion, rowData, true)) {
    // Deserialization successful
    // rowData now contains the row content
}
```

## 主要方法

- **`readBinaryHeader`**:
  - 输入: 打开的、可读的 `QIODevice*`，指向二进制文件的起始位置（或头部位置）。
  - 输出: 填充的 `BinaryFileHeader` 对象；函数返回 `true` 表示成功，`false` 表示失败。
- **`writeBinaryHeader`**:
  - 输入: 打开的、可写的 `QIODevice*`；有效的 `BinaryFileHeader` 对象。
  - 输出: 函数返回 `true` 表示成功，`false` 表示失败。
- **`serializeRow`**:
  - 输入: 行数据对象、列定义对象列表、是否使用固定长度格式(默认为true)。
  - 输出: 包含序列化字节的 `QByteArray`；函数返回 `true` 表示成功，`false` 表示失败。
- **`deserializeRow`**:
  - 输入: 序列化的字节数据、列定义对象列表、文件版本、是否使用固定长度格式(默认为true)。
  - 输出: 填充的行数据对象；函数返回 `true` 表示成功，`false` 表示失败。

## 向后兼容性

文件头的 `file_format_version` 和 `data_schema_version` 字段由 `BinaryFileHelper` 读写，这些版本信息将由调用方（如 `VectorDataHandler`）用于处理不同版本文件格式的兼容性问题。`BinaryFileHelper` 本身仅负责忠实地读写头部结构。

## 版本和变更历史

- **版本**: 2.0
- **变更历史**:
  - 2.0 (2023-07-01): 实现固定长度二进制存储格式，提高读写效率和数据一致性。
  - 1.0 (2023-01-15): 初始创建 `BinaryFileHelper` 类。实现 `readBinaryHeader` 和 `writeBinaryHeader` 方法，用于处理 `BinaryFileHeader` 的读写。添加了 `serializeRow` 和 `deserializeRow` 的变长格式接口。

# 数据库存储模块

## 功能概述

数据库存储模块负责管理向量编辑器的数据持久化，包括SQLite数据库和二进制文件存储。主要功能包括：

1. SQLite数据库连接和管理
2. 二进制文件读写和序列化
3. 数据结构版本控制和兼容性处理
4. 数据迁移和升级

## 二进制文件处理

本模块包含二进制文件读写的核心逻辑，用于存储向量表的大量数据。二进制文件实现了以下特性：

1. 固定格式的文件头部，包含版本信息、行列计数等
2. 支持不同数据类型的序列化和反序列化，支持固定长度和变长格式
3. 支持向后兼容性和数据格式版本检查
4. 错误处理和数据校验

### 二进制文件格式

每个二进制文件由以下部分组成：

1. **文件头部**: `BinaryFileHeader` 结构，包含文件元数据
2. **数据部分**: 序列化的行数据，每行包含：
   - 行字节长度 (4字节无符号整数)
   - 行数据字节（对于固定长度格式，每个字段长度固定，定义在`binary_field_lengths.h`中）

## 改进：固定长度二进制存储

### 功能概述

为了优化二进制数据存储和提高读写效率，我们将二进制存储格式从可变长度改为固定长度。每种数据类型现在有一个预定义的固定长度，并在前端实现了相应的输入限制，确保数据一致性和存储效率。这一改进还可以提高数据完整性和读写性能。

### 主要变更

1. 新增 `common/binary_field_lengths.h` 定义了各种数据类型的固定长度常量
2. 修改 `BinaryFileHelper` 类，支持固定长度序列化和反序列化
3. 更新 `VectorTableItemDelegate` 实现输入验证和长度限制

### 固定长度定义

数据类型的固定长度如下：

| 数据类型 | 固定长度(字节) | 描述 |
|---------|--------------|------|
| TEXT (通用) | 256 | 通用文本字段 |
| TEXT (Label) | 15 | 标签字段 |
| TEXT (Comment) | 30 | 注释字段 |
| TEXT (EXT) | 5 | EXT字段 |
| PIN_STATE_ID | 1 | 管脚状态标识(0,1,X,L,H,Z) |
| INTEGER/INSTRUCTION_ID/TIMESET_ID | 4 | 整数值 |
| REAL | 8 | 浮点数值 |
| BOOLEAN | 1 | 布尔值 |
| JSON_PROPERTIES | 1024 | JSON对象 |

### 一行数据的总字节数

一行数据的总字节数取决于表中列的数量和类型。以常见的向量表为例，如果一行包含：

- 1个 Label (15字节)
- 1个 Comment (30字节)
- 1个 Instruction (4字节)
- 1个 TimeSet (4字节)
- 10个 PIN_STATE (每个1字节，共10字节)
- 1个 EXT (5字节)

那么这一行的总字节数为：15 + 30 + 4 + 4 + 10 + 5 = 68字节

实际表格可能有更多列，总字节数将根据实际列结构计算。每种数据类型都有固定的存储空间，因此可以精确计算每行数据的存储需求。

### 序列化格式

固定长度序列化采用以下格式：

- TEXT: [字符串长度(4字节) + 实际UTF-8文本 + 填充字节]
- PIN_STATE_ID: [单个字符(1字节)]
- INTEGER类型: [32位整数值(4字节)]
- REAL: [64位双精度浮点数(8字节)]
- BOOLEAN: [8位无符号整数(1字节): 0表示false, 1表示true]
- JSON_PROPERTIES: [JSON字符串长度(4字节) + JSON字符串 + 填充字节]

### 前端输入限制

为保持数据一致性，前端编辑器对输入做了相应长度限制：

- Label输入框限制为15个字符
- Comment输入框限制为30个字符
- EXT输入框限制为2个字符
- 一般TEXT输入框限制为255个字符
- PIN_STATE始终为单个字符

## `database/binaryfilehelper.h` 和 `database/binaryfilehelper.cpp` 文件

`database/binaryfilehelper.h` 和 `database/binaryfilehelper.cpp` 文件定义并实现了 `Persistence::BinaryFileHelper` 类。这个类提供了一系列静态辅助函数，用于处理项目自定义二进制数据文件（例如 `.vbindata`）的底层读写操作。它主要负责：

1. **文件头处理**：读取和写入在 `common/binary_file_format.h` 中定义的 `BinaryFileHeader` 结构。
2. **数据序列化/反序列化**：提供接口，用于将内存中的行数据对象与二进制字节流进行相互转换。

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
    header.magic_number = Persistence::VEC_BINDATA_MAGIC; // 自动设置 "VECB"
    header.file_format_version = Persistence::CURRENT_FILE_FORMAT_VERSION; // 使用最新格式版本
    header.data_schema_version = 1; // 根据实际情况设置
    header.row_count_in_file = 100; // 准备写入的行数
    header.column_count_in_file = 10; // 列数量
    header.timestamp_created = QDateTime::currentSecsSinceEpoch();
    header.timestamp_updated = header.timestamp_created;
    
    if (Persistence::BinaryFileHelper::writeBinaryHeader(&binFile, header)) {
        // Header written successfully
        // Continue with writing row data...
    } else {
        // Failed to write header
    }
    binFile.close();
}
```

### 序列化和反序列化行数据

```cpp
#include "database/binaryfilehelper.h"
#include "vector/vector_data_types.h"

// 序列化一行数据（使用固定长度格式）
QByteArray serializedRow;
if (Persistence::BinaryFileHelper::serializeRow(rowData, columns, serializedRow, true)) {
    // Serialization successful
    // serializedRow now contains the binary representation of the row
}

// 反序列化行数据（使用固定长度格式）
Vector::RowData rowData;
if (Persistence::BinaryFileHelper::deserializeRow(bytes, columns, fileVersion, rowData, true)) {
    // Deserialization successful
    // rowData now contains the row content
}
```

## 主要方法

- **`readBinaryHeader`**:
  - 输入: 打开的、可读的 `QIODevice*`，指向二进制文件的起始位置（或头部位置）。
  - 输出: 填充的 `BinaryFileHeader` 对象；函数返回 `true` 表示成功，`false` 表示失败。
- **`writeBinaryHeader`**:
  - 输入: 打开的、可写的 `QIODevice*`；有效的 `BinaryFileHeader` 对象。
  - 输出: 函数返回 `true` 表示成功，`false` 表示失败。
- **`serializeRow`**:
  - 输入: 行数据对象、列定义对象列表、是否使用固定长度格式(默认为true)。
  - 输出: 包含序列化字节的 `QByteArray`；函数返回 `true` 表示成功，`false` 表示失败。
- **`deserializeRow`**:
  - 输入: 序列化的字节数据、列定义对象列表、文件版本、是否使用固定长度格式(默认为true)。
  - 输出: 填充的行数据对象；函数返回 `true` 表示成功，`false` 表示失败。

## 向后兼容性

文件头的 `file_format_version` 和 `data_schema_version` 字段由 `BinaryFileHelper` 读写，这些版本信息将由调用方（如 `VectorDataHandler`）用于处理不同版本文件格式的兼容性问题。`BinaryFileHelper` 本身仅负责忠实地读写头部结构。

## 版本和变更历史

- **版本**: 2.0
- **变更历史**:
  - 2.0 (2023-07-01): 实现固定长度二进制存储格式，提高读写效率和数据一致性。
  - 1.0 (2023-01-15): 初始创建 `BinaryFileHelper` 类。实现 `readBinaryHeader` 和 `writeBinaryHeader` 方法，用于处理 `BinaryFileHeader` 的读写。添加了 `serializeRow` 和 `deserializeRow` 的变长格式接口。

# 数据库存储模块

## 功能概述

数据库存储模块负责管理向量编辑器的数据持久化，包括SQLite数据库和二进制文件存储。主要功能包括：

1. SQLite数据库连接和管理
2. 二进制文件读写和序列化
3. 数据结构版本控制和兼容性处理
4. 数据迁移和升级

## 二进制文件处理

本模块包含二进制文件读写的核心逻辑，用于存储向量表的大量数据。二进制文件实现了以下特性：

1. 固定格式的文件头部，包含版本信息、行列计数等
2. 支持不同数据类型的序列化和反序列化，支持固定长度和变长格式
3. 支持向后兼容性和数据格式版本检查
4. 错误处理和数据校验

### 二进制文件格式

每个二进制文件由以下部分组成：

1. **文件头部**: `BinaryFileHeader` 结构，包含文件元数据
2. **数据部分**: 序列化的行数据，每行包含：
   - 行字节长度 (4字节无符号整数)
   - 行数据字节（对于固定长度格式，每个字段长度固定，定义在`binary_field_lengths.h`中）

## 最近更新

### 2025-05-16更新：增强二进制文件读取的容错性

**问题描述**：当用户通过管脚选择对话框添加新管脚后，由于二进制文件中缺少新添加管脚的数据，导致表格加载失败。具体表现为用户勾选B管脚后，在主界面无法显示B管脚列。

**解决方案**：

1. 修改`BinaryFileHelper::deserializeRow`方法，增强容错性：
   - 当数据流到达末尾时，为剩余列使用默认值
   - 当读取某列数据失败时，使用默认值并继续处理后续列
   - 添加数据流状态重置功能

2. 修改`BinaryFileHelper::readAllRowsFromBinary`方法，优化行级别错误处理：
   - 当单行反序列化失败时，使用空行数据替代而不是终止整个读取过程

**改进效果**：

- 增强了系统在列结构变化时的稳定性
- 当二进制文件数据与当前列配置不完全匹配时，系统可以继续工作
- 对于新添加的管脚列，系统会使用默认值（"X"）填充
- 确保用户能够正常显示所有配置的管脚，即使二进制文件还未更新

### 2025-05-17更新：修复管脚状态默认值和存储方式

**问题描述**：

1. 当向已有的向量表添加新管脚列时，系统使用"0"作为默认值，而不是期望的"X"作为管脚状态值
2. 在二进制文件中，管脚状态被错误地作为整数而不是字符串存储，导致新管脚列即使显示为"X"，保存后仍会丢失

**解决方案**：

1. 修改`BinaryFileHelper::deserializeRow`方法：
   - 将PIN_STATE_ID类型的列单独处理，在读取失败或流结束时使用"X"作为默认值
   - 将PIN_STATE_ID类型的列作为字符串而不是整数读取
   - 与普通TEXT类型的列区分开，后者仍使用空字符串作为默认值

2. 修改`BinaryFileHelper::readAllRowsFromBinary`方法：
   - 在创建空行时，为PIN_STATE_ID类型的列设置"X"作为默认值

3. 修改`BinaryFileHelper::serializeRow`方法：
   - PIN_STATE_ID类型的列单独处理，确保始终作为字符串写入
   - 当值为空、无效或0时，自动使用"X"作为默认值
   - 确保即使值以整数形式提供，也会被转换为字符串存储

**改进效果**：

- 新添加的管脚列默认显示为"X"而不是"0"
- 确保管脚状态在二进制文件中始终以字符串形式存储，而不是整数
- 提高了用户体验，使显示符合行业惯例（管脚状态未知时通常用"X"表示）
- 确保数据显示与存储的一致性，避免用户混淆
- 保证管脚列数据在保存和重新加载后能够正确保留

## 向后兼容性

本模块保持完全向后兼容。对于旧版本创建的二进制文件，系统会根据文件头中的版本信息选择适当的反序列化方法。当列结构发生变化时（如添加新管脚），系统会自动处理数据差异，确保不会因为数据结构变化而导致加载失败。
