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
#include "common/binary_file_format.h"
#include "database/binaryfilehelper.h"
#include "common/utils/pathutils.h" // 修正包含路径

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
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QSplitter>
#include <QSizePolicy>
#include <QSettings>
#include <QCloseEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_isUpdatingUI(false)
{
    setupUI();
    setupMenu();

    // 创建对话框管理器
    m_dialogManager = new DialogManager(this);

    // 设置窗口标题和大小
    setWindowTitle("VecEdit - 矢量测试编辑器");

    // 初始窗口大小（如果没有保存的状态）
    resize(1024, 768);

    // 初始化窗口大小信息标签
    m_windowSizeLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_windowSizeLabel);
    updateWindowSizeInfo();

    // 显示就绪状态
    statusBar()->showMessage("就绪");

    // 加载所有向量表
    loadVectorTable();

    // 启用自由缩放窗口
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(800, 600); // 设置窗口最小尺寸

    // 恢复上次的窗口状态
    restoreWindowState();

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

        // 检查和修复所有向量表的列配置
        checkAndFixAllVectorTables();

        // 设置窗口标题
        setWindowTitle(tr("VecEdit - 矢量测试编辑器 [%1]").arg(QFileInfo(dbPath).fileName()));

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

bool MainWindow::showTimeSetDialog(bool isNewTable)
{
    qDebug() << "MainWindow::showTimeSetDialog - 显示TimeSet设置对话框，isNewTable:" << isNewTable;

    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        QMessageBox::warning(this, "警告", "请先打开或创建一个项目数据库");
        return false;
    }

    // 创建TimeSet对话框，如果是新表则传递isInitialSetup=true参数
    TimeSetDialog dialog(this, isNewTable);

    // 显示对话框
    if (dialog.exec() == QDialog::Accepted)
    {
        qDebug() << "MainWindow::showTimeSetDialog - 用户确认TimeSet设置";
        return true;
    }
    else
    {
        qDebug() << "MainWindow::showTimeSetDialog - 用户取消TimeSet设置";
        return false;
    }
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
    m_vectorTableSelector->setObjectName(QStringLiteral("m_vectorTableSelector")); // <--- 添加这一行
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

    const QString funcName = "MainWindow::onVectorTableSelectionChanged";
    qDebug() << funcName << " - 向量表选择已更改，索引:" << index;

    // 获取当前选中表ID
    int tableId = m_vectorTableSelector->currentData().toInt();
    qDebug() << funcName << " - 当前表ID:" << tableId;

    // 刷新代理的表ID缓存
    if (m_itemDelegate)
    {
        qDebug() << funcName << " - 刷新代理表ID缓存";
        m_itemDelegate->refreshTableIdCache();
    }

    // 同步Tab页签选择
    syncTabWithComboBox(index);

    // 尝试修复当前表（如果需要）
    bool needsFix = false;
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT COUNT(*) FROM VectorTableColumnConfiguration WHERE master_record_id = ?");
    checkQuery.addBindValue(tableId);
    if (checkQuery.exec() && checkQuery.next())
    {
        int columnCount = checkQuery.value(0).toInt();
        qDebug() << funcName << " - 表 " << tableId << " 当前有 " << columnCount << " 个列配置";
        needsFix = (columnCount == 0);
    }

    if (needsFix)
    {
        qDebug() << funcName << " - 表 " << tableId << " 需要修复列配置";
        fixExistingTableWithoutColumns(tableId);
    }

    // 使用数据处理器加载数据
    qDebug() << funcName << " - 开始加载表格数据，表ID:" << tableId;
    bool loadSuccess = VectorDataHandler::instance().loadVectorTableData(tableId, m_vectorTableWidget);
    qDebug() << funcName << " - VectorDataHandler::loadVectorTableData 返回:" << loadSuccess
             << "，表ID:" << tableId
             << "，列数:" << m_vectorTableWidget->columnCount();

    if (loadSuccess)
    {
        qDebug() << funcName << " - 表格加载成功，列数:" << m_vectorTableWidget->columnCount();

        // 如果列数太少（只有管脚列，没有标准列），可能需要重新加载
        if (m_vectorTableWidget->columnCount() < 6)
        {
            qWarning() << funcName << " - 警告：列数太少（" << m_vectorTableWidget->columnCount()
                       << "），可能缺少标准列。尝试修复...";
            fixExistingTableWithoutColumns(tableId);
            // 重新加载表格
            loadSuccess = VectorDataHandler::instance().loadVectorTableData(tableId, m_vectorTableWidget);
            qDebug() << funcName << " - 修复后重新加载，结果:" << loadSuccess
                     << "，列数:" << m_vectorTableWidget->columnCount();
        }

        // 应用表格样式（优化版本，一次性完成所有样式设置，包括列宽和对齐）
        TableStyleManager::applyBatchTableStyle(m_vectorTableWidget);

        // 输出每一列的标题，用于调试
        QStringList columnHeaders;
        for (int i = 0; i < m_vectorTableWidget->columnCount(); i++)
        {
            QTableWidgetItem *headerItem = m_vectorTableWidget->horizontalHeaderItem(i);
            QString headerText = headerItem ? headerItem->text() : QString("列%1").arg(i);
            columnHeaders << headerText;
        }
        qDebug() << funcName << " - 表头列表:" << columnHeaders.join(", ");

        statusBar()->showMessage(QString("已加载向量表: %1，列数: %2").arg(m_vectorTableSelector->currentText()).arg(m_vectorTableWidget->columnCount()));
    }
    else
    {
        qWarning() << funcName << " - 表格加载失败，表ID:" << tableId;
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
    if (m_isUpdatingUI || index < 0)
        return;

    m_isUpdatingUI = true;
    syncComboBoxWithTab(index);
    m_isUpdatingUI = false;
}

void MainWindow::syncComboBoxWithTab(int tabIndex)
{
    if (tabIndex < 0 || tabIndex >= m_vectorTabWidget->count())
        return;

    int tableId = m_tabToTableId.value(tabIndex, -1);
    if (tableId < 0)
        return;

    // 找到对应的下拉框索引
    for (int i = 0; i < m_vectorTableSelector->count(); i++)
    {
        if (m_vectorTableSelector->itemData(i).toInt() == tableId)
        {
            m_vectorTableSelector->setCurrentIndex(i);

            // 刷新代理的表ID缓存
            if (m_itemDelegate)
            {
                qDebug() << "MainWindow::syncComboBoxWithTab - 刷新代理表ID缓存";
                m_itemDelegate->refreshTableIdCache();
            }

            // 检查当前是否显示向量表界面，如果不是则切换
            if (m_welcomeWidget->isVisible())
            {
                m_welcomeWidget->setVisible(false);
                m_vectorTableContainer->setVisible(true);
            }

            // 重新加载数据
            if (tableId > 0)
            {
                // 使用数据处理器加载数据
                if (VectorDataHandler::instance().loadVectorTableData(tableId, m_vectorTableWidget))
                {
                    qDebug() << "MainWindow::syncComboBoxWithTab - 成功重新加载表格数据，列数:" << m_vectorTableWidget->columnCount();
                    // 应用表格样式（优化版本，一次性完成所有样式设置，包括列宽和对齐）
                    TableStyleManager::applyBatchTableStyle(m_vectorTableWidget);

                    // 输出每一列的标题，用于调试
                    QStringList columnHeaders;
                    for (int i = 0; i < m_vectorTableWidget->columnCount(); i++)
                    {
                        QTableWidgetItem *headerItem = m_vectorTableWidget->horizontalHeaderItem(i);
                        QString headerText = headerItem ? headerItem->text() : QString("列%1").arg(i);
                        columnHeaders << headerText;
                    }
                    qDebug() << "MainWindow::syncComboBoxWithTab - 表头列表:" << columnHeaders.join(", ");
                }
            }

            break;
        }
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
    const QString funcName = "MainWindow::saveVectorTableData";
    qDebug() << funcName << " - 开始保存数据";

    // 获取当前选择的向量表
    QString currentTable = m_vectorTableSelector->currentText();
    if (currentTable.isEmpty())
    {
        QMessageBox::warning(this, "保存失败", "请先选择一个向量表");
        return;
    }

    // 获取表ID
    int tableId = m_vectorTableSelector->currentData().toInt();

    // Ensure m_vectorTableWidget is the correct one associated with the current tab/selection
    QWidget *currentTabWidget = m_vectorTabWidget->currentWidget();
    QTableWidget *targetTableWidget = nullptr;

    if (currentTabWidget)
    {
        // Find the QTableWidget within the current tab. This assumes a specific structure.
        // If your tabs directly contain QTableWidget, this is simpler.
        // If they contain a layout which then contains the QTableWidget, you'll need to find it.
        targetTableWidget = currentTabWidget->findChild<QTableWidget *>();
    }

    if (!targetTableWidget)
    {
        // Fallback or if the structure is QTableWidget is directly managed by MainWindow for the active view
        targetTableWidget = m_vectorTableWidget;
        qDebug() << funcName << " - 未找到当前Tab页中的TableWidget, 回退到 m_vectorTableWidget";
    }

    if (!targetTableWidget)
    {
        QMessageBox::critical(this, "保存失败", "无法确定要保存的表格控件。");
        qCritical() << funcName << " - 无法确定要保存的表格控件。";
        return;
    }
    qDebug() << funcName << " - 目标表格控件已确定。";

    // 使用数据处理器保存数据
    QString errorMessage;
    if (VectorDataHandler::instance().saveVectorTableData(tableId, targetTableWidget, errorMessage))
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
    const QString funcName = "MainWindow::addNewVectorTable";
    qDebug() << funcName << " - 开始添加新向量表";

    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        qDebug() << funcName << " - 未打开数据库，操作取消";
        QMessageBox::warning(this, "警告", "请先打开或创建一个项目数据库");
        return;
    }

    qDebug() << funcName << " - 数据库已连接 (" << m_currentDbPath << ")，准备创建向量表";

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
        qDebug() << funcName << " - 用户输入的向量表名称:" << tableName;

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
            qDebug() << funcName << " - 新向量表创建成功，ID:" << newTableId << ", 名称:" << tableName;

            // 使用 PathUtils 获取项目特定的二进制数据目录
            // m_currentDbPath 应该是最新且正确的数据库路径
            QString projectBinaryDataDir = Utils::PathUtils::getProjectBinaryDataDirectory(m_currentDbPath);
            if (projectBinaryDataDir.isEmpty())
            {
                QMessageBox::critical(this, "错误", QString("无法为数据库 '%1' 生成二进制数据目录路径。").arg(m_currentDbPath));
                // 考虑是否需要回滚 vector_tables 中的插入
                return;
            }
            qDebug() << funcName << " - 项目二进制数据目录:" << projectBinaryDataDir;

            QDir dataDir(projectBinaryDataDir);
            if (!dataDir.exists())
            {
                if (!dataDir.mkpath(".")) // mkpath creates parent directories if necessary
                {
                    QMessageBox::critical(this, "错误", "无法创建项目二进制数据目录: " + projectBinaryDataDir);
                    // 考虑是否需要回滚 vector_tables 中的插入
                    return;
                }
                qDebug() << funcName << " - 已创建项目二进制数据目录: " << projectBinaryDataDir;
            }

            // 构造二进制文件名 (纯文件名)
            QString binaryOnlyFileName = QString("table_%1_data.vbindata").arg(newTableId);
            // 使用QDir::cleanPath确保路径格式正确，然后转换为本地路径格式
            QString absoluteBinaryFilePath = QDir::cleanPath(projectBinaryDataDir + QDir::separator() + binaryOnlyFileName);
            absoluteBinaryFilePath = QDir::toNativeSeparators(absoluteBinaryFilePath);
            qDebug() << funcName << " - 绝对二进制文件路径:" << absoluteBinaryFilePath;

            // 创建VectorTableMasterRecord记录
            QSqlQuery insertMasterQuery(db);
            insertMasterQuery.prepare("INSERT INTO VectorTableMasterRecord "
                                      "(original_vector_table_id, table_name, binary_data_filename, "
                                      "file_format_version, data_schema_version, row_count, column_count) "
                                      "VALUES (?, ?, ?, ?, ?, ?, ?)"); // Added one more ? for schema
            insertMasterQuery.addBindValue(newTableId);
            insertMasterQuery.addBindValue(tableName);
            insertMasterQuery.addBindValue(binaryOnlyFileName);                       // <--- 存储纯文件名
            insertMasterQuery.addBindValue(Persistence::CURRENT_FILE_FORMAT_VERSION); // 使用定义的版本
            insertMasterQuery.addBindValue(1);                                        // 初始数据 schema version
            insertMasterQuery.addBindValue(0);                                        // row_count
            insertMasterQuery.addBindValue(0);                                        // column_count

            if (!insertMasterQuery.exec())
            {
                qCritical() << funcName << " - 创建VectorTableMasterRecord记录失败: " << insertMasterQuery.lastError().text();
                QMessageBox::critical(this, "错误", "创建VectorTableMasterRecord记录失败: " + insertMasterQuery.lastError().text());
                // 考虑回滚 vector_tables 和清理目录
                return;
            }
            qDebug() << funcName << " - VectorTableMasterRecord记录创建成功 for table ID:" << newTableId;

            // 添加默认列配置
            if (!addDefaultColumnConfigurations(newTableId))
            {
                qCritical() << funcName << " - 无法为表ID " << newTableId << " 添加默认列配置";
                QMessageBox::critical(this, "错误", "无法添加默认列配置");
                // 考虑回滚 vector_tables 和清理目录
                return;
            }
            qDebug() << funcName << " - 已成功添加默认列配置";

            // 创建空的二进制文件
            QFile binaryFile(absoluteBinaryFilePath);
            if (binaryFile.exists())
            {
                qWarning() << funcName << " - 二进制文件已存在 (这不应该发生对于新表):" << absoluteBinaryFilePath;
                // Decide on handling: overwrite, error out, or skip? For now, let's attempt to open and write header.
            }

            if (!binaryFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) // Truncate if exists
            {
                qCritical() << funcName << " - 无法打开/创建二进制数据文件: " << absoluteBinaryFilePath << ", 错误: " << binaryFile.errorString();
                QMessageBox::critical(this, "错误", "创建向量表失败: 无法创建二进制数据文件: " + binaryFile.errorString()); // 更明确的错误信息

                // --- 开始回滚 ---
                QSqlQuery rollbackQuery(db);
                db.transaction(); // 开始事务以便原子回滚

                // 删除列配置
                rollbackQuery.prepare("DELETE FROM VectorTableColumnConfiguration WHERE master_record_id = ?");
                rollbackQuery.addBindValue(newTableId);
                if (!rollbackQuery.exec())
                {
                    qWarning() << funcName << " - 回滚: 删除列配置失败: " << rollbackQuery.lastError().text();
                    // 即使回滚失败也要继续尝试删除其他记录
                }

                // 删除主记录
                rollbackQuery.prepare("DELETE FROM VectorTableMasterRecord WHERE id = ?");
                rollbackQuery.addBindValue(newTableId);
                if (!rollbackQuery.exec())
                {
                    qWarning() << funcName << " - 回滚: 删除主记录失败: " << rollbackQuery.lastError().text();
                }

                // 删除 vector_tables 记录
                rollbackQuery.prepare("DELETE FROM vector_tables WHERE id = ?");
                rollbackQuery.addBindValue(newTableId);
                if (!rollbackQuery.exec())
                {
                    qWarning() << funcName << " - 回滚: 删除 vector_tables 记录失败: " << rollbackQuery.lastError().text();
                }

                if (!db.commit())
                { // 提交事务
                    qWarning() << funcName << " - 回滚事务提交失败: " << db.lastError().text();
                    db.rollback(); // 尝试再次回滚以防万一
                }
                qInfo() << funcName << " - 已执行数据库回滚操作 for table ID: " << newTableId;
                // --- 结束回滚 ---

                return;
            }

            // 创建并写入文件头
            BinaryFileHeader header; // 从 common/binary_file_format.h
            header.magic_number = Persistence::VEC_BINDATA_MAGIC;
            header.file_format_version = Persistence::CURRENT_FILE_FORMAT_VERSION;
            header.row_count_in_file = 0;
            header.column_count_in_file = 0; // 初始没有列
            header.data_schema_version = 1;  // 与 VectorTableMasterRecord 中的 schema version 一致
            header.timestamp_created = QDateTime::currentSecsSinceEpoch();
            header.timestamp_updated = header.timestamp_created;
            // header.reserved 保持默认 (0)

            qDebug() << funcName << " - 准备写入文件头到:" << absoluteBinaryFilePath;
            bool headerWriteSuccess = Persistence::BinaryFileHelper::writeBinaryHeader(&binaryFile, header);
            binaryFile.close();

            if (!headerWriteSuccess)
            {
                qCritical() << funcName << " - 无法写入二进制文件头到:" << absoluteBinaryFilePath;
                QMessageBox::critical(this, "错误", "创建向量表失败: 无法写入二进制文件头"); // 更明确的错误信息
                // 考虑回滚和删除文件
                QFile::remove(absoluteBinaryFilePath); // Attempt to clean up

                // --- 开始回滚 ---
                QSqlQuery rollbackQuery(db);
                db.transaction(); // 开始事务以便原子回滚

                // 删除列配置
                rollbackQuery.prepare("DELETE FROM VectorTableColumnConfiguration WHERE master_record_id = ?");
                rollbackQuery.addBindValue(newTableId);
                if (!rollbackQuery.exec())
                {
                    qWarning() << funcName << " - 回滚: 删除列配置失败: " << rollbackQuery.lastError().text();
                    // 即使回滚失败也要继续尝试删除其他记录
                }

                // 删除主记录
                rollbackQuery.prepare("DELETE FROM VectorTableMasterRecord WHERE id = ?");
                rollbackQuery.addBindValue(newTableId);
                if (!rollbackQuery.exec())
                {
                    qWarning() << funcName << " - 回滚: 删除主记录失败: " << rollbackQuery.lastError().text();
                }

                // 删除 vector_tables 记录
                rollbackQuery.prepare("DELETE FROM vector_tables WHERE id = ?");
                rollbackQuery.addBindValue(newTableId);
                if (!rollbackQuery.exec())
                {
                    qWarning() << funcName << " - 回滚: 删除 vector_tables 记录失败: " << rollbackQuery.lastError().text();
                }

                if (!db.commit())
                { // 提交事务
                    qWarning() << funcName << " - 回滚事务提交失败: " << db.lastError().text();
                    db.rollback(); // 尝试再次回滚以防万一
                }
                qInfo() << funcName << " - 已执行数据库回滚操作 for table ID: " << newTableId;
                // --- 结束回滚 ---

                return;
            }

            qInfo() << funcName << " - 已成功创建空的二进制文件并写入文件头: " << absoluteBinaryFilePath;

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
            qCritical() << funcName << " - 新向量表创建失败 (vector_tables): " << insertQuery.lastError().text();
            QMessageBox::critical(this, "数据库错误", "无法在数据库中创建新向量表: " + insertQuery.lastError().text());
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
    const QString funcName = "MainWindow::showVectorDataDialog";
    qDebug() << funcName << " - 开始显示向量行数据录入对话框";

    if (m_dialogManager)
    {
        qDebug() << funcName << " - 调用对话框管理器显示向量行数据录入对话框";
        bool success = m_dialogManager->showVectorDataDialog(tableId, tableName, startIndex);
        qDebug() << funcName << " - 向量行数据录入对话框返回结果:" << success;

        // 如果成功添加了数据，刷新表格显示
        if (success)
        {
            qDebug() << funcName << " - 成功添加向量行数据，准备刷新表格";

            // 找到对应的表索引并刷新
            int currentIndex = m_vectorTableSelector->findData(tableId);
            if (currentIndex >= 0)
            {
                qDebug() << funcName << " - 找到表索引:" << currentIndex << "，设置为当前表";

                // 先保存当前的列状态
                m_vectorTableSelector->setCurrentIndex(currentIndex);

                // 强制调用loadVectorTableData而不是依赖信号槽，确保正确加载所有列
                if (VectorDataHandler::instance().loadVectorTableData(tableId, m_vectorTableWidget))
                {
                    qDebug() << funcName << " - 成功重新加载表格数据，列数:" << m_vectorTableWidget->columnCount();
                    // 应用表格样式（优化版本，一次性完成所有样式设置，包括列宽和对齐）
                    TableStyleManager::applyBatchTableStyle(m_vectorTableWidget);

                    // 输出每一列的标题，用于调试
                    QStringList columnHeaders;
                    for (int i = 0; i < m_vectorTableWidget->columnCount(); i++)
                    {
                        QTableWidgetItem *headerItem = m_vectorTableWidget->horizontalHeaderItem(i);
                        QString headerText = headerItem ? headerItem->text() : QString("列%1").arg(i);
                        columnHeaders << headerText;
                    }
                    qDebug() << funcName << " - 表头列表:" << columnHeaders.join(", ");
                }
                else
                {
                    qWarning() << funcName << " - 重新加载表格数据失败";
                }
            }
            else
            {
                qWarning() << funcName << " - 未找到表ID:" << tableId << "对应的索引";
            }
        }
    }
}

// 删除当前选中的向量表
void MainWindow::deleteCurrentVectorTable()
{
    // 获取当前选择的向量表
    int currentIndex = m_vectorTableSelector->currentIndex();
    if (currentIndex < 0)
    {
        QMessageBox::warning(this, "删除失败", "没有选中的向量表");
        return;
    }

    QString tableName = m_vectorTableSelector->currentText();
    int tableId = m_vectorTableSelector->currentData().toInt();

    // 确认删除
    QMessageBox::StandardButton reply = QMessageBox::question(this, "确认删除",
                                                              QString("确定要删除向量表 \"%1\" 吗？此操作不可撤销。").arg(tableName),
                                                              QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes)
    {
        return;
    }

    // 使用数据处理器删除向量表
    QString errorMessage;
    if (VectorDataHandler::instance().deleteVectorTable(tableId, errorMessage))
    {
        // 记录当前选中的索引
        int previousIndex = m_vectorTableSelector->currentIndex();

        // 从下拉框和页签中移除
        m_vectorTableSelector->removeItem(currentIndex);

        // 查找对应的页签
        for (int i = 0; i < m_tabToTableId.size(); ++i)
        {
            if (m_tabToTableId.value(i) == tableId)
            {
                // 清除映射关系并删除页签
                m_tabToTableId.remove(i);
                m_vectorTabWidget->removeTab(i);
                break;
            }
        }

        // 如果还有剩余表，选中前一个表（如果可能）或者第一个表
        if (m_vectorTableSelector->count() > 0)
        {
            int newIndex = previousIndex - 1;
            if (newIndex < 0)
                newIndex = 0;
            m_vectorTableSelector->setCurrentIndex(newIndex);
        }
        else
        {
            // 如果没有剩余表，清空表格并显示欢迎界面
            m_vectorTableWidget->setRowCount(0);
            m_vectorTableWidget->setColumnCount(0);
            m_welcomeWidget->setVisible(true);
            m_vectorTableContainer->setVisible(false);
        }

        QMessageBox::information(this, "删除成功", QString("向量表 \"%1\" 已成功删除").arg(tableName));
        statusBar()->showMessage(QString("向量表 \"%1\" 已成功删除").arg(tableName));
    }
    else
    {
        QMessageBox::critical(this, "删除失败", errorMessage);
        statusBar()->showMessage("删除失败: " + errorMessage);
    }
}

// 删除选中的向量行
void MainWindow::deleteSelectedVectorRows()
{
    const QString funcName = "MainWindow::deleteSelectedVectorRows";
    qDebug() << funcName << " - 开始处理删除选中的向量行";

    // 获取当前选择的向量表
    int currentIndex = m_vectorTableSelector->currentIndex();
    if (currentIndex < 0)
    {
        QMessageBox::warning(this, "删除失败", "没有选中的向量表");
        qWarning() << funcName << " - 没有选中的向量表";
        return;
    }

    // 获取表ID和名称
    int tableId = m_vectorTableSelector->currentData().toInt();
    QString tableName = m_vectorTableSelector->currentText();
    qDebug() << funcName << " - 当前选择的向量表：" << tableName << "，ID：" << tableId;

    // 获取选中的行
    QModelIndexList selectedIndexes = m_vectorTableWidget->selectionModel()->selectedRows();
    if (selectedIndexes.isEmpty())
    {
        QMessageBox::warning(this, "删除失败", "请先选择要删除的行");
        qWarning() << funcName << " - 用户未选择任何行";
        return;
    }

    qDebug() << funcName << " - 用户选择了" << selectedIndexes.size() << "行";

    // 确认删除
    QMessageBox::StandardButton reply = QMessageBox::question(this, "确认删除",
                                                              QString("确定要删除选定的 %1 行吗？此操作不可撤销。").arg(selectedIndexes.size()),
                                                              QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes)
    {
        qDebug() << funcName << " - 用户取消了删除操作";
        return;
    }

    // 获取所有选中的行索引
    QList<int> selectedRows;
    foreach (const QModelIndex &index, selectedIndexes)
    {
        selectedRows.append(index.row());
        qDebug() << funcName << " - 选中的行索引：" << index.row();
    }

    // 使用数据处理器删除选中的行
    QString errorMessage;
    qDebug() << funcName << " - 开始调用VectorDataHandler::deleteVectorRows，参数：tableId=" << tableId << "，行数=" << selectedRows.size();
    if (VectorDataHandler::instance().deleteVectorRows(tableId, selectedRows, errorMessage))
    {
        QMessageBox::information(this, "删除成功", "已成功删除 " + QString::number(selectedRows.size()) + " 行数据");
        qDebug() << funcName << " - 删除操作成功完成";

        // 刷新表格
        onVectorTableSelectionChanged(currentIndex);
    }
    else
    {
        QMessageBox::critical(this, "删除失败", errorMessage);
        qWarning() << funcName << " - 删除失败：" << errorMessage;
        // 在状态栏显示错误消息
        statusBar()->showMessage("删除行失败: " + errorMessage, 5000);
    }
}

// 删除指定范围内的向量行
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
    int totalRows = VectorDataHandler::instance().getVectorTableRowCount(tableId);
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
        if (VectorDataHandler::instance().deleteVectorRowsInRange(tableId, fromRow, toRow, errorMessage))
        {
            QMessageBox::information(this, "删除成功",
                                     QString("已成功删除第 %1 到 %2 行（共 %3 行）").arg(fromRow).arg(toRow).arg(rowCount));

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

// 显示填充TimeSet对话框
void MainWindow::showFillTimeSetDialog()
{
    // 获取当前选择的向量表
    int currentIndex = m_vectorTableSelector->currentIndex();
    if (currentIndex < 0)
    {
        QMessageBox::warning(this, "操作失败", "没有选中的向量表");
        return;
    }

    // 获取表ID
    int tableId = m_vectorTableSelector->currentData().toInt();

    // 获取向量表行数
    int rowCount = VectorDataHandler::instance().getVectorTableRowCount(tableId);
    if (rowCount <= 0)
    {
        QMessageBox::warning(this, "操作失败", "当前向量表没有数据行");
        return;
    }

    // 显示填充TimeSet对话框
    FillTimeSetDialog dialog(this);
    // 设置向量表总行数
    dialog.setVectorRowCount(rowCount);

    // 获取选中的行（如果有）并设置默认范围
    QModelIndexList selectedIndexes = m_vectorTableWidget->selectionModel()->selectedRows();
    if (!selectedIndexes.isEmpty())
    {
        // 找出最小和最大行号（1-based）
        int minRow = INT_MAX;
        int maxRow = 0;
        foreach (const QModelIndex &index, selectedIndexes)
        {
            int rowIdx = index.row() + 1; // 转为1-based
            minRow = qMin(minRow, rowIdx);
            maxRow = qMax(maxRow, rowIdx);
        }
        if (minRow <= maxRow)
        {
            dialog.setSelectedRange(minRow, maxRow);
        }
    }

    if (dialog.exec() == QDialog::Accepted)
    {
        int timeSetId = dialog.getSelectedTimeSetId();

        // 获取选中的行
        QList<int> selectedRows;
        foreach (const QModelIndex &index, selectedIndexes)
        {
            selectedRows.append(index.row());
        }

        // 填充TimeSet
        fillTimeSetForVectorTable(timeSetId, selectedRows);
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
        // 1. 查询表对应的二进制文件路径
        QSqlQuery fileQuery(db);
        fileQuery.prepare("SELECT binary_data_filename FROM VectorTableMasterRecord WHERE id = ?");
        fileQuery.addBindValue(tableId);
        if (!fileQuery.exec() || !fileQuery.next())
        {
            QString errorText = fileQuery.lastError().text();
            qDebug() << "填充TimeSet - 查询二进制文件名失败:" << errorText;
            throw std::runtime_error(("查询二进制文件名失败: " + errorText).toStdString());
        }

        QString binFileName = fileQuery.value(0).toString();
        if (binFileName.isEmpty())
        {
            qDebug() << "填充TimeSet - 二进制文件名为空，无法进行填充操作";
            throw std::runtime_error("向量表未配置二进制文件存储，无法进行填充操作");
        }

        // 2. 解析二进制文件路径
        // 使用PathUtils获取项目二进制数据目录
        QString projectBinaryDataDir = Utils::PathUtils::getProjectBinaryDataDirectory(m_currentDbPath);
        if (projectBinaryDataDir.isEmpty())
        {
            QString errorMsg = QString("无法为数据库 '%1' 生成二进制数据目录路径").arg(m_currentDbPath);
            qWarning() << "填充TimeSet - " << errorMsg;
            throw std::runtime_error(errorMsg.toStdString());
        }

        qDebug() << "填充TimeSet - 项目二进制数据目录:" << projectBinaryDataDir;

        // 相对路径转绝对路径
        QString absoluteBinFilePath;
        if (QFileInfo(binFileName).isRelative())
        {
            absoluteBinFilePath = QDir(projectBinaryDataDir).absoluteFilePath(binFileName);
            qDebug() << "填充TimeSet - 相对路径转换为绝对路径:" << binFileName << " -> " << absoluteBinFilePath;
        }
        else
        {
            absoluteBinFilePath = binFileName;
        }

        // 3. 检查二进制文件是否存在
        if (!QFile::exists(absoluteBinFilePath))
        {
            qWarning() << "填充TimeSet - 二进制文件不存在:" << absoluteBinFilePath;
            throw std::runtime_error(("二进制文件不存在: " + absoluteBinFilePath).toStdString());
        }

        // 4. 查询列定义，找出TimeSet列
        QSqlQuery colQuery(db);
        colQuery.prepare("SELECT id, column_name, column_order, column_type, data_properties, IsVisible "
                         "FROM VectorTableColumnConfiguration "
                         "WHERE master_record_id = ? ORDER BY column_order");
        colQuery.addBindValue(tableId);

        if (!colQuery.exec())
        {
            QString errorText = colQuery.lastError().text();
            qWarning() << "填充TimeSet - 查询列定义失败:" << errorText;
            throw std::runtime_error(("查询列定义失败: " + errorText).toStdString());
        }

        QList<Vector::ColumnInfo> columns;
        int timeSetColumnIndex = -1; // 用于标记TimeSet列的索引

        while (colQuery.next())
        {
            Vector::ColumnInfo colInfo;
            colInfo.id = colQuery.value(0).toInt();
            colInfo.name = colQuery.value(1).toString();
            colInfo.order = colQuery.value(2).toInt();
            colInfo.original_type_str = colQuery.value(3).toString();

            // 解析data_properties
            QString dataPropertiesStr = colQuery.value(4).toString();
            QJsonDocument jsonDoc = QJsonDocument::fromJson(dataPropertiesStr.toUtf8());
            if (!jsonDoc.isNull() && jsonDoc.isObject())
            {
                colInfo.data_properties = jsonDoc.object();
            }
            else
            {
                // 如果解析失败，创建一个空的QJsonObject
                colInfo.data_properties = QJsonObject();
            }

            colInfo.is_visible = colQuery.value(5).toBool();

            // 映射列类型字符串到枚举
            if (colInfo.original_type_str == "TEXT")
                colInfo.type = Vector::ColumnDataType::TEXT;
            else if (colInfo.original_type_str == "INTEGER")
                colInfo.type = Vector::ColumnDataType::INTEGER;
            else if (colInfo.original_type_str == "BOOLEAN")
                colInfo.type = Vector::ColumnDataType::BOOLEAN;
            else if (colInfo.original_type_str == "INSTRUCTION_ID")
                colInfo.type = Vector::ColumnDataType::INSTRUCTION_ID;
            else if (colInfo.original_type_str == "TIMESET_ID")
            {
                colInfo.type = Vector::ColumnDataType::TIMESET_ID;
                timeSetColumnIndex = columns.size(); // 记录TimeSet列的索引
            }
            else if (colInfo.original_type_str == "PIN_STATE_ID")
                colInfo.type = Vector::ColumnDataType::PIN_STATE_ID;
            else
                colInfo.type = Vector::ColumnDataType::TEXT; // 默认类型

            columns.append(colInfo);
        }

        if (timeSetColumnIndex == -1)
        {
            qWarning() << "填充TimeSet - 未找到TimeSet列，尝试修复表结构";

            // 尝试添加缺失的TimeSet列
            QSqlQuery addTimeSetColQuery(db);
            addTimeSetColQuery.prepare("INSERT INTO VectorTableColumnConfiguration "
                                       "(master_record_id, column_name, column_order, column_type, data_properties, IsVisible) "
                                       "VALUES (?, ?, ?, ?, ?, 1)");

            // 获取当前最大列序号
            int maxOrder = -1;
            QSqlQuery maxOrderQuery(db);
            maxOrderQuery.prepare("SELECT MAX(column_order) FROM VectorTableColumnConfiguration WHERE master_record_id = ?");
            maxOrderQuery.addBindValue(tableId);
            if (maxOrderQuery.exec() && maxOrderQuery.next())
            {
                maxOrder = maxOrderQuery.value(0).toInt();
            }

            // 如果无法获取最大列序号，默认放在第2位置
            if (maxOrder < 0)
            {
                maxOrder = 1; // 添加在位置2 (索引为2，实际是第3列)
            }

            // 添加TimeSet列
            addTimeSetColQuery.addBindValue(tableId);
            addTimeSetColQuery.addBindValue("TimeSet");
            addTimeSetColQuery.addBindValue(2); // 固定在位置2 (通常TimeSet是第3列)
            addTimeSetColQuery.addBindValue("TIMESET_ID");
            addTimeSetColQuery.addBindValue("{}");

            if (!addTimeSetColQuery.exec())
            {
                qWarning() << "填充TimeSet - 添加TimeSet列失败:" << addTimeSetColQuery.lastError().text();
                throw std::runtime_error(("未找到TimeSet类型的列，且尝试添加时失败。无法执行填充操作。"));
            }

            qDebug() << "填充TimeSet - 成功添加TimeSet列，重新获取列定义";

            // 重新查询列定义
            colQuery.clear();
            colQuery.prepare("SELECT id, column_name, column_order, column_type, data_properties, IsVisible "
                             "FROM VectorTableColumnConfiguration "
                             "WHERE master_record_id = ? ORDER BY column_order");
            colQuery.addBindValue(tableId);

            if (!colQuery.exec())
            {
                qWarning() << "填充TimeSet - 重新查询列定义失败:" << colQuery.lastError().text();
                throw std::runtime_error(("查询列定义失败: " + colQuery.lastError().text()).toStdString());
            }

            // 重新解析列定义
            columns.clear();
            timeSetColumnIndex = -1;

            while (colQuery.next())
            {
                Vector::ColumnInfo colInfo;
                colInfo.id = colQuery.value(0).toInt();
                colInfo.name = colQuery.value(1).toString();
                colInfo.order = colQuery.value(2).toInt();
                colInfo.original_type_str = colQuery.value(3).toString();

                // 解析data_properties
                QString dataPropertiesStr = colQuery.value(4).toString();
                QJsonDocument jsonDoc = QJsonDocument::fromJson(dataPropertiesStr.toUtf8());
                if (!jsonDoc.isNull() && jsonDoc.isObject())
                {
                    colInfo.data_properties = jsonDoc.object();
                }
                else
                {
                    colInfo.data_properties = QJsonObject();
                }

                colInfo.is_visible = colQuery.value(5).toBool();

                // 映射列类型字符串到枚举
                if (colInfo.original_type_str == "TEXT")
                    colInfo.type = Vector::ColumnDataType::TEXT;
                else if (colInfo.original_type_str == "INTEGER")
                    colInfo.type = Vector::ColumnDataType::INTEGER;
                else if (colInfo.original_type_str == "BOOLEAN")
                    colInfo.type = Vector::ColumnDataType::BOOLEAN;
                else if (colInfo.original_type_str == "INSTRUCTION_ID")
                    colInfo.type = Vector::ColumnDataType::INSTRUCTION_ID;
                else if (colInfo.original_type_str == "TIMESET_ID")
                {
                    colInfo.type = Vector::ColumnDataType::TIMESET_ID;
                    timeSetColumnIndex = columns.size(); // 记录TimeSet列的索引
                }
                else if (colInfo.original_type_str == "PIN_STATE_ID")
                    colInfo.type = Vector::ColumnDataType::PIN_STATE_ID;
                else
                    colInfo.type = Vector::ColumnDataType::TEXT; // 默认类型

                columns.append(colInfo);
            }

            // 再次检查是否找到TimeSet列
            if (timeSetColumnIndex == -1)
            {
                qWarning() << "填充TimeSet - 修复后仍未找到TimeSet列，放弃操作";
                throw std::runtime_error(("修复后仍未找到TimeSet类型的列，无法执行填充操作"));
            }

            qDebug() << "填充TimeSet - 成功修复表结构并找到TimeSet列，索引:" << timeSetColumnIndex;
        }

        // 5. 读取二进制文件数据
        QList<Vector::RowData> allRows;
        int schemaVersion = 1; // 默认值

        if (!Persistence::BinaryFileHelper::readAllRowsFromBinary(absoluteBinFilePath, columns, schemaVersion, allRows))
        {
            qWarning() << "填充TimeSet - 读取二进制文件失败:" << absoluteBinFilePath;
            QString errorMsg = QString("从二进制文件读取数据失败");
            throw std::runtime_error(errorMsg.toStdString());
        }

        qDebug() << "填充TimeSet - 从二进制文件读取到" << allRows.size() << "行数据";

        // 如果行数为0，提示用户
        if (allRows.size() == 0)
        {
            qWarning() << "填充TimeSet - 二进制文件中没有数据，无需填充";
            QMessageBox::warning(this, tr("警告"), tr("向量表中没有数据，无需填充"));
            db.rollback();
            return;
        }

        // 6. 更新内存中的TimeSet数据
        int updatedRowCount = 0;
        for (int i = 0; i < allRows.size(); i++)
        {
            Vector::RowData &rowData = allRows[i];

            // 检查行数据大小是否合法
            if (rowData.size() <= timeSetColumnIndex)
            {
                qWarning() << "填充TimeSet - 行" << i << "数据列数" << rowData.size()
                           << "小于TimeSet列索引" << timeSetColumnIndex << "，跳过此行";
                continue;
            }

            // 检查当前行是否是要更新的行
            bool shouldProcessThisRow = selectedUiRows.isEmpty(); // 如果没有选择行，处理所有行

            // 如果用户选择了特定行，检查当前行是否在选择范围内
            if (!shouldProcessThisRow && i < allRows.size())
            {
                // 注意：selectedUiRows中存储的是UI中的行索引，这与二进制文件中的行索引i可能不完全一致
                shouldProcessThisRow = selectedUiRows.contains(i);

                if (shouldProcessThisRow)
                {
                    qDebug() << "填充TimeSet - 行 " << i << " 在用户选择的行列表中";
                }
            }

            if (shouldProcessThisRow)
            {
                // 直接更新为新的TimeSet ID
                int oldTimeSetId = rowData[timeSetColumnIndex].toInt();
                rowData[timeSetColumnIndex] = timeSetId;
                updatedRowCount++;
                qDebug() << "填充TimeSet - 已更新行 " << i << " 的TimeSet ID 从 " << oldTimeSetId << " 到 " << timeSetId;
            }
        }

        qDebug() << "填充TimeSet - 内存中更新了" << updatedRowCount << "行数据";

        // 7. 写回二进制文件
        if (!Persistence::BinaryFileHelper::writeAllRowsToBinary(absoluteBinFilePath, columns, schemaVersion, allRows))
        {
            qWarning() << "填充TimeSet - 写入二进制文件失败:" << absoluteBinFilePath;
            QString errorMsg = QString("将更新后的数据写回二进制文件失败");
            throw std::runtime_error(errorMsg.toStdString());
        }

        qDebug() << "填充TimeSet - 已成功写入二进制文件:" << absoluteBinFilePath;

        // 更新数据库中的记录
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
        QMessageBox::information(this, tr("成功"), tr("已将 %1 填充到选中区域，共更新了 %2 行数据").arg(timeSetName).arg(updatedRowCount));
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
    // 获取当前选择的向量表
    int currentIndex = m_vectorTableSelector->currentIndex();
    if (currentIndex < 0)
    {
        QMessageBox::warning(this, "操作失败", "没有选中的向量表");
        return;
    }

    // 获取表ID
    int tableId = m_vectorTableSelector->currentData().toInt();

    // 获取向量表行数
    int rowCount = VectorDataHandler::instance().getVectorTableRowCount(tableId);
    if (rowCount <= 0)
    {
        QMessageBox::warning(this, "操作失败", "当前向量表没有数据行");
        return;
    }

    // 显示替换TimeSet对话框
    ReplaceTimeSetDialog dialog(this);
    // 设置向量表总行数
    dialog.setVectorRowCount(rowCount);

    // 获取选中的行（如果有）并设置默认范围
    QModelIndexList selectedIndexes = m_vectorTableWidget->selectionModel()->selectedRows();
    if (!selectedIndexes.isEmpty())
    {
        // 找出最小和最大行号（1-based）
        int minRow = INT_MAX;
        int maxRow = 0;
        foreach (const QModelIndex &index, selectedIndexes)
        {
            int rowIdx = index.row() + 1; // 转为1-based
            minRow = qMin(minRow, rowIdx);
            maxRow = qMax(maxRow, rowIdx);
        }
        if (minRow <= maxRow)
        {
            dialog.setSelectedRange(minRow, maxRow);
        }
    }

    if (dialog.exec() == QDialog::Accepted)
    {
        int fromTimeSetId = dialog.getFromTimeSetId();
        int toTimeSetId = dialog.getToTimeSetId();

        // 获取选中的行
        QList<int> selectedRows;
        foreach (const QModelIndex &index, selectedIndexes)
        {
            selectedRows.append(index.row());
        }

        // 替换TimeSet
        replaceTimeSetForVectorTable(fromTimeSetId, toTimeSetId, selectedRows);
    }
}

void MainWindow::replaceTimeSetForVectorTable(int fromTimeSetId, int toTimeSetId, const QList<int> &selectedUiRows)
{
    qDebug() << "替换TimeSet - 开始替换过程";

    // 输出选择的行信息
    if (selectedUiRows.isEmpty())
    {
        qDebug() << "替换TimeSet - 用户未选择特定行，将对所有行进行操作";
    }
    else
    {
        QStringList rowsList;
        for (int row : selectedUiRows)
        {
            rowsList << QString::number(row);
        }
        qDebug() << "替换TimeSet - 用户选择了以下行：" << rowsList.join(", ");
    }

    int currentIndex = m_vectorTableSelector->currentIndex();
    if (currentIndex < 0)
    {
        QMessageBox::warning(this, tr("警告"), tr("请先选择一个向量表"));
        qDebug() << "替换TimeSet - 未选择向量表，操作取消";
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
        // 1. 查询表对应的二进制文件路径
        QSqlQuery fileQuery(db);
        fileQuery.prepare("SELECT binary_data_filename FROM VectorTableMasterRecord WHERE id = ?");
        fileQuery.addBindValue(tableId);
        if (!fileQuery.exec() || !fileQuery.next())
        {
            QString errorText = fileQuery.lastError().text();
            qDebug() << "替换TimeSet - 查询二进制文件名失败:" << errorText;
            throw std::runtime_error(("查询二进制文件名失败: " + errorText).toStdString());
        }

        QString binFileName = fileQuery.value(0).toString();
        if (binFileName.isEmpty())
        {
            qDebug() << "替换TimeSet - 二进制文件名为空，无法进行替换操作";
            throw std::runtime_error("向量表未配置二进制文件存储，无法进行替换操作");
        }

        // 2. 解析二进制文件路径
        // 使用PathUtils获取项目二进制数据目录
        QString projectBinaryDataDir = Utils::PathUtils::getProjectBinaryDataDirectory(m_currentDbPath);
        if (projectBinaryDataDir.isEmpty())
        {
            QString errorMsg = QString("无法为数据库 '%1' 生成二进制数据目录路径").arg(m_currentDbPath);
            qWarning() << "替换TimeSet - " << errorMsg;
            throw std::runtime_error(errorMsg.toStdString());
        }

        qDebug() << "替换TimeSet - 项目二进制数据目录:" << projectBinaryDataDir;

        // 相对路径转绝对路径
        QString absoluteBinFilePath;
        if (QFileInfo(binFileName).isRelative())
        {
            absoluteBinFilePath = QDir(projectBinaryDataDir).absoluteFilePath(binFileName);
            qDebug() << "替换TimeSet - 相对路径转换为绝对路径:" << binFileName << " -> " << absoluteBinFilePath;
        }
        else
        {
            absoluteBinFilePath = binFileName;
        }

        // 3. 检查二进制文件是否存在
        if (!QFile::exists(absoluteBinFilePath))
        {
            qWarning() << "替换TimeSet - 二进制文件不存在:" << absoluteBinFilePath;
            throw std::runtime_error(("二进制文件不存在: " + absoluteBinFilePath).toStdString());
        }

        // 4. 查询列定义，找出TimeSet列
        QSqlQuery colQuery(db);
        colQuery.prepare("SELECT id, column_name, column_order, column_type, data_properties, IsVisible "
                         "FROM VectorTableColumnConfiguration "
                         "WHERE master_record_id = ? ORDER BY column_order");
        colQuery.addBindValue(tableId);

        if (!colQuery.exec())
        {
            QString errorText = colQuery.lastError().text();
            qWarning() << "替换TimeSet - 查询列定义失败:" << errorText;
            throw std::runtime_error(("查询列定义失败: " + errorText).toStdString());
        }

        QList<Vector::ColumnInfo> columns;
        int timeSetColumnIndex = -1; // 用于标记TimeSet列的索引

        while (colQuery.next())
        {
            Vector::ColumnInfo colInfo;
            colInfo.id = colQuery.value(0).toInt();
            colInfo.name = colQuery.value(1).toString();
            colInfo.order = colQuery.value(2).toInt();
            colInfo.original_type_str = colQuery.value(3).toString();

            // 解析data_properties
            QString dataPropertiesStr = colQuery.value(4).toString();
            QJsonDocument jsonDoc = QJsonDocument::fromJson(dataPropertiesStr.toUtf8());
            if (!jsonDoc.isNull() && jsonDoc.isObject())
            {
                colInfo.data_properties = jsonDoc.object();
            }
            else
            {
                // 如果解析失败，创建一个空的QJsonObject
                colInfo.data_properties = QJsonObject();
            }

            colInfo.is_visible = colQuery.value(5).toBool();

            // 映射列类型字符串到枚举
            if (colInfo.original_type_str == "TEXT")
                colInfo.type = Vector::ColumnDataType::TEXT;
            else if (colInfo.original_type_str == "INTEGER")
                colInfo.type = Vector::ColumnDataType::INTEGER;
            else if (colInfo.original_type_str == "BOOLEAN")
                colInfo.type = Vector::ColumnDataType::BOOLEAN;
            else if (colInfo.original_type_str == "INSTRUCTION_ID")
                colInfo.type = Vector::ColumnDataType::INSTRUCTION_ID;
            else if (colInfo.original_type_str == "TIMESET_ID")
            {
                colInfo.type = Vector::ColumnDataType::TIMESET_ID;
                timeSetColumnIndex = columns.size(); // 记录TimeSet列的索引
            }
            else if (colInfo.original_type_str == "PIN_STATE_ID")
                colInfo.type = Vector::ColumnDataType::PIN_STATE_ID;
            else
                colInfo.type = Vector::ColumnDataType::TEXT; // 默认类型

            columns.append(colInfo);
        }

        if (timeSetColumnIndex == -1)
        {
            qWarning() << "替换TimeSet - 未找到TimeSet列，尝试修复表结构";

            // 尝试添加缺失的TimeSet列
            QSqlQuery addTimeSetColQuery(db);
            addTimeSetColQuery.prepare("INSERT INTO VectorTableColumnConfiguration "
                                       "(master_record_id, column_name, column_order, column_type, data_properties, IsVisible) "
                                       "VALUES (?, ?, ?, ?, ?, 1)");

            // 获取当前最大列序号
            int maxOrder = -1;
            QSqlQuery maxOrderQuery(db);
            maxOrderQuery.prepare("SELECT MAX(column_order) FROM VectorTableColumnConfiguration WHERE master_record_id = ?");
            maxOrderQuery.addBindValue(tableId);
            if (maxOrderQuery.exec() && maxOrderQuery.next())
            {
                maxOrder = maxOrderQuery.value(0).toInt();
            }

            // 如果无法获取最大列序号，默认放在第2位置
            if (maxOrder < 0)
            {
                maxOrder = 1; // 添加在位置2 (索引为2，实际是第3列)
            }

            // 添加TimeSet列
            addTimeSetColQuery.addBindValue(tableId);
            addTimeSetColQuery.addBindValue("TimeSet");
            addTimeSetColQuery.addBindValue(2); // 固定在位置2 (通常TimeSet是第3列)
            addTimeSetColQuery.addBindValue("TIMESET_ID");
            addTimeSetColQuery.addBindValue("{}");

            if (!addTimeSetColQuery.exec())
            {
                qWarning() << "替换TimeSet - 添加TimeSet列失败:" << addTimeSetColQuery.lastError().text();
                throw std::runtime_error(("未找到TimeSet类型的列，且尝试添加时失败。无法执行替换操作。"));
            }

            qDebug() << "替换TimeSet - 成功添加TimeSet列，重新获取列定义";

            // 重新查询列定义
            colQuery.clear();
            colQuery.prepare("SELECT id, column_name, column_order, column_type, data_properties, IsVisible "
                             "FROM VectorTableColumnConfiguration "
                             "WHERE master_record_id = ? ORDER BY column_order");
            colQuery.addBindValue(tableId);

            if (!colQuery.exec())
            {
                qWarning() << "替换TimeSet - 重新查询列定义失败:" << colQuery.lastError().text();
                throw std::runtime_error(("查询列定义失败: " + colQuery.lastError().text()).toStdString());
            }

            // 重新解析列定义
            columns.clear();
            timeSetColumnIndex = -1;

            while (colQuery.next())
            {
                Vector::ColumnInfo colInfo;
                colInfo.id = colQuery.value(0).toInt();
                colInfo.name = colQuery.value(1).toString();
                colInfo.order = colQuery.value(2).toInt();
                colInfo.original_type_str = colQuery.value(3).toString();

                // 解析data_properties
                QString dataPropertiesStr = colQuery.value(4).toString();
                QJsonDocument jsonDoc = QJsonDocument::fromJson(dataPropertiesStr.toUtf8());
                if (!jsonDoc.isNull() && jsonDoc.isObject())
                {
                    colInfo.data_properties = jsonDoc.object();
                }
                else
                {
                    colInfo.data_properties = QJsonObject();
                }

                colInfo.is_visible = colQuery.value(5).toBool();

                // 映射列类型字符串到枚举
                if (colInfo.original_type_str == "TEXT")
                    colInfo.type = Vector::ColumnDataType::TEXT;
                else if (colInfo.original_type_str == "INTEGER")
                    colInfo.type = Vector::ColumnDataType::INTEGER;
                else if (colInfo.original_type_str == "BOOLEAN")
                    colInfo.type = Vector::ColumnDataType::BOOLEAN;
                else if (colInfo.original_type_str == "INSTRUCTION_ID")
                    colInfo.type = Vector::ColumnDataType::INSTRUCTION_ID;
                else if (colInfo.original_type_str == "TIMESET_ID")
                {
                    colInfo.type = Vector::ColumnDataType::TIMESET_ID;
                    timeSetColumnIndex = columns.size(); // 记录TimeSet列的索引
                }
                else if (colInfo.original_type_str == "PIN_STATE_ID")
                    colInfo.type = Vector::ColumnDataType::PIN_STATE_ID;
                else
                    colInfo.type = Vector::ColumnDataType::TEXT; // 默认类型

                columns.append(colInfo);
            }

            // 再次检查是否找到TimeSet列
            if (timeSetColumnIndex == -1)
            {
                qWarning() << "替换TimeSet - 修复后仍未找到TimeSet列，放弃操作";
                throw std::runtime_error(("修复后仍未找到TimeSet类型的列，无法执行替换操作"));
            }

            qDebug() << "替换TimeSet - 成功修复表结构并找到TimeSet列，索引:" << timeSetColumnIndex;
        }

        // 5. 读取二进制文件数据
        QList<Vector::RowData> allRows;
        int schemaVersion = 1; // 默认值

        if (!Persistence::BinaryFileHelper::readAllRowsFromBinary(absoluteBinFilePath, columns, schemaVersion, allRows))
        {
            qWarning() << "替换TimeSet - 读取二进制文件失败:" << absoluteBinFilePath;
            QString errorMsg = QString("从二进制文件读取数据失败");
            throw std::runtime_error(errorMsg.toStdString());
        }

        qDebug() << "替换TimeSet - 从二进制文件读取到" << allRows.size() << "行数据";

        // 如果行数为0，提示用户
        if (allRows.size() == 0)
        {
            qWarning() << "替换TimeSet - 二进制文件中没有数据，无需替换";
            QMessageBox::warning(this, tr("警告"), tr("向量表中没有数据，无需替换"));
            db.rollback();
            return;
        }

        // 输出所有行的TimeSet ID和预期替换值
        qDebug() << "替换TimeSet - 预期将TimeSet ID " << fromTimeSetId << " 替换为 " << toTimeSetId;
        qDebug() << "替换TimeSet - TimeSet列索引: " << timeSetColumnIndex;
        for (int i = 0; i < allRows.size(); i++)
        {
            if (allRows[i].size() > timeSetColumnIndex)
            {
                int rowTimeSetId = allRows[i][timeSetColumnIndex].toInt();
                qDebug() << "替换TimeSet - 行 " << i << " 的TimeSet ID: " << rowTimeSetId
                         << (rowTimeSetId == fromTimeSetId ? " (匹配)" : " (不匹配)");
            }
        }

        // 创建名称到ID的映射
        QMap<QString, int> timeSetNameToIdMap;
        QSqlQuery allTimeSetQuery(db);
        if (allTimeSetQuery.exec("SELECT id, timeset_name FROM timeset_list"))
        {
            while (allTimeSetQuery.next())
            {
                int id = allTimeSetQuery.value(0).toInt();
                QString name = allTimeSetQuery.value(1).toString();
                timeSetNameToIdMap[name] = id;
                qDebug() << "替换TimeSet - TimeSet名称映射: " << name << " -> ID: " << id;
            }
        }

        // 6. 更新内存中的TimeSet数据
        int updatedRowCount = 0;
        for (int i = 0; i < allRows.size(); i++)
        {
            Vector::RowData &rowData = allRows[i];

            // 检查行数据大小是否合法
            if (rowData.size() <= timeSetColumnIndex)
            {
                qWarning() << "替换TimeSet - 行" << i << "数据列数" << rowData.size()
                           << "小于TimeSet列索引" << timeSetColumnIndex << "，跳过此行";
                continue;
            }

            // 检查当前行是否是要更新的行
            bool shouldProcessThisRow = selectedUiRows.isEmpty(); // 如果没有选择行，处理所有行

            // 如果用户选择了特定行，检查当前行是否在选择范围内
            if (!shouldProcessThisRow && i < allRows.size())
            {
                // 注意：selectedUiRows中存储的是UI中的行索引，这与二进制文件中的行索引i可能不完全一致
                // 如果用户选择了某行，我们将处理它，无论它在二进制文件中的顺序如何
                shouldProcessThisRow = selectedUiRows.contains(i);

                if (shouldProcessThisRow)
                {
                    qDebug() << "替换TimeSet - 行 " << i << " 在用户选择的行列表中";
                }
            }

            if (shouldProcessThisRow)
            {
                int currentTimeSetId = rowData[timeSetColumnIndex].toInt();
                qDebug() << "替换TimeSet - 处理行 " << i << "，当前TimeSet ID: " << currentTimeSetId
                         << "，fromTimeSetId: " << fromTimeSetId;

                // 尝试直接通过ID匹配
                if (currentTimeSetId == fromTimeSetId)
                {
                    // 更新TimeSet ID
                    rowData[timeSetColumnIndex] = toTimeSetId;
                    updatedRowCount++;
                    qDebug() << "替换TimeSet - 已更新行 " << i << " 的TimeSet ID 从 " << fromTimeSetId << " 到 " << toTimeSetId;
                }
                // 尝试通过名称匹配（获取当前ID对应的名称，检查是否与fromTimeSetName匹配）
                else
                {
                    // 获取当前ID对应的名称
                    QString currentTimeSetName = "未知";
                    QSqlQuery currentTSNameQuery(db);
                    currentTSNameQuery.prepare("SELECT timeset_name FROM timeset_list WHERE id = ?");
                    currentTSNameQuery.addBindValue(currentTimeSetId);
                    if (currentTSNameQuery.exec() && currentTSNameQuery.next())
                    {
                        currentTimeSetName = currentTSNameQuery.value(0).toString();
                    }

                    // 如果当前TimeSet名称与fromTimeSetName匹配，则更新
                    if (currentTimeSetName == fromTimeSetName)
                    {
                        // 更新TimeSet ID
                        rowData[timeSetColumnIndex] = toTimeSetId;
                        updatedRowCount++;
                        qDebug() << "替换TimeSet - 已通过名称匹配更新行 " << i
                                 << " 的TimeSet从 " << currentTimeSetName << " (ID:" << currentTimeSetId
                                 << ") 到 " << toTimeSetName << " (ID:" << toTimeSetId << ")";
                    }
                }
            }
        }

        qDebug() << "替换TimeSet - 内存中更新了" << updatedRowCount << "行数据";

        if (updatedRowCount == 0)
        {
            QString selectMessage;
            if (selectedUiRows.isEmpty())
            {
                selectMessage = tr("所有行");
            }
            else
            {
                QStringList rowStrings;
                for (int row : selectedUiRows)
                {
                    rowStrings << QString::number(row + 1); // 转换为1-based显示给用户
                }
                selectMessage = tr("选中的行 (%1)").arg(rowStrings.join(", "));
            }

            qDebug() << "替换TimeSet - 在" << selectMessage << "中没有找到使用ID " << fromTimeSetId
                     << " 或名称 " << fromTimeSetName << " 的TimeSet行";

            // 检查二进制文件中的行使用的TimeSet IDs
            QSet<int> usedTimeSetIds;
            for (int i = 0; i < allRows.size(); i++)
            {
                // 只统计选中行或全部行（如果没有选择）
                if (selectedUiRows.isEmpty() || selectedUiRows.contains(i))
                {
                    if (allRows[i].size() > timeSetColumnIndex)
                    {
                        int usedId = allRows[i][timeSetColumnIndex].toInt();
                        usedTimeSetIds.insert(usedId);
                    }
                }
            }

            qDebug() << "替换TimeSet - 在" << selectMessage << "中使用的TimeSet IDs:" << usedTimeSetIds.values();

            // 获取每个使用的ID对应的名称
            QStringList usedTimeSetNames;
            for (int usedId : usedTimeSetIds)
            {
                QSqlQuery nameQuery(db);
                nameQuery.prepare("SELECT timeset_name FROM timeset_list WHERE id = ?");
                nameQuery.addBindValue(usedId);
                QString tsName = "未知";
                if (nameQuery.exec() && nameQuery.next())
                {
                    tsName = nameQuery.value(0).toString();
                    usedTimeSetNames.append(tsName);
                }

                qDebug() << "替换TimeSet - 在" << selectMessage << "中使用的TimeSet ID: " << usedId << ", 名称: " << tsName;
            }

            // 提示用户没有找到匹配的TimeSet
            QString usedInfo;
            if (!usedTimeSetNames.isEmpty())
            {
                usedInfo = tr("表中使用的TimeSet有: %1").arg(usedTimeSetNames.join(", "));
            }
            else
            {
                usedInfo = tr("表中没有使用任何TimeSet");
            }

            QMessageBox::information(this, tr("提示"),
                                     tr("在%1中没有找到使用 %2 (ID: %3) 的数据行需要替换。\n%4")
                                         .arg(selectMessage)
                                         .arg(fromTimeSetName)
                                         .arg(fromTimeSetId)
                                         .arg(usedInfo));
            db.rollback();
            return;
        }

        // 7. 写回二进制文件
        if (!Persistence::BinaryFileHelper::writeAllRowsToBinary(absoluteBinFilePath, columns, schemaVersion, allRows))
        {
            qWarning() << "替换TimeSet - 写入二进制文件失败:" << absoluteBinFilePath;
            QString errorMsg = QString("将更新后的数据写回二进制文件失败");
            throw std::runtime_error(errorMsg.toStdString());
        }

        qDebug() << "替换TimeSet - 已成功写入二进制文件:" << absoluteBinFilePath;

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
        QMessageBox::information(this, tr("成功"), tr("已将 %1 替换为 %2，共更新了 %3 行数据").arg(fromTimeSetName).arg(toTimeSetName).arg(updatedRowCount));
        qDebug() << "替换TimeSet - 操作成功完成，共更新" << updatedRowCount << "行数据";
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

                // 查找使用此管脚的向量表
                QSqlQuery findTablesQuery(db);
                findTablesQuery.prepare("SELECT DISTINCT vtm.id, vtm.table_name FROM VectorTableMasterRecord vtm "
                                        "JOIN VectorTableColumnConfiguration vtcc ON vtm.id = vtcc.master_record_id "
                                        "WHERE vtcc.column_name = ? AND vtcc.column_type = 'PIN_STATE_ID'");
                findTablesQuery.addBindValue(pinName);

                if (!findTablesQuery.exec())
                {
                    success = false;
                    errorMsg = findTablesQuery.lastError().text();
                    qDebug() << "MainWindow::deletePins - 查找使用管脚的向量表失败:" << errorMsg;
                    break;
                }

                // 对每个使用此管脚的向量表，将对应列标记为不可见
                while (findTablesQuery.next() && success)
                {
                    int tableId = findTablesQuery.value(0).toInt();
                    QString tableName = findTablesQuery.value(1).toString();

                    qDebug() << "MainWindow::deletePins - 在表 " << tableName << " (ID:" << tableId << ") 中将管脚 " << pinName << " 标记为不可见";

                    // 使用VectorDataHandler::hideVectorTableColumn方法逻辑删除列
                    QString hideError;
                    if (!VectorDataHandler::instance().hideVectorTableColumn(tableId, pinName, hideError))
                    {
                        success = false;
                        errorMsg = hideError;
                        qDebug() << "MainWindow::deletePins - 逻辑删除管脚列失败:" << errorMsg;
                        break;
                    }
                }

                if (!success)
                {
                    break;
                }

                qDebug() << "MainWindow::deletePins - 成功逻辑删除管脚列:" << pinName;

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
    const QString funcName = "MainWindow::refreshVectorTableData";
    qDebug() << funcName << " - 开始刷新向量表数据";

    int currentIndex = m_vectorTableSelector->currentIndex();
    if (currentIndex < 0)
    {
        qDebug() << funcName << " - 没有选中的向量表，不执行刷新";
        statusBar()->showMessage("没有选中的向量表");
        return;
    }
    int tableId = m_vectorTableSelector->currentData().toInt();
    QString tableName = m_vectorTableSelector->currentText();
    qDebug() << funcName << " - 正在刷新表ID:" << tableId << ", 名称:" << tableName;

    // 尝试修复当前表（如果需要）
    fixExistingTableWithoutColumns(tableId);

    // Ensure m_vectorTableWidget is the correct one associated with the current tab/selection
    QWidget *currentTabWidget = m_vectorTabWidget->currentWidget();
    QTableWidget *targetTableWidget = nullptr;

    if (currentTabWidget)
    {
        targetTableWidget = currentTabWidget->findChild<QTableWidget *>();
    }

    if (!targetTableWidget)
    {
        targetTableWidget = m_vectorTableWidget; // Fallback
        qDebug() << funcName << " - 未找到当前Tab页中的TableWidget, 回退到 m_vectorTableWidget for refresh";
    }

    if (!targetTableWidget)
    {
        qCritical() << funcName << " - 无法确定用于刷新的表格控件。";
        // Optionally show a message to the user
        return;
    }
    qDebug() << funcName << " - 用于刷新的表格控件已确定。";

    if (VectorDataHandler::instance().loadVectorTableData(tableId, targetTableWidget))
    {
        statusBar()->showMessage(QString("已刷新向量表: %1").arg(tableName));

        // 应用表格样式（优化版本，一次性完成所有样式设置，包括列宽和对齐）
        TableStyleManager::applyBatchTableStyle(m_vectorTableWidget);
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
    const QString funcName = "MainWindow::openTimeSetSettingsDialog"; // 添加 funcName 定义
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
            // int tableId = m_vectorTableSelector->itemData(currentIndex).toInt(); // 获取 tableId 以便在日志中使用（如果需要）
            qDebug() << funcName << " - TimeSet设置已更新，将刷新当前选择的向量表，下拉框索引为: " << currentIndex; // 修正日志，移除未定义的 tableId
            m_vectorTableSelector->setCurrentIndex(currentIndex);
            // onVectorTableSelectionChanged(currentIndex); // setCurrentIndex会触发信号
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

/**
 * @brief 为新创建的向量表添加默认的列配置
 *
 * @param tableId 向量表ID
 * @return bool 成功返回true，失败返回false
 */
bool MainWindow::addDefaultColumnConfigurations(int tableId)
{
    const QString funcName = "MainWindow::addDefaultColumnConfigurations";
    qDebug() << funcName << " - 开始为表ID " << tableId << " 添加默认列配置";

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qCritical() << funcName << " - 数据库未打开";
        return false;
    }

    db.transaction();

    try
    {
        QSqlQuery query(db);

        // 添加标准列配置
        // 1. 添加Label列
        query.prepare("INSERT INTO VectorTableColumnConfiguration "
                      "(master_record_id, column_name, column_order, column_type, data_properties) "
                      "VALUES (?, ?, ?, ?, ?)");
        query.addBindValue(tableId);
        query.addBindValue("Label");
        query.addBindValue(0);
        query.addBindValue("TEXT");
        query.addBindValue("{}");

        if (!query.exec())
        {
            throw QString("无法添加Label列: " + query.lastError().text());
        }

        // 2. 添加Instruction列
        query.prepare("INSERT INTO VectorTableColumnConfiguration "
                      "(master_record_id, column_name, column_order, column_type, data_properties) "
                      "VALUES (?, ?, ?, ?, ?)");
        query.addBindValue(tableId);
        query.addBindValue("Instruction");
        query.addBindValue(1);
        query.addBindValue("INSTRUCTION_ID");
        query.addBindValue("{}");

        if (!query.exec())
        {
            throw QString("无法添加Instruction列: " + query.lastError().text());
        }

        // 3. 添加TimeSet列
        query.prepare("INSERT INTO VectorTableColumnConfiguration "
                      "(master_record_id, column_name, column_order, column_type, data_properties) "
                      "VALUES (?, ?, ?, ?, ?)");
        query.addBindValue(tableId);
        query.addBindValue("TimeSet");
        query.addBindValue(2);
        query.addBindValue("TIMESET_ID");
        query.addBindValue("{}");

        if (!query.exec())
        {
            throw QString("无法添加TimeSet列: " + query.lastError().text());
        }

        // 4. 添加Capture列
        query.prepare("INSERT INTO VectorTableColumnConfiguration "
                      "(master_record_id, column_name, column_order, column_type, data_properties) "
                      "VALUES (?, ?, ?, ?, ?)");
        query.addBindValue(tableId);
        query.addBindValue("Capture");
        query.addBindValue(3);
        query.addBindValue("BOOLEAN");
        query.addBindValue("{}");

        if (!query.exec())
        {
            throw QString("无法添加Capture列: " + query.lastError().text());
        }

        // 5. 添加EXT列
        query.prepare("INSERT INTO VectorTableColumnConfiguration "
                      "(master_record_id, column_name, column_order, column_type, data_properties) "
                      "VALUES (?, ?, ?, ?, ?)");
        query.addBindValue(tableId);
        query.addBindValue("EXT");
        query.addBindValue(4);
        query.addBindValue("TEXT");
        query.addBindValue("{}");

        if (!query.exec())
        {
            throw QString("无法添加EXT列: " + query.lastError().text());
        }

        // 6. 添加Comment列
        query.prepare("INSERT INTO VectorTableColumnConfiguration "
                      "(master_record_id, column_name, column_order, column_type, data_properties) "
                      "VALUES (?, ?, ?, ?, ?)");
        query.addBindValue(tableId);
        query.addBindValue("Comment");
        query.addBindValue(5);
        query.addBindValue("TEXT");
        query.addBindValue("{}");

        if (!query.exec())
        {
            throw QString("无法添加Comment列: " + query.lastError().text());
        }

        // 更新主记录的列数
        query.prepare("UPDATE VectorTableMasterRecord SET column_count = ? WHERE id = ?");
        query.addBindValue(6); // 六个标准列
        query.addBindValue(tableId);

        if (!query.exec())
        {
            throw QString("无法更新主记录的列数: " + query.lastError().text());
        }

        // 提交事务
        if (!db.commit())
        {
            throw QString("无法提交事务: " + db.lastError().text());
        }

        qDebug() << funcName << " - 成功添加默认列配置";
        return true;
    }
    catch (const QString &error)
    {
        qCritical() << funcName << " - 错误: " << error;
        db.rollback();
        return false;
    }
}

/**
 * @brief 修复没有列配置的现有向量表
 *
 * 这是一个一次性工具方法，用于修复数据库中已经存在但缺少列配置的向量表
 *
 * @param tableId 要修复的向量表ID
 * @return bool 成功返回true，失败返回false
 */
bool MainWindow::fixExistingTableWithoutColumns(int tableId)
{
    const QString funcName = "MainWindow::fixExistingTableWithoutColumns";
    qDebug() << funcName << " - 开始修复表ID " << tableId << " 的列配置";

    // 首先检查这个表是否已经有列配置
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qCritical() << funcName << " - 数据库未打开";
        return false;
    }

    // 检查是否有列配置
    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT COUNT(*) FROM VectorTableColumnConfiguration WHERE master_record_id = ?");
    checkQuery.addBindValue(tableId);

    if (!checkQuery.exec() || !checkQuery.next())
    {
        qCritical() << funcName << " - 无法检查表ID " << tableId << " 的列配置: " << checkQuery.lastError().text();
        return false;
    }

    int columnCount = checkQuery.value(0).toInt();
    if (columnCount > 0)
    {
        qDebug() << funcName << " - 表ID " << tableId << " 已有 " << columnCount << " 个列配置，不需要修复";
        return true; // 已有列配置，不需要修复
    }

    // 添加默认列配置
    if (!addDefaultColumnConfigurations(tableId))
    {
        qCritical() << funcName << " - 无法为表ID " << tableId << " 添加默认列配置";
        return false;
    }

    // 查询与这个表关联的管脚
    QSqlQuery pinQuery(db);
    pinQuery.prepare("SELECT vtp.id, pl.pin_name, vtp.pin_id, vtp.pin_channel_count, vtp.pin_type "
                     "FROM vector_table_pins vtp "
                     "JOIN pin_list pl ON vtp.pin_id = pl.id "
                     "WHERE vtp.table_id = ?");
    pinQuery.addBindValue(tableId);

    if (!pinQuery.exec())
    {
        qCritical() << funcName << " - 无法查询表ID " << tableId << " 的管脚: " << pinQuery.lastError().text();
        return false;
    }

    // 设置事务
    db.transaction();

    try
    {
        // 从标准列开始，所以列序号从6开始（0-5已经由addDefaultColumnConfigurations添加）
        int columnOrder = 6;

        while (pinQuery.next())
        {
            int pinTableId = pinQuery.value(0).toInt();
            QString pinName = pinQuery.value(1).toString();
            int pinId = pinQuery.value(2).toInt();
            int channelCount = pinQuery.value(3).toInt();
            int pinType = pinQuery.value(4).toInt();

            // 构造JSON属性字符串
            QString jsonProps = QString("{\"pin_list_id\": %1, \"channel_count\": %2, \"type_id\": %3}")
                                    .arg(pinId)
                                    .arg(channelCount)
                                    .arg(pinType);

            // 添加管脚列配置
            QSqlQuery colInsertQuery(db);
            colInsertQuery.prepare("INSERT INTO VectorTableColumnConfiguration "
                                   "(master_record_id, column_name, column_order, column_type, data_properties) "
                                   "VALUES (?, ?, ?, ?, ?)");
            colInsertQuery.addBindValue(tableId);
            colInsertQuery.addBindValue(pinName);
            colInsertQuery.addBindValue(columnOrder++);
            colInsertQuery.addBindValue("PIN_STATE_ID");
            colInsertQuery.addBindValue(jsonProps);

            if (!colInsertQuery.exec())
            {
                throw QString("无法添加管脚列配置: " + colInsertQuery.lastError().text());
            }

            qDebug() << funcName << " - 已成功为表ID " << tableId << " 添加管脚列 " << pinName;
        }

        // 更新表的总列数
        QSqlQuery updateColumnCountQuery(db);
        updateColumnCountQuery.prepare("UPDATE VectorTableMasterRecord SET column_count = ? WHERE id = ?");
        updateColumnCountQuery.addBindValue(columnOrder); // 总列数
        updateColumnCountQuery.addBindValue(tableId);

        if (!updateColumnCountQuery.exec())
        {
            throw QString("无法更新列数: " + updateColumnCountQuery.lastError().text());
        }

        // 提交事务
        if (!db.commit())
        {
            throw QString("无法提交事务: " + db.lastError().text());
        }

        qDebug() << funcName << " - 成功修复表ID " << tableId << " 的列配置，共添加了 " << columnOrder << " 列";
        return true;
    }
    catch (const QString &error)
    {
        qCritical() << funcName << " - 错误: " << error;
        db.rollback();
        return false;
    }
}

void MainWindow::checkAndFixAllVectorTables()
{
    const QString funcName = "MainWindow::checkAndFixAllVectorTables";
    qDebug() << funcName << " - 开始检查和修复所有向量表的列配置";

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qCritical() << funcName << " - 数据库未打开";
        return;
    }

    // 获取所有向量表主记录
    QSqlQuery query(db);
    query.prepare("SELECT id FROM VectorTableMasterRecord");

    if (!query.exec())
    {
        qCritical() << funcName << " - 无法查询向量表主记录:" << query.lastError().text();
        return;
    }

    int fixedCount = 0;
    int totalCount = 0;

    while (query.next())
    {
        int tableId = query.value(0).toInt();
        totalCount++;

        // 检查是否有列配置
        QSqlQuery checkQuery(db);
        checkQuery.prepare("SELECT COUNT(*) FROM VectorTableColumnConfiguration WHERE master_record_id = ?");
        checkQuery.addBindValue(tableId);

        if (!checkQuery.exec() || !checkQuery.next())
        {
            qWarning() << funcName << " - 无法检查表ID " << tableId << " 的列配置:" << checkQuery.lastError().text();
            continue;
        }

        int columnCount = checkQuery.value(0).toInt();
        if (columnCount == 0)
        {
            qDebug() << funcName << " - 表ID " << tableId << " 没有列配置，尝试修复";
            if (fixExistingTableWithoutColumns(tableId))
            {
                fixedCount++;
                qDebug() << funcName << " - 成功修复表ID " << tableId << " 的列配置";
            }
            else
            {
                qWarning() << funcName << " - 修复表ID " << tableId << " 的列配置失败";
            }
        }
        else
        {
            qDebug() << funcName << " - 表ID " << tableId << " 有 " << columnCount << " 个列配置，不需要修复";
        }
    }

    qDebug() << funcName << " - 检查完成，共 " << totalCount << " 个表，修复了 " << fixedCount << " 个表";
}

// 添加一个resizeEvent重写，以便发出windowResized信号
void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    // 记录窗口大小变化
    qDebug() << "MainWindow::resizeEvent - 窗口大小变化: "
             << event->size().width() << "x" << event->size().height()
             << " (旧尺寸: " << event->oldSize().width() << "x" << event->oldSize().height() << ")";

    // 如果窗口大小发生实质性变化，才更新UI
    if (event->size() != event->oldSize())
    {
        // 通知窗口尺寸变化
        emit windowResized();

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

        // 更新窗口大小信息
        updateWindowSizeInfo();
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
    QSettings settings("VecEdit", "VecEdit");
    settings.beginGroup("MainWindow");

    // 保存窗口几何属性
    settings.setValue("geometry", saveGeometry());

    // 保存窗口状态（工具栏、停靠窗口等）
    settings.setValue("windowState", QMainWindow::saveState());

    // 保存窗口是否最大化
    settings.setValue("isMaximized", isMaximized());

    // 保存窗口大小和位置
    if (!isMaximized())
    {
        settings.setValue("size", size());
        settings.setValue("pos", pos());
    }

    settings.endGroup();

    qDebug() << "MainWindow::saveWindowState - 窗口状态已保存";
}

// 恢复窗口状态
void MainWindow::restoreWindowState()
{
    QSettings settings("VecEdit", "VecEdit");
    settings.beginGroup("MainWindow");

    // 恢复窗口几何属性
    if (settings.contains("geometry"))
    {
        restoreGeometry(settings.value("geometry").toByteArray());
    }

    // 恢复窗口状态
    if (settings.contains("windowState"))
    {
        restoreState(settings.value("windowState").toByteArray());
    }

    // 恢复窗口大小和位置
    if (settings.contains("size") && settings.contains("pos"))
    {
        resize(settings.value("size").toSize());
        move(settings.value("pos").toPoint());
    }

    // 恢复窗口是否最大化
    if (settings.value("isMaximized", false).toBool())
    {
        showMaximized();
    }

    settings.endGroup();

    // 更新窗口大小信息
    updateWindowSizeInfo();

    qDebug() << "MainWindow::restoreWindowState - 窗口状态已恢复";
}
