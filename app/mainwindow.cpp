#include "mainwindow.h"
#include "database/databasemanager.h"
#include "database/databaseviewdialog.h"
#include "pin/pinlistdialog.h"
#include "timeset/timesetdialog.h"
#include "timeset/filltimesetdialog.h"
#include "timeset/replacetimesetdialog.h"
#include "pin/pinvalueedit.h"
#include "vector/vectortabledelegate.h"
#include "vector/vectordatahandler.h"
#include "common/dialogmanager.h"
#include "pin/vectorpinsettingsdialog.h"
#include "pin/pinsettingsdialog.h"
#include "vector/deleterangevectordialog.h"
#include "common/tablestylemanager.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QStandardPaths>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QLabel>
#include <QDebug>
#include <QStatusBar>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTableWidgetItem>
#include <QFont>
#include <QStyledItemDelegate>
#include <QComboBox>
#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QKeyEvent>
#include <QIcon>
#include <QFileInfo>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QSpinBox>
#include <QScrollArea>
#include <QTimer>
#include <QListWidget>
#include <QListWidgetItem>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_isUpdatingUI(false)
{
    setupUI();
    setupMenu();

    // 创建数据处理器和对话框管理器
    m_dataHandler = new VectorDataHandler();
    m_dialogManager = new DialogManager(this);

    // 设置窗口标题和大小
    setWindowTitle("VecEdit - 矢量测试编辑器");
    resize(1024, 768);

    // 显示就绪状态
    statusBar()->showMessage("就绪");
}

MainWindow::~MainWindow()
{
    // Qt对象会自动清理
    closeCurrentProject();

    // 释放我们创建的对象
    if (m_dataHandler)
        delete m_dataHandler;

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
    QLabel *welcomeLabel = new QLabel(tr("欢迎使用VecEdit矢量测试编辑器"), m_welcomeWidget);
    welcomeLabel->setAlignment(Qt::AlignCenter);
    welcomeLabel->setStyleSheet("font-size: 24px; font-weight: bold; margin: 20px;");

    // 添加说明标签
    QLabel *instructionLabel = new QLabel(tr("请使用\"文件\"菜单创建或打开项目，然后通过\"查看\"菜单查看数据表"), m_welcomeWidget);
    instructionLabel->setAlignment(Qt::AlignCenter);
    instructionLabel->setStyleSheet("font-size: 16px; margin: 10px;");

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
    QAction *newProjectAction = fileMenu->addAction(tr("新建项目(&N)"));
    connect(newProjectAction, &QAction::triggered, this, &MainWindow::createNewProject);

    // 打开项目
    QAction *openProjectAction = fileMenu->addAction(tr("打开项目(&O)"));
    connect(openProjectAction, &QAction::triggered, this, &MainWindow::openExistingProject);

    // 分隔符
    fileMenu->addSeparator();

    // 关闭项目
    QAction *closeProjectAction = fileMenu->addAction(tr("关闭项目(&C)"));
    connect(closeProjectAction, &QAction::triggered, this, &MainWindow::closeCurrentProject);

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
}

void MainWindow::createNewProject()
{
    // 先关闭当前项目
    closeCurrentProject();

    // 选择保存位置和文件名
    QString documentsDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString dbPath = QFileDialog::getSaveFileName(this, tr("保存项目数据库"),
                                                  documentsDir + "/VecEditProject.db",
                                                  tr("SQLite数据库 (*.db)"));
    if (dbPath.isEmpty())
    {
        return;
    }

    // 获取schema.sql文件路径（与可执行文件同目录）
    QString schemaPath = QApplication::applicationDirPath() + "/schema.sql";

    // 使用DatabaseManager初始化数据库
    if (DatabaseManager::instance()->initializeNewDatabase(dbPath, schemaPath))
    {
        m_currentDbPath = dbPath;
        setWindowTitle(QString("VecEdit - 矢量测试编辑器 [%1]").arg(QFileInfo(dbPath).fileName()));
        statusBar()->showMessage(tr("数据库创建成功: %1").arg(dbPath));

        // 显示管脚添加对话框
        bool pinsAdded = showAddPinsDialog();

        // 如果成功添加了管脚，则显示TimeSet对话框
        bool timeSetAdded = false;
        if (pinsAdded)
        {
            timeSetAdded = showTimeSetDialog(true);
        }

        // 检查是否有向量表，如果没有，自动弹出创建向量表对话框
        QSqlDatabase db = DatabaseManager::instance()->database();
        QSqlQuery query(db);
        bool hasVectorTable = false;

        if (query.exec("SELECT COUNT(*) FROM vector_tables"))
        {
            if (query.next())
            {
                int count = query.value(0).toInt();
                hasVectorTable = (count > 0);
                qDebug() << "MainWindow::createNewProject - 检查向量表数量:" << count;
            }
        }

        if (!hasVectorTable && timeSetAdded)
        {
            qDebug() << "MainWindow::createNewProject - 未找到向量表，自动显示创建向量表对话框";
            addNewVectorTable();
        }

        // 显示创建成功的消息
        QString message;
        if (pinsAdded && timeSetAdded)
        {
            message = tr("项目数据库创建成功！管脚和TimeSet已添加。\n您可以通过\"查看\"菜单打开数据库查看器");
        }
        else if (pinsAdded)
        {
            message = tr("项目数据库创建成功！管脚已添加。\n您可以通过\"查看\"菜单打开数据库查看器");
        }
        else
        {
            message = tr("项目数据库创建成功！\n您可以通过\"查看\"菜单打开数据库查看器");
        }

        // 加载向量表数据
        loadVectorTable();

        QMessageBox::information(this, tr("成功"), message);
    }
    else
    {
        statusBar()->showMessage(tr("错误: %1").arg(DatabaseManager::instance()->lastError()));
        QMessageBox::critical(this, tr("错误"),
                              tr("创建项目数据库失败：\n%1").arg(DatabaseManager::instance()->lastError()));
    }
}

void MainWindow::openExistingProject()
{
    // 先关闭当前项目
    closeCurrentProject();

    // 选择要打开的数据库文件
    QString documentsDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString dbPath = QFileDialog::getOpenFileName(this, tr("打开项目数据库"),
                                                  documentsDir,
                                                  tr("SQLite数据库 (*.db)"));
    if (dbPath.isEmpty())
    {
        return;
    }

    // 使用DatabaseManager打开数据库
    if (DatabaseManager::instance()->openExistingDatabase(dbPath))
    {
        m_currentDbPath = dbPath;
        setWindowTitle(QString("VecEdit - 矢量测试编辑器 [%1]").arg(QFileInfo(dbPath).fileName()));

        // 显示当前数据库版本
        int version = DatabaseManager::instance()->getCurrentDatabaseVersion();
        statusBar()->showMessage(tr("数据库已打开: %1 (版本: %2)").arg(dbPath).arg(version));

        // 加载向量表数据
        loadVectorTable();

        QMessageBox::information(this, tr("成功"),
                                 tr("项目数据库已打开！当前版本：%1\n您可以通过\"查看\"菜单打开数据库查看器").arg(version));
    }
    else
    {
        statusBar()->showMessage(tr("错误: %1").arg(DatabaseManager::instance()->lastError()));
        QMessageBox::critical(this, tr("错误"),
                              tr("打开项目数据库失败：\n%1").arg(DatabaseManager::instance()->lastError()));
    }
}

void MainWindow::closeCurrentProject()
{
    if (!m_currentDbPath.isEmpty())
    {
        // 关闭数据库连接
        DatabaseManager::instance()->closeDatabase();
        m_currentDbPath.clear();

        // 显示欢迎界面
        if (m_welcomeWidget && m_vectorTableContainer)
        {
            m_welcomeWidget->setVisible(true);
            m_vectorTableContainer->setVisible(false);
        }

        // 重置窗口标题
        setWindowTitle("VecEdit - 矢量测试编辑器");
        statusBar()->showMessage("项目已关闭");
    }
}

void MainWindow::showDatabaseViewDialog()
{
    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        QMessageBox::warning(this, tr("警告"), tr("请先打开或创建一个项目数据库"));
        return;
    }

    // 使用对话框管理器显示数据库视图对话框
    m_dialogManager->showDatabaseViewDialog();
}

bool MainWindow::showAddPinsDialog()
{
    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        return false;
    }

    // 使用对话框管理器显示添加管脚对话框
    bool success = m_dialogManager->showAddPinsDialog();
    if (success)
    {
        statusBar()->showMessage(tr("管脚添加成功"));
    }
    return success;
}

bool MainWindow::showTimeSetDialog(bool isInitialSetup)
{
    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        return false;
    }

    // 使用对话框管理器显示TimeSet对话框，传递isInitialSetup参数
    bool success = m_dialogManager->showTimeSetDialog(isInitialSetup);

    // 无论是否成功，都刷新缓存，确保UI显示最新的TimeSet列表
    if (m_itemDelegate)
    {
        qDebug() << "MainWindow::showTimeSetDialog - 刷新TimeSet选项缓存";
        m_itemDelegate->refreshCache();
    }

    if (success)
    {
        statusBar()->showMessage(tr("TimeSet已成功添加"));
    }
    return success;
}

void MainWindow::setupVectorTableUI()
{
    // 创建向量表容器
    m_vectorTableContainer = new QWidget(this);
    QVBoxLayout *containerLayout = new QVBoxLayout(m_vectorTableContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);

    // 创建顶部控制栏
    QHBoxLayout *controlLayout = new QHBoxLayout();

    // 向量表选择下拉框
    QLabel *selectLabel = new QLabel(tr("选择向量表:"), this);
    m_vectorTableSelector = new QComboBox(this);
    m_vectorTableSelector->setMinimumWidth(150);
    controlLayout->addWidget(selectLabel);
    controlLayout->addWidget(m_vectorTableSelector);

    // 创建按钮分组1 - 管脚相关
    // 添加设置向量表管脚按钮
    m_setupPinsButton = new QPushButton(tr("设置向量表"), this);
    connect(m_setupPinsButton, &QPushButton::clicked, this, &MainWindow::setupVectorTablePins);
    controlLayout->addWidget(m_setupPinsButton);

    // 添加管脚设置按钮
    m_pinSettingsButton = new QPushButton(tr("管脚设置"), this);
    connect(m_pinSettingsButton, &QPushButton::clicked, this, &MainWindow::openPinSettingsDialog);
    controlLayout->addWidget(m_pinSettingsButton);

    // 添加"添加管脚"按钮
    m_addPinButton = new QPushButton(tr("添加管脚"), this);
    connect(m_addPinButton, &QPushButton::clicked, this, &MainWindow::addSinglePin);
    controlLayout->addWidget(m_addPinButton);

    // 添加"删除管脚"按钮
    m_deletePinButton = new QPushButton(tr("删除管脚"), this);
    connect(m_deletePinButton, &QPushButton::clicked, this, &MainWindow::deletePins);
    controlLayout->addWidget(m_deletePinButton);

    // 添加"添加分组"按钮
    m_addGroupButton = new QPushButton(tr("添加分组"), this);
    connect(m_addGroupButton, &QPushButton::clicked, this, &MainWindow::showPinGroupDialog);
    controlLayout->addWidget(m_addGroupButton);

    controlLayout->addSpacing(20);

    // 创建按钮分组2 - TimeSet相关
    // 添加TimeSet设置按钮
    m_timeSetSettingsButton = new QPushButton(tr("TimeSet设置"), this);
    connect(m_timeSetSettingsButton, &QPushButton::clicked, this, &MainWindow::openTimeSetSettingsDialog);
    controlLayout->addWidget(m_timeSetSettingsButton);

    // 添加填充TimeSet按钮
    m_fillTimeSetButton = new QPushButton(tr("填充TimeSet"), this);
    connect(m_fillTimeSetButton, &QPushButton::clicked, this, &MainWindow::showFillTimeSetDialog);
    controlLayout->addWidget(m_fillTimeSetButton);

    // 添加替换TimeSet按钮
    m_replaceTimeSetButton = new QPushButton(tr("替换TimeSet"), this);
    connect(m_replaceTimeSetButton, &QPushButton::clicked, this, &MainWindow::showReplaceTimeSetDialog);
    controlLayout->addWidget(m_replaceTimeSetButton);

    controlLayout->addSpacing(20);

    // 创建按钮分组3 - 向量表行操作
    // 添加向量行按钮
    QPushButton *addRowButton = new QPushButton(tr("添加向量行"), this);
    connect(addRowButton, &QPushButton::clicked, this, &MainWindow::addRowToCurrentVectorTable);
    controlLayout->addWidget(addRowButton);

    // 删除向量行按钮
    QPushButton *deleteRowButton = new QPushButton(tr("删除向量行"), this);
    connect(deleteRowButton, &QPushButton::clicked, this, &MainWindow::deleteSelectedVectorRows);
    controlLayout->addWidget(deleteRowButton);

    // 删除指定范围内的向量行按钮
    m_deleteRangeButton = new QPushButton(tr("删除范围"), this);
    connect(m_deleteRangeButton, &QPushButton::clicked, this, &MainWindow::deleteVectorRowsInRange);
    controlLayout->addWidget(m_deleteRangeButton);

    // 跳转到行按钮
    m_gotoLineButton = new QPushButton(tr("跳转到行"), this);
    connect(m_gotoLineButton, &QPushButton::clicked, this, &MainWindow::gotoLine);
    controlLayout->addWidget(m_gotoLineButton);

    controlLayout->addSpacing(20);

    // 创建按钮分组4 - 向量表操作和杂项
    // 添加新向量表按钮
    QPushButton *addTableButton = new QPushButton(tr("新建向量表"), this);
    connect(addTableButton, &QPushButton::clicked, this, &MainWindow::addNewVectorTable);
    controlLayout->addWidget(addTableButton);

    // 删除向量表按钮
    QPushButton *deleteTableButton = new QPushButton(tr("删除向量表"), this);
    connect(deleteTableButton, &QPushButton::clicked, this, &MainWindow::deleteCurrentVectorTable);
    controlLayout->addWidget(deleteTableButton);

    // 刷新按钮
    m_refreshButton = new QPushButton(tr("刷新"), this);
    connect(m_refreshButton, &QPushButton::clicked, this, &MainWindow::refreshVectorTableData);
    controlLayout->addWidget(m_refreshButton);

    // 保存按钮
    QPushButton *saveButton = new QPushButton(tr("保存"), this);
    connect(saveButton, &QPushButton::clicked, this, &MainWindow::saveVectorTableData);
    controlLayout->addWidget(saveButton);

    // 使用拉伸填充剩余空间
    controlLayout->addStretch();

    // 创建表格视图
    m_vectorTableWidget = new QTableWidget(this);
    m_vectorTableWidget->setAlternatingRowColors(true);
    m_vectorTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_vectorTableWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_vectorTableWidget->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_vectorTableWidget->horizontalHeader()->setStretchLastSection(true);
    m_vectorTableWidget->verticalHeader()->setDefaultSectionSize(25);
    m_vectorTableWidget->verticalHeader()->setVisible(true);

    // 连接向量表选择器信号
    connect(m_vectorTableSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onVectorTableSelectionChanged);

    // 创建项委托，处理单元格编辑
    m_itemDelegate = new VectorTableItemDelegate(this);
    m_vectorTableWidget->setItemDelegate(m_itemDelegate);

    // 创建Tab栏
    setupTabBar();

    // 将布局添加到容器
    containerLayout->addLayout(controlLayout);
    containerLayout->addWidget(m_vectorTableWidget);
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

void MainWindow::loadVectorTable()
{
    qDebug() << "MainWindow::loadVectorTable - 开始加载向量表";

    // 清空当前选择框
    m_vectorTableSelector->clear();

    // 清空Tab页签
    m_vectorTabWidget->clear();
    m_tabToTableId.clear();

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qDebug() << "MainWindow::loadVectorTable - 错误：数据库未打开";
        statusBar()->showMessage("错误：数据库未打开");
        return;
    }

    qDebug() << "MainWindow::loadVectorTable - 数据库已打开，开始查询向量表";

    // 刷新itemDelegate的缓存，确保TimeSet选项是最新的
    if (m_itemDelegate)
    {
        qDebug() << "MainWindow::loadVectorTable - 刷新TimeSet选项缓存";
        m_itemDelegate->refreshCache();
    }

    // 查询所有向量表
    QSqlQuery tableQuery(db);
    if (tableQuery.exec("SELECT id, table_name FROM vector_tables ORDER BY table_name"))
    {
        qDebug() << "MainWindow::loadVectorTable - 向量表查询执行成功";
        int count = 0;
        while (tableQuery.next())
        {
            int tableId = tableQuery.value(0).toInt();
            QString tableName = tableQuery.value(1).toString();

            // 添加到下拉选择框
            m_vectorTableSelector->addItem(tableName, tableId);

            // 添加到Tab页签
            addVectorTableTab(tableId, tableName);

            count++;
            qDebug() << "MainWindow::loadVectorTable - 找到向量表:" << tableName << "ID:" << tableId;
        }

        qDebug() << "MainWindow::loadVectorTable - 总共找到" << count << "个向量表";
    }
    else
    {
        qDebug() << "MainWindow::loadVectorTable - 向量表查询失败:" << tableQuery.lastError().text();
    }

    // 如果有向量表，显示向量表窗口，否则显示欢迎窗口
    if (m_vectorTableSelector->count() > 0)
    {
        qDebug() << "MainWindow::loadVectorTable - 有向量表，显示向量表窗口";
        m_welcomeWidget->setVisible(false);
        m_vectorTableContainer->setVisible(true);

        // 默认选择第一个表
        m_vectorTableSelector->setCurrentIndex(0);
    }
    else
    {
        qDebug() << "MainWindow::loadVectorTable - 没有找到向量表，显示欢迎窗口";
        m_welcomeWidget->setVisible(true);
        m_vectorTableContainer->setVisible(false);
        QMessageBox::information(this, "提示", "没有找到向量表，请先创建向量表");
    }
}

void MainWindow::onVectorTableSelectionChanged(int index)
{
    if (index < 0 || m_isUpdatingUI)
        return;

    // 设置标志防止循环更新
    m_isUpdatingUI = true;

    qDebug() << "MainWindow::onVectorTableSelectionChanged - 向量表选择已更改，索引:" << index;

    // 获取当前选中表ID
    int tableId = m_vectorTableSelector->currentData().toInt();

    // 同步Tab页签选择
    syncTabWithComboBox(index);

    // 使用数据处理器加载数据
    if (m_dataHandler->loadVectorTableData(tableId, m_vectorTableWidget))
    {
        // 应用表格样式
        TableStyleManager::applyTableStyle(m_vectorTableWidget);

        statusBar()->showMessage(QString("已加载向量表: %1").arg(m_vectorTableSelector->currentText()));
    }
    else
    {
        statusBar()->showMessage("加载向量表失败");
    }

    // 重置标志
    m_isUpdatingUI = false;
}

void MainWindow::syncTabWithComboBox(int comboBoxIndex)
{
    if (comboBoxIndex < 0 || comboBoxIndex >= m_vectorTableSelector->count())
        return;

    qDebug() << "MainWindow::syncTabWithComboBox - 同步Tab页签与下拉框选择";

    // 获取当前选择的表ID
    int tableId = m_vectorTableSelector->itemData(comboBoxIndex).toInt();

    // 在Map中查找对应的Tab索引
    int tabIndex = -1;
    for (auto it = m_tabToTableId.begin(); it != m_tabToTableId.end(); ++it)
    {
        if (it.value() == tableId)
        {
            tabIndex = it.key();
            break;
        }
    }

    // 如果找到对应的Tab，选中它
    if (tabIndex >= 0 && tabIndex < m_vectorTabWidget->count())
    {
        m_vectorTabWidget->setCurrentIndex(tabIndex);
    }
}

void MainWindow::onTabChanged(int index)
{
    if (index < 0 || m_isUpdatingUI)
        return;

    qDebug() << "MainWindow::onTabChanged - 当前选中的Tab页索引:" << index;

    // 设置标志防止循环更新
    m_isUpdatingUI = true;

    // 同步下拉框选择
    syncComboBoxWithTab(index);

    // 获取选中Tab对应的表ID
    int tableId = m_tabToTableId.value(index, -1);
    if (tableId >= 0)
    {
        qDebug() << "MainWindow::onTabChanged - 加载表ID:" << tableId << "的数据";

        // 使用数据处理器加载数据
        if (m_dataHandler->loadVectorTableData(tableId, m_vectorTableWidget))
        {
            // 应用表格样式
            TableStyleManager::applyTableStyle(m_vectorTableWidget);

            // 更新状态栏
            statusBar()->showMessage(QString("已加载向量表: %1").arg(m_vectorTabWidget->tabText(index)));

            qDebug() << "MainWindow::onTabChanged - 成功加载表格数据";
        }
        else
        {
            statusBar()->showMessage("加载向量表失败");
            qDebug() << "MainWindow::onTabChanged - 加载表格数据失败";
        }
    }

    // 重置标志
    m_isUpdatingUI = false;
}

void MainWindow::syncComboBoxWithTab(int tabIndex)
{
    if (tabIndex < 0 || tabIndex >= m_vectorTabWidget->count())
        return;

    qDebug() << "MainWindow::syncComboBoxWithTab - 同步下拉框与Tab页签选择";

    // 获取选中Tab对应的表ID
    int tableId = m_tabToTableId.value(tabIndex, -1);
    if (tableId < 0)
        return;

    // 在下拉框中查找对应索引
    int comboIndex = m_vectorTableSelector->findData(tableId);
    if (comboIndex >= 0)
    {
        m_vectorTableSelector->setCurrentIndex(comboIndex);
    }
}

void MainWindow::addVectorTableTab(int tableId, const QString &tableName)
{
    qDebug() << "MainWindow::addVectorTableTab - 添加向量表Tab页签:" << tableName;

    // 添加到Tab页签
    int index = m_vectorTabWidget->addTab(new QWidget(), tableName);

    // 存储映射关系
    m_tabToTableId[index] = tableId;
}

// 保存向量表数据
void MainWindow::saveVectorTableData()
{
    // 获取当前选择的向量表
    QString currentTable = m_vectorTableSelector->currentText();
    if (currentTable.isEmpty())
    {
        QMessageBox::warning(this, "保存失败", "请先选择一个向量表");
        return;
    }

    // 获取表ID
    int tableId = m_vectorTableSelector->currentData().toInt();

    // 使用数据处理器保存数据
    QString errorMessage;
    if (m_dataHandler->saveVectorTableData(tableId, m_vectorTableWidget, errorMessage))
    {
        QMessageBox::information(this, "保存成功", "向量表数据已成功保存");
        statusBar()->showMessage("向量表数据已成功保存");
    }
    else
    {
        QMessageBox::critical(this, "保存失败", errorMessage);
        statusBar()->showMessage("保存失败: " + errorMessage);
    }
}

void MainWindow::addNewVectorTable()
{
    qDebug() << "MainWindow::addNewVectorTable - 开始添加新向量表";

    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        qDebug() << "MainWindow::addNewVectorTable - 未打开数据库，操作取消";
        QMessageBox::warning(this, "警告", "请先打开或创建一个项目数据库");
        return;
    }

    qDebug() << "MainWindow::addNewVectorTable - 数据库已连接，准备创建向量表";

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();

    // 弹出向量表命名对话框
    QDialog vectorNameDialog(this);
    vectorNameDialog.setWindowTitle("创建向量表向导");
    vectorNameDialog.setFixedSize(320, 120);

    QVBoxLayout *layout = new QVBoxLayout(&vectorNameDialog);

    // 名称标签和输入框
    QHBoxLayout *nameLayout = new QHBoxLayout();
    QLabel *nameLabel = new QLabel("名称:", &vectorNameDialog);
    QLineEdit *nameEdit = new QLineEdit(&vectorNameDialog);
    nameEdit->setMinimumWidth(200);
    nameLayout->addWidget(nameLabel);
    nameLayout->addWidget(nameEdit);

    // 按钮
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &vectorNameDialog);
    connect(buttonBox, &QDialogButtonBox::accepted, &vectorNameDialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &vectorNameDialog, &QDialog::reject);

    layout->addLayout(nameLayout);
    layout->addWidget(buttonBox);

    // 执行对话框
    if (vectorNameDialog.exec() == QDialog::Accepted)
    {
        QString tableName = nameEdit->text().trimmed();
        qDebug() << "MainWindow::addNewVectorTable - 用户输入的向量表名称:" << tableName;

        // 验证用户输入
        if (tableName.isEmpty())
        {
            QMessageBox::warning(this, "错误", "向量表名称不能为空");
            return;
        }

        // 检查名称是否已存在
        QSqlQuery checkQuery(db);
        checkQuery.prepare("SELECT COUNT(*) FROM vector_tables WHERE LOWER(table_name) = LOWER(?)");
        checkQuery.addBindValue(tableName);

        if (checkQuery.exec() && checkQuery.next())
        {
            int count = checkQuery.value(0).toInt();
            if (count > 0)
            {
                QMessageBox::warning(this, "错误", "已存在同名向量表");
                return;
            }
        }

        // 插入新表
        QSqlQuery insertQuery(db);
        insertQuery.prepare("INSERT INTO vector_tables (table_name) VALUES (?)");
        insertQuery.addBindValue(tableName);

        if (insertQuery.exec())
        {
            int newTableId = insertQuery.lastInsertId().toInt();
            qDebug() << "MainWindow::addNewVectorTable - 新向量表创建成功，ID:" << newTableId;

            // 添加到下拉框和Tab页签
            m_vectorTableSelector->addItem(tableName, newTableId);
            addVectorTableTab(newTableId, tableName);

            // 选中新添加的表
            int newIndex = m_vectorTableSelector->findData(newTableId);
            if (newIndex >= 0)
            {
                m_vectorTableSelector->setCurrentIndex(newIndex);
            }

            // 显示管脚选择对话框
            showPinSelectionDialog(newTableId, tableName);

            // 更新UI显示
            if (m_welcomeWidget->isVisible())
            {
                m_welcomeWidget->setVisible(false);
                m_vectorTableContainer->setVisible(true);
            }
        }
        else
        {
            QMessageBox::critical(this, "错误", "创建向量表失败: " + insertQuery.lastError().text());
        }
    }
}

// 为当前选中的向量表添加行
void MainWindow::addRowToCurrentVectorTable()
{
    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        QMessageBox::warning(this, "警告", "请先打开或创建一个项目数据库");
        return;
    }

    // 检查是否有选中的向量表
    if (m_vectorTableSelector->count() == 0 || m_vectorTableSelector->currentIndex() < 0)
    {
        QMessageBox::warning(this, "警告", "请先选择一个向量表");
        return;
    }

    // 获取当前选中的向量表ID和名称
    int tableId = m_vectorTableSelector->currentData().toInt();
    QString tableName = m_vectorTableSelector->currentText();

    // 查询当前表中最大的排序索引
    int maxSortIndex = -1;
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery query(db);
    query.prepare("SELECT MAX(sort_index) FROM vector_table_data WHERE table_id = ?");
    query.addBindValue(tableId);

    if (query.exec() && query.next())
    {
        maxSortIndex = query.value(0).toInt();
    }

    // 使用对话框管理器显示向量行数据录入对话框
    if (m_dialogManager->showVectorDataDialog(tableId, tableName, maxSortIndex + 1))
    {
        // 刷新表格显示
        onVectorTableSelectionChanged(m_vectorTableSelector->currentIndex());
    }
}

// 删除当前选中的向量表
void MainWindow::deleteCurrentVectorTable()
{
    // 检查是否有选中的向量表
    if (m_vectorTableSelector->count() == 0 || m_vectorTableSelector->currentIndex() < 0)
    {
        QMessageBox::warning(this, "警告", "请先选择一个向量表");
        return;
    }

    // 获取表ID和名称
    int tableId = m_vectorTableSelector->currentData().toInt();
    QString tableName = m_vectorTableSelector->currentText();

    // 弹出确认对话框
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "确认删除",
                                  "确定要删除向量表 \"" + tableName + "\" 吗？\n此操作将删除表中的所有数据，且不可撤销。",
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::No)
    {
        return;
    }

    // 使用数据处理器删除向量表
    QString errorMessage;
    if (m_dataHandler->deleteVectorTable(tableId, errorMessage))
    {
        // 记录当前选中的索引
        int currentIndex = m_vectorTableSelector->currentIndex();

        // 找到对应的Tab索引
        int tabIndex = -1;
        for (auto it = m_tabToTableId.begin(); it != m_tabToTableId.end(); ++it)
        {
            if (it.value() == tableId)
            {
                tabIndex = it.key();
                break;
            }
        }

        // 删除Tab页签
        if (tabIndex >= 0)
        {
            m_vectorTabWidget->removeTab(tabIndex);
            m_tabToTableId.remove(tabIndex);

            // 更新其他Tab的映射关系
            QMap<int, int> updatedMap;
            for (auto it = m_tabToTableId.begin(); it != m_tabToTableId.end(); ++it)
            {
                int oldIndex = it.key();
                int newIndex = oldIndex > tabIndex ? oldIndex - 1 : oldIndex;
                updatedMap[newIndex] = it.value();
            }
            m_tabToTableId = updatedMap;
        }

        QMessageBox::information(this, "删除成功", "向量表 \"" + tableName + "\" 已成功删除");

        // 重新加载向量表列表
        loadVectorTable();
    }
    else
    {
        QMessageBox::critical(this, "删除失败", errorMessage);
        statusBar()->showMessage("删除向量表失败: " + errorMessage);
    }
}

// 删除选中的向量行
void MainWindow::deleteSelectedVectorRows()
{
    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        QMessageBox::warning(this, "警告", "请先打开或创建一个项目数据库");
        return;
    }

    // 检查是否有选中的向量表
    if (m_vectorTableSelector->count() == 0 || m_vectorTableSelector->currentIndex() < 0)
    {
        QMessageBox::warning(this, "警告", "请先选择一个向量表");
        return;
    }

    // 获取选中的行
    QList<int> selectedRows;
    QModelIndexList selectedIndexes = m_vectorTableWidget->selectionModel()->selectedRows();
    if (selectedIndexes.isEmpty())
    {
        QMessageBox::warning(this, "警告", "请先选择要删除的行");
        return;
    }

    for (const QModelIndex &index : selectedIndexes)
    {
        selectedRows.append(index.row());
    }

    // 弹出确认对话框
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "确认删除",
                                  "确定要删除选中的 " + QString::number(selectedRows.size()) + " 行数据吗？\n此操作不可撤销。",
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::No)
    {
        return;
    }

    // 获取表ID
    int tableId = m_vectorTableSelector->currentData().toInt();

    // 使用数据处理器删除选中的行
    QString errorMessage;
    if (m_dataHandler->deleteVectorRows(tableId, selectedRows, errorMessage))
    {
        QMessageBox::information(this, "删除成功", "已成功删除 " + QString::number(selectedRows.size()) + " 行数据");

        // 刷新表格
        onVectorTableSelectionChanged(m_vectorTableSelector->currentIndex());
    }
    else
    {
        QMessageBox::critical(this, "删除失败", errorMessage);
        statusBar()->showMessage("删除行失败: " + errorMessage);

        qDebug() << "MainWindow::deleteSelectedVectorRows - 删除失败：" << errorMessage;
    }
}

void MainWindow::showPinSelectionDialog(int tableId, const QString &tableName)
{
    if (m_dialogManager)
    {
        qDebug() << "MainWindow::showPinSelectionDialog - 开始显示管脚选择对话框";
        bool success = m_dialogManager->showPinSelectionDialog(tableId, tableName);
        qDebug() << "MainWindow::showPinSelectionDialog - 管脚选择对话框返回结果:" << success;

        // 无论对话框结果如何，都刷新表格显示
        int currentIndex = m_vectorTableSelector->findData(tableId);
        if (currentIndex >= 0)
        {
            qDebug() << "MainWindow::showPinSelectionDialog - 刷新表格显示，表ID:" << tableId;
            m_vectorTableSelector->setCurrentIndex(currentIndex);
            onVectorTableSelectionChanged(currentIndex);
        }
    }
}

void MainWindow::showVectorDataDialog(int tableId, const QString &tableName, int startIndex)
{
    if (m_dialogManager)
    {
        qDebug() << "MainWindow::showVectorDataDialog - 开始显示向量行数据录入对话框";
        bool success = m_dialogManager->showVectorDataDialog(tableId, tableName, startIndex);
        qDebug() << "MainWindow::showVectorDataDialog - 向量行数据录入对话框返回结果:" << success;

        // 如果成功添加了数据，刷新表格显示
        if (success)
        {
            qDebug() << "MainWindow::showVectorDataDialog - 成功添加向量行数据，刷新表格";
            // 找到对应的表索引并刷新
            int currentIndex = m_vectorTableSelector->findData(tableId);
            if (currentIndex >= 0)
            {
                m_vectorTableSelector->setCurrentIndex(currentIndex);
                onVectorTableSelectionChanged(currentIndex);
            }
        }
    }
}

void MainWindow::showFillTimeSetDialog()
{
    // 检查是否有向量表被选中
    int currentIndex = m_vectorTableSelector->currentIndex();
    if (currentIndex < 0)
    {
        QMessageBox::warning(this, tr("警告"), tr("请先选择一个向量表"));
        return;
    }

    // 获取当前向量表ID
    int tableId = m_vectorTableSelector->itemData(currentIndex).toInt();

    // 获取向量表行数
    int rowCount = m_dataHandler->getVectorTableRowCount(tableId);
    if (rowCount <= 0)
    {
        QMessageBox::warning(this, tr("警告"), tr("当前向量表没有行数据"));
        return;
    }

    // 创建填充TimeSet对话框
    FillTimeSetDialog dialog(this);
    dialog.setVectorRowCount(rowCount);

    // 获取选中的UI行 (0-based index)
    QList<int> selectedUiRows;
    QModelIndexList selectedIndexes = m_vectorTableWidget->selectionModel()->selectedRows();
    if (!selectedIndexes.isEmpty())
    {
        int minRow = INT_MAX;
        int maxRow = INT_MIN;
        foreach (const QModelIndex &index, selectedIndexes)
        {
            int uiRow = index.row();
            selectedUiRows.append(uiRow);
            minRow = qMin(minRow, uiRow);
            maxRow = qMax(maxRow, uiRow);
        }

        // 设置对话框的起始行和结束行，主要为了显示，实际更新使用selectedUiRows
        dialog.setSelectedRange(minRow + 1, maxRow + 1);
    }

    if (dialog.exec() == QDialog::Accepted)
    {
        // 获取用户输入
        int timeSetId = dialog.getSelectedTimeSetId();

        // 填充TimeSet，传递选中的UI行列表
        fillTimeSetForVectorTable(timeSetId, selectedUiRows);
    }
}

void MainWindow::fillTimeSetForVectorTable(int timeSetId, const QList<int> &selectedUiRows)
{
    // 添加调试日志
    qDebug() << "填充TimeSet开始 - TimeSet ID:" << timeSetId;
    if (!selectedUiRows.isEmpty())
    {
        QStringList rowList;
        for (int row : selectedUiRows)
        {
            rowList << QString::number(row);
        }
        qDebug() << "填充TimeSet - 针对UI行:" << rowList.join(',');
    }
    else
    {
        qDebug() << "填充TimeSet - 未选择行，将应用于整个表";
    }

    // 检查TimeSet ID有效性
    if (timeSetId <= 0)
    {
        QMessageBox::warning(this, tr("错误"), tr("TimeSet ID无效"));
        qDebug() << "填充TimeSet参数无效 - TimeSet ID:" << timeSetId;
        return;
    }

    // 获取当前向量表ID
    int currentIndex = m_vectorTableSelector->currentIndex();
    if (currentIndex < 0)
    {
        qDebug() << "填充TimeSet失败 - 未选择向量表";
        return;
    }
    int tableId = m_vectorTableSelector->itemData(currentIndex).toInt();
    QString tableName = m_vectorTableSelector->currentText();
    qDebug() << "填充TimeSet - 当前向量表ID:" << tableId << ", 名称:" << tableName;

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接失败"));
        qDebug() << "填充TimeSet失败 - 数据库连接失败";
        return;
    }

    // 获取TimeSet名称用于日志
    QSqlQuery nameQuery(db);
    nameQuery.prepare("SELECT timeset_name FROM timeset_list WHERE id = ?");
    nameQuery.addBindValue(timeSetId);
    QString timeSetName = "未知";
    if (nameQuery.exec() && nameQuery.next())
    {
        timeSetName = nameQuery.value(0).toString();
    }
    qDebug() << "填充TimeSet - 使用TimeSet:" << timeSetName << "(" << timeSetId << ")";

    // 开始事务
    db.transaction();

    try
    {
        QList<int> idsToUpdate; // 存储要更新的数据库行的ID

        if (!selectedUiRows.isEmpty())
        {
            // 1. 获取表中所有行的ID，按sort_index排序
            QList<int> allRowIds;
            QSqlQuery idQuery(db);
            QString idSql = QString("SELECT id FROM vector_table_data WHERE table_id = %1 ORDER BY sort_index").arg(tableId);
            qDebug() << "填充TimeSet - 查询所有行ID:" << idSql;
            if (!idQuery.exec(idSql))
            {
                throw std::runtime_error(("查询行ID失败: " + idQuery.lastError().text()).toStdString());
            }
            while (idQuery.next())
            {
                allRowIds.append(idQuery.value(0).toInt());
            }
            qDebug() << "填充TimeSet - 数据库中共有" << allRowIds.size() << "行";

            // 2. 根据选中的UI行索引，找到对应的数据库ID
            foreach (int uiRow, selectedUiRows)
            {
                if (uiRow >= 0 && uiRow < allRowIds.size())
                {
                    idsToUpdate.append(allRowIds[uiRow]);
                }
                else
                {
                    qDebug() << "填充TimeSet警告 - UI行索引" << uiRow << "无效，忽略";
                }
            }
            if (idsToUpdate.isEmpty())
            {
                qDebug() << "填充TimeSet - 没有有效的数据库ID需要更新，操作取消";
                db.rollback(); // 不需要执行事务，直接回滚
                return;
            }
            qDebug() << "填充TimeSet - 准备更新的数据库ID:" << idsToUpdate;
        }

        // 准备更新SQL
        QSqlQuery query(db);
        QString updateSQL;
        if (!idsToUpdate.isEmpty())
        {
            // 如果有选定行，则基于ID更新
            QString idPlaceholders = QString("?,").repeated(idsToUpdate.size());
            idPlaceholders.chop(1); // 移除最后一个逗号
            updateSQL = QString("UPDATE vector_table_data SET timeset_id = ? WHERE id IN (%1)").arg(idPlaceholders);
            query.prepare(updateSQL);
            query.addBindValue(timeSetId);
            foreach (int id, idsToUpdate)
            {
                query.addBindValue(id);
            }
            qDebug() << "填充TimeSet - 执行SQL (按ID):" << updateSQL;
            qDebug() << "参数: timesetId=" << timeSetId << ", IDs=" << idsToUpdate;
        }
        else
        {
            // 如果没有选定行，则更新整个表
            updateSQL = "UPDATE vector_table_data SET timeset_id = :timesetId WHERE table_id = :tableId";
            query.prepare(updateSQL);
            query.bindValue(":timesetId", timeSetId);
            query.bindValue(":tableId", tableId);
            qDebug() << "填充TimeSet - 执行SQL (全表):" << updateSQL;
            qDebug() << "参数: timesetId=" << timeSetId << ", tableId=" << tableId;
        }

        if (!query.exec())
        {
            QString errorText = query.lastError().text();
            qDebug() << "填充TimeSet失败 - SQL错误:" << errorText;
            throw std::runtime_error(errorText.toStdString());
        }

        int rowsAffected = query.numRowsAffected();
        qDebug() << "填充TimeSet - 已更新" << rowsAffected << "行";

        // 提交事务
        if (!db.commit())
        {
            QString errorText = db.lastError().text();
            qDebug() << "填充TimeSet失败 - 提交事务失败:" << errorText;
            throw std::runtime_error(errorText.toStdString());
        }

        // 重新加载表格数据
        onVectorTableSelectionChanged(currentIndex);
        qDebug() << "填充TimeSet - 已重新加载表格数据";

        // 显示成功消息
        QMessageBox::information(this, tr("成功"), tr("TimeSet填充完成"));
        qDebug() << "填充TimeSet - 操作成功完成";
    }
    catch (const std::exception &e)
    {
        // 回滚事务
        db.rollback();
        qDebug() << "填充TimeSet - 操作失败，已回滚事务:" << e.what();

        // 显示错误消息
        QMessageBox::critical(this, tr("错误"), tr("填充TimeSet失败: %1").arg(e.what()));
    }
}

void MainWindow::showReplaceTimeSetDialog()
{
    // 检查是否有向量表被选中
    int currentIndex = m_vectorTableSelector->currentIndex();
    if (currentIndex < 0)
    {
        QMessageBox::warning(this, tr("警告"), tr("请先选择一个向量表"));
        return;
    }

    // 获取当前向量表ID
    int tableId = m_vectorTableSelector->itemData(currentIndex).toInt();

    // 获取向量表行数
    int rowCount = m_dataHandler->getVectorTableRowCount(tableId);
    if (rowCount <= 0)
    {
        QMessageBox::warning(this, tr("警告"), tr("当前向量表没有行数据"));
        return;
    }

    // 创建替换TimeSet对话框
    ReplaceTimeSetDialog dialog(this);
    dialog.setVectorRowCount(rowCount);

    // 获取选中的UI行 (0-based index)
    QList<int> selectedUiRows;
    QModelIndexList selectedIndexes = m_vectorTableWidget->selectionModel()->selectedRows();
    if (!selectedIndexes.isEmpty())
    {
        int minRow = INT_MAX;
        int maxRow = INT_MIN;
        foreach (const QModelIndex &index, selectedIndexes)
        {
            int uiRow = index.row();
            selectedUiRows.append(uiRow);
            minRow = qMin(minRow, uiRow);
            maxRow = qMax(maxRow, uiRow);
        }

        // 设置对话框的起始行和结束行，主要为了显示，实际更新使用selectedUiRows
        dialog.setSelectedRange(minRow + 1, maxRow + 1);
    }

    if (dialog.exec() == QDialog::Accepted)
    {
        // 获取用户输入
        int fromTimeSetId = dialog.getFromTimeSetId();
        int toTimeSetId = dialog.getToTimeSetId();

        // 替换TimeSet，传递选中的UI行列表
        replaceTimeSetForVectorTable(fromTimeSetId, toTimeSetId, selectedUiRows);
    }
}

void MainWindow::replaceTimeSetForVectorTable(int fromTimeSetId, int toTimeSetId, const QList<int> &selectedUiRows)
{
    // 添加调试日志
    qDebug() << "替换TimeSet开始 - 从TimeSet ID:" << fromTimeSetId << " 到TimeSet ID:" << toTimeSetId;
    if (!selectedUiRows.isEmpty())
    {
        QStringList rowList;
        for (int row : selectedUiRows)
        {
            rowList << QString::number(row);
        }
        qDebug() << "替换TimeSet - 针对UI行:" << rowList.join(',');
    }
    else
    {
        qDebug() << "替换TimeSet - 未选择行，将应用于整个表";
    }

    // 检查TimeSet ID有效性
    if (fromTimeSetId <= 0 || toTimeSetId <= 0)
    {
        QMessageBox::warning(this, tr("错误"), tr("TimeSet ID无效"));
        qDebug() << "替换TimeSet参数无效 - FromTimeSet ID:" << fromTimeSetId << ", ToTimeSet ID:" << toTimeSetId;
        return;
    }

    // 获取当前向量表ID
    int currentIndex = m_vectorTableSelector->currentIndex();
    if (currentIndex < 0)
    {
        qDebug() << "替换TimeSet失败 - 未选择向量表";
        return;
    }
    int tableId = m_vectorTableSelector->itemData(currentIndex).toInt();
    QString tableName = m_vectorTableSelector->currentText();
    qDebug() << "替换TimeSet - 当前向量表ID:" << tableId << ", 名称:" << tableName;

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接失败"));
        qDebug() << "替换TimeSet失败 - 数据库连接失败";
        return;
    }

    // 获取TimeSet名称用于日志
    QSqlQuery fromNameQuery(db);
    fromNameQuery.prepare("SELECT timeset_name FROM timeset_list WHERE id = ?");
    fromNameQuery.addBindValue(fromTimeSetId);
    QString fromTimeSetName = "未知";
    if (fromNameQuery.exec() && fromNameQuery.next())
    {
        fromTimeSetName = fromNameQuery.value(0).toString();
    }

    QSqlQuery toNameQuery(db);
    toNameQuery.prepare("SELECT timeset_name FROM timeset_list WHERE id = ?");
    toNameQuery.addBindValue(toTimeSetId);
    QString toTimeSetName = "未知";
    if (toNameQuery.exec() && toNameQuery.next())
    {
        toTimeSetName = toNameQuery.value(0).toString();
    }
    qDebug() << "替换TimeSet - 查找:" << fromTimeSetName << "(" << fromTimeSetId << ") 替换:" << toTimeSetName << "(" << toTimeSetId << ")";

    // 开始事务
    db.transaction();

    try
    {
        QList<int> idsToUpdate; // 存储要更新的数据库行的ID

        if (!selectedUiRows.isEmpty())
        {
            // 1. 获取表中所有行的ID，按sort_index排序
            QList<int> allRowIds;
            QSqlQuery idQuery(db);
            QString idSql = QString("SELECT id FROM vector_table_data WHERE table_id = %1 ORDER BY sort_index").arg(tableId);
            qDebug() << "替换TimeSet - 查询所有行ID:" << idSql;
            if (!idQuery.exec(idSql))
            {
                throw std::runtime_error(("查询行ID失败: " + idQuery.lastError().text()).toStdString());
            }
            while (idQuery.next())
            {
                allRowIds.append(idQuery.value(0).toInt());
            }
            qDebug() << "替换TimeSet - 数据库中共有" << allRowIds.size() << "行";

            // 2. 根据选中的UI行索引，找到对应的数据库ID
            foreach (int uiRow, selectedUiRows)
            {
                if (uiRow >= 0 && uiRow < allRowIds.size())
                {
                    idsToUpdate.append(allRowIds[uiRow]);
                }
                else
                {
                    qDebug() << "替换TimeSet警告 - UI行索引" << uiRow << "无效，忽略";
                }
            }
            if (idsToUpdate.isEmpty())
            {
                qDebug() << "替换TimeSet - 没有有效的数据库ID需要更新，操作取消";
                db.rollback(); // 不需要执行事务，直接回滚
                return;
            }
            qDebug() << "替换TimeSet - 准备更新的数据库ID:" << idsToUpdate;
        }

        // 准备更新SQL
        QSqlQuery query(db);
        QString updateSQL;
        if (!idsToUpdate.isEmpty())
        {
            // 如果有选定行，则基于ID更新
            QString idPlaceholders = QString("?,").repeated(idsToUpdate.size());
            idPlaceholders.chop(1); // 移除最后一个逗号
            updateSQL = QString("UPDATE vector_table_data SET timeset_id = ? WHERE id IN (%1) AND timeset_id = ?").arg(idPlaceholders);
            query.prepare(updateSQL);
            query.addBindValue(toTimeSetId);
            foreach (int id, idsToUpdate)
            {
                query.addBindValue(id);
            }
            query.addBindValue(fromTimeSetId); // 仅更新匹配源TimeSet的行
            qDebug() << "替换TimeSet - 执行SQL (按ID):" << updateSQL;
            qDebug() << "参数: fromTimesetId=" << fromTimeSetId << ", toTimesetId=" << toTimeSetId << ", IDs=" << idsToUpdate;
        }
        else
        {
            // 如果没有选定行，则更新整个表
            updateSQL = "UPDATE vector_table_data SET timeset_id = :toTimesetId WHERE table_id = :tableId AND timeset_id = :fromTimesetId";
            query.prepare(updateSQL);
            query.bindValue(":toTimesetId", toTimeSetId);
            query.bindValue(":tableId", tableId);
            query.bindValue(":fromTimesetId", fromTimeSetId); // 仅更新匹配源TimeSet的行
            qDebug() << "替换TimeSet - 执行SQL (全表):" << updateSQL;
            qDebug() << "参数: fromTimesetId=" << fromTimeSetId << ", toTimesetId=" << toTimeSetId << ", tableId=" << tableId;
        }

        if (!query.exec())
        {
            QString errorText = query.lastError().text();
            qDebug() << "替换TimeSet失败 - SQL错误:" << errorText;
            throw std::runtime_error(errorText.toStdString());
        }

        int rowsAffected = query.numRowsAffected();
        qDebug() << "替换TimeSet - 已更新" << rowsAffected << "行";

        // 提交事务
        if (!db.commit())
        {
            QString errorText = db.lastError().text();
            qDebug() << "替换TimeSet失败 - 提交事务失败:" << errorText;
            throw std::runtime_error(errorText.toStdString());
        }

        // 重新加载表格数据
        onVectorTableSelectionChanged(currentIndex);
        qDebug() << "替换TimeSet - 已重新加载表格数据";

        // 显示成功消息
        QMessageBox::information(this, tr("成功"), tr("TimeSet替换完成"));
        qDebug() << "替换TimeSet - 操作成功完成";
    }
    catch (const std::exception &e)
    {
        // 回滚事务
        db.rollback();
        qDebug() << "替换TimeSet - 操作失败，已回滚事务:" << e.what();

        // 显示错误消息
        QMessageBox::critical(this, tr("错误"), tr("替换TimeSet失败: %1").arg(e.what()));
    }
}

void MainWindow::addVectorRow(QTableWidget *table, const QStringList &pinOptions, int rowIdx)
{
    VectorDataHandler::addVectorRow(table, pinOptions, rowIdx);
}

bool MainWindow::addPinsToDatabase(const QList<QString> &pinNames)
{
    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        return false;
    }

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();

    // 开始事务
    db.transaction();

    // 循环添加每个管脚
    bool success = true;
    for (const QString &pinName : pinNames)
    {
        QSqlQuery query(db);
        query.prepare("INSERT INTO pin_list (pin_name, pin_note, pin_nav_note) VALUES (?, ?, ?)");
        query.addBindValue(pinName);
        query.addBindValue(""); // pin_note为空
        query.addBindValue(""); // pin_nav_note为空

        if (!query.exec())
        {
            qDebug() << "添加管脚失败:" << query.lastError().text();
            success = false;
            break;
        }
    }

    // 提交或回滚事务
    if (success)
    {
        db.commit();
        return true;
    }
    else
    {
        db.rollback();
        return false;
    }
}

// 刷新当前向量表数据
void MainWindow::refreshVectorTableData()
{
    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        QMessageBox::warning(this, "警告", "请先打开或创建一个项目数据库");
        return;
    }

    // 检查是否有选中的向量表
    if (m_vectorTableSelector->count() == 0 || m_vectorTableSelector->currentIndex() < 0)
    {
        QMessageBox::warning(this, "警告", "请先选择一个向量表");
        return;
    }

    // 获取当前选中表ID
    int tableId = m_vectorTableSelector->currentData().toInt();
    QString tableName = m_vectorTableSelector->currentText();

    // 使用数据处理器重新加载数据
    if (m_dataHandler->loadVectorTableData(tableId, m_vectorTableWidget))
    {
        statusBar()->showMessage(QString("已刷新向量表: %1").arg(tableName));
    }
    else
    {
        statusBar()->showMessage("刷新向量表失败");
        QMessageBox::warning(this, "刷新失败", "无法刷新向量表数据，请检查数据库连接");
    }
}

// 打开TimeSet设置对话框
void MainWindow::openTimeSetSettingsDialog()
{
    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        QMessageBox::warning(this, "警告", "请先打开或创建一个项目数据库");
        return;
    }

    // 显示TimeSet设置对话框
    if (showTimeSetDialog(false))
    {
        // 如果有修改，刷新界面
        if (m_itemDelegate)
        {
            m_itemDelegate->refreshCache();
        }

        // 重新加载当前向量表数据
        int currentIndex = m_vectorTableSelector->currentIndex();
        if (currentIndex >= 0)
        {
            onVectorTableSelectionChanged(currentIndex);
        }

        statusBar()->showMessage("TimeSet设置已更新");
    }
}

void MainWindow::setupVectorTablePins()
{
    qDebug() << "MainWindow::setupVectorTablePins - 开始设置向量表管脚";

    // 获取当前选择的向量表
    if (m_vectorTableSelector->count() == 0 || m_vectorTableSelector->currentIndex() < 0)
    {
        QMessageBox::warning(this, "错误", "请先选择一个向量表");
        qDebug() << "MainWindow::setupVectorTablePins - 没有选择向量表";
        return;
    }

    // 获取表ID和名称
    int tableId = m_vectorTableSelector->currentData().toInt();
    QString tableName = m_vectorTableSelector->currentText();

    qDebug() << "MainWindow::setupVectorTablePins - 打开管脚设置对话框，表ID:" << tableId << "，表名:" << tableName;

    // 创建并显示管脚设置对话框
    VectorPinSettingsDialog dialog(tableId, tableName, this);

    if (dialog.exec() == QDialog::Accepted)
    {
        qDebug() << "MainWindow::setupVectorTablePins - 管脚设置已更新，刷新表格";

        // 刷新当前向量表
        onVectorTableSelectionChanged(m_vectorTableSelector->currentIndex());

        QMessageBox::information(this, "成功", "向量表管脚设置已更新");
    }
    else
    {
        qDebug() << "MainWindow::setupVectorTablePins - 用户取消了管脚设置";
    }
}

void MainWindow::deleteVectorRowsInRange()
{
    qDebug() << "MainWindow::deleteVectorRowsInRange - 开始处理删除指定范围内的向量行";

    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        QMessageBox::warning(this, "警告", "请先打开或创建一个项目数据库");
        return;
    }

    // 检查是否有选中的向量表
    if (m_vectorTableSelector->count() == 0 || m_vectorTableSelector->currentIndex() < 0)
    {
        QMessageBox::warning(this, "警告", "请先选择一个向量表");
        return;
    }

    // 获取当前选中的向量表ID
    int tableId = m_vectorTableSelector->currentData().toInt();

    // 获取向量表总行数
    int totalRows = m_dataHandler->getVectorTableRowCount(tableId);
    if (totalRows <= 0)
    {
        QMessageBox::warning(this, "警告", "当前向量表没有行数据");
        return;
    }

    qDebug() << "MainWindow::deleteVectorRowsInRange - 当前向量表总行数：" << totalRows;

    // 创建删除范围对话框
    DeleteRangeVectorDialog dialog(this);
    dialog.setMaxRow(totalRows);

    // 获取当前选中的行
    QModelIndexList selectedIndexes = m_vectorTableWidget->selectionModel()->selectedRows();
    if (!selectedIndexes.isEmpty())
    {
        if (selectedIndexes.size() == 1)
        {
            // 只选中了一行
            int row = selectedIndexes.first().row() + 1; // 将0-based转为1-based
            dialog.setSelectedRange(row, row);

            qDebug() << "MainWindow::deleteVectorRowsInRange - 当前选中了单行：" << row;
        }
        else
        {
            // 选中了多行，找出最小和最大行号
            int minRow = INT_MAX;
            int maxRow = INT_MIN;

            for (const QModelIndex &index : selectedIndexes)
            {
                int row = index.row() + 1; // 将0-based转为1-based
                minRow = qMin(minRow, row);
                maxRow = qMax(maxRow, row);
            }

            dialog.setSelectedRange(minRow, maxRow);

            qDebug() << "MainWindow::deleteVectorRowsInRange - 当前选中了多行，范围：" << minRow << "到" << maxRow;
        }
    }
    else
    {
        // 没有选中行，文本框保持为空
        dialog.clearSelectedRange();

        qDebug() << "MainWindow::deleteVectorRowsInRange - 没有选中行，文本框保持为空";
    }

    // 显示对话框
    if (dialog.exec() == QDialog::Accepted)
    {
        // 获取用户输入的范围
        int fromRow = dialog.getFromRow();
        int toRow = dialog.getToRow();

        // 验证用户输入是否有效
        if (fromRow <= 0 || toRow <= 0)
        {
            QMessageBox::warning(this, "警告", "请输入有效的行范围");
            qDebug() << "MainWindow::deleteVectorRowsInRange - 用户输入的范围无效：" << fromRow << "到" << toRow;
            return;
        }

        qDebug() << "MainWindow::deleteVectorRowsInRange - 用户确认删除范围：" << fromRow << "到" << toRow;

        // 确认删除
        QMessageBox::StandardButton reply;
        int rowCount = toRow - fromRow + 1;
        reply = QMessageBox::question(this, "确认删除",
                                      "确定要删除第 " + QString::number(fromRow) + " 到 " +
                                          QString::number(toRow) + " 行（共 " + QString::number(rowCount) + " 行）吗？\n此操作不可撤销。",
                                      QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::No)
        {
            qDebug() << "MainWindow::deleteVectorRowsInRange - 用户取消删除操作";
            return;
        }

        // 执行删除操作
        QString errorMessage;
        if (m_dataHandler->deleteVectorRowsInRange(tableId, fromRow, toRow, errorMessage))
        {
            QMessageBox::information(this, "删除成功",
                                     "已成功删除第 " + QString::number(fromRow) + " 到 " +
                                         QString::number(toRow) + " 行（共 " + QString::number(rowCount) + " 行）");

            // 刷新表格
            onVectorTableSelectionChanged(m_vectorTableSelector->currentIndex());

            qDebug() << "MainWindow::deleteVectorRowsInRange - 成功删除指定范围内的行";
        }
        else
        {
            QMessageBox::critical(this, "删除失败", errorMessage);
            statusBar()->showMessage("删除行失败: " + errorMessage);

            qDebug() << "MainWindow::deleteVectorRowsInRange - 删除失败：" << errorMessage;
        }
    }
}

// 关闭Tab页签
void MainWindow::closeTab(int index)
{
    if (index < 0 || index >= m_vectorTabWidget->count())
        return;

    qDebug() << "MainWindow::closeTab - 关闭Tab页签，索引:" << index;

    // 仅当有多个Tab页时才允许关闭
    if (m_vectorTabWidget->count() > 1)
    {
        int tableId = m_tabToTableId.value(index, -1);
        m_vectorTabWidget->removeTab(index);

        // 更新映射关系
        m_tabToTableId.remove(index);

        // 更新其他Tab的映射关系
        QMap<int, int> updatedMap;
        for (auto it = m_tabToTableId.begin(); it != m_tabToTableId.end(); ++it)
        {
            int oldIndex = it.key();
            int newIndex = oldIndex > index ? oldIndex - 1 : oldIndex;
            updatedMap[newIndex] = it.value();
        }
        m_tabToTableId = updatedMap;

        qDebug() << "MainWindow::closeTab - Tab页签已关闭，剩余Tab页数:" << m_vectorTabWidget->count();
    }
    else
    {
        qDebug() << "MainWindow::closeTab - 无法关闭，这是最后一个Tab页";
        QMessageBox::information(this, "提示", "至少需要保留一个Tab页签");
    }
}

// 添加单个管脚
void MainWindow::addSinglePin()
{
    qDebug() << "MainWindow::addSinglePin - 开始添加单个管脚";

    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        QMessageBox::warning(this, "警告", "请先打开或创建一个项目数据库");
        return;
    }

    // 弹出输入对话框，获取新管脚名称
    bool ok;
    QString pinName = QInputDialog::getText(this, "添加管脚",
                                            "请输入管脚名称：",
                                            QLineEdit::Normal,
                                            "", &ok);

    if (!ok || pinName.isEmpty())
    {
        qDebug() << "MainWindow::addSinglePin - 用户取消或输入为空";
        return;
    }

    qDebug() << "MainWindow::addSinglePin - 用户输入管脚名称:" << pinName;

    // 验证管脚名称是否已存在
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT COUNT(*) FROM pin_list WHERE LOWER(pin_name) = LOWER(?)");
    checkQuery.addBindValue(pinName);

    if (checkQuery.exec() && checkQuery.next())
    {
        int count = checkQuery.value(0).toInt();
        if (count > 0)
        {
            QMessageBox::warning(this, "错误", "已存在同名管脚");
            qDebug() << "MainWindow::addSinglePin - 已存在同名管脚";
            return;
        }
    }

    // 添加单个管脚到数据库
    QList<QString> pins;
    pins << pinName;
    if (addPinsToDatabase(pins))
    {
        QMessageBox::information(this, "成功", "管脚 " + pinName + " 已成功添加");
        qDebug() << "MainWindow::addSinglePin - 管脚添加成功";

        // 刷新当前向量表（如果有）
        if (m_vectorTableSelector->count() > 0 && m_vectorTableSelector->currentIndex() >= 0)
        {
            onVectorTableSelectionChanged(m_vectorTableSelector->currentIndex());
        }
    }
    else
    {
        QMessageBox::critical(this, "错误", "添加管脚失败，请检查数据库连接");
        qDebug() << "MainWindow::addSinglePin - 添加管脚失败";
    }
}

// 删除管脚
void MainWindow::deletePins()
{
    qDebug() << "MainWindow::deletePins - 开始删除管脚";

    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        QMessageBox::warning(this, "警告", "请先打开或创建一个项目数据库");
        return;
    }

    // 获取所有管脚列表
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery query(db);
    query.prepare("SELECT id, pin_name FROM pin_list ORDER BY pin_name");

    if (!query.exec())
    {
        QMessageBox::critical(this, "错误", "获取管脚列表失败: " + query.lastError().text());
        qDebug() << "MainWindow::deletePins - 获取管脚列表失败:" << query.lastError().text();
        return;
    }

    // 创建管脚选择对话框
    QDialog dialog(this);
    dialog.setWindowTitle("删除管脚");
    dialog.setMinimumWidth(300);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    // 添加说明标签
    QLabel *label = new QLabel("请选择要删除的管脚:", &dialog);
    layout->addWidget(label);

    // 添加管脚列表（使用复选框）
    QListWidget *pinList = new QListWidget(&dialog);
    layout->addWidget(pinList);

    // 存储管脚ID和名称的映射关系
    QMap<int, QString> pinMap;

    // 添加管脚到列表
    while (query.next())
    {
        int pinId = query.value(0).toInt();
        QString pinName = query.value(1).toString();

        QListWidgetItem *item = new QListWidgetItem(pinName, pinList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);

        pinMap[pinList->count() - 1] = pinName;
    }

    // 检查是否有可删除的管脚
    if (pinList->count() == 0)
    {
        QMessageBox::warning(this, "警告", "数据库中没有管脚可供删除");
        qDebug() << "MainWindow::deletePins - 数据库中没有管脚可供删除";
        return;
    }

    // 添加警告标签
    QLabel *warningLabel = new QLabel("警告: 删除管脚将会影响已使用该管脚的向量表！", &dialog);
    warningLabel->setStyleSheet("color: red;");
    layout->addWidget(warningLabel);

    // 添加按钮
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    // 显示对话框
    if (dialog.exec() == QDialog::Accepted)
    {
        // 获取选中的管脚
        QStringList pinsToDelete;
        for (int i = 0; i < pinList->count(); ++i)
        {
            QListWidgetItem *item = pinList->item(i);
            if (item->checkState() == Qt::Checked)
            {
                pinsToDelete << item->text();
            }
        }

        // 检查是否选中了要删除的管脚
        if (pinsToDelete.isEmpty())
        {
            QMessageBox::warning(this, "警告", "未选择要删除的管脚");
            qDebug() << "MainWindow::deletePins - 未选择要删除的管脚";
            return;
        }

        // 确认删除
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "确认删除",
                                      "确定要删除选中的 " + QString::number(pinsToDelete.size()) + " 个管脚吗？\n"
                                                                                                   "此操作将影响所有使用这些管脚的向量表，且不可撤销。",
                                      QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::No)
        {
            qDebug() << "MainWindow::deletePins - 用户取消删除操作";
            return;
        }

        // 执行删除操作
        db.transaction();
        bool success = true;
        QString errorMsg;

        for (const QString &pinName : pinsToDelete)
        {
            // 首先获取管脚ID
            QSqlQuery idQuery(db);
            idQuery.prepare("SELECT id FROM pin_list WHERE pin_name = ?");
            idQuery.addBindValue(pinName);

            if (idQuery.exec() && idQuery.next())
            {
                int pinId = idQuery.value(0).toInt();

                // 删除向量表与管脚的关联
                QSqlQuery deleteRelationQuery(db);
                deleteRelationQuery.prepare("DELETE FROM vector_table_pins WHERE pin_id = ?");
                deleteRelationQuery.addBindValue(pinId);

                if (!deleteRelationQuery.exec())
                {
                    success = false;
                    errorMsg = deleteRelationQuery.lastError().text();
                    qDebug() << "MainWindow::deletePins - 删除管脚关联失败:" << errorMsg;
                    break;
                }

                // 删除管脚本身
                QSqlQuery deletePinQuery(db);
                deletePinQuery.prepare("DELETE FROM pin_list WHERE id = ?");
                deletePinQuery.addBindValue(pinId);

                if (!deletePinQuery.exec())
                {
                    success = false;
                    errorMsg = deletePinQuery.lastError().text();
                    qDebug() << "MainWindow::deletePins - 删除管脚失败:" << errorMsg;
                    break;
                }
            }
            else
            {
                success = false;
                errorMsg = idQuery.lastError().text();
                qDebug() << "MainWindow::deletePins - 查询管脚ID失败:" << errorMsg;
                break;
            }
        }

        // 提交或回滚事务
        if (success)
        {
            db.commit();
            QMessageBox::information(this, "成功", "已成功删除 " + QString::number(pinsToDelete.size()) + " 个管脚");
            qDebug() << "MainWindow::deletePins - 成功删除" << pinsToDelete.size() << "个管脚";

            // 刷新当前向量表（如果有）
            if (m_vectorTableSelector->count() > 0 && m_vectorTableSelector->currentIndex() >= 0)
            {
                onVectorTableSelectionChanged(m_vectorTableSelector->currentIndex());
            }
        }
        else
        {
            db.rollback();
            QMessageBox::critical(this, "错误", "删除管脚失败: " + errorMsg);
            qDebug() << "MainWindow::deletePins - 删除失败，已回滚事务";
        }
    }
}

// 打开管脚设置对话框
void MainWindow::openPinSettingsDialog()
{
    qDebug() << "MainWindow::openPinSettingsDialog - 打开管脚设置对话框";

    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        QMessageBox::warning(this, "警告", "请先打开或创建一个项目数据库");
        return;
    }

    // 创建并显示管脚设置对话框
    PinSettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted)
    {
        qDebug() << "MainWindow::openPinSettingsDialog - 管脚设置已更新";
        // 刷新当前向量表（如果有）
        if (m_vectorTableSelector->count() > 0 && m_vectorTableSelector->currentIndex() >= 0)
        {
            onVectorTableSelectionChanged(m_vectorTableSelector->currentIndex());
        }
        QMessageBox::information(this, "成功", "管脚设置已更新");
    }
    else
    {
        qDebug() << "MainWindow::openPinSettingsDialog - 用户取消了管脚设置";
    }
}

// 跳转到指定行
void MainWindow::gotoLine()
{
    qDebug() << "MainWindow::gotoLine - 开始跳转到指定行";

    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        QMessageBox::warning(this, "警告", "请先打开或创建一个项目数据库");
        return;
    }

    // 检查是否有选中的向量表
    if (m_vectorTableSelector->count() == 0 || m_vectorTableSelector->currentIndex() < 0)
    {
        QMessageBox::warning(this, "警告", "请先选择一个向量表");
        return;
    }

    // 检查表格是否有数据
    int rowCount = m_vectorTableWidget->rowCount();
    if (rowCount <= 0)
    {
        QMessageBox::warning(this, "警告", "当前向量表没有数据");
        return;
    }

    qDebug() << "MainWindow::gotoLine - 当前表格有" << rowCount << "行数据";

    // 输入对话框获取行号
    bool ok;
    int targetLine = QInputDialog::getInt(this, "跳转到行",
                                          "请输入行号 (1-" + QString::number(rowCount) + "):",
                                          1, 1, rowCount, 1, &ok);

    if (!ok)
    {
        qDebug() << "MainWindow::gotoLine - 用户取消了跳转";
        return;
    }

    qDebug() << "MainWindow::gotoLine - 用户输入的行号:" << targetLine;

    // 跳转到指定行（行号需要转换为0-based索引）
    int rowIndex = targetLine - 1;
    if (rowIndex >= 0 && rowIndex < rowCount)
    {
        // 清除当前选择
        m_vectorTableWidget->clearSelection();

        // 选中目标行
        m_vectorTableWidget->selectRow(rowIndex);

        // 滚动到目标行
        m_vectorTableWidget->scrollTo(m_vectorTableWidget->model()->index(rowIndex, 0), QAbstractItemView::PositionAtCenter);

        qDebug() << "MainWindow::gotoLine - 已跳转到第" << targetLine << "行";
    }
    else
    {
        QMessageBox::warning(this, "错误", "无效的行号");
        qDebug() << "MainWindow::gotoLine - 无效的行号:" << targetLine;
    }
}

// 字体缩放滑块值改变响应
void MainWindow::onFontZoomSliderValueChanged(int value)
{
    qDebug() << "MainWindow::onFontZoomSliderValueChanged - 调整字体缩放值:" << value;

    // 计算缩放因子 (从50%到200%)
    double scaleFactor = value / 100.0;

    // 更新向量表字体大小
    if (m_vectorTableWidget)
    {
        QFont font = m_vectorTableWidget->font();
        int baseSize = 9; // 默认字体大小
        font.setPointSizeF(baseSize * scaleFactor);
        m_vectorTableWidget->setFont(font);

        // 更新表头字体
        QFont headerFont = m_vectorTableWidget->horizontalHeader()->font();
        headerFont.setPointSizeF(baseSize * scaleFactor);
        m_vectorTableWidget->horizontalHeader()->setFont(headerFont);
        m_vectorTableWidget->verticalHeader()->setFont(headerFont);

        // 调整行高以适应字体大小
        m_vectorTableWidget->verticalHeader()->setDefaultSectionSize(qMax(25, int(25 * scaleFactor)));

        qDebug() << "MainWindow::onFontZoomSliderValueChanged - 字体大小已调整为:" << font.pointSizeF();
    }
}

// 字体缩放重置响应
void MainWindow::onFontZoomReset()
{
    qDebug() << "MainWindow::onFontZoomReset - 重置字体缩放";

    // 重置字体大小到默认值
    if (m_vectorTableWidget)
    {
        QFont font = m_vectorTableWidget->font();
        font.setPointSizeF(9); // 恢复默认字体大小
        m_vectorTableWidget->setFont(font);

        // 重置表头字体
        QFont headerFont = m_vectorTableWidget->horizontalHeader()->font();
        headerFont.setPointSizeF(9);
        m_vectorTableWidget->horizontalHeader()->setFont(headerFont);
        m_vectorTableWidget->verticalHeader()->setFont(headerFont);

        // 重置行高
        m_vectorTableWidget->verticalHeader()->setDefaultSectionSize(25);

        qDebug() << "MainWindow::onFontZoomReset - 字体大小已重置为默认值";
    }
}

void MainWindow::showPinGroupDialog()
{
    qDebug() << "MainWindow::showPinGroupDialog - 显示管脚分组对话框";

    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        qDebug() << "MainWindow::showPinGroupDialog - 没有打开的数据库";
        QMessageBox::warning(this, tr("警告"), tr("请先打开或创建项目"));
        return;
    }

    // 使用对话框管理器显示分组对话框
    bool success = m_dialogManager->showPinGroupDialog();

    if (success)
    {
        qDebug() << "MainWindow::showPinGroupDialog - 分组创建成功";
        statusBar()->showMessage(tr("管脚分组已成功添加"));
    }
    else
    {
        qDebug() << "MainWindow::showPinGroupDialog - 分组创建取消或失败";
    }
}
