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
    qDebug() << funcName << " - Re-implementing for new schema. Adding default columns for table ID:" << tableId;

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qCritical() << funcName << " - Database is not open.";
        return false;
    }

    if (!db.transaction())
    {
        qCritical() << funcName << " - Failed to start a database transaction:" << db.lastError().text();
        return false;
    }

    struct DefaultColumn
    {
        QString name;
        QString type;
        int order;
        QString defaultValue;
    };

    const QList<DefaultColumn> columns_to_add = {
        {"Label", "TEXT", 0, ""},
        {"Instruction", "INSTRUCTION_ID", 1, "0"},
        {"TimeSet", "TIMESET_ID", 2, "1"},
        {"Capture", "BOOLEAN", 3, "0"},
        {"EXT", "TEXT", 4, ""},
        {"Comment", "TEXT", 5, ""}};

    QSqlQuery query(db);
    query.prepare(
        "INSERT INTO VectorTableColumnConfiguration "
        "(master_record_id, column_name, column_type, column_order, default_value, is_visible) "
        "VALUES (?, ?, ?, ?, ?, 1)");

    for (const auto &col : columns_to_add)
    {
        query.bindValue(0, tableId);
        query.bindValue(1, col.name);
        query.bindValue(2, col.type);
        query.bindValue(3, col.order);
        query.bindValue(4, col.defaultValue);

        if (!query.exec())
        {
            const QString errorMsg = QString("Failed to add column '%1': %2").arg(col.name).arg(query.lastError().text());
            qCritical() << funcName << " - Error:" << errorMsg;
            db.rollback();
            return false;
        }
    }

    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE VectorTableMasterRecord SET column_count = ? WHERE id = ?");
    updateQuery.addBindValue(columns_to_add.size());
    updateQuery.addBindValue(tableId);
    if (!updateQuery.exec())
    {
        const QString errorMsg = "Failed to update master record column count: " + updateQuery.lastError().text();
        qCritical() << funcName << " - Error:" << errorMsg;
        db.rollback();
        return false;
    }

    if (!db.commit())
    {
        qCritical() << funcName << " - Failed to commit the transaction:" << db.lastError().text();
        db.rollback(); // Attempt to rollback, though commit failed.
        return false;
    }

    qDebug() << funcName << " - Successfully added" << columns_to_add.size() << "default columns.";
    return true;
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

            // 添加管脚列配置
            QSqlQuery colInsertQuery(db);
            colInsertQuery.prepare("INSERT INTO VectorTableColumnConfiguration "
                                   "(master_record_id, column_name, column_order, column_type, default_value, is_visible) "
                                   "VALUES (?, ?, ?, ?, ?, 1)");
            colInsertQuery.addBindValue(tableId);
            colInsertQuery.addBindValue(pinName);
            colInsertQuery.addBindValue(columnOrder++);
            colInsertQuery.addBindValue("PIN_STATE_ID");
            colInsertQuery.addBindValue("0"); // 管脚默认值为 "0"

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

    // 添加条件判断逻辑：检查是否需要执行修复
    // 使用静态变量跟踪每个会话是否已执行过修复
    static bool hasRunInThisSession = false;
    static QMap<QString, bool> dbPathsChecked;

    // 获取当前数据库路径作为唯一标识符
    QString currentDbPath = m_currentDbPath;
    if (currentDbPath.isEmpty())
    {
        // 使用DatabaseManager中的m_dbFilePath成员变量
        auto dbManager = DatabaseManager::instance();
        if (dbManager && dbManager->isDatabaseConnected())
        {
            // 在DatabaseManager类中m_dbFilePath是私有的，但我们可以从主窗口成员获取
            currentDbPath = m_currentDbPath; // 使用MainWindow中的成员变量
        }
    }

    // 如果没有有效的数据库路径，使用唯一ID标识此会话
    if (currentDbPath.isEmpty())
    {
        currentDbPath = "current_session";
    }

    // 如果此会话已对此数据库执行过检查，则跳过
    if (hasRunInThisSession && dbPathsChecked.contains(currentDbPath))
    {
        qDebug() << funcName << " - 此数据库在本次会话中已执行过检查，跳过: " << currentDbPath;
        return;
    }

    qDebug() << funcName << " - 开始检查和修复所有向量表的列配置";

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qCritical() << funcName << " - 数据库未打开";
        return;
    }

    // 执行列类型格式修复
    // 先执行类型修复，以确保任何后续创建的列配置都有正确的文本类型值
    if (fixColumnTypeStorageFormat())
    {
        qDebug() << funcName << " - 成功修复了数据库中的列类型格式";
    }
    else
    {
        qWarning() << funcName << " - 修复数据库中的列类型格式失败，但将继续检查列配置";
    }

    // 执行管脚列类型值修复
    if (fixPinColumnTypes())
    {
        qDebug() << funcName << " - 成功修复了管脚列的类型值";
    }
    else
    {
        qWarning() << funcName << " - 修复管脚列类型值失败，但将继续检查列配置";
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

    // 记录此数据库在本会话中已执行过检查
    hasRunInThisSession = true;
    dbPathsChecked[currentDbPath] = true;

    // 修复后清除列配置缓存，确保后续获取的是正确配置
    clearColumnConfigCache();
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
    colQuery.prepare("SELECT id, column_name, column_order, column_type, default_value, is_visible FROM VectorTableColumnConfiguration WHERE master_record_id = ? AND is_visible = 1 ORDER BY column_order");
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
        col.type = Vector::columnDataTypeFromString(col.original_type_str, col.name);
        col.is_visible = colQuery.value(5).toBool();

        col.data_properties = QJsonObject(); // 不再需要解析 data_properties
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

    // 移除了旧的 onVectorTableSelectionChanged 调用。
    // 之前的调用因为会跳过同一个表的重载，导致列可见性等UI状态无法刷新。
    // 现在我们直接调用模型刷新和UI调整函数，强制更新视图。
    m_vectorTableModel->refreshColumns(tableId);
    adjustPinColumnWidths();

    // Refresh other parts of the UI
    refreshSidebarNavigator();

    // 显式更新波形图视图，但仅在自动更新启用时
    if (m_autoUpdateWaveform)
    {
        qDebug() << funcName << " - 自动更新已启用，准备更新波形图视图";
        updateWaveformView();
    }
    else
    {
        qDebug() << funcName << " - 自动更新已禁用，跳过波形图更新";
    }
}
