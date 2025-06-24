bool VectorDataHandler::insertVectorRows(int tableId, int startIndex, int rowCount, int timesetId,
                                         QTableWidget *dataTable, bool appendToEnd,
                                         const QList<QPair<int, QPair<QString, QPair<int, QString>>>> &selectedPins,
                                         QString &errorMessage)
{
    const QString funcName = "VectorDataHandler::insertVectorRows";
    m_cancelRequested.storeRelease(0);
    emit progressUpdated(0); // Start

    qDebug() << funcName << "- 开始插入向量行，表ID:" << tableId
             << "目标行数:" << rowCount << "源数据表行数:" << dataTable->rowCount()
             << "TimesetID:" << timesetId << "Append:" << appendToEnd << "StartIndex:" << startIndex;

    // 初始化缓存（如果尚未初始化）
    if (!m_cacheInitialized)
    {
        initializeCache();
    }

    errorMessage.clear();
    emit progressUpdated(2); // Initial checks passed

    // 1. 加载元数据和现有行数据 (如果存在)
    QString binFileName;
    QList<Vector::ColumnInfo> columns;
    int schemaVersion = 0;
    int existingRowCountFromMeta = 0;

    if (!loadVectorTableMeta(tableId, binFileName, columns, schemaVersion, existingRowCountFromMeta))
    {
        errorMessage = QString("无法加载表 %1 的元数据。").arg(tableId);
        qWarning() << funcName << "-" << errorMessage;
        emit progressUpdated(100); // End with error
        return false;
    }
    qDebug() << funcName << "- 元数据加载成功. BinFile:" << binFileName << "SchemaVersion:" << schemaVersion << "Columns:" << columns.size() << "ExistingMetaRows:" << existingRowCountFromMeta;
    emit progressUpdated(5);

    if (columns.isEmpty())
    {
        errorMessage = QString("表 %1 没有列配置信息，无法插入数据.").arg(tableId);
        qWarning() << funcName << "-" << errorMessage;
        emit progressUpdated(100); // End with error
        return false;
    }

    QString absoluteBinFilePath;
    QString resolveError;
    absoluteBinFilePath = resolveBinaryFilePath(tableId, resolveError);
    if (absoluteBinFilePath.isEmpty())
    {
        errorMessage = QString("无法解析表 %1 的二进制文件路径: %2").arg(tableId).arg(resolveError);
        qWarning() << funcName << "-" << errorMessage;
        emit progressUpdated(100); // End with error
        return false;
    }
    qDebug() << funcName << "- 二进制文件绝对路径:" << absoluteBinFilePath;
    emit progressUpdated(8);

    QFileInfo binFileInfo(absoluteBinFilePath);
    QDir binDir = binFileInfo.dir();
    if (!binDir.exists())
    {
        qInfo() << funcName << "- 目标二进制目录不存在，尝试创建:" << binDir.absolutePath();
        if (!binDir.mkpath("."))
        {
            errorMessage = QString("无法创建目标二进制目录: %1").arg(binDir.absolutePath());
            qWarning() << funcName << "-" << errorMessage;
            emit progressUpdated(100); // End with error
            return false;
        }
    }
    emit progressUpdated(10);

    // 2. 检查和验证行数据
    int sourceDataRowCount = dataTable->rowCount();
    if (sourceDataRowCount == 0 && rowCount > 0)
    {
        errorMessage = "源数据表为空，但请求插入多于0行。";
        qWarning() << funcName << "-" << errorMessage;
        emit progressUpdated(100); // End with error
        return false;
    }

    if (rowCount < 0)
    {
        errorMessage = "请求的总行数不能为负。";
        qWarning() << funcName << "-" << errorMessage;
        emit progressUpdated(100); // End with error
        return false;
    }

    int repeatTimes = 1;
    if (sourceDataRowCount > 0)
    {
        if (rowCount % sourceDataRowCount != 0 && rowCount > sourceDataRowCount)
        {
            errorMessage = "请求的总行数必须是源数据表行数的整数倍 (如果大于源数据表行数)。";
            qWarning() << funcName << "-" << errorMessage;
            emit progressUpdated(100); // End with error
            return false;
        }
        if (rowCount == 0) // Special case: insert 0 of the pattern means 0 repeats
            repeatTimes = 0;
        else
            repeatTimes = rowCount / sourceDataRowCount;
    }
    else if (rowCount > 0 && sourceDataRowCount == 0) // Should have been caught by earlier check
    {
        emit progressUpdated(100); // End with error
        return false;
    }
    else // rowCount == 0 and sourceDataRowCount == 0
    {
        repeatTimes = 0;
    }

    qDebug() << funcName << "- 计算重复次数:" << repeatTimes << " (基于请求总行数 " << rowCount << " 和源数据表行数 " << sourceDataRowCount << ")";

    if (dataTable->columnCount() != selectedPins.size() && sourceDataRowCount > 0)
    {
        errorMessage = QString("对话框提供的列数 (%1) 与选中管脚数 (%2) 不匹配。").arg(dataTable->columnCount()).arg(selectedPins.size());
        qWarning() << funcName << "-" << errorMessage;
        emit progressUpdated(100); // End with error
        return false;
    }

    emit progressUpdated(15);

    // 3. 创建二进制文件或读取已有数据
    QList<Vector::RowData> existingRows;

    // 仅在非追加模式下读取现有数据
    if (!appendToEnd && QFile::exists(absoluteBinFilePath))
    {
        qDebug() << funcName << "- 非追加模式，文件存在，尝试加载现有数据:" << absoluteBinFilePath;

        // 确定是否需要读取整个文件
        if (startIndex >= 0 && startIndex <= existingRowCountFromMeta)
        {
            if (!Persistence::BinaryFileHelper::readAllRowsFromBinary(absoluteBinFilePath, columns, schemaVersion, existingRows))
            {
                qWarning() << funcName << "- 无法从现有二进制文件读取行数据. File:" << absoluteBinFilePath << ". 将作为空文件处理。";
                existingRows.clear();
            }
            else
            {
                qDebug() << funcName << "- 从二进制文件成功加载" << existingRows.size() << "行现有数据.";
            }
        }
    }
    else if (appendToEnd && QFile::exists(absoluteBinFilePath))
    {
        // 在追加模式下，只需要确认文件存在并获取行数
        QFile file(absoluteBinFilePath);
        if (file.open(QIODevice::ReadOnly))
        {
            BinaryFileHeader header;
            if (Persistence::BinaryFileHelper::readBinaryHeader(&file, header))
            {
                existingRowCountFromMeta = header.row_count_in_file;
                qDebug() << funcName << "- 追加模式: 从文件头获取现有行数:" << existingRowCountFromMeta;
            }
            file.close();
        }
    }
    else
    {
        qDebug() << funcName << "- 二进制文件不存在:" << absoluteBinFilePath << ". 将创建新文件.";
    }

    emit progressUpdated(20); // 文件准备完成

    // 4. 确定实际插入位置
    int actualInsertionIndex = appendToEnd ? existingRowCountFromMeta : startIndex;
    if (actualInsertionIndex < 0)
        actualInsertionIndex = 0;
    if (actualInsertionIndex > existingRowCountFromMeta)
        actualInsertionIndex = existingRowCountFromMeta;

    // 在非追加模式下，记录插入位置，确保正确处理
    if (!appendToEnd)
    {
        qDebug() << funcName << "- 非追加模式，插入位置:" << actualInsertionIndex
                 << "，现有行数:" << existingRowCountFromMeta
                 << "，新增行数:" << rowCount;
    }

    // 创建按列名检索的映射，提前准备好提高效率
    QMap<QString, int> columnNameMap;
    for (int i = 0; i < columns.size(); ++i)
    {
        columnNameMap[columns[i].name] = i;
        qDebug() << funcName << " - 列映射: 名称=" << columns[i].name << "，索引=" << i << "，类型=" << columns[i].original_type_str;
    }

    // 5. 批量处理数据写入，避免一次性生成大量数据占用内存
    // 增大批处理大小，提高吞吐量
    const int BATCH_SIZE = 50000; // 增大批处理大小，提高性能

    // 对于超过100万行的大数据，做特殊的进度上报处理
    bool isLargeDataset = (rowCount > 1000000);
    int progressStart = 25;
    int progressEnd = isLargeDataset ? 95 : 90; // 大数据集预留更多进度给文件写入操作

    // 计算总批次数
    int totalBatches = (rowCount + BATCH_SIZE - 1) / BATCH_SIZE; // 向上取整
    int currentBatch = 0;
    int totalRowsProcessed = 0;

    qDebug() << funcName << "- 开始批量处理，总批次:" << totalBatches << "，每批大小:" << BATCH_SIZE;

    // 创建临时文件，用于高效写入
    QString tempFilePath = absoluteBinFilePath + ".tmp";
    QFile tempFile(tempFilePath);

    // 预先创建一个模板行，存储标准列的默认值，减少重复计算
    Vector::RowData templateRow;
    // templateRow.resize(columns.size()); // QT6写法
    int targetSize2 = columns.size(); // QT5写法开始
    while (templateRow.size() < targetSize2)
    {
        templateRow.append(QVariant());
    }
    // QT5写法结束

    // 初始化模板行的标准列
    if (columnNameMap.contains("Label"))
        templateRow[columnNameMap["Label"]] = "";

    if (columnNameMap.contains("Instruction"))
        templateRow[columnNameMap["Instruction"]] = 1; // 使用默认指令ID

    if (columnNameMap.contains("TimeSet"))
        templateRow[columnNameMap["TimeSet"]] = timesetId;

    if (columnNameMap.contains("Capture"))
        templateRow[columnNameMap["Capture"]] = "N"; // 默认不捕获

    if (columnNameMap.contains("Ext"))
        templateRow[columnNameMap["Ext"]] = "";

    if (columnNameMap.contains("Comment"))
        templateRow[columnNameMap["Comment"]] = "";

    // 创建数据临时缓存，用于存储从UITable读取的数据
    QVector<QVector<QString>> pinDataCache;
    pinDataCache.resize(sourceDataRowCount);
    for (int i = 0; i < sourceDataRowCount; i++)
    {
        pinDataCache[i].resize(selectedPins.size());

        // 读取界面表格中的数据并缓存
        for (int pinColIdx = 0; pinColIdx < selectedPins.size(); ++pinColIdx)
        {
            PinValueLineEdit *pinEdit = qobject_cast<PinValueLineEdit *>(dataTable->cellWidget(i, pinColIdx));
            QString pinValueStr = pinEdit ? pinEdit->text() : "X";
            if (pinValueStr.isEmpty())
                pinValueStr = "X";
            pinDataCache[i][pinColIdx] = pinValueStr;
        }
    }

    // 优化：为非追加模式，预先处理现有数据和插入点数据
    BinaryFileHeader header;
    QDataStream out;
    bool fileOpenSuccess = false;

    if (!appendToEnd && !existingRows.isEmpty())
    {
        // 非追加模式，需要处理前部分和后部分的现有数据
        if (!tempFile.open(QIODevice::WriteOnly))
        {
            errorMessage = QString("无法创建临时文件: %1, 错误: %2").arg(tempFilePath).arg(tempFile.errorString());
            qWarning() << funcName << "-" << errorMessage;
            emit progressUpdated(100);
            return false;
        }

        // 准备文件头
        header.magic_number = VBIN_MAGIC_NUMBER;
        header.file_format_version = Persistence::CURRENT_FILE_FORMAT_VERSION;
        header.data_schema_version = schemaVersion;
        header.row_count_in_file = existingRows.size() + rowCount;
        header.column_count_in_file = static_cast<uint32_t>(columns.size());
        header.timestamp_created = static_cast<uint64_t>(QDateTime::currentSecsSinceEpoch());
        header.timestamp_updated = header.timestamp_created;
        header.compression_type = 0;
        memset(header.reserved_bytes, 0, sizeof(header.reserved_bytes));

        // 写入文件头
        if (!Persistence::BinaryFileHelper::writeBinaryHeader(&tempFile, header))
        {
            errorMessage = "无法写入临时文件头";
            qWarning() << funcName << "-" << errorMessage;
            tempFile.close();
            QFile::remove(tempFilePath);
            emit progressUpdated(100);
            return false;
        }

        // 写入插入点之前的现有行
        out.setDevice(&tempFile);
        out.setByteOrder(QDataStream::LittleEndian);

        // 确保actualInsertionIndex不超出现有行的范围
        if (actualInsertionIndex > existingRows.size())
        {
            actualInsertionIndex = existingRows.size();
        }

        // 写入插入点之前的现有行
        for (int i = 0; i < actualInsertionIndex && i < existingRows.size(); i++)
        {
            QByteArray serializedRowData;
            if (!Persistence::BinaryFileHelper::serializeRow(existingRows[i], columns, serializedRowData))
            {
                errorMessage = "序列化现有行数据失败";
                tempFile.close();
                QFile::remove(tempFilePath);
                emit progressUpdated(100);
                return false;
            }

            quint32 rowBlockSize = static_cast<quint32>(serializedRowData.size());
            out << rowBlockSize;
            qint64 bytesWritten = out.writeRawData(serializedRowData.constData(), rowBlockSize);

            if (bytesWritten != rowBlockSize || out.status() != QDataStream::Ok)
            {
                errorMessage = "写入现有行数据失败";
                tempFile.close();
                QFile::remove(tempFilePath);
                emit progressUpdated(100);
                return false;
            }
        }

        fileOpenSuccess = true;
    }
    else if (appendToEnd)
    {
        // 追加模式，直接打开现有文件进行追加或创建新文件
        if (QFile::exists(absoluteBinFilePath))
        {
            bool validFileHeader = false;
            QFile origFile(absoluteBinFilePath);

            // 首先尝试只读方式打开文件验证文件头
            if (origFile.open(QIODevice::ReadOnly))
            {
                qDebug() << funcName << "- 尝试读取现有文件头进行验证";
                if (Persistence::BinaryFileHelper::readBinaryHeader(&origFile, header))
                {
                    validFileHeader = true;
                    qDebug() << funcName << "- 成功验证现有文件头，行数:" << header.row_count_in_file;
                }
                else
                {
                    qWarning() << funcName << "- 现有文件头无效或损坏，将创建新文件";
                }
                origFile.close();
            }

            // 如果文件头有效，创建临时文件并复制原文件内容
            if (validFileHeader)
            {
                // 创建临时文件并复制原文件内容
                if (!QFile::copy(absoluteBinFilePath, tempFilePath))
                {
                    errorMessage = QString("无法复制现有文件到临时文件: %1 -> %2").arg(absoluteBinFilePath).arg(tempFilePath);
                    qWarning() << funcName << "-" << errorMessage;
                    emit progressUpdated(100);
                    return false;
                }

                // 打开临时文件进行追加
                if (!tempFile.open(QIODevice::ReadWrite))
                {
                    errorMessage = QString("无法打开临时文件进行追加: %1, 错误: %2").arg(tempFilePath).arg(tempFile.errorString());
                    qWarning() << funcName << "-" << errorMessage;
                    QFile::remove(tempFilePath);
                    emit progressUpdated(100);
                    return false;
                }

                // 更新文件头信息
                header.row_count_in_file += rowCount;
                header.timestamp_updated = static_cast<uint64_t>(QDateTime::currentSecsSinceEpoch());

                // 重写文件头
                tempFile.seek(0);
                if (!Persistence::BinaryFileHelper::writeBinaryHeader(&tempFile, header))
                {
                    errorMessage = "无法更新临时文件头";
                    qWarning() << funcName << "-" << errorMessage;
                    tempFile.close();
                    QFile::remove(tempFilePath);
                    emit progressUpdated(100);
                    return false;
                }

                // 移动到文件末尾进行追加
                tempFile.seek(tempFile.size());
                out.setDevice(&tempFile);
                out.setByteOrder(QDataStream::LittleEndian);
                fileOpenSuccess = true;
                qDebug() << funcName << "- 成功准备临时文件进行追加，当前位置:" << tempFile.pos();
            }
            else
            {
                // 文件存在但头部无效，作为新文件处理
                qDebug() << funcName << "- 文件存在但头部无效，作为新文件处理";
                if (!tempFile.open(QIODevice::WriteOnly))
                {
                    errorMessage = QString("无法创建临时文件: %1, 错误: %2").arg(tempFilePath).arg(tempFile.errorString());
                    qWarning() << funcName << "-" << errorMessage;
                    emit progressUpdated(100);
                    return false;
                }

                // 准备文件头
                header.magic_number = VBIN_MAGIC_NUMBER;
                header.file_format_version = Persistence::CURRENT_FILE_FORMAT_VERSION;
                header.data_schema_version = schemaVersion;
                header.row_count_in_file = rowCount;
                header.column_count_in_file = static_cast<uint32_t>(columns.size());
                header.timestamp_created = static_cast<uint64_t>(QDateTime::currentSecsSinceEpoch());
                header.timestamp_updated = header.timestamp_created;
                header.compression_type = 0;
                memset(header.reserved_bytes, 0, sizeof(header.reserved_bytes));

                // 写入文件头
                if (!Persistence::BinaryFileHelper::writeBinaryHeader(&tempFile, header))
                {
                    errorMessage = "无法写入临时文件头";
                    qWarning() << funcName << "-" << errorMessage;
                    tempFile.close();
                    QFile::remove(tempFilePath);
                    emit progressUpdated(100);
                    return false;
                }

                out.setDevice(&tempFile);
                out.setByteOrder(QDataStream::LittleEndian);
                fileOpenSuccess = true;
            }
        }
        else
        {
            // 文件不存在，创建新文件
            if (!tempFile.open(QIODevice::WriteOnly))
            {
                errorMessage = QString("无法创建新文件: %1, 错误: %2").arg(tempFilePath).arg(tempFile.errorString());
                qWarning() << funcName << "-" << errorMessage;
                emit progressUpdated(100);
                return false;
            }

            // 准备文件头
            header.magic_number = VBIN_MAGIC_NUMBER;
            header.file_format_version = Persistence::CURRENT_FILE_FORMAT_VERSION;
            header.data_schema_version = schemaVersion;
            header.row_count_in_file = rowCount;
            header.column_count_in_file = static_cast<uint32_t>(columns.size());
            header.timestamp_created = static_cast<uint64_t>(QDateTime::currentSecsSinceEpoch());
            header.timestamp_updated = header.timestamp_created;
            header.compression_type = 0;
            memset(header.reserved_bytes, 0, sizeof(header.reserved_bytes));

            // 写入文件头
            if (!Persistence::BinaryFileHelper::writeBinaryHeader(&tempFile, header))
            {
                errorMessage = "无法写入新文件头";
                qWarning() << funcName << "-" << errorMessage;
                tempFile.close();
                QFile::remove(tempFilePath);
                emit progressUpdated(100);
                return false;
            }

            out.setDevice(&tempFile);
            out.setByteOrder(QDataStream::LittleEndian);
            fileOpenSuccess = true;
        }
    }
    else
    {
        // 非追加模式，创建新文件
        if (!tempFile.open(QIODevice::WriteOnly))
        {
            errorMessage = QString("无法创建新文件: %1, 错误: %2").arg(tempFilePath).arg(tempFile.errorString());
            qWarning() << funcName << "-" << errorMessage;
            emit progressUpdated(100);
            return false;
        }

        // 准备文件头
        header.magic_number = VBIN_MAGIC_NUMBER;
        header.file_format_version = Persistence::CURRENT_FILE_FORMAT_VERSION;
        header.data_schema_version = schemaVersion;
        header.row_count_in_file = rowCount;
        header.column_count_in_file = static_cast<uint32_t>(columns.size());
        header.timestamp_created = static_cast<uint64_t>(QDateTime::currentSecsSinceEpoch());
        header.timestamp_updated = header.timestamp_created;
        header.compression_type = 0;
        memset(header.reserved_bytes, 0, sizeof(header.reserved_bytes));

        // 写入文件头
        if (!Persistence::BinaryFileHelper::writeBinaryHeader(&tempFile, header))
        {
            errorMessage = "无法写入新文件头";
            qWarning() << funcName << "-" << errorMessage;
            tempFile.close();
            QFile::remove(tempFilePath);
            emit progressUpdated(100);
            return false;
        }

        out.setDevice(&tempFile);
        out.setByteOrder(QDataStream::LittleEndian);
        fileOpenSuccess = true;
    }

    if (!fileOpenSuccess)
    {
        errorMessage = "无法准备文件进行写入";
        qWarning() << funcName << "-" << errorMessage;
        emit progressUpdated(100);
        return false;
    }

    // 预先生成并缓存所有可能的行数据
    QList<QByteArray> serializedRowCache;
    if (sourceDataRowCount > 0 && sourceDataRowCount <= 1000) // 只对较小的模式进行缓存
    {
        qDebug() << funcName << "- 预生成行数据缓存";
        // serializedRowCache.resize(sourceDataRowCount); // QT6写法
        int targetSize3 = sourceDataRowCount; // QT5写法开始
        while (serializedRowCache.size() < targetSize3)
        {
            serializedRowCache.append(QByteArray());
        }
        // QT5写法结束

        for (int i = 0; i < sourceDataRowCount; ++i)
        {
            // 从模板行创建新行
            Vector::RowData newRow = templateRow;

            // 设置管脚列的值
            for (int pinColIdx = 0; pinColIdx < selectedPins.size(); ++pinColIdx)
            {
                const auto &pinSelection = selectedPins[pinColIdx];
                const QString &pinName = pinSelection.second.first;
                if (!columnNameMap.contains(pinName))
                    continue;

                int targetColIdx = columnNameMap[pinName];
                newRow[targetColIdx] = pinDataCache[i][pinColIdx];
            }

            // 序列化行数据并缓存
            QByteArray serializedRowData;
            if (!Persistence::BinaryFileHelper::serializeRow(newRow, columns, serializedRowData))
            {
                errorMessage = "序列化行数据失败";
                tempFile.close();
                QFile::remove(tempFilePath);
                emit progressUpdated(100);
                return false;
            }

            serializedRowCache[i] = serializedRowData;
        }
    }

    // 批量生成和保存数据
    while (totalRowsProcessed < rowCount && !m_cancelRequested.loadAcquire())
    {
        // 计算当前批次要处理的行数
        int currentBatchSize = qMin(BATCH_SIZE, rowCount - totalRowsProcessed);
        qDebug() << funcName << "- 处理批次 " << (currentBatch + 1) << "/" << totalBatches << "，当前批次大小:" << currentBatchSize;

        // 使用缓存的预生成数据或实时生成
        bool useCache = sourceDataRowCount > 0 && sourceDataRowCount <= 1000;

        // 优化：批量准备所有序列化数据，减少IO次数
        QVector<QByteArray> batchSerializedData;
        batchSerializedData.reserve(currentBatchSize);
        quint64 totalBatchDataSize = 0;

        // 先准备当前批次所有行的序列化数据
        for (int i = 0; i < currentBatchSize; ++i)
        {
            // 计算当前行在源数据表中的索引
            int srcRowIdx = (totalRowsProcessed + i) % sourceDataRowCount;
            QByteArray serializedData;

            if (useCache && !serializedRowCache.isEmpty())
            {
                // 使用缓存的序列化数据
                serializedData = serializedRowCache[srcRowIdx];
            }
            else
            {
                // 从模板行创建新行
                Vector::RowData newRow = templateRow;

                // 设置管脚列的值
                for (int pinColIdx = 0; pinColIdx < selectedPins.size(); ++pinColIdx)
                {
                    const auto &pinSelection = selectedPins[pinColIdx];
                    const QString &pinName = pinSelection.second.first;
                    if (!columnNameMap.contains(pinName))
                        continue;

                    int targetColIdx = columnNameMap[pinName];
                    newRow[targetColIdx] = pinDataCache[srcRowIdx][pinColIdx];
                }

                // 序列化行数据
                if (!Persistence::BinaryFileHelper::serializeRow(newRow, columns, serializedData))
                {
                    errorMessage = "序列化行数据失败";
                    tempFile.close();
                    QFile::remove(tempFilePath);
                    emit progressUpdated(100);
                    return false;
                }
            }

            totalBatchDataSize += sizeof(quint32) + serializedData.size(); // 行大小(4字节) + 行数据
            batchSerializedData.append(serializedData);
        }

        // 一次性写入当前批次所有数据
        for (int i = 0; i < currentBatchSize; ++i)
        {
            const QByteArray &serializedData = batchSerializedData[i];
            quint32 rowBlockSize = static_cast<quint32>(serializedData.size());
            out << rowBlockSize;
            qint64 bytesWritten = out.writeRawData(serializedData.constData(), rowBlockSize);

            if (bytesWritten != rowBlockSize || out.status() != QDataStream::Ok)
            {
                errorMessage = "写入行数据失败";
                tempFile.close();
                QFile::remove(tempFilePath);
                emit progressUpdated(100);
                return false;
            }
        }

        // 定期刷新文件，避免缓冲区溢出，但不要太频繁影响性能
        if (currentBatch % 10 == 0) // 减少刷新频率，提高写入性能
        {
            tempFile.flush();
            qDebug() << funcName << "- 已刷新文件，当前已处理" << totalRowsProcessed + currentBatchSize << "行";
        }

        // 更新处理进度
        totalRowsProcessed += currentBatchSize;
        currentBatch++;

        // 更新进度条（非线性进度，大数据进度增长更慢）
        double progress = progressStart + ((double)totalRowsProcessed / rowCount) * (progressEnd - progressStart);
        emit progressUpdated(static_cast<int>(progress));

        // 每批次处理完后，主动让出CPU时间，减轻UI冻结，但在大数据集情况下减少让出频率
        if (currentBatch % (isLargeDataset ? 20 : 10) == 0) // 减少让出CPU频率，提高处理速度
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    // 在非追加模式下，还需要写入插入点之后的现有数据
    if (!appendToEnd && actualInsertionIndex < existingRows.size())
    {
        qDebug() << funcName << "- 写入插入点之后的现有数据，从索引" << actualInsertionIndex << "开始，共"
                 << (existingRows.size() - actualInsertionIndex) << "行";

        for (int i = actualInsertionIndex; i < existingRows.size(); i++)
        {
            QByteArray serializedRowData;
            if (!Persistence::BinaryFileHelper::serializeRow(existingRows[i], columns, serializedRowData))
            {
                errorMessage = "序列化插入点之后的行数据失败";
                tempFile.close();
                QFile::remove(tempFilePath);
                emit progressUpdated(100);
                return false;
            }

            quint32 rowBlockSize = static_cast<quint32>(serializedRowData.size());
            out << rowBlockSize;
            qint64 bytesWritten = out.writeRawData(serializedRowData.constData(), rowBlockSize);

            if (bytesWritten != rowBlockSize || out.status() != QDataStream::Ok)
            {
                errorMessage = "写入插入点之后的行数据失败";
                tempFile.close();
                QFile::remove(tempFilePath);
                emit progressUpdated(100);
                return false;
            }
        }
    }

    // 刷新文件并关闭
    tempFile.flush();
    tempFile.close();

    // 如果用户取消了操作
    if (m_cancelRequested.loadAcquire())
    {
        qDebug() << funcName << "- 操作被用户取消。";
        errorMessage = "操作被用户取消。";
        QFile::remove(tempFilePath);
        emit progressUpdated(100);
        return false;
    }

    // 替换原文件
    if (QFile::exists(absoluteBinFilePath))
    {
        if (!QFile::remove(absoluteBinFilePath))
        {
            errorMessage = QString("无法删除原文件: %1").arg(absoluteBinFilePath);
            qWarning() << funcName << "-" << errorMessage;
            QFile::remove(tempFilePath);
            emit progressUpdated(100);
            return false;
        }
    }

    if (!QFile::rename(tempFilePath, absoluteBinFilePath))
    {
        errorMessage = QString("无法重命名临时文件: %1 -> %2").arg(tempFilePath).arg(absoluteBinFilePath);
        qWarning() << funcName << "-" << errorMessage;
        QFile::remove(tempFilePath);
        emit progressUpdated(100);
        return false;
    }

    emit progressUpdated(isLargeDataset ? 95 : 90); // 文件写入完成

    // 更新数据库中的行数记录
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.transaction())
    {
        errorMessage = "无法开始数据库事务以更新主记录。";
        qWarning() << funcName << "-" << errorMessage;
        emit progressUpdated(100);
        return false;
    }

    // 计算最终的行数
    int finalRowCount;
    if (appendToEnd)
    {
        finalRowCount = existingRowCountFromMeta + rowCount;
    }
    else
    {
        // 非追加模式，最终行数 = 原有行数 + 新增行数
        finalRowCount = existingRows.size() + rowCount;
    }

    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE VectorTableMasterRecord SET row_count = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?");
    updateQuery.addBindValue(finalRowCount);
    updateQuery.addBindValue(tableId);

    if (!updateQuery.exec())
    {
        errorMessage = QString("更新数据库中的行数记录失败: %1").arg(updateQuery.lastError().text());
        qWarning() << funcName << "-" << errorMessage;
        db.rollback();
        emit progressUpdated(100);
        return false;
    }

    if (!db.commit())
    {
        errorMessage = "提交数据库事务失败。";
        qWarning() << funcName << "-" << errorMessage;
        db.rollback();
        emit progressUpdated(100);
        return false;
    }

    qDebug() << funcName << "- 数据库元数据行数已更新为:" << finalRowCount << " for table ID:" << tableId;
    emit progressUpdated(100); // 操作完成
    qDebug() << funcName << "- 向量行数据操作成功完成。";
    return true;
}

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


