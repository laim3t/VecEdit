
// 获取向量表指定页数据
QList<Vector::RowData> VectorDataHandler::getPageData(int tableId, int pageIndex, int pageSize)
{
    // --- BEGIN: TEMPORARY FIX ---
    // 使用已验证可行的 getAllVectorRows 作为临时解决方案，绕过下面的分页bug
    const QString funcName = "VectorDataHandler::getPageData";
    qDebug() << funcName << " - [USING FALLBACK] 开始获取分页数据, 表ID:" << tableId
             << ", 页码:" << pageIndex << ", 每页行数:" << pageSize;

    bool ok = false;
    QList<Vector::RowData> allRows = getAllVectorRows(tableId, ok);

    if (!ok)
    {
        qWarning() << funcName << " - [USING FALLBACK] getAllVectorRows 获取数据失败。";
        return QList<Vector::RowData>();
    }

    int totalRowCount = allRows.size();
    if (pageSize <= 0)
        pageSize = 100;

    int startRow = pageIndex * pageSize;

    if (startRow < 0 || startRow >= totalRowCount)
    {
        qWarning() << funcName << " - 请求的起始行 " << startRow << " 超出了总行数 " << totalRowCount;
        return QList<Vector::RowData>();
    }

    int rowsToLoad = qMin(pageSize, totalRowCount - startRow);
    qDebug() << funcName << " - 总行数:" << totalRowCount
             << ", 起始行:" << startRow
             << ", 加载行数:" << rowsToLoad
             << ", 页码:" << pageIndex
             << ", 每页行数:" << pageSize;

    return allRows.mid(startRow, rowsToLoad);
    // --- END: TEMPORARY FIX ---

    /*  --- BEGIN: BUGGY PAGINATION LOGIC (COMMENTED OUT) ---
    const QString funcName = "VectorDataHandler::getPageData";
    QList<Vector::RowData> pageData;

    // 重置取消操作标志
// ... existing code ...
            qDebug() << funcName << " - 操作被取消";
            break;
        }

        // 创建一行数据容器
        Vector::RowData rowData;

        // 按列顺序读取并填充数据
        for (const auto &col : allColumns)
        {
            QVariant cellData;

            // 只处理在二进制文件范围内的列
            if (col.order < header.column_count_in_file)
            {
                switch (col.type)
                {
                case Vector::ColumnDataType::TEXT:
                {
                    // 读取文本数据
                    QByteArray textBytes(256, Qt::Uninitialized); // 使用固定大小
                    if (file.read(textBytes.data(), 256) != 256)
                    {
                        qWarning() << funcName << " - 读取文本数据失败";
                    }
                    QString text = QString::fromUtf8(textBytes).trimmed();
                    cellData = text;
                    break;
                }
                case Vector::ColumnDataType::TIMESET_ID:
                case Vector::ColumnDataType::INTEGER:
                case Vector::ColumnDataType::INSTRUCTION_ID:
                {
                    int value = 0;
                    if (file.read(reinterpret_cast<char *>(&value), sizeof(int)) != sizeof(int))
                    {
                        qWarning() << funcName << " - 读取整数数据失败";
                    }
                    cellData = value;
                    break;
                }
                case Vector::ColumnDataType::REAL:
                {
                    double value = 0.0;
                    if (file.read(reinterpret_cast<char *>(&value), sizeof(double)) != sizeof(double))
                    {
                        qWarning() << funcName << " - 读取浮点数数据失败";
                    }
                    cellData = value;
                    break;
                }
                case Vector::ColumnDataType::PIN_STATE_ID:
                {
                    char state = '\0';
                    if (file.read(&state, 1) != 1)
                    {
                        qWarning() << funcName << " - 读取管脚状态失败";
                    }
                    cellData = QString(state);
                    break;
                }
                case Vector::ColumnDataType::BOOLEAN:
                {
                    bool value = false;
                    if (file.read(reinterpret_cast<char *>(&value), sizeof(bool)) != sizeof(bool))
                    {
                        qWarning() << funcName << " - 读取布尔值数据失败";
                    }
                    cellData = value;
                    break;
                }
                case Vector::ColumnDataType::JSON_PROPERTIES:
                {
                    QByteArray jsonBytes(512, Qt::Uninitialized); // 使用固定大小
                    if (file.read(jsonBytes.data(), 512) != 512)
                    {
                        qWarning() << funcName << " - 读取JSON数据失败";
                    }
                    cellData = QString::fromUtf8(jsonBytes).trimmed();
                    break;
                }
                default:
                    // 对于未知类型，可以读取一个默认大小的数据块并跳过
                    char buffer[256];
                    file.read(buffer, 256);
                    cellData = QVariant(); // 或标记为无效
                    break;
                }
            }
            rowData.append(cellData);
        }
        pageData.append(rowData);
    }

    return pageData;
    --- END: BUGGY PAGINATION LOGIC (COMMENTED OUT) --- */
}

// 获取向量表的可见列信息
QList<Vector::ColumnInfo> VectorDataHandler::getVisibleColumns(int tableId)
{
    const QString funcName = "VectorDataHandler::getVisibleColumns";
    QList<Vector::ColumnInfo> columns;

    // 查询数据库获取列信息
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery colQuery(db);
    colQuery.prepare("SELECT id, column_name, column_order, column_type, data_properties FROM VectorTableColumnConfiguration WHERE master_record_id = ? AND IsVisible = 1 ORDER BY column_order");
    colQuery.addBindValue(tableId);

    if (!colQuery.exec())
    {
        qWarning() << funcName << " - 查询可见列结构失败, 错误:" << colQuery.lastError().text();
        return columns;
    }

    while (colQuery.next())
    {
        Vector::ColumnInfo colInfo;
        colInfo.id = colQuery.value(0).toInt();
        colInfo.vector_table_id = tableId;
        colInfo.name = colQuery.value(1).toString();
        colInfo.order = colQuery.value(2).toInt();
        colInfo.original_type_str = colQuery.value(3).toString();
        colInfo.type = Vector::columnDataTypeFromString(colInfo.original_type_str);
        colInfo.is_visible = true;

        // 解析JSON属性
        QString dataPropertiesJson = colQuery.value(4).toString();
        if (!dataPropertiesJson.isEmpty())
        {
            QJsonDocument doc = QJsonDocument::fromJson(dataPropertiesJson.toUtf8());
            if (!doc.isNull() && doc.isObject())
            {
                colInfo.data_properties = doc.object();
            }
        }

        columns.append(colInfo);
    }

    return columns;
}
