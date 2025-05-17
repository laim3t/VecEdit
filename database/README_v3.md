# 逻辑删除列（标记隐藏方案）实现文档

## 功能概述

本功能实现了数据表中列的"逻辑删除"机制，即通过标记列的可见性状态而非物理删除来实现列的隐藏功能。当用户"删除"一个管脚时，该管脚对应的列不会从数据库或二进制文件中实际删除，而是通过设置一个标志位（IsVisible）将其隐藏，使其在UI界面上不再显示。

## 实施细节

### 1. 数据库结构调整

- 在 `VectorTableColumnConfiguration` 表中添加了 `IsVisible` 布尔字段，默认值为1（可见）
- 创建 `update_to_v3.sql` 升级脚本，为现有表添加该字段并设置默认值
- 更新 `schema.sql`，确保新建的数据库包含此字段

### 2. 数据库版本升级

- 在 `DatabaseManager` 类中将 `LATEST_DB_SCHEMA_VERSION` 从2提升到3
- 添加 `performSchemaUpgradeToV3()` 方法执行数据库版本升级
- 升级过程中执行 `update_to_v3.sql` 脚本，为所有现有记录添加 `IsVisible` 字段并设置为1

### 3. 逻辑删除实现

- 在 `VectorDataHandler` 类中添加 `hideVectorTableColumn` 方法，用于将列的 `IsVisible` 值设为0
- 修改 `loadVectorTableMeta` 函数，查询时只返回 `IsVisible=1` 的列，确保隐藏的列不会显示在界面上

### 4. UI交互更新

- 在删除管脚操作时，不再物理删除数据库配置或修改二进制文件
- 而是调用 `hideVectorTableColumn` 方法将对应列标记为不可见

## 方案优势

1. **数据完整性**：所有数据都保留在数据库和二进制文件中，便于将来可能的恢复操作
2. **性能优化**：避免频繁修改二进制文件，降低I/O操作负担
3. **实现简便**：通过简单的标记机制实现列的隐藏，无需复杂的文件重构
4. **用户体验一致**：从用户角度看，删除操作的行为表现与实际删除相同

## 使用说明

### 开发者使用方法

1. 标记列为不可见：

```cpp
QString errorMsg;
if (!VectorDataHandler::instance().hideVectorTableColumn(tableId, columnName, errorMsg)) {
    qDebug() << "隐藏列失败: " << errorMsg;
}
```

2. 加载表数据时会自动过滤不可见列：

```cpp
// 调用loadVectorTableData时会自动只加载IsVisible=1的列
tableWidget->loadVectorTableData(tableId);
```

### 注意事项

1. 已被隐藏的列仍占用数据库空间和二进制文件中的位置
2. 当前版本不提供自动恢复隐藏列的UI界面，如需恢复需通过数据库直接操作
3. 如果未来需要支持恢复功能，可考虑添加管理界面显示和操作已隐藏的列

## 版本和变更历史

| 版本 | 日期 | 更改内容 |
|------|------|----------|
| V3.0 | 2023-11-30 | 初始实现逻辑删除列功能 |
