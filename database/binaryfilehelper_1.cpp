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
            value = bool(boolValue);
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

    return true;
}

bool BinaryFileHelper::readAllRowsFromBinary(const QString &binFilePath, const QList<Vector::ColumnInfo> &columns,
                                             int schemaVersion, QList<Vector::RowData> &rows)
{
    const QString funcName = "BinaryFileHelper::readAllRowsFromBinary";
    qDebug() << funcName << "- 开始读取文件:" << binFilePath;
    rows.clear(); // Ensure the output list is empty initially

    QFile file(binFilePath);
    qDebug() << funcName << "- 尝试打开文件进行读取.";
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << funcName << "- 错误: 无法打开文件:" << binFilePath << "错误信息:" << file.errorString();
        return false;
    }
    qDebug() << funcName << "- 文件打开成功.";

    BinaryFileHeader header;
    qDebug() << funcName << "- 尝试读取二进制头信息.";
    if (!readBinaryHeader(&file, header))
    {
        qWarning() << funcName << "- 错误: 无法读取或验证二进制头信息.";
        file.close();
        return false;
    }
    qDebug() << funcName << "- 二进制头信息读取成功. 文件中有" << header.row_count_in_file << "行数据.";
    header.logDetails(funcName); // Log header details

    // --- Version Compatibility Check ---
    if (header.data_schema_version != schemaVersion)
    {
        qWarning() << funcName << "- 警告: 文件数据schema版本 (" << header.data_schema_version
                   << ") 与预期的DB schema版本不同 (" << schemaVersion << "). 尝试兼容处理.";
        // Implement compatibility logic here if needed, e.g., using different deserializeRow versions.
        // For now, we'll proceed but pass the file's schema version to deserializeRow.
        if (header.data_schema_version > schemaVersion)
        {
            qCritical() << funcName << "- 错误: 文件数据schema版本高于DB schema版本. 无法加载.";
            file.close();
            return false;
        }
        // Allow older file versions to be read using their corresponding schema version
    }
    // --- End Version Check ---

    // --- Column Count Sanity Check ---
    if (header.column_count_in_file != static_cast<uint32_t>(columns.size()))
    {
        qWarning() << funcName << "- 警告: 头信息列数 (" << header.column_count_in_file
                   << ") 与DB schema中的列数不匹配 (" << columns.size() << ").";
        // Decide how to handle this. Abort? Try to proceed?
        // Let's try to proceed but log the warning. Deserialization might fail.
    }
    // --- End Column Count Check ---

    if (header.row_count_in_file == 0)
    {
        qDebug() << funcName << "- 文件头显示文件中有0行. 无数据可读.";
        file.close();
        return true; // Successfully read a file with 0 rows as indicated by header
    }

    qDebug() << funcName << "- 文件头显示" << header.row_count_in_file << "行. 开始数据反序列化循环.";

    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian); // Ensure consistency

    // 设置更合理的日志记录间隔
    const int LOG_INTERVAL = 5000; // 每5000行记录一次日志
    const QDateTime startTime = QDateTime::currentDateTime();

    quint64 actualRowsRead = 0; // Use quint64 to match header type

    // 添加安全检查来防止不合理的预分配内存
    if (header.row_count_in_file > std::numeric_limits<int>::max() || header.row_count_in_file > 10000000)
    {
        qCritical() << funcName << "- 错误: 文件头声明的行数(" << header.row_count_in_file
                    << ")过大，超过安全限制或int最大值. 将限制预分配.";
        // 使用安全的最大值来预分配，而不是全部分配
        rows.reserve(qMin(10000000, static_cast<int>(qMin(static_cast<quint64>(std::numeric_limits<int>::max()),
                                                          header.row_count_in_file))));
    }
    else
    {
        rows.reserve(static_cast<int>(header.row_count_in_file)); // QList uses int for size/reserve
    }

    // 用于输出完成百分比的计数器
    int lastReportedPercent = -1;

    while (!in.atEnd() && actualRowsRead < header.row_count_in_file)
    {
        // 保存当前位置，用于跟踪重定位
        qint64 currentPosition = file.pos();

        // Read the size of the next row block
        quint32 rowByteSize;
        in >> rowByteSize;

        if (in.status() != QDataStream::Ok || rowByteSize == 0)
        {
            qWarning() << funcName << "- 错误: 在位置" << file.pos() << "读取行大小失败. 状态:" << in.status();
            break;
        }

        // 增加防护措施：检测不合理的行大小
        const quint32 MAX_REASONABLE_ROW_SIZE = 1 * 1024 * 1024; // 1MB 是合理的单行最大值
        if (rowByteSize > MAX_REASONABLE_ROW_SIZE)
        {
            qCritical() << funcName << "- 检测到异常大的行大小:" << rowByteSize
                        << "字节，超过合理限制" << MAX_REASONABLE_ROW_SIZE << "字节. 可能是文件损坏或格式错误.";

            // 限制大小到合理范围
            rowByteSize = qMin(static_cast<quint32>(file.bytesAvailable()), MAX_REASONABLE_ROW_SIZE);
            qWarning() << funcName << "- 已将行大小调整为:" << rowByteSize << "字节，尝试继续读取";
        }

        // 检查是否是重定位标记 (0xFFFFFFFF)
        if (rowByteSize == 0xFFFFFFFF)
        {
            // 读取重定位位置
            qint64 redirectPosition;
            in >> redirectPosition;

            if (in.status() != QDataStream::Ok)
            {
                qWarning() << funcName << "- 错误: 在位置" << file.pos() << "读取重定向位置失败. 状态:" << in.status();
                break;
            }

            // 只在低频率输出重定向日志
            if (actualRowsRead % LOG_INTERVAL == 0)
            {
                qDebug() << funcName << "- 检测到重定位指针，从位置 " << currentPosition << " 重定向到位置 " << redirectPosition;
            }

            // 保存当前位置，在处理完重定向后需要返回
            qint64 returnPosition = file.pos();

            // 跳转到重定位位置
            if (!file.seek(redirectPosition))
            {
                qWarning() << funcName << "- 无法跳转到重定位位置 " << redirectPosition;
                break;
            }

            // 读取重定位位置的行大小
            in >> rowByteSize;

            if (in.status() != QDataStream::Ok || rowByteSize == 0)
            {
                qWarning() << funcName << "- 错误: 在重定向位置" << redirectPosition << "读取行大小失败. 状态:" << in.status();
                break;
            }

            // 对重定位位置也进行行大小检测
            if (rowByteSize > MAX_REASONABLE_ROW_SIZE)
            {
                qCritical() << funcName << "- 重定向位置检测到异常大的行大小:" << rowByteSize
                            << "字节，超过合理限制" << MAX_REASONABLE_ROW_SIZE << "字节. 可能是文件损坏或格式错误.";

                // 限制大小到合理范围
                rowByteSize = qMin(static_cast<quint32>(file.bytesAvailable()), MAX_REASONABLE_ROW_SIZE);
                qWarning() << funcName << "- 已将重定向位置的行大小调整为:" << rowByteSize << "字节，尝试继续读取";
            }

            // 如果重定位位置又是一个重定位指针，这是错误的（避免循环引用）
            if (rowByteSize == 0xFFFFFFFF)
            {
                qWarning() << funcName << "- 错误：重定位指针指向另一个重定位指针，可能存在循环引用";
                break;
            }

            // 读取并处理重定位位置的数据
            QByteArray rowBytes(rowByteSize, Qt::Uninitialized);
            if (in.readRawData(rowBytes.data(), rowByteSize) != static_cast<int>(rowByteSize))
            {
                qWarning() << funcName << "- 错误: 在重定向位置读取行数据失败. 预期:" << rowByteSize << "实际:" << rowBytes.size();
                break;
            }

            // 反序列化重定位位置的行数据
            Vector::RowData rowData;
            if (deserializeRow(rowBytes, columns, header.data_schema_version, rowData))
            {
                rows.append(rowData);
                actualRowsRead++;

                // 计算和显示进度
                int percent = static_cast<int>((actualRowsRead * 100) / header.row_count_in_file);
                if (percent != lastReportedPercent && (actualRowsRead % LOG_INTERVAL == 0 || percent - lastReportedPercent >= 10))
                {
                    QDateTime currentTime = QDateTime::currentDateTime();
                    qint64 elapsedMs = startTime.msecsTo(currentTime);
                    double rowsPerSecond = (elapsedMs > 0) ? (actualRowsRead * 1000.0 / elapsedMs) : 0;
                    qDebug() << funcName << "- 进度:" << actualRowsRead << "行读取 ("
                             << percent << "%), 速度:" << rowsPerSecond << "行/秒";
                    lastReportedPercent = percent;
                }
            }
            else
            {
                qWarning() << funcName << "- 在重定向位置反序列化行失败. 继续下一行.";
            }

            // 返回原位置继续处理后续行
            if (!file.seek(returnPosition))
            {
                qWarning() << funcName << "- 无法返回原位置 " << returnPosition << " 继续处理";
                break;
            }

            // 继续下一行的处理，跳过当前行的读取逻辑
            continue;
        }

        // Read the actual bytes for this row
        QByteArray rowBytes(rowByteSize, Qt::Uninitialized);
        if (in.readRawData(rowBytes.data(), rowByteSize) != static_cast<int>(rowByteSize))
        {
            qWarning() << funcName << "- 错误: 读取行数据失败. 预期:" << rowByteSize << "实际:" << rowBytes.size();
            break;
        }

        // Deserialize the row bytes into a row data object
        Vector::RowData rowData;
        if (deserializeRow(rowBytes, columns, header.data_schema_version, rowData))
        {
            rows.append(rowData);
            actualRowsRead++;

            // 计算和显示进度
            int percent = static_cast<int>((actualRowsRead * 100) / header.row_count_in_file);
            if (percent != lastReportedPercent && (actualRowsRead % LOG_INTERVAL == 0 || percent - lastReportedPercent >= 10))
            {
                QDateTime currentTime = QDateTime::currentDateTime();
                qint64 elapsedMs = startTime.msecsTo(currentTime);
                double rowsPerSecond = (elapsedMs > 0) ? (actualRowsRead * 1000.0 / elapsedMs) : 0;
                qDebug() << funcName << "- 进度:" << actualRowsRead << "行读取 ("
                         << percent << "%), 速度:" << rowsPerSecond << "行/秒";
                lastReportedPercent = percent;
            }
        }
        else
        {
            qWarning() << funcName << "- 反序列化行失败，位置:" << actualRowsRead + 1 << ". 继续下一行.";
            // Option: We could break here to abort on first error, but continuing allows for more resilience
        }
    }

    file.close();

    // 计算并显示总体处理统计信息
    QDateTime endTime = QDateTime::currentDateTime();
    qint64 totalElapsedMs = startTime.msecsTo(endTime);
    double avgRowsPerSecond = (totalElapsedMs > 0) ? (actualRowsRead * 1000.0 / totalElapsedMs) : 0;

    qDebug() << funcName << "- 文件已关闭. 总计读取:" << rows.size() << "行, 预期:" << header.row_count_in_file
             << "行. 耗时:" << (totalElapsedMs / 1000.0) << "秒, 平均速度:" << avgRowsPerSecond << "行/秒";

    if (actualRowsRead != header.row_count_in_file)
    {
        qWarning() << funcName << "- 警告: 实际读取行数 (" << actualRowsRead << ") 与文件头行数不匹配 (" << header.row_count_in_file << ")";
    }

    return !rows.isEmpty() || header.row_count_in_file == 0; // Consider empty file as success if header indicates 0 rows
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
            if (out.writeRawData(serializedRow.constData(), serializedRow.size()) == serializedRow.size())
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
