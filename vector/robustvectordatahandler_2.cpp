
bool RobustVectorDataHandler::batchUpdateVectorColumn(
    int tableId, int columnIndex, const QMap<int, QVariant> &rowValueMap, QString &errorMessage)
{
    const QString funcName = "RobustVectorDataHandler::batchUpdateVectorColumn";

    // 1. 获取表元数据 (只查询一次)
    QString binFileName;
    QList<Vector::ColumnInfo> columns;
    int schemaVersion = 0;
    int totalRowCount = 0;

    if (!loadVectorTableMeta(tableId, binFileName, columns, schemaVersion, totalRowCount))
    {
        errorMessage = "无法加载表元数据";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 2. 检查列索引是否有效
    if (columnIndex < 0 || columnIndex >= columns.size())
    {
        errorMessage = QString("无效的列索引: %1").arg(columnIndex);
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 3. 获取二进制文件路径
    QString binFilePath = resolveBinaryFilePath(tableId, errorMessage);
    if (binFilePath.isEmpty())
    {
        qWarning() << funcName << " - 无法解析二进制文件路径";
        return false;
    }

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        errorMessage = "数据库未连接";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 4. 一次性查询所有需要修改的行的索引信息
    QMap<int, QPair<qint64, quint32>> rowOffsetMap; // 行索引 -> (偏移量, 大小)

    // 构建IN条件的行号列表
    QStringList logicalRowsList;
    for (auto it = rowValueMap.constBegin(); it != rowValueMap.constEnd(); ++it)
    {
        logicalRowsList << QString::number(it.key());
    }

    QString inClause = logicalRowsList.join(',');
    QSqlQuery indexQuery(db);
    indexQuery.prepare(
        "SELECT logical_row_order, offset, size FROM VectorTableRowIndex "
        "WHERE master_record_id = ? AND logical_row_order IN (" +
        inClause + ") AND is_active = 1");
    indexQuery.addBindValue(tableId);

    if (!indexQuery.exec())
    {
        errorMessage = "无法查询行索引信息: " + indexQuery.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    while (indexQuery.next())
    {
        int rowIndex = indexQuery.value(0).toInt();
        qint64 offset = indexQuery.value(1).toLongLong();
        quint32 size = indexQuery.value(2).toUInt();
        rowOffsetMap[rowIndex] = qMakePair(offset, size);
    }

    // 如果某些行不存在，记录警告但继续处理其他行
    if (rowOffsetMap.size() != rowValueMap.size())
    {
        qWarning() << funcName << "- 部分行未找到索引信息，请求行数:"
                   << rowValueMap.size() << "，找到行数:" << rowOffsetMap.size();
    }

    if (rowOffsetMap.isEmpty())
    {
        errorMessage = "未找到任何需要更新的行";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 5. 打开二进制文件进行读写
    QFile binFile(binFilePath);
    if (!binFile.open(QIODevice::ReadWrite))
    {
        errorMessage = "无法打开二进制文件进行读写: " + binFile.errorString();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 开启事务处理
    if (!db.transaction())
    {
        errorMessage = "无法开启数据库事务: " + db.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        binFile.close();
        return false;
    }

    bool success = true;
    int processedCount = 0;
    int successCount = 0;
    QSqlQuery updateIndexQuery(db);
    updateIndexQuery.prepare(
        "UPDATE VectorTableRowIndex SET offset = ?, size = ? "
        "WHERE master_record_id = ? AND logical_row_order = ?");

    const int BATCH_SIZE = 100; // 每批处理行数
    int totalRows = rowOffsetMap.size();

    // 为了提高性能，预先为文件末尾可能的追加操作分配空间
    qint64 appendOffset = binFile.size();

    for (auto it = rowOffsetMap.constBegin(); it != rowOffsetMap.constEnd(); ++it)
    {
        int rowIndex = it.key();
        qint64 oldOffset = it.value().first;
        quint32 oldSize = it.value().second;

        // 从文件读取行数据
        if (!binFile.seek(oldOffset))
        {
            qWarning() << funcName << "- 无法定位到行" << rowIndex << "的数据位置";
            continue;
        }

        QByteArray rowBytes = binFile.read(oldSize);
        if (rowBytes.size() != oldSize)
        {
            qWarning() << funcName << "- 读取行数据失败，行:" << rowIndex
                       << "，读取了" << rowBytes.size() << "字节，应为" << oldSize << "字节";
            continue;
        }

        // 反序列化行数据
        Vector::RowData rowData;
        if (!Persistence::BinaryFileHelper::deserializeRow(rowBytes, rowData))
        {
            qWarning() << funcName << "- 反序列化行数据失败，行:" << rowIndex;
            continue;
        }

        // 确保行数据足够长
        while (rowData.size() <= columnIndex)
        {
            rowData.append(QVariant());
        }

        // 更新指定列的值
        rowData[columnIndex] = rowValueMap[rowIndex];

        // 重新序列化行数据
        QByteArray newRowBytes;
        if (!Persistence::BinaryFileHelper::serializeRowSimple(rowData, newRowBytes))
        {
            qWarning() << funcName << "- 序列化修改后的行数据失败，行:" << rowIndex;
            continue;
        }

        qint64 newOffset;
        quint32 newSize = newRowBytes.size();

        // 策略：如果新数据不大于旧数据则原位更新，否则追加到文件末尾
        if (newSize <= oldSize)
        {
            // 原位更新
            newOffset = oldOffset;
            if (!binFile.seek(oldOffset))
            {
                qWarning() << funcName << "- 无法定位到行" << rowIndex << "的写入位置";
                continue;
            }

            qint64 bytesWritten = binFile.write(newRowBytes);
            if (bytesWritten != newSize)
            {
                qWarning() << funcName << "- 写入行数据失败，行:" << rowIndex;
                continue;
            }
            // 原位更新不需要修改索引表
        }
        else
        {
            // 追加到文件末尾
            newOffset = appendOffset;
            if (!binFile.seek(newOffset))
            {
                qWarning() << funcName << "- 无法定位到文件末尾";
                continue;
            }

            qint64 bytesWritten = binFile.write(newRowBytes);
            if (bytesWritten != newSize)
            {
                qWarning() << funcName << "- 写入行数据失败，行:" << rowIndex;
                continue;
            }
            appendOffset += newSize; // 更新下一行的追加位置

            // 更新索引表中的偏移量和大小
            updateIndexQuery.bindValue(0, newOffset);
            updateIndexQuery.bindValue(1, newSize);
            updateIndexQuery.bindValue(2, tableId);
            updateIndexQuery.bindValue(3, rowIndex);

            if (!updateIndexQuery.exec())
            {
                qWarning() << funcName << "- 更新行索引失败，行:" << rowIndex
                           << ", 错误:" << updateIndexQuery.lastError().text();
                continue;
            }
        }

        successCount++; // 成功更新计数
        processedCount++;

        // 每处理一批次行或处理完成时，输出进度日志
        if (processedCount % BATCH_SIZE == 0 || processedCount == totalRows)
        {
            int percentage = (processedCount * 100) / totalRows;
            qDebug() << funcName << "- 已处理" << processedCount << "行，共" << totalRows
                     << "行 (" << percentage << "%)";
            emit progressUpdated(percentage); // 发出进度信号
        }
    }

    // 6. 提交事务
    if (successCount > 0)
    {
        if (!db.commit())
        {
            errorMessage = "无法提交数据库事务: " + db.lastError().text();
            qWarning() << funcName << " - " << errorMessage;
            db.rollback();
            binFile.close();
            return false;
        }
    }
    else
    {
        db.rollback();
        errorMessage = "未成功更新任何行";
        qWarning() << funcName << " - " << errorMessage;
        binFile.close();
        return false;
    }

    binFile.close();

    qDebug() << funcName << "- 批量更新完成，成功更新" << successCount
             << "行，请求更新" << rowValueMap.size() << "行";

    return successCount > 0;
}

bool RobustVectorDataHandler::batchFillTimeSet(int tableId, const QList<int> &rowIndexes, int timeSetId, QString &errorMessage)
{
    const QString funcName = "RobustVectorDataHandler::batchFillTimeSet";

    // 记录详细的性能数据
    struct PerformanceMetrics
    {
        qint64 dbQueryTime = 0;
        qint64 readTime = 0;
        qint64 deserializeTime = 0;
        qint64 serializeTime = 0;
        qint64 writeTime = 0;
        qint64 indexUpdateTime = 0;
        qint64 totalTime = 0;
        int rowCount = 0;
        int bytesProcessed = 0;
    };

    QElapsedTimer timer, stageTimer, operationTimer;
    timer.start(); // 开始总计时

    PerformanceMetrics metrics;
    metrics.rowCount = rowIndexes.size();

    qDebug() << funcName << " - 开始批量填充TimeSet（批量追加-批量索引更新模式）";
    qDebug() << funcName << " - 表ID:" << tableId << "，行数:" << rowIndexes.size() << "，TimeSet ID:" << timeSetId;

    // 1. 获取表元数据 (只查询一次)
    QString binFileName;
    QList<Vector::ColumnInfo> columns;
    int schemaVersion = 0;
    int totalRowCount = 0;

    if (!loadVectorTableMeta(tableId, binFileName, columns, schemaVersion, totalRowCount))
    {
        errorMessage = "无法加载表元数据";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 2. 查找TimeSet列的索引
    int timesetColumnIndex = -1;
    for (int i = 0; i < columns.size(); ++i)
    {
        if (columns[i].type == Vector::ColumnDataType::TIMESET_ID)
        {
            timesetColumnIndex = i;
            break;
        }
    }

    if (timesetColumnIndex < 0)
    {
        errorMessage = "找不到TimeSet列";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 3. 获取二进制文件路径
    QString binFilePath = resolveBinaryFilePath(tableId, errorMessage);
    if (binFilePath.isEmpty())
    {
        qWarning() << funcName << " - 无法解析二进制文件路径";
        return false;
    }

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        errorMessage = "数据库未连接";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 4. 一次性查询所有需要修改的行的索引信息
    QMap<int, QPair<qint64, quint32>> rowOffsetMap; // 行索引 -> (偏移量, 大小)

    // 构建IN条件的行号列表
    QStringList logicalRowsList;
    foreach (int rowIndex, rowIndexes)
    {
        logicalRowsList << QString::number(rowIndex);
    }

    QString inClause = logicalRowsList.isEmpty() ? "" : " AND logical_row_order IN (" + logicalRowsList.join(',') + ")";

    stageTimer.start();
    QSqlQuery indexQuery(db);

    // 如果没有指定行，处理所有行
    if (rowIndexes.isEmpty())
    {
        indexQuery.prepare(
            "SELECT logical_row_order, offset, size FROM VectorTableRowIndex "
            "WHERE master_record_id = ? AND is_active = 1");
        indexQuery.addBindValue(tableId);
    }
    else
    {
        indexQuery.prepare(
            "SELECT logical_row_order, offset, size FROM VectorTableRowIndex "
            "WHERE master_record_id = ? AND is_active = 1" +
            inClause);
        indexQuery.addBindValue(tableId);
    }

    if (!indexQuery.exec())
    {
        errorMessage = "无法查询行索引信息: " + indexQuery.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    while (indexQuery.next())
    {
        int rowIdx = indexQuery.value(0).toInt();
        qint64 offset = indexQuery.value(1).toLongLong();
        quint32 size = indexQuery.value(2).toUInt();
        rowOffsetMap[rowIdx] = qMakePair(offset, size);
    }

    metrics.dbQueryTime = stageTimer.elapsed();

    if (rowOffsetMap.isEmpty())
    {
        errorMessage = "未找到需要修改的行";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    qDebug() << funcName << " - 找到" << rowOffsetMap.size() << "行需要填充TimeSet";

    // 5. 打开二进制文件
    QFile binFile(binFilePath);
    if (!binFile.open(QIODevice::ReadWrite))
    {
        errorMessage = "无法打开二进制文件: " + binFile.errorString();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 6. 开始数据库事务
    if (!db.transaction())
    {
        errorMessage = "无法开启数据库事务: " + db.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        binFile.close();
        return false;
    }

    // 阶段1：批量读取和反序列化
    // 创建行数据映射
    QMap<int, Vector::RowData> rowDataMap;

    stageTimer.restart();
    int processedCount = 0;

    // 批量读取所有行数据
    for (auto it = rowOffsetMap.begin(); it != rowOffsetMap.end(); ++it)
    {
        int rowIndex = it.key();
        qint64 oldOffset = it.value().first;
        quint32 oldSize = it.value().second;

        // 从文件读取行数据
        operationTimer.restart();
        if (!binFile.seek(oldOffset))
        {
            qWarning() << funcName << "- 无法定位到行" << rowIndex << "的数据位置";
            continue;
        }

        QByteArray rowBytes = binFile.read(oldSize);
        metrics.readTime += operationTimer.elapsed();

        if (rowBytes.size() != oldSize)
        {
            qWarning() << funcName << "- 读取行数据失败，行:" << rowIndex
                       << "，读取了" << rowBytes.size() << "字节，应为" << oldSize << "字节";
            continue;
        }

        metrics.bytesProcessed += rowBytes.size();

        // 反序列化行数据
        Vector::RowData rowData;
        operationTimer.restart();
        if (!Persistence::BinaryFileHelper::deserializeRow(rowBytes, rowData))
        {
            qWarning() << funcName << "- 反序列化行数据失败，行:" << rowIndex;
            continue;
        }
        metrics.deserializeTime += operationTimer.elapsed();

        // 确保行数据足够长
        while (rowData.size() <= timesetColumnIndex)
        {
            rowData.append(QVariant());
        }

        // 更新TimeSet列的值
        QVariant timeSetValue(timeSetId);
        rowData[timesetColumnIndex] = timeSetValue;

        // 添加到映射中
        rowDataMap[rowIndex] = rowData;

        // 更新进度信息
        processedCount++;
        if (processedCount % 1000 == 0 || processedCount == rowOffsetMap.size())
        {
            int progress = (processedCount * 40) / rowOffsetMap.size(); // 读取阶段占40%
            emit progressUpdated(progress);
        }
    }

    qDebug() << funcName << " - [性能指标] 读取数据: " << metrics.readTime << "ms, 反序列化: " << metrics.deserializeTime << "ms";

    // 阶段2：批量追加阶段
    stageTimer.restart();

    // 定位到文件末尾准备追加
    qint64 appendOffset = binFile.size();
    binFile.seek(appendOffset);

    // 批量序列化和追加操作，并收集索引更新信息
    struct IndexUpdateInfo
    {
        qint64 newOffset;
        quint32 newSize;
    };
    QHash<int, IndexUpdateInfo> indexUpdates; // 行索引 -> 新索引信息
    indexUpdates.reserve(rowDataMap.size());  // 预分配容量

    int totalRows = rowDataMap.size();
    processedCount = 0;

    for (auto it = rowDataMap.begin(); it != rowDataMap.end(); ++it)
    {
        int rowIndex = it.key();
        const Vector::RowData &rowData = it.value();

        // 序列化行数据
        operationTimer.restart();
        QByteArray newRowBytes;
        if (!Persistence::BinaryFileHelper::serializeRowSimple(rowData, newRowBytes))
        {
            qWarning() << funcName << "- 序列化行数据失败，行:" << rowIndex;
            continue;
        }
        metrics.serializeTime += operationTimer.elapsed();

        // 追加到文件末尾
        qint64 currentOffset = binFile.pos();

        operationTimer.restart();
        qint64 bytesWritten = binFile.write(newRowBytes);
        metrics.writeTime += operationTimer.elapsed();

        if (bytesWritten != newRowBytes.size())
        {
            qWarning() << funcName << "- 写入行数据失败，行:" << rowIndex;
            continue;
        }

        // 记录新的偏移量和大小，稍后批量更新索引
        IndexUpdateInfo info;
        info.newOffset = currentOffset;
        info.newSize = newRowBytes.size();
        indexUpdates[rowIndex] = info;

        // 更新进度
        processedCount++;
        if (processedCount % 1000 == 0 || processedCount == totalRows)
        {
            int progress = 40 + (processedCount * 40) / totalRows; // 写入阶段占40%
            emit progressUpdated(progress);
        }
    }

    qDebug() << funcName << " - [性能指标] 序列化: " << metrics.serializeTime << "ms, 写入: " << metrics.writeTime << "ms";

    // 阶段3：批量索引更新
    stageTimer.restart();

    // 批量更新索引
    int batchSize = 1000;
    QStringList batchIds;
    int successCount = 0;
    int indexUpdateBatches = 0;
    int currentBatch = 0;

    for (auto it = indexUpdates.constBegin(); it != indexUpdates.constEnd(); ++it)
    {
        int rowIndex = it.key();
        const IndexUpdateInfo &info = it.value();

        // 更新索引信息
        QSqlQuery updateIndexQuery(db);
        updateIndexQuery.prepare(
            "UPDATE VectorTableRowIndex SET offset = ?, size = ? "
            "WHERE master_record_id = ? AND logical_row_order = ?");
        updateIndexQuery.addBindValue(info.newOffset);
        updateIndexQuery.addBindValue(info.newSize);
        updateIndexQuery.addBindValue(tableId);
        updateIndexQuery.addBindValue(rowIndex);

        if (!updateIndexQuery.exec())
        {
            qWarning() << funcName << "- 更新行索引失败，行:" << rowIndex
                       << "，错误:" << updateIndexQuery.lastError().text();
            continue;
        }

        successCount++;

        // 更新进度
        if (successCount % 1000 == 0 || successCount == indexUpdates.size())
        {
            int progress = 80 + (successCount * 20) / indexUpdates.size(); // 索引更新阶段占20%
            emit progressUpdated(progress);
        }
    }

    metrics.indexUpdateTime = stageTimer.elapsed();
    metrics.totalTime = timer.elapsed();

    // 提交事务
    if (successCount > 0)
    {
        if (!db.commit())
        {
            errorMessage = "无法提交数据库事务: " + db.lastError().text();
            qWarning() << funcName << " - " << errorMessage;
            db.rollback();
            binFile.close();
            return false;
        }
    }
    else
    {
        db.rollback();
        errorMessage = "未成功更新任何行";
        qWarning() << funcName << " - " << errorMessage;
        binFile.close();
        return false;
    }

    binFile.close();

    qDebug() << funcName << "- 批量填充TimeSet(批量追加模式)完成，成功更新" << successCount
             << "行，请求更新" << rowIndexes.size() << "行，总耗时:" << metrics.totalTime << "ms";
    qDebug() << funcName << "- 性能分析: 数据库查询=" << metrics.dbQueryTime
             << "ms, 读取=" << metrics.readTime
             << "ms, 反序列化=" << metrics.deserializeTime
             << "ms, 序列化=" << metrics.serializeTime
             << "ms, 写入=" << metrics.writeTime
             << "ms, 索引更新=" << metrics.indexUpdateTime
             << "ms, 总字节=" << metrics.bytesProcessed;

    // 完成进度
    emit progressUpdated(100);

    return successCount > 0;
}

bool RobustVectorDataHandler::batchReplaceTimeSet(int tableId, int fromTimeSetId, int toTimeSetId, const QList<int> &rowIndexes, QString &errorMessage)
{
    const QString funcName = "RobustVectorDataHandler::batchReplaceTimeSet";

    // 记录详细的性能数据
    struct PerformanceMetrics
    {
        qint64 dbQueryTime = 0;
        qint64 readTime = 0;
        qint64 deserializeTime = 0;
        qint64 serializeTime = 0;
        qint64 writeTime = 0;
        qint64 indexUpdateTime = 0;
        qint64 totalTime = 0;
        int rowCount = 0;
        int bytesProcessed = 0;
        int matchedRows = 0;
    };

    QElapsedTimer timer, stageTimer, operationTimer;
    timer.start(); // 开始总计时

    PerformanceMetrics metrics;
    metrics.rowCount = rowIndexes.size();

    qDebug() << funcName << " - 开始批量替换TimeSet（批量追加-批量索引更新模式）";
    qDebug() << funcName << " - 表ID:" << tableId << "，行数:" << rowIndexes.size()
             << "，从TimeSet ID:" << fromTimeSetId << " 到TimeSet ID:" << toTimeSetId;

    // 1. 获取表元数据 (只查询一次)
    QString binFileName;
    QList<Vector::ColumnInfo> columns;
    int schemaVersion = 0;
    int totalRowCount = 0;

    if (!loadVectorTableMeta(tableId, binFileName, columns, schemaVersion, totalRowCount))
    {
        errorMessage = "无法加载表元数据";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 2. 查找TimeSet列的索引
    int timesetColumnIndex = -1;
    for (int i = 0; i < columns.size(); ++i)
    {
        if (columns[i].type == Vector::ColumnDataType::TIMESET_ID)
        {
            timesetColumnIndex = i;
            break;
        }
    }

    if (timesetColumnIndex < 0)
    {
        errorMessage = "找不到TimeSet列";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 3. 获取二进制文件路径
    QString binFilePath = resolveBinaryFilePath(tableId, errorMessage);
    if (binFilePath.isEmpty())
    {
        qWarning() << funcName << " - 无法解析二进制文件路径";
        return false;
    }

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        errorMessage = "数据库未连接";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 4. 查询需要处理的行信息
    QMap<int, QPair<qint64, quint32>> rowOffsetMap; // 行索引 -> (偏移量, 大小)

    stageTimer.start();
    QSqlQuery indexQuery(db);

    // 如果指定了行范围，则只处理指定的行
    if (!rowIndexes.isEmpty())
    {
        // 构建IN子句
        QStringList rowIndicesStr;
        foreach (int row, rowIndexes)
        {
            rowIndicesStr.append(QString::number(row));
        }

        QString inClause = " AND logical_row_order IN (" + rowIndicesStr.join(',') + ")";

        indexQuery.prepare(
            "SELECT logical_row_order, offset, size FROM VectorTableRowIndex "
            "WHERE master_record_id = ? AND is_active = 1" +
            inClause);
        indexQuery.addBindValue(tableId);
    }
    else
    {
        // 处理所有行
        indexQuery.prepare(
            "SELECT logical_row_order, offset, size FROM VectorTableRowIndex "
            "WHERE master_record_id = ? AND is_active = 1");
        indexQuery.addBindValue(tableId);
    }

    if (!indexQuery.exec())
    {
        errorMessage = "无法查询行索引信息: " + indexQuery.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    while (indexQuery.next())
    {
        int rowIdx = indexQuery.value(0).toInt();
        qint64 offset = indexQuery.value(1).toLongLong();
        quint32 size = indexQuery.value(2).toUInt();
        rowOffsetMap[rowIdx] = qMakePair(offset, size);
    }

    metrics.dbQueryTime = stageTimer.elapsed();

    if (rowOffsetMap.isEmpty())
    {
        errorMessage = "未找到需要修改的行";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    qDebug() << funcName << " - 找到" << rowOffsetMap.size() << "行需要检查替换TimeSet";

    // 5. 打开二进制文件
    QFile binFile(binFilePath);
    if (!binFile.open(QIODevice::ReadWrite))
    {
        errorMessage = "无法打开二进制文件: " + binFile.errorString();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 6. 开始数据库事务
    if (!db.transaction())
    {
        errorMessage = "无法开启数据库事务: " + db.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        binFile.close();
        return false;
    }

    // 阶段1：批量读取和筛选需要更改的行
    QMap<int, Vector::RowData> rowDataMap; // 存储需要更新的行数据
    QVariant fromTimeSetValue(fromTimeSetId);
    QVariant toTimeSetValue(toTimeSetId);

    stageTimer.restart();
    int processedCount = 0;

    // 批量读取所有行数据并过滤出需要更新的
    for (auto it = rowOffsetMap.begin(); it != rowOffsetMap.end(); ++it)
    {
        int rowIndex = it.key();
        qint64 oldOffset = it.value().first;
        quint32 oldSize = it.value().second;

        // 从文件读取行数据
        operationTimer.restart();
        if (!binFile.seek(oldOffset))
        {
            qWarning() << funcName << "- 无法定位到行" << rowIndex << "的数据位置";
            continue;
        }

        QByteArray rowBytes = binFile.read(oldSize);
        metrics.readTime += operationTimer.elapsed();

        if (rowBytes.size() != oldSize)
        {
            qWarning() << funcName << "- 读取行数据失败，行:" << rowIndex
                       << "，读取了" << rowBytes.size() << "字节，应为" << oldSize << "字节";
            continue;
        }

        metrics.bytesProcessed += rowBytes.size();

        // 反序列化行数据
        Vector::RowData rowData;
        operationTimer.restart();
        if (!Persistence::BinaryFileHelper::deserializeRow(rowBytes, rowData))
        {
            qWarning() << funcName << "- 反序列化行数据失败，行:" << rowIndex;
            continue;
        }
        metrics.deserializeTime += operationTimer.elapsed();

        // 确保行数据足够长
        if (rowData.size() <= timesetColumnIndex)
        {
            continue; // 跳过没有TimeSet列的行
        }

        // 检查是否匹配要替换的TimeSet值
        if (rowData[timesetColumnIndex] == fromTimeSetValue)
        {
            // 更新TimeSet列的值
            rowData[timesetColumnIndex] = toTimeSetValue;

            // 添加到需要更新的映射中
            rowDataMap[rowIndex] = rowData;
            metrics.matchedRows++;
        }

        // 更新进度信息
        processedCount++;
        if (processedCount % 1000 == 0 || processedCount == rowOffsetMap.size())
        {
            int progress = (processedCount * 40) / rowOffsetMap.size(); // 读取阶段占40%
            emit progressUpdated(progress);
        }
    }

    qDebug() << funcName << " - [性能指标] 读取数据: " << metrics.readTime << "ms, 反序列化: "
             << metrics.deserializeTime << "ms, 匹配行数: " << metrics.matchedRows;

    // 如果没有需要更新的行，提前结束
    if (rowDataMap.isEmpty())
    {
        db.rollback();
        errorMessage = "未找到匹配的行需要替换TimeSet";
        qWarning() << funcName << " - " << errorMessage;
        binFile.close();
        return false;
    }

    // 阶段2：批量追加阶段
    stageTimer.restart();

    // 定位到文件末尾准备追加
    qint64 appendOffset = binFile.size();
    binFile.seek(appendOffset);

    // 批量序列化和追加操作，并收集索引更新信息
    struct IndexUpdateInfo
    {
        qint64 newOffset;
        quint32 newSize;
    };
    QHash<int, IndexUpdateInfo> indexUpdates; // 行索引 -> 新索引信息
    indexUpdates.reserve(rowDataMap.size());  // 预分配容量

    int totalRows = rowDataMap.size();
    processedCount = 0;

    for (auto it = rowDataMap.begin(); it != rowDataMap.end(); ++it)
    {
        int rowIndex = it.key();
        const Vector::RowData &rowData = it.value();

        // 序列化行数据
        operationTimer.restart();
        QByteArray newRowBytes;
        if (!Persistence::BinaryFileHelper::serializeRowSimple(rowData, newRowBytes))
        {
            qWarning() << funcName << "- 序列化行数据失败，行:" << rowIndex;
            continue;
        }
        metrics.serializeTime += operationTimer.elapsed();

        // 追加到文件末尾
        qint64 currentOffset = binFile.pos();

        operationTimer.restart();
        qint64 bytesWritten = binFile.write(newRowBytes);
        metrics.writeTime += operationTimer.elapsed();

        if (bytesWritten != newRowBytes.size())
        {
            qWarning() << funcName << "- 写入行数据失败，行:" << rowIndex;
            continue;
        }

        // 记录新的偏移量和大小，稍后批量更新索引
        IndexUpdateInfo info;
        info.newOffset = currentOffset;
        info.newSize = newRowBytes.size();
        indexUpdates[rowIndex] = info;

        // 更新进度
        processedCount++;
        if (processedCount % 1000 == 0 || processedCount == totalRows)
        {
            int progress = 40 + (processedCount * 40) / totalRows; // 写入阶段占40%
            emit progressUpdated(progress);
        }
    }

    qDebug() << funcName << " - [性能指标] 序列化: " << metrics.serializeTime << "ms, 写入: " << metrics.writeTime << "ms";

    // 阶段3：批量索引更新
    stageTimer.restart();

    // 批量更新索引
    int successCount = 0;

    for (auto it = indexUpdates.constBegin(); it != indexUpdates.constEnd(); ++it)
    {
        int rowIndex = it.key();
        const IndexUpdateInfo &info = it.value();

        // 更新索引信息
        QSqlQuery updateIndexQuery(db);
        updateIndexQuery.prepare(
            "UPDATE VectorTableRowIndex SET offset = ?, size = ? "
            "WHERE master_record_id = ? AND logical_row_order = ?");
        updateIndexQuery.addBindValue(info.newOffset);
        updateIndexQuery.addBindValue(info.newSize);
        updateIndexQuery.addBindValue(tableId);
        updateIndexQuery.addBindValue(rowIndex);

        if (!updateIndexQuery.exec())
        {
            qWarning() << funcName << "- 更新行索引失败，行:" << rowIndex
                       << "，错误:" << updateIndexQuery.lastError().text();
            continue;
        }

        successCount++;

        // 更新进度
        if (successCount % 1000 == 0 || successCount == indexUpdates.size())
        {
            int progress = 80 + (successCount * 20) / indexUpdates.size(); // 索引更新阶段占20%
            emit progressUpdated(progress);
        }
    }

    metrics.indexUpdateTime = stageTimer.elapsed();
    metrics.totalTime = timer.elapsed();

    // 提交事务
    if (successCount > 0)
    {
        if (!db.commit())
        {
            errorMessage = "无法提交数据库事务: " + db.lastError().text();
            qWarning() << funcName << " - " << errorMessage;
            db.rollback();
            binFile.close();
            return false;
        }
    }
    else
    {
        db.rollback();
        errorMessage = "未成功更新任何行";
        qWarning() << funcName << " - " << errorMessage;
        binFile.close();
        return false;
    }

    binFile.close();

    qDebug() << funcName << "- 批量替换TimeSet(批量追加模式)完成，成功更新" << successCount
             << "行，匹配行数:" << metrics.matchedRows << "，总耗时:" << metrics.totalTime << "ms";
    qDebug() << funcName << "- 性能分析: 数据库查询=" << metrics.dbQueryTime
             << "ms, 读取=" << metrics.readTime
             << "ms, 反序列化=" << metrics.deserializeTime
             << "ms, 序列化=" << metrics.serializeTime
             << "ms, 写入=" << metrics.writeTime
             << "ms, 索引更新=" << metrics.indexUpdateTime
             << "ms, 总字节=" << metrics.bytesProcessed;

    // 完成进度
    emit progressUpdated(100);

    return successCount > 0;
}
