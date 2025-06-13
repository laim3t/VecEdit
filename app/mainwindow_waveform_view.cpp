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
    m_waveformPlot->addLayer("selection"); // 为高亮和标签创建一个顶层图层
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

    // 创建行内编辑器
    m_waveformValueEditor = new QLineEdit(m_waveformPlot);
    m_waveformValueEditor->setVisible(false);
    m_waveformValueEditor->setFrame(false);
    m_waveformValueEditor->setMaxLength(1); // 限制只能输入一个字符
    m_waveformValueEditor->setToolTip(tr("有效输入: 0, 1, L, H, X, S, V, M"));
    m_waveformValueEditor->setStyleSheet("QLineEdit { border: 1px solid blue; background-color: white; }");
    // 改为连接 editingFinished 信号，以便处理回车和失去焦点两种情况
    connect(m_waveformValueEditor, &QLineEdit::editingFinished, this, &MainWindow::onWaveformValueEdited);

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

    // =======================================================================
    // 阶段 1: 清理画布
    // =======================================================================
    m_waveformPlot->clearGraphs();
    // 清除上次绘制的所有自定义Item
    for (int i = m_waveformPlot->itemCount() - 1; i >= 0; --i)
    {
        if (auto item = m_waveformPlot->item(i))
        {
            if (item->property("isVBox").toBool() ||
                item->property("isXLine").toBool() ||
                item->property("isXTransition").toBool())
            {
                m_waveformPlot->removeItem(item);
            }
        }
    }
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

    // =======================================================================
    // 阶段 2: 数据准备
    // =======================================================================

    // 2.1 获取数据源
    if (m_waveformPinSelector->count() == 0 && m_vectorTableWidget)
    {
        // 清空现有的选择项
        m_waveformPinSelector->clear();
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
                    m_waveformPinSelector->addItem(displayName, headerText);
                }
            }
        }
    }
    if (m_waveformPinSelector->count() == 0)
    {
        m_waveformPlot->replot();
        return;
    }
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
    int currentTableId = m_vectorTableSelector->currentData().toInt();
    bool ok = false;
    QList<Vector::RowData> allRows = VectorDataHandler::instance().getAllVectorRows(currentTableId, ok);
    if (!ok)
    {
        qWarning() << "updateWaveformView - Failed to get all vector rows.";
        m_waveformPlot->replot();
        return;
    }

    // [新增] 使用当前页面中已修改的、最新的数据"覆盖"从数据处理器获取的基础数据
    if (m_vectorTableWidget && pinColumnIndex >= 0)
    {
        int firstRowOfPage = m_currentPage * m_pageSize;
        for (int i = 0; i < m_vectorTableWidget->rowCount(); ++i)
        {
            int actualRowIndex = firstRowOfPage + i;
            if (actualRowIndex < allRows.count())
            {
                QTableWidgetItem *item = m_vectorTableWidget->item(i, pinColumnIndex);
                if (item)
                {
                    // 使用表格当前的值更新 allRows 对应位置的值
                    allRows[actualRowIndex][pinColumnIndex] = item->text();
                }
            }
        }
    }
    
    int rowCount = allRows.count();

    // 2.2 定义Y轴电平带
    const double Y_HIGH_TOP = 20.0, Y_HIGH_BOTTOM = 16.0;
    const double Y_MID_TOP = 12.0, Y_MID_BOTTOM = 8.0;
    const double Y_LOW_TOP = 4.0, Y_LOW_BOTTOM = 0.0;

    // 2.3 准备数据容器
    QVector<double> xData(rowCount + 1);
    QVector<double> mainLineData(rowCount + 1);
    QVector<double> fillA_top(rowCount + 1), fillA_bottom(rowCount + 1);
    QVector<double> fillB_top(rowCount + 1), fillB_bottom(rowCount + 1);

    // 2.4 循环填充数据容器（只填充，不绘制）
    for (int i = 0; i < rowCount; ++i)
    {
        xData[i] = i;
        QChar state = allRows[i][pinColumnIndex].toString().at(0);

        // 初始化
        mainLineData[i] = qQNaN();
        fillA_top[i] = fillA_bottom[i] = qQNaN();
        fillB_top[i] = fillB_bottom[i] = qQNaN();

        // 填充主线条数据
        switch (state.toLatin1())
        {
        case '1':
        case 'H':
            mainLineData[i] = Y_HIGH_TOP;
            break;
        case '0':
        case 'L':
            mainLineData[i] = Y_LOW_BOTTOM;
            break;
        case 'M':
        case 'X': // X也在此处获取数据以保证线条连续性
            mainLineData[i] = (Y_MID_TOP + Y_MID_BOTTOM) / 2.0;
            break;
        default: // S 和 V 没有主线条
            break;
        }

        // 填充背景数据 (所有字母状态都有背景)
        if (state.isLetter())
        {
            if (i % 2 == 0) // 偶数行蓝色
            {
                fillA_top[i] = Y_HIGH_TOP;
                fillA_bottom[i] = Y_LOW_BOTTOM;
            }
            else // 奇数行紫色
            {
                fillB_top[i] = Y_HIGH_TOP;
                fillB_bottom[i] = Y_LOW_BOTTOM;
            }
        }
    }
    // 为lsStepLeft扩展最后一个数据点
    if (rowCount > 0)
    {
        xData[rowCount] = rowCount;
        mainLineData[rowCount] = mainLineData[rowCount - 1];
        fillA_top[rowCount] = fillA_top[rowCount - 1];
        fillA_bottom[rowCount] = fillA_bottom[rowCount - 1];
        fillB_top[rowCount] = fillB_top[rowCount - 1];
        fillB_bottom[rowCount] = fillB_bottom[rowCount - 1];
    }

    // =======================================================================
    // 阶段 3: 统一绘制
    // =======================================================================

    // 3.1 绘制背景填充层
    QCPGraph *fillB_bottom_g = m_waveformPlot->addGraph();
    QCPGraph *fillB_top_g = m_waveformPlot->addGraph();
    QCPGraph *fillA_bottom_g = m_waveformPlot->addGraph();
    QCPGraph *fillA_top_g = m_waveformPlot->addGraph();

    QBrush brushA(QColor(0, 0, 255, 70), Qt::DiagCrossPattern);
    fillA_top_g->setPen(Qt::NoPen);
    fillA_bottom_g->setPen(Qt::NoPen);
    fillA_top_g->setBrush(brushA);
    fillA_top_g->setChannelFillGraph(fillA_bottom_g);
    fillA_top_g->setData(xData, fillA_top);
    fillA_bottom_g->setData(xData, fillA_bottom);
    fillA_top_g->setLineStyle(QCPGraph::lsStepLeft);
    fillA_bottom_g->setLineStyle(QCPGraph::lsStepLeft);

    QBrush brushB(QColor(128, 0, 128, 70), Qt::DiagCrossPattern);
    fillB_top_g->setPen(Qt::NoPen);
    fillB_bottom_g->setPen(Qt::NoPen);
    fillB_top_g->setBrush(brushB);
    fillB_top_g->setChannelFillGraph(fillB_bottom_g);
    fillB_top_g->setData(xData, fillB_top);
    fillB_bottom_g->setData(xData, fillB_bottom);
    fillB_top_g->setLineStyle(QCPGraph::lsStepLeft);
    fillB_bottom_g->setLineStyle(QCPGraph::lsStepLeft);

    // 3.2 绘制主线条层
    QCPGraph *line_g = m_waveformPlot->addGraph();
    line_g->setPen(QPen(Qt::red, 2.5));
    line_g->setBrush(Qt::NoBrush);
    line_g->setData(xData, mainLineData);
    line_g->setLineStyle(QCPGraph::lsStepLeft);

    // 3.3 绘制独立的样式Item和覆盖层
    for (int i = 0; i < rowCount; ++i)
    {
        QChar state = allRows[i][pinColumnIndex].toString().at(0);

        // 绘制 'V' 状态的红色方框
        if (state == 'V')
        {
            QCPItemRect *box = new QCPItemRect(m_waveformPlot);
            box->setProperty("isVBox", true);
            box->setPen(QPen(Qt::red, 2.5));
            box->setBrush(Qt::NoBrush);
            box->topLeft->setCoords(i, Y_HIGH_TOP);
            box->bottomRight->setCoords(i + 1, Y_LOW_BOTTOM);
        }

        // 绘制 'X' 状态的灰色中线覆盖层
        if (state == 'X')
        {
            QCPItemLine *line = new QCPItemLine(m_waveformPlot);
            line->setLayer("overlay");
            line->setProperty("isXLine", true);
            line->setPen(QPen(Qt::gray, 2.5));
            double y = (Y_MID_TOP + Y_MID_BOTTOM) / 2.0;
            line->start->setCoords(i, y);
            line->end->setCoords(i + 1, y);
        }

        // 绘制 '->X' 的灰色过渡线覆盖层
        if (i > 0)
        {
            QChar previousState = allRows[i - 1][pinColumnIndex].toString().at(0);
            if (state == 'X')
            {
                double previousY = qQNaN();
                if (previousState == 'V')
                {
                    previousY = Y_HIGH_TOP; // 用于画完整边框
                }
                else
                {
                    switch (previousState.toLatin1())
                    {
                    case '1':
                    case 'H':
                        previousY = Y_HIGH_TOP;
                        break;
                    case '0':
                    case 'L':
                        previousY = Y_LOW_BOTTOM;
                        break;
                    case 'M':
                        previousY = (Y_MID_TOP + Y_MID_BOTTOM) / 2.0;
                        break;
                    }
                }

                if (!qIsNaN(previousY))
                {
                    QCPItemLine *transitionLine = new QCPItemLine(m_waveformPlot);
                    transitionLine->setLayer("overlay");
                    transitionLine->setProperty("isXTransition", true);
                    transitionLine->setPen(QPen(Qt::gray, 2.5));
                    transitionLine->start->setCoords(i, previousY);

                    double currentY = (previousState == 'V') ? Y_LOW_BOTTOM : (Y_MID_TOP + Y_MID_BOTTOM) / 2.0;
                    transitionLine->end->setCoords(i, currentY);
                }
            }
        }
    }

    // 3.4 最终绘图设置
    double xMax = rowCount > 0 ? qMin(rowCount, 40) : 10; // 保持之前的缩放级别
    m_waveformPlot->xAxis->setRange(0, xMax);
    m_waveformPlot->yAxis->setRange(-2.0, 22.0);
    m_waveformPlot->yAxis->setTickLabels(false);
    m_waveformPlot->yAxis->setSubTicks(false);

    // 3.5 重绘
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
    highlightRect->setLayer("selection"); // 将高亮矩形放到顶层
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
                hexLabel->setLayer("selection"); // 将标签放到顶层
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

    // 连接鼠标双击信号
    connect(m_waveformPlot, &QCustomPlot::mouseDoubleClick, this, &MainWindow::onWaveformDoubleClicked);
}

void MainWindow::onWaveformDoubleClicked(QMouseEvent *event)
{
    if (!m_waveformPlot || !m_vectorTableWidget || !m_waveformValueEditor) return;

    // 1. 获取点击位置对应的行列信息
    double key = m_waveformPlot->xAxis->pixelToCoord(event->pos().x());
    int rowIndex = static_cast<int>(floor(key));

    int totalRows = VectorDataHandler::instance().getVectorTableRowCount(m_vectorTableSelector->currentData().toInt());
    if (rowIndex < 0 || rowIndex >= totalRows) return;

    QString pinName = m_waveformPinSelector->currentData().toString();
    int pinColumnIndex = -1;
    for (int col = 0; col < m_vectorTableWidget->columnCount(); ++col) {
        if (m_vectorTableWidget->horizontalHeaderItem(col) && m_vectorTableWidget->horizontalHeaderItem(col)->text() == pinName) {
            pinColumnIndex = col;
            break;
        }
    }

    if (pinColumnIndex < 0) return;

    // 2. 准备编辑器
    // 保存编辑上下文
    m_editingRow = rowIndex;
    m_editingPinColumn = pinColumnIndex;

    // 计算行在当前页的索引
    int rowInPage = rowIndex % m_pageSize;

    // 获取当前值
    QString currentValue = "0";
    if (rowInPage < m_vectorTableWidget->rowCount()) {
        QTableWidgetItem *cell = m_vectorTableWidget->item(rowInPage, pinColumnIndex);
        if (cell) {
            currentValue = cell->text();
        }
    }
    
    m_waveformValueEditor->setText(currentValue);

    // 3. 定位并显示编辑器
    // 获取高亮矩形的位置来定位编辑器
    QCPItemRect *highlightRect = nullptr;
    for (int i = 0; i < m_waveformPlot->itemCount(); ++i) {
        if (auto item = m_waveformPlot->item(i)) {
            if (item->property("isSelectionHighlight").toBool()) {
                highlightRect = qobject_cast<QCPItemRect*>(item);
                break;
            }
        }
    }

    if(highlightRect) {
        // 将 plot 坐标转换为 widget 像素坐标
        int x = m_waveformPlot->xAxis->coordToPixel(highlightRect->topLeft->coords().x());
        int y = m_waveformPlot->yAxis->coordToPixel(highlightRect->topLeft->coords().y());
        int width = m_waveformPlot->xAxis->coordToPixel(highlightRect->bottomRight->coords().x()) - x;
        int height = m_waveformPlot->yAxis->coordToPixel(highlightRect->bottomRight->coords().y()) - y;
        
        // 微调编辑框使其居中
        m_waveformValueEditor->setGeometry(x, y + height / 2 - m_waveformValueEditor->height() / 2, width, m_waveformValueEditor->height());
        m_waveformValueEditor->setVisible(true);
        m_waveformValueEditor->setFocus();
        m_waveformValueEditor->selectAll();
    }
}

void MainWindow::onWaveformValueEdited()
{
    if (!m_waveformValueEditor || m_editingRow < 0 || m_editingPinColumn < 0 || !m_vectorTableWidget) {
        if(m_waveformValueEditor) m_waveformValueEditor->setVisible(false);
        return;
    }

    // 1. 隐藏编辑器
    m_waveformValueEditor->setVisible(false);

    // 2. 获取新值
    QString newValue = m_waveformValueEditor->text().toUpper(); // 自动转为大写
    
    // 验证新值是否有效
    const QString validChars = "01LHXSVM";
    if (newValue.isEmpty() || !validChars.contains(newValue.at(0))) {
         // 如果无效，则不更新，直接隐藏编辑器
        m_editingRow = -1;
        m_editingPinColumn = -1;
        return;
    }

    // 3. 更新表格
    int rowInPage = m_editingRow % m_pageSize;
    if (rowInPage < m_vectorTableWidget->rowCount()) {
        QTableWidgetItem *cell = m_vectorTableWidget->item(rowInPage, m_editingPinColumn);
        if (cell) {
            // 检查值是否真的改变了
            if (cell->text() != newValue) {
                cell->setText(newValue);
                // onTableCellChanged 会被自动触发，处理数据保存
            }
        } else {
            // 如果单元格不存在，创建一个新的
            QTableWidgetItem *newItem = new QTableWidgetItem(newValue);
            m_vectorTableWidget->setItem(rowInPage, m_editingPinColumn, newItem);
        }
    }

    // 4. 更新波形图 (onTableCellChanged 也会更新，但为确保立即反馈，可以手动调用)
    updateWaveformView();
    // 确保更新后高亮仍然在
    highlightWaveformPoint(m_editingRow);

    // 5. 重置编辑状态
    m_editingRow = -1;
    m_editingPinColumn = -1;
}