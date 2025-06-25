// ==========================================================
//  Headers for: mainwindow_setup.cpp
// ==========================================================
#include "app/mainwindow.h"

// Qt Widgets & Core
#include <QtWidgets>

// Project-specific headers
#include "common/dialogmanager.h"
#include "common/tablestylemanager.h"
#include "vector/vectortabledelegate.h"
#include "pin/pinvalueedit.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_isUpdatingUI(false), m_currentHexValueColumn(-1), m_hasUnsavedChanges(false)
{
    setupUI();
    setupSidebarNavigator();          // 必须在 setupMenu() 之前调用，因为它初始化了 m_sidebarDock
    setupVectorColumnPropertiesBar(); // 必须在 setupMenu() 之前调用，因为它初始化了 m_columnPropertiesDock
    setupWaveformView();              // 必须在 setupMenu() 之前调用，因为它初始化了 m_waveformDock
    setupMenu();

    // 创建对话框管理器
    m_dialogManager = new DialogManager(this);

    // 设置窗口标题和大小
    setWindowTitle("向量编辑器");

    // 初始窗口大小（如果没有保存的状态）
    resize(1024, 768);

    // 初始化窗口大小信息标签
    m_windowSizeLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_windowSizeLabel);
    updateWindowSizeInfo();

    // 显示就绪状态
    statusBar()->showMessage("就绪");

    // 启用自由缩放窗口
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(800, 600); // 设置窗口最小尺寸

    // 恢复上次的窗口状态
    restoreWindowState();

    // 初始化菜单状态
    updateMenuState();

    // 连接窗口大小变化信号，当窗口大小改变时，更新管脚列宽度
    connect(this, &MainWindow::windowResized, [this]()
            {
        if (m_vectorTableWidget && m_vectorTableWidget->isVisible() && m_vectorTableWidget->columnCount() > 6) {
            qDebug() << "MainWindow - 窗口大小改变，调整管脚列宽度";
            TableStyleManager::setPinColumnWidths(m_vectorTableWidget);
        } });
}

MainWindow::~MainWindow()
{
    // Qt对象会自动清理
    closeCurrentProject();

    // 释放我们创建的对象
    if (m_dialogManager)
        delete m_dialogManager;
}

void MainWindow::setupUI()
{
    // 创建一个中央窗口小部件
    m_centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(m_centralWidget);

    // 创建欢迎页面
    m_welcomeWidget = new QWidget(this);
    QVBoxLayout *welcomeLayout = new QVBoxLayout(m_welcomeWidget);

    // 添加欢迎标签
    QLabel *welcomeLabel = new QLabel(tr("欢迎使用向量编辑器"), m_welcomeWidget);
    welcomeLabel->setAlignment(Qt::AlignCenter);
    welcomeLabel->setStyleSheet("font-size: 20pt; font-weight: bold; margin: 20px;");

    // 添加说明标签
    QLabel *instructionLabel = new QLabel(tr("请使用\"文件\"菜单创建或打开项目，然后通过\"查看\"菜单查看数据表"), m_welcomeWidget);
    instructionLabel->setAlignment(Qt::AlignCenter);
    instructionLabel->setStyleSheet("font-size: 14pt; margin: 10px;");

    // 将标签添加到欢迎页面布局
    welcomeLayout->addStretch();
    welcomeLayout->addWidget(welcomeLabel);
    welcomeLayout->addWidget(instructionLabel);
    welcomeLayout->addStretch();

    // 创建向量表显示界面
    setupVectorTableUI();

    // 初始显示欢迎页面
    mainLayout->addWidget(m_welcomeWidget);
    mainLayout->addWidget(m_vectorTableContainer);
    m_vectorTableContainer->setVisible(false);

    setCentralWidget(m_centralWidget);
}

void MainWindow::setupMenu()
{
    // 创建文件菜单
    QMenu *fileMenu = menuBar()->addMenu(tr("文件(&F)"));

    // 新建项目
    m_newProjectAction = fileMenu->addAction(tr("新建项目(&N)"));
    connect(m_newProjectAction, &QAction::triggered, this, &MainWindow::createNewProject);

    // 打开项目
    m_openProjectAction = fileMenu->addAction(tr("打开项目(&O)"));
    connect(m_openProjectAction, &QAction::triggered, this, &MainWindow::openExistingProject);

    // 分隔符
    fileMenu->addSeparator();

    // 关闭项目
    m_closeProjectAction = fileMenu->addAction(tr("关闭项目(&C)"));
    connect(m_closeProjectAction, &QAction::triggered, this, &MainWindow::closeCurrentProject);

    // 分隔符
    fileMenu->addSeparator();

    // 退出
    QAction *exitAction = fileMenu->addAction(tr("退出(&Q)"));
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    // 创建查看菜单
    QMenu *viewMenu = menuBar()->addMenu(tr("查看(&V)"));

    // 查看数据库
    QAction *viewDatabaseAction = viewMenu->addAction(tr("查看数据库(&D)"));
    connect(viewDatabaseAction, &QAction::triggered, this, &MainWindow::showDatabaseViewDialog);

    // 分隔符
    viewMenu->addSeparator();

    // 窗口控制
    QAction *maximizeAction = viewMenu->addAction(tr("窗口最大化(&M)"));
    connect(maximizeAction, &QAction::triggered, [this]()
            {
        qDebug() << "MainWindow - 窗口最大化操作触发";
        if (isMaximized()) {
            showNormal();
            qDebug() << "MainWindow - 恢复窗口正常大小";
        } else {
            showMaximized();
            qDebug() << "MainWindow - 窗口已最大化";
        } });

    // 设置窗口大小菜单项
    QAction *setSizeAction = viewMenu->addAction(tr("设置窗口大小(&S)"));
    connect(setSizeAction, &QAction::triggered, [this]()
            {
        qDebug() << "MainWindow - 触发设置窗口大小操作";
        
        // 获取当前窗口大小
        QSize currentSize = size();
        
        // 创建对话框
        QDialog dialog(this);
        dialog.setWindowTitle(tr("设置窗口大小"));
        
        // 创建表单布局
        QFormLayout *layout = new QFormLayout(&dialog);
        
        // 添加宽度和高度输入框
        QSpinBox *widthBox = new QSpinBox(&dialog);
        widthBox->setRange(800, 3840);  // 设置合理的范围
        widthBox->setValue(currentSize.width());
        widthBox->setSuffix(" px");
        
        QSpinBox *heightBox = new QSpinBox(&dialog);
        heightBox->setRange(600, 2160);  // 设置合理的范围
        heightBox->setValue(currentSize.height());
        heightBox->setSuffix(" px");
        
        layout->addRow(tr("宽度:"), widthBox);
        layout->addRow(tr("高度:"), heightBox);
        
        // 添加按钮
        QDialogButtonBox *buttonBox = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
            Qt::Horizontal, &dialog);
        layout->addRow(buttonBox);
        
        // 连接按钮信号
        connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        
        // 显示对话框
        if (dialog.exec() == QDialog::Accepted) {
            // 应用新的窗口大小
            int width = widthBox->value();
            int height = heightBox->value();
            qDebug() << "MainWindow - 设置窗口大小为: " << width << "x" << height;
            
            // 如果窗口是最大化状态，先恢复正常
            if (isMaximized()) {
                showNormal();
            }
            
            // 设置新的窗口大小
            resize(width, height);
        } });

    // 创建"视图"菜单 (用于控制Dock部件的显示/隐藏)
    m_viewMenu = menuBar()->addMenu(tr("视图(&I)"));
    QAction *toggleSidebarAction = m_sidebarDock->toggleViewAction();
    toggleSidebarAction->setText(tr("导航栏"));
    m_viewMenu->addAction(toggleSidebarAction);

    QAction *togglePropertiesAction = m_columnPropertiesDock->toggleViewAction();
    togglePropertiesAction->setText(tr("向量列属性"));
    m_viewMenu->addAction(togglePropertiesAction);

    // 添加波形图视图切换项
    m_viewMenu->addSeparator();
    m_toggleWaveformAction = m_waveformDock->toggleViewAction();
    m_toggleWaveformAction->setText(tr("波形图视图"));
    m_viewMenu->addAction(m_toggleWaveformAction);
}

void MainWindow::setupVectorTableUI()
{
    // 创建向量表容器
    m_vectorTableContainer = new QWidget(this);
    QVBoxLayout *containerLayout = new QVBoxLayout(m_vectorTableContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);

    // 创建顶部工具栏
    QToolBar *toolBar = new QToolBar(this);
    toolBar->setObjectName("vectorTableToolBar"); // 给工具栏一个对象名，方便查找
    toolBar->setIconSize(QSize(18, 18));
    toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon); // 默认样式
    toolBar->setStyleSheet("QToolBar { spacing: 1px; } QToolButton { padding: 2px; font-size: 9pt; }");
    toolBar->setMovable(false);

    // 刷新按钮
    m_refreshAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_BrowserReload), tr("刷新"), this);
    connect(m_refreshAction, &QAction::triggered, this, &MainWindow::refreshVectorTableData);
    toolBar->addAction(m_refreshAction);

    // 保存按钮
    QAction *saveAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_DialogSaveButton), tr("保存"), this);
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveVectorTableData);
    toolBar->addAction(saveAction);

    // 新建向量表按钮
    QAction *addTableAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_FileIcon), tr("新建向量表"), this);
    connect(addTableAction, &QAction::triggered, this, &MainWindow::addNewVectorTable);
    toolBar->addAction(addTableAction);

    toolBar->addSeparator();

    // 向量表选择下拉框 (保持 QWidget 嵌入 QToolBar)
    QLabel *selectLabel = new QLabel(tr("选择向量表:"), this);
    m_vectorTableSelector = new QComboBox(this);
    m_vectorTableSelector->setObjectName(QStringLiteral("m_vectorTableSelector"));
    m_vectorTableSelector->setMinimumWidth(150);

    QWidget *selectorWidget = new QWidget(toolBar);
    QHBoxLayout *selectorLayout = new QHBoxLayout(selectorWidget);
    selectorLayout->setContentsMargins(5, 0, 5, 0);
    selectorLayout->setSpacing(5);
    selectorLayout->addWidget(selectLabel);
    selectorLayout->addWidget(m_vectorTableSelector);
    toolBar->addWidget(selectorWidget); // QComboBox 作为 widget 添加

    toolBar->addSeparator();

    // 设置向量表管脚按钮
    m_setupPinsAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_FileDialogDetailedView), tr("设置向量表"), this);
    connect(m_setupPinsAction, &QAction::triggered, this, &MainWindow::setupVectorTablePins);
    toolBar->addAction(m_setupPinsAction);

    // 管脚设置按钮
    m_pinSettingsAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_FileDialogContentsView), tr("管脚设置"), this);
    connect(m_pinSettingsAction, &QAction::triggered, this, &MainWindow::openPinSettingsDialog);
    toolBar->addAction(m_pinSettingsAction);

    // 添加"添加管脚"按钮
    m_addPinAction = new QAction(QIcon(":/resources/icons/plus-circle.svg"), tr("添加管脚"), this);
    connect(m_addPinAction, &QAction::triggered, this, &MainWindow::addSinglePin);
    toolBar->addAction(m_addPinAction);

    // 添加"删除管脚"按钮
    m_deletePinAction = new QAction(QIcon(":/resources/icons/x-circle.svg"), tr("删除管脚"), this);
    connect(m_deletePinAction, &QAction::triggered, this, &MainWindow::deletePins);
    toolBar->addAction(m_deletePinAction);

    // 添加"添加分组"按钮
    m_addGroupAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_DialogYesToAllButton), tr("添加分组"), this);
    connect(m_addGroupAction, &QAction::triggered, this, &MainWindow::showPinGroupDialog);
    toolBar->addAction(m_addGroupAction);

    toolBar->addSeparator();

    // TimeSet设置按钮
    m_timeSetSettingsAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_ComputerIcon), tr("TimeSet设置"), this);
    connect(m_timeSetSettingsAction, &QAction::triggered, this, &MainWindow::openTimeSetSettingsDialog);
    toolBar->addAction(m_timeSetSettingsAction);

    // 向量填充按钮
    m_fillVectorAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_FileDialogListView), tr("向量填充"), this);
    connect(m_fillVectorAction, &QAction::triggered, this, &MainWindow::showFillVectorDialog);
    toolBar->addAction(m_fillVectorAction);

    // 填充TimeSet按钮
    m_fillTimeSetAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_ArrowRight), tr("填充TimeSet"), this);
    connect(m_fillTimeSetAction, &QAction::triggered, this, &MainWindow::showFillTimeSetDialog);
    toolBar->addAction(m_fillTimeSetAction);

    // 替换TimeSet按钮
    m_replaceTimeSetAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_BrowserReload), tr("替换TimeSet"), this); // 图标可能需要调整，原SP_BrowserReload与刷新重复
    connect(m_replaceTimeSetAction, &QAction::triggered, this, &MainWindow::showReplaceTimeSetDialog);
    toolBar->addAction(m_replaceTimeSetAction);

    toolBar->addSeparator();

    // 添加向量行按钮
    QAction *addRowAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_FileDialogNewFolder), tr("添加向量行"), this);
    connect(addRowAction, &QAction::triggered, this, &MainWindow::addRowToCurrentVectorTable);
    toolBar->addAction(addRowAction);

    // 删除向量行按钮 - 只显示图标模式
    QAction *deleteRowAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_DialogDiscardButton), tr("删除向量行"), this);
    deleteRowAction->setToolTip(tr("删除向量行"));
    connect(deleteRowAction, &QAction::triggered, this, &MainWindow::deleteSelectedVectorRows);
    toolBar->addAction(deleteRowAction);
    // 设置永久IconOnly
    if (!deleteRowAction->associatedWidgets().isEmpty())
    {
        QToolButton *button = qobject_cast<QToolButton *>(deleteRowAction->associatedWidgets().first());
        if (button)
            button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    }

    // 删除指定范围内的向量行按钮 - 只显示图标模式
    m_deleteRangeAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_TrashIcon), tr("删除范围"), this);
    m_deleteRangeAction->setToolTip(tr("删除范围"));
    connect(m_deleteRangeAction, &QAction::triggered, this, &MainWindow::deleteVectorRowsInRange);
    toolBar->addAction(m_deleteRangeAction);
    if (!m_deleteRangeAction->associatedWidgets().isEmpty())
    {
        QToolButton *button = qobject_cast<QToolButton *>(m_deleteRangeAction->associatedWidgets().first());
        if (button)
            button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    }

    // 跳转到行按钮 - 只显示图标模式
    m_gotoLineAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_ArrowForward), tr("跳转到行"), this);
    m_gotoLineAction->setToolTip(tr("跳转到行"));
    connect(m_gotoLineAction, &QAction::triggered, this, &MainWindow::gotoLine);
    toolBar->addAction(m_gotoLineAction);
    if (!m_gotoLineAction->associatedWidgets().isEmpty())
    {
        QToolButton *button = qobject_cast<QToolButton *>(m_gotoLineAction->associatedWidgets().first());
        if (button)
            button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    }

    toolBar->addSeparator();

    // 删除向量表按钮 - 只显示图标模式
    QAction *deleteTableAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_DialogCancelButton), tr("删除向量表"), this);
    deleteTableAction->setToolTip(tr("删除向量表"));
    connect(deleteTableAction, &QAction::triggered, this, &MainWindow::deleteCurrentVectorTable);
    toolBar->addAction(deleteTableAction);
    if (!deleteTableAction->associatedWidgets().isEmpty())
    {
        QToolButton *button = qobject_cast<QToolButton *>(deleteTableAction->associatedWidgets().first());
        if (button)
            button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    }

    // 将工具栏添加到容器布局的顶部
    containerLayout->addWidget(toolBar);

    // 创建旧的表格视图
    m_vectorTableWidget = new QTableWidget(this);
    m_vectorTableWidget->setAlternatingRowColors(true);
    m_vectorTableWidget->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_vectorTableWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_vectorTableWidget->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_vectorTableWidget->horizontalHeader()->setStretchLastSection(true);
    m_vectorTableWidget->verticalHeader()->setDefaultSectionSize(25);
    m_vectorTableWidget->verticalHeader()->setVisible(true);

    // 创建新的表格视图模型和视图
    m_vectorTableModel = new VectorTableModel(this);
    m_vectorTableView = new QTableView(this);
    m_vectorTableView->setAlternatingRowColors(true);
    m_vectorTableView->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_vectorTableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_vectorTableView->setModel(m_vectorTableModel);

    // 创建QStackedWidget来容纳旧表格和新表格
    m_vectorStackedWidget = new QStackedWidget(this);
    m_vectorStackedWidget->addWidget(m_vectorTableWidget); // 索引0 - 旧表格
    m_vectorStackedWidget->addWidget(m_vectorTableView);   // 索引1 - 新表格

    // 添加"切换视图"按钮到工具栏
    QAction *toggleViewAction = new QAction(tr("切换新旧视图"), this);
    toolBar->addAction(toggleViewAction);
    m_toggleTableViewAction = toggleViewAction;

    // 连接"切换视图"按钮的信号
    connect(toggleViewAction, &QAction::triggered, [this]()
            {
        // 切换当前显示的视图
        int nextIndex = (m_vectorStackedWidget->currentIndex() + 1) % 2;
        m_vectorStackedWidget->setCurrentIndex(nextIndex); });

    // 设置表格右键菜单和信号/槽连接
    m_vectorTableWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_vectorTableWidget, &QTableWidget::customContextMenuRequested,
            this, &MainWindow::showPinColumnContextMenu);

    // 监听单元格变更事件
    connect(m_vectorTableWidget, &QTableWidget::cellChanged, this, &MainWindow::onTableCellChanged);

    // 单元格点击时更新向量列属性栏
    connect(m_vectorTableWidget, &QTableWidget::cellClicked, this, &MainWindow::updateVectorColumnProperties);

    // 连接自定义单元格编辑器的值修改信号 (使用于PinValueLineEdit等)
    connect(m_vectorTableWidget, &QTableWidget::cellWidget, [this](int row, int column)
            {
        QWidget *widget = m_vectorTableWidget->cellWidget(row, column);
        if (PinValueLineEdit *pinEdit = qobject_cast<PinValueLineEdit*>(widget)) {
            connect(pinEdit, &PinValueLineEdit::textChanged, [this, row]() {
                onTableRowModified(row);
            });
        } });

    // 添加选择变更事件处理，更新16进制值
    connect(m_vectorTableWidget->selectionModel(), &QItemSelectionModel::selectionChanged,
            [this](const QItemSelection &selected, const QItemSelection &deselected)
            {
                // 获取当前选中的列
                QModelIndexList selectedIndexes = m_vectorTableWidget->selectionModel()->selectedIndexes();
                if (selectedIndexes.isEmpty())
                    return;

                // 检查是否所有选择都在同一列
                int column = selectedIndexes.first().column();
                bool sameColumn = true;

                for (const QModelIndex &idx : selectedIndexes)
                {
                    if (idx.column() != column)
                    {
                        sameColumn = false;
                        break;
                    }
                }

                // 只有当所有选择都在同一列时才更新16进制值
                if (sameColumn)
                {
                    QList<int> selectedRows;
                    for (const QModelIndex &idx : selectedIndexes)
                    {
                        if (!selectedRows.contains(idx.row()))
                            selectedRows.append(idx.row());
                    }

                    // 获取当前表的列配置信息
                    if (m_vectorTabWidget && m_vectorTabWidget->currentIndex() >= 0)
                    {
                        int currentTableId = m_tabToTableId[m_vectorTabWidget->currentIndex()];
                        QList<Vector::ColumnInfo> columns = getCurrentColumnConfiguration(currentTableId);

                        // 检查列索引是否有效
                        if (column >= 0 && column < columns.size())
                        {
                            // 获取列类型
                            Vector::ColumnDataType colType = columns[column].type;

                            // 只处理管脚列
                            if (colType == Vector::ColumnDataType::PIN_STATE_ID)
                            {
                                // 更新管脚名称和16进制值
                                updateVectorColumnProperties(selectedRows.first(), column);
                            }
                        }
                    }
                }
            });

    // 连接向量表选择器信号
    connect(m_vectorTableSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onVectorTableSelectionChanged);

    // 创建项委托，处理单元格编辑
    m_itemDelegate = new VectorTableItemDelegate(this);
    m_vectorTableWidget->setItemDelegate(m_itemDelegate);

    // 创建分页控件
    m_paginationWidget = new QWidget(this);
    QHBoxLayout *paginationLayout = new QHBoxLayout(m_paginationWidget);
    paginationLayout->setContentsMargins(5, 5, 5, 5);

    // 上一页按钮
    m_prevPageButton = new QPushButton(tr("上一页"), this);
    m_prevPageButton->setFixedWidth(80);
    connect(m_prevPageButton, &QPushButton::clicked, this, &MainWindow::loadPrevPage);
    paginationLayout->addWidget(m_prevPageButton);

    // 页码信息标签
    m_pageInfoLabel = new QLabel(tr("第 0/0 页，共 0 行"), this);
    paginationLayout->addWidget(m_pageInfoLabel);

    // 下一页按钮
    m_nextPageButton = new QPushButton(tr("下一页"), this);
    m_nextPageButton->setFixedWidth(80);
    connect(m_nextPageButton, &QPushButton::clicked, this, &MainWindow::loadNextPage);
    paginationLayout->addWidget(m_nextPageButton);

    // 每页行数选择
    QLabel *pageSizeLabel = new QLabel(tr("每页行数:"), this);
    paginationLayout->addWidget(pageSizeLabel);

    m_pageSizeSelector = new QComboBox(this);
    m_pageSizeSelector->addItem("100", 100);
    m_pageSizeSelector->addItem("500", 500);
    m_pageSizeSelector->addItem("1000", 1000);
    m_pageSizeSelector->addItem("5000", 5000);
    m_pageSizeSelector->setCurrentIndex(0);
    connect(m_pageSizeSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this](int index)
            {
                int newPageSize = m_pageSizeSelector->itemData(index).toInt();
                this->changePageSize(newPageSize);
            });
    paginationLayout->addWidget(m_pageSizeSelector);

    // 页码跳转
    QLabel *jumpLabel = new QLabel(tr("跳转到:"), this);
    paginationLayout->addWidget(jumpLabel);

    m_pageJumper = new QSpinBox(this);
    m_pageJumper->setMinimum(1);
    m_pageJumper->setMaximum(1);
    m_pageJumper->setFixedWidth(60);
    paginationLayout->addWidget(m_pageJumper);

    m_jumpButton = new QPushButton(tr("确定"), this);
    m_jumpButton->setFixedWidth(50);
    connect(m_jumpButton, &QPushButton::clicked, [this]()
            {
        int pageNum = m_pageJumper->value() - 1; // 转换为0-based索引
        this->jumpToPage(pageNum); });
    paginationLayout->addWidget(m_jumpButton);

    // 添加伸缩项
    paginationLayout->addStretch();

    // 初始化分页相关变量
    m_currentPage = 0;
    m_pageSize = 100; // 默认每页100行
    m_totalPages = 0;
    m_totalRows = 0;

    // 创建Tab栏
    setupTabBar();

    // 将布局添加到容器
    containerLayout->addWidget(m_vectorStackedWidget); // 使用堆栈窗口替代直接添加表格
    containerLayout->addWidget(m_paginationWidget);    // 添加分页控件
    containerLayout->addWidget(m_vectorTabWidget);

    // 将容器添加到主布局
    QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout *>(m_centralWidget->layout());
    if (mainLayout)
    {
        mainLayout->addWidget(m_vectorTableContainer);
    }
}

void MainWindow::setupTabBar()
{
    qDebug() << "MainWindow::setupTabBar() - 初始化Tab栏";
    m_vectorTabWidget = new QTabWidget(this);

    // 设置Tab不可关闭，移除关闭按钮
    m_vectorTabWidget->setTabsClosable(false);

    // 设置早期Windows风格的Tab栏样式
    QString tabStyle = R"(
        QTabWidget::pane { 
            border: 1px solid #A0A0A0;
            background: #F0F0F0;
        }
        QTabBar::tab {
            background: #E0E0E0;
            border: 1px solid #A0A0A0;
            border-bottom-color: #A0A0A0;
            border-top-left-radius: 2px;
            border-top-right-radius: 2px;
            min-width: 80px;
            padding: 4px 8px;
            margin-right: 1px;
            color: #000000;
            font-size: 12px;
        }
        QTabBar::tab:selected {
            background: #F0F0F0;
            border-bottom-color: #F0F0F0;
            margin-top: 0px;
        }
        QTabBar::tab:!selected {
            margin-top: 2px;
            background: #D5D5D5;
        }
        QTabBar::tab:hover:!selected {
            background: #E5E5E5;
        }
    )";

    m_vectorTabWidget->setStyleSheet(tabStyle);

    // 设置Tab栏的高度
    m_vectorTabWidget->setMinimumHeight(35);
    m_vectorTabWidget->setMaximumHeight(35);

    // 连接Tab切换的信号和槽
    connect(m_vectorTabWidget, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);

    qDebug() << "MainWindow::setupTabBar() - Tab栏初始化完成";
}

// 设置侧边导航栏
void MainWindow::setupSidebarNavigator()
{
    // 创建侧边停靠导航栏
    m_sidebarDock = new QDockWidget(tr("导航栏"), this);
    m_sidebarDock->setObjectName("sidebarDock");
    m_sidebarDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_sidebarDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);

    // 添加边框线样式，与向量列属性栏保持一致
    m_sidebarDock->setStyleSheet(
        "QDockWidget {"
        "   border: 1px solid #A0A0A0;"
        "}"
        "QDockWidget::title {"
        "   background-color: #E1E1E1;"
        "   padding-left: 5px;"
        "   border-bottom: 1px solid #A0A0A0;"
        "}");

    // 创建导航栏内容容器
    QWidget *sidebarContainer = new QWidget(m_sidebarDock);
    sidebarContainer->setObjectName("sidebarContainer");
    QVBoxLayout *sidebarLayout = new QVBoxLayout(sidebarContainer);
    sidebarLayout->setSpacing(10);
    sidebarLayout->setContentsMargins(10, 10, 10, 10);

    // 设置容器样式，与向量列属性栏保持一致
    QString containerStyle = R"(
        QWidget#sidebarContainer {
            background-color: #F0F0F0;
            border: 1px solid #A0A0A0;
            margin: 0px;
        }
    )";
    sidebarContainer->setStyleSheet(containerStyle);

    // 移除标题，根据用户要求统一界面

    // 创建树形控件
    m_sidebarTree = new QTreeWidget(sidebarContainer);
    m_sidebarTree->setHeaderLabel(tr("项目组件"));
    m_sidebarTree->setColumnCount(1);
    m_sidebarTree->setIconSize(QSize(16, 16));
    m_sidebarTree->setHeaderHidden(true); // 隐藏头部，使样式更简洁

    // 设置树形控件样式
    QString treeStyle = R"(
        QTreeWidget {
            background-color: #F0F0F0;
            border: none;
            font-size: 11pt;
        }
        QTreeWidget::item {
            padding: 5px 0px;
        }
        QTreeWidget::item:selected {
            background-color: #C0C0C0;
        }
    )";
    m_sidebarTree->setStyleSheet(treeStyle);

    sidebarLayout->addWidget(m_sidebarTree);

    // 创建根节点
    QTreeWidgetItem *pinRoot = new QTreeWidgetItem(m_sidebarTree);
    pinRoot->setText(0, tr("管脚"));
    pinRoot->setIcon(0, QApplication::style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    pinRoot->setData(0, Qt::UserRole, "pins");

    QTreeWidgetItem *timeSetRoot = new QTreeWidgetItem(m_sidebarTree);
    timeSetRoot->setText(0, tr("TimeSets"));
    timeSetRoot->setIcon(0, QApplication::style()->standardIcon(QStyle::SP_ComputerIcon));
    timeSetRoot->setData(0, Qt::UserRole, "timesets");

    QTreeWidgetItem *vectorTableRoot = new QTreeWidgetItem(m_sidebarTree);
    vectorTableRoot->setText(0, tr("向量表"));
    vectorTableRoot->setIcon(0, QApplication::style()->standardIcon(QStyle::SP_FileDialogListView));
    vectorTableRoot->setData(0, Qt::UserRole, "vectortables");

    QTreeWidgetItem *labelRoot = new QTreeWidgetItem(m_sidebarTree);
    labelRoot->setText(0, tr("标签"));
    labelRoot->setIcon(0, QApplication::style()->standardIcon(QStyle::SP_FileDialogInfoView));
    labelRoot->setData(0, Qt::UserRole, "labels");

    // 连接信号槽
    connect(m_sidebarTree, &QTreeWidget::itemClicked, this, &MainWindow::onSidebarItemClicked);

    // 添加占位空间
    sidebarLayout->addStretch();

    // 将容器设置为停靠部件的内容
    m_sidebarDock->setWidget(sidebarContainer);

    // 将侧边栏添加到主窗口的左侧
    addDockWidget(Qt::LeftDockWidgetArea, m_sidebarDock);

    // 设置最小宽度，与向量列属性栏保持一致
    sidebarContainer->setMinimumWidth(200);
}

void MainWindow::resetSidebarNavigator()
{
    if (!m_sidebarTree)
        return;

    for (int i = 0; i < m_sidebarTree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *topItem = m_sidebarTree->topLevelItem(i);
        qDeleteAll(topItem->takeChildren());
        topItem->setExpanded(false);
    }
}

// 添加一个resizeEvent重写，以便发出windowResized信号
void MainWindow::resizeEvent(QResizeEvent *event)
{
    // 调用父类方法
    QMainWindow::resizeEvent(event);

    // 记录窗口大小变化
    qDebug() << "MainWindow::resizeEvent - 窗口大小变化: "
             << event->size().width() << "x" << event->size().height()
             << " (旧尺寸: " << event->oldSize().width() << "x" << event->oldSize().height() << ")";

    // 如果窗口大小发生实质性变化，才更新UI
    if (event->size() != event->oldSize())
    {
        // 更新窗口大小信息
        updateWindowSizeInfo();

        // 根据窗口宽度动态调整工具栏按钮显示模式
        if (m_vectorTableContainer && m_vectorTableContainer->isVisible())
        {
            // 获取工具栏 (可以通过对象名查找)
            QToolBar *toolBar = m_vectorTableContainer->findChild<QToolBar *>("vectorTableToolBar");
            if (toolBar)
            {
                // 如果窗口宽度小于阈值 (例如1000，可以根据实际按钮数量和宽度调整)
                // 增加这个阈值，以便更早地切换到IconOnly模式
                if (event->size().width() < 1200) // 调整后的阈值
                {
                    toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
                }
                else
                {
                    // 恢复为图标+文本模式
                    toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
                    // 对于已明确设置为 ToolButtonIconOnly 的按钮，此设置不会覆盖它们
                    // 因为它们的样式是直接在 QToolButton 对象上设置的。
                    // QToolBar::setToolButtonStyle 只是一个默认值。
                    // 如果想让所有按钮都恢复，需要遍历并重设。
                    // 但当前逻辑是：全局切换，特定按钮保持IconOnly。
                    // 因此，这里的代码可以保持原样，或者移除下面的特定按钮检查。
                    // 为了确保永久IconOnly的按钮不受影响，并且其他按钮能正确切换，
                    // 我们依赖于在 setupVectorTableUI 中对特定按钮的 QToolButton 实例设置的样式。
                }
            }
        }

        // 确保向量表容器适应新窗口尺寸
        if (m_vectorTableWidget && m_vectorTableWidget->isVisible())
        {
            qDebug() << "MainWindow::resizeEvent - 调整向量表大小以适应窗口";

            // 刷新表格布局
            m_vectorTableWidget->updateGeometry();

            // 如果表格有列，调整列宽
            if (m_vectorTableWidget->columnCount() > 6)
            {
                TableStyleManager::setPinColumnWidths(m_vectorTableWidget);
            }
        }

        // 通知窗口尺寸变化
        emit windowResized();
    }
}
