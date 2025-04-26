#include "mainwindow.h"
#include "database/databasemanager.h"
#include "database/databaseviewdialog.h"
#include "pin/pinlistdialog.h"
#include "timeset/timesetdialog.h"
#include "timeset/filltimesetdialog.h"
#include "pin/pinvalueedit.h"
#include "vector/vectortabledelegate.h"
#include "vector/vectordatahandler.h"
#include "common/dialogmanager.h"

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

    // 添加填充TimeSet按钮
    m_fillTimeSetButton = new QPushButton(tr("填充TimeSet"), this);
    connect(m_fillTimeSetButton, &QPushButton::clicked, this, &MainWindow::showFillTimeSetDialog);
    controlLayout->addWidget(m_fillTimeSetButton);

    // 添加刷新按钮
    m_refreshButton = new QPushButton(tr("刷新"), this);
    connect(m_refreshButton, &QPushButton::clicked, this, &MainWindow::refreshVectorTableData);
    controlLayout->addWidget(m_refreshButton);

    // 添加TimeSet设置按钮
    m_timeSetSettingsButton = new QPushButton(tr("设置TimeSet"), this);
    connect(m_timeSetSettingsButton, &QPushButton::clicked, this, &MainWindow::openTimeSetSettingsDialog);
    controlLayout->addWidget(m_timeSetSettingsButton);

    // 添加其他按钮
    QPushButton *addRowButton = new QPushButton(tr("添加行"), this);
    QPushButton *deleteRowButton = new QPushButton(tr("删除所选行"), this);
    QPushButton *addTableButton = new QPushButton(tr("添加向量表"), this);
    QPushButton *deleteTableButton = new QPushButton(tr("删除当前向量表"), this);
    QPushButton *saveButton = new QPushButton(tr("保存"), this);

    // 连接信号
    connect(addRowButton, &QPushButton::clicked, this, &MainWindow::addRowToCurrentVectorTable);
    connect(deleteRowButton, &QPushButton::clicked, this, &MainWindow::deleteSelectedVectorRows);
    connect(addTableButton, &QPushButton::clicked, this, &MainWindow::addNewVectorTable);
    connect(deleteTableButton, &QPushButton::clicked, this, &MainWindow::deleteCurrentVectorTable);
    connect(saveButton, &QPushButton::clicked, this, &MainWindow::saveVectorTableData);
    connect(m_vectorTableSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onVectorTableSelectionChanged);

    // 添加按钮到控制布局
    controlLayout->addWidget(addRowButton);
    controlLayout->addWidget(deleteRowButton);
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

    // 创建并显示填充TimeSet对话框
    FillTimeSetDialog dialog(this);
    dialog.setVectorRowCount(rowCount);

    if (dialog.exec() == QDialog::Accepted)
    {
        // 获取用户输入
        int timeSetId = dialog.getSelectedTimeSetId();
        int startRow = dialog.getStartRow();
        int endRow = dialog.getEndRow();
        int step = dialog.getStepValue();

        // 填充TimeSet
        fillTimeSetForVectorTable(timeSetId, startRow, endRow, step);
    }
}

void MainWindow::fillTimeSetForVectorTable(int timeSetId, int startRow, int endRow, int step)
{
    // 检查参数有效性
    if (timeSetId <= 0 || startRow < 0 || endRow < startRow || step <= 0)
    {
        QMessageBox::warning(this, tr("错误"), tr("填充TimeSet参数无效"));
        return;
    }

    // 获取当前向量表ID
    int currentIndex = m_vectorTableSelector->currentIndex();
    if (currentIndex < 0)
    {
        return;
    }
    int tableId = m_vectorTableSelector->itemData(currentIndex).toInt();

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接失败"));
        return;
    }

    // 开始事务
    db.transaction();

    try
    {
        // 准备更新SQL
        QSqlQuery query(db);
        query.prepare("UPDATE vector_data SET timeset_id = :timesetId WHERE vector_table_id = :tableId AND row_index >= :startRow AND row_index <= :endRow AND (row_index - :startRow) % :step = 0");
        query.bindValue(":timesetId", timeSetId);
        query.bindValue(":tableId", tableId);
        query.bindValue(":startRow", startRow);
        query.bindValue(":endRow", endRow);
        query.bindValue(":step", step);

        if (!query.exec())
        {
            throw std::runtime_error(query.lastError().text().toStdString());
        }

        // 提交事务
        if (!db.commit())
        {
            throw std::runtime_error(db.lastError().text().toStdString());
        }

        // 重新加载表格数据
        loadVectorTable();

        // 显示成功消息
        QMessageBox::information(this, tr("成功"), tr("TimeSet填充完成"));
    }
    catch (const std::exception &e)
    {
        // 回滚事务
        db.rollback();

        // 显示错误消息
        QMessageBox::critical(this, tr("错误"), tr("填充TimeSet失败: %1").arg(e.what()));
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
