bool BinaryFileHelper::serializeRow(const Vector::RowData &rowData, const QList<Vector::ColumnInfo> &columns, QByteArray &serializedRow)
{
    // 使用静态计数器控制日志频率
    static int serializeCounter = 0;
    static const int LOG_INTERVAL = 1000; // 每1000次输出一次日志

    const QString funcName = "BinaryFileHelper::serializeRow";

    // 只在特定间隔输出日志，减少日志量
    if (++serializeCounter % LOG_INTERVAL == 1 || serializeCounter <= 3)
    {
        qDebug() << funcName << " - 开始序列化行数据, 列数:" << columns.size()
                 << ", 计数器:" << serializeCounter;
    }

    serializedRow.clear();

    if (rowData.size() != columns.size())
    {
        qWarning() << funcName << " - 列数与数据项数不一致!";
        return false;
    }

    QByteArray tempData;
    QDataStream out(&tempData, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    for (int i = 0; i < columns.size(); ++i)
    {
        const auto &col = columns[i];
        const QVariant &val = rowData[i];

        // 使用列名称和类型获取该字段的固定长度
        int fieldLength = getFixedLengthForType(col.type, col.name);
        // 移除循环中的日志
        // qDebug() << funcName << " - 字段固定长度:" << fieldLength;

        switch (col.type)
        {
        case Vector::ColumnDataType::TEXT:
        {
            QString text = val.toString();
            // 确保文本不超过字段的固定长度限制
            if (text.toUtf8().size() > fieldLength - 4) // 减去4字节用于存储长度字段
            {
                qWarning() << funcName << " - 文本长度超出限制，将被截断!";
                // 截断文本，确保UTF-8编码后不超过限制
                while (text.toUtf8().size() > fieldLength - 4)
                {
                    text.chop(1);
                }
            }

            // 写入文本的实际长度
            qint32 textLength = text.toUtf8().size();
            out << textLength;

            // 写入文本内容
            out.writeRawData(text.toUtf8().constData(), textLength);

            // 填充剩余空间
            int paddingNeeded = fieldLength - 4 - textLength; // 4字节长度字段
            if (paddingNeeded > 0)
            {
                QByteArray padding(paddingNeeded, '\0');
                out.writeRawData(padding.constData(), paddingNeeded);
            }
            break;
        }
        case Vector::ColumnDataType::PIN_STATE_ID:
        {
            // 管脚状态总是1个字符
            QString pinValue;
            if (val.isNull() || !val.isValid() || val.toString().isEmpty())
            {
                // 使用默认值'X'
                pinValue = "X";
                // 移除循环中的日志
                // qDebug() << funcName << " - 管脚状态列为空或无效，使用默认值'X'，列名:" << col.name;
            }
            else
            {
                pinValue = val.toString().left(1).toUpper(); // 只取第一个字符并转为大写
                // 验证是否为有效的管脚状态
                if (pinValue != "0" && pinValue != "1" && pinValue != "X" && pinValue != "L" &&
                    pinValue != "H" && pinValue != "S" && pinValue != "V" && pinValue != "M")
                {
                    pinValue = "X"; // 无效的状态使用X
                    // 移除循环中的日志
                    // qDebug() << funcName << " - 管脚状态值无效，使用默认值'X'，列名:" << col.name;
                }
            }
            // 直接写入字符
            char pinChar = pinValue.at(0).toLatin1();
            out.writeRawData(&pinChar, PIN_STATE_FIELD_MAX_LENGTH);
            break;
        }
        case Vector::ColumnDataType::INTEGER:
        case Vector::ColumnDataType::INSTRUCTION_ID:
        case Vector::ColumnDataType::TIMESET_ID:
        {
            int intValue = val.toInt();
            out << quint32(intValue);
            break;
        }
        case Vector::ColumnDataType::REAL:
        {
            double realValue = val.toDouble();
            out << realValue;
            break;
        }
        case Vector::ColumnDataType::BOOLEAN:
        {
            quint8 boolValue = val.toBool() ? 1 : 0;
            out << boolValue;
            break;
        }
        case Vector::ColumnDataType::JSON_PROPERTIES:
        {
            QString jsonStr = QString(QJsonDocument(val.toJsonObject()).toJson(QJsonDocument::Compact));
            if (jsonStr.length() > JSON_PROPERTIES_MAX_LENGTH / 2)
            {
                qWarning() << funcName << " - JSON属性超出最大长度限制! 原长度:" << jsonStr.length() << ", 最大允许:" << JSON_PROPERTIES_MAX_LENGTH / 2;
                // 这里我们选择截断JSON，但在实际应用中可能需要更优雅的处理
                jsonStr = jsonStr.left(JSON_PROPERTIES_MAX_LENGTH / 2);
            }
            out << quint32(jsonStr.length()); // 先写入实际长度
            out.writeRawData(jsonStr.toUtf8().constData(), jsonStr.toUtf8().size());
            // 填充剩余空间
            int padding = JSON_PROPERTIES_MAX_LENGTH - jsonStr.toUtf8().size() - sizeof(quint32);
            if (padding > 0)
            {
                QByteArray paddingData(padding, '\0');
                out.writeRawData(paddingData.constData(), paddingData.size());
            }
            break;
        }
        default:
            qWarning() << funcName << " - 不支持的列类型:" << static_cast<int>(col.type) << ", 列名:" << col.name;
            return false;
        }

        if (out.status() != QDataStream::Ok)
        {
            qWarning() << funcName << " - QDataStream 写入失败, 列:" << col.name;
            return false;
        }
    }

    // 将临时数据复制到输出
    serializedRow = tempData;

    // 仅在特定间隔输出完成日志
    if (serializeCounter % LOG_INTERVAL == 1 || serializeCounter <= 3)
    {
        qDebug() << funcName << " - 序列化完成, 字节长度:" << serializedRow.size()
                 << ", 计数器:" << serializeCounter;
    }
    return true;
}

bool BinaryFileHelper::deserializeRow(const QByteArray &bytes, const QList<Vector::ColumnInfo> &columns, int fileVersion, Vector::RowData &rowData)
{
    // 使用静态计数器控制日志频率
    static int deserializeCounter = 0;
    static const int LOG_INTERVAL = 1000; // 每1000次输出一次日志

    const QString funcName = "BinaryFileHelper::deserializeRow";
    // 只在特定间隔输出日志，减少日志量
    if (++deserializeCounter % LOG_INTERVAL == 1 || deserializeCounter <= 3)
    {
        // 只输出前几行和每LOG_INTERVAL行的日志
        qDebug() << funcName << " - 开始反序列化二进制数据，字节数:" << bytes.size()
                 << ", 列数:" << columns.size() << ", 文件版本:" << fileVersion
                 << ", 计数器:" << deserializeCounter;
    }

    rowData.clear();

    if (bytes.isEmpty())
    {
        qWarning() << funcName << " - 输入的二进制数据为空!";
        return false;
    }

    QDataStream in(bytes);
    in.setByteOrder(QDataStream::LittleEndian);

    for (const auto &col : columns)
    {
        // 获取该字段的固定长度
        int fieldLength = getFixedLengthForType(col.type, col.name);
        // 移除列级别的处理日志，减少日志输出
        // qDebug().nospace() << funcName << " - 处理列: " << col.name << ", 类型: " << static_cast<int>(col.type) << ", 固定长度: " << fieldLength;

        QVariant value;
        // 使用固定长度格式
        switch (col.type)
        {
        case Vector::ColumnDataType::TEXT:
        {
            // 读取文本实际长度
            qint32 textLength;
            in >> textLength;

            // 添加绝对限制，防止分配过多内存
            const int TEXT_ABSOLUTE_MAX_LENGTH = 100000; // 100KB 绝对上限
            if (textLength < 0 || textLength > fieldLength - 4 || textLength > TEXT_ABSOLUTE_MAX_LENGTH)
            {
                qWarning() << funcName << " - 反序列化TEXT字段时长度无效或过大:" << textLength << ", 列名:" << col.name;
                textLength = qMin(qMin(textLength, fieldLength - 4), TEXT_ABSOLUTE_MAX_LENGTH);
                if (textLength < 0)
                    textLength = 0;
            }

            // 读取实际文本内容
            QByteArray textData(textLength, Qt::Uninitialized);
            in.readRawData(textData.data(), textLength);
            value = QString::fromUtf8(textData);

            // 跳过填充字节
            int paddingToSkip = fieldLength - 4 - textLength;
            if (paddingToSkip > 0)
            {
                in.skipRawData(paddingToSkip);
            }
            break;
        }
        case Vector::ColumnDataType::PIN_STATE_ID:
        {
            // 管脚状态是1个字符
            char pinState;
            in.readRawData(&pinState, PIN_STATE_FIELD_MAX_LENGTH);
            value = QString(QChar(pinState));
            break;
        }
        case Vector::ColumnDataType::INTEGER:
        case Vector::ColumnDataType::INSTRUCTION_ID:
        case Vector::ColumnDataType::TIMESET_ID:
        {
            quint32 intValue;
            in >> intValue;
            value = intValue;
            break;
        }
        case Vector::ColumnDataType::REAL:
        {
            double doubleValue;
            in >> doubleValue;
            value = doubleValue;
            break;
        }
        case Vector::ColumnDataType::BOOLEAN:
        {
            quint8 boolValue;
            in >> boolValue;
            value = boolValue;
            break;
        }
        case Vector::ColumnDataType::JSON_PROPERTIES:
        {
            // 读取JSON字符串长度
            qint32 jsonLength;
            in >> jsonLength;

            // 添加绝对限制，防止分配过多内存
            const int JSON_ABSOLUTE_MAX_LENGTH = 500000; // 500KB 绝对上限
            if (jsonLength < 0 || jsonLength > JSON_PROPERTIES_MAX_LENGTH - 4 || jsonLength > JSON_ABSOLUTE_MAX_LENGTH)
            {
                qWarning() << funcName << " - 反序列化JSON字段时长度无效或过大:" << jsonLength;
                jsonLength = qMin(qMin(jsonLength, JSON_PROPERTIES_MAX_LENGTH - 4), JSON_ABSOLUTE_MAX_LENGTH);
                if (jsonLength < 0)
                    jsonLength = 0;
            }

            // 读取JSON字符串
            QByteArray jsonData(jsonLength, Qt::Uninitialized);
            in.readRawData(jsonData.data(), jsonLength);
            QString jsonString = QString::fromUtf8(jsonData);
            QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());

            if (doc.isNull() || !doc.isObject())
            {
                qWarning() << funcName << " - 解析JSON数据失败，列名:" << col.name;
                value = QJsonObject();
            }
            else
            {
                value = doc.object();
            }

            // 跳过填充字节
            int paddingToSkip = JSON_PROPERTIES_MAX_LENGTH - 4 - jsonLength;
            if (paddingToSkip > 0)
            {
                in.skipRawData(paddingToSkip);
            }
            break;
        }
        default:
            qWarning() << funcName << " - 不支持的列类型:" << static_cast<int>(col.type) << ", 列名:" << col.name;
            // 使用默认空值
            value = QVariant(); // Default to a null QVariant
            // 为了健壮性，即使遇到未知类型，也尝试跳过预期的固定长度，如果fieldLength在此处有意义
            // 但由于这是固定长度模式，我们必须读取或跳过fieldLength字节以保持流的同步
            // 注意：如果getFixedLengthForType为未知类型返回0或负值，这里需要额外处理
            if (fieldLength > 0)
            {
                in.skipRawData(fieldLength); // 假设未知类型也占用了其定义的固定长度
            }
            else
            {
                // 如果没有固定长度或长度无效，这可能是一个严重错误，可能导致后续数据错位
                qWarning() << funcName << " - 未知类型的字段长度无效，可能导致数据解析错误，列名:" << col.name;
                // 无法安全跳过，返回错误
                return false;
            }
            break;
        }

        rowData.append(value);
        // 移除每列的详细解析日志
        // qDebug().nospace() << funcName << " - 解析列: " << col.name << ", 值: " << value;
    }

    if (in.status() != QDataStream::Ok)
    {
        qWarning() << funcName << " - 反序列化过程中发生错误，QDataStream状态:" << in.status();
        return false;
    }

    if (deserializeCounter % LOG_INTERVAL == 1 || deserializeCounter <= 3)
    {
        qDebug() << funcName << " - 反序列化完成, 字节数:" << rowData.size();
    }
    return true;
}

bool BinaryFileHelper::readAllRowsFromBinary(const QString &binFilePath, const QList<Vector::ColumnInfo> &columns,
                                             int schemaVersion, QList<QList<QVariant>> &rows,
                                             int masterRecordId)
{
    const QString funcName = "BinaryFileHelper::readAllRowsFromBinary";
    qDebug() << funcName << "- [Indexed Read Mode] Reading from:" << binFilePath << "for master ID:" << masterRecordId;

    rows.clear();

    QFile file(binFilePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << funcName << "- Error: Cannot open file for reading:" << file.errorString();
        return false;
    }

    // 1. Read and validate file header
    BinaryFileHeader header;
    if (!readBinaryHeader(&file, header))
    {
        qWarning() << funcName << "- Error: Failed to read or validate binary file header.";
        file.close();
        return false;
    }

    // 2. Query the row index from the database
    QList<QPair<qint64, qint64>> indexData; // List of <offset, size>
    QSqlQuery query(DatabaseManager::instance()->database());
    query.prepare("SELECT offset, size FROM VectorTableRowIndex WHERE master_record_id = ? AND is_active = 1 ORDER BY logical_row_order ASC");
    query.addBindValue(masterRecordId);

    if (!query.exec())
    {
        qWarning() << funcName << "- Error: Failed to query row index:" << query.lastError().text();
        file.close();
        return false;
    }

    while (query.next())
    {
        indexData.append({query.value(0).toLongLong(), query.value(1).toLongLong()});
    }
    qDebug() << funcName << "- Successfully fetched" << indexData.size() << "row indices from database.";

    // 3. Read data rows based on the index
    QElapsedTimer timer;
    timer.start();

    for (const auto &indexEntry : indexData)
    {
        qint64 offset = indexEntry.first;
        qint64 size = indexEntry.second;

        if (!file.seek(offset))
        {
            qWarning() << funcName << "- Error: Failed to seek to offset" << offset << ". File may be corrupt.";
            // Decide if we should stop or try to continue
            continue;
        }

        QByteArray rowBytes = file.read(size);
        if (rowBytes.size() != size)
        {
            qWarning() << funcName << "- Error: Failed to read expected" << size << "bytes, but got" << rowBytes.size() << ". File may be corrupt.";
            continue;
        }

        Vector::RowData singleRowData;
        if (deserializeRow(rowBytes, columns, schemaVersion, singleRowData))
        {
            rows.append(singleRowData);
        }
        else
        {
            qWarning() << funcName << "- Error: Failed to deserialize row at offset" << offset;
            // Optionally, we could add a placeholder row to maintain row count integrity
        }
    }

    qint64 elapsed = timer.elapsed();
    qDebug() << funcName << "- File closed. Total rows read:" << rows.size() << ", Expected:" << indexData.size()
             << ". Time taken:" << elapsed / 1000.0 << "s. Speed:" << (rows.size() * 1000.0 / (elapsed > 0 ? elapsed : 1)) << "rows/s";

    if (rows.size() != header.row_count_in_file)
    {
        qWarning() << funcName << "- Warning: Number of rows read (" << rows.size()
                   << ") does not match header row count (" << header.row_count_in_file << ")";
    }

    file.close();
    return true;
}

bool BinaryFileHelper::writeAllRowsToBinary(const QString &binFilePath, const QList<Vector::ColumnInfo> &columns,
                                            int schemaVersion, const QList<Vector::RowData> &rows)
{
    const QString funcName = "BinaryFileHelper::writeAllRowsToBinary";
    // 减少日志输出
    qDebug() << funcName << "- 开始写入文件:" << binFilePath << ", 行数:" << rows.size() << ", 使用固定长度: true";

    QFile file(binFilePath);
    if (!file.open(QIODevice::WriteOnly))
    {
        qWarning() << funcName << "- 无法打开文件进行写入:" << binFilePath << ", 错误:" << file.errorString();
        return false;
    }

    // 准备并写入文件头
    BinaryFileHeader header;
    header.magic_number = VEC_BINDATA_MAGIC;
    header.file_format_version = CURRENT_FILE_FORMAT_VERSION;
    header.data_schema_version = schemaVersion;
    header.row_count_in_file = rows.size();
    header.column_count_in_file = columns.size();
    header.timestamp_created = QDateTime::currentSecsSinceEpoch();
    header.timestamp_updated = header.timestamp_created;
    header.compression_type = 0;
    std::memset(header.reserved_bytes, 0, sizeof(header.reserved_bytes));

    if (!writeBinaryHeader(&file, header))
    {
        qWarning() << funcName << "- 写入文件头失败";
        file.close();
        return false;
    }

    quint64 rowsWritten = 0;
    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);

    // 设置更大的进度日志间隔
    const int LOG_INTERVAL = 5000;
    const QDateTime startTime = QDateTime::currentDateTime();
    int lastReportedPercent = -1;

    for (const auto &rowData : rows)
    {
        QByteArray serializedRow;
        if (serializeRow(rowData, columns, serializedRow))
        {
            // 写入行数据大小
            out << static_cast<quint32>(serializedRow.size());

            // 写入行数据
            if (file.write(serializedRow) == serializedRow.size())
            {
                rowsWritten++;

                // 优化日志频率，使用百分比进度和时间统计
                if (rows.size() > LOG_INTERVAL)
                {
                    int percent = static_cast<int>((rowsWritten * 100) / rows.size());
                    if (percent != lastReportedPercent && (rowsWritten % LOG_INTERVAL == 0 || percent - lastReportedPercent >= 10))
                    {
                        QDateTime currentTime = QDateTime::currentDateTime();
                        qint64 elapsedMs = startTime.msecsTo(currentTime);
                        double rowsPerSecond = (elapsedMs > 0) ? (rowsWritten * 1000.0 / elapsedMs) : 0;

                        qDebug() << funcName << "- 进度:" << rowsWritten << "/"
                                 << rows.size() << "行已写入 (" << percent << "%), 速度:"
                                 << rowsPerSecond << "行/秒";
                        lastReportedPercent = percent;
                    }
                }
            }
            else
            {
                qWarning() << funcName << "- 写入行数据失败，位置:" << rowsWritten + 1;
                break;
            }
        }
        else
        {
            qWarning() << funcName << "- 序列化行数据失败，位置:" << rowsWritten + 1;
            break;
        }
    }

    // 说明: 成功写入所有行（或部分行）
    header.row_count_in_file = static_cast<unsigned int>(rows.size());
    bool writeHeaderSuccess = writeBinaryHeader(&file, header);
    if (!writeHeaderSuccess)
    {
        qWarning() << funcName << "- 警告: 写入文件头失败! 文件:" << binFilePath;
        // 继续执行, 因为数据已经写入了
    }

    file.close();

    // 全量重写会改变文件结构，需要清除行偏移缓存
    clearRowOffsetCache(binFilePath);

    qDebug() << funcName << "- 文件已关闭. 总计成功写入 " << rowsWritten << " 行.";
    if (rowsWritten != rows.size())
    {
        qWarning() << funcName << "- 部分行写入失败! 预期写入:" << rows.size()
                   << ", 实际写入:" << rowsWritten;
    }

    return rowsWritten == static_cast<quint64>(rows.size());
}
