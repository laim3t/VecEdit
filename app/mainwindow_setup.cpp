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
    : QMainWindow(parent), m_isUpdatingUI(false), m_currentHexValueColumn(-1)
{
    setupUI();
    setupMenu();
    setupSidebarNavigator();          // 添加侧边导航栏设置
    setupVectorColumnPropertiesBar(); // 添加向量列属性栏设置

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

    // 创建表格视图
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
    containerLayout->addWidget(m_vectorTableWidget);
    containerLayout->addWidget(m_paginationWidget); // 添加分页控件
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
    m_sidebarDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    // 创建树形控件
    m_sidebarTree = new QTreeWidget(m_sidebarDock);
    m_sidebarTree->setHeaderLabel(tr("项目组件"));
    m_sidebarTree->setColumnCount(1);
    m_sidebarTree->setIconSize(QSize(16, 16));

    // 设置树形控件样式
    QString treeStyle = R"(
        QTreeWidget {
            background-color: #F0F0F0;
            border: 1px solid #A0A0A0;
            font-size: 11pt;
        }
        QTreeWidget::item {
            padding: 3px 0px;
        }
        QTreeWidget::item:selected {
            background-color: #C0C0C0;
        }
    )";
    m_sidebarTree->setStyleSheet(treeStyle);

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

    // 将树形控件设置为停靠部件的内容
    m_sidebarDock->setWidget(m_sidebarTree);

    // 将侧边栏添加到主窗口的左侧
    addDockWidget(Qt::LeftDockWidgetArea, m_sidebarDock);
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
}

void MainWindow::updateMenuState()
{
    bool projectOpen = !m_currentDbPath.isEmpty();
    m_newProjectAction->setEnabled(!projectOpen);
    m_openProjectAction->setEnabled(!projectOpen);
    m_closeProjectAction->setEnabled(projectOpen);
}

// 设置向量列属性栏
void MainWindow::setupVectorColumnPropertiesBar()
{
    // 创建右侧停靠的向量列属性栏
    m_columnPropertiesDock = new QDockWidget(tr("向量列属性栏"), this);
    m_columnPropertiesDock->setObjectName("columnPropertiesDock");
    m_columnPropertiesDock->setAllowedAreas(Qt::RightDockWidgetArea);
    m_columnPropertiesDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

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
    propertiesLayout->setSpacing(10);
    propertiesLayout->setContentsMargins(10, 10, 10, 10);

    // 设置属性栏样式
    QString propertiesStyle = R"(
        QWidget#columnPropertiesWidget {
            background-color: #F0F0F0;
            border: 1px solid #A0A0A0;
            margin: 0px;
        }
        QLabel {
            font-weight: bold;
        }
        QLabel[objectName="titleLabel"] {
            background-color: #E1E1E1;
            border-bottom: 1px solid #A0A0A0;
        }
        QLineEdit {
            padding: 4px;
            border: 1px solid #A0A0A0;
            background-color: white;
            border-radius: 2px;
            min-height: 22px;
            margin: 1px;
        }
    )";
    m_columnPropertiesWidget->setStyleSheet(propertiesStyle);

    // 设置最小宽度
    m_columnPropertiesWidget->setMinimumWidth(200);

    // 添加标题
    QLabel *titleLabel = new QLabel(tr("向量列属性"), m_columnPropertiesWidget);
    titleLabel->setObjectName("titleLabel");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 14pt; padding: 10px;");
    propertiesLayout->addWidget(titleLabel);

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

    // 管脚值区域
    QGridLayout *pinLayout = new QGridLayout();
    pinLayout->setColumnStretch(1, 1);
    pinLayout->setAlignment(Qt::AlignLeft);
    pinLayout->setHorizontalSpacing(10);
    pinLayout->setVerticalSpacing(10);

    // 管脚
    QLabel *pinLabel = new QLabel(tr("管脚"));
    m_pinNameLabel = new QLabel(tr(""));
    m_pinNameLabel->setStyleSheet("font-weight: normal;");
    pinLayout->addWidget(pinLabel, 0, 0);
    pinLayout->addWidget(m_pinNameLabel, 0, 1);

    // 16进制值
    QLabel *hexValueLabel = new QLabel(tr("16进制值"));
    m_pinValueField = new QLineEdit();
    m_pinValueField->setReadOnly(false); // 改为可编辑
    m_pinValueField->setPlaceholderText(tr("输入16进制值"));
    // 不设置maxLength，让显示可以超过6个字符
    // 在验证函数中会限制用户输入不超过6个字符
    m_pinValueField->setFixedWidth(200);            // 设置更宽的固定宽度以便能显示8个选中单元格的内容
    m_pinValueField->setProperty("invalid", false); // 初始状态：有效
    m_pinValueField->setToolTipDuration(5000);      // 设置提示显示时间为5秒
    pinLayout->addWidget(hexValueLabel, 1, 0);
    pinLayout->addWidget(m_pinValueField, 1, 1);

    // 连接编辑完成信号
    connect(m_pinValueField, &QLineEdit::editingFinished, this, &MainWindow::onHexValueEdited);

    // 连接文本变化信号，实时验证输入
    connect(m_pinValueField, &QLineEdit::textChanged, this, &MainWindow::validateHexInput);

    // 错误个数
    QLabel *errorCountLabel = new QLabel(tr("错误个数"));
    m_errorCountField = new QLineEdit(""); // 初始为空，不设置默认值
    m_errorCountField->setReadOnly(true);
    m_errorCountField->setFixedWidth(200); // 与16进制值文本框保持相同宽度
    m_errorCountField->setPlaceholderText(tr("错误数量"));
    pinLayout->addWidget(errorCountLabel, 2, 0);
    pinLayout->addWidget(m_errorCountField, 2, 1);

    propertiesLayout->addLayout(pinLayout);

    // 添加占位空间
    propertiesLayout->addStretch();

    // 将容器设置为停靠部件的内容
    m_columnPropertiesDock->setWidget(m_columnPropertiesWidget);

    // 将属性栏添加到主窗口的右侧
    addDockWidget(Qt::RightDockWidgetArea, m_columnPropertiesDock);
}
