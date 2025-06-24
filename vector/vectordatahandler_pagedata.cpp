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
