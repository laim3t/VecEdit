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
    : QMainWindow(parent), m_isUpdatingUI(false), m_currentHexValueColumn(-1), m_hasUnsavedChanges(false),
      m_vectorTableModel(nullptr), m_isUsingNewTableModel(false)
{
    setupUI();
    setupSidebarNavigator();          // 必须在 setupMenu() 之前调用，因为它初始化了 m_sidebarDock
    setupVectorColumnPropertiesBar(); // 必须在 setupMenu() 之前调用，因为它初始化了 m_columnPropertiesDock
    setupWaveformView();              // 必须在 setupMenu() 之前调用，因为它初始化了 m_waveformDock
    setupVectorTableView();           // 初始化表格视图
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
    QMenu *docksMenu = viewMenu->addMenu(tr("面板(&P)"));

    // 添加侧边栏导航面板的切换动作
    QAction *toggleNavigatorAction = m_sidebarDock->toggleViewAction();
    toggleNavigatorAction->setText(tr("显示项目导航面板(&N)"));
    docksMenu->addAction(toggleNavigatorAction);

    // 添加向量列属性面板的切换动作
    QAction *togglePropertiesAction = m_columnPropertiesDock->toggleViewAction();
    togglePropertiesAction->setText(tr("显示向量列属性面板(&P)"));
    docksMenu->addAction(togglePropertiesAction);

    // 添加波形图面板的切换动作
    QAction *toggleWaveformAction = m_waveformDock->toggleViewAction();
    toggleWaveformAction->setText(tr("显示波形图面板(&W)"));
    docksMenu->addAction(toggleWaveformAction);
    
    // 分割线
    viewMenu->addSeparator();
    
    // 添加表格模式切换选项
    QAction *toggleTableModeAction = viewMenu->addAction(tr("使用优化表格模型(&T)"));
    toggleTableModeAction->setCheckable(true);
    toggleTableModeAction->setChecked(m_isUsingNewTableModel);
    connect(toggleTableModeAction, &QAction::toggled, [this](bool checked) {
        qDebug() << "MainWindow - 表格模式切换:" << (checked ? "新表格模型" : "传统表格控件");
        
        // 保存当前选择的表ID
        int currentTableId = -1;
        if (m_vectorTableSelector->currentIndex() >= 0) {
            currentTableId = m_vectorTableSelector->currentData().toInt();
        }
        
        // 更改表格模式
        m_isUsingNewTableModel = checked;
        
        // 同步视图状态
        syncViewWithTableModel();
        
        // 如果有打开的表格，重新加载
        if (currentTableId > 0 && m_vectorTableSelector->currentIndex() >= 0) {
            onVectorTableSelectionChanged(m_vectorTableSelector->currentIndex());
            
            if (checked) {
                QMessageBox::information(this, "表格模式已切换", 
                    "已切换到优化的表格模型 (VectorTableModel)。\n\n"
                    "此模式下表格可以处理数百万行数据而不会导致内存溢出或UI卡顿。\n"
                    "性能优化来自于按需加载策略和行缓存机制。");
            } else {
                QMessageBox::information(this, "表格模式已切换", 
                    "已切换到传统表格控件 (QTableWidget)。\n\n"
                    "注意：此模式下大型表格可能会出现性能问题。");
            }
        }
    });
}

void MainWindow::setupVectorTableUI()
{
    qDebug() << "MainWindow::setupVectorTableUI() - 初始化向量表UI";

    // 创建向量表容器
    m_vectorTableContainer = new QWidget(this);
    QVBoxLayout *containerLayout = new QVBoxLayout(m_vectorTableContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);

    // 创建工具栏和表格选择器区域
    QToolBar *toolBar = new QToolBar("向量表工具栏", m_vectorTableContainer);
    toolBar->setMovable(false);
    toolBar->setFloatable(false);
    toolBar->setIconSize(QSize(24, 24));
    toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    // 添加表格选择器
    QLabel *tableLabel = new QLabel(tr("当前向量表: "), toolBar);
    toolBar->addWidget(tableLabel);

    // 向量表选择器
    m_vectorTableSelector = new QComboBox(toolBar);
    m_vectorTableSelector->setObjectName("m_vectorTableSelector"); // 设置对象名
    m_vectorTableSelector->setMinimumWidth(250);
    toolBar->addWidget(m_vectorTableSelector);

    toolBar->addSeparator();

    // 添加行按钮
    QAction *addRowAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_FileDialogNewFolder), tr("添加行"), this);
    addRowAction->setToolTip(tr("向当前向量表添加新行"));
    connect(addRowAction, &QAction::triggered, this, &MainWindow::addRowToCurrentVectorTable);
    toolBar->addAction(addRowAction);
    if (!addRowAction->associatedWidgets().isEmpty())
    {
        QToolButton *button = qobject_cast<QToolButton *>(addRowAction->associatedWidgets().first());
        if (button)
            button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    }

    // 删除行按钮
    QAction *deleteRowAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_DialogDiscardButton), tr("删除所选行"), this);
    deleteRowAction->setToolTip(tr("删除选中的行"));
    connect(deleteRowAction, &QAction::triggered, this, &MainWindow::deleteSelectedVectorRows);
    toolBar->addAction(deleteRowAction);
    if (!deleteRowAction->associatedWidgets().isEmpty())
    {
        QToolButton *button = qobject_cast<QToolButton *>(deleteRowAction->associatedWidgets().first());
        if (button)
            button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    }

    // 跳转到指定行
    QAction *gotoAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_ArrowForward), tr("跳到行"), this);
    gotoAction->setToolTip(tr("跳转到指定行"));
    connect(gotoAction, &QAction::triggered, this, &MainWindow::gotoLine);
    toolBar->addAction(gotoAction);
    if (!gotoAction->associatedWidgets().isEmpty())
    {
        QToolButton *button = qobject_cast<QToolButton *>(gotoAction->associatedWidgets().first());
        if (button)
            button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    }

    toolBar->addSeparator();

    // 刷新按钮
    QAction *refreshAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_BrowserReload), tr("刷新"), this);
    refreshAction->setToolTip(tr("刷新表格数据"));
    connect(refreshAction, &QAction::triggered, this, &MainWindow::refreshVectorTableData);
    toolBar->addAction(refreshAction);
    if (!refreshAction->associatedWidgets().isEmpty())
    {
        QToolButton *button = qobject_cast<QToolButton *>(refreshAction->associatedWidgets().first());
        if (button)
            button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    }

    // 保存按钮
    QAction *saveAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_DialogSaveButton), tr("保存"), this);
    saveAction->setToolTip(tr("保存表格数据"));
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveVectorTableData);
    toolBar->addAction(saveAction);
    if (!saveAction->associatedWidgets().isEmpty())
    {
        QToolButton *button = qobject_cast<QToolButton *>(saveAction->associatedWidgets().first());
        if (button)
            button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    }

    toolBar->addSeparator();

    // 向量填充按钮
    m_fillVectorAction = new QAction(QIcon(":/icons/plus-circle.svg"), tr("向量填充"), this);
    m_fillVectorAction->setToolTip(tr("批量填充向量值"));
    connect(m_fillVectorAction, &QAction::triggered, this, &MainWindow::showFillVectorDialog);
    toolBar->addAction(m_fillVectorAction);
    if (!m_fillVectorAction->associatedWidgets().isEmpty())
    {
        QToolButton *button = qobject_cast<QToolButton *>(m_fillVectorAction->associatedWidgets().first());
        if (button)
            button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    }

    // TimeSet填充按钮
    m_fillTimeSetAction = new QAction(QIcon(":/icons/plus-circle.svg"), tr("TimeSet填充"), this);
    m_fillTimeSetAction->setToolTip(tr("批量填充TimeSet"));
    connect(m_fillTimeSetAction, &QAction::triggered, this, &MainWindow::showFillTimeSetDialog);
    toolBar->addAction(m_fillTimeSetAction);
    if (!m_fillTimeSetAction->associatedWidgets().isEmpty())
    {
        QToolButton *button = qobject_cast<QToolButton *>(m_fillTimeSetAction->associatedWidgets().first());
        if (button)
            button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    }

    // TimeSet替换按钮
    QAction *replaceTimeSetAction = new QAction(QIcon(":/icons/plus-circle.svg"), tr("TimeSet替换"), this);
    replaceTimeSetAction->setToolTip(tr("批量替换TimeSet"));
    connect(replaceTimeSetAction, &QAction::triggered, this, &MainWindow::showReplaceTimeSetDialog);
    toolBar->addAction(replaceTimeSetAction);
    if (!replaceTimeSetAction->associatedWidgets().isEmpty())
    {
        QToolButton *button = qobject_cast<QToolButton *>(replaceTimeSetAction->associatedWidgets().first());
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

    // 初始化两种表格显示方式
    // 1. 创建传统QTableWidget表格
    m_vectorTableWidget = new QTableWidget(this);
    m_vectorTableWidget->setAlternatingRowColors(true);
    m_vectorTableWidget->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_vectorTableWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_vectorTableWidget->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_vectorTableWidget->horizontalHeader()->setStretchLastSection(true);
    m_vectorTableWidget->verticalHeader()->setDefaultSectionSize(25);
    m_vectorTableWidget->verticalHeader()->setVisible(true);

    // 2. 创建新的QTableView表格视图
    m_vectorTableView = new QTableView(this);
    m_vectorTableView->setAlternatingRowColors(true);
    m_vectorTableView->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_vectorTableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_vectorTableView->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_vectorTableView->horizontalHeader()->setStretchLastSection(true);
    m_vectorTableView->verticalHeader()->setDefaultSectionSize(25);
    m_vectorTableView->verticalHeader()->setVisible(true);
    m_vectorTableView->setVisible(false); // 初始隐藏，待功能完成后逐步替换
    
    // 初始化表格模型
    m_vectorTableModel = new Vector::VectorTableModel(this);
    m_isUsingNewTableModel = false; // 默认使用旧模式
    
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
    containerLayout->addWidget(m_vectorTableView);  // 添加新的表格视图控件
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

// 添加新的方法来设置表格视图
void MainWindow::setupVectorTableView()
{
    qDebug() << "MainWindow::setupVectorTableView() - 初始化表格视图";
    
    // 设置表格视图的右键菜单策略
    m_vectorTableView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_vectorTableView, &QTableView::customContextMenuRequested,
            this, &MainWindow::showPinColumnContextMenu);
            
    // 连接选择变更事件
    connect(m_vectorTableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            [this](const QItemSelection &selected, const QItemSelection &deselected)
    {
        if (m_vectorTableView->model() == nullptr)
            return;
            
        QModelIndexList selectedIndexes = m_vectorTableView->selectionModel()->selectedIndexes();
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
        
        // 更新属性栏
        if (sameColumn && !selectedIndexes.isEmpty())
        {
            updateVectorColumnProperties(selectedIndexes.first().row(), column);
        }
    });
    
    // 连接单元格点击事件
    connect(m_vectorTableView, &QTableView::clicked, [this](const QModelIndex &index) {
        updateVectorColumnProperties(index.row(), index.column());
    });
    
    // 设置项委托，处理单元格编辑
    if (m_itemDelegate) {
        m_vectorTableView->setItemDelegate(m_itemDelegate);
    }
    
    // 连接数据更改信号 - 这个关键的连接在模型数据变化时更新UI和标记未保存状态
    if (m_vectorTableModel) {
        connect(m_vectorTableModel, &QAbstractItemModel::dataChanged,
                [this](const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles) {
            // 设置未保存标志
            m_hasUnsavedChanges = true;
            
            // 更新窗口标题以显示未保存状态
            updateWindowTitle(m_currentDbPath);
            
            // 如果波形图可见，更新波形图显示
            if (m_isWaveformVisible) {
                updateWaveformView();
            }
        });
    }
}

// 实现syncViewWithTableModel函数
void MainWindow::syncViewWithTableModel()
{
    qDebug() << "MainWindow::syncViewWithTableModel() - 同步视图状态，当前模式:" 
             << (m_isUsingNewTableModel ? "优化表格模型" : "传统表格控件");

    // 根据当前模式隐藏/显示相应的表格控件
    if (m_isUsingNewTableModel)
    {
        // 使用新的表格模型
        m_vectorTableWidget->setVisible(false);
        m_vectorTableView->setVisible(true);
        
        // 确保委托被正确设置
        if (m_itemDelegate && m_vectorTableView) {
            m_vectorTableView->setItemDelegate(m_itemDelegate);
        }
    }
    else
    {
        // 使用传统表格控件
        m_vectorTableView->setVisible(false);
        m_vectorTableWidget->setVisible(true);
    }
}
