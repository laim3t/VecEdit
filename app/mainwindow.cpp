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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
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

    // 添加填充TimeSet按钮
    m_fillTimeSetButton = new QPushButton(tr("填充TimeSet"), this);
    connect(m_fillTimeSetButton, &QPushButton::clicked, this, &MainWindow::showFillTimeSetDialog);
    controlLayout->addWidget(m_fillTimeSetButton);

    // 添加替换TimeSet按钮
    m_replaceTimeSetButton = new QPushButton(tr("替换TimeSet"), this);
    connect(m_replaceTimeSetButton, &QPushButton::clicked, this, &MainWindow::showReplaceTimeSetDialog);
    controlLayout->addWidget(m_replaceTimeSetButton);

    // 添加刷新按钮
    m_refreshButton = new QPushButton(tr("刷新"), this);
    connect(m_refreshButton, &QPushButton::clicked, this, &MainWindow::refreshVectorTableData);
    controlLayout->addWidget(m_refreshButton);

    // 添加TimeSet设置按钮
    m_timeSetSettingsButton = new QPushButton(tr("TimeSet设置"), this);
    connect(m_timeSetSettingsButton, &QPushButton::clicked, this, &MainWindow::openTimeSetSettingsDialog);
    controlLayout->addWidget(m_timeSetSettingsButton);

    // 添加其他按钮
    QPushButton *addRowButton = new QPushButton(tr("添加向量行"), this);
    QPushButton *deleteRowButton = new QPushButton(tr("删除向量行"), this);
    m_deleteRangeButton = new QPushButton(tr("删除指定范围内的向量行"), this);
    QPushButton *addTableButton = new QPushButton(tr("新建向量表"), this);
    QPushButton *deleteTableButton = new QPushButton(tr("删除向量表"), this);
    QPushButton *saveButton = new QPushButton(tr("保存"), this);

    // 连接信号
    connect(addRowButton, &QPushButton::clicked, this, &MainWindow::addRowToCurrentVectorTable);
    connect(deleteRowButton, &QPushButton::clicked, this, &MainWindow::deleteSelectedVectorRows);
    connect(m_deleteRangeButton, &QPushButton::clicked, this, &MainWindow::deleteVectorRowsInRange);
    connect(addTableButton, &QPushButton::clicked, this, &MainWindow::addNewVectorTable);
    connect(deleteTableButton, &QPushButton::clicked, this, &MainWindow::deleteCurrentVectorTable);
    connect(saveButton, &QPushButton::clicked, this, &MainWindow::saveVectorTableData);
    connect(m_vectorTableSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onVectorTableSelectionChanged);

    // 添加按钮到控制布局
    controlLayout->addWidget(addRowButton);
    controlLayout->addWidget(deleteRowButton);
    controlLayout->addWidget(m_deleteRangeButton);
    controlLayout->addWidget(addTableButton);
    controlLayout->addWidget(deleteTableButton);
    controlLayout->addWidget(saveButton);
    controlLayout->addStretch();

    // 创建向量表控件
    m_vectorTableWidget = new QTableWidget(this);
    m_vectorTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_vectorTableWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_vectorTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_vectorTableWidget->verticalHeader()->setVisible(true);
    m_vectorTableWidget->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);

    // 表格代理
    m_itemDelegate = new VectorTableItemDelegate(this);
    m_vectorTableWidget->setItemDelegate(m_itemDelegate);

    // 应用表格样式
    TableStyleManager::applyTableStyle(m_vectorTableWidget);

    // 添加到容器布局
    containerLayout->addLayout(controlLayout);
    containerLayout->addWidget(m_vectorTableWidget);

    // 将容器添加到主布局
    QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout *>(m_centralWidget->layout());
    if (mainLayout)
    {
        mainLayout->addWidget(m_vectorTableContainer);
    }
}

void MainWindow::loadVectorTable()
{
    qDebug() << "MainWindow::loadVectorTable - 开始加载向量表";

    // 清空当前选择框
    m_vectorTableSelector->clear();

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
            m_vectorTableSelector->addItem(tableName, tableId);
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
    if (index < 0)
        return;

    // 获取当前选中表ID
    int tableId = m_vectorTableSelector->currentData().toInt();

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
        if (tableName.isEmpty())
        {
            qDebug() << "MainWindow::addNewVectorTable - 表名为空，操作取消";
            QMessageBox::warning(this, "错误", "表名不能为空");
            return;
        }

        qDebug() << "MainWindow::addNewVectorTable - 用户输入的表名:" << tableName;

        // 检查表名是否已存在
        QSqlQuery query(db);
        query.prepare("SELECT COUNT(*) FROM vector_tables WHERE table_name = ?");
        query.addBindValue(tableName);
        if (query.exec() && query.next())
        {
            int count = query.value(0).toInt();
            if (count > 0)
            {
                qDebug() << "MainWindow::addNewVectorTable - 表名已存在";
                QMessageBox::warning(this, "错误", "该表名已存在，请使用其他名称");
                return;
            }
        }
        else
        {
            qDebug() << "MainWindow::addNewVectorTable - 检查表名失败:" << query.lastError().text();
        }

        qDebug() << "MainWindow::addNewVectorTable - 表名检查通过，准备插入新表";

        // 向数据库添加新表
        query.prepare("INSERT INTO vector_tables (table_name) VALUES (?)");
        query.addBindValue(tableName);
        if (query.exec())
        {
            int tableId = query.lastInsertId().toInt();
            qDebug() << "MainWindow::addNewVectorTable - 添加表成功，新表ID:" << tableId;

            // 显示管脚选择对话框
            showPinSelectionDialog(tableId, tableName);

            // 刷新向量表列表
            loadVectorTable();

            // 选择新添加的表
            for (int i = 0; i < m_vectorTableSelector->count(); i++)
            {
                if (m_vectorTableSelector->itemData(i).toInt() == tableId)
                {
                    m_vectorTableSelector->setCurrentIndex(i);
                    break;
                }
            }

            QMessageBox::information(this, "成功", "向量表 '" + tableName + "' 已成功创建");
        }
        else
        {
            qDebug() << "MainWindow::addNewVectorTable - 添加表失败:" << query.lastError().text();
            QMessageBox::critical(this, "错误", "创建向量表失败: " + query.lastError().text());
        }
    }
    else
    {
        qDebug() << "MainWindow::addNewVectorTable - 用户取消操作";
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

    // 弹出确认对话框
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "确认删除",
                                  "确定要删除向量表 \"" + tableName + "\" 吗？\n此操作不可撤销。",
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::No)
    {
        return;
    }

    // 使用数据处理器删除向量表
    QString errorMessage;
    if (m_dataHandler->deleteVectorTable(tableId, errorMessage))
    {
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
    }
}

void MainWindow::showPinSelectionDialog(int tableId, const QString &tableName)
{
    if (m_dialogManager)
    {
        m_dialogManager->showPinSelectionDialog(tableId, tableName);
    }
}

void MainWindow::showVectorDataDialog(int tableId, const QString &tableName, int startIndex)
{
    if (m_dialogManager)
    {
        m_dialogManager->showVectorDataDialog(tableId, tableName, startIndex);
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

        if (reply == QMessageBox::Yes)
        {
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
        else
        {
            qDebug() << "MainWindow::deleteVectorRowsInRange - 用户取消删除操作";
        }
    }
    else
    {
        qDebug() << "MainWindow::deleteVectorRowsInRange - 用户取消对话框";
    }
}

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

        // 如果当前有选中的向量表，刷新它的显示
        if (m_vectorTableSelector->currentIndex() >= 0)
        {
            onVectorTableSelectionChanged(m_vectorTableSelector->currentIndex());
        }

        QMessageBox::information(this, "成功", "管脚设置已更新");
        statusBar()->showMessage("管脚设置已更新");
    }
    else
    {
        qDebug() << "MainWindow::openPinSettingsDialog - 用户取消了管脚设置";
    }
}

void MainWindow::addSinglePin()
{
    qDebug() << "MainWindow::addSinglePin - 开始添加单个管脚";

    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        QMessageBox::warning(this, "警告", "请先打开或创建一个项目数据库");
        qDebug() << "MainWindow::addSinglePin - 数据库未连接";
        return;
    }

    // 弹出对话框输入管脚名称
    bool ok;
    QString pinName = QInputDialog::getText(this, "添加管脚",
                                            "请输入管脚名称:", QLineEdit::Normal,
                                            "", &ok);

    if (!ok || pinName.isEmpty())
    {
        qDebug() << "MainWindow::addSinglePin - 用户取消添加或未输入名称";
        return;
    }

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << "MainWindow::addSinglePin - 数据库未连接";
        QMessageBox::critical(this, "错误", "数据库连接失败，无法添加管脚");
        return;
    }

    // 检查管脚名称是否已存在
    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT id FROM pin_list WHERE pin_name = ?");
    checkQuery.addBindValue(pinName);

    if (checkQuery.exec() && checkQuery.next())
    {
        qDebug() << "MainWindow::addSinglePin - 管脚名称已存在:" << pinName;
        QMessageBox::warning(this, "重复的管脚名称",
                             QString("管脚名称 '%1' 已存在，请使用其他名称").arg(pinName));
        return;
    }

    // 添加新管脚到pin_list表
    QSqlQuery insertQuery(db);
    insertQuery.prepare("INSERT INTO pin_list (pin_name, pin_note) VALUES (?, '')");
    insertQuery.addBindValue(pinName);

    if (!insertQuery.exec())
    {
        qWarning() << "MainWindow::addSinglePin - 无法添加新管脚:" << insertQuery.lastError().text();
        QMessageBox::critical(this, "错误",
                              QString("添加管脚失败: %1").arg(insertQuery.lastError().text()));
        return;
    }

    int newPinId = insertQuery.lastInsertId().toInt();
    qDebug() << "MainWindow::addSinglePin - 成功添加新管脚，ID=" << newPinId << "，名称=" << pinName;

    // 打开管脚设置对话框以便用户设置
    openPinSettingsDialog();

    QMessageBox::information(this, "添加成功",
                             QString("成功添加管脚 '%1'，默认通道个数为1，请在管脚设置中配置工位信息").arg(pinName));
}
