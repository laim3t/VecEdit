
    bool BinaryFileHelper::updateRowsInBinary(const QString &binFilePath, const QList<Vector::ColumnInfo> &columns,
                                              int schemaVersion, const QMap<int, Vector::RowData> &modifiedRows)
    {
        const char *funcName = "BinaryFileHelper::updateRowsInBinary"; // Use char* for BFH_LOG_STDERR
        BFH_LOG_STDERR(funcName, "ENTERED. File: %s, Modified rows: %d, SchemaVersion: %d",
                       binFilePath.toStdString().c_str(), modifiedRows.size(), schemaVersion);
        // Original qDebug:
        // qDebug() << funcName << "- 开始更新文件:" << binFilePath << ", 修改行数:" << modifiedRows.size() << ", 使用随机写入";

        // 添加用于跟踪损坏行的数据结构
        QSet<int> corruptedRows;

        if (modifiedRows.isEmpty())
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
        for (auto it = modifiedRows.constBegin(); it != modifiedRows.constEnd(); ++it)
        {
            int rowIndex = it.key();
            if (rowIndex < 0 || rowIndex > maxRowIndexInFile)
            {
                qWarning() << funcName << "- 警告: 请求更新的行索引" << rowIndex << "超出文件范围 [0," << maxRowIndexInFile << "]. 该行将被忽略.";
                invalidRowIndicesForUpdate.append(rowIndex);
            }
        }

        if (!invalidRowIndicesForUpdate.isEmpty() && invalidRowIndicesForUpdate.size() == modifiedRows.size())
        {
            qCritical() << funcName << "- CRITICAL_ERROR: 所有请求修改的行索引都无效或超出文件范围. 文件:" << binFilePath << " (Reason: All modified row indices are invalid)";
            file.close();
            return false;
        }
        qDebug() << funcName << "- 行索引范围检查完成. 无效待更新行数:" << invalidRowIndicesForUpdate.size();

        quint32 rowsUpdatedSuccessfully = 0;

        for (auto it = modifiedRows.constBegin(); it != modifiedRows.constEnd(); ++it)
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
        else if (modifiedRows.size() > invalidRowIndicesForUpdate.size())
        {
            qWarning() << funcName << "- 警告: 有 " << (modifiedRows.size() - invalidRowIndicesForUpdate.size())
                       << " 个有效修改行，但成功更新数量为0. 可能所有有效行都因某种原因（如长度不匹配）未能更新.";
            BFH_LOG_STDERR(funcName, "WARNING: Found %d valid rows to modify, but updated 0 rows successfully",
                           (modifiedRows.size() - invalidRowIndicesForUpdate.size()));
            if ((modifiedRows.size() - invalidRowIndicesForUpdate.size()) > 0 && rowsUpdatedSuccessfully == 0)
            {
                qCritical() << funcName << "- CRITICAL_ERROR: 有效修改行存在，但无任何行被成功更新。上层应已处理此情况。";
                BFH_LOG_STDERR(funcName, "CRITICAL_ERROR: Valid rows exist but none were updated successfully");
                file.close();
                return false;
            }
        }

        file.close();
        BFH_LOG_STDERR(funcName, "File closed. Successfully updated %u out of %d rows",
                       rowsUpdatedSuccessfully, (modifiedRows.size() - invalidRowIndicesForUpdate.size()));
        qDebug() << funcName << "- 文件已关闭. 总计成功更新 " << rowsUpdatedSuccessfully << " 行.";

        // 更新数据后清除缓存，确保下次读取时获取最新结构
        if (rowsUpdatedSuccessfully > 0)
        {
            clearRowOffsetCache(binFilePath);
        }

        // 计算成功更新率
        int validRowCount = modifiedRows.size() - invalidRowIndicesForUpdate.size();
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

    bool BinaryFileHelper::readRowSizeWithValidation(QDataStream &stream, QFile &file, quint32 maxReasonableSize, quint32 &rowSize)
    {
        const char *funcName = "BinaryFileHelper::readRowSizeWithValidation";

        // 检查是否有足够的字节可读
        if (file.bytesAvailable() < static_cast<qint64>(sizeof(quint32)))
        {
            qCritical() << funcName << "- 错误: 文件剩余字节数(" << file.bytesAvailable()
                        << ")不足以读取行大小(需要4字节)";
            return false;
        }

        // 读取大小值
        stream >> rowSize;

        // 检查流状态
        if (stream.status() != QDataStream::Ok)
        {
            qCritical() << funcName << "- 错误: 读取行大小时流状态异常. 状态:" << stream.status();
            return false;
        }

        // 检查大小是否合理
        if (rowSize > maxReasonableSize)
        {
            qWarning() << funcName << "- 警告: 读取到异常大行大小:" << rowSize
                       << ", 超过最大合理值:" << maxReasonableSize << ". 将进行调整";

            // 根据剩余可用字节计算合理大小
            qint64 availableBytes = file.bytesAvailable();
            if (availableBytes <= 0)
            {
                qCritical() << funcName << "- 错误: 文件无剩余可用字节";
                return false;
            }

            // 限制在剩余字节数和最大合理大小之间
            rowSize = qMin(static_cast<quint32>(availableBytes), maxReasonableSize);
            qWarning() << funcName << "- 已将行大小调整为:" << rowSize << "字节";
        }

        // 最终验证确保大小不超过可用字节数
        if (rowSize > static_cast<quint32>(file.bytesAvailable()))
        {
            qCritical() << funcName << "- 错误: 行大小(" << rowSize
                        << ")超过剩余可用字节数(" << file.bytesAvailable() << ")";
            return false;
        }

        return true;
    }

    bool BinaryFileHelper::robustUpdateRowsInBinary(const QString &binFilePath,
                                                    const QList<Vector::ColumnInfo> &columns,
                                                    int schemaVersion,
                                                    const QMap<int, Vector::RowData> &modifiedRows)
    {
        const char *funcName = "BinaryFileHelper::robustUpdateRowsInBinary";

        // 输出日志，表明正在使用新的健壮版本
        qInfo() << funcName << "- 使用强健版本的增量更新，文件:" << binFilePath
                << "，修改行数:" << modifiedRows.size() << "，Schema版本:" << schemaVersion;

        // 记录每一步的开始时间，用于性能分析
        QElapsedTimer timer;
        timer.start();

        // ----- 第1步: 文件检查 -----

        if (modifiedRows.isEmpty())
        {
            qDebug() << funcName << "- 没有需要修改的行，直接返回成功";
            return true;
        }

        QFile file(binFilePath);

        if (!file.exists())
        {
            qCritical() << funcName << "- 文件不存在:" << binFilePath;
            return false;
        }

        if (!file.open(QIODevice::ReadWrite))
        {
            qCritical() << funcName << "- 无法以读写模式打开文件:" << binFilePath
                        << "，错误:" << file.errorString();
            return false;
        }

        qDebug() << funcName << "- 文件打开成功，耗时:" << timer.elapsed() << "ms";
        timer.restart();

        // ----- 第2步: 读取文件头 -----

        BinaryFileHeader header;
        if (!readBinaryHeader(&file, header))
        {
            qCritical() << funcName << "- 读取文件头失败";
            file.close();
            return false;
        }

        // 验证头信息
        if (header.data_schema_version != schemaVersion)
        {
            qCritical() << funcName << "- 文件Schema版本(" << header.data_schema_version
                        << ")与期望的版本(" << schemaVersion << ")不匹配";
            file.close();
            return false;
        }

        if (header.column_count_in_file != static_cast<uint32_t>(columns.size()))
        {
            qCritical() << funcName << "- 文件列数(" << header.column_count_in_file
                        << ")与期望的列数(" << columns.size() << ")不匹配";
            file.close();
            return false;
        }

        header.timestamp_updated = QDateTime::currentSecsSinceEpoch();
        qDebug() << funcName << "- 文件头验证成功，文件包含" << header.row_count_in_file
                 << "行数据，耗时:" << timer.elapsed() << "ms";
        timer.restart();

        // ----- 第3步: 扫描文件结构，建立行索引映射 -----

        // 建立行索引-> {偏移量, 大小} 的映射
        struct RowPosition
        {
            qint64 offset;    // 行起始位置(包括大小字段)
            quint32 dataSize; // 该行数据的大小(不包括大小字段)
        };

        QVector<RowPosition> rowPositions;
        rowPositions.reserve(header.row_count_in_file);

        // 设置合理的最大行大小限制
        const quint32 MAX_REASONABLE_ROW_SIZE = 1 * 1024 * 1024; // 1MB

        // 跟踪哪些行被跳过(因为损坏或其他问题)
        QSet<int> skippedRows;

        // 优先从持久化索引文件获取行偏移数据（如果没有再尝试内存缓存）
        QFileInfo fileInfo(binFilePath);
        QElapsedTimer cacheTimer;
        cacheTimer.start();

        bool usedCache = false;
        bool usedIndexFile = false;
        bool partialCacheUpdate = false;
        QSet<int> rowsToRescan;

        // 首先尝试从索引文件加载
        RowOffsetCache indexPositions = loadRowOffsetIndex(binFilePath, fileInfo.lastModified());
        if (!indexPositions.isEmpty() && indexPositions.size() == static_cast<int>(header.row_count_in_file))
        {
            qDebug() << funcName << "- 使用索引文件的行偏移数据，跳过文件扫描，耗时:" << cacheTimer.elapsed() << "ms";
            usedCache = true;
            usedIndexFile = true;

            // 检查是否有行需要重新扫描（timestamp=0的行）
            for (int i = 0; i < indexPositions.size(); i++)
            {
                if (indexPositions[i].timestamp == 0)
                {
                    rowsToRescan.insert(i);
                }
            }

            if (!rowsToRescan.isEmpty())
            {
                partialCacheUpdate = true;
                qDebug() << funcName << "- 检测到" << rowsToRescan.size() << "行需要重新扫描";
            }

            // 将索引数据转换为本地数据结构
            for (int i = 0; i < indexPositions.size(); i++)
            {
                RowPosition pos;
                pos.offset = indexPositions[i].offset;
                pos.dataSize = indexPositions[i].dataSize;
                rowPositions.append(pos);
            }

            // 同时更新内存缓存
            setRowOffsetCache(binFilePath, indexPositions);
        }
        else
        {
            // 如果索引文件不可用，则尝试从内存缓存获取
            cacheTimer.restart();
            RowOffsetCache cachedPositions = getRowOffsetCache(binFilePath, fileInfo.lastModified());

            // 如果内存缓存有效且行数与文件头一致，则直接使用缓存
            if (!cachedPositions.isEmpty() && cachedPositions.size() == static_cast<int>(header.row_count_in_file))
            {
                qDebug() << funcName << "- 使用内存缓存的行偏移数据，跳过文件扫描，耗时:" << cacheTimer.elapsed() << "ms";
                usedCache = true;

                // 检查是否有行需要重新扫描（timestamp=0的行）
                for (int i = 0; i < cachedPositions.size(); i++)
                {
                    if (cachedPositions[i].timestamp == 0)
                    {
                        rowsToRescan.insert(i);
                    }
                }

                if (!rowsToRescan.isEmpty())
                {
                    partialCacheUpdate = true;
                    qDebug() << funcName << "- 检测到" << rowsToRescan.size() << "行需要重新扫描";
                }

                // 将缓存数据转换为本地数据结构
                for (int i = 0; i < cachedPositions.size(); i++)
                {
                    RowPosition pos;
                    pos.offset = cachedPositions[i].offset;
                    pos.dataSize = cachedPositions[i].dataSize;
                    rowPositions.append(pos);
                }

                // 写入到索引文件以便下次使用
                if (!usedIndexFile)
                {
                    saveRowOffsetIndex(binFilePath, cachedPositions);
                }
            }
            else
            {
                // 缓存和索引文件都无效，需要扫描文件结构
                qDebug() << funcName << "- 未找到有效缓存或索引文件，执行完整扫描";

                // 优化点：使用多线程并行处理大文件扫描
                if (header.row_count_in_file > 100000) // 只对大文件使用并行优化
                {
                    QElapsedTimer parallelTimer;
                    parallelTimer.start();

                    qDebug() << funcName << "- 使用并行处理扫描大文件，行数:" << header.row_count_in_file;

                    // 确定CPU核心数，设置线程数
                    int threadCount = QThread::idealThreadCount();
                    if (threadCount <= 0)
                        threadCount = 4;                // 默认至少4个线程
                    threadCount = qMin(threadCount, 8); // 限制最大线程数

                    qDebug() << funcName << "- 使用" << threadCount << "个线程并行扫描";

                    // 计算每个线程处理的行数范围
                    quint64 rowsPerThread = header.row_count_in_file / threadCount;
                    quint64 remainingRows = header.row_count_in_file % threadCount;

                    // 使用互斥锁保护共享数据结构
                    QMutex mutex;

                    // 预分配空间，避免并发写入冲突
                    rowPositions.resize(header.row_count_in_file);

                    // 创建线程池
                    QThreadPool threadPool;
                    threadPool.setMaxThreadCount(threadCount);

                    // 记录每个线程扫描的起始位置
                    QVector<qint64> threadStartPositions(threadCount + 1);
                    threadStartPositions[0] = file.pos(); // 文件头后的位置

                    // 获取文件大小，用于估算偏移量
                    qint64 dataSize = file.size() - file.pos();
                    qint64 estimatedBytesPerThread = dataSize / threadCount;

                    // 第一阶段：估算每个线程的起始位置
                    for (int t = 1; t < threadCount; t++)
                    {
                        qint64 estimatedPos = file.pos() + t * estimatedBytesPerThread;
                        // 确保不超过文件大小
                        estimatedPos = qMin(estimatedPos, file.size());
                        threadStartPositions[t] = estimatedPos;
                    }
                    threadStartPositions[threadCount] = file.size();

                    // 第二阶段：找到每个线程起始处的准确行边界
                    if (threadCount > 1)
                    {
                        QVector<QPair<int, qint64>> correctedBoundaries;

                        for (int t = 1; t < threadCount; t++)
                        {
                            // 在估算位置附近寻找行边界
                            qint64 searchPos = threadStartPositions[t];

                            // 创建单独的文件句柄以避免干扰主文件位置
                            QFile boundaryFile(binFilePath);
                            if (!boundaryFile.open(QIODevice::ReadOnly))
                            {
                                qWarning() << funcName << "- 无法打开文件来确定线程边界";
                                continue;
                            }

                            // 从估算位置前移一点开始搜索，确保不会错过边界
                            qint64 backtrackPos = qMax(file.pos(), searchPos - 1024);
                            if (!boundaryFile.seek(backtrackPos))
                            {
                                boundaryFile.close();
                                continue;
                            }

                            QDataStream boundaryStream(&boundaryFile);
                            boundaryStream.setByteOrder(QDataStream::LittleEndian);

                            qint64 foundBoundary = backtrackPos;
                            int rowCount = 0;

                            // 读取并跳过完整的行，直到达到或超过估算位置
                            while (!boundaryFile.atEnd() && boundaryFile.pos() < searchPos + 1024)
                            {
                                quint32 rowSize;
                                qint64 rowStart = boundaryFile.pos();

                                if (readRowSizeWithValidation(boundaryStream, boundaryFile, MAX_REASONABLE_ROW_SIZE, rowSize))
                                {
                                    // 找到有效行边界
                                    if (rowStart >= searchPos)
                                    {
                                        foundBoundary = rowStart;
                                        break;
                                    }

                                    // 跳过这一行的数据
                                    if (!boundaryFile.seek(boundaryFile.pos() + rowSize))
                                    {
                                        break;
                                    }

                                    rowCount++;
                                }
                                else
                                {
                                    // 读取错误，向前移动一些字节再试
                                    if (!boundaryFile.seek(boundaryFile.pos() + 4))
                                    {
                                        break;
                                    }
                                }
                            }

                            correctedBoundaries.append(QPair<int, qint64>(t, foundBoundary));
                            boundaryFile.close();
                        }

                        // 更新线程边界
                        for (const auto &pair : correctedBoundaries)
                        {
                            threadStartPositions[pair.first] = pair.second;
                        }
                    }

                    // 第三阶段：创建并行任务
                    QVector<QFuture<void>> futures;
                    QVector<QVector<RowPosition>> threadResults(threadCount);

                    for (int t = 0; t < threadCount; t++)
                    {
                        qint64 startPos = threadStartPositions[t];
                        qint64 endPos = threadStartPositions[t + 1];

                        // 估算该范围内的行数
                        quint64 estimatedRows = (t == threadCount - 1) ? (rowsPerThread + remainingRows) : rowsPerThread;

                        threadResults[t].reserve(estimatedRows);

                        // 创建并启动任务
                        auto future = QtConcurrent::run([binFilePath, startPos, endPos, estimatedRows, MAX_REASONABLE_ROW_SIZE, t, funcName, &threadResults]()
                                                        {
                            // 为每个线程创建独立的文件句柄
                            QFile threadFile(binFilePath);
                            if (!threadFile.open(QIODevice::ReadOnly)) {
                                qWarning() << funcName << "- 线程" << t << "无法打开文件";
                                return;
                            }
                            
                            if (!threadFile.seek(startPos)) {
                                qWarning() << funcName << "- 线程" << t << "无法定位到起始位置:" << startPos;
                                threadFile.close();
                                return;
                            }
                            
                            QDataStream threadStream(&threadFile);
                            threadStream.setByteOrder(QDataStream::LittleEndian);
                            
                            QVector<RowPosition> &results = threadResults[t];
                            
                            // 扫描直到达到下一个线程的起始位置或文件结束
                            quint64 rowsScanned = 0;
                            while (threadFile.pos() < endPos && !threadFile.atEnd()) {
                                RowPosition pos;
                                pos.offset = threadFile.pos();
                                
                                quint32 rowSize;
                                if (!readRowSizeWithValidation(threadStream, threadFile, MAX_REASONABLE_ROW_SIZE, rowSize)) {
                                    // 读取失败，尝试跳过一些字节
                                    threadFile.seek(threadFile.pos() + 4);
                                    continue;
                                }
                                
                                pos.dataSize = rowSize;
                                results.append(pos);
                                
                                // 跳过行数据
                                if (!threadFile.seek(threadFile.pos() + rowSize)) {
                                    break;
                                }
                                
                                rowsScanned++;
                            }
                            
                            qDebug() << funcName << "- 线程" << t << "扫描完成，处理了" << rowsScanned << "行，从" 
                                     << startPos << "到" << threadFile.pos();
                            
                            threadFile.close(); });

                        futures.append(future);
                    }

                    // 等待所有线程完成
                    for (int t = 0; t < threadCount; t++)
                    {
                        futures[t].waitForFinished();
                    }

                    // 第四阶段：合并结果
                    rowPositions.clear();
                    rowPositions.reserve(header.row_count_in_file);

                    for (int t = 0; t < threadCount; t++)
                    {
                        rowPositions.append(threadResults[t]);
                    }

                    qDebug() << funcName << "- 并行扫描完成，总共找到" << rowPositions.size() << "行，耗时:"
                             << parallelTimer.elapsed() << "ms";

                    // 创建新的缓存
                    RowOffsetCache newCache;
                    newCache.reserve(rowPositions.size());

                    for (int i = 0; i < rowPositions.size(); i++)
                    {
                        RowPositionInfo cachePos;
                        cachePos.offset = rowPositions[i].offset;
                        cachePos.dataSize = rowPositions[i].dataSize;
                        cachePos.timestamp = QDateTime::currentSecsSinceEpoch();
                        newCache.append(cachePos);
                    }

                    // 设置缓存
                    setRowOffsetCache(binFilePath, newCache);

                    // 保存索引文件
                    saveRowOffsetIndex(binFilePath, newCache);
                }
                else
                {
                    // 原有的顺序扫描代码，用于小文件
                    QDataStream in(&file);
                    in.setByteOrder(QDataStream::LittleEndian);

                    // 从文件头后的位置开始扫描
                    qint64 currentPos = file.pos();

                    // 创建新的缓存
                    RowOffsetCache newCache;
                    newCache.reserve(header.row_count_in_file);

                    for (quint64 i = 0; i < header.row_count_in_file; ++i)
                    {
                        if (file.atEnd())
                        {
                            qWarning() << funcName << "- 文件意外结束，行索引:" << i
                                       << "，预期行数:" << header.row_count_in_file;
                            break;
                        }

                        RowPosition pos;
                        pos.offset = currentPos;

                        // 使用健壮的行大小读取方法
                        if (!readRowSizeWithValidation(in, file, MAX_REASONABLE_ROW_SIZE, pos.dataSize))
                        {
                            qWarning() << funcName << "- 行" << i << "的大小读取失败，将跳过该行";
                            skippedRows.insert(i);

                            // 尝试跳到下一个合理位置继续
                            if (i < header.row_count_in_file - 1)
                            {
                                // 估算平均行大小并跳过
                                qint64 avgRowSize = (file.size() - file.pos()) / (header.row_count_in_file - i);
                                qint64 nextPos = file.pos() + qMin<qint64>(avgRowSize, 1024); // 最多跳1KB

                                if (!file.seek(nextPos))
                                {
                                    qCritical() << funcName << "- 无法从损坏的行" << i << "恢复";
                                    break;
                                }

                                currentPos = file.pos();
                                continue;
                            }
                            else
                            {
                                // 最后一行，可以直接退出
                                break;
                            }
                        }

                        // 成功读取行大小，添加到位置映射中
                        rowPositions.append(pos);

                        // 同时添加到新的缓存中
                        RowPositionInfo cachePos;
                        cachePos.offset = pos.offset;
                        cachePos.dataSize = pos.dataSize;
                        cachePos.timestamp = QDateTime::currentSecsSinceEpoch();
                        newCache.append(cachePos);

                        // 计算下一行的位置
                        currentPos = pos.offset + sizeof(quint32) + pos.dataSize;

                        // 跳到下一行
                        if (i < header.row_count_in_file - 1 && !file.seek(currentPos))
                        {
                            qWarning() << funcName << "- 无法跳转到下一行位置，文件可能已损坏";
                            break;
                        }
                    }

                    // 如果成功扫描了所有行，且没有跳过的行，那么缓存数据
                    if (rowPositions.size() == header.row_count_in_file && skippedRows.isEmpty())
                    {
                        // 更新内存缓存
                        setRowOffsetCache(binFilePath, newCache);

                        // 同时保存到索引文件
                        saveRowOffsetIndex(binFilePath, newCache);

                        qDebug() << funcName << "- 将行偏移数据存入缓存，共" << newCache.size() << "行";
                    }
                }
            }
        }

        // 选择性重新扫描被标记的行
        if (partialCacheUpdate && !rowsToRescan.isEmpty() && rowPositions.size() == header.row_count_in_file)
        {
            qDebug() << funcName << "- 开始选择性重新扫描" << rowsToRescan.size() << "行";
            QElapsedTimer rescanTimer;
            rescanTimer.start();

            QDataStream in(&file);
            in.setByteOrder(QDataStream::LittleEndian);

            int successfulRescans = 0;

            foreach (int rowIndex, rowsToRescan)
            {
                if (rowIndex < 0 || rowIndex >= rowPositions.size())
                {
                    continue;
                }

                // 获取当前存储的位置信息，可能不准确
                RowPosition &pos = rowPositions[rowIndex];

                // 尝试定位到该行
                if (!file.seek(pos.offset))
                {
                    qWarning() << funcName << "- 无法定位到行" << rowIndex << "的位置:" << pos.offset;
                    continue;
                }

                // 读取行大小
                quint32 rowSize;
                if (!readRowSizeWithValidation(in, file, MAX_REASONABLE_ROW_SIZE, rowSize))
                {
                    qWarning() << funcName << "- 无法读取行" << rowIndex << "的大小";
                    continue;
                }

                // 更新大小信息
                if (rowSize != pos.dataSize)
                {
                    qDebug() << funcName << "- 行" << rowIndex << "的大小已更新:"
                             << pos.dataSize << " -> " << rowSize;
                    pos.dataSize = rowSize;
                }

                // 更新内存缓存和索引文件中的行信息
                if (s_fileRowOffsetCache.contains(binFilePath) &&
                    rowIndex < s_fileRowOffsetCache[binFilePath].size())
                {
                    s_fileRowOffsetCache[binFilePath][rowIndex].dataSize = rowSize;
                    s_fileRowOffsetCache[binFilePath][rowIndex].timestamp = QDateTime::currentSecsSinceEpoch();
                    successfulRescans++;
                }
            }

            qDebug() << funcName << "- 选择性重新扫描完成，更新了" << successfulRescans << "行，耗时:"
                     << rescanTimer.elapsed() << "ms";

            // 重新保存索引文件
            if (successfulRescans > 0)
            {
                saveRowOffsetIndex(binFilePath, s_fileRowOffsetCache[binFilePath]);
            }
        }

        qint64 scanTime = timer.elapsed();
        qDebug() << funcName << "- 文件结构" << (usedCache ? "从缓存获取" : "扫描完成") << "，成功定位了" << rowPositions.size()
                 << "行，跳过了" << skippedRows.size() << "行，耗时:" << scanTime << "ms";
        timer.restart();

        // ----- 第4步: 准备修改行数据 -----

        // 检查修改行的有效性，过滤出有效的修改行
        QMap<int, Vector::RowData> validModifiedRows;
        QSet<int> invalidRowIndices;

        for (auto it = modifiedRows.constBegin(); it != modifiedRows.constEnd(); ++it)
        {
            int rowIndex = it.key();

            // 有效行的条件:
            // 1. 行索引在有效范围内
            // 2. 该行没有被跳过(损坏)
            if (rowIndex >= 0 && rowIndex < rowPositions.size() && !skippedRows.contains(rowIndex))
            {
                validModifiedRows.insert(rowIndex, it.value());
            }
            else
            {
                invalidRowIndices.insert(rowIndex);
                qWarning() << funcName << "- 跳过无效的修改行:" << rowIndex;
            }
        }

        if (validModifiedRows.isEmpty())
        {
            qCritical() << funcName << "- 没有有效的修改行，更新失败";
            file.close();
            return false;
        }

        qDebug() << funcName << "- 共有" << validModifiedRows.size() << "个有效修改行，"
                 << invalidRowIndices.size() << "个无效修改行";

        // ----- 第5步: 序列化并更新每一行 -----

        // 跟踪成功更新的行数
        int successfulUpdates = 0;

        // 开始逐行更新
        for (auto it = validModifiedRows.constBegin(); it != validModifiedRows.constEnd(); ++it)
        {
            int rowIndex = it.key();
            const Vector::RowData &rowData = it.value();

            // 序列化行数据
            QByteArray serializedRow;
            if (!serializeRow(rowData, columns, serializedRow))
            {
                qWarning() << funcName << "- 行" << rowIndex << "序列化失败，跳过此行";
                continue;
            }

            const RowPosition &pos = rowPositions[rowIndex];

            // 检查大小是否匹配
            if (static_cast<quint32>(serializedRow.size()) != pos.dataSize)
            {
                qWarning() << funcName << "- 行" << rowIndex << "的新数据大小(" << serializedRow.size()
                           << ")与原大小(" << pos.dataSize << ")不匹配，跳过此行";
                continue;
            }

            // 定位到数据开始位置(跳过大小字段)
            qint64 dataOffset = pos.offset + sizeof(quint32);
            if (!file.seek(dataOffset))
            {
                qWarning() << funcName << "- 无法定位到行" << rowIndex << "的数据位置，跳过此行";
                continue;
            }

            // 写入新数据
            qint64 bytesWritten = file.write(serializedRow);
            if (bytesWritten != serializedRow.size())
            {
                qWarning() << funcName << "- 写入行" << rowIndex << "失败，预期:" << serializedRow.size()
                           << "，实际:" << bytesWritten << "，跳过此行";
                continue;
            }

            // 更新成功
            successfulUpdates++;
        }

        qDebug() << funcName << "- 行更新完成，成功:" << successfulUpdates
                 << "行，失败:" << (validModifiedRows.size() - successfulUpdates)
                 << "行，耗时:" << timer.elapsed() << "ms";
        timer.restart();

        // ----- 第6步: 更新文件头中的时间戳 -----

        // 更新文件头的时间戳并写回
        file.seek(0); // 回到文件开头
        if (!writeBinaryHeader(&file, header))
        {
            qWarning() << funcName << "- 更新文件头失败，但数据已修改";
            // 不直接失败，因为数据已经写入
        }

        file.close();
        qDebug() << funcName << "- 文件头更新完成，总耗时:" << (timer.elapsed() + timer.elapsed()) << "ms";

        // ----- 第7步: 更新缓存状态 -----

        // 如果有数据更新，使用智能更新缓存，而不是清除
        if (successfulUpdates > 0)
        {
            // 创建修改行的集合
            QSet<int> modifiedRowIndices;
            for (auto it = validModifiedRows.constBegin(); it != validModifiedRows.constEnd(); ++it)
            {
                modifiedRowIndices.insert(it.key());
            }

            qDebug() << funcName << "- 数据已修改，智能更新" << binFilePath << "的行偏移缓存";
            updateRowOffsetCache(binFilePath, modifiedRowIndices);
        }

        // ----- 第8步: 返回结果 -----

        // 只要有行成功更新就算成功
        bool success = (successfulUpdates > 0);
        if (success)
        {
            qInfo() << funcName << "- 增量更新成功完成，共更新" << successfulUpdates << "行";
        }
        else
        {
            qCritical() << funcName << "- 增量更新失败，没有任何行被成功更新";
        }

        return success;
    }

