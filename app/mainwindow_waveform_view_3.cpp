
void MainWindow::onWaveformContextMenuRequested(const QPoint &pos)
{
    if (!m_waveformPlot)
        return;

    // 获取点击位置对应的行索引
    double key = m_waveformPlot->xAxis->pixelToCoord(pos.x());
    int rowIndex = static_cast<int>(floor(key - m_currentXOffset));
    if (rowIndex < 0)
        return;

    // 获取点击位置对应的管脚索引
    double y = m_waveformPlot->yAxis->pixelToCoord(pos.y());
    const double PIN_HEIGHT = 25.0;
    const double PIN_GAP = 10.0;
    int pinIndex = static_cast<int>(floor(y / (PIN_HEIGHT + PIN_GAP)));

    // 更新选中点并高亮显示
    highlightWaveformPoint(rowIndex, pinIndex);

    QMenu contextMenu(this);
    QAction *jumpAction = contextMenu.addAction(tr("跳转至向量表"));

    connect(jumpAction, &QAction::triggered, this, [this, rowIndex, pinIndex]()
            {
        // 判断当前使用的视图类型
        bool isUsingNewView = (m_vectorStackedWidget && m_vectorStackedWidget->currentIndex() == 1);
        
        // 获取所有显示的管脚列表
        QList<QPair<QString, int>> displayedPinColumns;
        
        // 根据当前模式获取管脚信息
        if (m_showAllPins)
        {
            // 全部管脚模式 - 获取所有管脚列
            QList<QPair<QString, int>> allPinColumns;
            
            if (isUsingNewView && m_vectorTableModel)
            {
                // 从模型获取所有管脚列
                for (int col = 0; col < m_vectorTableModel->columnCount(); ++col)
                {
                    QVariant headerData = m_vectorTableModel->headerData(col, Qt::Horizontal, Qt::DisplayRole);
                    if (headerData.isValid())
                    {
                        int currentTableId = m_vectorTableSelector->currentData().toInt();
                        QList<Vector::ColumnInfo> columns = getCurrentColumnConfiguration(currentTableId);
                        if (col < columns.size() && columns[col].type == Vector::ColumnDataType::PIN_STATE_ID)
                        {
                            allPinColumns.append(qMakePair(headerData.toString().split('\n').first(), col));
                        }
                    }
                }
            }
            else if (m_vectorTableWidget)
            {
                // 从旧视图获取所有管脚列
                for (int col = 0; col < m_vectorTableWidget->columnCount(); ++col)
                {
                    QTableWidgetItem *headerItem = m_vectorTableWidget->horizontalHeaderItem(col);
                    if (headerItem)
                    {
                        int currentTableId = m_vectorTableSelector->currentData().toInt();
                        QList<Vector::ColumnInfo> columns = getCurrentColumnConfiguration(currentTableId);
                        if (col < columns.size() && columns[col].type == Vector::ColumnDataType::PIN_STATE_ID)
                        {
                            allPinColumns.append(qMakePair(headerItem->text().split('\n').first(), col));
                        }
                    }
                }
            }
            
            displayedPinColumns = allPinColumns;
        }
        else
        {
            // 单管脚模式 - 只获取当前选中的管脚
            if (m_waveformPinSelector->count() > 0)
            {
                QString selectedPinName = m_waveformPinSelector->currentText();
                QString selectedPinFullName = m_waveformPinSelector->currentData().toString();
                
                // 查找对应的列索引
                int pinColumnIndex = -1;
                
                if (isUsingNewView && m_vectorTableModel)
                {
                    // 在新视图中查找列
                    for (int col = 0; col < m_vectorTableModel->columnCount(); ++col)
                    {
                        QVariant headerData = m_vectorTableModel->headerData(col, Qt::Horizontal, Qt::DisplayRole);
                        if (headerData.isValid())
                        {
                            QString headerText = headerData.toString();
                            if (headerText == selectedPinFullName || headerText.split('\n').first() == selectedPinName)
                            {
                                pinColumnIndex = col;
                                break;
                            }
                        }
                    }
                }
                else if (m_vectorTableWidget)
                {
                    // 在旧视图中查找列
                    for (int col = 0; col < m_vectorTableWidget->columnCount(); ++col)
                    {
                        QTableWidgetItem *headerItem = m_vectorTableWidget->horizontalHeaderItem(col);
                        if (headerItem)
                        {
                            QString headerText = headerItem->text();
                            if (headerText == selectedPinFullName || headerText.split('\n').first() == selectedPinName)
                            {
                                pinColumnIndex = col;
                                break;
                            }
                        }
                    }
                }
                
                if (pinColumnIndex >= 0)
                {
                    displayedPinColumns.append(qMakePair(selectedPinName, pinColumnIndex));
                }
            }
        }
        
        // 检查选中的管脚索引是否有效
        if (pinIndex >= 0 && pinIndex < displayedPinColumns.size())
        {
            int pinColumnIndex = displayedPinColumns[pinIndex].second;
            
            // 计算行应该在哪个页面
            int pageForRow = rowIndex / m_pageSize;
            int rowInPage = rowIndex % m_pageSize;
            
            // 如果需要，切换到正确的页面
            if (pageForRow != m_currentPage)
            {
                // 保存旧页面数据
                saveCurrentTableData();
                
                // 更新页码
                m_currentPage = pageForRow;
                
                // 更新分页信息UI
                updatePaginationInfo();
                
                // 加载新页面
                int tableId = m_vectorTableSelector->currentData().toInt();
                if (m_useNewDataHandler) {
                    m_robustDataHandler->loadVectorTablePageData(
                        tableId, isUsingNewView ? nullptr : m_vectorTableWidget, m_currentPage, m_pageSize);
                } else {
                    VectorDataHandler::instance().loadVectorTablePageData(
                        tableId, isUsingNewView ? nullptr : m_vectorTableWidget, m_currentPage, m_pageSize);
                }
            }
            
            // Jump to the cell - 使用页内行索引
            if (rowInPage >= 0)
            {
                if (isUsingNewView && m_vectorTableModel)
                {
                    // 新视图跳转
                    QModelIndex targetIndex = m_vectorTableModel->index(rowInPage, pinColumnIndex);
                    if (targetIndex.isValid())
                    {
                        m_vectorTableView->setCurrentIndex(targetIndex);
                        m_vectorTableView->scrollTo(targetIndex, QAbstractItemView::PositionAtCenter);
                        m_vectorTableView->setFocus();
                    }
                }
                else if (m_vectorTableWidget && rowInPage < m_vectorTableWidget->rowCount())
                {
                    // 旧视图跳转
                    m_vectorTableWidget->setCurrentCell(rowInPage, pinColumnIndex);
                    m_vectorTableWidget->scrollToItem(m_vectorTableWidget->item(rowInPage, pinColumnIndex), 
                                                     QAbstractItemView::PositionAtCenter);
                    m_vectorTableWidget->setFocus(); // Ensure the table gets focus
                }
            }
        } });

    contextMenu.exec(m_waveformPlot->mapToGlobal(pos));
}
