// ==========================================================
//  波形图视图相关实现：mainwindow_waveform_view.cpp
// ==========================================================

void MainWindow::setupWaveformView()
{
    // 创建波形图停靠窗口
    m_waveformDock = new QDockWidget(tr("波形图视图"), this);
    m_waveformDock->setObjectName("waveformDock");
    m_waveformDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_waveformDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);

    // 添加边框线样式，与其他停靠窗口保持一致
    m_waveformDock->setStyleSheet(
        "QDockWidget {"
        "   border: 1px solid #A0A0A0;"
        "}"
        "QDockWidget::title {"
        "   background-color: #E1E1E1;"
        "   padding-left: 5px;"
        "   border-bottom: 1px solid #A0A0A0;"
        "}");

    // 创建波形图容器作为停靠窗口的内容
    m_waveformContainer = new QWidget(m_waveformDock);
    m_waveformContainer->setObjectName("waveformContainer");
    QVBoxLayout *waveformLayout = new QVBoxLayout(m_waveformContainer);
    waveformLayout->setContentsMargins(5, 5, 5, 5);

    // 设置容器样式，与其他停靠窗口保持一致
    QString containerStyle = R"(
        QWidget#waveformContainer {
            background-color: #F0F0F0;
            border: 1px solid #A0A0A0;
            margin: 0px;
        }
    )";
    m_waveformContainer->setStyleSheet(containerStyle);

    // 创建顶部工具栏
    QWidget *toolbarWidget = new QWidget(m_waveformContainer);
    QHBoxLayout *toolbarLayout = new QHBoxLayout(toolbarWidget);
    toolbarLayout->setContentsMargins(0, 0, 0, 0);

    // 管脚选择器标签
    QLabel *pinSelectorLabel = new QLabel(tr("选择管脚:"), toolbarWidget);
    toolbarLayout->addWidget(pinSelectorLabel);

    // 添加管脚选择器
    m_waveformPinSelector = new QComboBox(toolbarWidget);
    toolbarLayout->addWidget(m_waveformPinSelector);
    connect(m_waveformPinSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onWaveformPinSelectionChanged);

    // 添加刷新按钮
    QPushButton *refreshButton = new QPushButton(tr("刷新波形"), toolbarWidget);
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::updateWaveformView);
    toolbarLayout->addWidget(refreshButton);

    // 添加伸缩项
    toolbarLayout->addStretch();

    // 将工具栏添加到主布局
    waveformLayout->addWidget(toolbarWidget);

    // 创建波形图
    m_waveformPlot = new QCustomPlot(m_waveformContainer);
    m_waveformPlot->setMinimumHeight(200); // 降低最小高度，便于停靠
    m_waveformPlot->xAxis->setLabel(tr("行号"));
    m_waveformPlot->yAxis->setLabel(tr("值"));

    // 设置X轴只显示整数刻度
    QSharedPointer<QCPAxisTickerFixed> intTicker(new QCPAxisTickerFixed);
    intTicker->setTickStep(1.0); // 刻度间隔为1
    m_waveformPlot->xAxis->setTicker(intTicker);
    m_waveformPlot->xAxis->setSubTicks(false);    // 不显示子刻度
    m_waveformPlot->xAxis->setNumberFormat("f");  // 使用固定点表示法
    m_waveformPlot->xAxis->setNumberPrecision(0); // 不显示小数位

    m_waveformPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    m_waveformPlot->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_waveformPlot, &QWidget::customContextMenuRequested, this, &MainWindow::onWaveformContextMenuRequested);
    waveformLayout->addWidget(m_waveformPlot);

    // 设置波形图点击处理
    setupWaveformClickHandling();

    // 将容器设置为停靠窗口的内容
    m_waveformDock->setWidget(m_waveformContainer);

    // 添加到主窗口底部
    addDockWidget(Qt::BottomDockWidgetArea, m_waveformDock);

    // 设置默认值
    m_isWaveformVisible = true;
    m_selectedWaveformPoint = -1; // 初始化选中点为-1（表示未选中）
}

void MainWindow::toggleWaveformView(bool show)
{
    if (m_waveformDock)
    {
        m_waveformDock->setVisible(show);
        m_isWaveformVisible = show;

        if (show)
        {
            // 更新波形图
            updateWaveformView();
        }
    }
}

void MainWindow::onWaveformPinSelectionChanged(int index)
{
    // 当选择的管脚改变时更新波形图
    if (index >= 0)
    {
        updateWaveformView();
    }
}

void MainWindow::updateWaveformView()
{
    // 1. 初始检查
    if (!m_waveformPinSelector || !m_waveformPlot)
        return;

    // 2. 清理旧状态
    m_waveformPlot->clearGraphs();
    if (m_waveformPlot->plotLayout()->elementCount() > 0)
    {
        if (auto *oldTitle = m_waveformPlot->plotLayout()->element(0, 0))
        {
            if (dynamic_cast<QCPTextElement *>(oldTitle))
            {
                m_waveformPlot->plotLayout()->remove(oldTitle);
            }
        }
    }

    // 3. 如果选择器为空，则填充管脚列表
    if (m_waveformPinSelector->count() == 0 && m_vectorTableWidget)
    {
        // 清空现有的选择项
        m_waveformPinSelector->clear();

        // 遍历所有列
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
                    // 只取管脚名称的第一部分（以换行符分割）
                    QString displayName = headerText.split('\n').first();

                    // 添加项目，显示简短名称，但保留完整名称作为用户数据
                    m_waveformPinSelector->addItem(displayName, headerText);
                }
            }
        }
    }

    // 4. 检查是否有管脚可供绘制
    if (m_waveformPinSelector->count() == 0)
    {
        m_waveformPlot->replot();
        return;
    }

    // 5. 获取选定的管脚信息
    QString pinName = m_waveformPinSelector->currentData().toString();
    if (!m_vectorTableWidget || m_vectorTableWidget->columnCount() <= 0)
    {
        m_waveformPlot->replot();
        return;
    }
    int pinColumnIndex = -1;
    for (int col = 0; col < m_vectorTableWidget->columnCount(); ++col)
    {
        if (m_vectorTableWidget->horizontalHeaderItem(col) && m_vectorTableWidget->horizontalHeaderItem(col)->text() == pinName)
        {
            pinColumnIndex = col;
            break;
        }
    }
    if (pinColumnIndex < 0)
    {
        m_waveformPlot->replot();
        return;
    }

    // 6. 定义Y轴电平带
    const double Y_HIGH_TOP = 20.0, Y_HIGH_BOTTOM = 16.0;
    const double Y_MID_TOP = 12.0, Y_MID_BOTTOM = 8.0;
    const double Y_LOW_TOP = 4.0, Y_LOW_BOTTOM = 0.0;

    // 7. 准备数据容器
    int currentTableId = m_vectorTableSelector->currentData().toInt();
    bool ok = false;
    QList<Vector::RowData> allRows = VectorDataHandler::instance().getAllVectorRows(currentTableId, ok);

    if (!ok)
    {
        qWarning() << "updateWaveformView - Failed to get all vector rows.";
        m_waveformPlot->replot();
        return;
    }

    int rowCount = allRows.count();
    QVector<double> xData(rowCount + 1),
        lineData(rowCount + 1),
        fillA_top(rowCount + 1), fillA_bottom(rowCount + 1),
        fillB_top(rowCount + 1), fillB_bottom(rowCount + 1);

    // 8. 遍历表格填充数据
    for (int i = 0; i < rowCount; ++i)
    {
        xData[i] = i;
        const auto &rowData = allRows[i];
        QChar state = ' ';
        if (pinColumnIndex < rowData.size())
        {
            state = rowData[pinColumnIndex].toString().at(0);
        }

        fillA_top[i] = fillA_bottom[i] = qQNaN();
        fillB_top[i] = fillB_bottom[i] = qQNaN();
        switch (state.toLatin1())
        {
        case '1':
            lineData[i] = Y_HIGH_TOP;
            break;
        case '0':
            lineData[i] = Y_LOW_BOTTOM;
            break;
        case 'H':
            lineData[i] = Y_HIGH_TOP;
            fillA_top[i] = Y_HIGH_TOP;
            fillA_bottom[i] = Y_LOW_BOTTOM; // 将底部边界扩展到整个区域
            break;
        case 'L':
            lineData[i] = Y_LOW_BOTTOM;
            fillB_top[i] = Y_HIGH_TOP; // 将顶部边界扩展到整个区域
            fillB_bottom[i] = Y_LOW_BOTTOM;
            break;
        case 'M':
            lineData[i] = Y_MID_TOP;
            fillA_top[i] = Y_MID_TOP;
            fillA_bottom[i] = Y_MID_BOTTOM;
            break;
        case 'S':
            lineData[i] = Y_HIGH_TOP;
            fillB_top[i] = Y_HIGH_TOP;
            fillB_bottom[i] = Y_HIGH_BOTTOM;
            break;
        case 'V':
            lineData[i] = Y_MID_TOP;
            fillA_top[i] = Y_MID_TOP;
            fillA_bottom[i] = Y_MID_BOTTOM;
            break;
        case 'X':
            lineData[i] = (Y_MID_TOP + Y_MID_BOTTOM) / 2.0;
            fillB_top[i] = Y_MID_TOP;
            fillB_bottom[i] = Y_MID_BOTTOM;
            break;
        default:
            lineData[i] = qQNaN();
            break;
        }
    }
    if (rowCount > 0)
    { // 为lsStepLeft扩展数据以绘制最后一段
        xData[rowCount] = rowCount;
        lineData[rowCount] = lineData[rowCount - 1];
        fillA_top[rowCount] = fillA_top[rowCount - 1];
        fillA_bottom[rowCount] = fillA_bottom[rowCount - 1];
        fillB_top[rowCount] = fillB_top[rowCount - 1];
        fillB_bottom[rowCount] = fillB_bottom[rowCount - 1];
    }

    // 9. 创建和配置Graphs
    QCPGraph *fillB_bottom_g = m_waveformPlot->addGraph();
    QCPGraph *fillB_top_g = m_waveformPlot->addGraph();
    QCPGraph *fillA_bottom_g = m_waveformPlot->addGraph();
    QCPGraph *fillA_top_g = m_waveformPlot->addGraph();
    QCPGraph *line_g = m_waveformPlot->addGraph();

    QBrush brushA(QColor(0, 0, 255, 70), Qt::DiagCrossPattern);
    fillA_top_g->setPen(Qt::NoPen);
    fillA_bottom_g->setPen(Qt::NoPen); // 设置底部图形也为无画笔，去除边界线
    fillA_top_g->setBrush(brushA);
    fillA_top_g->setChannelFillGraph(fillA_bottom_g);

    QBrush brushB(QColor(128, 0, 128, 70), Qt::DiagCrossPattern);
    fillB_top_g->setPen(Qt::NoPen);
    fillB_bottom_g->setPen(Qt::NoPen); // 设置底部图形也为无画笔，去除边界线
    fillB_top_g->setBrush(brushB);
    fillB_top_g->setChannelFillGraph(fillB_bottom_g);

    line_g->setPen(QPen(Qt::red, 2.5));
    line_g->setBrush(Qt::NoBrush);

    // 10. 设置数据和线型
    line_g->setData(xData, lineData);
    line_g->setLineStyle(QCPGraph::lsStepLeft);
    fillA_top_g->setData(xData, fillA_top);
    fillA_bottom_g->setData(xData, fillA_bottom);
    fillA_top_g->setLineStyle(QCPGraph::lsStepLeft);
    fillA_bottom_g->setLineStyle(QCPGraph::lsStepLeft);
    fillB_top_g->setData(xData, fillB_top);
    fillB_bottom_g->setData(xData, fillB_bottom);
    fillB_top_g->setLineStyle(QCPGraph::lsStepLeft);
    fillB_bottom_g->setLineStyle(QCPGraph::lsStepLeft);

    // 11. 最终绘图设置
    // 确保X轴范围从0开始，不显示负坐标
    double xMax = rowCount > 0 ? qMin(rowCount, 5) : 10;
    m_waveformPlot->xAxis->setRange(0, xMax);
    m_waveformPlot->yAxis->setRange(-2.0, 22.0);
    m_waveformPlot->yAxis->setTickLabels(false);
    m_waveformPlot->yAxis->setSubTicks(false);

    // 12. 重绘
    m_waveformPlot->replot();
}

void MainWindow::onWaveformContextMenuRequested(const QPoint &pos)
{
    if (!m_waveformPlot || m_waveformPlot->graphCount() == 0 || !m_vectorTableWidget)
        return;

    // Convert widget coordinates to plot coordinates to find the row index
    double key = m_waveformPlot->xAxis->pixelToCoord(pos.x());
    int rowIndex = static_cast<int>(floor(key));

    // 验证行索引是否有效
    int totalRows = VectorDataHandler::instance().getVectorTableRowCount(m_vectorTableSelector->currentData().toInt());
    if (rowIndex < 0 || rowIndex >= totalRows)
        return;

    // 更新选中点并高亮显示
    highlightWaveformPoint(rowIndex);

    QMenu contextMenu(this);
    QAction *jumpAction = contextMenu.addAction(tr("跳转至向量表"));

    connect(jumpAction, &QAction::triggered, this, [this, rowIndex]()
            {
        // Find the column index from the currently selected pin
        QString pinName = m_waveformPinSelector->currentData().toString();
        int pinColumnIndex = -1;
        for (int col = 0; col < m_vectorTableWidget->columnCount(); ++col) {
            if (m_vectorTableWidget->horizontalHeaderItem(col) && m_vectorTableWidget->horizontalHeaderItem(col)->text() == pinName) {
                pinColumnIndex = col;
                break;
            }
        }

        if (pinColumnIndex >= 0) {
            // 计算行应该在哪个页面
            int pageForRow = rowIndex / m_pageSize;
            int rowInPage = rowIndex % m_pageSize;
            
            // 如果需要，切换到正确的页面
            if (pageForRow != m_currentPage) {
                // 保存旧页面数据
                saveCurrentTableData();
                
                // 更新页码
                m_currentPage = pageForRow;
                
                // 更新分页信息UI
                updatePaginationInfo();
                
                // 加载新页面
                int tableId = m_vectorTableSelector->currentData().toInt();
                VectorDataHandler::instance().loadVectorTablePageData(
                    tableId, m_vectorTableWidget, m_currentPage, m_pageSize);
            }
            
            // Jump to the cell - 使用页内行索引
            if (rowInPage < m_vectorTableWidget->rowCount()) {
                m_vectorTableWidget->setCurrentCell(rowInPage, pinColumnIndex);
                m_vectorTableWidget->scrollToItem(m_vectorTableWidget->item(rowInPage, pinColumnIndex), QAbstractItemView::PositionAtCenter);
                m_vectorTableWidget->setFocus(); // Ensure the table gets focus
            }
        } });

    contextMenu.exec(m_waveformPlot->mapToGlobal(pos));
}

// 高亮显示波形图中的指定点
void MainWindow::highlightWaveformPoint(int rowIndex)
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

    // 创建一个矩形用于高亮选中的点
    QCPItemRect *highlightRect = new QCPItemRect(m_waveformPlot);
    highlightRect->setProperty("isSelectionHighlight", true);
    highlightRect->topLeft->setCoords(rowIndex - 0.02, m_waveformPlot->yAxis->range().upper);
    highlightRect->bottomRight->setCoords(rowIndex + 0.98, m_waveformPlot->yAxis->range().lower);
    highlightRect->setPen(QPen(Qt::blue, 1));
    highlightRect->setBrush(QBrush(QColor(0, 0, 255, 30)));

    // 创建并显示十六进制值标签
    // 1. 获取列索引
    QString pinName = m_waveformPinSelector->currentData().toString();
    int pinColumnIndex = -1;
    for (int col = 0; col < m_vectorTableWidget->columnCount(); ++col)
    {
        if (m_vectorTableWidget->horizontalHeaderItem(col) && m_vectorTableWidget->horizontalHeaderItem(col)->text() == pinName)
        {
            pinColumnIndex = col;
            break;
        }
    }

    if (pinColumnIndex >= 0)
    {
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
            VectorDataHandler::instance().loadVectorTablePageData(
                tableId, m_vectorTableWidget, m_currentPage, m_pageSize);
        }

        // 确保行索引有效
        if (rowInPage < m_vectorTableWidget->rowCount())
        {
            // 获取单元格内容
            QTableWidgetItem *item = m_vectorTableWidget->item(rowInPage, pinColumnIndex);
            if (item)
            {
                QString cellValue = item->text();
                bool ok;
                int intValue = cellValue.toInt(&ok);
                QString hexValue;

                // 转换为十六进制
                if (ok)
                {
                    hexValue = "0x" + QString::number(intValue, 16);
                }
                else
                {
                    // 对于非数字，可以根据需要进行映射
                    hexValue = cellValue;
                }

                // 创建文本标签
                QCPItemText *hexLabel = new QCPItemText(m_waveformPlot);
                hexLabel->setProperty("isHexValueLabel", true);
                hexLabel->setText(hexValue);
                hexLabel->setFont(QFont("sans-serif", 10));
                hexLabel->setColor(Qt::black);

                // 设置背景
                hexLabel->setBrush(QBrush(QColor(240, 240, 240, 200)));
                hexLabel->setPen(QPen(Qt::gray));
                hexLabel->setPadding(QMargins(5, 2, 5, 2));

                // 设置标签位置
                double yPos = (m_waveformPlot->yAxis->range().upper + m_waveformPlot->yAxis->range().lower) / 2.0;
                hexLabel->position->setCoords(rowIndex + 0.5, yPos);

                // 高亮表格中的对应单元格
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
            int rowIndex = static_cast<int>(floor(key));
            
            // 检查索引是否有效（只响应正坐标）
            int totalRows = VectorDataHandler::instance().getVectorTableRowCount(m_vectorTableSelector->currentData().toInt());
            if (rowIndex >= 0 && rowIndex < totalRows) {
                // 高亮显示选中的点
                highlightWaveformPoint(rowIndex);
            }
        } });
}