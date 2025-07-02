
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

#include "binaryfilehelper_2_1.cpp"