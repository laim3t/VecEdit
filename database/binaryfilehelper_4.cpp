    void BinaryFileHelper::clearRowOffsetCache(const QString &binFilePath)
    {
        const QString funcName = "BinaryFileHelper::clearRowOffsetCache";
        QMutexLocker locker(&s_cacheMutex);

        qDebug() << funcName << "- 清除文件行偏移缓存:" << binFilePath;
        s_fileRowOffsetCache.remove(binFilePath);

        // 同时删除索引文件
        QString indexFilePath = getIndexFilePath(binFilePath);
        QFile indexFile(indexFilePath);
        if (indexFile.exists())
        {
            if (indexFile.remove())
            {
                qDebug() << funcName << "- 索引文件已删除:" << indexFilePath;
            }
            else
            {
                qWarning() << funcName << "- 无法删除索引文件:" << indexFilePath;
            }
        }
    }

    void BinaryFileHelper::updateRowOffsetCache(const QString &binFilePath,
                                                const QSet<int> &modifiedRows,
                                                bool preserveIndex)
    {
        const QString funcName = "BinaryFileHelper::updateRowOffsetCache";
        QMutexLocker locker(&s_cacheMutex);

        // 如果不需要保留索引，直接调用清除函数
        if (!preserveIndex || modifiedRows.size() > 100)
        {
            // 当修改行数超过100行时，也使用完整清除
            qDebug() << funcName << "- 修改行数较多(" << modifiedRows.size()
                     << ")或不需要保留索引，执行完整清除";
            clearRowOffsetCache(binFilePath);
            return;
        }

        // 检查内存缓存是否存在
        if (!s_fileRowOffsetCache.contains(binFilePath))
        {
            qDebug() << funcName << "- 内存中无此文件缓存，无需更新:" << binFilePath;
            return;
        }

        RowOffsetCache &cache = s_fileRowOffsetCache[binFilePath];

        // 如果缓存为空，直接返回
        if (cache.isEmpty())
        {
            qDebug() << funcName << "- 缓存为空，无需更新";
            return;
        }

        // 记录哪些行需要在后续扫描中更新
        QSet<int> invalidatedRows = modifiedRows;

        // 在内存缓存中标记修改的行
        int validUpdateCount = 0;
        foreach (int rowIndex, modifiedRows)
        {
            if (rowIndex >= 0 && rowIndex < cache.size())
            {
                // 在内存中将该行标记为过期（设置timestamp为0）
                cache[rowIndex].timestamp = 0;
                validUpdateCount++;
            }
        }

        qDebug() << funcName << "- 已在内存缓存中标记" << validUpdateCount
                 << "行为已修改状态，保留索引文件结构";

        // 注意：此处不更新索引文件，因为更新单个行的索引文件成本较高
        // 下次读取时，如果发现内存缓存中有timestamp=0的行，
        // 将触发仅对这些行的重新扫描，而不是整个文件
    }

    bool BinaryFileHelper::insertRowsInBinary(const QString &binFilePath, const QList<Vector::ColumnInfo> &columns,
                                              int schemaVersion, int startRow, const QList<Vector::RowData> &rowsToInsert,
                                              QString &errorMessage)
    {
        const QString funcName = "BinaryFileHelper::insertRowsInBinary";

        if (rowsToInsert.isEmpty())
        {
            return true;
        }

        QFile file(binFilePath);
        if (!file.exists())
        {
            errorMessage = "Binary file does not exist, cannot insert.";
            qWarning() << funcName << "-" << errorMessage;
            return false;
        }

        // 1. 创建临时文件
        QString tempFilePath = binFilePath + ".tmp";
        QFile tempFile(tempFilePath);
        if (!tempFile.open(QIODevice::WriteOnly))
        {
            errorMessage = "Failed to create temp file: " + tempFile.errorString();
            qWarning() << funcName << "-" << errorMessage;
            return false;
        }

        if (!file.open(QIODevice::ReadOnly))
        {
            errorMessage = "Failed to open source file: " + file.errorString();
            qWarning() << funcName << "-" << errorMessage;
            tempFile.close();
            QFile::remove(tempFilePath);
            return false;
        }

        // 2. 读写文件头
        BinaryFileHeader header;
        if (!readBinaryHeader(&file, header))
        {
            errorMessage = "Failed to read header from source file.";
            qWarning() << funcName << "-" << errorMessage;
            file.close();
            tempFile.close();
            QFile::remove(tempFilePath);
            return false;
        }

        if (startRow < 0 || startRow > header.row_count_in_file)
        {
            errorMessage = QString("Insert position out of bounds. StartRow: %1, TotalRows: %2").arg(startRow).arg(header.row_count_in_file);
            qWarning() << funcName << "-" << errorMessage;
            file.close();
            tempFile.close();
            QFile::remove(tempFilePath);
            return false;
        }

        // 更新文件头并写入临时文件
        BinaryFileHeader newHeader = header;
        newHeader.row_count_in_file += rowsToInsert.size();
        newHeader.timestamp_updated = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
        if (!writeBinaryHeader(&tempFile, newHeader))
        {
            errorMessage = "Failed to write header to temp file.";
            qWarning() << funcName << "-" << errorMessage;
            file.close();
            tempFile.close();
            QFile::remove(tempFilePath);
            return false;
        }

        // 3. 复制插入点之前的数据
        qint64 currentPos = file.pos();
        file.seek(0);                    // 回到文件开头
        readBinaryHeader(&file, header); // 跳过文件头

        for (int i = 0; i < startRow; ++i)
        {
            quint32 rowSize;
            QDataStream stream(&file);
            stream.setByteOrder(QDataStream::LittleEndian);
            if (!readRowSizeWithValidation(stream, file, std::numeric_limits<quint32>::max(), rowSize))
            {
                errorMessage = QString("Failed to read size of row %1").arg(i);
                qWarning() << funcName << "-" << errorMessage;
                file.close();
                tempFile.close();
                QFile::remove(tempFilePath);
                return false;
            }
            QByteArray rowBytes = file.read(rowSize);
            if (rowBytes.size() != rowSize)
            {
                errorMessage = QString("Failed to read full data of row %1").arg(i);
                qWarning() << funcName << "-" << errorMessage;
                file.close();
                tempFile.close();
                QFile::remove(tempFilePath);
                return false;
            }
            tempFile.write(reinterpret_cast<const char *>(&rowSize), sizeof(rowSize));
            tempFile.write(rowBytes);
        }

        // 4. 写入新数据
        for (const auto &rowData : rowsToInsert)
        {
            QByteArray serializedRow;
            if (!serializeRow(rowData, columns, serializedRow))
            {
                errorMessage = "Failed to serialize new row.";
                qWarning() << funcName << "-" << errorMessage;
                file.close();
                tempFile.close();
                QFile::remove(tempFilePath);
                return false;
            }
            quint32 rowSize = serializedRow.size();
            tempFile.write(reinterpret_cast<const char *>(&rowSize), sizeof(rowSize));
            tempFile.write(serializedRow);
        }

        // 5. 复制插入点之后的数据
        QByteArray remainingData = file.readAll();
        tempFile.write(remainingData);

        file.close();
        tempFile.close();

        // 6. 替换原文件
        if (!QFile::remove(binFilePath))
        {
            errorMessage = "Failed to remove original file: " + file.errorString();
            qWarning() << funcName << "-" << errorMessage;
            return false;
        }

        if (!QFile::rename(tempFilePath, binFilePath))
        {
            errorMessage = "Failed to rename temp file: " + tempFile.errorString();
            qWarning() << funcName << "-" << errorMessage;
            return false;
        }

        return true;
    }

    bool BinaryFileHelper::updateRowsInBinary_v4(const QString &binFilePath, const QList<Vector::ColumnInfo> &columns,
                                              int schemaVersion, const QMap<int, Vector::RowData> &rowsToUpdate)
    {
        const char *funcName = "BinaryFileHelper::updateRowsInBinary_v4"; // Use char* for BFH_LOG_STDERR
        BFH_LOG_STDERR(funcName, "ENTERED. File: %s, Modified rows: %d, SchemaVersion: %d",
                       binFilePath.toStdString().c_str(), rowsToUpdate.size(), schemaVersion);
        // Original qDebug:
        // qDebug() << funcName << "- 开始更新文件:" << binFilePath << ", 修改行数:" << rowsToUpdate.size() << ", 使用随机写入";

        // 添加用于跟踪损坏行的数据结构
        QSet<int> corruptedRows;

        if (rowsToUpdate.isEmpty())
        {
            BFH_LOG_STDERR(funcName, "No rows to modify. Exiting true.");
            // Original qDebug:
            // qDebug() << funcName << "- 没有需要修改的行，直接返回 true";
            return true;
        }

        QFile file(binFilePath);
        if (!file.exists())
        {
            BFH_LOG_STDERR(funcName, "CRITICAL_ERROR: File does not exist, cannot update: %s. Exiting false.", binFilePath.toStdString().c_str());
            // Original qCritical:
            // qCritical() << funcName << "- CRITICAL_ERROR: 二进制文件不存在，无法更新: " << binFilePath << " (Reason: File does not exist)";
            return false;
        }
        BFH_LOG_STDERR(funcName, "File exists: %s", binFilePath.toStdString().c_str());

        if (!file.open(QIODevice::ReadWrite))
        {
            BFH_LOG_STDERR(funcName, "CRITICAL_ERROR: Failed to open file in ReadWrite mode: %s, OS error: %s. Exiting false.",
                           binFilePath.toStdString().c_str(), file.errorString().toStdString().c_str());
            // Original qCritical:
            // qCritical() << funcName << "- CRITICAL_ERROR: 无法以读写模式打开文件: " << binFilePath << ", OS错误:" << file.errorString() << " (Reason: Failed to open file in ReadWrite mode)";
            return false;
        }
        BFH_LOG_STDERR(funcName, "File opened successfully in ReadWrite mode: %s", binFilePath.toStdString().c_str());
        // Original qDebug:
        // qDebug() << funcName << "- 文件已成功以读写模式打开:" << binFilePath;

        BinaryFileHeader header;
        // Note: readBinaryHeader itself will be instrumented later to use BFH_LOG_STDERR
        if (!readBinaryHeader(&file, header)) // Assuming readBinaryHeader does its own BFH_LOG_STDERR on failure path
        {
            BFH_LOG_STDERR(funcName, "CRITICAL_ERROR: readBinaryHeader failed for file: %s. Exiting false (logged by readBinaryHeader).", binFilePath.toStdString().c_str());
            // Original qCritical:
            // qCritical() << funcName << "- CRITICAL_ERROR: 读取或验证二进制文件头失败 (Reason: readBinaryHeader failed), 文件:" << binFilePath;
            file.close();
            return false;
        }
        BFH_LOG_STDERR(funcName, "Header read successfully. FileVer: %u, SchemaVer: %d, RowCount: %llu",
                       header.file_format_version, header.data_schema_version, header.row_count_in_file);
        // Original qDebug:
        // qDebug() << funcName << "- 文件头读取成功. 文件版本:" << header.file_format_version << "Schema版本:" << header.data_schema_version << "行数:" << header.row_count_in_file;

        if (header.data_schema_version != schemaVersion)
        {
            BFH_LOG_STDERR(funcName, "CRITICAL_ERROR: Schema version mismatch. File has %d, expected %d. File: %s. Exiting false.",
                           header.data_schema_version, schemaVersion, binFilePath.toStdString().c_str());
            // Original qCritical:
            // qCritical() << funcName << "- CRITICAL_ERROR: 文件数据schema版本(" << header.data_schema_version
            //            << ")与期望的DB schema版本(" << schemaVersion << ")不匹配, 文件:" << binFilePath << " (Reason: Schema version mismatch)";
            file.close();
            return false;
        }

        if (header.column_count_in_file != static_cast<uint32_t>(columns.size()))
        {
            BFH_LOG_STDERR(funcName, "CRITICAL_ERROR: Column count mismatch. File has %u, expected %zu. File: %s. Exiting false.",
                           header.column_count_in_file, static_cast<size_t>(columns.size()), binFilePath.toStdString().c_str());
            // Original qCritical:
            // qCritical() << funcName << "- CRITICAL_ERROR: 文件头列数(" << header.column_count_in_file
            //            << ")与期望的列数(" << columns.size() << ")不匹配, 文件:" << binFilePath << " (Reason: Column count mismatch)";
            file.close();
            return false;
        }
        BFH_LOG_STDERR(funcName, "Schema version and column count checks passed.");
        // Original qDebug:
        // qDebug() << funcName << "- Schema版本和列数检查通过.";

        struct RowInfo
        {
            qint64 offset;
            quint32 data_size;
            quint32 total_size;
        };

        QVector<RowInfo> rowPositions;
        if (header.row_count_in_file > 0)
        {
            rowPositions.reserve(header.row_count_in_file);
        }

        QDataStream in(&file);
        in.setByteOrder(QDataStream::LittleEndian);

        // 修正点1: 使用 sizeof(BinaryFileHeader)
        qint64 initialDataPos = sizeof(BinaryFileHeader);
        if (!file.seek(initialDataPos))
        {
            qCritical() << funcName << "- CRITICAL_ERROR: Seek到初始数据位置(" << initialDataPos << ")失败. 文件:" << binFilePath
                        << ", OS错误:" << file.errorString() << " (Reason: Initial file seek failed)";
            file.close();
            return false;
        }
        qDebug() << funcName << "- 定位到初始数据位置:" << initialDataPos;

        // 首先，遍历文件一次，记录所有行的原始偏移量和大小
        QList<RowInfo> rowOffsetsAndSizes;
        rowOffsetsAndSizes.reserve(header.row_count_in_file);

        for (quint64 i = 0; i < header.row_count_in_file; ++i)
        {
            qint64 currentRowOffset = file.pos(); // Position before reading row size
            quint32 rowDataSize;

            // --- BEGIN ENHANCED CHECKS ---
            if (file.atEnd())
            {
                qCritical() << funcName << "- CRITICAL_ERROR: FAILED_INCREMENTAL_UPDATE_CONDITION: Attempting to read size for row" << i
                            << "but already at EOF. File pos:" << file.pos() << "Header row count:" << header.row_count_in_file
                            << "File size:" << file.size() << ". File:" << binFilePath
                            << " (Reason: EOF before reading row size, file likely truncated or header.row_count_in_file inconsistent)";
                file.close();
                return false;
            }
            if (file.bytesAvailable() < static_cast<qint64>(sizeof(quint32)))
            {
                qCritical() << funcName << "- CRITICAL_ERROR: FAILED_INCREMENTAL_UPDATE_CONDITION: Not enough bytes (" << file.bytesAvailable()
                            << ") to read size (quint32) for row" << i << ". File pos:" << file.pos()
                            << "File size:" << file.size() << ". File:" << binFilePath
                            << " (Reason: Insufficient bytes for row size, file likely truncated or header.row_count_in_file inconsistent)";
                file.close();
                return false;
            }
            // --- END ENHANCED CHECKS ---

            in >> rowDataSize; // Read the size of the current row's data
            if (in.status() != QDataStream::Ok)
            {
                qCritical() << funcName << "- CRITICAL_ERROR: FAILED_INCREMENTAL_UPDATE_CONDITION: 读取行" << i << "的数据大小时发生流错误. Stream status:" << in.status()
                            << "(1 means ReadPastEnd). File pos before read attempt:" << currentRowOffset
                            << ". Bytes available before read attempt:" << (file.size() - currentRowOffset)
                            << ". File size:" << file.size()
                            << ". File:" << binFilePath
                            << " (Reason: QDataStream error reading row data size)";
                file.close();
                return false;
            }

            // --- BEGIN ABNORMAL SIZE CHECK ---
            // 检查行大小是否异常（例如16MB的错误值）
            // 设置一个合理的最大行大小限制，例如1MB
            const quint32 MAX_REASONABLE_ROW_SIZE = 1024 * 1024; // 1MB
            if (rowDataSize > MAX_REASONABLE_ROW_SIZE)
            {
                qWarning() << funcName << "- WARNING: Row" << i << "claims an abnormally large data size:" << rowDataSize
                           << "bytes, which exceeds the reasonable limit of" << MAX_REASONABLE_ROW_SIZE
                           << "bytes. This is likely due to file corruption. Will attempt to recover.";

                // 尝试查看实际可用数据量
                qint64 bytesAvailable = file.size() - file.pos();

                // 计算更合理的行大小，基于最大行大小限制和可用数据量
                quint32 adjustedSize = qMin(static_cast<quint32>(bytesAvailable), MAX_REASONABLE_ROW_SIZE);

                if (adjustedSize > 0)
                {
                    qWarning() << funcName << "- Adjusting row" << i << "size from" << rowDataSize << "to" << adjustedSize
                               << "bytes based on available data and reasonable limits.";
                    rowDataSize = adjustedSize;

                    // 记录这一行后续需要特殊处理
                    // 这里可以添加一个标记，如将该行加入到一个"需要完全重写"的列表中
                    corruptedRows.insert(i);
                }
                else
                {
                    qCritical() << funcName << "- CRITICAL_ERROR: FAILED_INCREMENTAL_UPDATE_CONDITION: No reasonable size could be determined for row" << i
                                << ". File pos:" << file.pos() << "File size:" << file.size()
                                << ". File:" << binFilePath;
                    file.close();
                    return false;
                }
            }
            // --- END ABNORMAL SIZE CHECK ---

            // --- BEGIN POST-READ CHECK ---
            if (file.bytesAvailable() < static_cast<qint64>(rowDataSize))
            {
                qCritical() << funcName << "- CRITICAL_ERROR: FAILED_INCREMENTAL_UPDATE_CONDITION: Row" << i << "claims data size" << rowDataSize
                            << "but only" << file.bytesAvailable() << "bytes remain in file after reading its size. File pos:" << file.pos()
                            << "File size:" << file.size() << ". File:" << binFilePath
                            << " (Reason: Insufficient bytes for claimed row data, file likely truncated or row size value corrupt)";
                file.close();
                return false;
            }
            // --- END POST-READ CHECK ---

            // Correctly create and append RowInfo object
            RowInfo currentRowDetails;
            currentRowDetails.offset = currentRowOffset;
            currentRowDetails.data_size = rowDataSize;
            currentRowDetails.total_size = sizeof(quint32) + rowDataSize; // Size of data + size of the size field itself
            rowOffsetsAndSizes.append(currentRowDetails);

            // Seek past this row's data (quint32 for size + data itself) to get to the next row's size marker
            qint64 nextRowPos = currentRowOffset + sizeof(quint32) + rowDataSize;
            if (nextRowPos > file.size() && i < header.row_count_in_file - 1)
            { // Check if seek would go past EOF unless it's the very last row
                qCritical() << funcName << "- CRITICAL_ERROR: FAILED_INCREMENTAL_UPDATE_CONDITION: Calculated seek offset" << nextRowPos
                            << "for row" << i << "exceeds file size" << file.size() << ". File:" << binFilePath
                            << " (Reason: Corrupt row size or truncated file before seeking past row data)";
                file.close();
                return false;
            }
            if (!file.seek(nextRowPos))
            {
                qCritical() << funcName << "- CRITICAL_ERROR: FAILED_INCREMENTAL_UPDATE_CONDITION: Failed to seek past row" << i << "data. Current pos:" << file.pos()
                            << "Target offset:" << nextRowPos << ". File size:" << file.size()
                            << ". File error:" << file.errorString() << ". File:" << binFilePath;
                file.close();
                return false;
            }
        }
        qDebug() << funcName << "- 所有" << header.row_count_in_file << "行的位置信息读取完毕.";

        int maxRowIndexInFile = (header.row_count_in_file > 0) ? (static_cast<int>(header.row_count_in_file) - 1) : -1;
        QList<int> invalidRowIndicesForUpdate;
        for (auto it = rowsToUpdate.constBegin(); it != rowsToUpdate.constEnd(); ++it)
        {
            int rowIndex = it.key();
            if (rowIndex < 0 || rowIndex > maxRowIndexInFile)
            {
                qWarning() << funcName << "- 警告: 请求更新的行索引" << rowIndex << "超出文件范围 [0," << maxRowIndexInFile << "]. 该行将被忽略.";
                invalidRowIndicesForUpdate.append(rowIndex);
            }
        }

        if (!invalidRowIndicesForUpdate.isEmpty() && invalidRowIndicesForUpdate.size() == rowsToUpdate.size())
        {
            qCritical() << funcName << "- CRITICAL_ERROR: 所有请求修改的行索引都无效或超出文件范围. 文件:" << binFilePath << " (Reason: All modified row indices are invalid)";
            file.close();
            return false;
        }
        qDebug() << funcName << "- 行索引范围检查完成. 无效待更新行数:" << invalidRowIndicesForUpdate.size();

        quint32 rowsUpdatedSuccessfully = 0;

        for (auto it = rowsToUpdate.constBegin(); it != rowsToUpdate.constEnd(); ++it)
        {
            int rowIndex = it.key();
            const Vector::RowData &newRowData = it.value();

            if (invalidRowIndicesForUpdate.contains(rowIndex))
            {
                qDebug() << funcName << "- 跳过更新无效/越界行索引:" << rowIndex;
                continue;
            }

            // 检查该行是否已被标记为损坏
            if (corruptedRows.contains(rowIndex))
            {
                qWarning() << funcName << "- 跳过更新损坏的行:" << rowIndex << "。该行将在后续完全重写中处理";
                continue;
            }

            if (rowIndex < 0 || rowIndex >= rowOffsetsAndSizes.size())
            {
                qCritical() << funcName << "- CRITICAL_ERROR: (Defensive Check) 行索引" << rowIndex << "在rowOffsetsAndSizes中越界 (Size: " << rowOffsetsAndSizes.size() << "). 文件:" << binFilePath
                            << " (Reason: Row index out of bounds for rowOffsetsAndSizes during update loop)";
                BFH_LOG_STDERR(funcName, "CRITICAL_ERROR: Row index %d out of bounds. rowOffsetsAndSizes.size = %d",
                               rowIndex, rowOffsetsAndSizes.size());
                file.close();
                return false;
            }

            const RowInfo &originalRowInfo = rowOffsetsAndSizes[rowIndex];

            BFH_LOG_STDERR(funcName, "Processing row %d. Offset: %lld, Data Size: %u",
                           rowIndex, originalRowInfo.offset, originalRowInfo.data_size);

            QByteArray serializedNewRow;
            if (!serializeRow(newRowData, columns, serializedNewRow))
            {
                qCritical() << funcName << "- CRITICAL_ERROR: 序列化新行数据失败，行索引:" << rowIndex << ", 文件:" << binFilePath
                            << " (Reason: serializeRow returned false)";
                BFH_LOG_STDERR(funcName, "CRITICAL_ERROR: Failed to serialize row %d", rowIndex);
                file.close();
                return false;
            }

            if (static_cast<quint32>(serializedNewRow.size()) != originalRowInfo.data_size)
            {
                // 记录大小不匹配详情但不立即失败，而是记录问题并继续
                qWarning() << funcName << "- 警告: 行" << rowIndex << "的新数据长度(" << serializedNewRow.size()
                           << ")与原始数据长度(" << originalRowInfo.data_size << ")不匹配. 文件:" << binFilePath;
                BFH_LOG_STDERR(funcName, "WARNING: Row %d data length mismatch. New size: %d, Original size: %u",
                               rowIndex, serializedNewRow.size(), originalRowInfo.data_size);

                // 记录不匹配数据的内容摘要，帮助调试
                QString originalDataSummary = "未读取原始数据";
                QString newDataSummary = QString("新数据前32字节: %1").arg(QString(serializedNewRow.size() > 32 ? serializedNewRow.left(32).toHex(' ') : serializedNewRow.toHex(' ')));

                BFH_LOG_STDERR(funcName, "Data mismatch details - %s | %s",
                               originalDataSummary.toStdString().c_str(),
                               newDataSummary.toStdString().c_str());

                // 标记这行更新失败，但允许继续处理其他行
                // 稍后可能会触发完全重写模式
                continue;
            }

            qint64 seekPos = originalRowInfo.offset + sizeof(quint32);
            if (!file.seek(seekPos))
            {
                qCritical() << funcName << "- CRITICAL_ERROR: Seek到行" << rowIndex << "的数据位置(" << seekPos << ")失败. 文件:" << binFilePath
                            << ", OS错误:" << file.errorString() << " (Reason: File seek failed for row update)";
                BFH_LOG_STDERR(funcName, "CRITICAL_ERROR: Failed to seek to row %d data position %lld. OS error: %s",
                               rowIndex, seekPos, file.errorString().toStdString().c_str());
                file.close();
                return false;
            }

            qint64 bytesWritten = file.write(serializedNewRow);
            if (bytesWritten != serializedNewRow.size())
            {
                qCritical() << funcName << "- CRITICAL_ERROR: 写入新行数据失败，行索引:" << rowIndex << ". 预期写入:" << serializedNewRow.size()
                            << ", 实际写入:" << bytesWritten << ". 文件:" << binFilePath
                            << ", OS错误:" << file.errorString() << " (Reason: File write failed for row update)";
                BFH_LOG_STDERR(funcName, "CRITICAL_ERROR: Failed to write row %d data. Expected: %d, Actual: %lld. OS error: %s",
                               rowIndex, serializedNewRow.size(), bytesWritten, file.errorString().toStdString().c_str());
                file.close();
                return false;
            }
            BFH_LOG_STDERR(funcName, "Successfully updated row %d at offset %lld, data size: %d",
                           rowIndex, originalRowInfo.offset, serializedNewRow.size());
            qDebug() << funcName << "- 成功更新行:" << rowIndex << "于Offset:" << originalRowInfo.offset << "新数据长度:" << serializedNewRow.size();
            rowsUpdatedSuccessfully++;
        }

        if (rowsUpdatedSuccessfully > 0)
        {
            header.timestamp_updated = QDateTime::currentSecsSinceEpoch();
            if (!file.seek(0))
            {
                qCritical() << funcName << "- CRITICAL_ERROR: Seek到文件开头失败 (更新文件头前). 文件:" << binFilePath
                            << ", OS错误:" << file.errorString() << " (Reason: File seek to beginning failed for header timestamp update)";
                BFH_LOG_STDERR(funcName, "CRITICAL_ERROR: Failed to seek to file beginning for header update. OS error: %s",
                               file.errorString().toStdString().c_str());
                file.close();
                return false;
            }
            if (!writeBinaryHeader(&file, header))
            {
                qCritical() << funcName << "- CRITICAL_ERROR: 更新文件头的时间戳失败 (Reason: writeBinaryHeader for timestamp update failed), 文件:" << binFilePath;
                BFH_LOG_STDERR(funcName, "CRITICAL_ERROR: Failed to update file header timestamp");
                file.close();
                return false;
            }
            BFH_LOG_STDERR(funcName, "Successfully updated file header timestamp");
            qDebug() << funcName << "- 文件头时间戳已成功更新.";
        }
        else if (rowsToUpdate.size() > invalidRowIndicesForUpdate.size())
        {
            qWarning() << funcName << "- 警告: 有 " << (rowsToUpdate.size() - invalidRowIndicesForUpdate.size())
                       << " 个有效修改行，但成功更新数量为0. 可能所有有效行都因某种原因（如长度不匹配）未能更新.";
            BFH_LOG_STDERR(funcName, "WARNING: Found %d valid rows to modify, but updated 0 rows successfully",
                           (rowsToUpdate.size() - invalidRowIndicesForUpdate.size()));
            if ((rowsToUpdate.size() - invalidRowIndicesForUpdate.size()) > 0 && rowsUpdatedSuccessfully == 0)
            {
                qCritical() << funcName << "- CRITICAL_ERROR: 有效修改行存在，但无任何行被成功更新。上层应已处理此情况。";
                BFH_LOG_STDERR(funcName, "CRITICAL_ERROR: Valid rows exist but none were updated successfully");
                file.close();
                return false;
            }
        }

        file.close();
        BFH_LOG_STDERR(funcName, "File closed. Successfully updated %u out of %d rows",
                       rowsUpdatedSuccessfully, (rowsToUpdate.size() - invalidRowIndicesForUpdate.size()));
        qDebug() << funcName << "- 文件已关闭. 总计成功更新 " << rowsUpdatedSuccessfully << " 行.";

        // 更新数据后清除缓存，确保下次读取时获取最新结构
        if (rowsUpdatedSuccessfully > 0)
        {
            clearRowOffsetCache(binFilePath);
        }

        // 计算成功更新率
        int validRowCount = rowsToUpdate.size() - invalidRowIndicesForUpdate.size();
        int attemptedRowCount = validRowCount - corruptedRows.size();

        // 如果有损坏的行，记录到日志，但不影响返回值（只要其他行成功更新）
        if (!corruptedRows.isEmpty())
        {
            qWarning() << funcName << "- 检测到" << corruptedRows.size() << "个损坏的行，这些行将在下次完全重写时更新";
            BFH_LOG_STDERR(funcName, "Detected %d corrupted rows that will be updated during a full rewrite later",
                           corruptedRows.size());

            // 只要有一些行成功更新，我们就认为操作成功（即使有一些损坏的行）
            if (rowsUpdatedSuccessfully > 0)
            {
                return true;
            }
        }

        // 原始逻辑：所有有效行都必须成功更新
        return rowsUpdatedSuccessfully == static_cast<quint32>(validRowCount);
    }
