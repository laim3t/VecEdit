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
    m_waveformPlot->setMinimumHeight(150);
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
    // 确保有管脚选择器和波形图
    if (!m_waveformPinSelector || !m_waveformPlot || m_waveformPinSelector->count() == 0)
        return;

    // 获取当前选中的管脚和对应的列
    int currentPinIndex = m_waveformPinSelector->currentIndex();
    QString pinName = m_waveformPinSelector->currentText();

    // 确保有数据表显示
    if (!m_vectorTableWidget || m_vectorTableWidget->columnCount() <= 0 || m_vectorTableWidget->rowCount() <= 0)
        return;

    // 更新波形图管脚选择器（如果尚未更新）
    if (m_waveformPinSelector->count() == 0)
    {
        QStringList pinHeaders;
        for (int col = 0; col < m_vectorTableWidget->columnCount(); ++col)
        {
            QTableWidgetItem *headerItem = m_vectorTableWidget->horizontalHeaderItem(col);
            if (headerItem)
            {
                QString headerText = headerItem->text();

                // 获取当前表的列配置信息，判断是否是管脚列
                int currentTableId = m_vectorTableSelector->currentData().toInt();
                QList<Vector::ColumnInfo> columns = getCurrentColumnConfiguration(currentTableId);

                if (col < columns.size() && columns[col].type == Vector::ColumnDataType::PIN_STATE_ID)
                {
                    pinHeaders << headerText;
                }
            }
        }

        // 添加到选择器
        m_waveformPinSelector->clear();
        m_waveformPinSelector->addItems(pinHeaders);

        // 如果有管脚可选，默认选择第一个
        if (m_waveformPinSelector->count() > 0)
        {
            m_waveformPinSelector->setCurrentIndex(0);
            pinName = m_waveformPinSelector->currentText();
        }
        else
        {
            return; // 没有管脚可用，退出
        }
    }

    // 找到对应的管脚列
    int pinColumnIndex = -1;
    for (int col = 0; col < m_vectorTableWidget->columnCount(); ++col)
    {
        QTableWidgetItem *headerItem = m_vectorTableWidget->horizontalHeaderItem(col);
        if (headerItem && headerItem->text() == pinName)
        {
            pinColumnIndex = col;
            break;
        }
    }

    if (pinColumnIndex < 0)
        return;

    // 准备数据
    QVector<double> xData, yData;

    // 获取视图中可见的所有行
    for (int row = 0; row < m_vectorTableWidget->rowCount(); ++row)
    {
        QTableWidgetItem *item = m_vectorTableWidget->item(row, pinColumnIndex);
        if (!item)
            continue;

        // 获取单元格值并转换为数值
        QString cellValue = item->text().trimmed();
        double value = 0;

        // 处理不同的管脚值，根据项目的具体表示方式调整
        if (cellValue == "1" || cellValue == "H" || cellValue == "高")
            value = 1;
        else if (cellValue == "0" || cellValue == "L" || cellValue == "低")
            value = 0;
        else if (cellValue == "Z" || cellValue == "阻抗")
            value = 0.5; // 高阻态显示为中间值

        xData.append(row);
        yData.append(value);
    }

    // 清除旧的图形
    m_waveformPlot->clearGraphs();

    // 添加新图形
    QCPGraph *graph = m_waveformPlot->addGraph();
    graph->setData(xData, yData);

    // 设置图形样式
    QPen graphPen;
    graphPen.setColor(QColor(0, 0, 255));
    graphPen.setWidth(2);
    graph->setPen(graphPen);
    graph->setLineStyle(QCPGraph::lsStepCenter); // 使用阶梯状线条，适合数字信号

    // 设置坐标轴范围
    m_waveformPlot->xAxis->setRange(-1, xData.size());
    m_waveformPlot->yAxis->setRange(-0.1, 1.1); // 数字信号的范围

    // 设置标题
    m_waveformPlot->plotLayout()->insertRow(0);
    m_waveformPlot->plotLayout()->addElement(0, 0, new QCPTextElement(m_waveformPlot, tr("管脚 '%1' 波形图").arg(pinName)));

    // 刷新绘图
    m_waveformPlot->replot();
}