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

    qDebug() << "RobustVectorDataHandler::markRowAsModified - 标记表" << tableId
             << "的行" << rowIndex << "为已修改";
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
        "SELECT id, column_name, column_order, column_type, is_visible " // Removed data_properties, default_value is not needed for loading
        "FROM VectorTableColumnConfiguration WHERE master_record_id = ? ORDER BY column_order");
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

        // data_properties is no longer used in this simplified schema.
        // The logic to parse it has been removed.

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

    // 开始事务
    if (!db.transaction())
    {
        errorMessage = "无法开始数据库事务: " + db.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    bool success = true;
    int modifiedCount = 0;

    // 获取已修改的行
    const QSet<int> modifiedRows = m_modifiedRows.value(tableId);

    // 计算当前页的全局起始行索引
    int startRow = currentPage * pageSize;

    // 遍历当前页的数据
    for (int i = 0; i < pageData.size(); ++i)
    {
        int globalRowIndex = startRow + i;

        // 检查此行是否被修改过
        if (!modifiedRows.contains(globalRowIndex))
        {
            continue; // 跳过未修改的行
        }

        // 获取行数据
        const Vector::RowData &rowData = pageData.at(i);

        // 更新行数据
        QString rowError;
        if (!updateVectorRow(tableId, globalRowIndex, rowData, rowError))
        {
            errorMessage = QString("更新行 %1 失败: %2").arg(globalRowIndex + 1).arg(rowError);
            qWarning() << funcName << " - " << errorMessage;
            success = false;
            break;
        }
        modifiedCount++;
    }

    // 提交或回滚事务
    if (success)
    {
        if (!db.commit())
        {
            errorMessage = "提交事务失败: " + db.lastError().text();
            qWarning() << funcName << " - " << errorMessage;
            db.rollback();
            return false;
        }

        // 清除已修改行的标记
        clearModifiedRows(tableId);
        errorMessage = QString("成功保存 %1 行已修改的数据。").arg(modifiedCount);
        qDebug() << funcName << " - " << errorMessage;
    }
    else
    {
        db.rollback();
        qWarning() << funcName << " - 保存失败，已回滚事务";
    }

    return success;
}

bool RobustVectorDataHandler::batchUpdateVectorColumn(
    int tableId, int columnIndex, const QMap<int, QVariant> &rowValueMap, QString &errorMessage)
{
    const QString funcName = "RobustVectorDataHandler::batchUpdateVectorColumn";

    // 1. 获取表元数据 (只查询一次)
    QString binFileName;
    QList<Vector::ColumnInfo> columns;
    int schemaVersion = 0;
    int totalRowCount = 0;

    if (!loadVectorTableMeta(tableId, binFileName, columns, schemaVersion, totalRowCount))
    {
        errorMessage = "无法加载表元数据";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 2. 检查列索引是否有效
    if (columnIndex < 0 || columnIndex >= columns.size())
    {
        errorMessage = QString("无效的列索引: %1").arg(columnIndex);
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 3. 获取二进制文件路径
    QString binFilePath = resolveBinaryFilePath(tableId, errorMessage);
    if (binFilePath.isEmpty())
    {
        qWarning() << funcName << " - 无法解析二进制文件路径";
        return false;
    }

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        errorMessage = "数据库未连接";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 4. 一次性查询所有需要修改的行的索引信息
    QMap<int, QPair<qint64, quint32>> rowOffsetMap; // 行索引 -> (偏移量, 大小)

    // 构建IN条件的行号列表
    QStringList logicalRowsList;
    for (auto it = rowValueMap.constBegin(); it != rowValueMap.constEnd(); ++it)
    {
        logicalRowsList << QString::number(it.key());
    }

    QString inClause = logicalRowsList.join(',');
    QSqlQuery indexQuery(db);
    indexQuery.prepare(
        "SELECT logical_row_order, offset, size FROM VectorTableRowIndex "
        "WHERE master_record_id = ? AND logical_row_order IN (" +
        inClause + ") AND is_active = 1");
    indexQuery.addBindValue(tableId);

    if (!indexQuery.exec())
    {
        errorMessage = "无法查询行索引信息: " + indexQuery.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    while (indexQuery.next())
    {
        int rowIndex = indexQuery.value(0).toInt();
        qint64 offset = indexQuery.value(1).toLongLong();
        quint32 size = indexQuery.value(2).toUInt();
        rowOffsetMap[rowIndex] = qMakePair(offset, size);
    }

    // 如果某些行不存在，记录警告但继续处理其他行
    if (rowOffsetMap.size() != rowValueMap.size())
    {
        qWarning() << funcName << "- 部分行未找到索引信息，请求行数:"
                   << rowValueMap.size() << "，找到行数:" << rowOffsetMap.size();
    }

    if (rowOffsetMap.isEmpty())
    {
        errorMessage = "未找到任何需要更新的行";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 5. 打开二进制文件进行读写
    QFile binFile(binFilePath);
    if (!binFile.open(QIODevice::ReadWrite))
    {
        errorMessage = "无法打开二进制文件进行读写: " + binFile.errorString();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 开启事务处理
    if (!db.transaction())
    {
        errorMessage = "无法开启数据库事务: " + db.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        binFile.close();
        return false;
    }

    bool success = true;
    int processedCount = 0;
    int successCount = 0;
    QSqlQuery updateIndexQuery(db);
    updateIndexQuery.prepare(
        "UPDATE VectorTableRowIndex SET offset = ?, size = ? "
        "WHERE master_record_id = ? AND logical_row_order = ?");

    const int BATCH_SIZE = 100; // 每批处理行数
    int totalRows = rowOffsetMap.size();

    // 为了提高性能，预先为文件末尾可能的追加操作分配空间
    qint64 appendOffset = binFile.size();

    for (auto it = rowOffsetMap.constBegin(); it != rowOffsetMap.constEnd(); ++it)
    {
        int rowIndex = it.key();
        qint64 oldOffset = it.value().first;
        quint32 oldSize = it.value().second;

        // 从文件读取行数据
        if (!binFile.seek(oldOffset))
        {
            qWarning() << funcName << "- 无法定位到行" << rowIndex << "的数据位置";
            continue;
        }

        QByteArray rowBytes = binFile.read(oldSize);
        if (rowBytes.size() != oldSize)
        {
            qWarning() << funcName << "- 读取行数据失败，行:" << rowIndex
                       << "，读取了" << rowBytes.size() << "字节，应为" << oldSize << "字节";
            continue;
        }

        // 反序列化行数据
        Vector::RowData rowData;
        if (!Persistence::BinaryFileHelper::deserializeRow(rowBytes, rowData))
        {
            qWarning() << funcName << "- 反序列化行数据失败，行:" << rowIndex;
            continue;
        }

        // 确保行数据足够长
        while (rowData.size() <= columnIndex)
        {
            rowData.append(QVariant());
        }

        // 更新指定列的值
        rowData[columnIndex] = rowValueMap[rowIndex];

        // 重新序列化行数据
        QByteArray newRowBytes;
        if (!Persistence::BinaryFileHelper::serializeRowSimple(rowData, newRowBytes))
        {
            qWarning() << funcName << "- 序列化修改后的行数据失败，行:" << rowIndex;
            continue;
        }

        qint64 newOffset;
        quint32 newSize = newRowBytes.size();

        // 策略：如果新数据不大于旧数据则原位更新，否则追加到文件末尾
        if (newSize <= oldSize)
        {
            // 原位更新
            newOffset = oldOffset;
            if (!binFile.seek(oldOffset))
            {
                qWarning() << funcName << "- 无法定位到行" << rowIndex << "的写入位置";
                continue;
            }

            qint64 bytesWritten = binFile.write(newRowBytes);
            if (bytesWritten != newSize)
            {
                qWarning() << funcName << "- 写入行数据失败，行:" << rowIndex;
                continue;
            }
            // 原位更新不需要修改索引表
        }
        else
        {
            // 追加到文件末尾
            newOffset = appendOffset;
            if (!binFile.seek(newOffset))
            {
                qWarning() << funcName << "- 无法定位到文件末尾";
                continue;
            }

            qint64 bytesWritten = binFile.write(newRowBytes);
            if (bytesWritten != newSize)
            {
                qWarning() << funcName << "- 写入行数据失败，行:" << rowIndex;
                continue;
            }
            appendOffset += newSize; // 更新下一行的追加位置

            // 更新索引表中的偏移量和大小
            updateIndexQuery.bindValue(0, newOffset);
            updateIndexQuery.bindValue(1, newSize);
            updateIndexQuery.bindValue(2, tableId);
            updateIndexQuery.bindValue(3, rowIndex);

            if (!updateIndexQuery.exec())
            {
                qWarning() << funcName << "- 更新行索引失败，行:" << rowIndex
                           << ", 错误:" << updateIndexQuery.lastError().text();
                continue;
            }
        }

        successCount++; // 成功更新计数
        processedCount++;

        // 每处理一批次行或处理完成时，输出进度日志
        if (processedCount % BATCH_SIZE == 0 || processedCount == totalRows)
        {
            int percentage = (processedCount * 100) / totalRows;
            qDebug() << funcName << "- 已处理" << processedCount << "行，共" << totalRows
                     << "行 (" << percentage << "%)";
            emit progressUpdated(percentage); // 发出进度信号
        }
    }

    // 6. 提交事务
    if (successCount > 0)
    {
        if (!db.commit())
        {
            errorMessage = "无法提交数据库事务: " + db.lastError().text();
            qWarning() << funcName << " - " << errorMessage;
            db.rollback();
            binFile.close();
            return false;
        }
    }
    else
    {
        db.rollback();
        errorMessage = "未成功更新任何行";
        qWarning() << funcName << " - " << errorMessage;
        binFile.close();
        return false;
    }

    binFile.close();

    qDebug() << funcName << "- 批量更新完成，成功更新" << successCount
             << "行，请求更新" << rowValueMap.size() << "行";

    return successCount > 0;
}

bool RobustVectorDataHandler::batchFillTimeSet(int tableId, const QList<int> &rowIndexes, int timeSetId, QString &errorMessage)
{
    const QString funcName = "RobustVectorDataHandler::batchFillTimeSet";

    // 记录详细的性能数据
    struct PerformanceMetrics
    {
        qint64 dbQueryTime = 0;
        qint64 readTime = 0;
        qint64 deserializeTime = 0;
        qint64 serializeTime = 0;
        qint64 writeTime = 0;
        qint64 indexUpdateTime = 0;
        qint64 totalTime = 0;
        int rowCount = 0;
        int bytesProcessed = 0;
    };

    QElapsedTimer timer, stageTimer, operationTimer;
    timer.start(); // 开始总计时

    PerformanceMetrics metrics;
    metrics.rowCount = rowIndexes.size();

    qDebug() << funcName << " - 开始批量填充TimeSet（批量追加-批量索引更新模式）";
    qDebug() << funcName << " - 表ID:" << tableId << "，行数:" << rowIndexes.size() << "，TimeSet ID:" << timeSetId;

    // 1. 获取表元数据 (只查询一次)
    QString binFileName;
    QList<Vector::ColumnInfo> columns;
    int schemaVersion = 0;
    int totalRowCount = 0;

    if (!loadVectorTableMeta(tableId, binFileName, columns, schemaVersion, totalRowCount))
    {
        errorMessage = "无法加载表元数据";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 2. 查找TimeSet列的索引
    int timesetColumnIndex = -1;
    for (int i = 0; i < columns.size(); ++i)
    {
        if (columns[i].type == Vector::ColumnDataType::TIMESET_ID)
        {
            timesetColumnIndex = i;
            break;
        }
    }

    if (timesetColumnIndex < 0)
    {
        errorMessage = "找不到TimeSet列";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 3. 获取二进制文件路径
    QString binFilePath = resolveBinaryFilePath(tableId, errorMessage);
    if (binFilePath.isEmpty())
    {
        qWarning() << funcName << " - 无法解析二进制文件路径";
        return false;
    }

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        errorMessage = "数据库未连接";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 4. 一次性查询所有需要修改的行的索引信息
    QMap<int, QPair<qint64, quint32>> rowOffsetMap; // 行索引 -> (偏移量, 大小)

    // 构建IN条件的行号列表
    QStringList logicalRowsList;
    foreach (int rowIndex, rowIndexes)
    {
        logicalRowsList << QString::number(rowIndex);
    }

    QString inClause = logicalRowsList.isEmpty() ? "" : " AND logical_row_order IN (" + logicalRowsList.join(',') + ")";

    stageTimer.start();
    QSqlQuery indexQuery(db);

    // 如果没有指定行，处理所有行
    if (rowIndexes.isEmpty())
    {
        indexQuery.prepare(
            "SELECT logical_row_order, offset, size FROM VectorTableRowIndex "
            "WHERE master_record_id = ? AND is_active = 1");
        indexQuery.addBindValue(tableId);
    }
    else
    {
        indexQuery.prepare(
            "SELECT logical_row_order, offset, size FROM VectorTableRowIndex "
            "WHERE master_record_id = ? AND is_active = 1" +
            inClause);
        indexQuery.addBindValue(tableId);
    }

    if (!indexQuery.exec())
    {
        errorMessage = "无法查询行索引信息: " + indexQuery.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    while (indexQuery.next())
    {
        int rowIdx = indexQuery.value(0).toInt();
        qint64 offset = indexQuery.value(1).toLongLong();
        quint32 size = indexQuery.value(2).toUInt();
        rowOffsetMap[rowIdx] = qMakePair(offset, size);
    }

    metrics.dbQueryTime = stageTimer.elapsed();

    if (rowOffsetMap.isEmpty())
    {
        errorMessage = "未找到需要修改的行";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    qDebug() << funcName << " - 找到" << rowOffsetMap.size() << "行需要填充TimeSet";

    // 5. 打开二进制文件
    QFile binFile(binFilePath);
    if (!binFile.open(QIODevice::ReadWrite))
    {
        errorMessage = "无法打开二进制文件: " + binFile.errorString();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 6. 开始数据库事务
    if (!db.transaction())
    {
        errorMessage = "无法开启数据库事务: " + db.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        binFile.close();
        return false;
    }

    // 阶段1：批量读取和反序列化
    // 创建行数据映射
    QMap<int, Vector::RowData> rowDataMap;

    stageTimer.restart();
    int processedCount = 0;

    // 批量读取所有行数据
    for (auto it = rowOffsetMap.begin(); it != rowOffsetMap.end(); ++it)
    {
        int rowIndex = it.key();
        qint64 oldOffset = it.value().first;
        quint32 oldSize = it.value().second;

        // 从文件读取行数据
        operationTimer.restart();
        if (!binFile.seek(oldOffset))
        {
            qWarning() << funcName << "- 无法定位到行" << rowIndex << "的数据位置";
            continue;
        }

        QByteArray rowBytes = binFile.read(oldSize);
        metrics.readTime += operationTimer.elapsed();

        if (rowBytes.size() != oldSize)
        {
            qWarning() << funcName << "- 读取行数据失败，行:" << rowIndex
                       << "，读取了" << rowBytes.size() << "字节，应为" << oldSize << "字节";
            continue;
        }

        metrics.bytesProcessed += rowBytes.size();

        // 反序列化行数据
        Vector::RowData rowData;
        operationTimer.restart();
        if (!Persistence::BinaryFileHelper::deserializeRow(rowBytes, rowData))
        {
            qWarning() << funcName << "- 反序列化行数据失败，行:" << rowIndex;
            continue;
        }
        metrics.deserializeTime += operationTimer.elapsed();

        // 确保行数据足够长
        while (rowData.size() <= timesetColumnIndex)
        {
            rowData.append(QVariant());
        }

        // 更新TimeSet列的值
        QVariant timeSetValue(timeSetId);
        rowData[timesetColumnIndex] = timeSetValue;

        // 添加到映射中
        rowDataMap[rowIndex] = rowData;

        // 更新进度信息
        processedCount++;
        if (processedCount % 1000 == 0 || processedCount == rowOffsetMap.size())
        {
            int progress = (processedCount * 40) / rowOffsetMap.size(); // 读取阶段占40%
            emit progressUpdated(progress);
        }
    }

    qDebug() << funcName << " - [性能指标] 读取数据: " << metrics.readTime << "ms, 反序列化: " << metrics.deserializeTime << "ms";

    // 阶段2：批量追加阶段
    stageTimer.restart();

    // 定位到文件末尾准备追加
    qint64 appendOffset = binFile.size();
    binFile.seek(appendOffset);

    // 批量序列化和追加操作，并收集索引更新信息
    struct IndexUpdateInfo
    {
        qint64 newOffset;
        quint32 newSize;
    };
    QHash<int, IndexUpdateInfo> indexUpdates; // 行索引 -> 新索引信息
    indexUpdates.reserve(rowDataMap.size());  // 预分配容量

    int totalRows = rowDataMap.size();
    processedCount = 0;

    for (auto it = rowDataMap.begin(); it != rowDataMap.end(); ++it)
    {
        int rowIndex = it.key();
        const Vector::RowData &rowData = it.value();

        // 序列化行数据
        operationTimer.restart();
        QByteArray newRowBytes;
        if (!Persistence::BinaryFileHelper::serializeRowSimple(rowData, newRowBytes))
        {
            qWarning() << funcName << "- 序列化行数据失败，行:" << rowIndex;
            continue;
        }
        metrics.serializeTime += operationTimer.elapsed();

        // 追加到文件末尾
        qint64 currentOffset = binFile.pos();

        operationTimer.restart();
        qint64 bytesWritten = binFile.write(newRowBytes);
        metrics.writeTime += operationTimer.elapsed();

        if (bytesWritten != newRowBytes.size())
        {
            qWarning() << funcName << "- 写入行数据失败，行:" << rowIndex;
            continue;
        }

        // 记录新的偏移量和大小，稍后批量更新索引
        IndexUpdateInfo info;
        info.newOffset = currentOffset;
        info.newSize = newRowBytes.size();
        indexUpdates[rowIndex] = info;

        // 更新进度
        processedCount++;
        if (processedCount % 1000 == 0 || processedCount == totalRows)
        {
            int progress = 40 + (processedCount * 40) / totalRows; // 写入阶段占40%
            emit progressUpdated(progress);
        }
    }

    qDebug() << funcName << " - [性能指标] 序列化: " << metrics.serializeTime << "ms, 写入: " << metrics.writeTime << "ms";

    // 阶段3：批量索引更新
    stageTimer.restart();

    // 批量更新索引
    int batchSize = 1000;
    QStringList batchIds;
    int successCount = 0;
    int indexUpdateBatches = 0;
    int currentBatch = 0;

    for (auto it = indexUpdates.constBegin(); it != indexUpdates.constEnd(); ++it)
    {
        int rowIndex = it.key();
        const IndexUpdateInfo &info = it.value();

        // 更新索引信息
        QSqlQuery updateIndexQuery(db);
        updateIndexQuery.prepare(
            "UPDATE VectorTableRowIndex SET offset = ?, size = ? "
            "WHERE master_record_id = ? AND logical_row_order = ?");
        updateIndexQuery.addBindValue(info.newOffset);
        updateIndexQuery.addBindValue(info.newSize);
        updateIndexQuery.addBindValue(tableId);
        updateIndexQuery.addBindValue(rowIndex);

        if (!updateIndexQuery.exec())
        {
            qWarning() << funcName << "- 更新行索引失败，行:" << rowIndex
                       << "，错误:" << updateIndexQuery.lastError().text();
            continue;
        }

        successCount++;

        // 更新进度
        if (successCount % 1000 == 0 || successCount == indexUpdates.size())
        {
            int progress = 80 + (successCount * 20) / indexUpdates.size(); // 索引更新阶段占20%
            emit progressUpdated(progress);
        }
    }

    metrics.indexUpdateTime = stageTimer.elapsed();
    metrics.totalTime = timer.elapsed();

    // 提交事务
    if (successCount > 0)
    {
        if (!db.commit())
        {
            errorMessage = "无法提交数据库事务: " + db.lastError().text();
            qWarning() << funcName << " - " << errorMessage;
            db.rollback();
            binFile.close();
            return false;
        }
    }
    else
    {
        db.rollback();
        errorMessage = "未成功更新任何行";
        qWarning() << funcName << " - " << errorMessage;
        binFile.close();
        return false;
    }

    binFile.close();

    qDebug() << funcName << "- 批量填充TimeSet(批量追加模式)完成，成功更新" << successCount
             << "行，请求更新" << rowIndexes.size() << "行，总耗时:" << metrics.totalTime << "ms";
    qDebug() << funcName << "- 性能分析: 数据库查询=" << metrics.dbQueryTime
             << "ms, 读取=" << metrics.readTime
             << "ms, 反序列化=" << metrics.deserializeTime
             << "ms, 序列化=" << metrics.serializeTime
             << "ms, 写入=" << metrics.writeTime
             << "ms, 索引更新=" << metrics.indexUpdateTime
             << "ms, 总字节=" << metrics.bytesProcessed;

    // 完成进度
    emit progressUpdated(100);

    return successCount > 0;
}

bool RobustVectorDataHandler::batchReplaceTimeSet(int tableId, int fromTimeSetId, int toTimeSetId, const QList<int> &rowIndexes, QString &errorMessage)
{
    const QString funcName = "RobustVectorDataHandler::batchReplaceTimeSet";

    // 记录详细的性能数据
    struct PerformanceMetrics
    {
        qint64 dbQueryTime = 0;
        qint64 readTime = 0;
        qint64 deserializeTime = 0;
        qint64 serializeTime = 0;
        qint64 writeTime = 0;
        qint64 indexUpdateTime = 0;
        qint64 totalTime = 0;
        int rowCount = 0;
        int bytesProcessed = 0;
        int matchedRows = 0;
    };

    QElapsedTimer timer, stageTimer, operationTimer;
    timer.start(); // 开始总计时

    PerformanceMetrics metrics;
    metrics.rowCount = rowIndexes.size();

    qDebug() << funcName << " - 开始批量替换TimeSet（批量追加-批量索引更新模式）";
    qDebug() << funcName << " - 表ID:" << tableId << "，行数:" << rowIndexes.size()
             << "，从TimeSet ID:" << fromTimeSetId << " 到TimeSet ID:" << toTimeSetId;

    // 1. 获取表元数据 (只查询一次)
    QString binFileName;
    QList<Vector::ColumnInfo> columns;
    int schemaVersion = 0;
    int totalRowCount = 0;

    if (!loadVectorTableMeta(tableId, binFileName, columns, schemaVersion, totalRowCount))
    {
        errorMessage = "无法加载表元数据";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 2. 查找TimeSet列的索引
    int timesetColumnIndex = -1;
    for (int i = 0; i < columns.size(); ++i)
    {
        if (columns[i].type == Vector::ColumnDataType::TIMESET_ID)
        {
            timesetColumnIndex = i;
            break;
        }
    }

    if (timesetColumnIndex < 0)
    {
        errorMessage = "找不到TimeSet列";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 3. 获取二进制文件路径
    QString binFilePath = resolveBinaryFilePath(tableId, errorMessage);
    if (binFilePath.isEmpty())
    {
        qWarning() << funcName << " - 无法解析二进制文件路径";
        return false;
    }

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        errorMessage = "数据库未连接";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 4. 查询需要处理的行信息
    QMap<int, QPair<qint64, quint32>> rowOffsetMap; // 行索引 -> (偏移量, 大小)

    stageTimer.start();
    QSqlQuery indexQuery(db);

    // 如果指定了行范围，则只处理指定的行
    if (!rowIndexes.isEmpty())
    {
        // 构建IN子句
        QStringList rowIndicesStr;
        foreach (int row, rowIndexes)
        {
            rowIndicesStr.append(QString::number(row));
        }

        QString inClause = " AND logical_row_order IN (" + rowIndicesStr.join(',') + ")";

        indexQuery.prepare(
            "SELECT logical_row_order, offset, size FROM VectorTableRowIndex "
            "WHERE master_record_id = ? AND is_active = 1" +
            inClause);
        indexQuery.addBindValue(tableId);
    }
    else
    {
        // 处理所有行
        indexQuery.prepare(
            "SELECT logical_row_order, offset, size FROM VectorTableRowIndex "
            "WHERE master_record_id = ? AND is_active = 1");
        indexQuery.addBindValue(tableId);
    }

    if (!indexQuery.exec())
    {
        errorMessage = "无法查询行索引信息: " + indexQuery.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    while (indexQuery.next())
    {
        int rowIdx = indexQuery.value(0).toInt();
        qint64 offset = indexQuery.value(1).toLongLong();
        quint32 size = indexQuery.value(2).toUInt();
        rowOffsetMap[rowIdx] = qMakePair(offset, size);
    }

    metrics.dbQueryTime = stageTimer.elapsed();

    if (rowOffsetMap.isEmpty())
    {
        errorMessage = "未找到需要修改的行";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    qDebug() << funcName << " - 找到" << rowOffsetMap.size() << "行需要检查替换TimeSet";

    // 5. 打开二进制文件
    QFile binFile(binFilePath);
    if (!binFile.open(QIODevice::ReadWrite))
    {
        errorMessage = "无法打开二进制文件: " + binFile.errorString();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 6. 开始数据库事务
    if (!db.transaction())
    {
        errorMessage = "无法开启数据库事务: " + db.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        binFile.close();
        return false;
    }

    // 阶段1：批量读取和筛选需要更改的行
    QMap<int, Vector::RowData> rowDataMap; // 存储需要更新的行数据
    QVariant fromTimeSetValue(fromTimeSetId);
    QVariant toTimeSetValue(toTimeSetId);

    stageTimer.restart();
    int processedCount = 0;

    // 批量读取所有行数据并过滤出需要更新的
    for (auto it = rowOffsetMap.begin(); it != rowOffsetMap.end(); ++it)
    {
        int rowIndex = it.key();
        qint64 oldOffset = it.value().first;
        quint32 oldSize = it.value().second;

        // 从文件读取行数据
        operationTimer.restart();
        if (!binFile.seek(oldOffset))
        {
            qWarning() << funcName << "- 无法定位到行" << rowIndex << "的数据位置";
            continue;
        }

        QByteArray rowBytes = binFile.read(oldSize);
        metrics.readTime += operationTimer.elapsed();

        if (rowBytes.size() != oldSize)
        {
            qWarning() << funcName << "- 读取行数据失败，行:" << rowIndex
                       << "，读取了" << rowBytes.size() << "字节，应为" << oldSize << "字节";
            continue;
        }

        metrics.bytesProcessed += rowBytes.size();

        // 反序列化行数据
        Vector::RowData rowData;
        operationTimer.restart();
        if (!Persistence::BinaryFileHelper::deserializeRow(rowBytes, rowData))
        {
            qWarning() << funcName << "- 反序列化行数据失败，行:" << rowIndex;
            continue;
        }
        metrics.deserializeTime += operationTimer.elapsed();

        // 确保行数据足够长
        if (rowData.size() <= timesetColumnIndex)
        {
            continue; // 跳过没有TimeSet列的行
        }

        // 检查是否匹配要替换的TimeSet值
        if (rowData[timesetColumnIndex] == fromTimeSetValue)
        {
            // 更新TimeSet列的值
            rowData[timesetColumnIndex] = toTimeSetValue;

            // 添加到需要更新的映射中
            rowDataMap[rowIndex] = rowData;
            metrics.matchedRows++;
        }

        // 更新进度信息
        processedCount++;
        if (processedCount % 1000 == 0 || processedCount == rowOffsetMap.size())
        {
            int progress = (processedCount * 40) / rowOffsetMap.size(); // 读取阶段占40%
            emit progressUpdated(progress);
        }
    }

    qDebug() << funcName << " - [性能指标] 读取数据: " << metrics.readTime << "ms, 反序列化: "
             << metrics.deserializeTime << "ms, 匹配行数: " << metrics.matchedRows;

    // 如果没有需要更新的行，提前结束
    if (rowDataMap.isEmpty())
    {
        db.rollback();
        errorMessage = "未找到匹配的行需要替换TimeSet";
        qWarning() << funcName << " - " << errorMessage;
        binFile.close();
        return false;
    }

    // 阶段2：批量追加阶段
    stageTimer.restart();

    // 定位到文件末尾准备追加
    qint64 appendOffset = binFile.size();
    binFile.seek(appendOffset);

    // 批量序列化和追加操作，并收集索引更新信息
    struct IndexUpdateInfo
    {
        qint64 newOffset;
        quint32 newSize;
    };
    QHash<int, IndexUpdateInfo> indexUpdates; // 行索引 -> 新索引信息
    indexUpdates.reserve(rowDataMap.size());  // 预分配容量

    int totalRows = rowDataMap.size();
    processedCount = 0;

    for (auto it = rowDataMap.begin(); it != rowDataMap.end(); ++it)
    {
        int rowIndex = it.key();
        const Vector::RowData &rowData = it.value();

        // 序列化行数据
        operationTimer.restart();
        QByteArray newRowBytes;
        if (!Persistence::BinaryFileHelper::serializeRowSimple(rowData, newRowBytes))
        {
            qWarning() << funcName << "- 序列化行数据失败，行:" << rowIndex;
            continue;
        }
        metrics.serializeTime += operationTimer.elapsed();

        // 追加到文件末尾
        qint64 currentOffset = binFile.pos();

        operationTimer.restart();
        qint64 bytesWritten = binFile.write(newRowBytes);
        metrics.writeTime += operationTimer.elapsed();

        if (bytesWritten != newRowBytes.size())
        {
            qWarning() << funcName << "- 写入行数据失败，行:" << rowIndex;
            continue;
        }

        // 记录新的偏移量和大小，稍后批量更新索引
        IndexUpdateInfo info;
        info.newOffset = currentOffset;
        info.newSize = newRowBytes.size();
        indexUpdates[rowIndex] = info;

        // 更新进度
        processedCount++;
        if (processedCount % 1000 == 0 || processedCount == totalRows)
        {
            int progress = 40 + (processedCount * 40) / totalRows; // 写入阶段占40%
            emit progressUpdated(progress);
        }
    }

    qDebug() << funcName << " - [性能指标] 序列化: " << metrics.serializeTime << "ms, 写入: " << metrics.writeTime << "ms";

    // 阶段3：批量索引更新
    stageTimer.restart();

    // 批量更新索引
    int successCount = 0;

    for (auto it = indexUpdates.constBegin(); it != indexUpdates.constEnd(); ++it)
    {
        int rowIndex = it.key();
        const IndexUpdateInfo &info = it.value();

        // 更新索引信息
        QSqlQuery updateIndexQuery(db);
        updateIndexQuery.prepare(
            "UPDATE VectorTableRowIndex SET offset = ?, size = ? "
            "WHERE master_record_id = ? AND logical_row_order = ?");
        updateIndexQuery.addBindValue(info.newOffset);
        updateIndexQuery.addBindValue(info.newSize);
        updateIndexQuery.addBindValue(tableId);
        updateIndexQuery.addBindValue(rowIndex);

        if (!updateIndexQuery.exec())
        {
            qWarning() << funcName << "- 更新行索引失败，行:" << rowIndex
                       << "，错误:" << updateIndexQuery.lastError().text();
            continue;
        }

        successCount++;

        // 更新进度
        if (successCount % 1000 == 0 || successCount == indexUpdates.size())
        {
            int progress = 80 + (successCount * 20) / indexUpdates.size(); // 索引更新阶段占20%
            emit progressUpdated(progress);
        }
    }

    metrics.indexUpdateTime = stageTimer.elapsed();
    metrics.totalTime = timer.elapsed();

    // 提交事务
    if (successCount > 0)
    {
        if (!db.commit())
        {
            errorMessage = "无法提交数据库事务: " + db.lastError().text();
            qWarning() << funcName << " - " << errorMessage;
            db.rollback();
            binFile.close();
            return false;
        }
    }
    else
    {
        db.rollback();
        errorMessage = "未成功更新任何行";
        qWarning() << funcName << " - " << errorMessage;
        binFile.close();
        return false;
    }

    binFile.close();

    qDebug() << funcName << "- 批量替换TimeSet(批量追加模式)完成，成功更新" << successCount
             << "行，匹配行数:" << metrics.matchedRows << "，总耗时:" << metrics.totalTime << "ms";
    qDebug() << funcName << "- 性能分析: 数据库查询=" << metrics.dbQueryTime
             << "ms, 读取=" << metrics.readTime
             << "ms, 反序列化=" << metrics.deserializeTime
             << "ms, 序列化=" << metrics.serializeTime
             << "ms, 写入=" << metrics.writeTime
             << "ms, 索引更新=" << metrics.indexUpdateTime
             << "ms, 总字节=" << metrics.bytesProcessed;

    // 完成进度
    emit progressUpdated(100);

    return successCount > 0;
}

bool RobustVectorDataHandler::batchUpdateVectorColumnOptimized(
    int tableId, int columnIndex, const QMap<int, QVariant> &rowValueMap, QString &errorMessage)
{
    const QString funcName = "RobustVectorDataHandler::batchUpdateVectorColumnOptimized";

    // 记录详细的性能数据
    struct PerformanceMetrics
    {
        qint64 dbQueryTime = 0;
        qint64 readTime = 0;
        qint64 deserializeTime = 0;
        qint64 serializeTime = 0;
        qint64 writeTime = 0;
        qint64 indexUpdateTime = 0;
        qint64 totalTime = 0;
        int rowCount = 0;
        int bytesProcessed = 0;
    };

    QElapsedTimer timer;
    timer.start(); // 开始计时

    PerformanceMetrics metrics;
    metrics.rowCount = rowValueMap.size();

    qDebug() << funcName << " - 开始批量更新向量列数据（批量追加-批量索引更新模式）";
    qDebug() << funcName << " - 表ID:" << tableId << "，列索引:" << columnIndex << "，行数:" << rowValueMap.size();

    // 1. 获取表元数据 (只查询一次)
    QString binFileName;
    QList<Vector::ColumnInfo> columns;
    int schemaVersion = 0;
    int totalRowCount = 0;

    if (!loadVectorTableMeta(tableId, binFileName, columns, schemaVersion, totalRowCount))
    {
        errorMessage = "无法加载表元数据";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 2. 检查列索引是否有效
    if (columnIndex < 0 || columnIndex >= columns.size())
    {
        errorMessage = QString("无效的列索引: %1").arg(columnIndex);
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 3. 获取二进制文件路径
    QString binFilePath = resolveBinaryFilePath(tableId, errorMessage);
    if (binFilePath.isEmpty())
    {
        qWarning() << funcName << " - 无法解析二进制文件路径";
        return false;
    }

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        errorMessage = "数据库未连接";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 数据库查询时间记录
    QElapsedTimer queryTimer;
    queryTimer.start();

    // 4. 一次性查询所有需要修改的行的索引信息
    QMap<int, QPair<qint64, quint32>> rowOffsetMap; // 行索引 -> (偏移量, 大小)

    // 构建IN条件的行号列表
    QStringList logicalRowsList;
    for (auto it = rowValueMap.constBegin(); it != rowValueMap.constEnd(); ++it)
    {
        logicalRowsList << QString::number(it.key());
    }

    QString inClause = logicalRowsList.join(',');
    QSqlQuery indexQuery(db);
    indexQuery.prepare(
        "SELECT logical_row_order, offset, size FROM VectorTableRowIndex "
        "WHERE master_record_id = ? AND logical_row_order IN (" +
        inClause + ") AND is_active = 1");
    indexQuery.addBindValue(tableId);

    if (!indexQuery.exec())
    {
        errorMessage = "无法查询行索引信息: " + indexQuery.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    while (indexQuery.next())
    {
        int rowIndex = indexQuery.value(0).toInt();
        qint64 offset = indexQuery.value(1).toLongLong();
        quint32 size = indexQuery.value(2).toUInt();
        rowOffsetMap[rowIndex] = qMakePair(offset, size);
    }

    metrics.dbQueryTime = queryTimer.elapsed();

    // 如果某些行不存在，记录警告但继续处理其他行
    if (rowOffsetMap.size() != rowValueMap.size())
    {
        qWarning() << funcName << "- 部分行未找到索引信息，请求行数:"
                   << rowValueMap.size() << "，找到行数:" << rowOffsetMap.size();
    }

    if (rowOffsetMap.isEmpty())
    {
        errorMessage = "未找到任何需要更新的行";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    qDebug() << funcName << " - 查询索引信息完成，耗时: " << metrics.dbQueryTime << "ms";

    // 开启事务处理
    if (!db.transaction())
    {
        errorMessage = "无法开启数据库事务: " + db.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 5. 打开二进制文件进行读写
    QFile binFile(binFilePath);
    if (!binFile.open(QIODevice::ReadWrite))
    {
        errorMessage = "无法打开二进制文件进行读写: " + binFile.errorString();
        qWarning() << funcName << " - " << errorMessage;
        db.rollback();
        return false;
    }

    // 优化核心：批量追加-批量索引更新

    // 阶段1：准备阶段
    QElapsedTimer stageTimer;
    stageTimer.start();
    QElapsedTimer operationTimer;

    // 在内存中预读所有需要的行数据
    QHash<int, Vector::RowData> rowDataMap; // 行索引 -> 行数据
    // 移除不必要的中间存储，减少内存使用
    // QHash<int, QByteArray> oldRowBytesMap;  // 行索引 -> 原始二进制数据

    // 预分配容量，减少重新分配次数
    rowDataMap.reserve(rowOffsetMap.size());
    // oldRowBytesMap.reserve(rowOffsetMap.size());

    // 一次性读取所有行数据
    for (auto it = rowOffsetMap.constBegin(); it != rowOffsetMap.constEnd(); ++it)
    {
        int rowIndex = it.key();
        qint64 oldOffset = it.value().first;
        quint32 oldSize = it.value().second;

        // 定位到行位置并读取数据
        operationTimer.restart();
        if (!binFile.seek(oldOffset))
        {
            qWarning() << funcName << "- 无法定位到行" << rowIndex << "的数据位置";
            continue;
        }

        QByteArray rowBytes = binFile.read(oldSize);
        metrics.readTime += operationTimer.elapsed();
        metrics.bytesProcessed += rowBytes.size();

        if (rowBytes.size() != oldSize)
        {
            qWarning() << funcName << "- 读取行数据失败，行:" << rowIndex
                       << "，读取了" << rowBytes.size() << "字节，应为" << oldSize << "字节";
            continue;
        }

        // 移除不必要的存储
        // oldRowBytesMap[rowIndex] = rowBytes;

        // 反序列化行数据
        operationTimer.restart();
        Vector::RowData rowData;
        if (!Persistence::BinaryFileHelper::deserializeRow(rowBytes, rowData))
        {
            qWarning() << funcName << "- 反序列化行数据失败，行:" << rowIndex;
            continue;
        }
        metrics.deserializeTime += operationTimer.elapsed();

        // 确保行数据足够长
        while (rowData.size() <= columnIndex)
        {
            rowData.append(QVariant());
        }

        // 更新列值
        rowData[columnIndex] = rowValueMap[rowIndex];

        // 保存修改后的行数据到映射中，使用移动语义避免复制
        rowDataMap[rowIndex] = std::move(rowData);
    }

    qint64 stageTime = stageTimer.elapsed();
    qDebug() << funcName << " - 准备阶段完成，耗时: " << stageTime << "ms";
    qDebug() << funcName << " - [性能指标] 读取数据: " << metrics.readTime << "ms, 反序列化: " << metrics.deserializeTime << "ms";

    // 阶段2：批量追加阶段
    stageTimer.restart();

    // 定位到文件末尾准备追加
    qint64 appendOffset = binFile.size();
    binFile.seek(appendOffset);

    // 批量序列化和追加操作，并收集索引更新信息
    struct IndexUpdateInfo
    {
        qint64 newOffset;
        quint32 newSize;
    };
    QHash<int, IndexUpdateInfo> indexUpdates; // 行索引 -> 新索引信息
    indexUpdates.reserve(rowDataMap.size());  // 预分配容量

    int totalRows = rowDataMap.size();
    processedCount = 0;

    for (auto it = rowDataMap.begin(); it != rowDataMap.end(); ++it)
    {
        int rowIndex = it.key();
        const Vector::RowData &rowData = it.value();

        // 序列化行数据
        operationTimer.restart();
        QByteArray newRowBytes;
        if (!Persistence::BinaryFileHelper::serializeRowSimple(rowData, newRowBytes))
        {
            qWarning() << funcName << "- 序列化行数据失败，行:" << rowIndex;
            continue;
        }
        metrics.serializeTime += operationTimer.elapsed();

        // 追加到文件末尾
        qint64 currentOffset = binFile.pos();

        operationTimer.restart();
        qint64 bytesWritten = binFile.write(newRowBytes);
        metrics.writeTime += operationTimer.elapsed();

        if (bytesWritten != newRowBytes.size())
        {
            qWarning() << funcName << "- 写入行数据失败，行:" << rowIndex;
            continue;
        }

        // 记录新的偏移量和大小，稍后批量更新索引
        IndexUpdateInfo info;
        info.newOffset = currentOffset;
        info.newSize = newRowBytes.size();
        indexUpdates[rowIndex] = info;

        // 更新进度
        processedCount++;
        if (processedCount % 1000 == 0 || processedCount == totalRows)
        {
            int percentage = (processedCount * 100) / totalRows;
            qDebug() << funcName << "- 批量追加阶段: 已处理" << processedCount << "行，共" << totalRows
                     << "行 (" << percentage << "%)";
            emit progressUpdated(percentage / 2); // 前50%进度
        }
    }

    stageTime = stageTimer.elapsed();
    qDebug() << funcName << " - 批量追加阶段完成，耗时: " << stageTime << "ms";
    qDebug() << funcName << " - [性能指标] 序列化: " << metrics.serializeTime << "ms, 写入数据: " << metrics.writeTime << "ms";

    // 阶段3：批量索引更新阶段
    stageTimer.restart();

    // 使用批量绑定方式进行索引更新
    const int BATCH_SIZE = 1000; // 更大的批次大小
    QSqlQuery updateBatchQuery(db);
    updateBatchQuery.prepare(
        "UPDATE VectorTableRowIndex SET offset = ?, size = ? "
        "WHERE master_record_id = ? AND logical_row_order = ?");

    QVariantList offsetParams;
    QVariantList sizeParams;
    QVariantList tableIdParams;
    QVariantList rowIndexParams;

    processedCount = 0;
    int batchCount = 0;

    for (auto it = indexUpdates.begin(); it != indexUpdates.end(); ++it)
    {
        int rowIndex = it.key();
        const IndexUpdateInfo &info = it.value();

        // 添加参数到批处理列表
        offsetParams.append(info.newOffset);
        sizeParams.append(info.newSize);
        tableIdParams.append(tableId);
        rowIndexParams.append(rowIndex);

        processedCount++;
        batchCount++;

        // 当达到批量大小或处理完所有行时执行批量更新
        if (batchCount >= BATCH_SIZE || processedCount == totalRows)
        {
            // 绑定参数
            updateBatchQuery.addBindValue(offsetParams);
            updateBatchQuery.addBindValue(sizeParams);
            updateBatchQuery.addBindValue(tableIdParams);
            updateBatchQuery.addBindValue(rowIndexParams);

            // 执行批量更新
            operationTimer.restart();
            if (!updateBatchQuery.execBatch())
            {
                qWarning() << funcName << "- 批量更新索引失败: " << updateBatchQuery.lastError().text();
                errorMessage = "批量更新索引失败: " + updateBatchQuery.lastError().text();
                db.rollback();
                binFile.close();
                return false;
            }
            metrics.indexUpdateTime += operationTimer.elapsed();

            // 清空参数列表，准备下一批
            offsetParams.clear();
            sizeParams.clear();
            tableIdParams.clear();
            rowIndexParams.clear();
            batchCount = 0;

            // 更新进度
            int percentage = 50 + (processedCount * 50) / totalRows; // 后50%进度
            qDebug() << funcName << "- 批量索引更新阶段: 已处理" << processedCount << "行，共" << totalRows
                     << "行 (" << percentage << "%)";
            emit progressUpdated(percentage);
        }
    }

    stageTime = stageTimer.elapsed();
    qDebug() << funcName << " - 批量索引更新阶段完成，耗时: " << stageTime << "ms";
    qDebug() << funcName << " - [性能指标] 索引更新: " << metrics.indexUpdateTime << "ms";

    // 提交事务
    if (!db.commit())
    {
        errorMessage = "无法提交数据库事务: " + db.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        db.rollback();
        binFile.close();
        return false;
    }

    // 关闭文件
    binFile.close();

    // 计算总耗时和详细性能分析
    metrics.totalTime = timer.elapsed();

    // 输出总耗时和详细的性能报告
    qDebug() << funcName << " - 批量更新完成，总耗时: " << metrics.totalTime << "ms";
    qDebug() << funcName << " - 成功更新 " << indexUpdates.size() << " 行数据";
    qDebug() << funcName << " - 处理字节总量: " << metrics.bytesProcessed << " bytes";

    // 各阶段详细性能分析
    qDebug() << funcName << " - [性能分析]";
    qDebug() << funcName << " - 数据库查询: " << metrics.dbQueryTime << "ms (" << (metrics.dbQueryTime * 100.0 / metrics.totalTime) << "%)";
    qDebug() << funcName << " - 数据读取: " << metrics.readTime << "ms (" << (metrics.readTime * 100.0 / metrics.totalTime) << "%)";
    qDebug() << funcName << " - 反序列化: " << metrics.deserializeTime << "ms (" << (metrics.deserializeTime * 100.0 / metrics.totalTime) << "%)";
    qDebug() << funcName << " - 序列化: " << metrics.serializeTime << "ms (" << (metrics.serializeTime * 100.0 / metrics.totalTime) << "%)";
    qDebug() << funcName << " - 数据写入: " << metrics.writeTime << "ms (" << (metrics.writeTime * 100.0 / metrics.totalTime) << "%)";
    qDebug() << funcName << " - 索引更新: " << metrics.indexUpdateTime << "ms (" << (metrics.indexUpdateTime * 100.0 / metrics.totalTime) << "%)";
    qDebug() << funcName << " - 其他操作: " << (metrics.totalTime - metrics.dbQueryTime - metrics.readTime - metrics.deserializeTime - metrics.serializeTime - metrics.writeTime - metrics.indexUpdateTime) << "ms";
    qDebug() << funcName << " - 平均每行耗时: " << (metrics.totalTime / (double)indexUpdates.size()) << "ms";

    return !indexUpdates.isEmpty();
}
