// 实现表格行操作方法

bool VectorTableModel::insertRows(int row, int count, const QModelIndex &parent)
{
    if (parent.isValid() || count <= 0)
    {
        return false;
    }

    if (!m_robustDataHandler)
    {
        qWarning() << "VectorTableModel::insertRows - RobustVectorDataHandler is null.";
        return false;
    }

    // 性能优化：预先获取列信息
    static QList<Vector::ColumnInfo> columns;
    // 如果列信息为空或表ID变更，重新获取列信息
    if (columns.isEmpty() || m_lastTableId != m_tableId)
    {
        columns = m_robustDataHandler->getVisibleColumns(m_tableId);
        m_lastTableId = m_tableId;
    }

    if (columns.isEmpty())
    {
        qWarning() << "VectorTableModel::insertRows - Failed to get column info for table" << m_tableId;
        return false;
    }

    // 性能优化：预分配内存空间，减少重新分配次数
    QList<Vector::RowData> rowsToInsert;
    rowsToInsert.reserve(count);

    // 2. 高效创建新行数据 - 使用固定模板减少循环开销
    Vector::RowData templateRow;
    templateRow.reserve(columns.size());

    // 创建一个模板行，后续直接复制
    for (const auto &colInfo : columns)
    {
        // 根据列类型设置合理的默认值
        switch (colInfo.type)
        {
        case Vector::ColumnDataType::TEXT:
            templateRow.append(QString(""));
            break;
        case Vector::ColumnDataType::INSTRUCTION_ID:
            templateRow.append(1); // 默认指令ID
            break;
        case Vector::ColumnDataType::TIMESET_ID:
            templateRow.append(1); // 默认TimeSet ID
            break;
        case Vector::ColumnDataType::BOOLEAN: // Capture
            templateRow.append(false);
            break;
        case Vector::ColumnDataType::PIN_STATE_ID:
            templateRow.append("X"); // 默认管脚状态
            break;
        default:
            templateRow.append(QVariant()); // 其他类型使用空的QVariant
            break;
        }
    }

    // 性能优化：批量生成所有行，避免多次循环
    for (int i = 0; i < count; ++i)
    {
        // 直接复制模板行，避免重复创建
        rowsToInsert.append(templateRow);
    }

    // 3. 调用持久化层接口，将新行数据写入后端存储
    QString errorMessage;
    bool success = m_robustDataHandler->insertVectorRows(m_tableId, row, rowsToInsert, errorMessage);

    if (!success)
    {
        qWarning() << "VectorTableModel::insertRows - Backend insertion failed:" << errorMessage;
        return false;
    }

    // 4. 持久化成功后，再更新内存模型并通知视图
    beginInsertRows(parent, row, row + count - 1);

    // 性能优化：避免逐个插入，使用更高效的方法
    if (row == m_pageData.size())
    {
        // 如果是在末尾添加，直接附加
        m_pageData.append(rowsToInsert);
    }
    else
    {
        // 如果是在中间插入，先预留空间
        for (int i = 0; i < count; ++i)
        {
            m_pageData.insert(row + i, rowsToInsert.at(i));
        }
    }

    m_totalRows += count;

    endInsertRows();

    return true;
}

bool VectorTableModel::removeRows(int row, int count, const QModelIndex &parent)
{
    // 检查参数是否有效
    if (parent.isValid() || row < 0 || count <= 0 || (row + count) > m_pageData.size() || m_tableId <= 0)
        return false;

    const QString funcName = "VectorTableModel::removeRows";
    qDebug() << funcName << "- 开始从位置" << row << "删除" << count << "行";

    // 获取要删除的行的实际索引（考虑分页）
    QList<int> rowIndexes;
    for (int i = 0; i < count; i++)
    {
        rowIndexes.append(row + i + (m_currentPage * m_pageSize));
    }

    // 调用VectorDataHandler删除行
    QString errorMessage;
    bool deleteSuccess;
    if (m_useNewDataHandler)
    {
        deleteSuccess = m_robustDataHandler->deleteVectorRows(m_tableId, rowIndexes, errorMessage);
    }
    else
    {
        deleteSuccess = VectorDataHandler::instance().deleteVectorRows(m_tableId, rowIndexes, errorMessage);
    }
    if (!deleteSuccess)
    {
        qWarning() << funcName << "- 删除行失败:" << errorMessage;
        return false;
    }

    // 开始移除行
    beginRemoveRows(parent, row, row + count - 1);

    // 从模型数据中移除行
    for (int i = 0; i < count; i++)
    {
        if (row < m_pageData.size())
        {
            m_pageData.removeAt(row);
        }
    }

    // 更新总行数
    m_totalRows -= count;

    // 结束移除行
    endRemoveRows();

    qDebug() << funcName << "- 已成功删除" << count << "行";
    return true;
}

bool VectorTableModel::deleteSelectedRows(const QList<int> &rowIndexes, QString &errorMessage)
{
    const QString funcName = "VectorTableModel::deleteSelectedRows";
    qDebug() << funcName << "- 开始删除选中的行，共" << rowIndexes.size() << "行";

    if (rowIndexes.isEmpty() || m_tableId <= 0)
    {
        errorMessage = "没有选中任何行或表ID无效";
        return false;
    }

    // 获取绝对行索引（考虑分页）
    QList<int> absoluteRowIndexes;
    for (int rowIndex : rowIndexes)
    {
        // 计算绝对行索引
        int absoluteRowIndex = rowIndex;
        absoluteRowIndexes.append(absoluteRowIndex);
    }

    // 调用VectorDataHandler删除行
    bool deleteSuccess;
    if (m_useNewDataHandler)
    {
        deleteSuccess = m_robustDataHandler->deleteVectorRows(m_tableId, absoluteRowIndexes, errorMessage);
    }
    else
    {
        deleteSuccess = VectorDataHandler::instance().deleteVectorRows(m_tableId, absoluteRowIndexes, errorMessage);
    }
    if (!deleteSuccess)
    {
        qWarning() << funcName << "- 删除行失败:" << errorMessage;
        return false;
    }

    // 重新加载数据
    if (m_useNewDataHandler)
    {
        // 如果使用新的数据处理器，加载全部数据（不分页）
        qDebug() << funcName << "- 使用新数据处理器，加载全部数据";
        loadAllData(m_tableId);
    }
    else
    {
        // 如果使用旧的数据处理器，仍然使用分页加载
        qDebug() << funcName << "- 使用旧数据处理器，加载当前页数据";
        loadPage(m_tableId, m_currentPage);
    }

    qDebug() << funcName << "- 已成功删除选中的行";
    return true;
}

bool VectorTableModel::deleteRowsInRange(int fromRow, int toRow, QString &errorMessage)
{
    const QString funcName = "VectorTableModel::deleteRowsInRange";
    qDebug() << funcName << "- 开始删除从第" << fromRow << "行到第" << toRow << "行";

    if (fromRow <= 0 || toRow <= 0 || fromRow > toRow || m_tableId <= 0)
    {
        errorMessage = "无效的行范围或表ID";
        return false;
    }

    // 调用VectorDataHandler删除行范围
    bool deleteSuccess;
    if (m_useNewDataHandler)
    {
        deleteSuccess = m_robustDataHandler->deleteVectorRowsInRange(m_tableId, fromRow, toRow, errorMessage);
    }
    else
    {
        deleteSuccess = VectorDataHandler::instance().deleteVectorRowsInRange(m_tableId, fromRow, toRow, errorMessage);
    }
    if (!deleteSuccess)
    {
        qWarning() << funcName << "- 删除行范围失败:" << errorMessage;
        return false;
    }

    // 重新加载数据
    if (m_useNewDataHandler)
    {
        // 如果使用新的数据处理器，加载全部数据（不分页）
        qDebug() << funcName << "- 使用新数据处理器，加载全部数据";
        loadAllData(m_tableId);
    }
    else
    {
        // 如果使用旧的数据处理器，仍然使用分页加载
        qDebug() << funcName << "- 使用旧数据处理器，加载当前页数据";
        loadPage(m_tableId, m_currentPage);
    }

    qDebug() << funcName << "- 已成功删除行范围";
    return true;
}

bool VectorTableModel::addNewRow(int timesetId, const QMap<int, QString> &pinValues, QString &errorMessage)
{
    // 此方法已废弃，所有行添加操作应通过insertRows进行。
    // 保留此空函数体是为了防止在完全移除前，某些旧的调用链路导致编译失败。
    qWarning() << "VectorTableModel::addNewRow is deprecated and should not be used. Use insertRows instead.";
    errorMessage = "This function is deprecated.";
    return false;
}

// 新轨道模式：加载表格的所有数据
void VectorTableModel::loadAllData(int tableId)
{
    qDebug() << "VectorTableModel::loadAllData - 一次性加载表ID:" << tableId << "的所有数据";

    // 保存新的表ID
    m_tableId = tableId;
    m_currentPage = 0; // 在全量数据模式下，页码概念不再重要，但保持为0

    // 刷新指令和TimeSet的缓存
    refreshCaches();

    // 告诉视图我们要开始修改数据了
    beginResetModel();

    // 获取列信息
    if (m_useNewDataHandler)
    {
        m_columns = m_robustDataHandler->getVisibleColumns(tableId);
    }
    else
    {
        m_columns = VectorDataHandler::instance().getVisibleColumns(tableId);
    }

    // 获取总行数
    if (m_useNewDataHandler)
    {
        m_totalRows = m_robustDataHandler->getVectorTableRowCount(tableId);
    }
    else
    {
        m_totalRows = VectorDataHandler::instance().getVectorTableRowCount(tableId);
    }

    // 获取所有数据
    if (m_useNewDataHandler)
    {
        bool ok = false;
        QList<QList<QVariant>> allRows = m_robustDataHandler->getAllVectorRows(tableId, ok);
        if (ok)
        {
            // 直接设置模型数据，而不是累加
            m_pageData = allRows;
            qDebug() << "VectorTableModel::loadAllData - 成功加载全部" << allRows.size() << "行数据";
        }
        else
        {
            qWarning() << "VectorTableModel::loadAllData - 加载全部数据失败";
            m_pageData.clear();
        }
    }
    else
    {
        // 旧数据处理器不支持全量加载，使用分页加载的第一页
        m_pageData = VectorDataHandler::instance().getPageData(tableId, 0, m_pageSize);
        qWarning() << "VectorTableModel::loadAllData - 旧数据处理器不支持全量加载，已加载第一页" << m_pageData.size() << "行";
    }

    // 告诉视图我们已经修改完数据
    endResetModel();

    qDebug() << "VectorTableModel::loadAllData - 加载完成，列数:" << m_columns.size()
             << "，加载行数:" << m_pageData.size() << "，总行数:" << m_totalRows;
}

void VectorTableModel::refreshColumns(int tableId)
{
    // 确保表ID有效
    if (tableId <= 0 || tableId != m_tableId)
    {
        qWarning() << "VectorTableModel::refreshColumns - 无效的表ID或与当前加载的表ID不匹配";
        return;
    }

    // 获取最新的列信息
    if (m_useNewDataHandler)
    {
        m_columns = m_robustDataHandler->getVisibleColumns(tableId);
    }
    else
    {
        m_columns = VectorDataHandler::instance().getVisibleColumns(tableId);
    }

    // 通知视图列数可能已更改
    beginResetModel();
    endResetModel();

    qDebug() << "VectorTableModel::refreshColumns - 成功刷新表ID:" << tableId
             << "的列信息，当前列数:" << m_columns.size();
}