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
    m_waveformPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    waveformLayout->addWidget(m_waveformPlot);

    // 设置默认值
    m_isWaveformVisible = false;

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
    m_waveformPlot->xAxis->setRange(0, rowCount > 0 ? qMin(rowCount, 5) : 10);
    m_waveformPlot->yAxis->setRange(-2.0, 22.0);
    m_waveformPlot->yAxis->setTickLabels(false);
    m_waveformPlot->yAxis->setSubTicks(false);

    // 12. 重绘
    m_waveformPlot->replot();
}