

bool VectorDataHandler::deleteVectorRows(int tableId, const QList<int> &rowIndexes, QString &errorMessage)
{
    const QString funcName = "VectorDataHandler::deleteVectorRows";
    qDebug() << funcName << "- 开始删除向量行，表ID:" << tableId << "，选中行数:" << rowIndexes.size();

    // 初始化缓存（如果尚未初始化）
    if (!m_cacheInitialized)
    {
        initializeCache();
    }

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        errorMessage = "数据库未打开";
        qWarning() << funcName << "- 错误:" << errorMessage;
        return false;
    }

    // 检查表是否存在
    QSqlQuery tableCheckQuery(db);
    tableCheckQuery.prepare("SELECT table_name FROM vector_tables WHERE id = ?");
    tableCheckQuery.addBindValue(tableId);

    if (!tableCheckQuery.exec() || !tableCheckQuery.next())
    {
        errorMessage = QString("找不到ID为 %1 的向量表").arg(tableId);
        qWarning() << funcName << "- 错误:" << errorMessage;
        return false;
    }

    QString tableName = tableCheckQuery.value(0).toString();
    qDebug() << funcName << "- 向量表名称:" << tableName;

    // 检查选中的行索引是否有效
    if (rowIndexes.isEmpty())
    {
        errorMessage = "未选择任何行";
        qWarning() << funcName << "- 错误:" << errorMessage;
        return false;
    }

    // 1. 加载元数据和二进制文件路径
    QString binFileName;
    QList<Vector::ColumnInfo> columns;
    int schemaVersion = 0;
    int currentRowCount = 0;

    if (!loadVectorTableMeta(tableId, binFileName, columns, schemaVersion, currentRowCount))
    {
        errorMessage = QString("无法加载表 %1 的元数据").arg(tableId);
        qWarning() << funcName << "- 错误:" << errorMessage;
        return false;
    }

    // [修复] 重新加载完整的列定义，以反映二进制文件的物理结构
    columns = getAllColumnInfo(tableId);

    // 如果元数据显示表中没有行
    if (currentRowCount <= 0)
    {
        errorMessage = "表中没有数据行可删除";
        qWarning() << funcName << "- 错误:" << errorMessage;
        return false;
    }

    qDebug() << funcName << "- 从元数据获取的行数:" << currentRowCount;

    // 2. 解析二进制文件路径
    QString resolveError;
    QString absoluteBinFilePath = resolveBinaryFilePath(tableId, resolveError);
    if (absoluteBinFilePath.isEmpty())
    {
        errorMessage = "无法解析二进制文件路径: " + resolveError;
        qWarning() << funcName << "- 错误:" << errorMessage;
        return false;
    }

    // 3. 检查文件是否存在
    QFile binFile(absoluteBinFilePath);
    if (!binFile.exists())
    {
        errorMessage = "找不到二进制数据文件: " + absoluteBinFilePath;
        qWarning() << funcName << "- 错误:" << errorMessage;
        return false;
    }

    // 4. 读取所有行数据
    QList<Vector::RowData> allRows;
    if (!Persistence::BinaryFileHelper::readAllRowsFromBinary(absoluteBinFilePath, columns, schemaVersion, allRows))
    {
        errorMessage = "从二进制文件读取数据失败";
        qWarning() << funcName << "- 错误:" << errorMessage;
        return false;
    }

    qDebug() << funcName << "- 从二进制文件读取到的行数:" << allRows.size();

    // 验证实际读取的行数与元数据中的行数是否一致
    if (allRows.size() != currentRowCount)
    {
        qWarning() << funcName << " - 警告: 元数据中的行数(" << currentRowCount
                   << ")与二进制文件中的行数(" << allRows.size() << ")不一致";
    }

    // 5. 检查行索引是否有效
    QList<int> validRowIndexes;
    for (int rowIndex : rowIndexes)
    {
        if (rowIndex >= 0 && rowIndex < allRows.size())
        {
            validRowIndexes.append(rowIndex);
            qDebug() << funcName << "- 将删除行索引:" << rowIndex;
        }
        else
        {
            qWarning() << funcName << "- 忽略无效的行索引:" << rowIndex << "，总行数:" << allRows.size();
        }
    }

    if (validRowIndexes.isEmpty())
    {
        errorMessage = "没有有效的行索引可删除";
        qWarning() << funcName << "- 错误:" << errorMessage;
        return false;
    }

    // 6. 删除指定的行（从大到小排序，避免索引变化影响删除）
    std::sort(validRowIndexes.begin(), validRowIndexes.end(), std::greater<int>());

    for (int rowIndex : validRowIndexes)
    {
        allRows.removeAt(rowIndex);
        qDebug() << funcName << "- 已删除行索引:" << rowIndex;
    }

    // 7. 将更新后的数据写回文件
    if (!Persistence::BinaryFileHelper::writeAllRowsToBinary(absoluteBinFilePath, columns, schemaVersion, allRows))
    {
        errorMessage = "将更新后的数据写回二进制文件失败";
        qWarning() << funcName << "- 错误:" << errorMessage;
        return false;
    }

    // 8. 更新数据库中的行数记录
    QSqlQuery updateRowCountQuery(db);
    updateRowCountQuery.prepare("UPDATE VectorTableMasterRecord SET row_count = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?");
    updateRowCountQuery.addBindValue(allRows.size());
    updateRowCountQuery.addBindValue(tableId);

    if (!updateRowCountQuery.exec())
    {
        errorMessage = "更新数据库中的行数记录失败: " + updateRowCountQuery.lastError().text();
        qWarning() << funcName << "- 错误:" << errorMessage;
        return false;
    }

    qDebug() << funcName << "- 成功删除了" << validRowIndexes.size() << "行，剩余行数:" << allRows.size();
    return true;
}
