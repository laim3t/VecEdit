bool RobustVectorDataHandler::batchUpdateVectorColumnOptimized(
    int tableId, int columnIndex, const QMap<int, QVariant> &rowValueMap, QString &errorMessage)
{
    const QString funcName = "RobustVectorDataHandler::batchUpdateVectorColumnOptimized";

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

    QElapsedTimer timer;
    timer.start(); // 开始计时

    PerformanceMetrics metrics;
    metrics.rowCount = rowValueMap.size();

    qDebug() << funcName << " - 开始批量更新向量列数据（批量追加-批量索引更新模式）";
    qDebug() << funcName << " - 表ID:" << tableId << "，列索引:" << columnIndex << "，行数:" << rowValueMap.size();

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

    // 数据库查询时间记录
    QElapsedTimer queryTimer;
    queryTimer.start();

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

    metrics.dbQueryTime = queryTimer.elapsed();

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

    qDebug() << funcName << " - 查询索引信息完成，耗时: " << metrics.dbQueryTime << "ms";

    // 开启事务处理
    if (!db.transaction())
    {
        errorMessage = "无法开启数据库事务: " + db.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 5. 打开二进制文件进行读写
    QFile binFile(binFilePath);
    if (!binFile.open(QIODevice::ReadWrite))
    {
        errorMessage = "无法打开二进制文件进行读写: " + binFile.errorString();
        qWarning() << funcName << " - " << errorMessage;
        db.rollback();
        return false;
    }

    // 优化核心：批量追加-批量索引更新

    // 阶段1：准备阶段
    QElapsedTimer stageTimer;
    stageTimer.start();
    QElapsedTimer operationTimer;

    // 在内存中预读所有需要的行数据
    QHash<int, Vector::RowData> rowDataMap; // 行索引 -> 行数据
    // 移除不必要的中间存储，减少内存使用
    // QHash<int, QByteArray> oldRowBytesMap;  // 行索引 -> 原始二进制数据

    // 预分配容量，减少重新分配次数
    rowDataMap.reserve(rowOffsetMap.size());
    // oldRowBytesMap.reserve(rowOffsetMap.size());

    // 一次性读取所有行数据
    for (auto it = rowOffsetMap.constBegin(); it != rowOffsetMap.constEnd(); ++it)
    {
        int rowIndex = it.key();
        qint64 oldOffset = it.value().first;
        quint32 oldSize = it.value().second;

        // 定位到行位置并读取数据
        operationTimer.restart();
        if (!binFile.seek(oldOffset))
        {
            qWarning() << funcName << "- 无法定位到行" << rowIndex << "的数据位置";
            continue;
        }

        QByteArray rowBytes = binFile.read(oldSize);
        metrics.readTime += operationTimer.elapsed();
        metrics.bytesProcessed += rowBytes.size();

        if (rowBytes.size() != oldSize)
        {
            qWarning() << funcName << "- 读取行数据失败，行:" << rowIndex
                       << "，读取了" << rowBytes.size() << "字节，应为" << oldSize << "字节";
            continue;
        }

        // 移除不必要的存储
        // oldRowBytesMap[rowIndex] = rowBytes;

        // 反序列化行数据
        operationTimer.restart();
        Vector::RowData rowData;
        if (!Persistence::BinaryFileHelper::deserializeRow(rowBytes, rowData))
        {
            qWarning() << funcName << "- 反序列化行数据失败，行:" << rowIndex;
            continue;
        }
        metrics.deserializeTime += operationTimer.elapsed();

        // 确保行数据足够长
        while (rowData.size() <= columnIndex)
        {
            rowData.append(QVariant());
        }

        // 更新列值
        rowData[columnIndex] = rowValueMap[rowIndex];

        // 保存修改后的行数据到映射中，使用移动语义避免复制
        rowDataMap[rowIndex] = std::move(rowData);
    }

    qint64 stageTime = stageTimer.elapsed();
    qDebug() << funcName << " - 准备阶段完成，耗时: " << stageTime << "ms";
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
    int processedCount = 0;

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
            int percentage = (processedCount * 100) / totalRows;
            qDebug() << funcName << "- 批量追加阶段: 已处理" << processedCount << "行，共" << totalRows
                     << "行 (" << percentage << "%)";
            emit progressUpdated(percentage / 2); // 前50%进度
        }
    }

    stageTime = stageTimer.elapsed();
    qDebug() << funcName << " - 批量追加阶段完成，耗时: " << stageTime << "ms";
    qDebug() << funcName << " - [性能指标] 序列化: " << metrics.serializeTime << "ms, 写入数据: " << metrics.writeTime << "ms";

    // 阶段3：批量索引更新阶段
    stageTimer.restart();

    // 使用批量绑定方式进行索引更新
    const int BATCH_SIZE = 1000; // 更大的批次大小
    QSqlQuery updateBatchQuery(db);
    updateBatchQuery.prepare(
        "UPDATE VectorTableRowIndex SET offset = ?, size = ? "
        "WHERE master_record_id = ? AND logical_row_order = ?");

    QVariantList offsetParams;
    QVariantList sizeParams;
    QVariantList tableIdParams;
    QVariantList rowIndexParams;

    processedCount = 0;
    int batchCount = 0;

    for (auto it = indexUpdates.begin(); it != indexUpdates.end(); ++it)
    {
        int rowIndex = it.key();
        const IndexUpdateInfo &info = it.value();

        // 添加参数到批处理列表
        offsetParams.append(info.newOffset);
        sizeParams.append(info.newSize);
        tableIdParams.append(tableId);
        rowIndexParams.append(rowIndex);

        processedCount++;
        batchCount++;

        // 当达到批量大小或处理完所有行时执行批量更新
        if (batchCount >= BATCH_SIZE || processedCount == totalRows)
        {
            // 绑定参数
            updateBatchQuery.addBindValue(offsetParams);
            updateBatchQuery.addBindValue(sizeParams);
            updateBatchQuery.addBindValue(tableIdParams);
            updateBatchQuery.addBindValue(rowIndexParams);

            // 执行批量更新
            operationTimer.restart();
            if (!updateBatchQuery.execBatch())
            {
                qWarning() << funcName << "- 批量更新索引失败: " << updateBatchQuery.lastError().text();
                errorMessage = "批量更新索引失败: " + updateBatchQuery.lastError().text();
                db.rollback();
                binFile.close();
                return false;
            }
            metrics.indexUpdateTime += operationTimer.elapsed();

            // 清空参数列表，准备下一批
            offsetParams.clear();
            sizeParams.clear();
            tableIdParams.clear();
            rowIndexParams.clear();
            batchCount = 0;

            // 更新进度
            int percentage = 50 + (processedCount * 50) / totalRows; // 后50%进度
            qDebug() << funcName << "- 批量索引更新阶段: 已处理" << processedCount << "行，共" << totalRows
                     << "行 (" << percentage << "%)";
            emit progressUpdated(percentage);
        }
    }

    stageTime = stageTimer.elapsed();
    qDebug() << funcName << " - 批量索引更新阶段完成，耗时: " << stageTime << "ms";
    qDebug() << funcName << " - [性能指标] 索引更新: " << metrics.indexUpdateTime << "ms";

    // 提交事务
    if (!db.commit())
    {
        errorMessage = "无法提交数据库事务: " + db.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        db.rollback();
        binFile.close();
        return false;
    }

    // 关闭文件
    binFile.close();

    // 计算总耗时和详细性能分析
    metrics.totalTime = timer.elapsed();

    // 输出总耗时和详细的性能报告
    qDebug() << funcName << " - 批量更新完成，总耗时: " << metrics.totalTime << "ms";
    qDebug() << funcName << " - 成功更新 " << indexUpdates.size() << " 行数据";
    qDebug() << funcName << " - 处理字节总量: " << metrics.bytesProcessed << " bytes";

    // 各阶段详细性能分析
    qDebug() << funcName << " - [性能分析]";
    qDebug() << funcName << " - 数据库查询: " << metrics.dbQueryTime << "ms (" << (metrics.dbQueryTime * 100.0 / metrics.totalTime) << "%)";
    qDebug() << funcName << " - 数据读取: " << metrics.readTime << "ms (" << (metrics.readTime * 100.0 / metrics.totalTime) << "%)";
    qDebug() << funcName << " - 反序列化: " << metrics.deserializeTime << "ms (" << (metrics.deserializeTime * 100.0 / metrics.totalTime) << "%)";
    qDebug() << funcName << " - 序列化: " << metrics.serializeTime << "ms (" << (metrics.serializeTime * 100.0 / metrics.totalTime) << "%)";
    qDebug() << funcName << " - 数据写入: " << metrics.writeTime << "ms (" << (metrics.writeTime * 100.0 / metrics.totalTime) << "%)";
    qDebug() << funcName << " - 索引更新: " << metrics.indexUpdateTime << "ms (" << (metrics.indexUpdateTime * 100.0 / metrics.totalTime) << "%)";
    qDebug() << funcName << " - 其他操作: " << (metrics.totalTime - metrics.dbQueryTime - metrics.readTime - metrics.deserializeTime - metrics.serializeTime - metrics.writeTime - metrics.indexUpdateTime) << "ms";
    qDebug() << funcName << " - 平均每行耗时: " << (metrics.totalTime / (double)indexUpdates.size()) << "ms";

    return !indexUpdates.isEmpty();
}
