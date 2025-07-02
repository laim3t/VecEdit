

// Add this new function implementation
QList<Vector::ColumnInfo> MainWindow::getCurrentColumnConfiguration(int tableId)
{
    const QString funcName = "MainWindow::getCurrentColumnConfiguration";
    QList<Vector::ColumnInfo> columns;

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << funcName << " - 数据库未打开";
        return columns;
    }

    QSqlQuery colQuery(db);
    // 在准备新查询前先调用clear()，确保查询对象状态干净，这是治本之策
    colQuery.clear();
    // 恢复使用安全的参数绑定
    colQuery.prepare("SELECT * FROM VectorTableColumnConfiguration WHERE master_record_id = ? ORDER BY column_order");
    colQuery.addBindValue(tableId);

    if (!colQuery.exec())
    {
        qWarning() << funcName << " - 查询列结构失败, 表ID:" << tableId << ", 错误:" << colQuery.lastError().text();
        return columns;
    }

    qDebug() << funcName << " - 为表ID:" << tableId << "获取了" << colQuery.size() << "列配置。";

    while (colQuery.next())
    {
        Vector::ColumnInfo col;
        // 按索引取值以匹配 SELECT *
        col.id = colQuery.value(0).toInt();
        col.vector_table_id = tableId;
        col.name = colQuery.value(2).toString();
        col.order = colQuery.value(3).toInt();
        col.original_type_str = colQuery.value(4).toString();
        col.type = Vector::columnDataTypeFromString(col.original_type_str, col.name);
        col.is_visible = colQuery.value(6).toBool();

        QString propStr = colQuery.value(7).toString();
        if (!propStr.isEmpty())
        {
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(propStr.toUtf8(), &err);
            if (err.error == QJsonParseError::NoError && doc.isObject())
                col.data_properties = doc.object();
        }

        columns.append(col);
    }
    return columns;
}

// 辅助函数：比较两个列配置列表，判断是否发生了可能影响二进制布局的实质性变化
// 只比较列的数量、名称和顺序。
static bool areColumnConfigurationsDifferentSimplified(const QList<Vector::ColumnInfo> &oldCols, const QList<Vector::ColumnInfo> &newCols)
{
    if (oldCols.size() != newCols.size())
    {
        return true; // 列数量不同，需要迁移
    }

    // 假设列是按 column_order 排序的
    for (int i = 0; i < oldCols.size(); ++i)
    {
        if (oldCols[i].name != newCols[i].name || oldCols[i].order != newCols[i].order)
        {
            return true; // 列名或顺序不同，需要迁移
        }
    }

    return false;
}

// 辅助函数：将旧的行数据适配到新的列结构
static QList<Vector::RowData> adaptRowDataToNewColumns(const QList<Vector::RowData> &oldData, const QList<Vector::ColumnInfo> &oldCols, const QList<Vector::ColumnInfo> &newCols)
{
    if (oldData.isEmpty())
    {
        return {}; // 如果没有旧数据，无需迁移
    }

    QList<Vector::RowData> newDataList;
    newDataList.reserve(oldData.size());

    for (const auto &oldRow : oldData)
    {
        // 为旧行数据创建一个 名称 -> 数据值 的映射，方便查找
        QMap<QString, QVariant> oldRowMap;
        for (int i = 0; i < oldCols.size(); ++i)
        {
            if (i < oldRow.size())
            {
                oldRowMap[oldCols[i].name] = oldRow[i];
            }
        }

        Vector::RowData newRow;
        for (int i = 0; i < newCols.size(); ++i)
        {
            newRow.append(QVariant());
        }

        for (int i = 0; i < newCols.size(); ++i)
        {
            const auto &newColInfo = newCols[i];

            if (oldRowMap.contains(newColInfo.name))
            {
                // 如果新列在旧数据中存在，则直接拷贝
                newRow[i] = oldRowMap[newColInfo.name];
            }
            else
            {
                // 如果是新增的列，则赋予默认值
                if (newColInfo.type == Vector::ColumnDataType::PIN_STATE_ID)
                {
                    newRow[i] = "X"; // 新增管脚列的默认值为 "X"
                }
                else
                {
                    // For other types, the default QVariant is already what we want
                    // so we don't need to do anything here.
                    // The list was already filled with default QVariants.
                    // newRow[i] = QVariant();
                }
            }
        }
        newDataList.append(newRow);
    }

    return newDataList;
}

/**
 * @brief 检查Label值是否在整个表中重复
 *
 * @param tableId 向量表ID
 * @param labelValue 要检查的Label值
 * @param currentRow 当前行索引（在整个表中的实际索引，不是页内索引）
 * @param duplicateRow [输出] 如果找到重复值，设置为重复行的索引
 * @return bool 如果Label重复返回true，否则返回false
 */
bool MainWindow::isLabelDuplicate(int tableId, const QString &labelValue, int currentRow, int &duplicateRow)
{
    const QString funcName = "MainWindow::isLabelDuplicate";
    qDebug() << funcName << " - 检查Label值:" << labelValue << "在表ID:" << tableId << "中是否重复";

    if (labelValue.isEmpty())
    {
        qDebug() << funcName << " - Label值为空，不检查重复";
        return false;
    }

    // 获取表的所有行数据
    bool ok = false;
    QList<Vector::RowData> allRows;
    if (m_useNewDataHandler)
    {
        allRows = m_robustDataHandler->getAllVectorRows(tableId, ok);
    }
    else
    {
        allRows = VectorDataHandler::instance().getAllVectorRows(tableId, ok);
    }

    if (!ok)
    {
        qWarning() << funcName << " - 无法获取表格所有行数据";
        return false;
    }

    // 获取表的列配置
    QList<Vector::ColumnInfo> columns;
    if (m_useNewDataHandler)
    {
        columns = m_robustDataHandler->getAllColumnInfo(tableId);
    }
    else
    {
        columns = VectorDataHandler::instance().getAllColumnInfo(tableId);
    }

    // 查找Label列的索引
    int labelColumnIndex = -1;
    for (int i = 0; i < columns.size(); ++i)
    {
        if (columns[i].name.toLower() == "label")
        {
            labelColumnIndex = i;
            break;
        }
    }

    if (labelColumnIndex == -1)
    {
        qWarning() << funcName << " - 找不到Label列";
        return false;
    }

    // 遍历所有行检查是否有重复的Label值
    for (int i = 0; i < allRows.size(); ++i)
    {
        if (i != currentRow) // 跳过当前行
        {
            if (labelColumnIndex < allRows[i].size())
            {
                QString rowLabelValue = allRows[i][labelColumnIndex].toString().trimmed();
                if (rowLabelValue == labelValue)
                {
                    duplicateRow = i;
                    qDebug() << funcName << " - 发现重复的Label值:" << labelValue << "在行:" << i;
                    return true;
                }
            }
        }
    }

    qDebug() << funcName << " - 未发现重复的Label值";
    return false;
}

bool MainWindow::updateSelectedPinsAsColumns(int tableId)
{
    const QString funcName = "MainWindow::updateSelectedPinsAsColumns";
    qDebug() << funcName << " - Starting to update pin columns for tableId:" << tableId;

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << funcName << " - Database is not open.";
        return false;
    }

    // 1. 获取该表已选择的管脚
    QSqlQuery pinQuery(db);
    pinQuery.prepare(
        "SELECT p.pin_name "
        "FROM vector_table_pins vtp "
        "JOIN pin_list p ON vtp.pin_id = p.id "
        "WHERE vtp.table_id = ?");
    pinQuery.addBindValue(tableId);

    if (!pinQuery.exec())
    {
        qWarning() << funcName << " - Failed to query selected pins:" << pinQuery.lastError().text();
        return false;
    }

    QStringList pinNames;
    while (pinQuery.next())
    {
        pinNames << pinQuery.value(0).toString();
    }

    if (pinNames.isEmpty())
    {
        qInfo() << funcName << " - No pins selected for tableId:" << tableId << ". Nothing to do.";
        return true; // 没有选择管脚不是一个错误
    }
    qDebug() << funcName << " - Found selected pins:" << pinNames;

    // 2. 获取当前最大的 column_order
    QSqlQuery orderQuery(db);
    orderQuery.prepare("SELECT MAX(column_order) FROM VectorTableColumnConfiguration WHERE master_record_id = ?");
    orderQuery.addBindValue(tableId);
    int maxOrder = 0;
    if (orderQuery.exec() && orderQuery.next())
    {
        maxOrder = orderQuery.value(0).toInt();
    }
    qDebug() << funcName << " - Current max column order is:" << maxOrder;

    // 3. 准备插入新列
    db.transaction(); // 开始事务
    QSqlQuery insertQuery(db);
    insertQuery.prepare(
        "INSERT INTO VectorTableColumnConfiguration (master_record_id, column_name, column_type, column_order, default_value, is_visible) "
        "VALUES (?, ?, ?, ?, ?, 1)");

    for (const QString &pinName : pinNames)
    {
        maxOrder++;
        insertQuery.bindValue(0, tableId);
        insertQuery.bindValue(1, pinName);
        insertQuery.bindValue(2, "Pin"); // 管脚列的类型固定为 'Pin'
        insertQuery.bindValue(3, maxOrder);
        insertQuery.bindValue(4, "0"); // 默认值设为 '0'

        if (!insertQuery.exec())
        {
            qWarning() << funcName << " - Failed to insert pin column" << pinName << ":" << insertQuery.lastError().text();
            db.rollback(); // 插入失败，回滚事务
            return false;
        }
    }

    if (!db.commit())
    {
        qWarning() << funcName << " - Transaction commit failed:" << db.lastError().text();
        db.rollback();
        return false;
    }

    qInfo() << funcName << " - Successfully added" << pinNames.count() << "pin columns to configuration for tableId:" << tableId;
    return true;
}

/**
 * @brief 修复数据库中列类型的存储格式，将数字转换为对应的文本描述
 *
 * 这个函数会将 VectorTableColumnConfiguration 表中所有的 column_type 字段
 * 从数字形式 (如 "0", "1", "2") 转换为对应的文本描述 (如 "TEXT", "INTEGER", "BOOLEAN")
 *
 * @return bool 成功返回true，失败返回false
 */
bool MainWindow::fixColumnTypeStorageFormat()
{
    const QString funcName = "MainWindow::fixColumnTypeStorageFormat";
    qDebug() << funcName << " - 开始修复数据库中的列类型存储格式";

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qCritical() << funcName << " - 数据库未打开";
        return false;
    }

    // 创建数字到文本类型的映射
    QMap<int, QString> typeMap;
    typeMap[0] = "TEXT";
    typeMap[1] = "INTEGER";
    typeMap[2] = "REAL";
    typeMap[3] = "INSTRUCTION_ID";
    typeMap[4] = "TIMESET_ID";
    typeMap[5] = "PIN_STATE_ID";
    typeMap[6] = "BOOLEAN";
    typeMap[7] = "JSON_PROPERTIES";

    // 对已知的标准列创建列名到类型的映射
    QMap<QString, QString> columnNameTypeMap;
    columnNameTypeMap["Label"] = "TEXT";
    columnNameTypeMap["Instruction"] = "INSTRUCTION_ID";
    columnNameTypeMap["TimeSet"] = "TIMESET_ID";
    columnNameTypeMap["Capture"] = "BOOLEAN";
    columnNameTypeMap["EXT"] = "TEXT";
    columnNameTypeMap["Comment"] = "TEXT";

    // 从pin_list表获取所有管脚名称，用于准确识别管脚列
    QSet<QString> pinNames;
    QSqlQuery pinQuery(db);
    pinQuery.prepare("SELECT pin_name FROM pin_list");
    if (pinQuery.exec())
    {
        while (pinQuery.next())
        {
            pinNames.insert(pinQuery.value(0).toString());
        }
        qDebug() << funcName << " - 从pin_list表获取了" << pinNames.size() << "个管脚名称";
    }
    else
    {
        qWarning() << funcName << " - 无法查询pin_list表:" << pinQuery.lastError().text();
    }

    // 开始事务
    if (!db.transaction())
    {
        qCritical() << funcName << " - 无法开始事务:" << db.lastError().text();
        return false;
    }

    try
    {
        // 查询所有列配置
        QSqlQuery query(db);
        query.prepare("SELECT id, master_record_id, column_name, column_type FROM VectorTableColumnConfiguration");

        if (!query.exec())
        {
            throw QString("查询列配置失败: " + query.lastError().text());
        }

        int updatedCount = 0;
        QSqlQuery updateQuery(db);
        updateQuery.prepare("UPDATE VectorTableColumnConfiguration SET column_type = ? WHERE id = ?");

        while (query.next())
        {
            int id = query.value(0).toInt();
            QString columnName = query.value(2).toString();
            QString currentTypeStr = query.value(3).toString();

            QString newTypeStr;

            // 首先检查是否是标准列名
            if (columnNameTypeMap.contains(columnName))
            {
                newTypeStr = columnNameTypeMap[columnName];
            }
            // 然后检查是否是已知的管脚名称
            else if (pinNames.contains(columnName))
            {
                newTypeStr = "PIN_STATE_ID";
                qDebug() << funcName << " - 识别到管脚列:" << columnName << "，设置类型为PIN_STATE_ID";
            }
            else
            {
                // 如果不是标准列名或已知管脚，检查当前类型是否为数字
                bool isInt;
                int typeInt = currentTypeStr.toInt(&isInt);

                if (isInt && typeMap.contains(typeInt))
                {
                    newTypeStr = typeMap[typeInt];
                }
                else if (currentTypeStr.startsWith("PIN") || columnName.startsWith("Pin"))
                {
                    // 管脚列特殊处理（备用规则，以防pin_list表不完整）
                    newTypeStr = "PIN_STATE_ID";
                    qDebug() << funcName << " - 通过命名规则识别到管脚列:" << columnName << "，设置类型为PIN_STATE_ID";
                }
                else
                {
                    // 如果无法确定类型，保持不变
                    newTypeStr = currentTypeStr;
                }
            }

            // 仅当类型发生变化时才更新
            if (newTypeStr != currentTypeStr)
            {
                updateQuery.bindValue(0, newTypeStr);
                updateQuery.bindValue(1, id);

                if (!updateQuery.exec())
                {
                    throw QString("更新列类型失败: " + updateQuery.lastError().text());
                }

                updatedCount++;
            }
        }

        if (!db.commit())
        {
            throw QString("提交事务失败: " + db.lastError().text());
        }

        qDebug() << funcName << " - 成功修复了" << updatedCount << "条列类型记录";
        return true;
    }
    catch (const QString &error)
    {
        qCritical() << funcName << " - 错误: " << error;
        db.rollback();
        return false;
    }
}
