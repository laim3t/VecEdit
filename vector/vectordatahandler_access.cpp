QList<Vector::RowData> VectorDataHandler::getAllVectorRows(int tableId, bool &ok)
{
    const QString funcName = "VectorDataHandler::getAllVectorRows";
    qDebug() << funcName << "- Getting all rows for table" << tableId;
    ok = false;
    QString errorMsg;

    // 首先，解析二进制文件的绝对路径
    QString absoluteBinFilePath = resolveBinaryFilePath(tableId, errorMsg);
    if (absoluteBinFilePath.isEmpty())
    {
        qWarning() << funcName << "- Failed to resolve binary file path for table" << tableId << "Error:" << errorMsg;
        return QList<Vector::RowData>();
    }

    // 检查缓存是否有效
    if (isTableDataCacheValid(tableId, absoluteBinFilePath))
    {
        qDebug() << funcName << "- Cache is valid, returning cached data for table" << tableId;
        ok = true;
        return m_tableDataCache[tableId];
    }

    // 缓存无效或不存在，从文件加载
    qDebug() << funcName << "- Cache not valid, loading from file for table" << tableId;

    // 加载元数据以获取列结构和schema版本
    QString binFileName; // 这个是相对路径，从meta里读出来的
    QList<Vector::ColumnInfo> columns;
    int schemaVersion = 0;
    int rowCount = 0; // 这个也是从meta里读出来的

    if (!loadVectorTableMeta(tableId, binFileName, columns, schemaVersion, rowCount))
    {
        qWarning() << funcName << "- Failed to load vector table metadata for table" << tableId;
        return QList<Vector::RowData>();
    }

    // 从二进制文件读取所有行
    QList<Vector::RowData> rows;
    if (!readAllRowsFromBinary(absoluteBinFilePath, columns, schemaVersion, rows))
    {
        qWarning() << funcName << "- Failed to read all rows from binary for table" << tableId;
        return QList<Vector::RowData>();
    }

    // 更新缓存
    updateTableDataCache(tableId, rows, absoluteBinFilePath);

    qDebug() << funcName << "- Successfully loaded" << rows.count() << "rows for table" << tableId;
    ok = true;
    return rows;
}

int VectorDataHandler::getSchemaVersion(int tableId)
{
    const QString funcName = "VectorDataHandler::getSchemaVersion";
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << funcName << "- 数据库未打开";
        return 1; // Default
    }
    QSqlQuery metaQuery(db);
    metaQuery.prepare("SELECT data_schema_version FROM VectorTableMasterRecord WHERE id = ?");
    metaQuery.addBindValue(tableId);
    if (!metaQuery.exec() || !metaQuery.next())
    {
        qWarning() << funcName << "- 查询主记录失败, 表ID:" << tableId << ", 错误:" << metaQuery.lastError().text();
        return 1; // Default
    }
    return metaQuery.value(0).toInt();
}

QList<Vector::ColumnInfo> VectorDataHandler::getAllColumnInfo(int tableId)
{
    const QString funcName = "VectorDataHandler::getAllColumnInfo";
    QList<Vector::ColumnInfo> columns;
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << funcName << "- 数据库未打开";
        return columns;
    }
    QSqlQuery colQuery(db);
    colQuery.prepare("SELECT id, column_name, column_order, column_type, data_properties, IsVisible FROM VectorTableColumnConfiguration WHERE master_record_id = ? ORDER BY column_order");
    colQuery.addBindValue(tableId);
    if (!colQuery.exec())
    {
        qWarning() << funcName << "- 查询完整列结构失败, 错误:" << colQuery.lastError().text();
        return columns;
    }

    while (colQuery.next())
    {
        Vector::ColumnInfo col;
        col.id = colQuery.value(0).toInt();
        col.vector_table_id = tableId;
        col.name = colQuery.value(1).toString();
        col.order = colQuery.value(2).toInt();
        col.original_type_str = colQuery.value(3).toString();
        col.type = Vector::columnDataTypeFromString(col.original_type_str);
        col.is_visible = colQuery.value(5).toBool();

        QString propStr = colQuery.value(4).toString();
        if (!propStr.isEmpty())
        {
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(propStr.toUtf8(), &err);
            if (err.error == QJsonParseError::NoError && doc.isObject())
            {
                col.data_properties = doc.object();
            }
        }
        columns.append(col);
    }
    return columns;
}

int VectorDataHandler::getVectorTableRowCount(int tableId)
{
    const QString funcName = "VectorDataHandler::getVectorTableRowCount";
    qDebug() << funcName << " - 获取表ID为" << tableId << "的行数";

    // 首先尝试从元数据中获取行数
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << funcName << " - 数据库未打开";
        return 0;
    }

    QSqlQuery metaQuery(db);
    metaQuery.prepare("SELECT row_count FROM VectorTableMasterRecord WHERE id = ?");
    metaQuery.addBindValue(tableId);

    if (metaQuery.exec() && metaQuery.next())
    {
        int rowCount = metaQuery.value(0).toInt();
        qDebug() << funcName << " - 从元数据获取的行数:" << rowCount;

        // 如果元数据中的行数大于0，直接返回
        if (rowCount > 0)
        {
            return rowCount;
        }
    }
    else
    {
        qWarning() << funcName << " - 查询元数据行数失败:" << metaQuery.lastError().text();
    }

    // 如果元数据中没有行数或行数为0，尝试从二进制文件头中获取
    QString binFileName;
    QList<Vector::ColumnInfo> columns;
    int schemaVersion = 0;
    int rowCountFromMeta = 0;

    if (!loadVectorTableMeta(tableId, binFileName, columns, schemaVersion, rowCountFromMeta))
    {
        qWarning() << funcName << " - 无法加载元数据";
        return 0;
    }

    // 如果已经从loadVectorTableMeta获取到行数，返回它
    if (rowCountFromMeta > 0)
    {
        qDebug() << funcName << " - 从loadVectorTableMeta获取的行数:" << rowCountFromMeta;
        return rowCountFromMeta;
    }

    // 最后尝试直接从二进制文件头中获取
    QString resolveError;
    QString absoluteBinFilePath = resolveBinaryFilePath(tableId, resolveError);
    if (absoluteBinFilePath.isEmpty())
    {
        qWarning() << funcName << " - 无法解析二进制文件路径:" << resolveError;
        return 0;
    }

    QFile binFile(absoluteBinFilePath);
    if (!binFile.exists())
    {
        qWarning() << funcName << " - 二进制文件不存在:" << absoluteBinFilePath;
        return 0;
    }

    if (binFile.open(QIODevice::ReadOnly))
    {
        BinaryFileHeader header;
        if (Persistence::BinaryFileHelper::readBinaryHeader(&binFile, header))
        {
            qDebug() << funcName << " - 从二进制文件头获取的行数:" << header.row_count_in_file;
            binFile.close();
            return header.row_count_in_file;
        }
        else
        {
            qWarning() << funcName << " - 读取二进制文件头失败";
            binFile.close();
        }
    }
    else
    {
        qWarning() << funcName << " - 无法打开二进制文件:" << binFile.errorString();
    }

    // 如果所有方法都失败，尝试读取整个文件来计算行数
    QList<Vector::RowData> allRows;
    if (Persistence::BinaryFileHelper::readAllRowsFromBinary(absoluteBinFilePath, columns, schemaVersion, allRows))
    {
        int rowsCount = allRows.size();
        qDebug() << funcName << " - 通过读取整个文件获取的行数:" << rowsCount;
        return rowsCount;
    }

    qWarning() << funcName << " - 所有尝试都失败，返回0行";
    return 0;
}

// 重构后的主流程
bool VectorDataHandler::loadVectorTableData(int tableId, QTableWidget *tableWidget)
{
    const QString funcName = "VectorDataHandler::loadVectorTableData";
    qDebug() << funcName << " - 开始加载, 表ID:" << tableId;
    if (!tableWidget)
    {
        qWarning() << funcName << " - tableWidget 为空";
        return false;
    }

    // 初始化缓存（如果尚未初始化）
    if (!m_cacheInitialized)
    {
        initializeCache();
    }

    // 禁用表格更新，减少UI重绘，提升性能
    tableWidget->setUpdatesEnabled(false);
    tableWidget->horizontalHeader()->setUpdatesEnabled(false);
    tableWidget->verticalHeader()->setUpdatesEnabled(false);

    // 保存当前滚动条位置
    QScrollBar *vScrollBar = tableWidget->verticalScrollBar();
    QScrollBar *hScrollBar = tableWidget->horizontalScrollBar();
    int vScrollValue = vScrollBar ? vScrollBar->value() : 0;
    int hScrollValue = hScrollBar ? hScrollBar->value() : 0;

    // 阻止信号以避免不必要的更新
    tableWidget->blockSignals(true);

    // 原有的清理逻辑
    tableWidget->clearContents(); // Use clearContents instead of clear to keep headers
    tableWidget->setRowCount(0);
    // Don't clear columns/headers here if they might already be set,
    // or reset them based on loaded meta later.

    // 1. 读取元数据 (仍然需要，用于获取列信息等)
    QString binFileNameFromMeta; // Variable name emphasizes it's from metadata
    QList<Vector::ColumnInfo> columns;
    int schemaVersion = 0;
    int rowCountFromMeta = 0; // Variable name emphasizes it's from metadata
    if (!loadVectorTableMeta(tableId, binFileNameFromMeta, columns, schemaVersion, rowCountFromMeta))
    {
        qWarning() << funcName << " - 元数据加载失败, 表ID:" << tableId;
        // Clear the table to indicate failure
        tableWidget->setColumnCount(0);
        tableWidget->setRowCount(0);

        // 恢复更新和信号
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);
        return false;
    }
    qDebug() << funcName << " - 元数据加载成功, 列数:" << columns.size() << ", DB记录行数:" << rowCountFromMeta;

    // 如果没有列信息，也无法继续
    if (columns.isEmpty())
    {
        qWarning() << funcName << " - 表 " << tableId << " 没有列配置。";
        tableWidget->setColumnCount(0);
        tableWidget->setRowCount(0);

        // 恢复更新和信号
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);
        return true; // Return true as metadata loaded, but table is empty/unconfigured
    }

    // 2. 解析二进制文件路径
    QString errorMsg;
    QString absoluteBinFilePath = resolveBinaryFilePath(tableId, errorMsg);
    if (absoluteBinFilePath.isEmpty())
    {
        qWarning() << funcName << " - 无法解析二进制文件路径: " << errorMsg;
        // Indicate error state, e.g., clear table, show message
        tableWidget->setColumnCount(0);
        tableWidget->setRowCount(0);

        // 恢复更新和信号
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);
        return false;
    }

    // 检查文件是否存在，如果不存在，对于加载操作来说通常是一个错误，
    // 除非我们允许一个没有二进制文件的表（可能仅元数据）。
    // 新创建的表应该有一个空的带头部的二进制文件。
    if (!QFile::exists(absoluteBinFilePath))
    {
        qWarning() << funcName << " - 二进制文件不存在: " << absoluteBinFilePath;
        // Decide how to handle this. Is it an error? Or just means 0 rows?
        // Let's assume for now it might be a newly created table where binary file creation failed
        // or an old table where the file was lost. Treat as error for now.
        tableWidget->setColumnCount(0);
        tableWidget->setRowCount(0);

        // 恢复更新和信号
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);
        return false; // Return false as data is missing
    }

    // 3. 读取所有行的原始数据
    QList<Vector::RowData> allRowsOriginal;

    // 3.1 获取全部列信息（包括隐藏的）以加载完整的二进制数据
    QList<Vector::ColumnInfo> allColumns;
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery colQuery(db);
    colQuery.prepare("SELECT id, column_name, column_order, column_type, data_properties, IsVisible FROM VectorTableColumnConfiguration WHERE master_record_id = ? ORDER BY column_order");
    colQuery.addBindValue(tableId);
    if (!colQuery.exec())
    {
        qWarning() << funcName << " - 查询完整列结构失败, 错误:" << colQuery.lastError().text();
        tableWidget->setColumnCount(0);
        tableWidget->setRowCount(0);

        // 恢复更新和信号
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);
        return false;
    }

    // 构建列映射：原始列索引 -> 可见列索引
    // 这将帮助我们从二进制数据中正确提取数据到UI表中
    QMap<int, int> columnIndexMapping;
    int visibleColIndex = 0;

    while (colQuery.next())
    {
        Vector::ColumnInfo col;
        col.id = colQuery.value(0).toInt();
        col.vector_table_id = tableId;
        col.name = colQuery.value(1).toString();
        col.order = colQuery.value(2).toInt();
        col.original_type_str = colQuery.value(3).toString();
        col.type = Vector::columnDataTypeFromString(col.original_type_str);
        col.is_visible = colQuery.value(5).toBool();

        QString propStr = colQuery.value(4).toString();
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

        // 只为可见列创建映射
        if (col.is_visible)
        {
            // 原始索引 -> 可见索引的映射
            columnIndexMapping[col.order] = visibleColIndex++;
            qDebug() << funcName << " - 列映射: 原始索引" << col.order
                     << " -> 可见索引" << (visibleColIndex - 1)
                     << ", 列名:" << col.name;
        }
    }

    // 加载完整的二进制数据（包括隐藏列）
    if (!Persistence::BinaryFileHelper::readAllRowsFromBinary(absoluteBinFilePath, allColumns, schemaVersion, allRowsOriginal))
    {
        qWarning() << funcName << " - 二进制数据加载失败 (readAllRowsFromBinary 返回 false), 文件:" << absoluteBinFilePath;
        tableWidget->setColumnCount(0);
        tableWidget->setRowCount(0);

        // 恢复更新和信号
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);
        return false;
    }
    qDebug() << funcName << " - 从二进制文件加载了 " << allRowsOriginal.size() << " 行, 原始列数:" << allColumns.size() << ", 可见列数:" << columns.size();

    // 校验从文件读取的行数与数据库记录的行数 (可选，但推荐)
    if (allRowsOriginal.size() != rowCountFromMeta)
    {
        qWarning() << funcName << " - 文件中的行数 (" << allRowsOriginal.size()
                   << ") 与数据库元数据记录的行数 (" << rowCountFromMeta
                   << ") 不匹配！文件: " << absoluteBinFilePath;
        // Decide how to proceed. Trust the file? Trust the DB? Error out?
        // For now, let's trust the file content but log a warning.
        // Consider adding logic to update the DB row count if file is trusted.
    }

    // 4. 设置表头
    tableWidget->setColumnCount(columns.size());
    qDebug() << funcName << " - 设置表格列数:" << columns.size();

    QStringList headers;
    QMap<QString, QString> headerMapping; // 存储原始列名到表头显示文本的映射

    for (const auto &col : columns)
    {
        // 检查是否是管脚列
        if (col.type == Vector::ColumnDataType::PIN_STATE_ID && !col.data_properties.isEmpty())
        {
            // 获取管脚属性
            int channelCount = col.data_properties["channel_count"].toInt(1);
            int typeId = col.data_properties["type_id"].toInt(1);

            // 获取类型名称
            QString typeName = "In"; // 默认为输入类型
            QSqlQuery typeQuery(DatabaseManager::instance()->database());
            typeQuery.prepare("SELECT type_name FROM type_options WHERE id = ?");
            typeQuery.addBindValue(typeId);
            if (typeQuery.exec() && typeQuery.next())
            {
                typeName = typeQuery.value(0).toString();
            }

            // 创建带有管脚信息的表头
            QString headerText = col.name + "\nx" + QString::number(channelCount) + "\n" + typeName;
            headers << headerText;
            headerMapping[col.name] = headerText; // 保存映射关系
            qDebug() << funcName << " - 添加管脚列表头:" << headerText << "，原始列名:" << col.name << "，索引:" << headers.size() - 1;
        }
        else
        {
            // 标准列，直接使用列名
            headers << col.name;
            headerMapping[col.name] = col.name; // 标准列映射相同
            qDebug() << funcName << " - 添加标准列表头:" << col.name << "，索引:" << headers.size() - 1;
        }
    }
    tableWidget->setHorizontalHeaderLabels(headers);
    qDebug() << funcName << " - 设置表头完成，列数:" << headers.size() << "，列表:" << headers.join(", ");

    // 确保所有表头项居中对齐
    for (int i = 0; i < tableWidget->columnCount(); ++i)
    {
        QTableWidgetItem *headerItem = tableWidget->horizontalHeaderItem(i);
        if (headerItem)
        {
            headerItem->setTextAlignment(Qt::AlignCenter);
            qDebug() << funcName << " - 设置表头项居中对齐:" << i << "," << headerItem->text();
        }
    }

    // 确保列数与表头列表一致
    if (tableWidget->columnCount() != headers.size())
    {
        qWarning() << funcName << " - 警告：表格列数 (" << tableWidget->columnCount()
                   << ") 与表头列表数 (" << headers.size() << ") 不一致！";
        tableWidget->setColumnCount(headers.size());
    }

    // 5. 填充数据，根据映射关系处理
    tableWidget->setRowCount(allRowsOriginal.size());
    qDebug() << funcName << " - 准备填充 " << allRowsOriginal.size() << " 行到 QTableWidget";

    // 创建列ID到索引的映射，加速查找
    QMap<int, int> columnIdToIndexMap;
    for (int i = 0; i < allColumns.size(); ++i)
    {
        columnIdToIndexMap[allColumns[i].id] = i;
        qDebug() << funcName << " - 列ID映射: ID=" << allColumns[i].id << " -> 索引=" << i
                 << ", 名称=" << allColumns[i].name << ", 可见=" << allColumns[i].is_visible;
    }

    for (int row = 0; row < allRowsOriginal.size(); ++row)
    {
        const Vector::RowData &originalRowData = allRowsOriginal[row];

        // 遍历可见列
        for (int visibleColIdx = 0; visibleColIdx < columns.size(); ++visibleColIdx)
        {
            const auto &visibleCol = columns[visibleColIdx];

            // 查找此可见列在原始数据中的索引位置
            if (!columnIdToIndexMap.contains(visibleCol.id))
            {
                qWarning() << funcName << " - 行" << row << "列" << visibleColIdx
                           << " (" << visibleCol.name << ") 在列ID映射中未找到，使用默认值";

                // 使用默认值
                QTableWidgetItem *newItem = new QTableWidgetItem();
                if (visibleCol.type == Vector::ColumnDataType::PIN_STATE_ID)
                {
                    newItem->setText("X");
                }
                else
                {
                    newItem->setText("");
                }
                tableWidget->setItem(row, visibleColIdx, newItem);
                continue;
            }

            // 获取原始列索引
            int originalColIdx = columnIdToIndexMap[visibleCol.id];

            qDebug() << funcName << " - 找到列映射: UI列" << visibleColIdx
                     << " (" << visibleCol.name << ") -> 原始列" << originalColIdx
                     << " (" << allColumns[originalColIdx].name << "), 类型:"
                     << (visibleCol.type == Vector::ColumnDataType::PIN_STATE_ID ? "管脚列" : "标准列");

            if (originalColIdx >= originalRowData.size())
            {
                qWarning() << funcName << " - 行" << row << "列" << visibleColIdx
                           << " (" << visibleCol.name << ") 超出原始数据范围，使用默认值";

                // 使用默认值
                QTableWidgetItem *newItem = new QTableWidgetItem();
                if (visibleCol.type == Vector::ColumnDataType::PIN_STATE_ID)
                {
                    newItem->setText("X");
                }
                else
                {
                    newItem->setText("");
                }
                tableWidget->setItem(row, visibleColIdx, newItem);
                continue;
            }

            // 获取原始值
            QVariant originalValue = originalRowData[originalColIdx];
            qDebug() << funcName << " - 行" << row << ", 列" << visibleColIdx
                     << " (" << visibleCol.name << ") 从原始列" << originalColIdx
                     << "获取值:" << originalValue;

            // 根据列类型处理数据
            if (visibleCol.type == Vector::ColumnDataType::PIN_STATE_ID)
            {
                // 设置管脚列的值（从缓存中读取）
                QString pinStateText;
                if (originalValue.isNull() || !originalValue.isValid() || originalValue.toString().isEmpty())
                {
                    pinStateText = "X"; // 默认值
                }
                else
                {
                    pinStateText = originalValue.toString();
                }

                // 创建PinValueLineEdit作为单元格控件
                PinValueLineEdit *pinEdit = new PinValueLineEdit(tableWidget);
                pinEdit->setText(pinStateText);
                tableWidget->setCellWidget(row, visibleColIdx, pinEdit);
                qDebug() << funcName << " - 行" << row << ", 列" << visibleColIdx
                         << " 设置管脚状态为:" << pinStateText;
            }
            else if (visibleCol.type == Vector::ColumnDataType::INSTRUCTION_ID)
            {
                // 处理INSTRUCTION_ID类型，从缓存获取指令文本
                int instructionId = originalValue.toInt();
                QString instructionText;

                // 从缓存中获取指令文本
                if (m_instructionCache.contains(instructionId))
                {
                    instructionText = m_instructionCache.value(instructionId);
                    qDebug() << funcName << " - 从缓存获取指令文本，ID:" << instructionId << ", 文本:" << instructionText;
                }
                else
                {
                    // 缓存中没有，从数据库获取
                    QSqlQuery instructionQuery(db);
                    instructionQuery.prepare("SELECT instruction_value FROM instruction_options WHERE id = ?");
                    instructionQuery.addBindValue(instructionId);

                    if (instructionQuery.exec() && instructionQuery.next())
                    {
                        instructionText = instructionQuery.value(0).toString();
                        // 添加到缓存
                        m_instructionCache[instructionId] = instructionText;
                        qDebug() << funcName << " - 添加指令到缓存，ID:" << instructionId << ", 文本:" << instructionText;
                    }
                    else
                    {
                        instructionText = QString("未知(%1)").arg(instructionId);
                        qWarning() << funcName << " - 无法获取指令文本，ID:" << instructionId;
                    }
                }

                QTableWidgetItem *newItem = new QTableWidgetItem(instructionText);
                newItem->setData(Qt::UserRole, instructionId);
                tableWidget->setItem(row, visibleColIdx, newItem);
                qDebug() << funcName << " - 行" << row << ", 列" << visibleColIdx
                         << " 设置指令为:" << instructionText << "(ID:" << instructionId << ")";
            }
            else if (visibleCol.type == Vector::ColumnDataType::TIMESET_ID)
            {
                // 处理TIMESET_ID类型，从缓存获取TimeSet文本
                int timesetId = originalValue.toInt();
                QString timesetText;

                // 从缓存中获取TimeSet文本
                if (m_timesetCache.contains(timesetId))
                {
                    timesetText = m_timesetCache.value(timesetId);
                    qDebug() << funcName << " - 从缓存获取TimeSet文本，ID:" << timesetId << ", 文本:" << timesetText;
                }
                else
                {
                    // 缓存中没有，从数据库获取
                    QSqlQuery timesetQuery(db);
                    timesetQuery.prepare("SELECT timeset_name FROM timeset_list WHERE id = ?");
                    timesetQuery.addBindValue(timesetId);

                    if (timesetQuery.exec() && timesetQuery.next())
                    {
                        timesetText = timesetQuery.value(0).toString();
                        // 添加到缓存
                        m_timesetCache[timesetId] = timesetText;
                        qDebug() << funcName << " - 添加TimeSet到缓存，ID:" << timesetId << ", 文本:" << timesetText;
                    }
                    else
                    {
                        timesetText = QString("未知(%1)").arg(timesetId);
                        qWarning() << funcName << " - 无法获取TimeSet文本，ID:" << timesetId;
                    }
                }

                QTableWidgetItem *newItem = new QTableWidgetItem(timesetText);
                newItem->setData(Qt::UserRole, timesetId);
                tableWidget->setItem(row, visibleColIdx, newItem);
                qDebug() << funcName << " - 行" << row << ", 列" << visibleColIdx
                         << " 设置TimeSet为:" << timesetText << "(ID:" << timesetId << ")";
            }
            else if (visibleCol.type == Vector::ColumnDataType::BOOLEAN)
            {
                // 对于布尔值处理 (Capture)
                bool boolValue = originalValue.toBool();
                QString displayText = boolValue ? "Y" : "N";

                QTableWidgetItem *newItem = new QTableWidgetItem(displayText);
                newItem->setData(Qt::UserRole, boolValue);
                tableWidget->setItem(row, visibleColIdx, newItem);
                qDebug() << funcName << " - 行" << row << ", 列" << visibleColIdx
                         << " 设置布尔值为:" << displayText << "(" << boolValue << ")";
            }
            else
            {
                // 其他类型的列，直接创建QTableWidgetItem
                QTableWidgetItem *newItem = new QTableWidgetItem();
                QVariant originalValue = originalRowData[originalColIdx]; // 添加声明

                // 根据具体类型设置文本
                if (visibleCol.type == Vector::ColumnDataType::INTEGER)
                {
                    newItem->setText(originalValue.isValid() ? QString::number(originalValue.toInt()) : "");
                    newItem->setData(Qt::UserRole, originalValue.isValid() ? originalValue.toInt() : 0);
                }
                else if (visibleCol.type == Vector::ColumnDataType::REAL)
                {
                    newItem->setText(originalValue.isValid() ? QString::number(originalValue.toDouble()) : "");
                    newItem->setData(Qt::UserRole, originalValue.isValid() ? originalValue.toDouble() : 0.0);
                }
                else
                {
                    // TEXT 或其他类型，直接转换为字符串
                    newItem->setText(originalValue.toString());
                }

                tableWidget->setItem(row, visibleColIdx, newItem);
                qDebug() << funcName << " - 行" << row << ", 列" << visibleColIdx
                         << " 设置为:" << newItem->text();
            }
        }
    }

    // 在函数结束前恢复更新和信号
    tableWidget->blockSignals(false);

    // 恢复滚动条位置
    if (vScrollBar)
    {
        vScrollBar->setValue(vScrollValue);
    }
    if (hScrollBar)
    {
        hScrollBar->setValue(hScrollValue);
    }

    // 恢复UI更新
    tableWidget->verticalHeader()->setUpdatesEnabled(true);
    tableWidget->horizontalHeader()->setUpdatesEnabled(true);
    tableWidget->setUpdatesEnabled(true);

    // 强制刷新视图
    tableWidget->viewport()->update();

    qDebug() << funcName << " - 表格填充完成, 总行数:" << tableWidget->rowCount()
             << ", 总列数:" << tableWidget->columnCount();
    return true;
}

// 实现分页加载数据方法
bool VectorDataHandler::loadVectorTablePageData(int tableId, QTableWidget *tableWidget, int pageIndex, int pageSize)
{
    const QString funcName = "VectorDataHandler::loadVectorTablePageData";
    qDebug() << funcName << " - 开始加载分页数据, 表ID:" << tableId
             << ", 页码:" << pageIndex << ", 每页行数:" << pageSize;

    if (!tableWidget)
    {
        qWarning() << funcName << " - tableWidget 为空";
        return false;
    }

    // 初始化缓存（如果尚未初始化）
    if (!m_cacheInitialized)
    {
        initializeCache();
    }

    // 禁用表格更新，减少UI重绘，提升性能
    tableWidget->setUpdatesEnabled(false);
    tableWidget->horizontalHeader()->setUpdatesEnabled(false);
    tableWidget->verticalHeader()->setUpdatesEnabled(false);

    // 保存当前滚动条位置
    QScrollBar *vScrollBar = tableWidget->verticalScrollBar();
    QScrollBar *hScrollBar = tableWidget->horizontalScrollBar();
    int vScrollValue = vScrollBar ? vScrollBar->value() : 0;
    int hScrollValue = hScrollBar ? hScrollBar->value() : 0;

    // 阻止信号以避免不必要的更新
    tableWidget->blockSignals(true);

    // 清理现有内容，但保留表头
    tableWidget->clearContents();

    // 1. 读取元数据 (需要获取列信息和总行数)
    QString binFileNameFromMeta; // 从元数据获取的文件名
    QList<Vector::ColumnInfo> columns;
    int schemaVersion = 0;
    int totalRowCount = 0;

    if (!loadVectorTableMeta(tableId, binFileNameFromMeta, columns, schemaVersion, totalRowCount))
    {
        qWarning() << funcName << " - 元数据加载失败, 表ID:" << tableId;
        // 清理表格，表示失败
        tableWidget->setRowCount(0);

        // 恢复更新和信号
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);
        return false;
    }
    qDebug() << funcName << " - 元数据加载成功, 列数:" << columns.size() << ", 总行数:" << totalRowCount;

    // 计算分页参数
    int totalPages = (totalRowCount + pageSize - 1) / pageSize; // 向上取整
    if (pageIndex < 0)
        pageIndex = 0;
    if (pageIndex >= totalPages && totalPages > 0)
        pageIndex = totalPages - 1;

    int startRow = pageIndex * pageSize;
    int rowsToLoad = qMin(pageSize, totalRowCount - startRow);

    qDebug() << funcName << " - 分页参数: 总页数=" << totalPages
             << ", 当前页=" << pageIndex << ", 起始行=" << startRow
             << ", 加载行数=" << rowsToLoad;

    // 设置表格行数
    tableWidget->setRowCount(rowsToLoad);

    // 如果没有数据，直接返回成功
    if (rowsToLoad <= 0)
    {
        qDebug() << funcName << " - 当前页没有数据, 直接返回";

        // 恢复更新和信号
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);
        return true;
    }

    // 如果列数为0，也无法继续
    if (columns.isEmpty())
    {
        qWarning() << funcName << " - 表 " << tableId << " 没有列配置。";
        tableWidget->setRowCount(0);

        // 恢复更新和信号
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);
        return true; // 返回true，因为元数据已加载，只是表为空
    }

    // 2. 解析二进制文件路径
    QString errorMsg;
    QString absoluteBinFilePath = resolveBinaryFilePath(tableId, errorMsg);
    if (absoluteBinFilePath.isEmpty())
    {
        qWarning() << funcName << " - 无法解析二进制文件路径: " << errorMsg;
        // 设置错误状态
        tableWidget->setRowCount(0);

        // 恢复更新和信号
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);
        return false;
    }

    // 检查文件是否存在
    if (!QFile::exists(absoluteBinFilePath))
    {
        qWarning() << funcName << " - 二进制文件不存在: " << absoluteBinFilePath;
        tableWidget->setRowCount(0);

        // 恢复更新和信号
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);
        return false;
    }

    // 3. 设置表格列（如果需要）
    if (tableWidget->columnCount() != columns.size())
    {
        tableWidget->setColumnCount(columns.size());

        // 设置表头
        QStringList headers;
        for (const auto &col : columns)
        {
            // 根据列类型设置表头
            if (col.type == Vector::ColumnDataType::PIN_STATE_ID && !col.data_properties.isEmpty())
            {
                // 获取管脚属性
                int channelCount = col.data_properties["channel_count"].toInt(1);
                int typeId = col.data_properties["type_id"].toInt(1);

                // 获取类型名称
                QString typeName = "In"; // 默认为输入类型
                QSqlQuery typeQuery(DatabaseManager::instance()->database());
                typeQuery.prepare("SELECT type_name FROM type_options WHERE id = ?");
                typeQuery.addBindValue(typeId);
                if (typeQuery.exec() && typeQuery.next())
                {
                    typeName = typeQuery.value(0).toString();
                }

                // 创建带有管脚信息的表头
                QString headerText = col.name + "\nx" + QString::number(channelCount) + "\n" + typeName;
                headers << headerText;
            }
            else
            {
                // 标准列，直接使用列名
                headers << col.name;
            }
        }

        tableWidget->setHorizontalHeaderLabels(headers);

        // 设置表头居中对齐
        for (int i = 0; i < tableWidget->columnCount(); ++i)
        {
            QTableWidgetItem *headerItem = tableWidget->horizontalHeaderItem(i);
            if (headerItem)
            {
                headerItem->setTextAlignment(Qt::AlignCenter);
            }
        }
    }
    else
    {
        // 即使列数没有变化，也需要刷新表头内容以反映管脚类型的更改
        QStringList headers;
        for (const auto &col : columns)
        {
            // 根据列类型设置表头
            if (col.type == Vector::ColumnDataType::PIN_STATE_ID && !col.data_properties.isEmpty())
            {
                // 获取管脚属性
                int channelCount = col.data_properties["channel_count"].toInt(1);
                int typeId = col.data_properties["type_id"].toInt(1);

                // 获取类型名称
                QString typeName = "In"; // 默认为输入类型
                QSqlQuery typeQuery(DatabaseManager::instance()->database());
                typeQuery.prepare("SELECT type_name FROM type_options WHERE id = ?");
                typeQuery.addBindValue(typeId);
                if (typeQuery.exec() && typeQuery.next())
                {
                    typeName = typeQuery.value(0).toString();
                }

                // 创建带有管脚信息的表头
                QString headerText = col.name + "\nx" + QString::number(channelCount) + "\n" + typeName;
                headers << headerText;
            }
            else
            {
                // 标准列，直接使用列名
                headers << col.name;
            }
        }

        tableWidget->setHorizontalHeaderLabels(headers);

        // 设置表头居中对齐
        for (int i = 0; i < tableWidget->columnCount(); ++i)
        {
            QTableWidgetItem *headerItem = tableWidget->horizontalHeaderItem(i);
            if (headerItem)
            {
                headerItem->setTextAlignment(Qt::AlignCenter);
            }
        }
    }

    // 4. 读取指定范围的行数据
    QList<Vector::RowData> pageRows;
    QList<Vector::ColumnInfo> allColumns;

    // 获取完整列信息（包括隐藏列）
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery colQuery(db);
    colQuery.prepare("SELECT id, column_name, column_order, column_type, data_properties, IsVisible FROM VectorTableColumnConfiguration WHERE master_record_id = ? ORDER BY column_order");
    colQuery.addBindValue(tableId);

    if (!colQuery.exec())
    {
        qWarning() << funcName << " - 查询完整列结构失败, 错误:" << colQuery.lastError().text();
        tableWidget->setRowCount(0);

        // 恢复更新和信号
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);
        return false;
    }

    // 构建列映射
    QMap<int, int> columnIndexMapping;
    int visibleColIndex = 0;

    while (colQuery.next())
    {
        Vector::ColumnInfo col;
        col.id = colQuery.value(0).toInt();
        col.vector_table_id = tableId;
        col.name = colQuery.value(1).toString();
        col.order = colQuery.value(2).toInt();
        col.original_type_str = colQuery.value(3).toString();
        col.type = Vector::columnDataTypeFromString(col.original_type_str);
        col.is_visible = colQuery.value(5).toBool();

        QString propStr = colQuery.value(4).toString();
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

        // 只为可见列创建映射
        if (col.is_visible)
        {
            // 原始索引 -> 可见索引的映射
            columnIndexMapping[col.order] = visibleColIndex++;
        }
    }

    // 使用BinaryFileHelper读取指定范围的行数据
    QFile file(absoluteBinFilePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << funcName << " - 无法打开二进制文件:" << absoluteBinFilePath;
        tableWidget->setRowCount(0);

        // 恢复更新和信号
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);
        return false;
    }

    // 读取文件头
    BinaryFileHeader header;
    if (!Persistence::BinaryFileHelper::readBinaryHeader(&file, header))
    {
        qWarning() << funcName << " - 无法读取二进制文件头:" << absoluteBinFilePath;
        file.close();
        tableWidget->setRowCount(0);

        // 恢复更新和信号
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);
        return false;
    }

    // 验证行范围
    if (startRow >= header.row_count_in_file)
    {
        qWarning() << funcName << " - 起始行(" << startRow << ")超出文件总行数(" << header.row_count_in_file << ")";
        file.close();
        tableWidget->setRowCount(0);

        // 恢复更新和信号
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);
        return false;
    }

    // 准备数据结构
    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);

    // 跳过文件头

    // 跳过前面的行数据
    for (int i = 0; i < startRow; ++i)
    {
        quint32 rowBlockSize = 0;
        in >> rowBlockSize;
        if (in.status() != QDataStream::Ok || rowBlockSize == 0)
        {
            qWarning() << funcName << " - 读取行" << i << "的块大小失败";
            file.close();
            tableWidget->setRowCount(0);

            // 恢复更新和信号
            tableWidget->blockSignals(false);
            tableWidget->verticalHeader()->setUpdatesEnabled(true);
            tableWidget->horizontalHeader()->setUpdatesEnabled(true);
            tableWidget->setUpdatesEnabled(true);
            return false;
        }
        
        // 对跳过的行大小也进行检查
        const quint32 MAX_REASONABLE_ROW_SIZE = 1 * 1024 * 1024; // 1MB 是合理的单行最大值
        if (rowBlockSize > MAX_REASONABLE_ROW_SIZE) 
        {
            qCritical() << funcName << " - 跳过行时检测到异常大的行大小:" << rowBlockSize 
                       << "字节，超过合理限制" << MAX_REASONABLE_ROW_SIZE << "字节，行:" << i << ". 可能是文件损坏.";
            
            // 将大小重置为合理值，防止seek操作超过文件边界
            rowBlockSize = qMin(rowBlockSize, static_cast<quint32>(file.bytesAvailable()));
            
            if (rowBlockSize == 0) {
                qWarning() << funcName << " - 文件中没有更多可用数据，无法继续跳过行";
                file.close();
                tableWidget->setRowCount(0);
                tableWidget->blockSignals(false);
                tableWidget->verticalHeader()->setUpdatesEnabled(true);
                tableWidget->horizontalHeader()->setUpdatesEnabled(true);
                tableWidget->setUpdatesEnabled(true);
                return false;
            }
        }

        // 跳过这一行的数据
        if (!file.seek(file.pos() + rowBlockSize))
        {
            qWarning() << funcName << " - 无法跳过行" << i << "的数据";
            file.close();
            tableWidget->setRowCount(0);

            // 恢复更新和信号
            tableWidget->blockSignals(false);
            tableWidget->verticalHeader()->setUpdatesEnabled(true);
            tableWidget->horizontalHeader()->setUpdatesEnabled(true);
            tableWidget->setUpdatesEnabled(true);
            return false;
        }
    }

    // 读取当前页的行数据
    pageRows.reserve(rowsToLoad);

    // 记录加载开始时间，用于计算性能统计
    QDateTime startTime = QDateTime::currentDateTime();
    int lastReportedPercent = -1;
    const int LOG_INTERVAL = 100; // 分页加载使用较小的日志间隔

    for (int i = 0; i < rowsToLoad; ++i)
    {
        quint32 rowBlockSize = 0;
        in >> rowBlockSize;
        if (in.status() != QDataStream::Ok || rowBlockSize == 0)
        {
            qWarning() << funcName << " - 读取块大小失败，行:" << (startRow + i);
            break;
        }
        
        // 添加对行大小的安全检查
        const quint32 MAX_REASONABLE_ROW_SIZE = 1 * 1024 * 1024; // 1MB 是合理的单行最大值
        if (rowBlockSize > MAX_REASONABLE_ROW_SIZE) 
        {
            qCritical() << funcName << " - 检测到异常大的行大小:" << rowBlockSize 
                       << "字节，超过合理限制" << MAX_REASONABLE_ROW_SIZE << "字节，行:" << (startRow + i) << ". 可能是文件损坏.";
                       
            // 创建一个空行，允许继续处理其他行
            Vector::RowData emptyRow;
            for (int j = 0; j < allColumns.size(); ++j) {
                emptyRow.append(QVariant());
            }
            pageRows.append(emptyRow);
            
            // 尝试跳过这个巨大的数据块，如果可能的话
            if (file.bytesAvailable() >= rowBlockSize) {
                file.skip(rowBlockSize);
            } else {
                // 如果无法跳过，则中断处理
                qWarning() << funcName << " - 无法跳过大型数据块，中断处理";
                break;
            }
            
            continue; // 跳过后面的处理，进入下一行
        }

        // 读取这一行的数据
        QByteArray rowDataBlock;
        try {
        rowDataBlock.resize(rowBlockSize);
        } catch (const std::bad_alloc&) {
            qCritical() << funcName << " - 内存分配失败，无法为行" << (startRow + i) << "分配" << rowBlockSize << "字节的内存";
            
            // 创建一个空行并继续
            Vector::RowData emptyRow;
            for (int j = 0; j < allColumns.size(); ++j) {
                emptyRow.append(QVariant());
            }
            pageRows.append(emptyRow);
            
            // 尝试跳过这个无法分配内存的数据块
            if (file.bytesAvailable() >= rowBlockSize) {
                file.skip(rowBlockSize);
            } else {
                break;
            }
            
            continue;
        }
        qint64 bytesRead = in.readRawData(rowDataBlock.data(), rowBlockSize);

        if (bytesRead != rowBlockSize)
        {
            qWarning() << funcName << " - 读取行数据失败，行:" << (startRow + i);
            break;
        }

        // 反序列化这一行
        Vector::RowData rowData;
        if (!Persistence::BinaryFileHelper::deserializeRow(rowDataBlock, allColumns, header.data_schema_version, rowData))
        {
            qWarning() << funcName << " - 反序列化行失败，行:" << (startRow + i);
            // 创建一个空行
            rowData.clear();
            for (int j = 0; j < allColumns.size(); ++j)
            {
                rowData.append(QVariant());
            }
        }

        pageRows.append(rowData);

        // 记录进度
        if (rowsToLoad > LOG_INTERVAL)
        {
            int percent = static_cast<int>((i + 1) * 100 / rowsToLoad);
            if (percent != lastReportedPercent && (i % LOG_INTERVAL == LOG_INTERVAL - 1 || percent - lastReportedPercent >= 10))
            {
                QDateTime currentTime = QDateTime::currentDateTime();
                qint64 elapsedMs = startTime.msecsTo(currentTime);
                double rowsPerSecond = (elapsedMs > 0) ? ((i + 1) * 1000.0 / elapsedMs) : 0;
                qDebug() << funcName << " - 页面数据加载进度: " << (i + 1) << "/" << rowsToLoad
                         << " (" << percent << "%), 速度: " << rowsPerSecond << " 行/秒";
                lastReportedPercent = percent;
            }
        }
    }

    QDateTime endTime = QDateTime::currentDateTime();
    qint64 totalElapsedMs = startTime.msecsTo(endTime);
    if (pageRows.size() > 0 && totalElapsedMs > 100)
    { // 仅当加载时间超过100毫秒时记录
        double avgRowsPerSecond = (totalElapsedMs > 0) ? (pageRows.size() * 1000.0 / totalElapsedMs) : 0;
        qDebug() << funcName << " - 页面数据加载完成: 读取" << pageRows.size() << "行, 耗时: "
                 << (totalElapsedMs / 1000.0) << "秒, 平均速度: " << avgRowsPerSecond << "行/秒";
    }

    file.close();

    // 5. 填充表格
    // 创建列ID到索引的映射，加速查找
    QMap<int, int> columnIdToIndexMap;
    for (int i = 0; i < allColumns.size(); ++i)
    {
        columnIdToIndexMap[allColumns[i].id] = i;
    }

    for (int row = 0; row < pageRows.size(); ++row)
    {
        const Vector::RowData &originalRowData = pageRows[row];

        // 遍历可见列
        for (int visibleColIdx = 0; visibleColIdx < columns.size(); ++visibleColIdx)
        {
            const auto &visibleCol = columns[visibleColIdx];

            // 查找此可见列在原始数据中的索引位置
            if (!columnIdToIndexMap.contains(visibleCol.id))
            {
                // 使用默认值
                QTableWidgetItem *newItem = new QTableWidgetItem();
                if (visibleCol.type == Vector::ColumnDataType::PIN_STATE_ID)
                {
                    newItem->setText("X");
                }
                else
                {
                    newItem->setText("");
                }
                tableWidget->setItem(row, visibleColIdx, newItem);
                continue;
            }

            // 获取原始列索引
            int originalColIdx = columnIdToIndexMap[visibleCol.id];

            if (originalColIdx >= originalRowData.size())
            {
                // 使用默认值
                QTableWidgetItem *newItem = new QTableWidgetItem();
                if (visibleCol.type == Vector::ColumnDataType::PIN_STATE_ID)
                {
                    newItem->setText("X");
                }
                else
                {
                    newItem->setText("");
                }
                tableWidget->setItem(row, visibleColIdx, newItem);
                continue;
            }

            // 获取原始值
            QVariant originalValue = originalRowData[originalColIdx];

            // 根据列类型处理数据
            if (visibleCol.type == Vector::ColumnDataType::PIN_STATE_ID)
            {
                // 设置管脚列的值
                QString pinStateText;
                if (originalValue.isNull() || !originalValue.isValid() || originalValue.toString().isEmpty())
                {
                    pinStateText = "X"; // 默认值
                }
                else
                {
                    pinStateText = originalValue.toString();
                }

                // [修复] 不再使用 setCellWidget，而是创建 QTableWidgetItem
                // 这样可以恢复单元格的标准选择行为
                QTableWidgetItem *newItem = new QTableWidgetItem(pinStateText);
                tableWidget->setItem(row, visibleColIdx, newItem);
            }
            else if (visibleCol.type == Vector::ColumnDataType::INSTRUCTION_ID)
            {
                // 处理INSTRUCTION_ID类型，从缓存获取指令文本
                int instructionId = originalValue.toInt();
                QString instructionText;

                // 从缓存中获取指令文本
                if (m_instructionCache.contains(instructionId))
                {
                    instructionText = m_instructionCache.value(instructionId);
                }
                else
                {
                    // 缓存中没有，从数据库获取
                    QSqlQuery instructionQuery(db);
                    instructionQuery.prepare("SELECT instruction_value FROM instruction_options WHERE id = ?");
                    instructionQuery.addBindValue(instructionId);

                    if (instructionQuery.exec() && instructionQuery.next())
                    {
                        instructionText = instructionQuery.value(0).toString();
                        // 添加到缓存
                        m_instructionCache[instructionId] = instructionText;
                    }
                    else
                    {
                        instructionText = QString("未知(%1)").arg(instructionId);
                    }
                }

                QTableWidgetItem *newItem = new QTableWidgetItem(instructionText);
                newItem->setData(Qt::UserRole, instructionId);
                tableWidget->setItem(row, visibleColIdx, newItem);
            }
            else if (visibleCol.type == Vector::ColumnDataType::TIMESET_ID)
            {
                // 处理TIMESET_ID类型，从缓存获取TimeSet文本
                int timesetId = originalValue.toInt();
                QString timesetText;

                // 从缓存中获取TimeSet文本
                if (m_timesetCache.contains(timesetId))
                {
                    timesetText = m_timesetCache.value(timesetId);
                }
                else
                {
                    // 缓存中没有，从数据库获取
                    QSqlQuery timesetQuery(db);
                    timesetQuery.prepare("SELECT timeset_name FROM timeset_list WHERE id = ?");
                    timesetQuery.addBindValue(timesetId);

                    if (timesetQuery.exec() && timesetQuery.next())
                    {
                        timesetText = timesetQuery.value(0).toString();
                        // 添加到缓存
                        m_timesetCache[timesetId] = timesetText;
                    }
                    else
                    {
                        timesetText = QString("未知(%1)").arg(timesetId);
                    }
                }

                QTableWidgetItem *newItem = new QTableWidgetItem(timesetText);
                newItem->setData(Qt::UserRole, timesetId);
                tableWidget->setItem(row, visibleColIdx, newItem);
            }
            else if (visibleCol.type == Vector::ColumnDataType::BOOLEAN)
            {
                // 对于布尔值处理 (Capture)
                bool boolValue = originalValue.toBool();
                QString displayText = boolValue ? "Y" : "N";

                QTableWidgetItem *newItem = new QTableWidgetItem(displayText);
                newItem->setData(Qt::UserRole, boolValue);
                tableWidget->setItem(row, visibleColIdx, newItem);
                qDebug() << funcName << " - 行" << row << ", 列" << visibleColIdx
                         << " 设置布尔值为:" << displayText << "(" << boolValue << ")";
            }
            else
            {
                // 其他类型的列，直接创建QTableWidgetItem
                QTableWidgetItem *newItem = new QTableWidgetItem();
                QVariant originalValue = originalRowData[originalColIdx]; // 添加声明

                // 根据具体类型设置文本
                if (visibleCol.type == Vector::ColumnDataType::INTEGER)
                {
                    newItem->setText(originalValue.isValid() ? QString::number(originalValue.toInt()) : "");
                    newItem->setData(Qt::UserRole, originalValue.isValid() ? originalValue.toInt() : 0);
                }
                else if (visibleCol.type == Vector::ColumnDataType::REAL)
                {
                    newItem->setText(originalValue.isValid() ? QString::number(originalValue.toDouble()) : "");
                    newItem->setData(Qt::UserRole, originalValue.isValid() ? originalValue.toDouble() : 0.0);
                }
                else
                {
                    // TEXT 或其他类型，直接转换为字符串
                    newItem->setText(originalValue.toString());
                }

                tableWidget->setItem(row, visibleColIdx, newItem);
                qDebug() << funcName << " - 行" << row << ", 列" << visibleColIdx
                         << " 设置为:" << newItem->text();
            }
        }
    }

    // 显示行号(从startRow+1开始，而不是从1开始)
    for (int i = 0; i < rowsToLoad; i++)
    {
        QTableWidgetItem *rowHeaderItem = new QTableWidgetItem(QString::number(startRow + i + 1));
        tableWidget->setVerticalHeaderItem(i, rowHeaderItem);
    }

    // 在函数结束前恢复更新和信号
    tableWidget->blockSignals(false);

    // 恢复滚动条位置
    if (vScrollBar)
    {
        vScrollBar->setValue(vScrollValue);
    }
    if (hScrollBar)
    {
        hScrollBar->setValue(hScrollValue);
    }

    // 恢复UI更新
    tableWidget->verticalHeader()->setUpdatesEnabled(true);
    tableWidget->horizontalHeader()->setUpdatesEnabled(true);
    tableWidget->setUpdatesEnabled(true);

    // 强制刷新视图
    tableWidget->viewport()->update();

    qDebug() << funcName << " - 分页数据加载完成，共" << pageRows.size() << "行";
    return true;
}
