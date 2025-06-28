void MainWindow::onVectorTableSelectionChanged(int index)
{
    if (index < 0 || m_isUpdatingUI)
        return;

    // 设置标志防止循环更新
    m_isUpdatingUI = true;

    const QString funcName = "MainWindow::onVectorTableSelectionChanged";
    qDebug() << funcName << " - 向量表选择已更改，索引:" << index;

    // 获取当前选中表ID
    int tableId = m_vectorTableSelector->currentData().toInt();
    qDebug() << funcName << " - 当前表ID:" << tableId;

    // 清空波形图管脚选择器，以便在加载表格后重新填充
    if (m_waveformPinSelector)
    {
        m_waveformPinSelector->clear();
    }

    // 刷新代理的表ID缓存
    if (m_itemDelegate)
    {
        qDebug() << funcName << " - 刷新代理表ID缓存";
        m_itemDelegate->refreshTableIdCache();
    }

    // 同步Tab页签选择
    syncTabWithComboBox(index);

    // 尝试修复当前表（如果需要）
    bool needsFix = false;
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT COUNT(*) FROM VectorTableColumnConfiguration WHERE master_record_id = ?");
    checkQuery.addBindValue(tableId);
    if (checkQuery.exec() && checkQuery.next())
    {
        int columnCount = checkQuery.value(0).toInt();
        qDebug() << funcName << " - 表 " << tableId << " 当前有 " << columnCount << " 个列配置";
        needsFix = (columnCount == 0);
    }

    if (needsFix)
    {
        qDebug() << funcName << " - 表 " << tableId << " 需要修复列配置";
        fixExistingTableWithoutColumns(tableId);
    }

    // 重置分页状态
    m_currentPage = 0;

    // 获取总行数并更新页面信息
    if (m_useNewDataHandler)
    {
        m_totalRows = m_robustDataHandler->getVectorTableRowCount(tableId);
    }
    else
    {
        m_totalRows = VectorDataHandler::instance().getVectorTableRowCount(tableId);
    }
    m_totalPages = (m_totalRows + m_pageSize - 1) / m_pageSize; // 向上取整

    // 更新分页信息显示
    updatePaginationInfo();

    // 根据当前使用的视图类型选择不同的加载方法
    if (m_vectorStackedWidget->currentWidget() == m_vectorTableView)
    {
        // 使用Model/View架构加载数据
        qDebug() << funcName << " - 使用Model/View架构加载数据，表ID:" << tableId << "，页码:" << m_currentPage;
        if (m_vectorTableModel)
        {
            m_vectorTableModel->loadPage(tableId, m_currentPage);
            qDebug() << funcName << " - 新表格模型数据加载完成";
            statusBar()->showMessage(QString("已加载向量表: %1").arg(m_vectorTableSelector->currentText()));
        }
        else
        {
            qWarning() << funcName << " - 表格模型未初始化，无法加载数据";
            statusBar()->showMessage("加载向量表失败：表格模型未初始化");
        }
    }
    else
    {
        // 使用旧的QTableWidget方式加载数据
        qDebug() << funcName << " - 开始加载表格数据，表ID:" << tableId << "，使用分页加载，页码:" << m_currentPage << "，每页行数:" << m_pageSize;
        bool loadResult;
        if (m_useNewDataHandler)
        {
            loadResult = m_robustDataHandler->loadVectorTablePageData(tableId, m_vectorTableWidget, m_currentPage, m_pageSize);
        }
        else
        {
            loadResult = VectorDataHandler::instance().loadVectorTablePageData(tableId, m_vectorTableWidget, m_currentPage, m_pageSize);
        }
        qDebug() << funcName << " - VectorDataHandler::loadVectorTablePageData 返回:" << loadResult
                 << "，表ID:" << tableId
                 << "，列数:" << m_vectorTableWidget->columnCount();

        if (loadResult)
        {
            qDebug() << funcName << " - 表格加载成功，列数:" << m_vectorTableWidget->columnCount();

            // 更新波形图视图
            if (m_isWaveformVisible && m_waveformPlot)
            {
                updateWaveformView();
            }

            // 如果列数太少（只有管脚列，没有标准列），可能需要重新加载
            if (m_vectorTableWidget->columnCount() < 6)
            {
                qWarning() << funcName << " - 警告：列数太少（" << m_vectorTableWidget->columnCount()
                           << "），可能缺少标准列。尝试修复...";
                fixExistingTableWithoutColumns(tableId);
                // 重新加载表格（使用分页）
                if (m_useNewDataHandler)
                {
                    loadResult = m_robustDataHandler->loadVectorTablePageData(tableId, m_vectorTableWidget, m_currentPage, m_pageSize);
                }
                else
                {
                    loadResult = VectorDataHandler::instance().loadVectorTablePageData(tableId, m_vectorTableWidget, m_currentPage, m_pageSize);
                }
                qDebug() << funcName << " - 修复后重新加载，结果:" << loadResult
                         << "，列数:" << m_vectorTableWidget->columnCount();
            }

            // 应用表格样式（优化版本，一次性完成所有样式设置，包括列宽和对齐）
            TableStyleManager::applyBatchTableStyle(m_vectorTableWidget);

            // 输出每一列的标题，用于调试
            QStringList columnHeaders;
            for (int i = 0; i < m_vectorTableWidget->columnCount(); i++)
            {
                QTableWidgetItem *headerItem = m_vectorTableWidget->horizontalHeaderItem(i);
                QString headerText = headerItem ? headerItem->text() : QString("列%1").arg(i);
                columnHeaders << headerText;
            }
            qDebug() << funcName << " - 表头列表:" << columnHeaders.join(", ");

            statusBar()->showMessage(QString("已加载向量表: %1，列数: %2").arg(m_vectorTableSelector->currentText()).arg(m_vectorTableWidget->columnCount()));

            // 同时加载新模型数据，以保持两种视图的数据同步
            qDebug() << funcName << " - 同步加载新表格模型数据，表ID:" << tableId;
            m_vectorTableModel->loadPage(tableId, m_currentPage);
        }
        else
        {
            qWarning() << funcName << " - 表格加载失败，表ID:" << tableId;
            statusBar()->showMessage("加载向量表失败");
        }
    }

    // 重置标志
    m_isUpdatingUI = false;
}

void MainWindow::syncTabWithComboBox(int comboBoxIndex)
{
    if (comboBoxIndex < 0 || comboBoxIndex >= m_vectorTableSelector->count())
        return;

    qDebug() << "MainWindow::syncTabWithComboBox - 同步Tab页签与下拉框选择";

    // 获取当前选择的表ID
    int tableId = m_vectorTableSelector->itemData(comboBoxIndex).toInt();

    // 在Map中查找对应的Tab索引
    int tabIndex = -1;
    for (auto it = m_tabToTableId.begin(); it != m_tabToTableId.end(); ++it)
    {
        if (it.value() == tableId)
        {
            tabIndex = it.key();
            break;
        }
    }

    // 如果找到对应的Tab，选中它
    if (tabIndex >= 0 && tabIndex < m_vectorTabWidget->count())
    {
        m_vectorTabWidget->setCurrentIndex(tabIndex);
    }
}

void MainWindow::onTabChanged(int index)
{
    if (m_isUpdatingUI || index < 0)
        return;

    m_isUpdatingUI = true;
    syncComboBoxWithTab(index);
    m_isUpdatingUI = false;
}

void MainWindow::syncComboBoxWithTab(int tabIndex)
{
    if (tabIndex < 0 || tabIndex >= m_vectorTabWidget->count())
        return;

    int tableId = m_tabToTableId.value(tabIndex, -1);
    if (tableId < 0)
        return;

    // 找到对应的下拉框索引
    for (int i = 0; i < m_vectorTableSelector->count(); i++)
    {
        if (m_vectorTableSelector->itemData(i).toInt() == tableId)
        {
            m_vectorTableSelector->setCurrentIndex(i);

            // 刷新代理的表ID缓存
            if (m_itemDelegate)
            {
                qDebug() << "MainWindow::syncComboBoxWithTab - 刷新代理表ID缓存";
                m_itemDelegate->refreshTableIdCache();
            }

            // 检查当前是否显示向量表界面，如果不是则切换
            if (m_welcomeWidget->isVisible())
            {
                m_welcomeWidget->setVisible(false);
                m_vectorTableContainer->setVisible(true);
            }

            // 重新加载数据
            if (tableId > 0)
            {
                // 重置分页状态
                m_currentPage = 0;

                // 获取总行数并更新页面信息
                if (m_useNewDataHandler)
                {
                    m_totalRows = m_robustDataHandler->getVectorTableRowCount(tableId);
                }
                else
                {
                    m_totalRows = VectorDataHandler::instance().getVectorTableRowCount(tableId);
                }
                m_totalPages = (m_totalRows + m_pageSize - 1) / m_pageSize; // 向上取整

                // 更新分页信息显示
                updatePaginationInfo();

                // 使用分页方式加载数据
                bool loadResult;
                if (m_useNewDataHandler)
                {
                    loadResult = m_robustDataHandler->loadVectorTablePageData(tableId, m_vectorTableWidget, m_currentPage, m_pageSize);
                }
                else
                {
                    loadResult = VectorDataHandler::instance().loadVectorTablePageData(tableId, m_vectorTableWidget, m_currentPage, m_pageSize);
                }

                if (loadResult)
                {
                    qDebug() << "MainWindow::syncComboBoxWithTab - 成功重新加载表格数据，页码:" << m_currentPage
                             << "，每页行数:" << m_pageSize << "，列数:" << m_vectorTableWidget->columnCount();
                    // 应用表格样式（优化版本，一次性完成所有样式设置，包括列宽和对齐）
                    TableStyleManager::applyBatchTableStyle(m_vectorTableWidget);

                    // 输出每一列的标题，用于调试
                    QStringList columnHeaders;
                    for (int i = 0; i < m_vectorTableWidget->columnCount(); i++)
                    {
                        QTableWidgetItem *headerItem = m_vectorTableWidget->horizontalHeaderItem(i);
                        QString headerText = headerItem ? headerItem->text() : QString("列%1").arg(i);
                        columnHeaders << headerText;
                    }
                    qDebug() << "MainWindow::syncComboBoxWithTab - 表头列表:" << columnHeaders.join(", ");
                }
            }

            break;
        }
    }
}

// 处理表格单元格变更
void MainWindow::onTableCellChanged(int row, int column)
{
    qDebug() << "MainWindow::onTableCellChanged - 单元格变更: 行=" << row << ", 列=" << column;

    // 获取当前表的列配置信息
    int tabIndex = m_vectorTabWidget->currentIndex();
    if (tabIndex < 0 || !m_tabToTableId.contains(tabIndex))
    {
        onTableRowModified(row);
        return;
    }

    int tableId = m_tabToTableId[tabIndex];
    QList<Vector::ColumnInfo> columns = getCurrentColumnConfiguration(tableId);

    // 检查是否是Label列发生变化
    bool isLabelColumn = false;
    int labelColumnIndex = -1;
    if (column < columns.size() && columns[column].name.toLower() == "label")
    {
        isLabelColumn = true;
        labelColumnIndex = column;
    }

    // 如果是Label列发生变化，检查是否有重复值
    if (isLabelColumn && m_vectorTableWidget)
    {
        QTableWidgetItem *currentItem = m_vectorTableWidget->item(row, column);
        if (currentItem)
        {
            QString newLabel = currentItem->text().trimmed();
            if (!newLabel.isEmpty())
            {
                // 计算当前行在整个表中的实际索引（考虑分页）
                int actualRowIndex = m_currentPage * m_pageSize + row;

                // 检查Label值是否重复
                int duplicateRow = -1;
                if (isLabelDuplicate(tableId, newLabel, actualRowIndex, duplicateRow))
                {
                    // 阻止表格信号，避免递归触发
                    m_vectorTableWidget->blockSignals(true);

                    // 将单元格值恢复为空
                    currentItem->setText("");

                    // 重新启用表格信号
                    m_vectorTableWidget->blockSignals(false);

                    // 计算重复行在哪一页以及页内索引
                    int duplicatePage = duplicateRow / m_pageSize + 1;      // 显示给用户的页码从1开始
                    int duplicateRowInPage = duplicateRow % m_pageSize + 1; // 显示给用户的行号从1开始

                    // 弹出警告对话框
                    QMessageBox::warning(this,
                                         "标签重复",
                                         QString("标签 '%1' 已经存在于第 %2 页第 %3 行，请使用不同的标签值。")
                                             .arg(newLabel)
                                             .arg(duplicatePage)
                                             .arg(duplicateRowInPage));

                    // 让单元格保持编辑状态
                    m_vectorTableWidget->editItem(currentItem);

                    // 由于恢复了值，不需要标记为已修改
                    return;
                }
            }
        }
    }

    // 标记行为已修改
    onTableRowModified(row);

    // 如果是Label列变化，刷新侧边栏导航树
    if (isLabelColumn)
    {
        qDebug() << "MainWindow::onTableCellChanged - Label列变化，刷新侧边栏";
        refreshSidebarNavigator();
    }
}

// 处理表格行修改
void MainWindow::onTableRowModified(int row)
{
    // 获取当前选中的向量表ID
    int tabIndex = m_vectorTabWidget->currentIndex();
    if (tabIndex < 0 || !m_tabToTableId.contains(tabIndex))
    {
        qWarning() << "MainWindow::onTableRowModified - 没有选中有效的向量表";
        return;
    }

    int tableId = m_tabToTableId[tabIndex];

    // 计算实际数据库中的行索引（考虑分页）
    int actualRowIndex = m_currentPage * m_pageSize + row;
    qDebug() << "MainWindow::onTableRowModified - 标记表ID:" << tableId << "的行:" << actualRowIndex << "为已修改";

    // 标记行为已修改
    if (m_useNewDataHandler)
    {
        m_robustDataHandler->markRowAsModified(tableId, actualRowIndex);
    }
    else
    {
        VectorDataHandler::instance().markRowAsModified(tableId, actualRowIndex);
    }

    // 更新修改标志和窗口标题
    m_hasUnsavedChanges = true;
    updateWindowTitle(m_currentDbPath);

    // 如果波形图可见，则更新它
    if (m_isWaveformVisible)
    {
        updateWaveformView();
        // 确保修改后高亮仍然在正确的位置
        if (m_selectedWaveformPoint >= 0)
        {
            highlightWaveformPoint(m_selectedWaveformPoint);
        }
    }
}

// 字体缩放滑块值改变响应
void MainWindow::onFontZoomSliderValueChanged(int value)
{
    qDebug() << "MainWindow::onFontZoomSliderValueChanged - 调整字体缩放值:" << value;

    // 计算缩放因子 (从50%到200%)
    double scaleFactor = value / 100.0;

    // 更新向量表字体大小
    if (m_vectorTableWidget)
    {
        QFont font = m_vectorTableWidget->font();
        int baseSize = 9; // 默认字体大小
        font.setPointSizeF(baseSize * scaleFactor);
        m_vectorTableWidget->setFont(font);

        // 更新表头字体
        QFont headerFont = m_vectorTableWidget->horizontalHeader()->font();
        headerFont.setPointSizeF(baseSize * scaleFactor);
        m_vectorTableWidget->horizontalHeader()->setFont(headerFont);
        m_vectorTableWidget->verticalHeader()->setFont(headerFont);

        // 调整行高以适应字体大小
        m_vectorTableWidget->verticalHeader()->setDefaultSectionSize(qMax(25, int(25 * scaleFactor)));

        qDebug() << "MainWindow::onFontZoomSliderValueChanged - 字体大小已调整为:" << font.pointSizeF();
    }
}

// 字体缩放重置响应
void MainWindow::onFontZoomReset()
{
    qDebug() << "MainWindow::onFontZoomReset - 重置字体缩放";

    // 重置字体大小到默认值
    if (m_vectorTableWidget)
    {
        QFont font = m_vectorTableWidget->font();
        font.setPointSizeF(9); // 恢复默认字体大小
        m_vectorTableWidget->setFont(font);

        // 重置表头字体
        QFont headerFont = m_vectorTableWidget->horizontalHeader()->font();
        headerFont.setPointSizeF(9);
        m_vectorTableWidget->horizontalHeader()->setFont(headerFont);
        m_vectorTableWidget->verticalHeader()->setFont(headerFont);

        // 重置行高
        m_vectorTableWidget->verticalHeader()->setDefaultSectionSize(25);

        qDebug() << "MainWindow::onFontZoomReset - 字体大小已重置为默认值";
    }
}

// 处理16进制值编辑后的同步操作
void MainWindow::onHexValueEdited()
{
    // 获取输入的16进制值
    QString hexValue = m_pinValueField->text().trimmed();
    if (hexValue.isEmpty())
        return;

    // 如果输入无效，则不执行任何操作
    if (m_pinValueField->property("invalid").toBool())
    {
        return;
    }

    // 使用已保存的列和行信息，如果不存在则尝试获取当前选中内容
    QList<int> selectedRows = m_currentSelectedRows;
    int selectedColumn = m_currentHexValueColumn;

    // 如果没有保存的列信息或行信息，则尝试从当前选中项获取
    if (selectedColumn < 0 || selectedRows.isEmpty())
    {
        selectedRows.clear();
        bool sameColumn = true;

        QList<QTableWidgetItem *> selectedItems = m_vectorTableWidget->selectedItems();
        if (selectedItems.isEmpty())
            return;

        selectedColumn = selectedItems.first()->column();

        // 检查是否所有选择都在同一列
        for (QTableWidgetItem *item : selectedItems)
        {
            if (item->column() != selectedColumn)
            {
                sameColumn = false;
                break;
            }

            if (!selectedRows.contains(item->row()))
                selectedRows.append(item->row());
        }

        if (!sameColumn || selectedRows.isEmpty())
            return;
    }

    // 确保行按从上到下排序
    std::sort(selectedRows.begin(), selectedRows.end());

    // 获取当前表的列配置信息
    if (m_vectorTabWidget->currentIndex() < 0)
        return;

    int currentTableId = m_tabToTableId[m_vectorTabWidget->currentIndex()];
    QList<Vector::ColumnInfo> columns = getCurrentColumnConfiguration(currentTableId);

    // 检查列索引是否有效且是否为PIN_STATE_ID列
    if (selectedColumn < 0 || selectedColumn >= columns.size())
        return;

    Vector::ColumnDataType colType = columns[selectedColumn].type;
    if (colType != Vector::ColumnDataType::PIN_STATE_ID)
        return;

    // 判断格式类型和提取16进制值
    bool useHLFormat = false;
    QString hexDigits;
    bool validFormat = false;

    // 统一将输入转为小写以便处理
    QString lowerHexValue = hexValue.toLower();

    if (lowerHexValue.startsWith("+0x"))
    {
        useHLFormat = true;
        hexDigits = lowerHexValue.mid(3);
        validFormat = true;
    }
    else if (lowerHexValue.startsWith("0x"))
    {
        useHLFormat = false;
        hexDigits = lowerHexValue.mid(2);
        validFormat = true;
    }
    else
    {
        return;
    }

    if (!validFormat)
    {
        return;
    }

    QRegExp hexRegex("^[0-9a-fA-F]{1,2}$");
    if (!hexRegex.exactMatch(hexDigits))
    {
        return;
    }

    bool ok;
    int decimalValue = hexDigits.toInt(&ok, 16);
    if (!ok)
    {
        return;
    }

    // 1. 转换为8位二进制字符串
    QString binaryStr = QString::number(decimalValue, 2).rightJustified(8, '0');

    // 2. 确定要操作的行数 (最多8行)
    int rowsToChange = qMin(selectedRows.size(), 8);

    // 3. 从8位字符串中截取右边的部分
    QString finalBinaryStr = binaryStr.right(rowsToChange);

    // 4. 将最终的二进制字符串覆写到选中的单元格
    m_vectorTableWidget->blockSignals(true);

    for (int i = 0; i < finalBinaryStr.length(); ++i)
    {
        int row = selectedRows[i];
        QChar bit = finalBinaryStr[i];
        QString newValue;
        if (useHLFormat)
        {
            newValue = (bit == '1' ? "H" : "L");
        }
        else
        {
            newValue = bit;
        }

        QTableWidgetItem *item = m_vectorTableWidget->item(row, selectedColumn);
        if (item)
        {
            item->setText(newValue);
        }
        else
        {
            item = new QTableWidgetItem(newValue);
            m_vectorTableWidget->setItem(row, selectedColumn, item);
        }
    }

    m_vectorTableWidget->blockSignals(false);

    // 手动触发一次数据变更的逻辑，以便undo/redo和保存状态能够更新
    if (!selectedRows.isEmpty())
    {
        onTableRowModified(selectedRows.first());
    }

    // --- 新增：处理回车后的跳转和选择逻辑 ---

    // 1. 确定最后一个被影响的行和总行数
    int lastAffectedRow = -1;
    if (selectedRows.size() <= 8)
    {
        lastAffectedRow = selectedRows.last();
    }
    else
    {
        lastAffectedRow = selectedRows[7]; // n > 8 时，只影响前8行
    }

    int totalRowCount = m_vectorTableWidget->rowCount();
    int nextRow = lastAffectedRow + 1;

    // 如果下一行超出表格范围，则不执行任何操作
    if (nextRow >= totalRowCount)
    {
        return;
    }

    // 2. 清除当前选择
    m_vectorTableWidget->clearSelection();

    // 3. 根据"连续"模式设置新选择
    if (m_continuousSelectCheckBox && m_continuousSelectCheckBox->isChecked())
    {
        // 连续模式开启
        int selectionStartRow = nextRow;
        int selectionEndRow = qMin(selectionStartRow + 7, totalRowCount - 1);

        // 创建选择范围
        QTableWidgetSelectionRange range(selectionStartRow, m_currentHexValueColumn,
                                         selectionEndRow, m_currentHexValueColumn);
        m_vectorTableWidget->setCurrentItem(m_vectorTableWidget->item(selectionStartRow, m_currentHexValueColumn));
        m_vectorTableWidget->setRangeSelected(range, true);

        // 确保新选区可见
        m_vectorTableWidget->scrollToItem(m_vectorTableWidget->item(selectionStartRow, m_currentHexValueColumn), QAbstractItemView::PositionAtTop);

        // 将焦点设置回输入框
        m_pinValueField->setFocus();
        m_pinValueField->selectAll();
    }
    else
    {
        // 连续模式关闭
        m_vectorTableWidget->setCurrentCell(nextRow, m_currentHexValueColumn);
    }
}

void MainWindow::on_action_triggered(bool checked)
{
    // 这个槽函数当前没有具体操作，可以根据需要进行扩展
    qDebug() << "Action triggered, checked:" << checked;
}