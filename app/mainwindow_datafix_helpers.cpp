// ==========================================================
//  Headers for: mainwindow_datafix_helpers.cpp
// ==========================================================
#include "app/mainwindow.h"

// Qt Widgets & Core
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

// Project-specific headers
#include "database/databasemanager.h"
#include "vector/vectordatahandler.h"

// Add this new function implementation
bool MainWindow::areColumnConfigurationsDifferentSimplified(const QList<Vector::ColumnInfo> &config1, const QList<Vector::ColumnInfo> &config2)
{
    if (config1.size() != config2.size())
    {
        qDebug() << "areColumnConfigurationsDifferentSimplified: Sizes differ - config1:" << config1.size() << "config2:" << config2.size();
        return true;
    }

    QSet<QString> names1, names2;
    for (const auto &col : config1)
    {
        names1.insert(col.name);
    }
    for (const auto &col : config2)
    {
        names2.insert(col.name);
    }

    if (names1 != names2)
    {
        qDebug() << "areColumnConfigurationsDifferentSimplified: Names differ.";
        qDebug() << "Config1 names:" << names1;
        qDebug() << "Config2 names:" << names2;
        return true;
    }

    // Optional: Check for type changes for same-named columns if necessary for your definition of "different"
    // For now, just size and name set equality is enough for "simplified"

    return false; // Configurations are considered the same by this simplified check
}

// Add this new function implementation
QList<Vector::RowData> MainWindow::adaptRowDataToNewColumns(const QList<Vector::RowData> &oldRowDataList,
                                                            const QList<Vector::ColumnInfo> &oldColumns,
                                                            const QList<Vector::ColumnInfo> &newColumns)
{
    const QString funcName = "MainWindow::adaptRowDataToNewColumns";
    QList<Vector::RowData> newRowDataList;
    newRowDataList.reserve(oldRowDataList.size());

    // Create a map of old column names to their index for efficient lookup
    QMap<QString, int> oldColumnNameToIndex;
    for (int i = 0; i < oldColumns.size(); ++i)
    {
        oldColumnNameToIndex[oldColumns[i].name] = i;
    }

    // Create a map of old column names to their type for type-aware default values (optional enhancement)
    QMap<QString, Vector::ColumnDataType> oldColumnNameToType;
    for (const auto &col : oldColumns)
    {
        oldColumnNameToType[col.name] = col.type;
    }

    for (const Vector::RowData &oldRow : oldRowDataList)
    {
        // Vector::RowData newRow(newColumns.size()); // Initialize newRow with newColumns.size() default QVariants - Incorrect
        Vector::RowData newRow;
        newRow.reserve(newColumns.size()); // Good practice to reserve if size is known

        for (int colIdx = 0; colIdx < newColumns.size(); ++colIdx)
        { // Changed loop variable name for clarity
            const Vector::ColumnInfo &newCol = newColumns[colIdx];
            QVariant cellData; // Temporary variable to hold the data for the current cell

            if (oldColumnNameToIndex.contains(newCol.name))
            {
                // Column exists in old data, copy it
                int oldIndex = oldColumnNameToIndex[newCol.name];
                if (oldIndex >= 0 && oldIndex < oldRow.size())
                { // Bounds check
                    cellData = oldRow[oldIndex];
                }
                else
                {
                    qWarning() << funcName << "- Old column index out of bounds for column:" << newCol.name << "oldIndex:" << oldIndex << "oldRow.size:" << oldRow.size();
                    if (newCol.type == Vector::ColumnDataType::PIN_STATE_ID)
                    {
                        cellData = "X";
                    }
                    else
                    {
                        cellData = QVariant();
                    }
                }
            }
            else
            {
                // Column is new, fill with default value
                if (newCol.type == Vector::ColumnDataType::PIN_STATE_ID)
                {
                    cellData = "X";
                }
                else if (newCol.type == Vector::ColumnDataType::TEXT)
                {
                    cellData = QString();
                }
                else if (newCol.type == Vector::ColumnDataType::INTEGER ||
                         newCol.type == Vector::ColumnDataType::INSTRUCTION_ID ||
                         newCol.type == Vector::ColumnDataType::TIMESET_ID)
                {
                    cellData = 0;
                }
                else if (newCol.type == Vector::ColumnDataType::REAL)
                {
                    cellData = 0.0;
                }
                else if (newCol.type == Vector::ColumnDataType::BOOLEAN)
                {
                    cellData = false;
                }
                else
                {
                    qWarning() << funcName << "- Unhandled new column type for default value:" << newCol.name << "type:" << newCol.original_type_str;
                    cellData = QVariant();
                }
            }
            newRow.append(cellData); // Append the determined cell data
        }
        newRowDataList.append(newRow);
    }
    qDebug() << funcName << "- Data adaptation complete. Processed" << oldRowDataList.size() << "rows, produced" << newRowDataList.size() << "rows for new structure.";
    return newRowDataList;
}

/**
 * @brief 为新创建的向量表添加默认的列配置
 *
 * @param tableId 向量表ID
 * @return bool 成功返回true，失败返回false
 */
bool MainWindow::addDefaultColumnConfigurations(int tableId)
{
    const QString funcName = "MainWindow::addDefaultColumnConfigurations";
    qDebug() << funcName << " - 开始为表ID " << tableId << " 添加默认列配置";

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qCritical() << funcName << " - 数据库未打开";
        return false;
    }

    db.transaction();

    try
    {
        QSqlQuery query(db);

        // 添加标准列配置
        // 1. 添加Label列
        query.prepare("INSERT INTO VectorTableColumnConfiguration "
                      "(master_record_id, column_name, column_order, column_type, data_properties) "
                      "VALUES (?, ?, ?, ?, ?)");
        query.addBindValue(tableId);
        query.addBindValue("Label");
        query.addBindValue(0);
        query.addBindValue("TEXT");
        query.addBindValue("{}");

        if (!query.exec())
        {
            throw QString("无法添加Label列: " + query.lastError().text());
        }

        // 2. 添加Instruction列
        query.prepare("INSERT INTO VectorTableColumnConfiguration "
                      "(master_record_id, column_name, column_order, column_type, data_properties) "
                      "VALUES (?, ?, ?, ?, ?)");
        query.addBindValue(tableId);
        query.addBindValue("Instruction");
        query.addBindValue(1);
        query.addBindValue("INSTRUCTION_ID");
        query.addBindValue("{}");

        if (!query.exec())
        {
            throw QString("无法添加Instruction列: " + query.lastError().text());
        }

        // 3. 添加TimeSet列
        query.prepare("INSERT INTO VectorTableColumnConfiguration "
                      "(master_record_id, column_name, column_order, column_type, data_properties) "
                      "VALUES (?, ?, ?, ?, ?)");
        query.addBindValue(tableId);
        query.addBindValue("TimeSet");
        query.addBindValue(2);
        query.addBindValue("TIMESET_ID");
        query.addBindValue("{}");

        if (!query.exec())
        {
            throw QString("无法添加TimeSet列: " + query.lastError().text());
        }

        // 4. 添加Capture列
        query.prepare("INSERT INTO VectorTableColumnConfiguration "
                      "(master_record_id, column_name, column_order, column_type, data_properties) "
                      "VALUES (?, ?, ?, ?, ?)");
        query.addBindValue(tableId);
        query.addBindValue("Capture");
        query.addBindValue(3);
        query.addBindValue("BOOLEAN");
        query.addBindValue("{}");

        if (!query.exec())
        {
            throw QString("无法添加Capture列: " + query.lastError().text());
        }

        // 5. 添加EXT列
        query.prepare("INSERT INTO VectorTableColumnConfiguration "
                      "(master_record_id, column_name, column_order, column_type, data_properties) "
                      "VALUES (?, ?, ?, ?, ?)");
        query.addBindValue(tableId);
        query.addBindValue("EXT");
        query.addBindValue(4);
        query.addBindValue("TEXT");
        query.addBindValue("{}");

        if (!query.exec())
        {
            throw QString("无法添加EXT列: " + query.lastError().text());
        }

        // 6. 添加Comment列
        query.prepare("INSERT INTO VectorTableColumnConfiguration "
                      "(master_record_id, column_name, column_order, column_type, data_properties) "
                      "VALUES (?, ?, ?, ?, ?)");
        query.addBindValue(tableId);
        query.addBindValue("Comment");
        query.addBindValue(5);
        query.addBindValue("TEXT");
        query.addBindValue("{}");

        if (!query.exec())
        {
            throw QString("无法添加Comment列: " + query.lastError().text());
        }

        // 更新主记录的列数
        query.prepare("UPDATE VectorTableMasterRecord SET column_count = ? WHERE id = ?");
        query.addBindValue(6); // 六个标准列
        query.addBindValue(tableId);

        if (!query.exec())
        {
            throw QString("无法更新主记录的列数: " + query.lastError().text());
        }

        // 提交事务
        if (!db.commit())
        {
            throw QString("无法提交事务: " + db.lastError().text());
        }

        qDebug() << funcName << " - 成功添加默认列配置";
        return true;
    }
    catch (const QString &error)
    {
        qCritical() << funcName << " - 错误: " << error;
        db.rollback();
        return false;
    }
}

/**
 * @brief 修复没有列配置的现有向量表
 *
 * 这是一个一次性工具方法，用于修复数据库中已经存在但缺少列配置的向量表
 *
 * @param tableId 要修复的向量表ID
 * @return bool 成功返回true，失败返回false
 */
bool MainWindow::fixExistingTableWithoutColumns(int tableId)
{
    const QString funcName = "MainWindow::fixExistingTableWithoutColumns";
    qDebug() << funcName << " - 开始修复表ID " << tableId << " 的列配置";

    // 首先检查这个表是否已经有列配置
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qCritical() << funcName << " - 数据库未打开";
        return false;
    }

    // 检查是否有列配置
    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT COUNT(*) FROM VectorTableColumnConfiguration WHERE master_record_id = ?");
    checkQuery.addBindValue(tableId);

    if (!checkQuery.exec() || !checkQuery.next())
    {
        qCritical() << funcName << " - 无法检查表ID " << tableId << " 的列配置: " << checkQuery.lastError().text();
        return false;
    }

    int columnCount = checkQuery.value(0).toInt();
    if (columnCount > 0)
    {
        qDebug() << funcName << " - 表ID " << tableId << " 已有 " << columnCount << " 个列配置，不需要修复";
        return true; // 已有列配置，不需要修复
    }

    // 添加默认列配置
    if (!addDefaultColumnConfigurations(tableId))
    {
        qCritical() << funcName << " - 无法为表ID " << tableId << " 添加默认列配置";
        return false;
    }

    // 查询与这个表关联的管脚
    QSqlQuery pinQuery(db);
    pinQuery.prepare("SELECT vtp.id, pl.pin_name, vtp.pin_id, vtp.pin_channel_count, vtp.pin_type "
                     "FROM vector_table_pins vtp "
                     "JOIN pin_list pl ON vtp.pin_id = pl.id "
                     "WHERE vtp.table_id = ?");
    pinQuery.addBindValue(tableId);

    if (!pinQuery.exec())
    {
        qCritical() << funcName << " - 无法查询表ID " << tableId << " 的管脚: " << pinQuery.lastError().text();
        return false;
    }

    // 设置事务
    db.transaction();

    try
    {
        // 从标准列开始，所以列序号从6开始（0-5已经由addDefaultColumnConfigurations添加）
        int columnOrder = 6;

        while (pinQuery.next())
        {
            int pinTableId = pinQuery.value(0).toInt();
            QString pinName = pinQuery.value(1).toString();
            int pinId = pinQuery.value(2).toInt();
            int channelCount = pinQuery.value(3).toInt();
            int pinType = pinQuery.value(4).toInt();

            // 构造JSON属性字符串
            QString jsonProps = QString("{\"pin_list_id\": %1, \"channel_count\": %2, \"type_id\": %3}")
                                    .arg(pinId)
                                    .arg(channelCount)
                                    .arg(pinType);

            // 添加管脚列配置
            QSqlQuery colInsertQuery(db);
            colInsertQuery.prepare("INSERT INTO VectorTableColumnConfiguration "
                                   "(master_record_id, column_name, column_order, column_type, data_properties) "
                                   "VALUES (?, ?, ?, ?, ?)");
            colInsertQuery.addBindValue(tableId);
            colInsertQuery.addBindValue(pinName);
            colInsertQuery.addBindValue(columnOrder++);
            colInsertQuery.addBindValue("PIN_STATE_ID");
            colInsertQuery.addBindValue(jsonProps);

            if (!colInsertQuery.exec())
            {
                throw QString("无法添加管脚列配置: " + colInsertQuery.lastError().text());
            }

            qDebug() << funcName << " - 已成功为表ID " << tableId << " 添加管脚列 " << pinName;
        }

        // 更新表的总列数
        QSqlQuery updateColumnCountQuery(db);
        updateColumnCountQuery.prepare("UPDATE VectorTableMasterRecord SET column_count = ? WHERE id = ?");
        updateColumnCountQuery.addBindValue(columnOrder); // 总列数
        updateColumnCountQuery.addBindValue(tableId);

        if (!updateColumnCountQuery.exec())
        {
            throw QString("无法更新列数: " + updateColumnCountQuery.lastError().text());
        }

        // 提交事务
        if (!db.commit())
        {
            throw QString("无法提交事务: " + db.lastError().text());
        }

        qDebug() << funcName << " - 成功修复表ID " << tableId << " 的列配置，共添加了 " << columnOrder << " 列";
        return true;
    }
    catch (const QString &error)
    {
        qCritical() << funcName << " - 错误: " << error;
        db.rollback();
        return false;
    }
}

void MainWindow::checkAndFixAllVectorTables()
{
    const QString funcName = "MainWindow::checkAndFixAllVectorTables";
    qDebug() << funcName << " - 开始检查和修复所有向量表的列配置";

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qCritical() << funcName << " - 数据库未打开";
        return;
    }

    // 获取所有向量表主记录
    QSqlQuery query(db);
    query.prepare("SELECT id FROM VectorTableMasterRecord");

    if (!query.exec())
    {
        qCritical() << funcName << " - 无法查询向量表主记录:" << query.lastError().text();
        return;
    }

    int fixedCount = 0;
    int totalCount = 0;

    while (query.next())
    {
        int tableId = query.value(0).toInt();
        totalCount++;

        // 检查是否有列配置
        QSqlQuery checkQuery(db);
        checkQuery.prepare("SELECT COUNT(*) FROM VectorTableColumnConfiguration WHERE master_record_id = ?");
        checkQuery.addBindValue(tableId);

        if (!checkQuery.exec() || !checkQuery.next())
        {
            qWarning() << funcName << " - 无法检查表ID " << tableId << " 的列配置:" << checkQuery.lastError().text();
            continue;
        }

        int columnCount = checkQuery.value(0).toInt();
        if (columnCount == 0)
        {
            qDebug() << funcName << " - 表ID " << tableId << " 没有列配置，尝试修复";
            if (fixExistingTableWithoutColumns(tableId))
            {
                fixedCount++;
                qDebug() << funcName << " - 成功修复表ID " << tableId << " 的列配置";
            }
            else
            {
                qWarning() << funcName << " - 修复表ID " << tableId << " 的列配置失败";
            }
        }
        else
        {
            qDebug() << funcName << " - 表ID " << tableId << " 有 " << columnCount << " 个列配置，不需要修复";
        }
    }

    qDebug() << funcName << " - 检查完成，共 " << totalCount << " 个表，修复了 " << fixedCount << " 个表";
}

// 辅助方法：从数据库加载向量表元数据
bool MainWindow::loadVectorTableMeta(int tableId, QString &binFileName, QList<Vector::ColumnInfo> &columns, int &schemaVersion, int &rowCount)
{
    const QString funcName = "MainWindow::loadVectorTableMeta";
    qDebug() << funcName << " - 查询表ID:" << tableId;
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << funcName << " - 数据库未打开";
        return false;
    }
    // 1. 查询主记录表
    QSqlQuery metaQuery(db);
    metaQuery.prepare("SELECT binary_data_filename, data_schema_version, row_count FROM VectorTableMasterRecord WHERE id = ?");
    metaQuery.addBindValue(tableId);
    if (!metaQuery.exec() || !metaQuery.next())
    {
        qWarning() << funcName << " - 查询主记录失败, 表ID:" << tableId << ", 错误:" << metaQuery.lastError().text();
        return false;
    }
    binFileName = metaQuery.value(0).toString();
    schemaVersion = metaQuery.value(1).toInt();
    rowCount = metaQuery.value(2).toInt();
    qDebug() << funcName << " - 文件名:" << binFileName << ", schemaVersion:" << schemaVersion << ", rowCount:" << rowCount;

    // 2. 查询列结构 - 只加载IsVisible=1的列
    QSqlQuery colQuery(db);
    colQuery.prepare("SELECT id, column_name, column_order, column_type, data_properties, IsVisible FROM VectorTableColumnConfiguration WHERE master_record_id = ? AND IsVisible = 1 ORDER BY column_order");
    colQuery.addBindValue(tableId);
    if (!colQuery.exec())
    {
        qWarning() << funcName << " - 查询列结构失败, 错误:" << colQuery.lastError().text();
        return false;
    }

    columns.clear();
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
            qDebug().nospace() << funcName << " - JSON Parsing Details for Column: '" << col.name
                               << "', Input: '" << propStr
                               << "', ErrorCode: " << err.error
                               << " (ErrorStr: " << err.errorString()
                               << "), IsObject: " << doc.isObject();

            if (err.error == QJsonParseError::NoError && doc.isObject())
            {
                col.data_properties = doc.object();
            }
            else
            {
                qWarning().nospace() << funcName << " - 列属性JSON解析判定为失败 (条件分支), 列: '" << col.name
                                     << "', Input: '" << propStr
                                     << "', ErrorCode: " << err.error
                                     << " (ErrorStr: " << err.errorString()
                                     << "), IsObject: " << doc.isObject();
            }
        }
        columns.append(col);
    }
    return true;
}

// Add implementation for reloadAndRefreshVectorTable
void MainWindow::reloadAndRefreshVectorTable(int tableId)
{
    const QString funcName = "MainWindow::reloadAndRefreshVectorTable";
    qDebug() << funcName << "- Reloading and refreshing UI for table ID:" << tableId;

    // 首先清除表格数据缓存，确保获取最新数据
    if (m_useNewDataHandler)
    {
        m_robustDataHandler->clearTableDataCache(tableId);
    }
    else
    {
        VectorDataHandler::instance().clearTableDataCache(tableId);
    }

    // 1. Ensure the table is selected in the ComboBox and TabWidget
    int comboBoxIndex = m_vectorTableSelector->findData(tableId);
    if (comboBoxIndex != -1)
    {
        if (m_vectorTableSelector->currentIndex() != comboBoxIndex)
        {
            m_vectorTableSelector->setCurrentIndex(comboBoxIndex); // This should trigger onVectorTableSelectionChanged
        }
        else
        {
            // If it's already selected, force a refresh of its data
            onVectorTableSelectionChanged(comboBoxIndex);
        }
    }
    else
    {
        qWarning() << funcName << "- Table ID" << tableId << "not found in selector. Cannot refresh.";
        // Fallback: reload all tables if the specific one isn't found (might be a new table not yet in UI)
        loadVectorTable();
    }

    // 2. Refresh the sidebar (in case table names or other project components changed)
    refreshSidebarNavigator();

    // 3. 确保波形图视图也被更新，特别是管脚选择器下拉框
    if (m_isWaveformVisible && m_waveformPlot && m_waveformPinSelector)
    {
        qDebug() << funcName << "- 显式更新波形图视图";
        updateWaveformView();
    }
}

// Add this new function implementation
QList<Vector::ColumnInfo> MainWindow::getCurrentColumnConfiguration(int tableId)
{
    const QString funcName = "MainWindow::getCurrentColumnConfiguration";
    QList<Vector::ColumnInfo> columns;
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << funcName << "- 数据库未打开";
        return columns;
    }

    QSqlQuery colQuery(db);
    // 获取所有列，无论是否可见，因为迁移需要处理所有物理列
    colQuery.prepare("SELECT id, column_name, column_order, column_type, data_properties, IsVisible "
                     "FROM VectorTableColumnConfiguration WHERE master_record_id = ? ORDER BY column_order");
    colQuery.addBindValue(tableId);

    if (!colQuery.exec())
    {
        qWarning() << funcName << "- 查询列结构失败, 表ID:" << tableId << ", 错误:" << colQuery.lastError().text();
        return columns;
    }

    while (colQuery.next())
    {
        Vector::ColumnInfo col;
        col.id = colQuery.value(0).toInt(); // This is the id from VectorTableColumnConfiguration table
        col.vector_table_id = tableId;      // The master_record_id it belongs to
        col.name = colQuery.value(1).toString();
        col.order = colQuery.value(2).toInt();
        col.original_type_str = colQuery.value(3).toString();               // Store the original string
        col.type = Vector::columnDataTypeFromString(col.original_type_str); // Convert to enum

        QString propStr = colQuery.value(4).toString();
        if (!propStr.isEmpty())
        {
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(propStr.toUtf8(), &err);
            if (err.error == QJsonParseError::NoError && doc.isObject())
            {
                col.data_properties = doc.object();
            }
            else
            {
                qWarning() << funcName << " - 解析列属性JSON失败 for column " << col.name << ", error: " << err.errorString();
            }
        }
        col.is_visible = colQuery.value(5).toBool();
        columns.append(col);
    }
    qDebug() << funcName << "- 为表ID:" << tableId << "获取了" << columns.size() << "列配置。";
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
