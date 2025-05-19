#include "timesetdataaccess.h"
#include "database/databasemanager.h"

TimeSetDataAccess::TimeSetDataAccess(QSqlDatabase &db) : m_db(db)
{
}

bool TimeSetDataAccess::loadWaveOptions(QMap<int, QString> &waveOptions)
{
    QSqlQuery query(m_db);

    if (query.exec("SELECT id, wave_type FROM wave_options"))
    {
        waveOptions.clear();
        while (query.next())
        {
            int id = query.value(0).toInt();
            QString waveType = query.value(1).toString();
            waveOptions[id] = waveType;
        }
        return true;
    }
    else
    {
        qWarning() << "加载波形选项失败:" << query.lastError().text();
        return false;
    }
}

bool TimeSetDataAccess::loadPins(QMap<int, QString> &pinList)
{
    QSqlQuery query(m_db);

    if (query.exec("SELECT id, pin_name FROM pin_list"))
    {
        pinList.clear();
        while (query.next())
        {
            int id = query.value(0).toInt();
            QString pinName = query.value(1).toString();
            pinList[id] = pinName;
        }
        return true;
    }
    else
    {
        qWarning() << "加载管脚列表失败:" << query.lastError().text();
        return false;
    }
}

bool TimeSetDataAccess::isTimeSetNameExists(const QString &name)
{
    QSqlQuery query(m_db);

    qDebug() << "检查TimeSet名称是否存在:" << name;

    // 使用COLLATE NOCASE来实现不区分大小写的查询
    query.prepare("SELECT id, timeset_name FROM timeset_list WHERE LOWER(timeset_name) = LOWER(?)");
    query.addBindValue(name);

    if (query.exec())
    {
        bool exists = query.next();
        if (exists)
        {
            // 添加调试信息，显示找到的记录
            QString existingName = query.value(1).toString();
            int existingId = query.value(0).toInt();
            qDebug() << "TimeSet名称已存在: id=" << existingId << ", name=" << existingName;
        }
        else
        {
            qDebug() << "TimeSet名称不存在";
        }
        return exists;
    }
    else
    {
        qWarning() << "检查TimeSet名称是否存在失败:" << query.lastError().text();
        qWarning() << "SQL错误代码:" << query.lastError().nativeErrorCode();
        return false;
    }
}

QList<TimeSetData> TimeSetDataAccess::loadExistingTimeSets()
{
    QList<TimeSetData> result;
    loadExistingTimeSets(result);
    return result;
}

bool TimeSetDataAccess::loadExistingTimeSets(QList<TimeSetData> &timeSetDataList)
{
    QSqlQuery query(m_db);

    qDebug() << "开始加载TimeSet数据...";

    // 检查数据库连接状态
    if (!m_db.isOpen())
    {
        qWarning() << "数据库连接未打开!";
        return false;
    }

    qDebug() << "执行SQL查询: SELECT id, timeset_name, period FROM timeset_list ORDER BY id";

    if (query.exec("SELECT id, timeset_name, period FROM timeset_list ORDER BY id"))
    {
        timeSetDataList.clear();
        int count = 0;

        qDebug() << "查询执行成功，开始遍历结果";

        while (query.next())
        {
            TimeSetData timeSet;
            timeSet.dbId = query.value(0).toInt();
            timeSet.name = query.value(1).toString();
            timeSet.period = query.value(2).toDouble();

            // 加载关联的管脚ID
            timeSet.pinIds = getPinIdsForTimeSet(timeSet.dbId);

            // 加载边缘设置
            timeSet.edges = loadTimeSetEdges(timeSet.dbId);

            timeSetDataList.append(timeSet);
            count++;

            qDebug() << "加载TimeSet: id=" << timeSet.dbId << ", name=" << timeSet.name
                     << ", period=" << timeSet.period
                     << ", 关联管脚数:" << timeSet.pinIds.size()
                     << ", 边缘设置数:" << timeSet.edges.size();
        }

        qDebug() << "成功加载" << count << "个TimeSet记录";

        if (count == 0)
        {
            // 查询是否有记录但未被加载
            QSqlQuery countQuery(m_db);
            if (countQuery.exec("SELECT COUNT(*) FROM timeset_list"))
            {
                if (countQuery.next())
                {
                    int totalCount = countQuery.value(0).toInt();
                    qDebug() << "数据库中实际存在" << totalCount << "个TimeSet记录";
                }
            }
            else
            {
                qWarning() << "检查TimeSet总数失败:" << countQuery.lastError().text();
            }
        }

        return true;
    }
    else
    {
        qWarning() << "加载TimeSet设置失败:" << query.lastError().text();
        qWarning() << "SQL错误代码:" << query.lastError().nativeErrorCode();
        return false;
    }
}

QList<int> TimeSetDataAccess::getPinIdsForTimeSet(int timeSetId)
{
    QList<int> pinIds;
    QSqlQuery pinQuery(m_db);
    pinQuery.prepare("SELECT pin_id FROM timeset_pins WHERE timeset_id = ?");
    pinQuery.addBindValue(timeSetId);

    if (pinQuery.exec())
    {
        while (pinQuery.next())
        {
            pinIds.append(pinQuery.value(0).toInt());
        }
    }

    return pinIds;
}

QList<TimeSetEdgeData> TimeSetDataAccess::loadTimeSetEdges(int timeSetId)
{
    QList<TimeSetEdgeData> edges;
    if (!m_db.isOpen())
    {
        qWarning() << "TimeSetDataAccess::loadTimeSetEdges - 错误：数据库未连接！";
        return edges; // Return empty list
    }

    QSqlQuery query(m_db);
    QString selectSql = "SELECT timeset_id, pin_id, T1R, T1F, STBR, wave_id FROM timeset_settings WHERE timeset_id = ?";
    qDebug() << "TimeSetDataAccess::loadTimeSetEdges - 准备执行查询SQL:" << selectSql << "with ID:" << timeSetId;
    if (!query.prepare(selectSql))
    {
        qWarning() << "准备加载边沿参数失败:" << query.lastError().text();
        return edges;
    }
    query.addBindValue(timeSetId);

    if (!query.exec())
    {
        qWarning() << "加载边沿参数失败:" << query.lastError().text();
        qWarning() << "失败的SQL:" << selectSql << "参数:" << timeSetId;
    }
    else
    {
        qDebug() << "TimeSetDataAccess::loadTimeSetEdges - 查询成功 for TimeSet ID:" << timeSetId;
        while (query.next())
        {
            TimeSetEdgeData edge;
            edge.timesetId = query.value(0).toInt();
            edge.pinId = query.value(1).toInt();
            edge.t1r = query.value(2).toDouble();
            edge.t1f = query.value(3).toDouble();
            edge.stbr = query.value(4).toDouble();
            edge.waveId = query.value(5).toInt();
            edges.append(edge);
            qDebug() << "TimeSetDataAccess::loadTimeSetEdges - 加载边沿: Pin ID=" << edge.pinId;
        }
        qDebug() << "TimeSetDataAccess::loadTimeSetEdges - 总共加载" << edges.size() << "个边沿 for TimeSet ID:" << timeSetId;
    }
    query.finish(); // Explicitly finish
    return edges;
}

bool TimeSetDataAccess::saveTimeSetToDatabase(const TimeSetData &timeSet, int &outTimeSetId)
{
    QSqlQuery query(m_db);

    if (timeSet.dbId <= 0)
    {
        // 新增TimeSet
        query.prepare("INSERT INTO timeset_list (timeset_name, period) VALUES (?, ?)");
        query.addBindValue(timeSet.name);
        query.addBindValue(timeSet.period);

        if (!query.exec())
        {
            qWarning() << "新增TimeSet失败:" << query.lastError().text();
            return false;
        }

        outTimeSetId = query.lastInsertId().toInt();
    }
    else
    {
        // 更新现有TimeSet
        query.prepare("UPDATE timeset_list SET timeset_name = ?, period = ? WHERE id = ?");
        query.addBindValue(timeSet.name);
        query.addBindValue(timeSet.period);
        query.addBindValue(timeSet.dbId);

        if (!query.exec())
        {
            qWarning() << "更新TimeSet失败:" << query.lastError().text();
            return false;
        }

        outTimeSetId = timeSet.dbId;
    }

    // 更新管脚关联
    if (!savePinSelection(outTimeSetId, timeSet.pinIds))
    {
        return false;
    }

    return true;
}

bool TimeSetDataAccess::savePinSelection(int timeSetId, const QList<int> &selectedPinIds)
{
    // 在执行任何查询前，严格检查数据库连接状态
    if (!m_db.isOpen())
    {
        qWarning() << "TimeSetDataAccess::savePinSelection - 错误：数据库未连接！";
        return false;
    }

    qDebug() << "TimeSetDataAccess::savePinSelection - 开始为 TimeSet ID:" << timeSetId << "保存管脚选择";

    // 先删除旧关联
    QSqlQuery deleteQuery(m_db);
    QString deleteSql = "DELETE FROM timeset_settings WHERE timeset_id = ?";
    qDebug() << "TimeSetDataAccess::savePinSelection - 准备执行删除SQL:" << deleteSql << "with ID:" << timeSetId;
    deleteQuery.prepare(deleteSql);
    deleteQuery.addBindValue(timeSetId);

    if (!deleteQuery.exec())
    {
        qWarning() << "删除TimeSet管脚关联失败:" << deleteQuery.lastError().text();
        qWarning() << "失败的SQL:" << deleteSql << "参数:" << timeSetId;
        deleteQuery.finish(); // Explicitly finish
        return false;
    }
    qDebug() << "TimeSetDataAccess::savePinSelection - 成功删除旧关联 for TimeSet ID:" << timeSetId;
    deleteQuery.finish(); // Explicitly finish

    // 添加新关联
    if (selectedPinIds.isEmpty())
    {
        qDebug() << "TimeSetDataAccess::savePinSelection - 没有需要添加的新管脚关联 for TimeSet ID:" << timeSetId;
        return true; // 没有新管脚需要添加，操作成功
    }

    qDebug() << "TimeSetDataAccess::savePinSelection - 开始添加" << selectedPinIds.size() << "个新管脚关联 for TimeSet ID:" << timeSetId;
    for (int pinId : selectedPinIds)
    {
        // 再次检查数据库连接状态（以防万一）
        if (!m_db.isOpen())
        {
            qWarning() << "TimeSetDataAccess::savePinSelection - 错误：在循环中数据库未连接！";
            return false;
        }

        QSqlQuery insertQuery(m_db);
        QString insertSql = "INSERT INTO timeset_settings (timeset_id, pin_id, T1R, T1F, STBR, wave_id) VALUES (?, ?, ?, ?, ?, ?)";
        insertQuery.prepare(insertSql);
        insertQuery.addBindValue(timeSetId);
        insertQuery.addBindValue(pinId);
        insertQuery.addBindValue(250.0); // 默认T1R
        insertQuery.addBindValue(750.0); // 默认T1F
        insertQuery.addBindValue(500.0); // 默认STBR
        insertQuery.addBindValue(1);     // 默认wave_id

        if (!insertQuery.exec())
        {
            qWarning() << "添加TimeSet管脚关联失败:" << insertQuery.lastError().text();
            qWarning() << "失败的SQL:" << insertSql << "参数: (" << timeSetId << "," << pinId << ", 250, 750, 500, 1)";
            insertQuery.finish(); // Explicitly finish
            return false;
        }
        qDebug() << "TimeSetDataAccess::savePinSelection - 成功添加关联: TimeSet ID=" << timeSetId << ", Pin ID=" << pinId;
        insertQuery.finish(); // Explicitly finish
    }

    qDebug() << "TimeSetDataAccess::savePinSelection - 成功完成 for TimeSet ID:" << timeSetId;
    return true;
}

bool TimeSetDataAccess::saveTimeSetEdgesToDatabase(int timeSetId, const QList<TimeSetEdgeData> &edges)
{
    if (!m_db.isOpen())
    {
        qWarning() << "TimeSetDataAccess::saveTimeSetEdgesToDatabase - 错误：数据库未连接！ (at start)";
        return false;
    }
    qDebug() << "TimeSetDataAccess::saveTimeSetEdgesToDatabase - 开始为 TimeSet ID:" << timeSetId << "保存边沿参数";

    // 先删除旧的边沿参数
    { // Scope for deleteQuery
        QSqlQuery deleteQuery(m_db);
        QString deleteSql = "DELETE FROM timeset_settings WHERE timeset_id = ?";
        qDebug() << "TimeSetDataAccess::saveTimeSetEdgesToDatabase - 准备执行删除SQL:" << deleteSql << "with ID:" << timeSetId;
        if (!deleteQuery.prepare(deleteSql))
        {
            qWarning() << "准备删除旧边沿参数失败:" << deleteQuery.lastError().text();
            return false;
        }
        deleteQuery.addBindValue(timeSetId);

        if (!m_db.isOpen())
        { // Check connection *just before* exec
            qWarning() << "TimeSetDataAccess::saveTimeSetEdgesToDatabase - 错误：数据库在执行删除前断开！";
            return false;
        }

        if (!deleteQuery.exec())
        {
            qWarning() << "删除旧边沿参数失败:" << deleteQuery.lastError().text();
            qWarning() << "失败的SQL:" << deleteSql << "参数:" << timeSetId;
            deleteQuery.finish(); // Explicitly finish
            return false;
        }
        qDebug() << "TimeSetDataAccess::saveTimeSetEdgesToDatabase - 成功删除旧边沿参数 for TimeSet ID:" << timeSetId;
        deleteQuery.finish(); // Explicitly finish
    } // deleteQuery goes out of scope here

    // 添加新的边沿参数
    if (edges.isEmpty())
    {
        qDebug() << "TimeSetDataAccess::saveTimeSetEdgesToDatabase - 没有新的边沿参数需要添加 for TimeSet ID:" << timeSetId;
        return true;
    }

    qDebug() << "TimeSetDataAccess::saveTimeSetEdgesToDatabase - 开始添加" << edges.size() << "个新的边沿参数 for TimeSet ID:" << timeSetId;
    for (const TimeSetEdgeData &edge : edges)
    {
        if (!m_db.isOpen())
        { // Check connection in loop
            qWarning() << "TimeSetDataAccess::saveTimeSetEdgesToDatabase - 错误：数据库在插入循环中断开！";
            return false;
        }

        QSqlQuery insertQuery(m_db); // New query object for each iteration
        QString insertSql = "INSERT INTO timeset_settings (timeset_id, pin_id, T1R, T1F, STBR, wave_id) VALUES (?, ?, ?, ?, ?, ?)";
        if (!insertQuery.prepare(insertSql))
        {
            qWarning() << "准备插入边沿参数失败:" << insertQuery.lastError().text();
            insertQuery.finish(); // Explicitly finish before returning
            return false;         // Stop if prepare fails
        }
        insertQuery.addBindValue(timeSetId);
        insertQuery.addBindValue(edge.pinId);
        insertQuery.addBindValue(edge.t1r);
        insertQuery.addBindValue(edge.t1f);
        insertQuery.addBindValue(edge.stbr);
        insertQuery.addBindValue(edge.waveId);

        if (!insertQuery.exec())
        {
            qWarning() << "添加边沿参数失败:" << insertQuery.lastError().text();
            qWarning() << "失败的SQL:" << insertSql << "参数: (" << timeSetId << "," << edge.pinId << "," << edge.t1r << "," << edge.t1f << "," << edge.stbr << "," << edge.waveId << ")";
            insertQuery.finish(); // Explicitly finish
            return false;         // Stop on first error
        }
        qDebug() << "TimeSetDataAccess::saveTimeSetEdgesToDatabase - 成功添加边沿: TS ID=" << timeSetId << ", Pin ID=" << edge.pinId;
        insertQuery.finish(); // Explicitly finish
    }

    qDebug() << "TimeSetDataAccess::saveTimeSetEdgesToDatabase - 成功完成 for TimeSet ID:" << timeSetId;
    return true;
}

bool TimeSetDataAccess::updateTimeSetName(int timeSetId, const QString &newName)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE timeset_list SET timeset_name = ? WHERE id = ?");
    query.addBindValue(newName);
    query.addBindValue(timeSetId);

    if (!query.exec())
    {
        qWarning() << "更新TimeSet名称失败:" << query.lastError().text();
        return false;
    }

    return true;
}

bool TimeSetDataAccess::updateTimeSetPeriod(int timeSetId, double period)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE timeset_list SET period = ? WHERE id = ?");
    query.addBindValue(period);
    query.addBindValue(timeSetId);

    if (!query.exec())
    {
        qWarning() << "更新TimeSet周期失败:" << query.lastError().text();
        return false;
    }

    return true;
}

bool TimeSetDataAccess::deleteTimeSet(int timeSetId)
{
    // 先删除关联的边沿参数
    QSqlQuery edgeQuery(m_db);
    edgeQuery.prepare("DELETE FROM timeset_settings WHERE timeset_id = ?");
    edgeQuery.addBindValue(timeSetId);

    if (!edgeQuery.exec())
    {
        qWarning() << "删除关联的边沿参数失败:" << edgeQuery.lastError().text();
        edgeQuery.finish(); // 添加 finish
        return false;
    }
    edgeQuery.finish(); // 添加 finish

    // 删除TimeSet主条目
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM timeset_list WHERE id = ?");
    query.addBindValue(timeSetId);

    if (!query.exec())
    {
        qWarning() << "删除TimeSet主条目失败:" << query.lastError().text();
        query.finish(); // 添加 finish
        return false;
    }
    query.finish(); // 添加 finish

    qDebug() << "TimeSetDataAccess::deleteTimeSet - 成功删除 TimeSet ID:" << timeSetId;
    return true;
}

bool TimeSetDataAccess::deleteTimeSetEdge(int timeSetId, int pinId)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM timeset_settings WHERE timeset_id = ? AND pin_id = ?");
    query.addBindValue(timeSetId);
    query.addBindValue(pinId);

    if (!query.exec())
    {
        qWarning() << "删除边沿参数失败:" << query.lastError().text();
        query.finish(); // 在失败时也调用 finish
        return false;
    }

    query.finish(); // 确保查询对象被正确清理
    return true;
}

bool TimeSetDataAccess::loadVectorData(int tableId, QTableWidget *vectorTable)
{
    // 首先查询表的行数
    QSqlQuery rowCountQuery(m_db);
    rowCountQuery.prepare("SELECT MAX(row_id) FROM vector_data WHERE table_id = ?");
    rowCountQuery.addBindValue(tableId);

    int maxRows = 0;
    if (rowCountQuery.exec() && rowCountQuery.next())
    {
        maxRows = rowCountQuery.value(0).toInt() + 1; // 行ID从0开始
    }

    // 查询所有管脚选项 - 修改为使用vector_table_pins表和pin_list表
    QSqlQuery pinOptionQuery(m_db);
    pinOptionQuery.prepare("SELECT pl.id as pin_id, pl.pin_name FROM pin_list pl "
                           "JOIN vector_table_pins vtp ON pl.id = vtp.pin_id "
                           "WHERE vtp.table_id = ? ORDER BY vtp.id");
    pinOptionQuery.addBindValue(tableId);

    QStringList pinOptions;
    QMap<int, QString> pinIdToName;
    if (pinOptionQuery.exec())
    {
        while (pinOptionQuery.next())
        {
            int pinId = pinOptionQuery.value(0).toInt();
            QString pinName = pinOptionQuery.value(1).toString();
            pinOptions << pinName;
            pinIdToName[pinId] = pinName;
        }
    }

    // 设置表格列数
    vectorTable->setColumnCount(pinOptions.size());
    vectorTable->setHorizontalHeaderLabels(pinOptions);

    // 设置行数
    vectorTable->setRowCount(maxRows);

    // 加载数据 - 修改为使用vector_table_pins表关联
    QSqlQuery dataQuery(m_db);
    dataQuery.prepare("SELECT vd.row_id, vd.pin_id, vd.pin_value "
                      "FROM vector_data vd "
                      "JOIN vector_table_pins vtp ON vd.pin_id = vtp.pin_id AND vd.table_id = vtp.table_id "
                      "WHERE vd.table_id = ? "
                      "ORDER BY vd.row_id, vtp.id");
    dataQuery.addBindValue(tableId);

    if (dataQuery.exec())
    {
        while (dataQuery.next())
        {
            int rowId = dataQuery.value(0).toInt();
            int pinId = dataQuery.value(1).toInt();
            QString pinValue = dataQuery.value(2).toString();

            // 使用pinIdToName映射获取管脚名称
            QString pinName = pinIdToName.value(pinId);

            // 找到对应的列
            int col = pinOptions.indexOf(pinName);
            if (col >= 0)
            {
                QTableWidgetItem *item = new QTableWidgetItem(pinValue);
                vectorTable->setItem(rowId, col, item);
            }
        }
    }
    else
    {
        qWarning() << "加载向量数据失败:" << dataQuery.lastError().text();
        return false;
    }

    return true;
}

bool TimeSetDataAccess::saveVectorData(int tableId, QTableWidget *vectorTable, int insertPosition, bool appendToEnd)
{
    // 获取管脚ID映射 - 修改为使用vector_table_pins表和pin_list表
    QSqlQuery pinOptionQuery(m_db);
    pinOptionQuery.prepare("SELECT pl.pin_name, pl.id as pin_id FROM pin_list pl "
                           "JOIN vector_table_pins vtp ON pl.id = vtp.pin_id "
                           "WHERE vtp.table_id = ? ORDER BY vtp.id");
    pinOptionQuery.addBindValue(tableId);

    QMap<QString, int> pinNameToId;
    if (pinOptionQuery.exec())
    {
        while (pinOptionQuery.next())
        {
            QString pinName = pinOptionQuery.value(0).toString();
            int pinId = pinOptionQuery.value(1).toInt();
            pinNameToId[pinName] = pinId;
        }
    }
    else
    {
        qWarning() << "获取管脚选项失败:" << pinOptionQuery.lastError().text();
        return false;
    }

    // 开始事务
    m_db.transaction();

    // 决定插入位置
    int startRow = 0;
    if (appendToEnd)
    {
        // 获取最大行ID
        QSqlQuery maxRowQuery(m_db);
        maxRowQuery.prepare("SELECT MAX(row_id) FROM vector_data WHERE table_id = ?");
        maxRowQuery.addBindValue(tableId);

        if (maxRowQuery.exec() && maxRowQuery.next())
        {
            startRow = maxRowQuery.value(0).toInt() + 1;
        }
    }
    else
    {
        startRow = insertPosition;

        // 为新行腾出空间，将现有数据行ID向后移动
        QSqlQuery updateRowsQuery(m_db);
        updateRowsQuery.prepare("UPDATE vector_data SET row_id = row_id + ? "
                                "WHERE table_id = ? AND row_id >= ? "
                                "ORDER BY row_id DESC");
        updateRowsQuery.addBindValue(vectorTable->rowCount());
        updateRowsQuery.addBindValue(tableId);
        updateRowsQuery.addBindValue(startRow);

        if (!updateRowsQuery.exec())
        {
            qWarning() << "移动现有行失败:" << updateRowsQuery.lastError().text();
            m_db.rollback();
            return false;
        }
    }

    // 插入新行数据
    for (int row = 0; row < vectorTable->rowCount(); row++)
    {
        for (int col = 0; col < vectorTable->columnCount(); col++)
        {
            QTableWidgetItem *item = vectorTable->item(row, col);
            if (item)
            {
                QString pinName = vectorTable->horizontalHeaderItem(col)->text();
                if (pinNameToId.contains(pinName))
                {
                    int pinId = pinNameToId[pinName];
                    QString pinValue = item->text();

                    QSqlQuery insertQuery(m_db);
                    insertQuery.prepare("INSERT INTO vector_data (table_id, row_id, pin_id, pin_value) "
                                        "VALUES (?, ?, ?, ?)");
                    insertQuery.addBindValue(tableId);
                    insertQuery.addBindValue(startRow + row);
                    insertQuery.addBindValue(pinId);
                    insertQuery.addBindValue(pinValue);

                    if (!insertQuery.exec())
                    {
                        qWarning() << "插入向量数据失败:" << insertQuery.lastError().text();
                        m_db.rollback();
                        return false;
                    }
                }
            }
        }
    }

    // 提交事务
    if (!m_db.commit())
    {
        qWarning() << "提交事务失败:" << m_db.lastError().text();
        m_db.rollback();
        return false;
    }

    return true;
}

// 新增：检查TimeSet是否被向量表使用
bool TimeSetDataAccess::isTimeSetInUse(int timeSetId)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT COUNT(*) FROM vector_table_data WHERE timeset_id = ?");
    query.addBindValue(timeSetId);

    if (query.exec() && query.next())
    {
        int count = query.value(0).toInt();
        qDebug() << "TimeSetDataAccess::isTimeSetInUse - TimeSet ID:" << timeSetId << "在vector_table_data中的引用计数:" << count;
        return count > 0;
    }
    else
    {
        qWarning() << "检查TimeSet是否在使用失败:" << query.lastError().text();
        return false; // 查询失败时，保守地认为它可能在使用中或返回错误
    }
}