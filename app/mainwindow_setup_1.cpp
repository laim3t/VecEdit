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
#include "vector/vectortablemodel.h"
#include "timeset/timesetdialog.h"
#include "database/databasemanager.h"
#include "common/logger.h"

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

    // 创建工具栏
    QToolBar *toolBar = new QToolBar(this);
    toolBar->setObjectName("vectorTableToolBar");
    toolBar->setMovable(false);
    toolBar->setIconSize(QSize(24, 24));

    // 创建向量表选择下拉框
    m_vectorTableSelector = new QComboBox(this);
    m_vectorTableSelector->setObjectName("m_vectorTableSelector");
    m_vectorTableSelector->setMinimumWidth(200);
    connect(m_vectorTableSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onVectorTableSelectionChanged);

    // 创建新增向量表按钮
    QAction *newTableAction = new QAction(QIcon(":/icons/plus-circle.svg"), "新增向量表", this);
    connect(newTableAction, &QAction::triggered, this, &MainWindow::addNewVectorTable);
    toolBar->addAction(newTableAction);
    if (!newTableAction->associatedWidgets().isEmpty())
    {
        QToolButton *button = qobject_cast<QToolButton *>(newTableAction->associatedWidgets().first());
        if (button)
            button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    }

    // 添加向量表选择器到工具栏
    toolBar->addWidget(m_vectorTableSelector);

    // 创建删除向量表按钮
    QAction *deleteTableAction = new QAction(QIcon(":/icons/x-circle.svg"), "删除向量表", this);
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

    // 创建表格视图（旧版QTableWidget，保留一段时间用于兼容）
    m_vectorTableWidget = new QTableWidget(this);
    m_vectorTableWidget->setAlternatingRowColors(true);
    m_vectorTableWidget->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_vectorTableWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_vectorTableWidget->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_vectorTableWidget->horizontalHeader()->setStretchLastSection(true);
    m_vectorTableWidget->verticalHeader()->setDefaultSectionSize(25);
    m_vectorTableWidget->verticalHeader()->setVisible(true);

    // 设置表格右键菜单和信号/槽连接
    m_vectorTableWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_vectorTableWidget, &QTableWidget::customContextMenuRequested,
            this, &MainWindow::showPinColumnContextMenu);

    // 监听单元格变更事件
    connect(m_vectorTableWidget, &QTableWidget::cellChanged, this, &MainWindow::onTableCellChanged);

    // 单元格点击时更新向量列属性栏
    connect(m_vectorTableWidget, &QTableWidget::cellClicked, this, &MainWindow::updateVectorColumnProperties);

    // 连接自定义单元格编辑器的值修改信号 (使用于PinValueLineEdit等)
    // 创建自定义委托（旧版，用于QTableWidget）
    m_itemDelegate = new VectorTableItemDelegate(this);
    m_vectorTableWidget->setItemDelegate(m_itemDelegate);

    // 创建新的视图（基于模型/视图架构的QTableView）
    m_vectorTableView = new QTableView(this);
    m_vectorTableView->setVisible(false); // 默认隐藏，在需要时显示
    m_vectorTableView->setAlternatingRowColors(true);
    m_vectorTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_vectorTableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_vectorTableView->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_vectorTableView->horizontalHeader()->setStretchLastSection(true);
    m_vectorTableView->verticalHeader()->setDefaultSectionSize(25);
    m_vectorTableView->verticalHeader()->setVisible(true);
    m_vectorTableView->setItemDelegate(m_itemDelegate); // 重用委托
    m_vectorTableView->setContextMenuPolicy(Qt::CustomContextMenu);

    // 创建模型
    m_vectorTableModel = new VectorTableModel(this);

    // 连接模型数据变更信号
    connect(m_vectorTableModel, &QAbstractItemModel::dataChanged, this, &MainWindow::onModelDataChanged);

    // 连接选择变更信号
    connect(m_vectorTableView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &MainWindow::updateVectorColumnPropertiesModel);

    // 分页控件
    QWidget *paginationWidget = new QWidget(this);
    QHBoxLayout *paginationLayout = new QHBoxLayout(paginationWidget);
    paginationLayout->setContentsMargins(5, 2, 5, 2);

    m_prevPageButton = new QPushButton("上一页", paginationWidget);
    m_nextPageButton = new QPushButton("下一页", paginationWidget);
    m_paginationInfoLabel = new QLabel("页码: 0/0 (总行数: 0)", paginationWidget);
    QLabel *pageSizeLabel = new QLabel("每页行数:", paginationWidget);
    m_pageSizeSelector = new QComboBox(paginationWidget);
    m_pageSizeSelector->addItems(QStringList() << "100" << "200" << "500" << "1000");
    m_pageSizeSelector->setCurrentText("200");
    QLabel *jumpToPageLabel = new QLabel("跳转到:", paginationWidget);
    m_pageJumpSpinBox = new QSpinBox(paginationWidget);
    m_pageJumpSpinBox->setMinimum(1);
    m_pageJumpSpinBox->setMaximum(1);
    QPushButton *jumpButton = new QPushButton("跳转", paginationWidget);

    paginationLayout->addWidget(m_prevPageButton);
    paginationLayout->addWidget(m_paginationInfoLabel);
    paginationLayout->addWidget(m_nextPageButton);
    paginationLayout->addSpacing(20);
    paginationLayout->addWidget(pageSizeLabel);
    paginationLayout->addWidget(m_pageSizeSelector);
    paginationLayout->addSpacing(20);
    paginationLayout->addWidget(jumpToPageLabel);
    paginationLayout->addWidget(m_pageJumpSpinBox);
    paginationLayout->addWidget(jumpButton);
    paginationLayout->addStretch();

    // 连接分页控件信号
    connect(m_prevPageButton, &QPushButton::clicked, this, &MainWindow::loadPrevPage);
    connect(m_nextPageButton, &QPushButton::clicked, this, &MainWindow::loadNextPage);
    connect(m_pageSizeSelector, &QComboBox::currentTextChanged, [this](const QString &text)
            {
        int newSize = text.toInt();
        if (newSize > 0)
        {
            changePageSize(newSize);
        } });
    connect(jumpButton, &QPushButton::clicked, [this]()
            {
                int pageNum = m_pageJumpSpinBox->value();
                jumpToPage(pageNum - 1); // 转换为0-based索引
            });

    // 设置分页初始值
    m_pageSize = 200;
    m_currentPage = 0;
    m_totalRows = 0;
    m_totalPages = 0;

    // 将表格控件放入布局中
    containerLayout->addWidget(m_vectorTableWidget);
    containerLayout->addWidget(m_vectorTableView);
    containerLayout->addWidget(paginationWidget);
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
