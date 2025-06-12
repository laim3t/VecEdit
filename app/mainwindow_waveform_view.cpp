// ==========================================================
//  波形图视图相关实现：mainwindow_waveform_view.cpp
// ==========================================================

void MainWindow::setupWaveformView()
{
    // 创建波形图容器
    m_waveformContainer = new QWidget(this);
    QVBoxLayout *waveformLayout = new QVBoxLayout(m_waveformContainer);
    waveformLayout->setContentsMargins(5, 5, 5, 5);

    // 创建顶部工具栏
    QWidget *toolbarWidget = new QWidget(m_waveformContainer);
    QHBoxLayout *toolbarLayout = new QHBoxLayout(toolbarWidget);
    toolbarLayout->setContentsMargins(0, 0, 0, 0);

    // 标题标签
    QLabel *titleLabel = new QLabel(tr("波形图视图"), toolbarWidget);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    toolbarLayout->addWidget(titleLabel);

    // 添加管脚选择器
    QLabel *pinSelectorLabel = new QLabel(tr("选择管脚:"), toolbarWidget);
    toolbarLayout->addWidget(pinSelectorLabel);

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
    m_waveformPlot->setMinimumHeight(250);
    m_waveformPlot->setMinimumWidth(1000);
    m_waveformPlot->xAxis->setLabel(tr("行号"));
    m_waveformPlot->yAxis->setLabel(tr("值"));
    
    // 设置X轴只显示整数刻度
    QSharedPointer<QCPAxisTickerFixed> intTicker(new QCPAxisTickerFixed);
    intTicker->setTickStep(1.0);  // 刻度间隔为1
    m_waveformPlot->xAxis->setTicker(intTicker);
    m_waveformPlot->xAxis->setSubTicks(false); // 不显示子刻度
    m_waveformPlot->xAxis->setNumberFormat("f"); // 使用固定点表示法
    m_waveformPlot->xAxis->setNumberPrecision(0); // 不显示小数位
    
    m_waveformPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    m_waveformPlot->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_waveformPlot, &QWidget::customContextMenuRequested, this, &MainWindow::onWaveformContextMenuRequested);
    waveformLayout->addWidget(m_waveformPlot);

    // 设置波形图点击处理
    setupWaveformClickHandling();

    // 设置默认值
    m_isWaveformVisible = false;
    m_selectedWaveformPoint = -1;  // 初始化选中点为-1（表示未选中）

    // 将波形图容器添加到主窗口
    QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout *>(m_centralWidget->layout());
    if (mainLayout)
    {
        mainLayout->addWidget(m_waveformContainer);
        m_waveformContainer->setVisible(false); // 初始时隐藏
    }
}

void MainWindow::toggleWaveformView(bool show)
{
    if (m_waveformContainer)
    {
        m_waveformContainer->setVisible(show);
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
        if (auto* oldTitle = m_waveformPlot->plotLayout()->element(0, 0))
        {
            if (dynamic_cast<QCPTextElement*>(oldTitle))
            {
                m_waveformPlot->plotLayout()->remove(oldTitle);
            }
        }
    }

    // 3. 如果选择器为空，则填充管脚列表
    if (m_waveformPinSelector->count() == 0 && m_vectorTableWidget)
    {
        QStringList pinHeaders;
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
                    pinHeaders << headerText;
                }
            }
        }
        m_waveformPinSelector->clear();
        m_waveformPinSelector->addItems(pinHeaders);
    }

    // 4. 检查是否有管脚可供绘制
    if (m_waveformPinSelector->count() == 0) {
        m_waveformPlot->replot();
        return;
    }

    // 5. 获取选定的管脚信息
    QString pinName = m_waveformPinSelector->currentText();
    if (!m_vectorTableWidget || m_vectorTableWidget->columnCount() <= 0) {
        m_waveformPlot->replot();
        return;
    }
    int pinColumnIndex = -1;
    for (int col = 0; col < m_vectorTableWidget->columnCount(); ++col) {
        if (m_vectorTableWidget->horizontalHeaderItem(col) && m_vectorTableWidget->horizontalHeaderItem(col)->text() == pinName) {
            pinColumnIndex = col;
            break;
        }
    }
    if (pinColumnIndex < 0) {
        m_waveformPlot->replot();
        return;
    }

    // 6. 定义Y轴电平带
    const double Y_HIGH_TOP = 20.0, Y_HIGH_BOTTOM = 16.0;
    const double Y_MID_TOP = 12.0, Y_MID_BOTTOM = 8.0;
    const double Y_LOW_TOP = 4.0, Y_LOW_BOTTOM = 0.0;

    // 7. 准备数据容器
    int rowCount = m_vectorTableWidget->rowCount();
    QVector<double> xData(rowCount + 1),
        lineData(rowCount + 1),
        fillA_top(rowCount + 1), fillA_bottom(rowCount + 1),
        fillB_top(rowCount + 1), fillB_bottom(rowCount + 1);

    // 8. 遍历表格填充数据
    for (int i = 0; i < rowCount; ++i) {
        xData[i] = i;
        QTableWidgetItem* item = m_vectorTableWidget->item(i, pinColumnIndex);
        QChar state = item ? item->text().at(0) : ' ';
        fillA_top[i] = fillA_bottom[i] = qQNaN();
        fillB_top[i] = fillB_bottom[i] = qQNaN();
        switch (state.toLatin1()) {
            case '1': lineData[i] = Y_HIGH_TOP; break;
            case '0': lineData[i] = Y_LOW_BOTTOM; break;
            case 'H': lineData[i] = Y_HIGH_TOP; fillA_top[i] = Y_HIGH_TOP; fillA_bottom[i] = Y_HIGH_BOTTOM; break;
            case 'L': lineData[i] = Y_LOW_BOTTOM; fillB_top[i] = Y_LOW_TOP; fillB_bottom[i] = Y_LOW_BOTTOM; break;
            case 'M': lineData[i] = Y_MID_TOP; fillA_top[i] = Y_MID_TOP; fillA_bottom[i] = Y_MID_BOTTOM; break;
            case 'S': lineData[i] = Y_HIGH_TOP; fillB_top[i] = Y_HIGH_TOP; fillB_bottom[i] = Y_HIGH_BOTTOM; break;
            case 'V': lineData[i] = Y_MID_TOP; fillA_top[i] = Y_MID_TOP; fillA_bottom[i] = Y_MID_BOTTOM; break;
            case 'X': lineData[i] = (Y_MID_TOP + Y_MID_BOTTOM) / 2.0; fillB_top[i] = Y_MID_TOP; fillB_bottom[i] = Y_MID_BOTTOM; break;
            default: lineData[i] = qQNaN(); break;
        }
    }
    if (rowCount > 0) { // 为lsStepLeft扩展数据以绘制最后一段
        xData[rowCount] = rowCount;
        lineData[rowCount] = lineData[rowCount-1];
        fillA_top[rowCount] = fillA_top[rowCount-1];
        fillA_bottom[rowCount] = fillA_bottom[rowCount-1];
        fillB_top[rowCount] = fillB_top[rowCount-1];
        fillB_bottom[rowCount] = fillB_bottom[rowCount-1];
    }

    // 9. 创建和配置Graphs
    QCPGraph *fillB_bottom_g = m_waveformPlot->addGraph();
    QCPGraph *fillB_top_g = m_waveformPlot->addGraph();
    QCPGraph *fillA_bottom_g = m_waveformPlot->addGraph();
    QCPGraph *fillA_top_g = m_waveformPlot->addGraph();
    QCPGraph *line_g = m_waveformPlot->addGraph();

    QBrush brushA(QColor(0, 0, 255, 40), Qt::CrossPattern);
    fillA_top_g->setPen(Qt::NoPen);
    fillA_top_g->setBrush(brushA);
    fillA_top_g->setChannelFillGraph(fillA_bottom_g);

    QBrush brushB(QColor(128, 0, 128, 40), Qt::CrossPattern);
    fillB_top_g->setPen(Qt::NoPen);
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
    int rowIndex = qRound(key);

    // Validate the row index
    if (rowIndex < 0 || rowIndex >= m_vectorTableWidget->rowCount())
        return;

    // 更新选中点并高亮显示
    highlightWaveformPoint(rowIndex);

    QMenu contextMenu(this);
    QAction *jumpAction = contextMenu.addAction(tr("跳转至向量表"));

    connect(jumpAction, &QAction::triggered, this, [this, rowIndex]() {
        // Find the column index from the currently selected pin
        QString pinName = m_waveformPinSelector->currentText();
        int pinColumnIndex = -1;
        for (int col = 0; col < m_vectorTableWidget->columnCount(); ++col) {
            if (m_vectorTableWidget->horizontalHeaderItem(col) && m_vectorTableWidget->horizontalHeaderItem(col)->text() == pinName) {
                pinColumnIndex = col;
                break;
            }
        }

        if (pinColumnIndex >= 0) {
            // Jump to the cell
            m_vectorTableWidget->setCurrentCell(rowIndex, pinColumnIndex);
            m_vectorTableWidget->scrollToItem(m_vectorTableWidget->item(rowIndex, pinColumnIndex), QAbstractItemView::PositionAtCenter);
            m_vectorTableWidget->setFocus(); // Ensure the table gets focus
        }
    });

    contextMenu.exec(m_waveformPlot->mapToGlobal(pos));
}

// 高亮显示波形图中的指定点
void MainWindow::highlightWaveformPoint(int rowIndex)
{
    if (!m_waveformPlot || rowIndex < 0 || rowIndex >= m_vectorTableWidget->rowCount())
        return;

    // 保存选中的点
    m_selectedWaveformPoint = rowIndex;

    // 移除所有已有的选中标记
    for (int i = m_waveformPlot->itemCount() - 1; i >= 0; i--) {
        if (auto item = dynamic_cast<QCPItemRect*>(m_waveformPlot->item(i))) {
            if (item->property("isSelectionHighlight").toBool()) {
                m_waveformPlot->removeItem(i);
            }
        }
    }

    // 创建一个矩形用于高亮选中的点
    QCPItemRect *highlightRect = new QCPItemRect(m_waveformPlot);
    highlightRect->setProperty("isSelectionHighlight", true);
    
    // 设置矩形的位置，覆盖整个格子区域（从当前点到下一个点）
    // 左边界是当前点减去一点偏移，右边界是下一个点减去一点偏移
    highlightRect->topLeft->setCoords(rowIndex - 0.02, m_waveformPlot->yAxis->range().upper);
    highlightRect->bottomRight->setCoords(rowIndex + 0.98, m_waveformPlot->yAxis->range().lower);
    
    // 设置矩形的样式
    highlightRect->setPen(QPen(Qt::green, 2, Qt::DashLine));
    highlightRect->setBrush(QBrush(QColor(0, 255, 0, 30)));
    
    // 重绘波形图
    m_waveformPlot->replot();
}

// 添加鼠标点击事件处理
void MainWindow::setupWaveformClickHandling()
{
    if (!m_waveformPlot)
        return;
    
    // 连接鼠标点击信号
    connect(m_waveformPlot, &QCustomPlot::mousePress, this, [this](QMouseEvent *event) {
        if (event->button() == Qt::LeftButton) {
            // 获取点击位置对应的数据点索引
            double key = m_waveformPlot->xAxis->pixelToCoord(event->pos().x());
            int rowIndex = qRound(key);
            
            // 检查索引是否有效（只响应正坐标）
            if (rowIndex >= 0 && rowIndex < m_vectorTableWidget->rowCount()) {
                // 高亮显示选中的点
                highlightWaveformPoint(rowIndex);
            }
        }
    });
}