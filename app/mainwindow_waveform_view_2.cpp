// 高亮显示波形图中的指定点
void MainWindow::highlightWaveformPoint(int rowIndex, int pinIndex)
{
    if (!m_waveformPlot || rowIndex < 0)
        return;

    // 保存选中的点
    m_selectedWaveformPoint = rowIndex;

    // 移除所有已有的选中标记和文本标签
    for (int i = m_waveformPlot->itemCount() - 1; i >= 0; i--)
    {
        if (auto item = m_waveformPlot->item(i))
        {
            if (item->property("isSelectionHighlight").toBool() || item->property("isHexValueLabel").toBool())
            {
                m_waveformPlot->removeItem(i);
            }
        }
    }

    // 获取当前显示的管脚列表，以验证pinIndex
    QList<QPair<QString, int>> displayedPinColumns;
    if (m_vectorTableWidget)
    {
        QList<QPair<QString, int>> allPinColumns;
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

        if (m_showAllPins)
        {
            displayedPinColumns = allPinColumns;
        }
        else if (m_waveformPinSelector->count() > 0)
        {
            QString selectedPinName = m_waveformPinSelector->currentText();
            for (const auto &pin : allPinColumns)
            {
                if (pin.first == selectedPinName)
                {
                    displayedPinColumns.append(pin);
                    break;
                }
            }
        }
    }

    if (pinIndex < 0 || pinIndex >= displayedPinColumns.size())
    {
        m_waveformPlot->replot();
        return; // 点击位置无效，不在任何管脚上
    }

    // [FIX] 获取当前管脚的T1R偏移量，而不是使用全局最大偏移
    QString pinName = displayedPinColumns[pinIndex].first;
    int pinId = getPinIdByName(pinName);
    double pin_t1rRatio = m_pinT1rRatios.value(pinId, 0.0);

    // 计算特定管脚的Y轴边界
    const double PIN_HEIGHT = 25.0;
    const double PIN_GAP = 10.0;
    double pinBaseY = pinIndex * (PIN_HEIGHT + PIN_GAP);
    double pinHighY = pinBaseY + PIN_HEIGHT;
    double pinLowY = pinBaseY;

    // 创建一个只高亮特定管脚的矩形
    QCPItemRect *highlightRect = new QCPItemRect(m_waveformPlot);
    highlightRect->setLayer("selection");
    highlightRect->setProperty("isSelectionHighlight", true);
    highlightRect->topLeft->setCoords(rowIndex + pin_t1rRatio, pinHighY);
    highlightRect->bottomRight->setCoords(rowIndex + pin_t1rRatio + 1.0, pinLowY);
    highlightRect->setPen(QPen(Qt::blue, 1));
    highlightRect->setBrush(QBrush(QColor(0, 0, 255, 30)));

    // 获取对应管脚的列索引
    int pinColumnIndex = displayedPinColumns[pinIndex].second;

    // 创建并显示十六进制值标签
    if (pinColumnIndex >= 0)
    {
        // 计算行应该在哪个页面
        int pageForRow = rowIndex / m_pageSize;
        int rowInPage = rowIndex % m_pageSize;

        // 如果需要，切换到正确的页面
        if (pageForRow != m_currentPage)
        {
            saveCurrentTableData();
            m_currentPage = pageForRow;
            updatePaginationInfo();
            int tableId = m_vectorTableSelector->currentData().toInt();
            VectorDataHandler::instance().loadVectorTablePageData(
                tableId, m_vectorTableWidget, m_currentPage, m_pageSize);
        }

        if (rowInPage < m_vectorTableWidget->rowCount())
        {
            QTableWidgetItem *item = m_vectorTableWidget->item(rowInPage, pinColumnIndex);
            if (item)
            {
                QString cellValue = item->text();
                bool ok;
                int intValue = cellValue.toInt(&ok);
                QString hexValue = ok ? "0x" + QString::number(intValue, 16) : cellValue;

                QCPItemText *hexLabel = new QCPItemText(m_waveformPlot);
                hexLabel->setLayer("selection");
                hexLabel->setProperty("isHexValueLabel", true);
                hexLabel->setText(hexValue);
                hexLabel->setFont(QFont("sans-serif", 10));
                hexLabel->setColor(Qt::black);
                hexLabel->setBrush(QBrush(QColor(240, 240, 240, 200)));
                hexLabel->setPen(QPen(Qt::gray));
                hexLabel->setPadding(QMargins(5, 2, 5, 2));

                // 将标签定位在管脚波形的垂直中心
                double pinMidY = (pinHighY + pinLowY) / 2.0;
                hexLabel->position->setCoords(rowIndex + pin_t1rRatio + 0.5, pinMidY);

                m_vectorTableWidget->setCurrentCell(rowInPage, pinColumnIndex);
                m_vectorTableWidget->scrollToItem(item, QAbstractItemView::PositionAtCenter);
            }
        }
    }

    // 重绘波形图
    m_waveformPlot->replot();
}

// 添加鼠标点击事件处理
void MainWindow::setupWaveformClickHandling()
{
    if (!m_waveformPlot)
        return;

    // 连接鼠标点击信号
    connect(m_waveformPlot, &QCustomPlot::mousePress, this, [this](QMouseEvent *event)
            {
        if (event->button() == Qt::LeftButton) {
            // 获取点击位置对应的数据点索引
            double key = m_waveformPlot->xAxis->pixelToCoord(event->pos().x());
            double y = m_waveformPlot->yAxis->pixelToCoord(event->pos().y());
            
            // 计算管脚索引
            const double PIN_HEIGHT = 25.0;
            const double PIN_GAP = 10.0;
            int pinIndex = static_cast<int>(floor(y / (PIN_HEIGHT + PIN_GAP)));

            // [FIX] 根据点击的管脚获取其专属的T1R偏移量
            double pin_t1rRatio = 0.0;
            QList<QPair<QString, int>> allPinColumns;
            if (m_vectorTableWidget) {
                 for (int col = 0; col < m_vectorTableWidget->columnCount(); ++col) {
                    auto headerItem = m_vectorTableWidget->horizontalHeaderItem(col);
                    if (headerItem) {
                        int currentTableId = m_vectorTableSelector->currentData().toInt();
                        auto columns = getCurrentColumnConfiguration(currentTableId);
                        if (col < columns.size() && columns[col].type == Vector::ColumnDataType::PIN_STATE_ID) {
                            allPinColumns.append(qMakePair(headerItem->text().split('\n').first(), col));
                        }
                    }
                }
            }

            if (m_showAllPins) {
                if (pinIndex >= 0 && pinIndex < allPinColumns.size()) {
                    int pinId = getPinIdByName(allPinColumns[pinIndex].first);
                    pin_t1rRatio = m_pinT1rRatios.value(pinId, 0.0);
                }
            } else if (m_waveformPinSelector->count() > 0) {
                // 单管脚模式下，使用当前选中管脚的T1R
                int pinId = getPinIdByName(m_waveformPinSelector->currentText());
                pin_t1rRatio = m_pinT1rRatios.value(pinId, 0.0);
            }

            // 在计算行索引时考虑该管脚的特定偏移量
            int rowIndex = static_cast<int>(floor(key - pin_t1rRatio));
            
            // 检查索引是否有效（只响应正坐标）
            int totalRows = VectorDataHandler::instance().getVectorTableRowCount(m_vectorTableSelector->currentData().toInt());
            if (rowIndex >= 0 && rowIndex < totalRows) {
                // 高亮显示选中的点
                highlightWaveformPoint(rowIndex, pinIndex);
            }
        } });

    // 连接鼠标双击信号
    connect(m_waveformPlot, &QCustomPlot::mouseDoubleClick, this, &MainWindow::onWaveformDoubleClicked);
}

void MainWindow::onWaveformDoubleClicked(QMouseEvent *event)
{
    if (!m_waveformPlot || !m_vectorTableWidget || !m_waveformValueEditor)
        return;

    // 1. 获取点击位置对应的行列信息
    double key = m_waveformPlot->xAxis->pixelToCoord(event->pos().x());
    double value = m_waveformPlot->yAxis->pixelToCoord(event->pos().y());

    // 判断点击了哪个管脚，根据Y坐标
    const double PIN_HEIGHT = 25.0;
    const double PIN_GAP = 10.0;
    int pinIndexByY = static_cast<int>(floor(value / (PIN_HEIGHT + PIN_GAP)));

    // 获取所有PIN_STATE_ID类型的列
    QList<QPair<QString, int>> pinColumns;
    if (m_vectorTableWidget)
    {
        for (int col = 0; col < m_vectorTableWidget->columnCount(); ++col)
        {
            QTableWidgetItem *headerItem = m_vectorTableWidget->horizontalHeaderItem(col);
            if (headerItem)
            {
                QString headerText = headerItem->text();
                int currentTableId = m_vectorTableSelector->currentData().toInt();
                QList<Vector::ColumnInfo> columns = getCurrentColumnConfiguration(currentTableId);
                if (col < columns.size() && columns[col].type == Vector::ColumnDataType::PIN_STATE_ID)
                {
                    QString displayName = headerText.split('\n').first();
                    pinColumns.append(qMakePair(displayName, col));
                }
            }
        }
    }

    // [FIX] 根据点击的管脚获取其专属的T1R偏移量
    double pin_t1rRatio = 0.0;
    if (m_showAllPins)
    {
        if (pinIndexByY >= 0 && pinIndexByY < pinColumns.size())
        {
            int pinId = getPinIdByName(pinColumns[pinIndexByY].first);
            pin_t1rRatio = m_pinT1rRatios.value(pinId, 0.0);
        }
    }
    else if (m_waveformPinSelector->count() > 0)
    {
        // 单管脚模式下，使用当前选中管脚的T1R
        int pinId = getPinIdByName(m_waveformPinSelector->currentText());
        pin_t1rRatio = m_pinT1rRatios.value(pinId, 0.0);
    }

    // 在计算行索引时考虑该管脚的特定偏移量
    int rowIndex = static_cast<int>(floor(key - pin_t1rRatio));

    int totalRows = VectorDataHandler::instance().getVectorTableRowCount(m_vectorTableSelector->currentData().toInt());
    if (rowIndex < 0 || rowIndex >= totalRows)
        return;

    int pinColumnIndex = -1;
    QString pinName;

    // 检查点击的管脚索引是否有效
    if (pinIndexByY >= 0 && pinIndexByY < pinColumns.size())
    {
        // 如果Y坐标在有效范围内，则使用点击的管脚
        pinName = pinColumns[pinIndexByY].first;
        pinColumnIndex = pinColumns[pinIndexByY].second;
        m_waveformPinSelector->setCurrentText(pinName);
    }
    else
    {
        // 否则，回退到使用当前下拉框中选择的管脚
        pinName = m_waveformPinSelector->currentText();
        QString pinFullName = m_waveformPinSelector->currentData().toString();
        for (const auto &pin : pinColumns)
        {
            if (pin.first == pinName)
            {
                pinColumnIndex = pin.second;
                break;
            }
        }
    }

    if (pinColumnIndex < 0)
        return;

    // 保存编辑上下文
    m_editingRow = rowIndex;
    m_editingPinColumn = pinColumnIndex;

    // 2. 获取当前值
    QString currentValue;
    if (rowIndex < m_vectorTableWidget->rowCount())
    {
        QTableWidgetItem *cell = m_vectorTableWidget->item(rowIndex, pinColumnIndex);
        if (cell)
        {
            currentValue = cell->text();
        }
    }

    m_waveformValueEditor->setText(currentValue);

    // 3. 定位并显示编辑器
    // 获取高亮矩形的位置来定位编辑器
    QCPItemRect *highlightRect = nullptr;
    for (int i = 0; i < m_waveformPlot->itemCount(); ++i)
    {
        if (auto item = m_waveformPlot->item(i))
        {
            if (item->property("isSelectionHighlight").toBool())
            {
                highlightRect = qobject_cast<QCPItemRect *>(item);
                break;
            }
        }
    }

    if (highlightRect)
    {
        // 将 plot 坐标转换为 widget 像素坐标
        int x = m_waveformPlot->xAxis->coordToPixel(highlightRect->topLeft->coords().x());
        int y = m_waveformPlot->yAxis->coordToPixel(highlightRect->topLeft->coords().y());
        int width = m_waveformPlot->xAxis->coordToPixel(highlightRect->bottomRight->coords().x()) - x;
        int height = m_waveformPlot->yAxis->coordToPixel(highlightRect->bottomRight->coords().y()) - y;

        // 微调编辑框使其居中，使用整数尺寸避免小数问题
        m_waveformValueEditor->setGeometry(x, y + height / 2 - m_waveformValueEditor->height() / 2, width, m_waveformValueEditor->height());
        m_waveformValueEditor->setVisible(true);
        m_waveformValueEditor->setFocus();
        m_waveformValueEditor->selectAll();
    }
}

void MainWindow::onWaveformValueEdited()
{
    if (!m_waveformValueEditor || m_editingRow < 0 || m_editingPinColumn < 0 || !m_vectorTableWidget)
    {
        if (m_waveformValueEditor)
            m_waveformValueEditor->setVisible(false);
        return;
    }

    // 1. 隐藏编辑器
    m_waveformValueEditor->setVisible(false);

    // 2. 获取新值
    QString newValue = m_waveformValueEditor->text().toUpper(); // 自动转为大写

    // 验证新值是否有效
    const QString validChars = "01LHXSVM";
    if (newValue.isEmpty() || !validChars.contains(newValue.at(0)))
    {
        // 如果无效，则不更新，直接隐藏编辑器
        m_editingRow = -1;
        m_editingPinColumn = -1;
        return;
    }

    // 3. 更新表格
    int rowInPage = m_editingRow % m_pageSize;
    if (rowInPage < m_vectorTableWidget->rowCount())
    {
        QTableWidgetItem *cell = m_vectorTableWidget->item(rowInPage, m_editingPinColumn);
        if (cell)
        {
            // 检查值是否真的改变了
            if (cell->text() != newValue)
            {
                cell->setText(newValue);
                // onTableCellChanged 会被自动触发，处理数据保存
            }
        }
        else
        {
            // 如果单元格不存在，创建一个新的
            QTableWidgetItem *newItem = new QTableWidgetItem(newValue);
            m_vectorTableWidget->setItem(rowInPage, m_editingPinColumn, newItem);
        }
    }

    // 4. 更新波形图 (onTableCellChanged 也会更新，但为确保立即反馈，可以手动调用)
    updateWaveformView();

    // 重新计算 pinIndex 以保持高亮
    QList<QPair<QString, int>> pinColumns;
    for (int col = 0; col < m_vectorTableWidget->columnCount(); ++col)
    {
        QTableWidgetItem *headerItem = m_vectorTableWidget->horizontalHeaderItem(col);
        if (headerItem)
        {
            int currentTableId = m_vectorTableSelector->currentData().toInt();
            QList<Vector::ColumnInfo> columns = getCurrentColumnConfiguration(currentTableId);
            if (col < columns.size() && columns[col].type == Vector::ColumnDataType::PIN_STATE_ID)
            {
                pinColumns.append(qMakePair(headerItem->text().split('\n').first(), col));
            }
        }
    }
    int pinIndex = -1;
    for (int i = 0; i < pinColumns.size(); ++i)
    {
        if (pinColumns[i].second == m_editingPinColumn)
        {
            pinIndex = i;
            break;
        }
    }

    // 确保更新后高亮仍然在
    highlightWaveformPoint(m_editingRow, pinIndex);

    // 5. 重置编辑状态
    m_editingRow = -1;
    m_editingPinColumn = -1;
}

// 根据管脚ID获取管脚名称
QString MainWindow::getPinNameById(int pinId)
{
    if (pinId <= 0)
    {
        return QString();
    }

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << "getPinNameById - 数据库连接失败";
        return QString();
    }

    QSqlQuery query(db);
    query.prepare("SELECT name FROM pin_list WHERE id = ?");
    query.addBindValue(pinId);

    if (!query.exec())
    {
        qWarning() << "getPinNameById - 查询失败: " << query.lastError().text();
        return QString();
    }

    if (query.next())
    {
        QString pinName = query.value(0).toString();
        qDebug() << "getPinNameById - 管脚ID:" << pinId << " 名称:" << pinName;
        return pinName;
    }
    else
    {
        qWarning() << "getPinNameById - 未找到管脚ID:" << pinId;
        return QString();
    }
}