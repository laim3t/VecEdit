#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QDateTime>
#include <QElapsedTimer> // 添加QElapsedTimer头文件
#include <QHash>

void RobustVectorDataHandler::clearTableDataCache(int tableId)
{
    qWarning() << "RobustVectorDataHandler::clearTableDataCache is not implemented yet.";
}

void RobustVectorDataHandler::clearAllTableDataCache()
{
    qWarning() << "RobustVectorDataHandler::clearAllTableDataCache is not implemented yet.";
}

bool RobustVectorDataHandler::loadVectorTablePageData(int tableId, QTableWidget *tableWidget, int pageIndex, int pageSize)
{
    const QString funcName = "RobustVectorDataHandler::loadVectorTablePageData";
    qDebug() << funcName << " - 开始加载分页数据, 表ID:" << tableId
             << ", 页码:" << pageIndex << ", 每页行数:" << pageSize;

    if (!tableWidget)
    {
        qWarning() << funcName << " - tableWidget 为空";
        return false;
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

    try
    {
        // 1. 使用新的独立方法获取列信息和总行数
        QString binFileName;
        QList<Vector::ColumnInfo> columns;
        int schemaVersion = 0;
        int totalRowCount = 0;

        if (!loadVectorTableMeta(tableId, binFileName, columns, schemaVersion, totalRowCount))
        {
            qWarning() << funcName << " - 无法加载表元数据";

            // 恢复更新和信号
            tableWidget->blockSignals(false);
            tableWidget->verticalHeader()->setUpdatesEnabled(true);
            tableWidget->horizontalHeader()->setUpdatesEnabled(true);
            tableWidget->setUpdatesEnabled(true);
            return false;
        }

        qDebug() << funcName << " - 元数据加载成功, 列数:" << columns.size() << ", 总行数:" << totalRowCount;

        // 过滤仅保留可见的列
        QList<Vector::ColumnInfo> visibleColumns;
        for (const auto &col : columns)
        {
            if (col.is_visible)
            {
                visibleColumns.append(col);
            }
        }

        if (visibleColumns.isEmpty())
        {
            qWarning() << funcName << " - 表 " << tableId << " 没有可见的列";
            tableWidget->setRowCount(0);
            tableWidget->setColumnCount(0);

            // 恢复更新和信号
            tableWidget->blockSignals(false);
            tableWidget->verticalHeader()->setUpdatesEnabled(true);
            tableWidget->horizontalHeader()->setUpdatesEnabled(true);
            tableWidget->setUpdatesEnabled(true);
            return true; // 返回true，因为元数据已加载成功，只是没有可见列
        }

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

        // 2. 设置表格列
        tableWidget->setColumnCount(visibleColumns.size());

        // 设置表头
        QStringList headers;
        for (const auto &col : visibleColumns)
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

        // 3. 解析二进制文件路径
        QString errorMsg;
        QString absoluteBinFilePath = resolveBinaryFilePath(tableId, errorMsg);
        if (absoluteBinFilePath.isEmpty())
        {
            qWarning() << funcName << " - 无法解析二进制文件路径: " << errorMsg;

            // 恢复更新和信号
            tableWidget->blockSignals(false);
            tableWidget->verticalHeader()->setUpdatesEnabled(true);
            tableWidget->horizontalHeader()->setUpdatesEnabled(true);
            tableWidget->setUpdatesEnabled(true);
            return false;
        }

        // 4. 读取页面数据
        QList<Vector::RowData> pageRows;
        if (!readPageDataFromBinary(absoluteBinFilePath, columns, schemaVersion, startRow, rowsToLoad, pageRows))
        {
            qWarning() << funcName << " - 无法读取二进制文件数据";

            // 恢复更新和信号
            tableWidget->blockSignals(false);
            tableWidget->verticalHeader()->setUpdatesEnabled(true);
            tableWidget->horizontalHeader()->setUpdatesEnabled(true);
            tableWidget->setUpdatesEnabled(true);
            return false;
        }

        if (pageRows.isEmpty() && totalRowCount > 0)
        {
            qWarning() << funcName << " - 无法获取页面数据";
            tableWidget->setRowCount(0);

            // 恢复更新和信号
            tableWidget->blockSignals(false);
            tableWidget->verticalHeader()->setUpdatesEnabled(true);
            tableWidget->horizontalHeader()->setUpdatesEnabled(true);
            tableWidget->setUpdatesEnabled(true);
            return false;
        }

        // 5. 填充表格数据
        QSqlDatabase db = DatabaseManager::instance()->database();
        for (int row = 0; row < pageRows.size(); ++row)
        {
            const auto &rowData = pageRows[row];
            int visibleColIdx = 0;

            for (int colIndex = 0; colIndex < columns.size(); ++colIndex)
            {
                const auto &column = columns[colIndex];

                // 跳过不可见的列
                if (!column.is_visible)
                {
                    continue;
                }

                if (rowData.size() <= colIndex)
                {
                    qWarning() << funcName << " - 行数据不足，跳过剩余列, 行:" << row;
                    break;
                }

                const QVariant &cellValue = rowData[colIndex];

                // 根据列类型创建表格项
                QTableWidgetItem *item = nullptr;

                switch (column.type)
                {
                case Vector::ColumnDataType::INSTRUCTION_ID:
                {
                    int instructionId = cellValue.toInt();
                    QString instructionText = "Unknown";

                    // 获取指令文本
                    QSqlQuery instructionQuery(db);
                    instructionQuery.prepare("SELECT instruction_text FROM instructions WHERE id = ?");
                    instructionQuery.addBindValue(instructionId);
                    if (instructionQuery.exec() && instructionQuery.next())
                    {
                        instructionText = instructionQuery.value(0).toString();
                    }

                    item = new QTableWidgetItem(instructionText);
                    item->setData(Qt::UserRole, instructionId); // 保存原始ID
                    break;
                }
                case Vector::ColumnDataType::TIMESET_ID:
                {
                    int timesetId = cellValue.toInt();
                    QString timesetName = "Unknown";

                    // 获取TimeSet名称
                    QSqlQuery timesetQuery(db);
                    timesetQuery.prepare("SELECT timeset_name FROM timeset WHERE id = ?");
                    timesetQuery.addBindValue(timesetId);
                    if (timesetQuery.exec() && timesetQuery.next())
                    {
                        timesetName = timesetQuery.value(0).toString();
                    }

                    item = new QTableWidgetItem(timesetName);
                    item->setData(Qt::UserRole, timesetId); // 保存原始ID
                    break;
                }
                case Vector::ColumnDataType::PIN_STATE_ID:
                {
                    // 创建自定义的管脚状态编辑器
                    PinValueLineEdit *pinEdit = new PinValueLineEdit(tableWidget);
                    pinEdit->setAlignment(Qt::AlignCenter);
                    pinEdit->setText(cellValue.toString());
                    tableWidget->setCellWidget(row, visibleColIdx, pinEdit);

                    // 创建一个隐藏项来保存值，以便于后续访问
                    item = new QTableWidgetItem(cellValue.toString());
                    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                    break;
                }
                case Vector::ColumnDataType::BOOLEAN:
                {
                    bool isChecked = cellValue.toBool();
                    item = new QTableWidgetItem();
                    item->setCheckState(isChecked ? Qt::Checked : Qt::Unchecked);
                    item->setTextAlignment(Qt::AlignCenter);
                    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
                    break;
                }
                default:
                    // 文本或其他类型
                    item = new QTableWidgetItem(cellValue.toString());
                    break;
                }

                if (item)
                {
                    tableWidget->setItem(row, visibleColIdx, item);

                    // 设置文本居中对齐
                    if (column.type != Vector::ColumnDataType::PIN_STATE_ID)
                    {
                        item->setTextAlignment(Qt::AlignCenter);
                    }
                }

                visibleColIdx++;
            }
        }

        // 6. 设置行标题为行号
        QStringList rowLabels;
        for (int i = 0; i < rowsToLoad; ++i)
        {
            rowLabels << QString::number(startRow + i + 1); // 显示1-based的行号
        }
        tableWidget->setVerticalHeaderLabels(rowLabels);

        // 恢复滚动条位置
        if (vScrollBar)
            vScrollBar->setValue(vScrollValue);
        if (hScrollBar)
            hScrollBar->setValue(hScrollValue);

        // 恢复信号和更新
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);

        return true;
    }
    catch (const std::exception &e)
    {
        qCritical() << funcName << " - 加载失败，异常:" << e.what();

        // 恢复信号和更新
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);

        return false;
    }
    catch (...)
    {
        qCritical() << funcName << " - 加载失败，未知异常";

        // 恢复信号和更新
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);

        return false;
    }
}

bool RobustVectorDataHandler::loadVectorTablePageDataForModel(int tableId, VectorTableModel *model, int pageIndex, int pageSize)
{
    qWarning() << "RobustVectorDataHandler::loadVectorTablePageDataForModel is not implemented yet.";
    return false;
}

QList<QList<QVariant>> RobustVectorDataHandler::getPageData(int tableId, int pageIndex, int pageSize)
{
    const QString funcName = "RobustVectorDataHandler::getPageData";
    QString errorMessage;

    // 1. 加载元数据
    QString binFileName;
    QList<Vector::ColumnInfo> columns;
    int schemaVersion = 0;
    int totalRowCount = 0;
    if (!loadVectorTableMeta(tableId, binFileName, columns, schemaVersion, totalRowCount))
    {
        qWarning() << funcName << " - Failed to load meta for table" << tableId;
        return {};
    }

    // 2. 计算分页参数
    int startRow = pageIndex * pageSize;
    if (startRow >= totalRowCount)
    {
        return {}; // 页码超出范围，返回空列表
    }
    int numRows = std::min(pageSize, totalRowCount - startRow);

    // 3. 获取二进制文件路径
    QString absoluteBinFilePath = resolveBinaryFilePath(tableId, errorMessage);
    if (absoluteBinFilePath.isEmpty())
    {
        qWarning() << funcName << " - " << errorMessage;
        return {};
    }

    // 4. 从二进制文件读取数据
    QList<QList<QVariant>> pageRows;
    if (!Persistence::BinaryFileHelper::readPageDataFromBinary(absoluteBinFilePath, columns, schemaVersion, startRow, numRows, pageRows))
    {
        qWarning() << funcName << " - Failed to read page data from binary file for table" << tableId;
        return {};
    }

    qDebug() << funcName << " - Successfully read" << pageRows.size() << "rows for table" << tableId << "page" << pageIndex;

    return pageRows;
}

QList<Vector::ColumnInfo> RobustVectorDataHandler::getVisibleColumns(int tableId)
{
    const QString funcName = "RobustVectorDataHandler::getVisibleColumns";
    QList<Vector::ColumnInfo> result;

    // 获取所有列信息
    QString binFileName;
    QList<Vector::ColumnInfo> allColumns;
    int schemaVersion = 0;
    int totalRowCount = 0;

    if (!loadVectorTableMeta(tableId, binFileName, allColumns, schemaVersion, totalRowCount))
    {
        qWarning() << funcName << " - 无法加载表元数据";
        return result;
    }

    // 过滤出可见的列
    for (const auto &column : allColumns)
    {
        if (column.is_visible)
        {
            result.append(column);
        }
    }

    return result;
}

void RobustVectorDataHandler::markRowAsModified(int tableId, int rowIndex)
{
    // 确保表ID存在于映射中
    if (!m_modifiedRows.contains(tableId))
    {
        m_modifiedRows[tableId] = QSet<int>();
    }

    // 将行索引添加到修改集合中
    m_modifiedRows[tableId].insert(rowIndex);

    // 日志限制 - 每20000行只输出一次
    static int logCounter = 0;
    const int LOG_THRESHOLD = 20000;

    if (++logCounter % LOG_THRESHOLD == 0)
    {
        qDebug() << "RobustVectorDataHandler::markRowAsModified - 已累计标记" << LOG_THRESHOLD
                 << "行为已修改，当前表" << tableId << "的行" << rowIndex << "，该表共标记"
                 << m_modifiedRows[tableId].size() << "行";
    }
}

void RobustVectorDataHandler::clearModifiedRows(int tableId)
{
    // 如果指定了特定的表ID，则只清除该表的修改标记
    if (tableId > 0)
    {
        if (m_modifiedRows.contains(tableId))
        {
            m_modifiedRows[tableId].clear();
            qDebug() << "RobustVectorDataHandler::clearModifiedRows - 已清除表" << tableId << "的所有修改标记";
        }
    }
    // 否则清除所有表的修改标记
    else
    {
        m_modifiedRows.clear();
        qDebug() << "RobustVectorDataHandler::clearModifiedRows - 已清除所有表的修改标记";
    }
}

bool RobustVectorDataHandler::isRowModified(int tableId, int rowIndex)
{
    // 当rowIndex为-1时，检查整个表是否有任何修改
    if (rowIndex == -1)
    {
        return m_modifiedRows.contains(tableId) && !m_modifiedRows[tableId].isEmpty();
    }

    // 检查特定行是否被修改
    return m_modifiedRows.contains(tableId) && m_modifiedRows[tableId].contains(rowIndex);
}

QString RobustVectorDataHandler::resolveBinaryFilePath(int tableId, QString &errorMsg)
{
    const QString funcName = "RobustVectorDataHandler::resolveBinaryFilePath";
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        errorMsg = "数据库未打开";
        qWarning() << funcName << " - " << errorMsg;
        return QString();
    }

    // 1. 查询二进制文件名
    QSqlQuery query(db);
    query.prepare("SELECT binary_data_filename FROM VectorTableMasterRecord WHERE id = ?");
    query.addBindValue(tableId);
    if (!query.exec() || !query.next())
    {
        errorMsg = "无法获取表 " + QString::number(tableId) + " 的记录: " + query.lastError().text();
        qWarning() << funcName << " - " << errorMsg;
        return QString();
    }
    QString binFileName = query.value(0).toString();
    if (binFileName.isEmpty())
    {
        errorMsg = "表 " + QString::number(tableId) + " 没有关联的二进制文件";
        qWarning() << funcName << " - " << errorMsg;
        return QString();
    }

    // 2. 获取数据库文件路径
    QString dbPath = db.databaseName();
    if (dbPath.isEmpty() || dbPath == ":memory:")
    {
        errorMsg = "无法确定数据库文件路径";
        qWarning() << funcName << " - " << errorMsg;
        return QString();
    }

    // 3. 使用PathUtils获取正确的二进制数据目录
    QString binaryDataDir = Utils::PathUtils::getProjectBinaryDataDirectory(dbPath);
    if (binaryDataDir.isEmpty())
    {
        errorMsg = "无法生成项目二进制数据目录";
        qWarning() << funcName << " - " << errorMsg;
        return QString();
    }

    // 4. 构建并返回最终的绝对路径
    QDir dir(binaryDataDir);
    QString fullPath = dir.absoluteFilePath(binFileName);

    // 文件存在性检查是可选的，因为在创建新表时文件可能尚不存在
    // QFileInfo fileInfo(fullPath);
    // if (!fileInfo.exists())
    // {
    //     qWarning() << funcName << " - 警告: 二进制文件不存在: " << fullPath;
    // }

    qDebug() << funcName << " - 解析后的二进制文件路径: " << fullPath;
    return fullPath;
}

bool RobustVectorDataHandler::loadVectorTableMeta(int tableId, QString &binFileName, QList<Vector::ColumnInfo> &columns,
                                                  int &schemaVersion, int &totalRowCount)
{
    const QString funcName = "RobustVectorDataHandler::loadVectorTableMeta";
    qDebug() << funcName << " - Querying metadata for table ID:" << tableId;

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << funcName << " - Database is not open.";
        return false;
    }

    // 1. Query the master record table
    QSqlQuery metaQuery(db);
    metaQuery.prepare("SELECT binary_data_filename, data_schema_version, row_count FROM VectorTableMasterRecord WHERE id = ?");
    metaQuery.addBindValue(tableId);
    if (!metaQuery.exec() || !metaQuery.next())
    {
        qWarning() << funcName << " - Failed to query master record for table ID:" << tableId << ", Error:" << metaQuery.lastError().text();
        return false;
    }

    binFileName = metaQuery.value(0).toString();
    schemaVersion = metaQuery.value(1).toInt();
    totalRowCount = metaQuery.value(2).toInt();
    qDebug() << funcName << " - Master record OK. Filename:" << binFileName << ", schemaVersion:" << schemaVersion << ", rowCount:" << totalRowCount;

    // 2. Query column configuration with the corrected schema
    columns.clear();
    QSqlQuery colQuery(db);
    colQuery.prepare(
        "SELECT id, column_name, column_order, column_type, is_visible "
        "FROM VectorTableColumnConfiguration WHERE master_record_id = ? AND is_visible = 1 ORDER BY column_order");
    colQuery.addBindValue(tableId);
    if (!colQuery.exec())
    {
        qWarning() << funcName << " - Failed to query column structure, Error:" << colQuery.lastError().text();
        return false;
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
        col.is_visible = colQuery.value(4).toBool();

        // 如果是管脚列类型，添加类型信息到data_properties
        if (col.type == Vector::ColumnDataType::PIN_STATE_ID)
        {
            // 查询管脚类型信息
            QSqlQuery pinQuery(db);
            pinQuery.prepare("SELECT pin_type FROM vector_table_pins WHERE table_id = ? AND pin_id = (SELECT id FROM pin_list WHERE pin_name = ?)");
            pinQuery.addBindValue(tableId);
            pinQuery.addBindValue(col.name);

            if (pinQuery.exec() && pinQuery.next())
            {
                int typeId = pinQuery.value(0).toInt();

                // 存储管脚通道数和类型ID
                col.data_properties["pin_type_id"] = typeId;
                col.data_properties["channel_count"] = 1; // 默认值

                // 查询类型名称
                QSqlQuery typeQuery(db);
                typeQuery.prepare("SELECT type_name FROM type_options WHERE id = ?");
                typeQuery.addBindValue(typeId);
                if (typeQuery.exec() && typeQuery.next())
                {
                    QString typeName = typeQuery.value(0).toString();
                    col.data_properties["pin_type_name"] = typeName;
                }
            }
            else
            {
                // 如果查询失败，使用默认值
                col.data_properties["pin_type_name"] = "In";
                col.data_properties["channel_count"] = 1;
            }
        }

        columns.append(col);
    }

    qDebug() << funcName << " - Successfully loaded" << columns.count() << "column configurations.";
    return true;
}

bool RobustVectorDataHandler::readPageDataFromBinary(const QString &absoluteBinFilePath,
                                                     const QList<Vector::ColumnInfo> &columns,
                                                     int schemaVersion,
                                                     int startRow,
                                                     int numRows,
                                                     QList<QList<QVariant>> &pageRows)
{
    // 直接调用Persistence命名空间下的BinaryFileHelper的readPageDataFromBinary方法
    return Persistence::BinaryFileHelper::readPageDataFromBinary(
        absoluteBinFilePath,
        columns,
        schemaVersion,
        startRow,
        numRows,
        pageRows);
}

bool RobustVectorDataHandler::ensureBinaryFilesCompatibility(const QString &dbPath, QString &errorMessage)
{
    const QString funcName = "RobustVectorDataHandler::ensureBinaryFilesCompatibility";

    // 确保数据库已打开
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        errorMessage = "数据库未打开";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 获取二进制数据目录
    QString binaryDataDir = Utils::PathUtils::getProjectBinaryDataDirectory(dbPath);
    if (binaryDataDir.isEmpty())
    {
        errorMessage = "无法确定二进制数据目录";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 确保目录存在
    QDir dir(binaryDataDir);
    if (!dir.exists())
    {
        if (!dir.mkpath("."))
        {
            errorMessage = "无法创建二进制数据目录: " + binaryDataDir;
            qWarning() << funcName << " - " << errorMessage;
            return false;
        }
        qDebug() << funcName << " - 创建二进制数据目录: " << binaryDataDir;
    }

    // 查询所有向量表
    QSqlQuery query(db);
    query.prepare("SELECT id, binary_data_filename, data_schema_version FROM VectorTableMasterRecord");
    if (!query.exec())
    {
        errorMessage = "无法查询向量表记录: " + query.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 处理每个表
    while (query.next())
    {
        int tableId = query.value(0).toInt();
        QString binFileName = query.value(1).toString();
        int schemaVersion = query.value(2).toInt();

        // 检查二进制文件是否存在
        QString binFilePath = dir.absoluteFilePath(binFileName);
        QFileInfo fileInfo(binFilePath);

        if (fileInfo.exists())
        {
            // 文件存在，验证头部
            QFile file(binFilePath);
            if (!file.open(QIODevice::ReadOnly))
            {
                qWarning() << funcName << " - 无法打开二进制文件进行验证: " << binFilePath;
                continue; // 处理下一个表
            }

            Persistence::HeaderData headerData;
            if (!Persistence::BinaryFileHelper::readAndValidateHeader(file, headerData, schemaVersion))
            {
                qWarning() << funcName << " - 二进制文件头部无效: " << binFilePath;
                // 对于新项目，头部无效意味着文件损坏，可以考虑备份并重建
            }
            else
            {
                qDebug() << funcName << " - 文件 " << binFilePath << " 头部有效";
            }
            file.close();
        }
        else
        {
            qDebug() << funcName << " - 表 " << tableId << " 的二进制文件不存在，将创建新文件";

            // 获取列配置
            QList<Vector::ColumnInfo> columns;
            int rowCount;
            if (loadVectorTableMeta(tableId, binFileName, columns, schemaVersion, rowCount))
            {
                // 创建新的二进制文件
                if (Persistence::BinaryFileHelper::initBinaryFile(binFilePath, columns.size(), schemaVersion))
                {
                    qDebug() << funcName << " - 为表 " << tableId << " 创建了新的二进制文件";
                }
                else
                {
                    qWarning() << funcName << " - 无法为表 " << tableId << " 创建新的二进制文件";
                }
            }
            else
            {
                qWarning() << funcName << " - 无法获取表 " << tableId << " 的元数据";
            }
        }
    }

    return true;
}

bool RobustVectorDataHandler::saveDataFromModel(int tableId, const QList<Vector::RowData> &pageData,
                                                int currentPage, int pageSize, int totalRows,
                                                QString &errorMessage)
{
    const QString funcName = "RobustVectorDataHandler::saveDataFromModel";
    qDebug() << funcName << " - 开始从模型保存表ID:" << tableId << "的数据";

    // 检查是否有修改
    if (!isRowModified(tableId, -1))
    {
        qDebug() << funcName << " - 没有检测到修改，跳过保存";
        errorMessage = "没有检测到数据变更，跳过保存";
        return true;
    }

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        errorMessage = "数据库未打开";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 开始计时
    QElapsedTimer timer;
    timer.start();

    // 获取已修改的行
    const QSet<int> modifiedRows = m_modifiedRows.value(tableId);
    qDebug() << funcName << " - 检测到" << modifiedRows.size() << "行数据被修改";

    // 计算当前页的全局起始行索引
    int startRow = currentPage * pageSize;

    // 构建批量更新数据结构
    QMap<int, Vector::RowData> rowsToUpdate;

    // 遍历当前页的数据，收集所有需要更新的行
    int modifiedCount = 0;
    for (int i = 0; i < pageData.size(); ++i)
    {
        int globalRowIndex = startRow + i;

        // 检查此行是否被修改过
        if (!modifiedRows.contains(globalRowIndex))
        {
            continue; // 跳过未修改的行
        }

        // 获取行数据并加入批量更新集合
        rowsToUpdate[globalRowIndex] = pageData.at(i);
        modifiedCount++;
    }

    // 如果没有实际需要更新的行，直接返回成功
    if (rowsToUpdate.isEmpty())
    {
        qDebug() << funcName << " - 没有当前页面上的行需要更新，返回成功";
        return true;
    }

    // 调用批量更新方法，一次性处理所有修改的行
    bool success = batchUpdateVectorRows(tableId, rowsToUpdate, errorMessage);

    if (success)
    {
        // 清除已修改行的标记
        clearModifiedRows(tableId);

        // 记录完成时间
        qint64 elapsedMs = timer.elapsed();
        errorMessage = QString("成功批量保存 %1 行已修改的数据，用时 %2 毫秒。").arg(modifiedCount).arg(elapsedMs);
        qDebug() << funcName << " - " << errorMessage;
    }
    else
    {
        qWarning() << funcName << " - 批量保存失败: " << errorMessage;
    }

    return success;
}
