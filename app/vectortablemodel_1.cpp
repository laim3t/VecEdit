// 实现表格行操作方法

bool VectorTableModel::insertRows(int row, int count, const QModelIndex &parent)
{
    if (parent.isValid() || count <= 0)
        return false;

    if (!m_robustDataHandler)
    {
        qWarning() << "VectorTableModel::insertRows - RobustVectorDataHandler is null.";
        return false;
    }

    // 1. 获取当前表的列定义，以构建新行的数据结构
    QList<Vector::ColumnInfo> columns = m_robustDataHandler->getVisibleColumns(m_tableId);
    if (columns.isEmpty())
    {
        qWarning() << "VectorTableModel::insertRows - Failed to get column info for table" << m_tableId;
        return false;
    }

    // 2. 根据列定义创建'count'个带有默认值的新行
    QList<Vector::RowData> rowsToInsert;
    for (int i = 0; i < count; ++i)
    {
        Vector::RowData newRow;
        for (const auto &colInfo : columns)
        {
            // 根据列类型设置合理的默认值
            switch (colInfo.type)
            {
            case Vector::ColumnDataType::TEXT:
                // 例如，Label 和 Comment 列可以为空
                if (colInfo.name.compare("Label", Qt::CaseInsensitive) == 0)
                {
                    // 不再为Label列生成默认值，使用空字符串
                    newRow.append(QString(""));
                }
                else
                {
                    newRow.append(QString(""));
                }
                break;
            case Vector::ColumnDataType::INSTRUCTION_ID:
                newRow.append(1); // 默认指令ID
                break;
            case Vector::ColumnDataType::TIMESET_ID:
                newRow.append(1); // 默认TimeSet ID
                break;
            case Vector::ColumnDataType::BOOLEAN: // Capture
                newRow.append(false);
                break;
            case Vector::ColumnDataType::PIN_STATE_ID:
                newRow.append("X"); // 默认管脚状态
                break;
            default:
                newRow.append(QVariant()); // 其他类型使用空的QVariant
                break;
            }
        }
        rowsToInsert.append(newRow);
    }

    // 通知视图我们将要插入行
    beginInsertRows(parent, row, row + count - 1);

    // 3. 调用新的、正确的insertVectorRows接口插入到后端存储
    QString errorMessage;
    bool success = m_robustDataHandler->insertVectorRows(m_tableId, row, rowsToInsert, errorMessage);

    if (success)
    {
        // 更新总行数
        m_totalRows += count;
    }
    else
    {
        qWarning() << "VectorTableModel::insertRows failed:" << errorMessage;
    }

    // 完成行插入操作
    endInsertRows();
    
    // 如果插入成功，只加载当前页的数据，而不是全部数据
    if (success)
    {
        // 注意：我们不调用loadAllData而是重新加载当前页数据
        // 加载的页码应该是0，确保首先加载首页
        m_currentPage = 0;
        
        // 获取列信息
        if (m_useNewDataHandler)
        {
            m_columns = m_robustDataHandler->getVisibleColumns(m_tableId);
        }
        else
        {
            m_columns = VectorDataHandler::instance().getVisibleColumns(m_tableId);
        }
        
        // 更新总行数
        if (m_useNewDataHandler)
        {
            m_totalRows = m_robustDataHandler->getVectorTableRowCount(m_tableId);
        }
        else
        {
            m_totalRows = VectorDataHandler::instance().getVectorTableRowCount(m_tableId);
        }
        
        // 重置模型并加载第一页数据
        beginResetModel();
        
        // 获取第一页数据
        if (m_useNewDataHandler)
        {
            m_pageData = m_robustDataHandler->getPageData(m_tableId, 0, m_pageSize);
        }
        else
        {
            m_pageData = VectorDataHandler::instance().getPageData(m_tableId, 0, m_pageSize);
        }
        
        endResetModel();
        
        qDebug() << "VectorTableModel::insertRows - 成功重新加载首页数据，行数:" << m_pageData.size()
                 << "，总行数:" << m_totalRows;
    }
    
    return success;
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

// 获取当前加载的表ID
int VectorTableModel::getCurrentTableId() const
{
    return m_tableId;
}

// 实现无限滚动功能：追加下一页数据
bool VectorTableModel::appendPage()
{
    const QString funcName = "VectorTableModel::appendPage";
    int oldPage = m_currentPage;
    qDebug() << funcName << " - 正在追加下一页数据，当前表ID:" << m_tableId << "当前页:" << oldPage;

    // 检查参数有效性
    if (m_tableId <= 0)
    {
        qWarning() << funcName << " - 无效的表ID";
        return false;
    }

    // 计算下一页页码
    int nextPage = oldPage + 1;
    qDebug() << funcName << " - 计算下一页页码:" << nextPage;

    // 计算当前已加载的数据行数
    int loadedRows = m_pageData.size();
    int expectedRows = (oldPage + 1) * m_pageSize; // 当前应该已加载的行数

    // 检查是否已经加载了所有数据
    if (loadedRows >= m_totalRows)
    {
        qDebug() << funcName << " - 已加载全部数据，无需追加 (已加载:" << loadedRows 
                 << "行，总行数:" << m_totalRows << ")";
        return false;
    }

    // 检查当前加载的行数是否与预期一致，如果不一致可能是页码计算有误
    if (loadedRows < expectedRows - m_pageSize || loadedRows > expectedRows)
    {
        qWarning() << funcName << " - 当前加载的行数与预期不符，可能存在页码计算错误"
                   << " (已加载:" << loadedRows << "行，预期:" << expectedRows << "行)";
        // 不要中断，继续尝试加载
    }

    // 获取下一页数据
    QList<Vector::RowData> nextPageData;
    if (m_useNewDataHandler)
    {
        nextPageData = m_robustDataHandler->getPageData(m_tableId, nextPage, m_pageSize);
    }
    else
    {
        nextPageData = VectorDataHandler::instance().getPageData(m_tableId, nextPage, m_pageSize);
    }

    // 检查是否成功获取到数据
    if (nextPageData.isEmpty())
    {
        qDebug() << funcName << " - 下一页没有数据，无需追加 (页码:" << nextPage << ")";
        return false;
    }

    // 计算实际添加的行数
    int startRow = m_pageData.size();
    int addedRows = nextPageData.size();

    // 告诉视图我们要开始插入行
    beginInsertRows(QModelIndex(), startRow, startRow + addedRows - 1);

    // 追加数据到当前数据集
    m_pageData.append(nextPageData);

    // 更新当前页码 - 确保在这里更新
    m_currentPage = nextPage;

    // 告诉视图插入完成
    endInsertRows();

    qDebug() << funcName << " - 成功追加" << addedRows << "行数据，当前总行数:" << m_pageData.size()
             << "，总数据行数:" << m_totalRows << "，页码从" << oldPage << "更新为" << m_currentPage;

    return true;
}