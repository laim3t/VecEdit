// 更新状态栏中的窗口大小信息
void MainWindow::updateWindowSizeInfo()
{
    if (m_windowSizeLabel)
    {
        QSize currentSize = size();
        QString sizeInfo = tr("窗口大小: %1 x %2").arg(currentSize.width()).arg(currentSize.height());
        m_windowSizeLabel->setText(sizeInfo);
        qDebug() << "MainWindow::updateWindowSizeInfo - " << sizeInfo;
    }
}

// 拦截关闭事件，保存窗口状态
void MainWindow::closeEvent(QCloseEvent *event)
{
    // 如果有未保存的内容，提示用户保存
    if (m_hasUnsavedChanges)
    {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            "保存修改",
            "当前有未保存的内容，是否保存？",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

        if (reply == QMessageBox::Yes)
        {
            // 保存内容
            saveVectorTableData();

            // 如果保存后仍有未保存内容（可能保存失败），则取消关闭
            if (m_hasUnsavedChanges)
            {
                event->ignore();
                return;
            }
        }
        else if (reply == QMessageBox::Cancel)
        {
            // 用户取消关闭
            event->ignore();
            return;
        }
        // 如果是No，则不保存直接关闭，继续执行
    }

    saveWindowState();
    QMainWindow::closeEvent(event);
}

// 保存窗口状态
void MainWindow::saveWindowState()
{
    // 将窗口状态保存到配置文件
    QSettings settings(QDir::homePath() + "/.vecedit/settings.ini", QSettings::IniFormat);

    // 保存窗口几何形状
    settings.setValue("MainWindow/geometry", saveGeometry());

    // 保存窗口状态（工具栏、停靠窗口等）
    settings.setValue("MainWindow/windowState", QMainWindow::saveState());

    // 保存分页大小
    settings.setValue("MainWindow/pageSize", m_pageSize);

    // 保存向量表选择
    if (m_vectorTableSelector && m_vectorTableSelector->count() > 0)
    {
        settings.setValue("MainWindow/lastSelectedVectorTable", m_vectorTableSelector->currentText());
    }

    // 保存侧边栏状态
    if (m_sidebarDock)
    {
        settings.setValue("MainWindow/sidebarVisible", m_sidebarDock->isVisible());
        settings.setValue("MainWindow/sidebarDocked", !m_sidebarDock->isFloating());
        settings.setValue("MainWindow/sidebarArea", (int)dockWidgetArea(m_sidebarDock));
    }

    // 保存向量列属性栏状态
    if (m_columnPropertiesDock)
    {
        settings.setValue("MainWindow/columnPropertiesVisible", m_columnPropertiesDock->isVisible());
        settings.setValue("MainWindow/columnPropertiesDocked", !m_columnPropertiesDock->isFloating());
        settings.setValue("MainWindow/columnPropertiesArea", (int)dockWidgetArea(m_columnPropertiesDock));
    }

    // 保存波形图停靠窗口状态
    if (m_waveformDock)
    {
        settings.setValue("MainWindow/waveformVisible", m_waveformDock->isVisible());
        settings.setValue("MainWindow/waveformDocked", !m_waveformDock->isFloating());
        settings.setValue("MainWindow/waveformArea", (int)dockWidgetArea(m_waveformDock));
    }
}

// 恢复窗口状态
void MainWindow::restoreWindowState()
{
    QSettings settings(QDir::homePath() + "/.vecedit/settings.ini", QSettings::IniFormat);

    // 恢复窗口几何形状
    if (settings.contains("MainWindow/geometry"))
    {
        restoreGeometry(settings.value("MainWindow/geometry").toByteArray());
    }

    // 恢复窗口状态
    if (settings.contains("MainWindow/windowState"))
    {
        QMainWindow::restoreState(settings.value("MainWindow/windowState").toByteArray());
    }

    // 恢复分页大小
    if (settings.contains("MainWindow/pageSize"))
    {
        int savedPageSize = settings.value("MainWindow/pageSize").toInt();
        if (savedPageSize > 0 && savedPageSize != m_pageSize)
        {
            // 确保页面大小选择器已初始化
            if (m_pageSizeSelector)
            {
                for (int i = 0; i < m_pageSizeSelector->count(); ++i)
                {
                    if (m_pageSizeSelector->itemData(i).toInt() == savedPageSize)
                    {
                        m_pageSizeSelector->setCurrentIndex(i);
                        break;
                    }
                }
            }

            m_pageSize = savedPageSize;
        }
    }

    // 恢复向量表选择
    if (settings.contains("MainWindow/lastSelectedVectorTable") && m_vectorTableSelector && m_vectorTableSelector->count() > 0)
    {
        QString lastTable = settings.value("MainWindow/lastSelectedVectorTable").toString();
        int index = m_vectorTableSelector->findText(lastTable);
        if (index >= 0)
        {
            m_vectorTableSelector->setCurrentIndex(index);
        }
    }

    // 恢复侧边栏状态（如果QMainWindow::restoreState没有处理）
    if (m_sidebarDock)
    {
        if (settings.contains("MainWindow/sidebarVisible"))
        {
            bool visible = settings.value("MainWindow/sidebarVisible").toBool();
            m_sidebarDock->setVisible(visible);
        }

        if (settings.contains("MainWindow/sidebarDocked") && settings.contains("MainWindow/sidebarArea"))
        {
            bool docked = settings.value("MainWindow/sidebarDocked").toBool();
            if (!docked)
            {
                m_sidebarDock->setFloating(true);
            }
            else
            {
                Qt::DockWidgetArea area = (Qt::DockWidgetArea)settings.value("MainWindow/sidebarArea").toInt();
                addDockWidget(area, m_sidebarDock);
            }
        }
    }

    // 恢复向量列属性栏状态（如果QMainWindow::restoreState没有处理）
    if (m_columnPropertiesDock)
    {
        if (settings.contains("MainWindow/columnPropertiesVisible"))
        {
            bool visible = settings.value("MainWindow/columnPropertiesVisible").toBool();
            m_columnPropertiesDock->setVisible(visible);
        }

        if (settings.contains("MainWindow/columnPropertiesDocked") && settings.contains("MainWindow/columnPropertiesArea"))
        {
            bool docked = settings.value("MainWindow/columnPropertiesDocked").toBool();
            if (!docked)
            {
                m_columnPropertiesDock->setFloating(true);
            }
            else
            {
                Qt::DockWidgetArea area = (Qt::DockWidgetArea)settings.value("MainWindow/columnPropertiesArea").toInt();
                addDockWidget(area, m_columnPropertiesDock);
            }
        }
    }

    // 恢复波形图停靠窗口状态
    if (m_waveformDock)
    {
        if (settings.contains("MainWindow/waveformVisible"))
        {
            bool visible = settings.value("MainWindow/waveformVisible").toBool();
            m_waveformDock->setVisible(visible);
        }

        if (settings.contains("MainWindow/waveformDocked") && settings.contains("MainWindow/waveformArea"))
        {
            bool docked = settings.value("MainWindow/waveformDocked").toBool();
            if (!docked)
            {
                m_waveformDock->setFloating(true);
            }
            else
            {
                Qt::DockWidgetArea area = (Qt::DockWidgetArea)settings.value("MainWindow/waveformArea").toInt();
                addDockWidget(area, m_waveformDock);
            }
        }
    }
}

void MainWindow::updateMenuState()
{
    bool projectOpen = !m_currentDbPath.isEmpty();
    m_newProjectAction->setEnabled(!projectOpen);
    m_openProjectAction->setEnabled(!projectOpen);
    m_closeProjectAction->setEnabled(projectOpen);
}

// 创建水平分隔线的辅助函数
QFrame *MainWindow::createHorizontalSeparator()
{
    QFrame *separator = new QFrame();
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    separator->setMaximumHeight(1);                                        // 使分隔线更细
    separator->setStyleSheet("background-color: #CCCCCC; margin: 6px 0;"); // 浅灰色分隔线
    return separator;
}

// 设置向量列属性栏
void MainWindow::setupVectorColumnPropertiesBar()
{
    // 创建右侧停靠的向量列属性栏
    m_columnPropertiesDock = new QDockWidget(tr("向量列属性栏"), this);
    m_columnPropertiesDock->setObjectName("columnPropertiesDock");
    m_columnPropertiesDock->setAllowedAreas(Qt::RightDockWidgetArea);
    m_columnPropertiesDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);

    // 添加边框线样式
    m_columnPropertiesDock->setStyleSheet(
        "QDockWidget {"
        "   border: 1px solid #A0A0A0;"
        "}"
        "QDockWidget::title {"
        "   background-color: #E1E1E1;"
        "   padding-left: 5px;"
        "   border-bottom: 1px solid #A0A0A0;"
        "}");

    // 创建属性栏内容容器
    m_columnPropertiesWidget = new QWidget(m_columnPropertiesDock);
    m_columnPropertiesWidget->setObjectName("columnPropertiesWidget");
    QVBoxLayout *propertiesLayout = new QVBoxLayout(m_columnPropertiesWidget);
    propertiesLayout->setSpacing(10);                     // 调整控件之间的垂直间距
    propertiesLayout->setContentsMargins(12, 12, 12, 12); // 调整与窗口边缘的间距

    // 设置属性栏样式
    QString propertiesStyle = R"(
        QWidget#columnPropertiesWidget {
            background-color: #F0F0F0;
            border: 1px solid #A0A0A0;
            margin: 0px;
        }
        QLabel {
            font-weight: bold;
            padding-top: 3px; /* 上方添加内边距使标签垂直居中 */
        }
        QLabel[objectName="titleLabel"] {
            background-color: #E1E1E1;
            border-bottom: 1px solid #A0A0A0;
        }
        QLineEdit {
            padding: 3px 6px; /* 适当内部填充 */
            border: 1px solid #A0A0A0;
            background-color: white;
            border-radius: 3px; /* 适当圆角 */
            min-height: 26px; /* 适当高度 */
            margin: 2px; /* 适当外边距 */
        }
        /* 表单布局中的QLabel */
        QFormLayout > QLabel {
            margin-left: 2px; /* 左侧添加边距 */
        }
    )";
    m_columnPropertiesWidget->setStyleSheet(propertiesStyle);

    // 设置最小宽度
    m_columnPropertiesWidget->setMinimumWidth(200);

    // 移除标题，根据用户要求统一界面

    // 列名称区域
    QLabel *columnNameTitle = new QLabel(tr("列名称:"));
    m_columnNamePinLabel = new QLabel("");
    m_columnNamePinLabel->setStyleSheet("font-weight: normal;");
    QHBoxLayout *columnNameLayout = new QHBoxLayout();
    columnNameLayout->addWidget(columnNameTitle);
    columnNameLayout->addWidget(m_columnNamePinLabel);
    columnNameLayout->addStretch(); // 添加弹簧，将左侧控件和右侧控件分开

    // 添加连续勾选框到右侧
    m_continuousSelectCheckBox = new QCheckBox(tr("连续"));
    m_continuousSelectCheckBox->setChecked(false); // 默认不勾选
    m_continuousSelectCheckBox->setToolTip(tr("勾选后，16进制值回车将选择后续8个连续单元格"));
    columnNameLayout->addWidget(m_continuousSelectCheckBox);

    propertiesLayout->addLayout(columnNameLayout);

    // 在表格中以16进制显示提示
    QLabel *hexDisplayLabel = new QLabel(tr("在表格中以16进制显示"));
    hexDisplayLabel->setStyleSheet("font-weight: normal; color: #666666;");
    propertiesLayout->addWidget(hexDisplayLabel);

    // 添加第一条水平分隔线
    propertiesLayout->addWidget(createHorizontalSeparator());

    // 使用表单布局替代网格布局，实现更好的对齐效果
    QFormLayout *formLayout = new QFormLayout();
    formLayout->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint); // 防止字段自动拉伸
    formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);     // 标签左对齐且垂直居中
    formLayout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);          // 表单整体左对齐和顶部对齐
    formLayout->setHorizontalSpacing(12);                                // 标签和字段之间水平间距
    formLayout->setVerticalSpacing(12);                                  // 行之间的垂直间距

    // 管脚
    QLabel *pinLabel = new QLabel(tr("管脚"));
    m_pinNameLabel = new QLabel(tr(""));
    m_pinNameLabel->setStyleSheet("font-weight: normal;");
    formLayout->addRow(pinLabel, m_pinNameLabel);

    // 添加小型分隔线（仅用于表单内的分隔）
    QFrame *miniSeparator = createHorizontalSeparator();
    miniSeparator->setStyleSheet("background-color: #E0E0E0; margin: 3px 0;"); // 更淡的颜色和更小的边距
    formLayout->addRow("", miniSeparator);

    // 16进制值
    QLabel *hexValueLabel = new QLabel(tr("16进制值"));

    // 创建带有描述的标签
    QWidget *hexLabelWithDesc = new QWidget();
    QVBoxLayout *hexLabelLayout = new QVBoxLayout(hexLabelWithDesc);
    hexLabelLayout->setSpacing(2);
    hexLabelLayout->setContentsMargins(0, 0, 0, 0);

    hexLabelLayout->addWidget(hexValueLabel);

    QLabel *hexValueDesc = new QLabel(tr("(串行1的8行数据)"));
    hexValueDesc->setStyleSheet("font-weight: normal; font-size: 9pt; color: #666666;");
    hexLabelLayout->addWidget(hexValueDesc);

    m_pinValueField = new QLineEdit();
    m_pinValueField->setReadOnly(false); // 改为可编辑
    m_pinValueField->setPlaceholderText(tr("输入16进制值"));
    // 不设置maxLength，让显示可以超过6个字符
    // 在验证函数中会限制用户输入不超过6个字符
    m_pinValueField->setFixedWidth(120);                               // 调整到合适宽度
    m_pinValueField->setMinimumHeight(26);                             // 调整高度使布局更协调
    m_pinValueField->setProperty("invalid", false);                    // 初始状态：有效
    m_pinValueField->setToolTipDuration(5000);                         // 设置提示显示时间为5秒
    m_pinValueField->setStyleSheet("QLineEdit { padding: 3px 6px; }"); // 调整内边距

    // 将标签和输入框添加到表单布局
    formLayout->addRow(hexLabelWithDesc, m_pinValueField);

    // 添加小型分隔线
    QFrame *miniSeparator2 = createHorizontalSeparator();
    miniSeparator2->setStyleSheet("background-color: #E0E0E0; margin: 3px 0;"); // 更淡的颜色和更小的边距
    formLayout->addRow("", miniSeparator2);

    // 连接编辑完成信号
    connect(m_pinValueField, &QLineEdit::editingFinished, this, &MainWindow::onHexValueEdited);

    // 为新视图创建相同的连接（会根据当前活动的视图自动选择）
    connect(m_pinValueField, &QLineEdit::editingFinished, [this]()
            {
        // 如果当前正在显示新视图，则调用新视图的处理函数
        if (m_vectorStackedWidget && m_vectorStackedWidget->currentIndex() == 1) {
            this->onHexValueEditedForModel();
        } });

    // 连接文本变化信号，实时验证输入
    connect(m_pinValueField, &QLineEdit::textChanged, this, &MainWindow::validateHexInput);

    // 错误个数
    QLabel *errorCountLabel = new QLabel(tr("错误个数"));
    m_errorCountField = new QLineEdit(""); // 初始为空，不设置默认值
    m_errorCountField->setReadOnly(true);
    m_errorCountField->setFixedWidth(120);   // 与16进制值文本框保持相同宽度
    m_errorCountField->setMinimumHeight(26); // 与16进制值文本框保持一致的高度
    m_errorCountField->setPlaceholderText(tr("错误数量"));
    m_errorCountField->setStyleSheet("QLineEdit { padding: 3px 6px; }"); // 与16进制值文本框保持一致的样式

    // 添加到表单布局
    formLayout->addRow(errorCountLabel, m_errorCountField);

    // 添加表单布局到主布局
    propertiesLayout->addLayout(formLayout);

    // 添加水平分隔线
    propertiesLayout->addWidget(createHorizontalSeparator());

    // 添加占位空间
    propertiesLayout->addStretch();

    // 将容器设置为停靠部件的内容
    m_columnPropertiesDock->setWidget(m_columnPropertiesWidget);

    // 将属性栏添加到主窗口的右侧
    addDockWidget(Qt::RightDockWidgetArea, m_columnPropertiesDock);
}