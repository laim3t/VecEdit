// ==========================================================
//  波形图视图相关实现：mainwindow_waveform_view.cpp
// ==========================================================

#include <QDebug>
#include <QMenu>
#include <QAction>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonArray>

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

    // 添加"全部"勾选框
    m_showAllPinsCheckBox = new QCheckBox(tr("全部"), toolbarWidget);
    connect(m_showAllPinsCheckBox, &QCheckBox::stateChanged, this, &MainWindow::onShowAllPinsChanged);
    toolbarLayout->addWidget(m_showAllPinsCheckBox);

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

    // 设置交互：允许拖拽和缩放
    m_waveformPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    m_waveformPlot->axisRect()->setRangeDrag(Qt::Horizontal);
    m_waveformPlot->axisRect()->setRangeZoomAxes(m_waveformPlot->xAxis, nullptr);

    // 设置图例（禁用）
    m_waveformPlot->legend->setVisible(false);
    m_waveformPlot->legend->setFont(QFont("Helvetica", 9));
    m_waveformPlot->legend->setBrush(QBrush(QColor(255, 255, 255, 230)));
    m_waveformPlot->legend->setBorderPen(Qt::NoPen);
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
    m_showAllPins = false;        // 默认不显示所有管脚
    if (m_showAllPinsCheckBox)
    {
        m_showAllPinsCheckBox->setChecked(false); // 默认不勾选"全部"复选框
    }
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

// 处理"全部"勾选框状态变化
void MainWindow::onShowAllPinsChanged(int state)
{
    // 更新显示所有管脚的标志
    m_showAllPins = (state == Qt::Checked);

    // 根据勾选状态启用或禁用管脚选择器
    m_waveformPinSelector->setEnabled(!m_showAllPins);

    // 更新波形图
    updateWaveformView();
}

void MainWindow::updateWaveformView()
{
    // 1. 初始检查
    if (!m_waveformPinSelector || !m_waveformPlot)
        return;

    // 保存当前的X轴范围（缩放状态）
    double savedXMin = m_waveformPlot->xAxis->range().lower;
    double savedXMax = m_waveformPlot->xAxis->range().upper;

    // 检查是否存在自定义缩放 - 通过比较范围与默认范围是否不同
    bool isDefaultRange = (savedXMin == -0.5 || qFuzzyCompare(savedXMin, -0.5)) &&
                          (savedXMax <= 40 || qFuzzyCompare(savedXMax, 40));
    bool hasCustomRange = !isDefaultRange;

    // =======================================================================
    // 阶段 1: 清理画布
    // =======================================================================
    m_waveformPlot->clearGraphs();

    // 清理特殊波形点集合
    m_r0Points.clear();
    m_rzPoints.clear();
    m_sbcPoints.clear();

    // 清除上次绘制的所有自定义Item
    for (int i = m_waveformPlot->itemCount() - 1; i >= 0; --i)
    {
        if (auto item = m_waveformPlot->item(i))
        {
            if (item->property("isVBox").toBool() ||
                item->property("isXLine").toBool() ||
                item->property("isXTransition").toBool() ||
                item->property("isT1RLine").toBool() ||       // 添加T1R线清理
                item->property("isT1RLabel").toBool() ||      // 添加T1R标签清理
                item->property("isT1RMiddleLine").toBool() || // 添加T1R中间线清理
                item->property("isT1RTransition").toBool() || // 添加T1R过渡线清理
                item->property("isSelectionHighlight").toBool() ||
                item->property("isHexValueLabel").toBool() ||
                item->property("isR0Line").toBool() ||  // 添加R0线清理
                item->property("isRZLine").toBool() ||  // 添加RZ线清理
                item->property("isSBCLine").toBool() || // 添加SBC线清理
                item->property("isPinLabel").toBool())  // 添加SBC线清理
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
    // 阶段 2: 收集管脚数据和准备绘制区域
    // =======================================================================

    // 2.1 获取所有管脚信息
    QList<QPair<QString, int>> pinColumns; // 存储管脚名称和对应的列索引
    int currentTableId = m_vectorTableSelector->currentData().toInt();
    QList<Vector::ColumnInfo> columns;
    
    // 根据使用的模式获取表格列配置
    if (m_isUsingNewTableModel && m_vectorTableModel)
    {
        columns = getCurrentColumnConfiguration(currentTableId);
        
        // 从数据模型收集所有管脚信息
        for (int col = 0; col < columns.size(); ++col)
        {
            if (columns[col].type == Vector::ColumnDataType::PIN_STATE_ID)
            {
                QString displayName = columns[col].name;
                pinColumns.append(qMakePair(displayName, col));
                
                // 确保下拉选择器中有此管脚
                bool found = false;
                for (int i = 0; i < m_waveformPinSelector->count(); ++i)
                {
                    if (m_waveformPinSelector->itemText(i) == displayName)
                    {
                        found = true;
                        break;
                    }
                }
                
                if (!found)
                {
                    m_waveformPinSelector->addItem(displayName, displayName);
                }
            }
        }
    }
    else if (m_vectorTableWidget)
    {
        if (m_waveformPinSelector->count() == 0)
        {
            // 清空现有的选择项
            m_waveformPinSelector->clear();
            for (int col = 0; col < m_vectorTableWidget->columnCount(); ++col)
            {
                QTableWidgetItem *headerItem = m_vectorTableWidget->horizontalHeaderItem(col);
                if (headerItem)
                {
                    QString headerText = headerItem->text();
                    columns = getCurrentColumnConfiguration(currentTableId);
                    if (col < columns.size() && columns[col].type == Vector::ColumnDataType::PIN_STATE_ID)
                    {
                        QString displayName = headerText.split('\n').first();
                        m_waveformPinSelector->addItem(displayName, headerText);
                        // 同时收集所有管脚信息
                        pinColumns.append(qMakePair(displayName, col));
                    }
                }
            }
        }
        else
        {
            // 如果选择器已经有项目，收集所有管脚信息
            for (int col = 0; col < m_vectorTableWidget->columnCount(); ++col)
            {
                QTableWidgetItem *headerItem = m_vectorTableWidget->horizontalHeaderItem(col);
                if (headerItem)
                {
                    QString headerText = headerItem->text();
                    columns = getCurrentColumnConfiguration(currentTableId);
                    if (col < columns.size() && columns[col].type == Vector::ColumnDataType::PIN_STATE_ID)
                    {
                        QString displayName = headerText.split('\n').first();
                        pinColumns.append(qMakePair(displayName, col));
                    }
                }
            }
        }
    }

    // 根据"全部"勾选框状态，决定要显示的管脚
    if (!m_showAllPins && m_waveformPinSelector->count() > 0)
    {
        // 仅显示当前选择的单个管脚
        QString pinName = m_waveformPinSelector->currentText();
        QString pinFullName = m_waveformPinSelector->currentData().toString();

        // 清空收集的管脚列表，只保留当前选中的管脚
        pinColumns.clear();

        // 查找当前选中管脚的列索引
        if (m_isUsingNewTableModel && m_vectorTableModel)
        {
            for (int col = 0; col < columns.size(); ++col)
            {
                if (columns[col].name == pinName && 
                    columns[col].type == Vector::ColumnDataType::PIN_STATE_ID)
                {
                    pinColumns.append(qMakePair(pinName, col));
                    qDebug() << "updateWaveformView - 在新表格模型中找到管脚" << pinName << "列索引为" << col;
                    break;
                }
            }
        } 
        else if (m_vectorTableWidget)
        {
            for (int col = 0; col < m_vectorTableWidget->columnCount(); ++col)
            {
                QTableWidgetItem *headerItem = m_vectorTableWidget->horizontalHeaderItem(col);
                if (headerItem && headerItem->text() == pinFullName)
                {
                    pinColumns.append(qMakePair(pinName, col));
                    break;
                }
            }
        }
    }

    if (pinColumns.isEmpty())
    {
        m_waveformPlot->replot();
        return;
    }

    // 获取表格数据 - 根据不同的模式
    int rowCount = 0;
    QList<Vector::RowData> allRows; // 存储所有行数据
    bool needUpdateCache = false; // 是否需要从UI更新缓存
    
    if (m_isUsingNewTableModel && m_vectorTableModel)
    {
        // 使用优化模型时，直接从模型获取行数
        rowCount = m_vectorTableModel->rowCount();
        // 注意：这里不会一次性加载所有数据，而是后续按需加载
        needUpdateCache = false;
    }
    else
    {
        // 传统模式：一次性加载所有数据
        bool ok = false;
        allRows = VectorDataHandler::instance().getAllVectorRows(currentTableId, ok);
        if (!ok)
        {
            qWarning() << "updateWaveformView - Failed to get all vector rows.";
            m_waveformPlot->replot();
            return;
        }
        rowCount = allRows.count();
        
        // [新增] 使用当前页面中已修改的、最新的数据"覆盖"从数据处理器获取的基础数据
        if (m_vectorTableWidget)
        {
            int firstRowOfPage = m_currentPage * m_pageSize;
            for (int pinIndex = 0; pinIndex < pinColumns.size(); ++pinIndex)
            {
                int pinColumnIndex = pinColumns[pinIndex].second;
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
        }
    }

    if (rowCount <= 0)
    {
        qWarning() << "updateWaveformView - No rows found.";
        m_waveformPlot->replot();
        return;
    }

    int pinCount = pinColumns.size();

    // 2.2 为每个管脚计算Y轴位置
    const double PIN_HEIGHT = 25.0;  // 每个管脚波形的高度
    const double PIN_GAP = 10.0;     // 管脚之间的间隔
    const double WAVE_HEIGHT = 20.0; // 单个波形的实际高度(高-低电平差)

    // 计算总的Y轴范围
    double totalYRange = pinCount * (PIN_HEIGHT + PIN_GAP);

    // 设置新的Y轴范围
    m_waveformPlot->yAxis->setRange(-PIN_GAP, totalYRange);

    // =======================================================================
    // 阶段 3: 获取TimeSet信息
    // =======================================================================

    // 获取当前表的ID和第一行的TimeSet信息
    int firstRowTimeSetId = getTimeSetIdForRow(currentTableId, 0); // 获取第一行的TimeSet ID
    qDebug() << "updateWaveformView - 当前表ID:" << currentTableId << ", 第一行TimeSet ID:" << firstRowTimeSetId;

    // 如果没有获取到有效的TimeSet ID，尝试其他方法获取
    if (firstRowTimeSetId <= 0)
    {
        qDebug() << "updateWaveformView - 尝试直接从数据库查询当前表的TimeSet ID";

        // 直接查询当前表的第一行数据中的TimeSet ID
        QSqlDatabase db = DatabaseManager::instance()->database();
        if (db.isOpen())
        {
            // 先获取TimeSet列的位置
            QSqlQuery colQuery(db);
            colQuery.prepare("SELECT column_order FROM VectorTableColumnConfiguration "
                             "WHERE master_record_id = ? AND column_type = 'TIMESET_ID' LIMIT 1");
            colQuery.addBindValue(currentTableId);

            if (colQuery.exec() && colQuery.next())
            {
                int timeSetColOrder = colQuery.value(0).toInt();

                if (m_isUsingNewTableModel && m_vectorTableModel && m_vectorTableModel->rowCount() > 0)
                {
                    // 从模型中获取数据
                    QModelIndex index = m_vectorTableModel->index(0, timeSetColOrder);
                    QVariant value = m_vectorTableModel->data(index);
                    if (value.isValid())
                    {
                        firstRowTimeSetId = value.toInt();
                        qDebug() << "updateWaveformView - 从模型获取到TimeSet ID:" << firstRowTimeSetId;
                    }
                }
                else
                {
                    // 传统方式：从数据库获取
                    QSqlQuery rowQuery(db);
                    rowQuery.prepare("SELECT data_json FROM vector_data WHERE table_id = ? ORDER BY row_index LIMIT 1");
                    rowQuery.addBindValue(currentTableId);

                    if (rowQuery.exec() && rowQuery.next())
                    {
                        QString dataJson = rowQuery.value(0).toString();
                        QJsonDocument doc = QJsonDocument::fromJson(dataJson.toUtf8());
                        if (doc.isArray())
                        {
                            QJsonArray array = doc.array();
                            if (timeSetColOrder < array.size())
                            {
                                firstRowTimeSetId = array.at(timeSetColOrder).toInt();
                                qDebug() << "updateWaveformView - 直接从数据库获取到TimeSet ID:" << firstRowTimeSetId;
                            }
                        }
                    }
                }
            }
        }
    }

    // 如果仍然没有获取到TimeSet ID，尝试获取任何可用的TimeSet
    if (firstRowTimeSetId <= 0)
    {
        qDebug() << "updateWaveformView - 尝试获取任何可用的TimeSet ID";

        QSqlDatabase db = DatabaseManager::instance()->database();
        if (db.isOpen())
        {
            QSqlQuery anyTSQuery(db);
            if (anyTSQuery.exec("SELECT id FROM timeset_list LIMIT 1") && anyTSQuery.next())
            {
                firstRowTimeSetId = anyTSQuery.value(0).toInt();
                qDebug() << "updateWaveformView - 获取到任意可用的TimeSet ID:" << firstRowTimeSetId;
            }
        }
    }

    // 获取周期信息
    m_currentPeriod = 1000.0; // 默认周期值
    double t1rRatio = 0.25;   // 默认比例

    // 从数据库获取周期值
    if (firstRowTimeSetId > 0)
    {
        QSqlDatabase db = DatabaseManager::instance()->database();
        if (db.isOpen())
        {
            QSqlQuery periodQuery(db);
            periodQuery.prepare("SELECT period FROM timeset_list WHERE id = ?");
            periodQuery.addBindValue(firstRowTimeSetId);

            if (periodQuery.exec() && periodQuery.next())
            {
                m_currentPeriod = periodQuery.value(0).toDouble();
                qDebug() << "updateWaveformView - 直接查询到的周期值:" << m_currentPeriod << "ns";
            }
        }
    }

    // =======================================================================
    // 阶段 3.5: 预计算所有管脚的T1R并找到最大值，用于对齐
    // =======================================================================
    double max_t1rRatio = 0.0;
    m_pinT1rRatios.clear(); // 清空缓存

    if (firstRowTimeSetId > 0 && m_currentPeriod > 0)
    {
        QSqlDatabase db = DatabaseManager::instance()->database();
        if (db.isOpen())
        {
            for (int pinIndex = 0; pinIndex < pinCount; ++pinIndex)
            {
                int pinId = getPinIdByName(pinColumns[pinIndex].first);
                if (pinId <= 0)
                    continue;

                double t1r = 0.0;
                QSqlQuery t1rQuery(db);
                t1rQuery.prepare("SELECT T1R FROM timeset_settings WHERE timeset_id = ? AND pin_id = ?");
                t1rQuery.addBindValue(firstRowTimeSetId);
                t1rQuery.addBindValue(pinId);

                if (t1rQuery.exec() && t1rQuery.next())
                {
                    t1r = t1rQuery.value(0).toDouble();
                }
                double ratio = t1r / m_currentPeriod;
                m_pinT1rRatios[pinId] = ratio; // 缓存每个管脚的T1R比例
                if (ratio > max_t1rRatio)
                {
                    max_t1rRatio = ratio;
                }
            }
        }
    }
    m_currentXOffset = max_t1rRatio;

    // =======================================================================
    // 阶段 4: 为每个管脚绘制波形
    // =======================================================================

    // 记录当前X偏移量供其他函数使用
    // m_currentXOffset = 0.0;
    for (int pinIndex = 0; pinIndex < pinCount; ++pinIndex)
    {
        QString pinName = pinColumns[pinIndex].first;
        int pinColumnIndex = pinColumns[pinIndex].second;
        int pinId = getPinIdByName(pinName);

        // --- 从缓存中获取当前管脚的T1R比例 ---
        double pin_t1rRatio = m_pinT1rRatios.value(pinId, 0.0);

        // 计算当前管脚的Y轴位置
        double pinBaseY = pinIndex * (PIN_HEIGHT + PIN_GAP); // 当前管脚的基准Y坐标
        double pinHighY = pinBaseY + PIN_HEIGHT;             // 高电平Y坐标
        double pinLowY = pinBaseY;                           // 低电平Y坐标
        double pinMidY = (pinHighY + pinLowY) / 2.0;         // 中间电平Y坐标

        // 准备数据容器
        QVector<double> xData(rowCount + 1);
        QVector<double> mainLineData(rowCount + 1);
        QVector<double> fillA_top(rowCount + 1), fillA_bottom(rowCount + 1);
        QVector<double> fillB_top(rowCount + 1), fillB_bottom(rowCount + 1);

        // 循环填充数据容器
        for (int i = 0; i < rowCount; ++i)
        {
            xData[i] = i + pin_t1rRatio; // 使用当前管脚的T1R比例作为偏移量
            
            // 根据不同模式获取单元格数据
            QString cellValue;
            if (m_isUsingNewTableModel && m_vectorTableModel)
            {
                // 从模型获取数据
                QModelIndex index = m_vectorTableModel->index(i, pinColumnIndex);
                if (index.isValid())
                {
                    cellValue = m_vectorTableModel->data(index).toString();
                    // 打印调试信息，帮助诊断
                    if (i < 5) // 只打印前几行以避免日志过多
                    {
                        qDebug() << "波形数据 - 行:" << i << ", 管脚:" << pinName 
                                 << ", 列:" << pinColumnIndex << ", 值:" << cellValue;
                    }
                }
            }
            else
            {
                // 从预加载数据获取
                cellValue = allRows[i][pinColumnIndex].toString();
            }
            
            QChar state = cellValue.isEmpty() ? 'L' : cellValue.at(0); // 默认为L（低电平）

            // 初始化
            mainLineData[i] = qQNaN();
            fillA_top[i] = fillA_bottom[i] = qQNaN();
            fillB_top[i] = fillB_bottom[i] = qQNaN();

            // 填充主线条数据
            switch (state.toLatin1())
            {
            case '1':
            case 'H':
                mainLineData[i] = pinHighY;
                break;
            case '0':
            case 'L':
                mainLineData[i] = pinLowY;
                break;
            case 'M':
            case 'X': // X也在此处获取数据以保证线条连续性
                mainLineData[i] = pinMidY;
                break;
            default: // S 和 V 没有主线条
                break;
            }

            // 填充背景数据 (所有字母状态都有背景)
            if (state.isLetter())
            {
                if (i % 2 == 0) // 偶数行蓝色
                {
                    fillA_top[i] = pinHighY;
                    fillA_bottom[i] = pinLowY;
                }
                else // 奇数行紫色
                {
                    fillB_top[i] = pinHighY;
                    fillB_bottom[i] = pinLowY;
                }
            }
        }

        // 为lsStepLeft扩展最后一个数据点
        if (rowCount > 0)
        {
            xData[rowCount] = rowCount + pin_t1rRatio; // 使用当前管脚的T1R比例作为偏移量
            mainLineData[rowCount] = mainLineData[rowCount - 1];
            fillA_top[rowCount] = fillA_top[rowCount - 1];
            fillA_bottom[rowCount] = fillA_bottom[rowCount - 1];
            fillB_top[rowCount] = fillB_top[rowCount - 1];
            fillB_bottom[rowCount] = fillB_bottom[rowCount - 1];
        }

        // 应用特殊波形类型 (NRZ, RZ, R0, SBC)
        // 为每个管脚应用其对应的波形模式
        if (firstRowTimeSetId > 0)
        {
            // 应用波形模式（NRZ, RZ, R0, SBC）
            applyWaveformPattern(firstRowTimeSetId, pinId, xData, mainLineData, pin_t1rRatio, m_currentPeriod);
        }

        // 绘制背景填充层
        QCPGraph *fillB_bottom_g = m_waveformPlot->addGraph();
        QCPGraph *fillB_top_g = m_waveformPlot->addGraph();
        QCPGraph *fillA_bottom_g = m_waveformPlot->addGraph();
        QCPGraph *fillA_top_g = m_waveformPlot->addGraph();

        // 设置图形不显示在图例中
        fillA_bottom_g->setName(QString());
        fillA_top_g->setName(QString());
        fillB_bottom_g->setName(QString());
        fillB_top_g->setName(QString());

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

        // 绘制T1R区域（为每个管脚独立绘制）
        if (pin_t1rRatio > 0 && pin_t1rRatio < 1.0)
        {
            // 创建垂直填充的两个图形
            QCPGraph *t1rFill_bottom = m_waveformPlot->addGraph();
            QCPGraph *t1rFill_top = m_waveformPlot->addGraph();

            t1rFill_bottom->setName(QString());
            t1rFill_top->setName(QString());

            // 设置T1R区域的数据点
            QVector<double> t1rX(2), t1rBottom(2), t1rTop(2);
            t1rX[0] = 0.0;          // T1R区域从X=0开始
            t1rX[1] = pin_t1rRatio; // T1R区域结束于pin_t1rRatio处
            t1rBottom[0] = t1rBottom[1] = pinLowY;
            t1rTop[0] = t1rTop[1] = pinHighY;

            // 应用蓝色交叉填充样式
            QBrush t1rBrush(QColor(0, 120, 255, 120), Qt::DiagCrossPattern);
            t1rFill_top->setPen(Qt::NoPen);
            t1rFill_bottom->setPen(Qt::NoPen);
            t1rFill_top->setBrush(t1rBrush);
            t1rFill_top->setChannelFillGraph(t1rFill_bottom);
            t1rFill_top->setData(t1rX, t1rTop);
            t1rFill_bottom->setData(t1rX, t1rBottom);

            // 添加灰色中间线
            QPen middleLinePen(Qt::gray, 1.0);
            QCPItemLine *middleLine = new QCPItemLine(m_waveformPlot);
            middleLine->setProperty("isT1RMiddleLine", true);
            middleLine->setPen(middleLinePen);
            middleLine->start->setCoords(0.0, pinMidY);        // 从X=0开始
            middleLine->end->setCoords(pin_t1rRatio, pinMidY); // 到pin_t1rRatio结束

            // 添加T1R区域与后续波形的竖线过渡
            double firstWaveY = pinLowY; // 默认值，低电平

            if (rowCount > 0)
            {
                QString firstCellValue;
                if (m_isUsingNewTableModel && m_vectorTableModel)
                {
                    // 从模型获取第一个单元格数据
                    QModelIndex index = m_vectorTableModel->index(0, pinColumnIndex);
                    firstCellValue = m_vectorTableModel->data(index).toString();
                }
                else
                {
                    // 从预加载数据获取
                    firstCellValue = allRows[0][pinColumnIndex].toString();
                }
                
                QChar firstPointState = firstCellValue.isEmpty() ? 'L' : firstCellValue.at(0);
                switch (firstPointState.toLatin1())
                {
                case '1':
                case 'H':
                    firstWaveY = pinHighY;
                    break;
                case '0':
                case 'L':
                    firstWaveY = pinLowY;
                    break;
                case 'M':
                case 'X':
                    firstWaveY = pinMidY;
                    break;
                default:
                    firstWaveY = pinLowY;
                    break;
                }
            }

            // 创建从中间线到第一个波形点的竖线过渡
            QCPItemLine *transitionLine = new QCPItemLine(m_waveformPlot);
            transitionLine->setProperty("isT1RTransition", true);
            transitionLine->setPen(QPen(Qt::gray, 1.5));
            transitionLine->start->setCoords(pin_t1rRatio, pinMidY); // 从pin_t1rRatio位置开始
            transitionLine->end->setCoords(pin_t1rRatio, firstWaveY);
        }

        // 绘制主波形线
        QCPGraph *line_g = m_waveformPlot->addGraph();
        // 为每个管脚设置不同颜色，使用HSV色彩空间均匀分布颜色
        QColor pinColor;
        if (pinCount <= 10)
        {
            // 少于10个管脚时使用预设颜色
            QList<QColor> presetColors = {
                Qt::red, Qt::blue, Qt::green, Qt::magenta, Qt::cyan,
                QColor(255, 128, 0), // 橙色
                QColor(128, 0, 255), // 紫色
                QColor(0, 128, 128), // 青绿色
                QColor(128, 128, 0), // 橄榄色
                QColor(255, 0, 128)  // 粉红色
            };
            pinColor = presetColors[pinIndex % presetColors.size()];
        }
        else
        {
            // 更多管脚时使用HSV均匀分布
            pinColor = QColor::fromHsv(pinIndex * 360 / pinCount, 255, 255);
        }

        line_g->setPen(QPen(pinColor, 2.0));
        line_g->setBrush(Qt::NoBrush);
        line_g->setName(pinName); // 设置图例显示的管脚名称
        line_g->setData(xData, mainLineData);
        line_g->setLineStyle(QCPGraph::lsStepLeft);

        // 绘制独立的样式Item和覆盖层
        for (int i = 0; i < rowCount; ++i)
        {
            // 获取单元格状态
            QString cellValue;
            if (m_isUsingNewTableModel && m_vectorTableModel)
            {
                // 从模型获取数据
                QModelIndex index = m_vectorTableModel->index(i, pinColumnIndex);
                cellValue = m_vectorTableModel->data(index).toString();
            }
            else
            {
                // 从预加载数据获取
                cellValue = allRows[i][pinColumnIndex].toString();
            }
            QChar state = cellValue.isEmpty() ? 'L' : cellValue.at(0);

            // 绘制 'V' 状态的红色方框
            if (state == 'V')
            {
                QCPItemRect *box = new QCPItemRect(m_waveformPlot);
                box->setProperty("isVBox", true);
                box->setPen(QPen(pinColor, 2.0));
                box->setBrush(Qt::NoBrush);
                box->topLeft->setCoords(i + pin_t1rRatio, pinHighY);
                box->bottomRight->setCoords(i + 1 + pin_t1rRatio, pinLowY);
            }

            // 绘制 'X' 状态的灰色中线覆盖层
            if (state == 'X')
            {
                QCPItemLine *line = new QCPItemLine(m_waveformPlot);
                line->setLayer("overlay");
                line->setProperty("isXLine", true);
                line->setPen(QPen(Qt::gray, 1.5));
                line->start->setCoords(i + pin_t1rRatio, pinMidY);
                line->end->setCoords(i + 1 + pin_t1rRatio, pinMidY);
            }

            // 绘制 '->X' 的灰色过渡线覆盖层
            if (i > 0)
            {
                QString prevCellValue;
                if (m_isUsingNewTableModel && m_vectorTableModel)
                {
                    QModelIndex prevIndex = m_vectorTableModel->index(i - 1, pinColumnIndex);
                    prevCellValue = m_vectorTableModel->data(prevIndex).toString();
                }
                else
                {
                    prevCellValue = allRows[i - 1][pinColumnIndex].toString();
                }
                
                QChar previousState = prevCellValue.isEmpty() ? 'L' : prevCellValue.at(0);
                
                if (state == 'X')
                {
                    double previousY = qQNaN();
                    if (previousState == 'V')
                    {
                        previousY = pinHighY;
                    }
                    else
                    {
                        switch (previousState.toLatin1())
                        {
                        case '1':
                        case 'H':
                            previousY = pinHighY;
                            break;
                        case '0':
                        case 'L':
                            previousY = pinLowY;
                            break;
                        case 'M':
                            previousY = pinMidY;
                            break;
                        }
                    }

                    if (!qIsNaN(previousY))
                    {
                        QCPItemLine *transitionLine = new QCPItemLine(m_waveformPlot);
                        transitionLine->setLayer("overlay");
                        transitionLine->setProperty("isXTransition", true);
                        transitionLine->setPen(QPen(Qt::gray, 1.5));
                        transitionLine->start->setCoords(i + pin_t1rRatio, previousY);
                        transitionLine->end->setCoords(i + pin_t1rRatio, pinMidY);
                    }
                }
            }
        }

        // 添加管脚标签
        QCPItemText *pinLabel = new QCPItemText(m_waveformPlot);
        pinLabel->setProperty("isPinLabel", true);
        pinLabel->setPositionAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        pinLabel->setTextAlignment(Qt::AlignLeft);
        pinLabel->position->setCoords(-0.3, pinMidY);
        pinLabel->setText(pinName);
        pinLabel->setFont(QFont("Arial", 8));
        pinLabel->setPen(QPen(pinColor));
        pinLabel->setBrush(QBrush(QColor(255, 255, 255, 180)));
        pinLabel->setPadding(QMargins(2, 1, 2, 1));
    }

    // =======================================================================
    // 阶段 5: 最终绘图设置
    // =======================================================================

    // 计算适当的X轴范围
    double xMax = rowCount > 0 ? qMin(rowCount, 40) : 10;

    // 设置初始X轴范围
    if (!hasCustomRange)
    {
        // 如果没有自定义缩放，则使用默认范围
        m_waveformPlot->xAxis->setRange(-0.5, xMax + max_t1rRatio); // 调整X轴范围，确保从-0.5开始以显示管脚标签
    }
    else
    {
        // 如果有自定义缩放，则恢复之前保存的范围
        m_waveformPlot->xAxis->setRange(savedXMin, savedXMax);
    }

    // 隐藏Y轴刻度
    m_waveformPlot->yAxis->setTickLabels(false);
    m_waveformPlot->yAxis->setSubTicks(false);

    // 设置图例位置
    m_waveformPlot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop | Qt::AlignRight);

    // 如果管脚数量超过8个，可能需要设置较小的图例字体
    if (pinCount > 8)
    {
        m_waveformPlot->legend->setFont(QFont("Arial", 7));
    }

    // 调整波形图高度以适应所有管脚
    m_waveformPlot->setMinimumHeight(qMax(200, int(totalYRange) + 50)); // 确保足够高度显示所有管脚

    // 绘制特殊波形模式（RZ, R0, SBC）
    drawWaveformPatterns();

    // 重绘
    m_waveformPlot->replot();
}

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
void MainWindow::highlightWaveformPoint(int rowIndex, int pinIndex)
{
    if (!m_waveformPlot)
        return;

    m_selectedWaveformPoint = rowIndex;

    // 清除之前的高亮项
    for (int i = m_waveformPlot->itemCount() - 1; i >= 0; i--)
    {
        if (auto item = m_waveformPlot->item(i))
        {
            if (item->property("isSelectionHighlight").toBool() || 
                item->property("isHexValueLabel").toBool())
            {
                m_waveformPlot->removeItem(item);
            }
        }
    }

    if (rowIndex < 0)
    {
        m_waveformPlot->replot();
        return;
    }

    int currentTableId = m_vectorTableSelector->currentData().toInt();
    bool ok = false;
    QList<Vector::RowData> allRows;  // 只在传统模式下填充
    
    if (!m_isUsingNewTableModel)
    {
        // 传统模式：加载所有行数据
        allRows = VectorDataHandler::instance().getAllVectorRows(currentTableId, ok);
        if (!ok || allRows.isEmpty() || rowIndex >= allRows.count())
        {
            qWarning() << "highlightWaveformPoint - 无法获取行数据或行索引超出范围";
            return;
        }
    }
    else if (m_vectorTableModel)
    {
        // 模型模式：检查行索引是否有效
        if (rowIndex >= m_vectorTableModel->rowCount())
        {
            qWarning() << "highlightWaveformPoint - 行索引超出模型行数范围";
            return;
        }
    }
    else
    {
        qWarning() << "highlightWaveformPoint - 无效的模型或模式";
        return;
    }

    // 获取要高亮的行
    Vector::RowData rowData;
    if (m_isUsingNewTableModel && m_vectorTableModel)
    {
        // 从模型获取行数据
        for (int col = 0; col < m_vectorTableModel->columnCount(); ++col)
        {
            QModelIndex index = m_vectorTableModel->index(rowIndex, col);
            rowData.append(m_vectorTableModel->data(index));
        }
    }
    else
    {
        // 从预加载的数据获取
        rowData = allRows[rowIndex];
    }

    // 获取所有管脚信息
    QList<QPair<QString, int>> pinColumns;
    QList<Vector::ColumnInfo> columns = getCurrentColumnConfiguration(currentTableId);
    
    if (m_isUsingNewTableModel && m_vectorTableModel)
    {
        // 模型模式：从列配置获取PIN_STATE列
        for (int col = 0; col < columns.size(); ++col)
        {
            if (columns[col].type == Vector::ColumnDataType::PIN_STATE_ID)
            {
                pinColumns.append(qMakePair(columns[col].name, col));
            }
        }
    }
    else if (m_vectorTableWidget)
    {
        // 传统模式：从表格表头获取PIN_STATE列
        for (int col = 0; col < m_vectorTableWidget->columnCount(); ++col)
        {
            QTableWidgetItem *headerItem = m_vectorTableWidget->horizontalHeaderItem(col);
            if (headerItem)
            {
                QString headerText = headerItem->text();
                if (col < columns.size() && columns[col].type == Vector::ColumnDataType::PIN_STATE_ID)
                {
                    QString displayName = headerText.split('\n').first();
                    pinColumns.append(qMakePair(displayName, col));
                }
            }
        }
    }

    // 如果没有指定特定的管脚，高亮所有管脚
    if (pinIndex < 0 || pinIndex >= pinColumns.size())
    {
        for (int i = 0; i < pinColumns.size(); ++i)
        {
            addHighlightForPin(rowIndex, i, pinColumns, rowData);
        }
    }
    else
    {
        // 只高亮指定的管脚
        addHighlightForPin(rowIndex, pinIndex, pinColumns, rowData);
    }

    // 重绘波形图
    m_waveformPlot->replot();
}

// 添加高亮效果
void MainWindow::addHighlightForPin(int rowIndex, int pinIndex, const QList<QPair<QString, int>> &pinColumns, const Vector::RowData &rowData)
{
    if (pinIndex < 0 || pinIndex >= pinColumns.size() || !m_waveformPlot)
        return;

    // 获取管脚列信息
    QString pinName = pinColumns[pinIndex].first;
    int pinColumnIndex = pinColumns[pinIndex].second;

    // 检查列索引是否合法
    if (pinColumnIndex < 0 || pinColumnIndex >= rowData.size())
    {
        qWarning() << "addHighlightForPin - 列索引超出范围:" << pinColumnIndex << ", 行数据大小:" << rowData.size();
        return;
    }

    // 获取管脚ID
    int pinId = getPinIdByName(pinName);
    double pin_t1rRatio = m_pinT1rRatios.value(pinId, 0.0);

    // --- 从缓存中获取当前管脚的T1R比例 ---
    const double PIN_HEIGHT = 25.0;  // 每个管脚波形的高度
    const double PIN_GAP = 10.0;     // 管脚之间的间隔

    // 计算当前管脚的Y轴位置
    double pinBaseY = pinIndex * (PIN_HEIGHT + PIN_GAP); // 当前管脚的基准Y坐标
    double pinHighY = pinBaseY + PIN_HEIGHT;             // 高电平Y坐标
    double pinLowY = pinBaseY;                           // 低电平Y坐标
    double pinMidY = (pinHighY + pinLowY) / 2.0;         // 中间电平Y坐标

    // 获取该点的状态
    QVariant cellData = rowData[pinColumnIndex];
    QString cellStr = cellData.toString();
    QChar state = cellStr.isEmpty() ? 'L' : cellStr.at(0);

    // 确定高亮点的Y坐标（根据其状态）
    double highlightY = pinLowY; // 默认为低电平
    switch (state.toLatin1())
    {
    case '1':
    case 'H':
        highlightY = pinHighY;
        break;
    case '0':
    case 'L':
        highlightY = pinLowY;
        break;
    case 'M':
    case 'X':
        highlightY = pinMidY;
        break;
    }

    // 创建高亮点
    QCPItemEllipse *highlight = new QCPItemEllipse(m_waveformPlot);
    highlight->setProperty("isSelectionHighlight", true);
    highlight->setPen(QPen(Qt::red, 1.5));
    highlight->setBrush(QBrush(QColor(255, 0, 0, 70)));

    // 设置选中点的坐标
    double xCoord = rowIndex + pin_t1rRatio;
    double ellipseSize = 0.4; // 高亮点大小
    highlight->topLeft->setCoords(xCoord - ellipseSize / 2, highlightY + ellipseSize / 2);
    highlight->bottomRight->setCoords(xCoord + ellipseSize / 2, highlightY - ellipseSize / 2);

    // 添加16进制值标签
    if (cellStr.length() >= 2)
    {
        QString hexValue = cellStr.mid(1);
        if (!hexValue.isEmpty())
        {
            QCPItemText *hexLabel = new QCPItemText(m_waveformPlot);
            hexLabel->setProperty("isHexValueLabel", true);
            hexLabel->setPositionAlignment(Qt::AlignBottom | Qt::AlignHCenter);
            hexLabel->position->setCoords(xCoord, highlightY + ellipseSize);
            hexLabel->setText("0x" + hexValue);
            hexLabel->setFont(QFont("Arial", 8));
            hexLabel->setPen(QPen(Qt::black));
        }
    }
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
    if (!m_waveformPlot || !m_waveformValueEditor || !m_vectorTableWidget)
        return;

    // 获取编辑器中的值
    QString newValue = m_waveformValueEditor->text().trimmed();
    m_waveformValueEditor->hide();

    if (newValue.isEmpty())
        return;

    // 确保值转换为大写
    newValue = newValue.toUpper();

    // 验证输入值 - 只接受有效的PIN_STATE值
    QChar firstChar = newValue.at(0);
    if (!QString("01LHMXSV").contains(firstChar))
    {
        qWarning() << "onWaveformValueEdited - 无效的波形值:" << newValue;
        QMessageBox::warning(this, tr("无效输入"), tr("请输入有效的引脚状态值: 0, 1, L, H, M, X, S, V"));
        return;
    }

    // 如果点未选中，无法编辑
    if (m_selectedWaveformPoint < 0)
        return;

    // 获取当前选中的点的行索引
    int rowIndex = m_selectedWaveformPoint;
    qDebug() << "onWaveformValueEdited - 编辑行" << rowIndex << "的值为" << newValue;

    // 获取表格中首个选中的管脚列
    int pinColumnIndex = -1;
    for (int col = 0; col < m_vectorTableWidget->columnCount(); ++col)
    {
        if (m_vectorTableWidget->horizontalHeaderItem(col))
        {
            QString headerText = m_vectorTableWidget->horizontalHeaderItem(col)->text();
            QString pinName = m_waveformPinSelector->currentText();

            // 检查列是否是当前选中的管脚
            if (headerText.startsWith(pinName + "\n") || headerText == pinName)
            {
                pinColumnIndex = col;
                break;
            }
        }
    }

    if (pinColumnIndex < 0)
    {
        qWarning() << "onWaveformValueEdited - 未找到匹配的管脚列";
        return;
    }

    // 计算当前页码的实际行索引（考虑分页）
    int actualRowIndex = rowIndex;

    // 更新数据表格中的对应单元格
    int tableRow = actualRowIndex - (m_currentPage * m_pageSize);
    if (tableRow >= 0 && tableRow < m_vectorTableWidget->rowCount())
    {
        // 更新当前可见页面中的表格单元格
        if (m_isUsingNewTableModel && m_vectorTableModel)
        {
            // 使用模型API更新数据
            QModelIndex cellIndex = m_vectorTableModel->index(actualRowIndex, pinColumnIndex);
            m_vectorTableModel->setData(cellIndex, newValue, Qt::EditRole);
        }
        else
        {
            // 传统表格更新
            QTableWidgetItem *item = m_vectorTableWidget->item(tableRow, pinColumnIndex);
            if (item)
            {
                item->setText(newValue);
            }
            else
            {
                item = new QTableWidgetItem(newValue);
                m_vectorTableWidget->setItem(tableRow, pinColumnIndex, item);
            }
            
            // 记录行已修改，以便后续保存
            onTableRowModified(tableRow);
        }
    }
    else
    {
        // 行不在当前可见范围内，需要直接更新二进制文件
        qDebug() << "onWaveformValueEdited - 行不在当前页范围内，将直接更新数据源";
        
        // 获取当前表ID
        int currentTableId = m_vectorTableSelector->currentData().toInt();
        
        if (m_isUsingNewTableModel && m_vectorTableModel)
        {
            // 使用模型API更新数据，即使行不可见也是一样的处理方式
            QModelIndex cellIndex = m_vectorTableModel->index(actualRowIndex, pinColumnIndex);
            m_vectorTableModel->setData(cellIndex, newValue, Qt::EditRole);
            qDebug() << "onWaveformValueEdited - 使用模型API更新不可见行数据";
        }
        else
        {
            // 传统模式：获取该行的数据并直接更新
            Vector::RowData rowData;
            if (VectorDataHandler::instance().fetchRowData(currentTableId, actualRowIndex, rowData))
            {
                // 直接使用pinColumnIndex作为列索引，这是之前已经获取的管脚列索引
                if (pinColumnIndex >= 0)
                {
                    // 更新数据
                    if (!VectorDataHandler::instance().updateCellData(currentTableId, actualRowIndex, pinColumnIndex, newValue))
                    {
                        QMessageBox::warning(this, tr("更新失败"), tr("无法更新波形数据"));
                        return;
                    }
                    
                    // 注意：不需要更新UI，因为这行不在可见范围内
                }
                else
                {
                    qWarning() << "onWaveformValueEdited - 无效的列索引";
                    return;
                }
            }
            else
            {
                qWarning() << "onWaveformValueEdited - 获取行数据失败";
                return;
            }
        }
    }

    // 刷新波形图
    updateWaveformView();

    // 重新高亮选中的点
    highlightWaveformPoint(rowIndex, -1);
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