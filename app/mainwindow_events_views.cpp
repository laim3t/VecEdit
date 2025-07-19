void MainWindow::onVectorTableSelectionChanged(int index)
{
    const QString funcName = "MainWindow::onVectorTableSelectionChanged";
    qDebug() << funcName << " - 向量表选择已更改，索引:" << index;
    if (index < 0)
        return;

    // 记录向量表加载开始时间，用于监控UI响应状态
    logUIResponse("向量表切换操作");

    int tableId = m_vectorTableSelector->itemData(index).toInt();
    qDebug() << funcName << " - 当前表ID:" << tableId;

    // 【关键修复】
    // 检查模型是否已在显示目标表格的数据，但仅在以下情况下跳过加载：
    // 1. 模型不为空
    // 2. 模型的当前表ID与新表ID相同
    // 3. 模型的当前表ID不是无效值(-1)
    // 4. 模型中有数据
    if (m_vectorTableModel &&
        m_vectorTableModel->getCurrentTableId() == tableId &&
        m_vectorTableModel->getCurrentTableId() != -1 &&
        m_vectorTableModel->rowCount() > 0)
    {
        qDebug() << funcName << " - 模型已在显示表" << tableId << "的数据，跳过不必要的重复加载。";
        // 由于跳过加载，手动重置UI响应监控
        m_tableLoadStartTime = 0;
        return;
    }

    // 更新波形图管脚选择器以反映新表格的管脚配置
    updateWaveformPinSelector();

    // 更新代理的缓存
    if (m_itemDelegate)
    {
        m_itemDelegate->refreshTableIdCache();
    }

    // 同步Tab页签与下拉框选择
    syncTabWithComboBox(index);

    // 根据当前使用的视图类型选择不同的加载方法
    bool isUsingNewView = (m_vectorStackedWidget->currentWidget() == m_vectorTableView);

    if (isUsingNewView)
    {
        qDebug() << funcName << " - 使用Model/View架构加载数据";
        if (m_vectorTableModel)
        {
            // 一次性连接数据加载完成信号
            QObject::connect(m_vectorTableModel, &VectorTableModel::dataLoadCompleted, this, [this](int loadedTableId)
                             {
                                 qDebug() << "【性能监控】模型数据加载完成，表ID:" << loadedTableId;
                                 // 数据加载完成后的UI渲染可能仍需要时间，UI响应监控会继续工作直到检测到用户交互
                             },
                             Qt::UniqueConnection);

            if (m_useNewDataHandler)
            {
                m_vectorTableModel->loadAllData(tableId);
            }
            else
            {
                m_vectorTableModel->loadPage(tableId, 0); // 分页模式总是从第一页开始
            }
        }
    }
    else
    {
        // 使用旧的QTableWidget加载数据
        qDebug() << funcName << " - 使用QTableWidget加载数据";
        m_currentPage = 0; // 切换表格时总是重置到第一页
        loadCurrentPage(); // 该函数内部会处理分页和UI更新
    }

    // 根据自动更新标志决定是否更新波形图
    if (m_autoUpdateWaveform)
    {
        updateWaveformView();
    }
    else
    {
        qDebug() << funcName << " - 自动更新波形图已禁用，跳过波形图更新";
    }
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
            // 设置下拉框的当前索引。
            // 这将触发 onVectorTableSelectionChanged 信号，
            // 该信号是处理数据加载的唯一正确途径。
            m_vectorTableSelector->setCurrentIndex(i);
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

    // 根据自动更新标志决定是否更新波形图
    if (m_isWaveformVisible && m_autoUpdateWaveform)
    {
        updateWaveformView();
        // 确保修改后高亮仍然在正确的位置
        if (m_selectedWaveformPoint >= 0)
        {
            highlightWaveformPoint(m_selectedWaveformPoint);
        }
    }
    else if (m_isWaveformVisible && !m_autoUpdateWaveform)
    {
        qDebug() << "MainWindow::onTableRowModified - 自动更新波形图已禁用，跳过波形图更新";
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

// 实现事件过滤器，用于监控UI响应状态
bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    // 只在表格加载期间监控事件
    if (m_tableLoadStartTime > 0)
    {
        // 这些事件表明UI已经恢复响应
        if (event->type() == QEvent::Paint ||
            event->type() == QEvent::MouseMove ||
            event->type() == QEvent::MouseButtonPress ||
            event->type() == QEvent::KeyPress)
        {

            // 启动一个短定时器，确保UI完全响应
            if (!m_uiResponseTimer->isActive())
            {
                m_uiResponseTimer->start(100); // 100ms后确认UI已响应
            }
        }
    }

    // 继续传递事件
    return QMainWindow::eventFilter(watched, event);
}

// 记录UI响应状态的方法
void MainWindow::logUIResponse(const QString &operation)
{
    // 记录操作开始时间
    m_tableLoadStartTime = QDateTime::currentMSecsSinceEpoch();
    qDebug() << "【性能监控】" << operation << "开始时间:" << QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
}