// 跳转到指定页
void MainWindow::jumpToPage(int pageNum)
{
    const QString funcName = "MainWindow::jumpToPage";
    qDebug() << funcName << " - 跳转到页码:" << pageNum;

    if (pageNum < 0 || pageNum >= m_totalPages)
    {
        qWarning() << funcName << " - 无效的页码:" << pageNum;
        return;
    }

    // 在切换页面前自动保存当前页面的修改
    saveCurrentTableData();

    m_currentPage = pageNum;
    loadCurrentPage();
}

// 跳转到指定行
void MainWindow::gotoLine()
{
    const QString funcName = "MainWindow::gotoLine";
    qDebug() << funcName << " - 开始跳转到指定行";

    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        QMessageBox::warning(this, "警告", "请先打开或创建一个项目数据库");
        return;
    }

    // 检查是否有选中的向量表
    if (m_vectorTableSelector->count() == 0 || m_vectorTableSelector->currentIndex() < 0)
    {
        QMessageBox::warning(this, "警告", "请先选择一个向量表");
        return;
    }

    // 获取当前表ID
    int tableId = m_vectorTableSelector->currentData().toInt();

    // 获取表的总行数
    int totalRowCount;
    if (m_useNewDataHandler)
    {
        totalRowCount = m_robustDataHandler->getVectorTableRowCount(tableId);
    }
    else
    {
        totalRowCount = VectorDataHandler::instance().getVectorTableRowCount(tableId);
    }

    if (totalRowCount <= 0)
    {
        QMessageBox::warning(this, "警告", "当前向量表没有数据");
        return;
    }

    qDebug() << funcName << " - 当前表格总共有" << totalRowCount << "行数据";

    // 输入对话框获取行号
    bool ok;
    int targetLine = QInputDialog::getInt(this, "跳转到行",
                                          "请输入行号 (1-" + QString::number(totalRowCount) + "):",
                                          1, 1, totalRowCount, 1, &ok);

    if (!ok)
    {
        qDebug() << funcName << " - 用户取消了跳转";
        return;
    }

    qDebug() << funcName << " - 用户输入的行号:" << targetLine;

    // 计算目标行所在的页码
    int targetPage = (targetLine - 1) / m_pageSize;
    int rowInPage = (targetLine - 1) % m_pageSize;

    qDebug() << funcName << " - 目标行" << targetLine << "在第" << (targetPage + 1) << "页，页内行号:" << (rowInPage + 1);

    // 如果不是当前页，需要切换页面
    if (targetPage != m_currentPage)
    {
        m_currentPage = targetPage;
        loadCurrentPage();
    }

    // 在页面中选中行（使用页内索引）
    if (rowInPage >= 0 && rowInPage < m_vectorTableWidget->rowCount())
    {
        // 清除当前选择
        m_vectorTableWidget->clearSelection();

        // 选中目标行
        m_vectorTableWidget->selectRow(rowInPage);

        // 滚动到目标行
        m_vectorTableWidget->scrollTo(m_vectorTableWidget->model()->index(rowInPage, 0), QAbstractItemView::PositionAtCenter);

        qDebug() << funcName << " - 已跳转到第" << targetLine << "行 (第" << (targetPage + 1) << "页，页内行:" << (rowInPage + 1) << ")";
        statusBar()->showMessage(tr("已跳转到第 %1 行（第 %2 页）").arg(targetLine).arg(targetPage + 1));
    }
    else
    {
        QMessageBox::warning(this, "错误", "无效的行号");
        qDebug() << funcName << " - 无效的行号:" << targetLine << ", 页内索引:" << rowInPage;
    }
}

// 关闭Tab页签
void MainWindow::closeTab(int index)
{
    if (index < 0 || index >= m_vectorTabWidget->count())
        return;

    qDebug() << "MainWindow::closeTab - 关闭Tab页签，索引:" << index;

    // 仅当有多个Tab页时才允许关闭
    if (m_vectorTabWidget->count() > 1)
    {
        int tableId = m_tabToTableId.value(index, -1);
        m_vectorTabWidget->removeTab(index);

        // 更新映射关系
        m_tabToTableId.remove(index);

        // 更新其他Tab的映射关系
        QMap<int, int> updatedMap;
        for (auto it = m_tabToTableId.begin(); it != m_tabToTableId.end(); ++it)
        {
            int oldIndex = it.key();
            int newIndex = oldIndex > index ? oldIndex - 1 : oldIndex;
            updatedMap[newIndex] = it.value();
        }
        m_tabToTableId = updatedMap;

        qDebug() << "MainWindow::closeTab - Tab页签已关闭，剩余Tab页数:" << m_vectorTabWidget->count();
    }
    else
    {
        qDebug() << "MainWindow::closeTab - 无法关闭，这是最后一个Tab页";
        QMessageBox::information(this, "提示", "至少需要保留一个Tab页签");
    }
}

// 实现跳转到波形图指定点的函数
void MainWindow::jumpToWaveformPoint(int rowIndex, const QString &pinName)
{
    // 确保波形图是可见的
    if (!m_isWaveformVisible)
    {
        toggleWaveformView(true);
    }

    // 判断当前使用的视图类型
    bool isUsingNewView = (m_vectorStackedWidget && m_vectorStackedWidget->currentIndex() == 1);
    int selectedPinIndex = -1;
    QString simplePinName = pinName.split('\n').first(); // 处理可能的多行标题

    // 1. 收集所有管脚列信息 (无论是"全部"模式还是单管脚模式都需要用到)
    QList<QPair<QString, int>> allPinColumns;
    if (isUsingNewView && m_vectorTableModel)
    {
        // 从模型获取管脚列信息
        for (int col = 0; col < m_vectorTableModel->columnCount(); ++col)
        {
            QVariant headerData = m_vectorTableModel->headerData(col, Qt::Horizontal, Qt::DisplayRole);
            if (headerData.isValid())
            {
                int currentTableId = m_vectorTableSelector->currentData().toInt();
                QList<Vector::ColumnInfo> columns = getCurrentColumnConfiguration(currentTableId);
                if (col < columns.size() && columns[col].type == Vector::ColumnDataType::PIN_STATE_ID)
                {
                    QString colName = headerData.toString().split('\n').first();
                    allPinColumns.append(qMakePair(colName, col));
                }
            }
        }
    }
    else if (m_vectorTableWidget)
    {
        // 从旧视图获取管脚列信息
        for (int col = 0; col < m_vectorTableWidget->columnCount(); ++col)
        {
            QTableWidgetItem *headerItem = m_vectorTableWidget->horizontalHeaderItem(col);
            if (headerItem)
            {
                int currentTableId = m_vectorTableSelector->currentData().toInt();
                QList<Vector::ColumnInfo> columns = getCurrentColumnConfiguration(currentTableId);
                if (col < columns.size() && columns[col].type == Vector::ColumnDataType::PIN_STATE_ID)
                {
                    QString colName = headerItem->text().split('\n').first();
                    allPinColumns.append(qMakePair(colName, col));
                }
            }
        }
    }

    // 2. 处理两种模式：单管脚模式和全部管脚模式
    if (m_showAllPins)
    {
        // "全部"模式 - 在所有管脚中查找匹配的管脚名称
        for (int i = 0; i < allPinColumns.size(); ++i)
        {
            if (allPinColumns[i].first == simplePinName)
            {
                selectedPinIndex = i;
                break;
            }
        }
    }
    else
    {
        // 单管脚模式 - 选择正确的管脚并更新波形图
        for (int i = 0; i < m_waveformPinSelector->count(); i++)
        {
            QString itemText = m_waveformPinSelector->itemText(i);
            QString itemData = m_waveformPinSelector->itemData(i).toString().split('\n').first();

            if (itemText == simplePinName || itemData == simplePinName)
            {
                if (m_waveformPinSelector->currentIndex() != i)
                {
                    m_waveformPinSelector->setCurrentIndex(i);
                    // onWaveformPinSelectionChanged 会自动更新波形图
                }
                else
                {
                    // 如果已经是当前选中的管脚，手动更新波形图
                    updateWaveformView();
                }
                selectedPinIndex = 0; // 在单管脚模式下，选中的管脚索引始终为0
                break;
            }
        }
    }

    // 如果没有找到匹配的管脚，不进行高亮
    if (selectedPinIndex < 0)
    {
        return;
    }

    // 3. 确保rowIndex在可见范围内并高亮显示
    if (m_waveformPlot)
    {
        // 不再使用T1R偏移调整行索引，确保高亮区域从整数位置开始
        QString selectedPinName;
        if (m_showAllPins && selectedPinIndex < allPinColumns.size())
        {
            selectedPinName = allPinColumns[selectedPinIndex].first;
        }
        else
        {
            selectedPinName = m_waveformPinSelector->currentText();
        }

        // 调整视图范围确保点可见
        double currentMin = m_waveformPlot->xAxis->range().lower;
        double currentMax = m_waveformPlot->xAxis->range().upper;
        double rangeSize = currentMax - currentMin;

        if (rowIndex < currentMin || rowIndex > currentMax)
        {
            // 计算新的范围，使行索引在中间
            double newMin = qMax(0.0, rowIndex - rangeSize / 2);
            double newMax = newMin + rangeSize;

            // 确保不超过数据范围
            int totalRows;
            if (m_useNewDataHandler)
            {
                totalRows = m_robustDataHandler->getVectorTableRowCount(
                    m_vectorTableSelector->currentData().toInt());
            }
            else
            {
                totalRows = VectorDataHandler::instance().getVectorTableRowCount(
                    m_vectorTableSelector->currentData().toInt());
            }

            if (newMax > totalRows)
            {
                newMax = totalRows;
                newMin = qMax(0.0, newMax - rangeSize);
            }

            // 确保最小值始终为0（不显示负坐标）
            newMin = qMax(0.0, newMin);

            m_waveformPlot->xAxis->setRange(newMin, newMax);
        }

        // 高亮显示选中的点
        highlightWaveformPoint(rowIndex, selectedPinIndex);
    }
}

// 实时验证16进制输入
void MainWindow::validateHexInput(const QString &text)
{
    // 如果为空则重置状态
    if (text.isEmpty())
    {
        m_pinValueField->setStyleSheet("");
        m_pinValueField->setToolTip("");
        m_pinValueField->setProperty("invalid", false);
        return;
    }

    // 获取当前选中的行数
    int selectedRowCount = m_currentSelectedRows.size();
    if (selectedRowCount == 0)
    {
        // 如果没有选中行，尝试从当前选择获取
        QList<QTableWidgetItem *> selectedItems = m_vectorTableWidget->selectedItems();
        if (!selectedItems.isEmpty())
        {
            QSet<int> rowSet;
            int firstColumn = selectedItems.first()->column();
            bool sameColumn = true;

            for (QTableWidgetItem *item : selectedItems)
            {
                if (item->column() != firstColumn)
                {
                    sameColumn = false;
                    break;
                }
                rowSet.insert(item->row());
            }

            if (sameColumn)
            {
                selectedRowCount = rowSet.size();
            }
        }
    }

    // 没有选中任何行，不进行验证
    if (selectedRowCount == 0)
    {
        m_pinValueField->setStyleSheet("");
        m_pinValueField->setToolTip("");
        m_pinValueField->setProperty("invalid", false);
        return;
    }

    // 如果文本长度超过6，并且不是由calculateAndDisplayHexValue函数设置的，
    // 则可能是用户正在手动输入过长的内容
    if (text.length() > 6)
    {
        // 检查是否像是用户输入的16进制值（以0x或+0x开头）
        if (text.startsWith("0x", Qt::CaseInsensitive) || text.startsWith("+0x", Qt::CaseInsensitive))
        {
            m_pinValueField->setStyleSheet("border: 2px solid red");
            m_pinValueField->setToolTip(tr("输入错误：16进制值前缀后最多只能有2位数字 (0-9, A-F)"));
            m_pinValueField->setProperty("invalid", true);
            return;
        }
        // 如果不是典型的16进制输入格式，可能是显示多个单元格的内容，不进行验证
        // 此时将验证状态设为有效
        m_pinValueField->setStyleSheet("");
        m_pinValueField->setToolTip("");
        m_pinValueField->setProperty("invalid", false);
        return;
    }

    // --- 验证逻辑简化 ---

    // 解析输入值
    QString hexDigits;
    bool validFormat = false;

    // 统一将输入转为小写以便处理
    QString lowerText = text.toLower();

    if (lowerText.startsWith("+0x"))
    {
        hexDigits = lowerText.mid(3);
        validFormat = true;
    }
    else if (lowerText.startsWith("0x"))
    {
        hexDigits = lowerText.mid(2);
        validFormat = true;
    }

    if (!validFormat)
    {
        m_pinValueField->setStyleSheet("");
        m_pinValueField->setToolTip("");
        m_pinValueField->setProperty("invalid", false);
        return;
    }

    QRegExp hexRegex("^[0-9a-f]{1,2}$");
    if (!hexRegex.exactMatch(hexDigits))
    {
        m_pinValueField->setStyleSheet("border: 2px solid red");
        m_pinValueField->setToolTip(tr("输入错误：16进制值必须是1-2位的有效16进制数字 (0-9, A-F)"));
        m_pinValueField->setProperty("invalid", true);
        return;
    }

    // 所有验证通过
    m_pinValueField->setStyleSheet("");
    m_pinValueField->setToolTip("");
    m_pinValueField->setProperty("invalid", false);
}

#include "mainwindow_actions_utils_1.cpp"