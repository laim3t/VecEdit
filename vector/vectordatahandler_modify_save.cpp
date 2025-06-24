

// 实现优化版本的保存分页数据函数
bool VectorDataHandler::saveVectorTableDataPaged(int tableId, QTableWidget *currentPageTable,
                                                 int currentPage, int pageSize, int totalRows,
                                                 QString &errorMessage)
{
    const QString funcName = "VectorDataHandler::saveVectorTableDataPaged";

    // 只记录关键日志，减少日志量提升性能
    qDebug() << funcName << " - 开始保存分页数据, 表ID:" << tableId
             << ", 当前页:" << currentPage << ", 页大小:" << pageSize;

    if (!currentPageTable)
    {
        errorMessage = "表控件为空";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 初始化缓存（如果尚未初始化）
    if (!m_cacheInitialized)
    {
        initializeCache();
    }

    // 1. 获取列信息和预期 schema version (从元数据 - 只包含可见列)
    QString binFileName;
    QList<Vector::ColumnInfo> visibleColumns;
    int schemaVersion = 0;
    int rowCount = 0;
    if (!loadVectorTableMeta(tableId, binFileName, visibleColumns, schemaVersion, rowCount))
    {
        errorMessage = "元数据加载失败，无法确定列结构和版本";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 如果没有列配置，则无法保存
    if (visibleColumns.isEmpty())
    {
        errorMessage = QString("表 %1 没有可见的列配置，无法保存数据。").arg(tableId);
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 1.1 加载所有列信息（包括隐藏列）- 这对于正确序列化至关重要
    QList<Vector::ColumnInfo> allColumns;
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery allColQuery(db);
    allColQuery.prepare("SELECT id, column_name, column_order, column_type, data_properties, IsVisible FROM VectorTableColumnConfiguration WHERE master_record_id = ? ORDER BY column_order");
    allColQuery.addBindValue(tableId);
    if (!allColQuery.exec())
    {
        errorMessage = "查询完整列结构失败: " + allColQuery.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    while (allColQuery.next())
    {
        Vector::ColumnInfo col;
        col.id = allColQuery.value(0).toInt();
        col.vector_table_id = tableId;
        col.name = allColQuery.value(1).toString();
        col.order = allColQuery.value(2).toInt();
        col.original_type_str = allColQuery.value(3).toString();
        col.type = Vector::columnDataTypeFromString(col.original_type_str);
        col.is_visible = allColQuery.value(5).toBool();

        QString propStr = allColQuery.value(4).toString();
        if (!propStr.isEmpty())
        {
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(propStr.toUtf8(), &err);
            if (err.error == QJsonParseError::NoError && doc.isObject())
            {
                col.data_properties = doc.object();
            }
        }

        allColumns.append(col);
    }

    // 构建ID到列索引的映射
    QMap<int, int> columnIdToIndexMap;
    for (int i = 0; i < allColumns.size(); ++i)
    {
        columnIdToIndexMap[allColumns[i].id] = i;
    }

    // 2. 解析二进制文件路径
    QString resolveErrorMsg;
    QString absoluteBinFilePath = resolveBinaryFilePath(tableId, resolveErrorMsg);
    if (absoluteBinFilePath.isEmpty())
    {
        errorMessage = "无法解析二进制文件路径: " + resolveErrorMsg;
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 检查目标目录是否存在
    QFileInfo binFileInfo(absoluteBinFilePath);
    QDir binDir = binFileInfo.dir();
    if (!binDir.exists() && !binDir.mkpath("."))
    {
        errorMessage = QString("无法创建目标二进制目录: %1").arg(binDir.absolutePath());
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 3. 建立表格控件列与数据库可见列之间的映射关系
    int tableColCount = currentPageTable->columnCount();
    int visibleDbColCount = visibleColumns.size();

    // 确保表头与数据库列名一致，构建映射关系
    QMap<int, int> tableColToVisibleDbColMap; // 键: 表格列索引, 值: 数据库可见列索引

    for (int tableCol = 0; tableCol < tableColCount; ++tableCol)
    {
        QString tableHeader = currentPageTable->horizontalHeaderItem(tableCol)->text();
        // 对于包含换行符的列名（如管脚列），只取第一行作为管脚名
        QString simplifiedHeader = tableHeader.split("\n").first();

        // 查找匹配的数据库可见列
        bool found = false;
        for (int dbCol = 0; dbCol < visibleDbColCount; ++dbCol)
        {
            // 对于管脚列，只比较管脚名部分
            if (visibleColumns[dbCol].name == simplifiedHeader ||
                (visibleColumns[dbCol].type == Vector::ColumnDataType::PIN_STATE_ID &&
                 tableHeader.startsWith(visibleColumns[dbCol].name + "\n")))
            {
                tableColToVisibleDbColMap[tableCol] = dbCol;
                found = true;
                break;
            }
        }
        if (!found)
        {
            errorMessage = QString("无法找到表格列 '%1' 对应的数据库可见列").arg(tableHeader);
            qWarning() << funcName << " - " << errorMessage;
            return false;
        }
    }

    // 如果映射关系不完整，无法保存
    if (tableColToVisibleDbColMap.size() != tableColCount)
    {
        errorMessage = QString("表格列与数据库可见列映射不完整，无法保存");
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 4. 性能优化: 使用缓存机制避免每次都从文件读取所有行数据
    QList<Vector::RowData> allRows;

    // 检查缓存是否有效
    bool useCachedData = isTableDataCacheValid(tableId, absoluteBinFilePath);

    if (useCachedData)
    {
        // 使用缓存数据
        allRows = m_tableDataCache[tableId];
        qDebug() << funcName << " - 使用表" << tableId << "的缓存数据，包含" << allRows.size() << "行";
    }
    else
    {
        // 缓存无效，从文件读取所有行
        qDebug() << funcName << " - 表" << tableId << "的缓存无效或不存在，从文件读取";

        // 从二进制文件读取所有行
        if (!readAllRowsFromBinary(absoluteBinFilePath, allColumns, schemaVersion, allRows))
        {
            // 如果读取失败，可能是文件不存在或格式有问题，尝试创建新数据
            qWarning() << funcName << " - 读取现有二进制数据失败，将创建新数据";
            allRows.clear();

            // 确保行数量足够
            allRows.reserve(totalRows);
            for (int row = 0; row < totalRows; ++row)
            {
                Vector::RowData rowData;
                // rowData.resize(allColumns.size()); // QT6写法
                int targetSize4 = allColumns.size(); // QT5写法开始
                while (rowData.size() < targetSize4)
                {
                    rowData.append(QVariant());
                }
                // QT5写法结束

                // 设置所有列的默认值
                for (int colIdx = 0; colIdx < allColumns.size(); ++colIdx)
                {
                    const auto &col = allColumns[colIdx];
                    if (col.type == Vector::ColumnDataType::PIN_STATE_ID)
                    {
                        rowData[colIdx] = "X"; // 管脚列默认为X
                    }
                    else
                    {
                        rowData[colIdx] = QVariant(); // 其他列使用默认空值
                    }
                }

                allRows.append(rowData);
            }
        }

        // 更新缓存
        updateTableDataCache(tableId, allRows, absoluteBinFilePath);
    }

    // 确保数据行数与totalRows匹配
    if (allRows.size() < totalRows)
    {
        int additionalRows = totalRows - allRows.size();

        for (int i = 0; i < additionalRows; ++i)
        {
            Vector::RowData rowData;
            // rowData.resize(allColumns.size()); // QT6写法
            int targetSize5 = allColumns.size(); // QT5写法开始
            while (rowData.size() < targetSize5)
            {
                rowData.append(QVariant());
            }
            // QT5写法结束

            // 设置默认值
            for (int colIdx = 0; colIdx < allColumns.size(); ++colIdx)
            {
                const auto &col = allColumns[colIdx];
                if (col.type == Vector::ColumnDataType::PIN_STATE_ID)
                {
                    rowData[colIdx] = "X";
                }
                else
                {
                    rowData[colIdx] = QVariant();
                }
            }

            allRows.append(rowData);
        }

        // 如果添加了行，更新缓存
        if (additionalRows > 0)
        {
            updateTableDataCache(tableId, allRows, absoluteBinFilePath);
        }
    }

    // 预先获取指令和TimeSet的ID映射以便转换
    QMap<QString, int> instructionNameToIdMap;
    QMap<QString, int> timesetNameToIdMap;

    // 获取指令名称到ID的映射
    QSqlQuery instructionQuery(db);
    if (instructionQuery.exec("SELECT id, instruction_value FROM instruction_options"))
    {
        while (instructionQuery.next())
        {
            int id = instructionQuery.value(0).toInt();
            QString name = instructionQuery.value(1).toString();
            instructionNameToIdMap[name] = id;

            // 更新缓存
            if (!m_instructionCache.contains(id) || m_instructionCache[id] != name)
            {
                m_instructionCache[id] = name;
            }
        }
    }

    // 获取TimeSet名称到ID的映射
    QSqlQuery timesetQuery(db);
    if (timesetQuery.exec("SELECT id, timeset_name FROM timeset_list"))
    {
        while (timesetQuery.next())
        {
            int id = timesetQuery.value(0).toInt();
            QString name = timesetQuery.value(1).toString();
            timesetNameToIdMap[name] = id;

            // 更新缓存
            if (!m_timesetCache.contains(id) || m_timesetCache[id] != name)
            {
                m_timesetCache[id] = name;
            }
        }
    }

    // 5. 更新当前页的数据到内存中，而不创建临时表格
    int startRowIndex = currentPage * pageSize;
    int rowsInCurrentPage = currentPageTable->rowCount();
    int modifiedCount = 0; // 记录修改的行数

    // 确保不会越界
    if (startRowIndex >= allRows.size())
    {
        errorMessage = QString("当前页起始行(%1)超出总行数(%2)").arg(startRowIndex).arg(allRows.size());
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 更新当前页的行数据到allRows
    for (int rowInPage = 0; rowInPage < rowsInCurrentPage; ++rowInPage)
    {
        int rowInFullData = startRowIndex + rowInPage;
        if (rowInFullData >= allRows.size())
        {
            break; // 防止越界
        }

        bool rowModified = false; // 标记此行是否有修改

        // 为行创建一份新数据
        Vector::RowData &rowData = allRows[rowInFullData];

        // 从当前页表格更新数据
        for (int tableCol = 0; tableCol < tableColCount; ++tableCol)
        {
            int visibleDbCol = tableColToVisibleDbColMap[tableCol];
            const auto &visibleColumn = visibleColumns[visibleDbCol];

            // 找到此可见列对应的原始列索引
            if (!columnIdToIndexMap.contains(visibleColumn.id))
            {
                continue; // 跳过找不到的列
            }

            int originalColIdx = columnIdToIndexMap[visibleColumn.id];
            QVariant oldValue = rowData[originalColIdx]; // 保存旧值用于比较

            // 处理单元格内容 - 特殊处理管脚列（单元格控件）
            if (visibleColumn.type == Vector::ColumnDataType::PIN_STATE_ID)
            {
                // 对于管脚列，优先从单元格控件获取值
                PinValueLineEdit *pinEdit = qobject_cast<PinValueLineEdit *>(currentPageTable->cellWidget(rowInPage, tableCol));
                if (pinEdit)
                {
                    QString pinStateText = pinEdit->text();
                    if (pinStateText.isEmpty())
                    {
                        pinStateText = "X"; // 默认值
                    }

                    // 检查数据是否有变更
                    if (oldValue.toString() != pinStateText)
                    {
                        rowData[originalColIdx] = pinStateText;
                        rowModified = true;
                    }

                    continue; // 已处理，跳过下面的处理
                }

                // 如果没有找到编辑控件，尝试从QTableWidgetItem获取值
                QTableWidgetItem *item = currentPageTable->item(rowInPage, tableCol);
                if (item && !item->text().isEmpty())
                {
                    QString pinStateText = item->text();

                    // 检查数据是否有变更
                    if (oldValue.toString() != pinStateText)
                    {
                        rowData[originalColIdx] = pinStateText;
                        rowModified = true;
                    }

                    continue;
                }
            }
            else
            {
                // 非管脚列
                QTableWidgetItem *item = currentPageTable->item(rowInPage, tableCol);
                if (!item)
                {
                    continue; // 不更新空单元格
                }

                // 获取单元格文本
                QString cellText = item->text();
                QVariant newValue;

                // 根据列类型处理不同格式的数据
                if (visibleColumn.type == Vector::ColumnDataType::INSTRUCTION_ID)
                {
                    // 将指令名称转换为ID存储
                    if (instructionNameToIdMap.contains(cellText))
                    {
                        int instructionId = instructionNameToIdMap[cellText];
                        newValue = instructionId;
                    }
                    else
                    {
                        newValue = -1; // 默认未知ID
                    }
                }
                else if (visibleColumn.type == Vector::ColumnDataType::TIMESET_ID)
                {
                    // 将TimeSet名称转换为ID存储
                    if (timesetNameToIdMap.contains(cellText))
                    {
                        int timesetId = timesetNameToIdMap[cellText];
                        newValue = timesetId;
                    }
                    else
                    {
                        newValue = -1; // 默认未知ID
                    }
                }
                else if (visibleColumn.type == Vector::ColumnDataType::BOOLEAN)
                {
                    // 布尔值处理
                    newValue = (cellText.toUpper() == "Y");
                }
                else
                {
                    // 普通文本或数字值
                    newValue = cellText;
                }

                // 检查数据是否有变更
                if (oldValue != newValue)
                {
                    rowData[originalColIdx] = newValue;
                    rowModified = true;
                }
            }
        }

        if (rowModified)
        {
            modifiedCount++;
            // 标记行为已修改
            markRowAsModified(tableId, rowInFullData);
        }
    }

    // 如果没有检测到任何数据变更，可以直接返回成功
    if (modifiedCount == 0 && m_modifiedRows.value(tableId, QSet<int>()).isEmpty()) // 确保同时检查modifiedCount和m_modifiedRows
    {
        errorMessage = "没有检测到数据变更 (modifiedCount is 0 and m_modifiedRows is empty for tableId)，跳过保存";
        qInfo() << funcName << " - " << errorMessage; // 改为qInfo以便区分
        return true;
    }
    qDebug() << funcName << " - 检测到 modifiedCount:" << modifiedCount << "，或 m_modifiedRows 非空。准备保存。";

    // 6. 写入二进制文件 - 使用优化的方式
    qDebug() << funcName << " - 准备写入数据到二进制文件，报告的 modifiedCount:" << modifiedCount;

    QMap<int, Vector::RowData> modifiedRowsMap;
    if (m_modifiedRows.contains(tableId))
    {
        const QSet<int> &rowsActuallyMarked = m_modifiedRows.value(tableId);
        qDebug() << funcName << " - m_modifiedRows for table" << tableId << "contains" << rowsActuallyMarked.size() << "indices:" << rowsActuallyMarked;
        for (int rowIndex : rowsActuallyMarked)
        {
            if (rowIndex >= 0 && rowIndex < allRows.size())
            {
                modifiedRowsMap[rowIndex] = allRows[rowIndex];
            }
            else
            {
                qWarning() << funcName << " - Invalid rowIndex" << rowIndex << "found in m_modifiedRows. Skipping.";
            }
        }
    }
    else
    {
        qDebug() << funcName << " - m_modifiedRows does not contain tableId" << tableId;
    }

    // 备用逻辑检查：如果UI报告有修改，但内部标记的修改行集合为空或未填充到map
    if (modifiedRowsMap.isEmpty() && modifiedCount > 0)
    {
        qWarning() << funcName << " - WARNING: modifiedRowsMap is empty BUT modifiedCount is" << modifiedCount
                   << ". This might indicate an issue with how m_modifiedRows is populated or cleared."
                   << "Falling back to considering all rows in the current page as modified for the map.";
        // 重新从当前页构建（这通常不应该发生如果 markRowAsModified 正常工作）
        for (int rowInPage = 0; rowInPage < rowsInCurrentPage; ++rowInPage)
        {
            int rowInFullData = startRowIndex + rowInPage;
            if (rowInFullData >= 0 && rowInFullData < allRows.size())
            {
                // 只添加当前页内且在 allRows 范围内的行
                if (isRowModified(tableId, rowInFullData))
                { // 再次检查是否真的被标记为修改
                    modifiedRowsMap[rowInFullData] = allRows[rowInFullData];
                }
            }
        }
        qDebug() << funcName << " - After fallback population from current page, modifiedRowsMap.size() is now:" << modifiedRowsMap.size();
    }

    qDebug() << funcName << " - Final check before deciding write strategy: modifiedCount =" << modifiedCount
             << ", modifiedRowsMap.size() =" << modifiedRowsMap.size();

    bool writeSuccess = false;
    if (modifiedCount > 0)
    { // 主要条件：确实有修改发生
        if (!modifiedRowsMap.isEmpty())
        {
            qInfo() << funcName << " - Attempting INCREMENTAL update with" << modifiedRowsMap.size() << "rows in modifiedRowsMap.";
            // 使用新的健壮版本的增量更新函数
            writeSuccess = Persistence::BinaryFileHelper::robustUpdateRowsInBinary(absoluteBinFilePath, allColumns, schemaVersion, modifiedRowsMap);

            if (!writeSuccess)
            {
                qWarning() << funcName << " - ROBUST INCREMENTAL update FAILED. Attempting FULL rewrite.";
                // 在这里，我们期望 robustUpdateRowsInBinary 内部已经打印了详细日志指明具体失败原因
                writeSuccess = Persistence::BinaryFileHelper::writeAllRowsToBinary(absoluteBinFilePath, allColumns, schemaVersion, allRows);
                if (writeSuccess)
                {
                    qInfo() << funcName << " - FULL rewrite successful after robust incremental update failed.";
                }
                else
                {
                    qCritical() << funcName << " - CRITICAL: FULL rewrite ALSO FAILED after robust incremental update failed. Data NOT saved for file:" << absoluteBinFilePath;
                }
            }
            else
            {
                qInfo() << funcName << " - ROBUST INCREMENTAL update successful.";
            }
        }
        else // modifiedRowsMap is empty, but modifiedCount > 0
        {
            qWarning() << funcName << " - modifiedRowsMap is EMPTY, but modifiedCount is" << modifiedCount
                       << ". This is unexpected if changes were properly tracked."
                       << "Proceeding with FULL rewrite to ensure data integrity.";
            writeSuccess = Persistence::BinaryFileHelper::writeAllRowsToBinary(absoluteBinFilePath, allColumns, schemaVersion, allRows);
            if (writeSuccess)
            {
                qInfo() << funcName << " - FULL rewrite successful (due to empty modifiedRowsMap but positive modifiedCount).";
            }
            else
            {
                qCritical() << funcName << " - CRITICAL: FULL rewrite FAILED (due to empty modifiedRowsMap but positive modifiedCount). Data NOT saved for file:" << absoluteBinFilePath;
            }
        }
    }
    else
    { // modifiedCount == 0 (and m_modifiedRows was also empty from earlier check)
        qInfo() << funcName << " - No modifications detected (modifiedCount is 0), no save operation performed.";
        //  errorMessage = "没有检测到数据变更，跳过保存"; // Set earlier
        return true; // No changes, so considered a "successful" save of no changes.
    }

    if (!writeSuccess)
    {
        errorMessage = QString("写入二进制文件失败 (final check): %1").arg(absoluteBinFilePath);
        // qWarning 已经被上面的逻辑覆盖，这里可以不用重复，除非上面的逻辑有遗漏
        // qWarning() << funcName << " - " << errorMessage; // Redundant if qCritical used above for write failures
        return false;
    }
    else
    {
        qInfo() << funcName << " - Data saved successfully to" << absoluteBinFilePath << "(Cache will be updated).";
        updateTableDataCache(tableId, allRows, absoluteBinFilePath); // 更新缓存
    }

    // 7. 更新数据库中的行数和时间戳记录 (这部分逻辑保持不变)
    if (!db.transaction())
    {
        errorMessage = "无法开始数据库事务以更新主记录。";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 计算最终的行数
    int finalRowCount = allRows.size();

    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE VectorTableMasterRecord SET row_count = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?");
    updateQuery.addBindValue(finalRowCount);
    updateQuery.addBindValue(tableId);

    if (!updateQuery.exec())
    {
        errorMessage = QString("更新数据库中的行数记录失败: %1").arg(updateQuery.lastError().text());
        qWarning() << funcName << " - " << errorMessage;
        db.rollback();
        return false;
    }

    if (!db.commit())
    {
        errorMessage = "提交数据库事务失败。";
        qWarning() << funcName << " - " << errorMessage;
        db.rollback();
        return false;
    }

    // 清除当前表的修改标记
    clearModifiedRows(tableId);

    errorMessage = QString("已成功保存数据，更新了 %1 行").arg(modifiedCount);
    qDebug() << funcName << " - 保存成功，修改行数:" << modifiedCount << ", 总行数:" << finalRowCount;

    return true;
}


bool VectorDataHandler::saveVectorTableData(int tableId, QTableWidget *tableWidget, QString &errorMessage)
{
    const QString funcName = "VectorDataHandler::saveVectorTableData";
    qDebug() << funcName << " - 开始保存, 表ID:" << tableId;
    if (!tableWidget)
    {
        errorMessage = "表控件为空";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 初始化缓存（如果尚未初始化）
    if (!m_cacheInitialized)
    {
        initializeCache();
    }

    // 1. 获取列信息和预期 schema version (从元数据 - 只包含可见列)
    QString ignoredBinFileName; // We resolve the path separately
    QList<Vector::ColumnInfo> visibleColumns;
    int schemaVersion = 0;
    int ignoredRowCount = 0;
    if (!loadVectorTableMeta(tableId, ignoredBinFileName, visibleColumns, schemaVersion, ignoredRowCount))
    {
        errorMessage = "元数据加载失败，无法确定列结构和版本";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 如果没有列配置，则无法保存
    if (visibleColumns.isEmpty())
    {
        errorMessage = QString("表 %1 没有可见的列配置，无法保存数据。").arg(tableId);
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }
    qDebug() << funcName << " - 可见列元数据加载成功, 列数:" << visibleColumns.size() << ", Schema版本:" << schemaVersion;

    // 1.1 加载所有列信息（包括隐藏列）- 这对于正确序列化至关重要
    QList<Vector::ColumnInfo> allColumns;
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery allColQuery(db);
    allColQuery.prepare("SELECT id, column_name, column_order, column_type, data_properties, IsVisible FROM VectorTableColumnConfiguration WHERE master_record_id = ? ORDER BY column_order");
    allColQuery.addBindValue(tableId);
    if (!allColQuery.exec())
    {
        errorMessage = "查询完整列结构失败: " + allColQuery.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    while (allColQuery.next())
    {
        Vector::ColumnInfo col;
        col.id = allColQuery.value(0).toInt();
        col.vector_table_id = tableId;
        col.name = allColQuery.value(1).toString();
        col.order = allColQuery.value(2).toInt();
        col.original_type_str = allColQuery.value(3).toString();
        col.type = Vector::columnDataTypeFromString(col.original_type_str);
        col.is_visible = allColQuery.value(5).toBool();

        QString propStr = allColQuery.value(4).toString();
        if (!propStr.isEmpty())
        {
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(propStr.toUtf8(), &err);
            if (err.error == QJsonParseError::NoError && doc.isObject())
            {
                col.data_properties = doc.object();
            }
        }

        allColumns.append(col);
        qDebug() << funcName << " - 加载列: ID=" << col.id << ", 名称=" << col.name
                 << ", 顺序=" << col.order << ", 可见=" << col.is_visible << ", 类型=" << col.original_type_str;
    }

    // 构建ID到列索引的映射，用于后续查找
    QMap<int, int> columnIdToIndexMap;
    for (int i = 0; i < allColumns.size(); ++i)
    {
        columnIdToIndexMap[allColumns[i].id] = i;
        qDebug() << funcName << " - 列ID映射: ID=" << allColumns[i].id << " -> 索引=" << i
                 << ", 名称=" << allColumns[i].name;
    }

    // 2. 解析二进制文件路径
    QString resolveErrorMsg;
    QString absoluteBinFilePath = resolveBinaryFilePath(tableId, resolveErrorMsg);
    if (absoluteBinFilePath.isEmpty())
    {
        errorMessage = "无法解析二进制文件路径: " + resolveErrorMsg;
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 检查目标目录是否存在，如果不存在则尝试创建
    QFileInfo binFileInfo(absoluteBinFilePath);
    QDir binDir = binFileInfo.dir();
    if (!binDir.exists())
    {
        qInfo() << funcName << " - 目标二进制目录不存在，尝试创建:" << binDir.absolutePath();
        if (!binDir.mkpath("."))
        {
            errorMessage = QString("无法创建目标二进制目录: %1").arg(binDir.absolutePath());
            qWarning() << funcName << " - " << errorMessage;
            return false;
        }
    }

    // 3. 建立表格控件列与数据库可见列之间的映射关系
    int tableColCount = tableWidget->columnCount();
    int visibleDbColCount = visibleColumns.size();
    qDebug() << funcName << " - 表格列数:" << tableColCount << ", 数据库可见列数:" << visibleDbColCount
             << ", 数据库总列数:" << allColumns.size();

    // 确保表头与数据库列名一致，构建映射关系
    QMap<int, int> tableColToVisibleDbColMap; // 键: 表格列索引, 值: 数据库可见列索引

    for (int tableCol = 0; tableCol < tableColCount; ++tableCol)
    {
        QString tableHeader = tableWidget->horizontalHeaderItem(tableCol)->text();
        // 对于包含换行符的列名（如管脚列），只取第一行作为管脚名
        QString simplifiedHeader = tableHeader.split("\n").first();

        qDebug() << funcName << " - 处理表格列" << tableCol << ", 原始表头:" << tableHeader
                 << ", 简化后:" << simplifiedHeader;

        // 查找匹配的数据库可见列
        bool found = false;
        for (int dbCol = 0; dbCol < visibleDbColCount; ++dbCol)
        {
            // 对于管脚列，只比较管脚名部分
            if (visibleColumns[dbCol].name == simplifiedHeader ||
                (visibleColumns[dbCol].type == Vector::ColumnDataType::PIN_STATE_ID &&
                 tableHeader.startsWith(visibleColumns[dbCol].name + "\n")))
            {
                tableColToVisibleDbColMap[tableCol] = dbCol;
                qDebug() << funcName << " - 映射表格列" << tableCol << "(" << tableHeader << ") -> 数据库可见列" << dbCol << "(" << visibleColumns[dbCol].name << ")";
                found = true;
                break;
            }
        }
        if (!found)
        {
            qWarning() << funcName << " - 警告: 找不到表格列" << tableCol << "(" << tableHeader << ")对应的数据库可见列";
            errorMessage = QString("无法找到表格列 '%1' 对应的数据库可见列").arg(tableHeader);
            return false;
        }
    }

    // 如果映射关系不完整，无法保存
    if (tableColToVisibleDbColMap.size() != tableColCount)
    {
        errorMessage = QString("表格列与数据库可见列映射不完整，无法保存");
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 4. 收集所有行数据
    QList<Vector::RowData> allRows;
    int tableRowCount = tableWidget->rowCount();
    qDebug() << funcName << " - 从 QTableWidget (行:" << tableRowCount << ", 列:" << tableColCount << ") 收集数据";

    // 预先获取指令和TimeSet的ID映射以便转换
    QMap<QString, int> instructionNameToIdMap;
    QMap<QString, int> timesetNameToIdMap;

    // 获取指令名称到ID的映射
    QSqlQuery instructionQuery(db);
    if (instructionQuery.exec("SELECT id, instruction_value FROM instruction_options"))
    {
        while (instructionQuery.next())
        {
            int id = instructionQuery.value(0).toInt();
            QString name = instructionQuery.value(1).toString();
            instructionNameToIdMap[name] = id;

            // 同时更新缓存
            if (!m_instructionCache.contains(id) || m_instructionCache[id] != name)
            {
                m_instructionCache[id] = name;
                qDebug() << funcName << " - 更新指令缓存: " << name << " -> ID:" << id;
            }

            qDebug() << funcName << " - 指令映射: " << name << " -> ID:" << id;
        }
    }
    else
    {
        qWarning() << funcName << " - 获取指令映射失败: " << instructionQuery.lastError().text();
    }

    // 获取TimeSet名称到ID的映射
    QSqlQuery timesetQuery(db);
    if (timesetQuery.exec("SELECT id, timeset_name FROM timeset_list"))
    {
        while (timesetQuery.next())
        {
            int id = timesetQuery.value(0).toInt();
            QString name = timesetQuery.value(1).toString();
            timesetNameToIdMap[name] = id;

            // 同时更新缓存
            if (!m_timesetCache.contains(id) || m_timesetCache[id] != name)
            {
                m_timesetCache[id] = name;
                qDebug() << funcName << " - 更新TimeSet缓存: " << name << " -> ID:" << id;
            }

            qDebug() << funcName << " - TimeSet映射: " << name << " -> ID:" << id;
        }
    }
    else
    {
        qWarning() << funcName << " - 获取TimeSet映射失败: " << timesetQuery.lastError().text();
    }

    allRows.reserve(tableRowCount);
    for (int row = 0; row < tableRowCount; ++row)
    {
        // 为每一行创建包含所有数据库列的数据（包括隐藏列）
        Vector::RowData rowData;
        // rowData.resize(allColumns.size()); // QT6写法
        int targetSize1 = allColumns.size(); // QT5写法开始 (修正变量名)
        while (rowData.size() < targetSize1)
        {
            rowData.append(QVariant());
        }
        // QT5写法结束

        // 设置所有列的默认值
        for (int colIdx = 0; colIdx < allColumns.size(); ++colIdx)
        {
            const auto &col = allColumns[colIdx];
            if (col.type == Vector::ColumnDataType::PIN_STATE_ID)
            {
                rowData[colIdx] = "X"; // 管脚列默认为X
            }
            else
            {
                rowData[colIdx] = QVariant(); // 其他列使用默认空值
            }
        }

        // 从表格中读取可见列的实际值
        for (int tableCol = 0; tableCol < tableColCount; ++tableCol)
        {
            int visibleDbCol = tableColToVisibleDbColMap[tableCol];
            const auto &visibleColumn = visibleColumns[visibleDbCol];

            // 找到此可见列对应的原始列索引
            if (!columnIdToIndexMap.contains(visibleColumn.id))
            {
                qWarning() << funcName << " - 警告: 可见列ID" << visibleColumn.id
                           << "(" << visibleColumn.name << ")未在原始列映射中找到";
                continue;
            }

            int originalColIdx = columnIdToIndexMap[visibleColumn.id];

            // 处理单元格内容 - 检查是否为管脚列（单元格控件）
            if (visibleColumn.type == Vector::ColumnDataType::PIN_STATE_ID)
            {
                // 对于管脚列，先尝试从单元格控件获取值
                PinValueLineEdit *pinEdit = qobject_cast<PinValueLineEdit *>(tableWidget->cellWidget(row, tableCol));
                if (pinEdit)
                {
                    QString pinStateText = pinEdit->text();
                    if (pinStateText.isEmpty())
                    {
                        pinStateText = "X"; // 默认值
                    }
                    rowData[originalColIdx] = pinStateText;
                    qDebug() << funcName << " - 行" << row << "列" << tableCol << " 管脚状态(从编辑器): " << pinStateText;
                    continue; // 已处理，跳过后面的QTableWidgetItem检查
                }

                // 如果没有找到编辑控件，也尝试从QTableWidgetItem获取值
                // 这处理了一些非标准情况，比如用户直接修改了单元格而非通过编辑控件
                QTableWidgetItem *item = tableWidget->item(row, tableCol);
                if (item && !item->text().isEmpty())
                {
                    QString pinStateText = item->text();
                    rowData[originalColIdx] = pinStateText;
                    qDebug() << funcName << " - 行" << row << "列" << tableCol << " 管脚状态(从单元格): " << pinStateText;
                    continue;
                }
            }

            // 对于非管脚列或管脚列单元格控件不存在的情况，尝试从QTableWidgetItem获取值
            QTableWidgetItem *item = tableWidget->item(row, tableCol);
            if (!item)
            {
                qDebug() << funcName << " - 行" << row << "列" << tableCol << "项为空，设置为默认值";
                continue; // 使用默认值
            }

            // 根据列类型处理不同格式的数据
            QString cellText = item->text();
            if (visibleColumn.type == Vector::ColumnDataType::INSTRUCTION_ID)
            {
                // 将指令名称转换为ID存储
                if (instructionNameToIdMap.contains(cellText))
                {
                    int instructionId = instructionNameToIdMap[cellText];
                    rowData[originalColIdx] = instructionId;
                    qDebug() << funcName << " - 行" << row << "列" << tableCol << " 指令: " << cellText << " -> ID:" << instructionId;
                }
                else
                {
                    qWarning() << funcName << " - 行" << row << "列" << tableCol << " 未找到指令ID映射: " << cellText;
                    rowData[originalColIdx] = -1; // 默认未知ID
                }
            }
            else if (visibleColumn.type == Vector::ColumnDataType::TIMESET_ID)
            {
                // 将TimeSet名称转换为ID存储
                if (timesetNameToIdMap.contains(cellText))
                {
                    int timesetId = timesetNameToIdMap[cellText];
                    rowData[originalColIdx] = timesetId;
                    qDebug() << funcName << " - 行" << row << "列" << tableCol << " TimeSet: " << cellText << " -> ID:" << timesetId;
                }
                else
                {
                    qWarning() << funcName << " - 行" << row << "列" << tableCol << " 未找到TimeSet ID映射: " << cellText;
                    rowData[originalColIdx] = -1; // 默认未知ID
                }
            }
            else if (visibleColumn.type == Vector::ColumnDataType::BOOLEAN)
            {
                // 对于布尔值处理 (Capture)
                bool boolValue = rowData[originalColIdx].toBool();
                QString displayText = boolValue ? "Y" : "N";

                QTableWidgetItem *newItem = new QTableWidgetItem(displayText);
                newItem->setData(Qt::UserRole, boolValue);
                tableWidget->setItem(row, tableCol, newItem);
                qDebug() << funcName << " - 行" << row << ", 列" << tableCol
                         << " 设置布尔值为:" << displayText << "(" << boolValue << ")";
            }
            else
            {
                // 其他类型的列，直接创建QTableWidgetItem
                QTableWidgetItem *newItem = new QTableWidgetItem();
                QVariant originalValue = rowData[originalColIdx]; // 添加声明

                // 根据具体类型设置文本
                if (visibleColumn.type == Vector::ColumnDataType::INTEGER)
                {
                    newItem->setText(originalValue.isValid() ? QString::number(originalValue.toInt()) : "");
                    newItem->setData(Qt::UserRole, originalValue.isValid() ? originalValue.toInt() : 0);
                }
                else if (visibleColumn.type == Vector::ColumnDataType::REAL)
                {
                    newItem->setText(originalValue.isValid() ? QString::number(originalValue.toDouble()) : "");
                    newItem->setData(Qt::UserRole, originalValue.isValid() ? originalValue.toDouble() : 0.0);
                }
                else
                {
                    // TEXT 或其他类型，直接转换为字符串
                    newItem->setText(originalValue.toString());
                }

                tableWidget->setItem(row, tableCol, newItem);
                qDebug() << funcName << " - 行" << row << ", 列" << tableCol
                         << " 设置为:" << newItem->text();
            }
        }

        allRows.append(rowData);
    }

    // 5. 写入二进制文件 - 使用allColumns而不是visibleColumns
    qDebug() << funcName << " - 准备写入数据到二进制文件: " << absoluteBinFilePath;
    if (!Persistence::BinaryFileHelper::writeAllRowsToBinary(absoluteBinFilePath, allColumns, schemaVersion, allRows))
    {
        errorMessage = QString("写入二进制文件失败: %1").arg(absoluteBinFilePath);
        qWarning() << funcName << "-" << errorMessage;
        return false;
    }
    qDebug() << funcName << " - 二进制文件写入成功";

    // 6. 更新数据库中的行数记录
    if (!db.transaction())
    {
        errorMessage = "无法开始数据库事务以更新主记录。";
        qWarning() << funcName << "-" << errorMessage;
        return false;
    }

    // 计算最终的行数
    int finalRowCount = allRows.size();

    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE VectorTableMasterRecord SET row_count = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?");
    updateQuery.addBindValue(finalRowCount);
    updateQuery.addBindValue(tableId);

    if (!updateQuery.exec())
    {
        errorMessage = QString("更新数据库中的行数记录失败: %1").arg(updateQuery.lastError().text());
        qWarning() << funcName << "-" << errorMessage;
        db.rollback();
        return false;
    }

    if (!db.commit())
    {
        errorMessage = "提交数据库事务失败。";
        qWarning() << funcName << "-" << errorMessage;
        db.rollback();
        return false;
    }

    qDebug() << funcName << "- 数据库元数据行数已更新为:" << finalRowCount << " for table ID:" << tableId;
    return true;
}

// 实现标记行已修改的方法
void VectorDataHandler::markRowAsModified(int tableId, int rowIndex)
{
    const QString funcName = "VectorDataHandler::markRowAsModified";

    // 添加到修改行集合中
    m_modifiedRows[tableId].insert(rowIndex);
    qDebug() << funcName << " - 已标记表ID:" << tableId << "的行:" << rowIndex << "为已修改，当前修改行数:"
             << m_modifiedRows[tableId].size();
}
