bool VectorDataHandler::deleteVectorRowsInRange(int tableId, int fromRow, int toRow, QString &errorMessage)
{
    const QString funcName = "VectorDataHandler::deleteVectorRowsInRange";
    qDebug() << funcName << " - 开始删除范围内的向量行，表ID：" << tableId
             << "，从行：" << fromRow << "，到行：" << toRow;

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
        qWarning() << funcName << " - 错误：" << errorMessage;
        return false;
    }

    // 检查表是否存在
    QSqlQuery tableCheckQuery(db);
    tableCheckQuery.prepare("SELECT table_name FROM vector_tables WHERE id = ?");
    tableCheckQuery.addBindValue(tableId);

    if (!tableCheckQuery.exec() || !tableCheckQuery.next())
    {
        errorMessage = QString("找不到ID为 %1 的向量表").arg(tableId);
        qWarning() << funcName << " - 错误:" << errorMessage;
        return false;
    }

    QString tableName = tableCheckQuery.value(0).toString();
    qDebug() << funcName << " - 向量表名称:" << tableName;

    // 检查输入的行范围是否有效
    if (fromRow <= 0 || toRow <= 0)
    {
        errorMessage = "无效的行范围：起始行和结束行必须大于0";
        qWarning() << funcName << " - 错误：" << errorMessage;
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
        qWarning() << funcName << " - 错误：" << errorMessage;
        return false;
    }

    // [修复] 重新加载完整的列定义，以反映二进制文件的物理结构
    columns = getAllColumnInfo(tableId);

    // 如果元数据显示表中没有行
    if (currentRowCount <= 0)
    {
        errorMessage = "表中没有数据行可删除";
        qWarning() << funcName << " - 错误：" << errorMessage;
        return false;
    }

    qDebug() << funcName << " - 从元数据获取的行数:" << currentRowCount;

    // 2. 解析二进制文件路径
    QString resolveError;
    QString absoluteBinFilePath = resolveBinaryFilePath(tableId, resolveError);
    if (absoluteBinFilePath.isEmpty())
    {
        errorMessage = "无法解析二进制文件路径: " + resolveError;
        qWarning() << funcName << " - 错误：" << errorMessage;
        return false;
    }

    // 3. 检查文件是否存在
    QFile binFile(absoluteBinFilePath);
    if (!binFile.exists())
    {
        errorMessage = "找不到二进制数据文件: " + absoluteBinFilePath;
        qWarning() << funcName << " - 错误：" << errorMessage;
        return false;
    }

    // 4. 读取所有行数据
    QList<Vector::RowData> allRows;
    if (!Persistence::BinaryFileHelper::readAllRowsFromBinary(absoluteBinFilePath, columns, schemaVersion, allRows))
    {
        errorMessage = "从二进制文件读取数据失败";
        qWarning() << funcName << " - 错误：" << errorMessage;
        return false;
    }

    qDebug() << funcName << " - 从二进制文件读取到的行数:" << allRows.size();

    // 验证实际读取的行数与元数据中的行数是否一致
    if (allRows.size() != currentRowCount)
    {
        qWarning() << funcName << " - 警告: 元数据中的行数(" << currentRowCount
                   << ")与二进制文件中的行数(" << allRows.size() << ")不一致";
    }

    // 5. 检查行范围是否有效
    if (fromRow > toRow || fromRow < 1 || toRow > allRows.size())
    {
        errorMessage = QString("无效的行范围：从%1到%2，表总行数为%3").arg(fromRow).arg(toRow).arg(allRows.size());
        qWarning() << funcName << " - 错误：" << errorMessage;
        return false;
    }

    // 调整为基于0的索引
    int startIndex = fromRow - 1;
    int endIndex = toRow - 1;

    // 计算要删除的行数
    int rowsToDelete = endIndex - startIndex + 1;
    qDebug() << funcName << "- 将删除从索引" << startIndex << "到" << endIndex << "的" << rowsToDelete << "行";

    // 6. 删除指定范围的行（从大到小删除，避免索引变化）
    for (int i = endIndex; i >= startIndex; i--)
    {
        allRows.removeAt(i);
        qDebug() << funcName << "- 已删除行索引:" << i;
    }

    // 7. 将更新后的数据写回文件
    if (!Persistence::BinaryFileHelper::writeAllRowsToBinary(absoluteBinFilePath, columns, schemaVersion, allRows))
    {
        errorMessage = "将更新后的数据写回二进制文件失败";
        qWarning() << funcName << " - 错误：" << errorMessage;
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
        qWarning() << funcName << " - 错误：" << errorMessage;
        return false;
    }

    qDebug() << funcName << " - 成功删除了范围内的 " << rowsToDelete << " 行，剩余行数: " << allRows.size();
    return true;
}

bool VectorDataHandler::deleteVectorTable(int tableId, QString &errorMessage)
{
    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        errorMessage = "数据库未打开";
        return false;
    }

    db.transaction();
    QSqlQuery query(db);

    try
    {
        // 先删除与该表关联的管脚值数据
        query.prepare("DELETE FROM vector_table_pin_values WHERE vector_data_id IN "
                      "(SELECT id FROM vector_table_data WHERE table_id = ?)");
        query.addBindValue(tableId);
        if (!query.exec())
        {
            throw QString("删除管脚值数据失败: " + query.lastError().text());
        }

        // 删除向量表数据
        query.prepare("DELETE FROM vector_table_data WHERE table_id = ?");
        query.addBindValue(tableId);
        if (!query.exec())
        {
            throw QString("删除向量表数据失败: " + query.lastError().text());
        }

        // 删除向量表管脚配置
        query.prepare("DELETE FROM vector_table_pins WHERE table_id = ?");
        query.addBindValue(tableId);
        if (!query.exec())
        {
            throw QString("删除向量表管脚配置失败: " + query.lastError().text());
        }

        // 最后删除向量表记录
        query.prepare("DELETE FROM vector_tables WHERE id = ?");
        query.addBindValue(tableId);
        if (!query.exec())
        {
            throw QString("删除向量表记录失败: " + query.lastError().text());
        }

        // 提交事务
        db.commit();
        return true;
    }
    catch (const QString &error)
    {
        // 回滚事务
        db.rollback();
        errorMessage = error;
        return false;
    }
}