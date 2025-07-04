// 更新分页信息显示
void MainWindow::updatePaginationInfo()
{
    const QString funcName = "MainWindow::updatePaginationInfo";
    qDebug() << funcName << " - 更新分页信息，当前页:" << m_currentPage << "，总页数:" << m_totalPages << "，总行数:" << m_totalRows;

    // 检查是否使用新视图 (QTableView)
    bool isUsingNewView = (m_vectorStackedWidget && m_vectorStackedWidget->currentWidget() == m_vectorTableView);

    // 在新轨道下隐藏分页控件（新视图不需要手动分页功能）
    if (isUsingNewView && m_useNewDataHandler)
    {
        // 确保所有分页相关控件都被隐藏
        m_paginationWidget->setVisible(false);
        m_prevPageButton->setVisible(false);
        m_nextPageButton->setVisible(false);
        m_pageInfoLabel->setVisible(false);
        m_pageSizeSelector->setVisible(false);
        m_pageJumper->setVisible(false);
        m_jumpButton->setVisible(false);

        qDebug() << funcName << " - 新轨道模式，隐藏所有分页控件";
        return;
    }
    else
    {
        // 旧轨道模式，显示分页控件
        m_paginationWidget->setVisible(true);
        m_prevPageButton->setVisible(true);
        m_nextPageButton->setVisible(true);
        m_pageInfoLabel->setVisible(true);
        m_pageSizeSelector->setVisible(true);
        m_pageJumper->setVisible(true);
        m_jumpButton->setVisible(true);
    }

    // 更新页码信息标签
    m_pageInfoLabel->setText(tr("第 %1/%2 页，共 %3 行").arg(m_currentPage + 1).arg(m_totalPages).arg(m_totalRows));

    // 更新上一页按钮状态
    m_prevPageButton->setEnabled(m_currentPage > 0);

    // 更新下一页按钮状态
    m_nextPageButton->setEnabled(m_currentPage < m_totalPages - 1 && m_totalPages > 0);

    // 更新页码跳转输入框
    m_pageJumper->setMaximum(m_totalPages > 0 ? m_totalPages : 1);
    m_pageJumper->setValue(m_currentPage + 1);

    // 根据总页数启用或禁用跳转按钮
    m_jumpButton->setEnabled(m_totalPages > 1);
}

// 加载当前页数据
void MainWindow::loadCurrentPage()
{
    const QString funcName = "MainWindow::loadCurrentPage";
    qDebug() << funcName << " - 加载当前页数据，页码:" << m_currentPage;

    // 获取当前选中的向量表ID
    int tabIndex = m_vectorTabWidget->currentIndex();
    if (tabIndex < 0 || !m_tabToTableId.contains(tabIndex))
    {
        qWarning() << funcName << " - 没有选中有效的向量表";
        return;
    }

    int tableId = m_tabToTableId[tabIndex];
    qDebug() << funcName << " - 当前向量表ID:" << tableId;

    // 获取向量表总行数
    if (m_useNewDataHandler)
    {
        m_totalRows = m_robustDataHandler->getVectorTableRowCount(tableId);
    }
    else
    {
        m_totalRows = VectorDataHandler::instance().getVectorTableRowCount(tableId);
    }

    // 计算总页数
    m_totalPages = (m_totalRows + m_pageSize - 1) / m_pageSize; // 向上取整

    // 确保当前页码在有效范围内
    if (m_currentPage < 0)
        m_currentPage = 0;
    if (m_currentPage >= m_totalPages && m_totalPages > 0)
        m_currentPage = m_totalPages - 1;

    qDebug() << funcName << " - 总行数:" << m_totalRows << "，总页数:" << m_totalPages << "，当前页:" << m_currentPage;

    // 检查是否使用新视图 (QTableView)
    bool isUsingNewView = (m_vectorStackedWidget && m_vectorStackedWidget->currentWidget() == m_vectorTableView);

    // 根据当前使用的视图类型选择不同的加载方法
    bool success = false;
    if (isUsingNewView)
    {
        // 使用Model/View架构加载数据
        qDebug() << funcName << " - 使用Model/View架构加载数据，表ID:" << tableId;
        if (m_vectorTableModel)
        {
            if (m_useNewDataHandler)
            {
                // 新轨道模式：一次性加载所有数据，忽略分页
                qDebug() << funcName << " - 新轨道模式：一次性加载所有数据";
                m_vectorTableModel->loadAllData(tableId);
            }
            else
            {
                // 旧数据处理器模式：仍然使用分页
                qDebug() << funcName << " - 旧数据处理器模式：加载页面数据";
                // 确保模型使用与MainWindow相同的页面大小
                if (m_vectorTableModel->pageSize() != m_pageSize)
                {
                    qDebug() << funcName << " - 更新模型的页面大小从" << m_vectorTableModel->pageSize() << "到" << m_pageSize;
                    // 使用新添加的setPageSize方法
                    m_vectorTableModel->setPageSize(m_pageSize);
                }
                m_vectorTableModel->loadPage(tableId, m_currentPage);
            }
            success = true; // 假设loadPage总是成功
            qDebug() << funcName << " - 新表格模型数据加载完成";
        }
        else
        {
            qWarning() << funcName << " - 表格模型未初始化，无法加载数据";
            success = false;
        }
    }
    else
    {
        // 使用旧的QTableWidget方式加载数据
        qDebug() << funcName << " - 使用QTableWidget加载数据，表ID:" << tableId << "，页码:" << m_currentPage;
        if (m_useNewDataHandler)
        {
            success = m_robustDataHandler->loadVectorTablePageData(tableId, m_vectorTableWidget, m_currentPage, m_pageSize);
        }
        else
        {
            success = VectorDataHandler::instance().loadVectorTablePageData(tableId, m_vectorTableWidget, m_currentPage, m_pageSize);
        }
    }

    if (!success)
    {
        qWarning() << funcName << " - 加载页面数据失败";
    }

    // 更新分页信息显示
    updatePaginationInfo();
}

// 加载下一页
void MainWindow::loadNextPage()
{
    const QString funcName = "MainWindow::loadNextPage";
    qDebug() << funcName << " - 加载下一页";

    if (m_currentPage < m_totalPages - 1)
    {
        // 在切换页面前自动保存当前页面的修改
        saveCurrentTableData();

        m_currentPage++;
        loadCurrentPage();
    }
    else
    {
        qWarning() << funcName << " - 已经是最后一页";
    }
}

// 加载上一页
void MainWindow::loadPrevPage()
{
    const QString funcName = "MainWindow::loadPrevPage";
    qDebug() << funcName << " - 加载上一页";

    if (m_currentPage > 0)
    {
        // 在切换页面前自动保存当前页面的修改
        saveCurrentTableData();

        m_currentPage--;
        loadCurrentPage();
    }
    else
    {
        qWarning() << funcName << " - 已经是第一页";
    }
}

// 修改每页行数
void MainWindow::changePageSize(int newSize)
{
    const QString funcName = "MainWindow::changePageSize";
    qDebug() << funcName << " - 修改每页行数为:" << newSize;

    if (newSize <= 0)
    {
        qWarning() << funcName << " - 无效的页面大小:" << newSize;
        return;
    }

    // 在改变页面大小前自动保存当前页面的修改
    saveCurrentTableData();

    // 保存当前页的第一行在整个数据集中的索引
    int currentFirstRow = m_currentPage * m_pageSize;

    // 更新页面大小
    m_pageSize = newSize;

    // 计算新的页码
    m_currentPage = currentFirstRow / m_pageSize;

    // 获取当前选中的向量表ID
    int tabIndex = m_vectorTabWidget->currentIndex();
    if (tabIndex < 0 || !m_tabToTableId.contains(tabIndex))
    {
        qWarning() << funcName << " - 没有选中有效的向量表";
        return;
    }

    int tableId = m_tabToTableId[tabIndex];

    // 重新计算总页数
    if (m_useNewDataHandler)
    {
        m_totalRows = m_robustDataHandler->getVectorTableRowCount(tableId);
    }
    else
    {
        m_totalRows = VectorDataHandler::instance().getVectorTableRowCount(tableId);
    }
    m_totalPages = (m_totalRows + m_pageSize - 1) / m_pageSize; // 向上取整

    // 确保当前页码在有效范围内
    if (m_currentPage < 0)
        m_currentPage = 0;
    if (m_currentPage >= m_totalPages && m_totalPages > 0)
        m_currentPage = m_totalPages - 1;

    qDebug() << funcName << " - 更新后总行数:" << m_totalRows << "，总页数:" << m_totalPages << "，当前页:" << m_currentPage;

    // 重新加载当前页
    loadCurrentPage();
}

void MainWindow::saveCurrentTableData()
{
    const QString funcName = "MainWindow::saveCurrentTableData";
    qDebug() << funcName << " - 开始保存当前页面数据";

    // 获取当前选择的向量表
    if (m_vectorTableSelector->currentIndex() < 0)
    {
        qDebug() << funcName << " - 无当前表，不进行保存";
        return;
    }

    // 获取表ID
    int tableId = m_vectorTableSelector->currentData().toInt();

    // 保存结果变量
    QString errorMessage;
    bool saveSuccess = false;

    // 根据当前使用的视图类型选择不同的保存方法
    if (m_vectorStackedWidget->currentWidget() == m_vectorTableView)
    {
        // 使用Model/View架构保存数据
        if (m_vectorTableModel)
        {
            saveSuccess = m_vectorTableModel->saveData(errorMessage);
        }
        else
        {
            qWarning() << funcName << " - 数据模型未初始化，无法保存";
            return;
        }
    }
    else
    {
        // 使用旧的QTableWidget方式保存数据
        if (!m_vectorTableWidget)
        {
            qDebug() << funcName << " - 表格控件未初始化，不进行保存";
            return;
        }

        // 使用分页保存模式
        if (m_useNewDataHandler)
        {
            saveSuccess = m_robustDataHandler->saveVectorTableDataPaged(
                tableId,
                m_vectorTableWidget,
                m_currentPage,
                m_pageSize,
                m_totalRows,
                errorMessage);
        }
        else
        {
            saveSuccess = VectorDataHandler::instance().saveVectorTableDataPaged(
                tableId,
                m_vectorTableWidget,
                m_currentPage,
                m_pageSize,
                m_totalRows,
                errorMessage);
        }
    }

    if (!saveSuccess)
    {
        qWarning() << funcName << " - 保存失败: " << errorMessage;
        // 这里不弹出错误消息框，因为这是自动保存，不应打断用户操作
    }
    else
    {
        qDebug() << funcName << " - 保存成功";
    }
}
