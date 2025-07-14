# Bug修复记录：删除向量表后无法重新创建同名表

## 问题描述

在"新轨道"架构下，执行"删除向量表"操作后，再尝试创建一个同名的向量表时，系统提示"已存在同名向量表"的错误。

相关日志信息：

```
[2025-07-14 16:07:44.775] [Warning] "MainWindow::loadVectorTableMeta" - 查询主记录失败, 表ID: 2 , 错误: "" (mainwindow_datafix_helpers.cpp:496, bool MainWindow::loadVectorTableMeta(int, QString&, QList<Vector::ColumnInfo>&, int&, int&))
```

## 问题分析

通过代码分析，发现以下问题：

1. 在 `RobustVectorDataHandler::deleteVectorTable` 方法中，删除了以下三个表中的记录：
   - `VectorTableRowIndex` - 行索引记录
   - `VectorTableColumnConfiguration` - 列配置记录
   - `VectorTableMasterRecord` - 主表记录

2. 但是没有处理 `vector_tables` 表中的记录，这导致在检查表名是否存在时（依据 `vector_tables` 表），系统认为同名表仍然存在。

3. 在 `MainWindow::addNewVectorTable` 方法中，检查表名是否存在时，是通过查询 `vector_tables` 表来判断的：

   ```cpp
   checkQuery.prepare("SELECT COUNT(*) FROM vector_tables WHERE table_name = ?");
   ```

## 首次解决尝试及问题

我首先尝试的解决方案是：在 `RobustVectorDataHandler::deleteVectorTable` 方法中，添加删除 `vector_tables` 表记录的代码。

然而，这导致了新的问题：当创建同名表时，虽然不再提示"已存在同名向量表"，但表格创建后没有默认的六列字段（Label, Instruction, TimeSet, Capture, EXT, Comment），界面显示为空白。

这表明 `vector_tables` 表中的记录在系统中扮演着重要角色，并且与其他组件有复杂的依赖关系。简单删除这些记录破坏了创建新表时必要的列配置初始化流程。

## 改进的解决方案

经过深入分析系统设计，我采用了更加合适的解决方案：

1. 不直接删除 `vector_tables` 表中的记录，而是将表名重命名为一个带有时间戳和"_deleted"标记的唯一名称。

2. 具体实现：

   ```cpp
   // 获取表名并更新vector_tables表以允许重新使用表名
   QSqlQuery getTableNameQuery(db);
   getTableNameQuery.prepare("SELECT table_name FROM vector_tables WHERE id = ?");
   getTableNameQuery.addBindValue(tableId);
   QString oldTableName;
   if (getTableNameQuery.exec() && getTableNameQuery.next())
   {
       oldTableName = getTableNameQuery.value(0).toString();
       // 在表名后添加时间戳和"_deleted"标记，使其唯一，同时释放原表名
       QString newTableName = QString("%1_deleted_%2").arg(oldTableName).arg(QDateTime::currentMSecsSinceEpoch());
       
       QSqlQuery updateTableNameQuery(db);
       updateTableNameQuery.prepare("UPDATE vector_tables SET table_name = ? WHERE id = ?");
       updateTableNameQuery.addBindValue(newTableName);
       updateTableNameQuery.addBindValue(tableId);
       // 执行并记录结果...
   }
   ```

3. 这种方法的优势：
   - 保留了 `vector_tables` 表记录的结构和关联关系
   - 释放了原表名，允许创建同名表
   - 维护了数据完整性，不会干扰新表创建时的列配置初始化
   - 实现了"软删除"概念，为将来可能的恢复功能留下了可能性

## 影响范围

该Bug会影响用户在删除向量表后再创建同名表的操作，导致用户体验不佳。修复后，用户可以正常地删除向量表并重新创建同名表，同时保持系统的数据完整性。

## 验证方法

1. 创建一个向量表，记录其名称
2. 删除该向量表
3. 尝试再次创建相同名称的向量表
4. 验证是否能够成功创建，且新表应正确显示默认的六列字段（Label, Instruction, TimeSet, Capture, EXT, Comment）

## 后续建议

1. 考虑为 `vector_tables` 表添加一个 `is_deleted` 标志字段，以便更明确地标记已删除的表
2. 实现一个表回收站功能，允许用户恢复误删的表
3. 完善系统文档，明确记录各表之间的依赖关系，避免类似问题再次发生
4. 增加单元测试，覆盖删除和重建向量表的场景
