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
#include <QToolBar>
#include <QToolButton>
#include <QVariant>
#include <QProgressDialog>
#include <QTreeView>
#include <QStandardItemModel>

#include "migration/datamigrator.h"

// 辅助函数：比较两个列配置列表，判断是否发生了可能影响二进制布局的实质性变化
// 只比较列的数量、名称和顺序。
static bool areColumnConfigurationsDifferentSimplified(const QList<Vector::ColumnInfo> &oldCols, const QList<Vector::ColumnInfo> &newCols)
{
    if (oldCols.size() != newCols.size())
    {
        return true; // 列数量不同，需要迁移
    }

    // 假设列是按 column_order 排序的
    for (int i = 0; i < oldCols.size(); ++i)
    {
        if (oldCols[i].name != newCols[i].name || oldCols[i].order != newCols[i].order)
        {
            return true; // 列名或顺序不同，需要迁移
        }
    }

    return false;
}

// 辅助函数：将旧的行数据适配到新的列结构
static QList<Vector::RowData> adaptRowDataToNewColumns(const QList<Vector::RowData> &oldData, const QList<Vector::ColumnInfo> &oldCols, const QList<Vector::ColumnInfo> &newCols)
{
    if (oldData.isEmpty())
    {
        return {}; // 如果没有旧数据，无需迁移
    }

    QList<Vector::RowData> newDataList;
    newDataList.reserve(oldData.size());

    for (const auto &oldRow : oldData)
    {
        // 为旧行数据创建一个 名称 -> 数据值 的映射，方便查找
        QMap<QString, QVariant> oldRowMap;
        for (int i = 0; i < oldCols.size(); ++i)
        {
            if (i < oldRow.size())
            {
                oldRowMap[oldCols[i].name] = oldRow[i];
            }
        }

        Vector::RowData newRow;
        for (int i = 0; i < newCols.size(); ++i)
        {
            newRow.append(QVariant());
        }

        for (int i = 0; i < newCols.size(); ++i)
        {
            const auto &newColInfo = newCols[i];

            if (oldRowMap.contains(newColInfo.name))
            {
                // 如果新列在旧数据中存在，则直接拷贝
                newRow[i] = oldRowMap[newColInfo.name];
            }
            else
            {
                // 如果是新增的列，则赋予默认值
                if (newColInfo.type == Vector::ColumnDataType::PIN_STATE_ID)
                {
                    newRow[i] = "X"; // 新增管脚列的默认值为 "X"
                }
                else
                {
                    // For other types, the default QVariant is already what we want
                    // so we don't need to do anything here.
                    // The list was already filled with default QVariants.
                    // newRow[i] = QVariant();
                }
            }
        }
        newDataList.append(newRow);
    }

    return newDataList;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_isUpdatingUI(false)
{
    setupUI();
    setupMenu();
    setupSidebarNavigator(); // 添加侧边导航栏设置

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

void MainWindow::createNewProject()
{
    // 先关闭当前项目
    closeCurrentProject();

    // 使用QSettings获取上次使用的路径
    QSettings settings(QDir::homePath() + "/.vecedit/settings.ini", QSettings::IniFormat);
    QString lastPath = settings.value("lastUsedPath", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();
    QDir dir(lastPath);
    if (!dir.exists())
    {
        lastPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }

    // 选择保存位置和文件名
    QString dbPath = QFileDialog::getSaveFileName(this, tr("保存项目数据库"),
                                                  lastPath + "/VecEditProject.db",
                                                  tr("SQLite数据库 (*.db)"));
    if (dbPath.isEmpty())
    {
        return;
    }

    // 保存本次使用的路径
    settings.setValue("lastUsedPath", QFileInfo(dbPath).absolutePath());

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

        // 更新菜单状态
        updateMenuState();

        // QMessageBox::information(this, tr("成功"), message);
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

    // 使用QSettings获取上次使用的路径
    QSettings settings(QDir::homePath() + "/.vecedit/settings.ini", QSettings::IniFormat);
    QString lastPath = settings.value("lastUsedPath", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();
    QDir dir(lastPath);
    if (!dir.exists())
    {
        lastPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }

    // 选择要打开的数据库文件
    QString dbPath = QFileDialog::getOpenFileName(this, tr("打开项目数据库"),
                                                  lastPath,
                                                  tr("SQLite数据库 (*.db)"));
    if (dbPath.isEmpty())
    {
        return;
    }

    // 保存本次使用的路径
    settings.setValue("lastUsedPath", QFileInfo(dbPath).absolutePath());

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

        // 刷新侧边导航栏
        refreshSidebarNavigator();

        // 设置窗口标题
        setWindowTitle(tr("向量编辑器 [%1]").arg(QFileInfo(dbPath).fileName()));

        // 更新菜单状态
        updateMenuState();

        // QMessageBox::information(this, tr("成功"),
        //                          tr("项目数据库已打开！当前版本：%1\n您可以通过\"查看\"菜单打开数据库查看器").arg(version));
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

        // 清理侧边导航栏
        resetSidebarNavigator();

        // 重置窗口标题
        setWindowTitle("向量编辑器");
        statusBar()->showMessage("项目已关闭");

        // 更新菜单状态
        updateMenuState();
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

        // 刷新侧边导航栏
        refreshSidebarNavigator();

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
    m_vectorTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_vectorTableWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_vectorTableWidget->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_vectorTableWidget->horizontalHeader()->setStretchLastSection(true);
    m_vectorTableWidget->verticalHeader()->setDefaultSectionSize(25);
    m_vectorTableWidget->verticalHeader()->setVisible(true);

    // 连接单元格修改信号
    connect(m_vectorTableWidget, &QTableWidget::cellChanged, this, &MainWindow::onTableCellChanged);

    // 连接自定义单元格编辑器的值修改信号 (使用于PinValueLineEdit等)
    connect(m_vectorTableWidget, &QTableWidget::cellWidget, [this](int row, int column)
            {
        QWidget *widget = m_vectorTableWidget->cellWidget(row, column);
        if (PinValueLineEdit *pinEdit = qobject_cast<PinValueLineEdit*>(widget)) {
            connect(pinEdit, &PinValueLineEdit::textChanged, [this, row]() {
                onTableRowModified(row);
            });
        } });

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

// 更新分页信息显示
void MainWindow::updatePaginationInfo()
{
    const QString funcName = "MainWindow::updatePaginationInfo";
    qDebug() << funcName << " - 更新分页信息，当前页:" << m_currentPage << "，总页数:" << m_totalPages << "，总行数:" << m_totalRows;

    // 更新页码信息标签
    m_pageInfoLabel->setText(tr("第 %1/%2 页，共 %3 行").arg(m_currentPage + 1).arg(m_totalPages).arg(m_totalRows));

    // 更新上一页按钮状态
    m_prevPageButton->setEnabled(m_currentPage > 0);

    // 更新下一页按钮状态
    m_nextPageButton->setEnabled(m_currentPage < m_totalPages - 1 && m_totalPages > 0);

    // 更新页码跳转输入框
    m_pageJumper->setMaximum(m_totalPages > 0 ? m_totalPages : 1);
    m_pageJumper->setValue(m_currentPage + 1);

    // 根据总页数启用或禁用跳转按钮
    m_jumpButton->setEnabled(m_totalPages > 1);
}

// 加载当前页数据
void MainWindow::loadCurrentPage()
{
    const QString funcName = "MainWindow::loadCurrentPage";
    qDebug() << funcName << " - 加载当前页数据，页码:" << m_currentPage;

    // 获取当前选中的向量表ID
    int tabIndex = m_vectorTabWidget->currentIndex();
    if (tabIndex < 0 || !m_tabToTableId.contains(tabIndex))
    {
        qWarning() << funcName << " - 没有选中有效的向量表";
        return;
    }

    int tableId = m_tabToTableId[tabIndex];
    qDebug() << funcName << " - 当前向量表ID:" << tableId;

    // 获取向量表总行数
    m_totalRows = VectorDataHandler::instance().getVectorTableRowCount(tableId);

    // 计算总页数
    m_totalPages = (m_totalRows + m_pageSize - 1) / m_pageSize; // 向上取整

    // 确保当前页码在有效范围内
    if (m_currentPage < 0)
        m_currentPage = 0;
    if (m_currentPage >= m_totalPages && m_totalPages > 0)
        m_currentPage = m_totalPages - 1;

    qDebug() << funcName << " - 总行数:" << m_totalRows << "，总页数:" << m_totalPages << "，当前页:" << m_currentPage;

    // 在加载新页面数据前，自动保存当前页面的修改
    /*
    QString errorMsg;
    if (m_vectorTableWidget->rowCount() > 0) // 确保当前有数据需要保存
    {
        qDebug() << funcName << " - 在切换页面前自动保存当前页面修改";
        if (!VectorDataHandler::instance().saveVectorTableData(tableId, m_vectorTableWidget, errorMsg))
        {
            qWarning() << funcName << " - 保存当前页面失败:" << errorMsg;
        }
        else
        {
            qDebug() << funcName << " - 当前页面保存成功";
        }
    }
    */

    // 加载当前页数据
    bool success = VectorDataHandler::instance().loadVectorTablePageData(tableId, m_vectorTableWidget, m_currentPage, m_pageSize);

    if (!success)
    {
        qWarning() << funcName << " - 加载页面数据失败";
    }

    // 更新分页信息显示
    updatePaginationInfo();
}

// 加载下一页
void MainWindow::loadNextPage()
{
    const QString funcName = "MainWindow::loadNextPage";
    qDebug() << funcName << " - 加载下一页";

    if (m_currentPage < m_totalPages - 1)
    {
        m_currentPage++;
        loadCurrentPage();
    }
    else
    {
        qWarning() << funcName << " - 已经是最后一页";
    }
}

// 加载上一页
void MainWindow::loadPrevPage()
{
    const QString funcName = "MainWindow::loadPrevPage";
    qDebug() << funcName << " - 加载上一页";

    if (m_currentPage > 0)
    {
        m_currentPage--;
        loadCurrentPage();
    }
    else
    {
        qWarning() << funcName << " - 已经是第一页";
    }
}

// 修改每页行数
void MainWindow::changePageSize(int newSize)
{
    const QString funcName = "MainWindow::changePageSize";
    qDebug() << funcName << " - 修改每页行数为:" << newSize;

    if (newSize <= 0)
    {
        qWarning() << funcName << " - 无效的页面大小:" << newSize;
        return;
    }

    // 保存当前页的第一行在整个数据集中的索引
    int currentFirstRow = m_currentPage * m_pageSize;

    // 更新页面大小
    m_pageSize = newSize;

    // 计算新的页码
    m_currentPage = currentFirstRow / m_pageSize;

    // 重新加载当前页
    loadCurrentPage();
}

// 跳转到指定页
void MainWindow::jumpToPage(int pageNum)
{
    const QString funcName = "MainWindow::jumpToPage";
    qDebug() << funcName << " - 跳转到页码:" << pageNum;

    if (pageNum < 0 || pageNum >= m_totalPages)
    {
        qWarning() << funcName << " - 无效的页码:" << pageNum;
        return;
    }

    m_currentPage = pageNum;
    loadCurrentPage();
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

    // 重置分页状态
    m_currentPage = 0;

    // 获取总行数并更新页面信息
    m_totalRows = VectorDataHandler::instance().getVectorTableRowCount(tableId);
    m_totalPages = (m_totalRows + m_pageSize - 1) / m_pageSize; // 向上取整

    // 更新分页信息显示
    updatePaginationInfo();

    // 使用分页方式加载数据
    qDebug() << funcName << " - 开始加载表格数据，表ID:" << tableId << "，使用分页加载，页码:" << m_currentPage << "，每页行数:" << m_pageSize;
    bool loadSuccess = VectorDataHandler::instance().loadVectorTablePageData(tableId, m_vectorTableWidget, m_currentPage, m_pageSize);
    qDebug() << funcName << " - VectorDataHandler::loadVectorTablePageData 返回:" << loadSuccess
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
            // 重新加载表格（使用分页）
            loadSuccess = VectorDataHandler::instance().loadVectorTablePageData(tableId, m_vectorTableWidget, m_currentPage, m_pageSize);
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
                // 重置分页状态
                m_currentPage = 0;

                // 获取总行数并更新页面信息
                m_totalRows = VectorDataHandler::instance().getVectorTableRowCount(tableId);
                m_totalPages = (m_totalRows + m_pageSize - 1) / m_pageSize; // 向上取整

                // 更新分页信息显示
                updatePaginationInfo();

                // 使用分页方式加载数据
                if (VectorDataHandler::instance().loadVectorTablePageData(tableId, m_vectorTableWidget, m_currentPage, m_pageSize))
                {
                    qDebug() << "MainWindow::syncComboBoxWithTab - 成功重新加载表格数据，页码:" << m_currentPage
                             << "，每页行数:" << m_pageSize << "，列数:" << m_vectorTableWidget->columnCount();
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

    // 创建并显示保存中对话框
    QMessageBox savingDialog(this);
    savingDialog.setWindowTitle("保存中");
    savingDialog.setText("正在保存数据，请稍候...");
    savingDialog.setStandardButtons(QMessageBox::NoButton);
    savingDialog.setIcon(QMessageBox::Information);

    // 使对话框非模态，并立即显示
    savingDialog.setModal(false);
    savingDialog.show();

    // 立即处理事件，确保对话框显示出来
    QCoreApplication::processEvents();

    // 保存当前表格的状态信息
    int currentPage = m_currentPage;
    int pageSize = m_pageSize;

    // 保存结果变量
    bool saveSuccess = false;
    QString errorMessage;

    // 性能优化：检查是否在分页模式下，是否有待保存的修改
    if (m_totalRows > pageSize)
    {
        qDebug() << funcName << " - 检测到分页模式，准备保存数据";

        // 优化：直接传递分页信息到VectorDataHandler，避免创建临时表格
        saveSuccess = VectorDataHandler::instance().saveVectorTableDataPaged(
            tableId,
            targetTableWidget,
            currentPage,
            pageSize,
            m_totalRows,
            errorMessage);
    }
    else
    {
        // 非分页模式，统一使用 saveVectorTableDataPaged 进行保存
        qDebug() << funcName << " - 非分页模式，但仍调用 saveVectorTableDataPaged 以启用增量更新尝试";
        int currentRowCount = targetTableWidget ? targetTableWidget->rowCount() : 0;
        saveSuccess = VectorDataHandler::instance().saveVectorTableDataPaged(
            tableId,
            targetTableWidget,
            0,               // currentPage 设为 0
            currentRowCount, // pageSize 设为当前行数
            currentRowCount, // totalRows 设为当前行数
            errorMessage);
    }

    // 关闭"保存中"对话框
    savingDialog.close();

    // 根据保存结果显示相应的消息
    if (saveSuccess)
    {
        // 检查errorMessage中是否包含"没有检测到数据变更"消息
        if (errorMessage.contains("没有检测到数据变更"))
        {
            // 无变更的情况
            QMessageBox::information(this, "保存结果", errorMessage);
            statusBar()->showMessage(errorMessage);
        }
        else
        {
            // 有变更的情况，显示保存了多少行
            QMessageBox::information(this, "保存成功", errorMessage);
            statusBar()->showMessage(errorMessage);
        }

        // 不再重新加载当前页数据，保留用户的编辑状态
        qDebug() << funcName << " - 保存操作完成，保留用户当前的界面编辑状态";
    }
    else
    {
        // 保存失败的情况
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
    QLabel *nameLabel = new QLabel("向量表名称:", &vectorNameDialog);
    layout->addWidget(nameLabel);

    QLineEdit *nameEdit = new QLineEdit(&vectorNameDialog);
    nameEdit->setPlaceholderText("请输入向量表名称");
    layout->addWidget(nameEdit);

    // 按钮
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &vectorNameDialog);
    layout->addWidget(buttonBox);

    // 连接信号和槽
    connect(buttonBox, &QDialogButtonBox::accepted, &vectorNameDialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &vectorNameDialog, &QDialog::reject);

    // 显示对话框
    if (vectorNameDialog.exec() == QDialog::Accepted)
    {
        // 获取用户输入的表名
        QString tableName = nameEdit->text().trimmed();
        qDebug() << funcName << " - 用户输入的表名: " << tableName;

        // 检查表名是否为空
        if (tableName.isEmpty())
        {
            qDebug() << funcName << " - 表名为空，操作取消";
            QMessageBox::warning(this, "错误", "请输入有效的向量表名称");
            return;
        }

        // 检查表名是否已存在
        QSqlQuery checkQuery(db);
        checkQuery.prepare("SELECT COUNT(*) FROM vector_tables WHERE table_name = ?");
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

            // 在成功创建向量表后添加代码，在执行成功后刷新导航栏
            // 查找类似于 "QMessageBox::information(this, "成功", "成功创建向量表");" 这样的代码之后添加刷新
            // 示例位置
            refreshSidebarNavigator();
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
    const QString funcName = "MainWindow::showPinSelectionDialog";
    qDebug() << funcName << " - 开始显示管脚选择对话框 for tableId:" << tableId << "Name:" << tableName;

    // 确保数据库已连接且表ID有效
    if (!DatabaseManager::instance()->isDatabaseConnected() || tableId <= 0)
    { // Corrected: ->isDatabaseConnected()
        qWarning() << funcName << " - 数据库未连接或表ID无效 (" << tableId << ")";
        return;
    }

    // Corrected: use m_dialogManager instance
    bool success = m_dialogManager->showPinSelectionDialog(tableId, tableName);

    if (success)
    {
        qInfo() << funcName << " - 管脚配置成功完成 for table ID:" << tableId;

        // 新增：在管脚配置成功后，立即更新二进制文件头的列计数
        updateBinaryHeaderColumnCount(tableId);

        // 重新加载并刷新向量表视图以反映更改
        reloadAndRefreshVectorTable(tableId); // Implementation will be added
    }
    else
    {
        qWarning() << funcName << " - 管脚配置被取消或失败 for table ID:" << tableId;
        // 如果这是新表创建流程的一部分，并且管脚配置失败/取消，
        // 可能需要考虑是否回滚表的创建或进行其他清理。
        // 目前，我们只重新加载以确保UI与DB（可能部分配置的）状态一致。
        reloadAndRefreshVectorTable(tableId); // Implementation will be added
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
        int pageOffset = m_currentPage * m_pageSize; // 分页偏移
        foreach (const QModelIndex &index, selectedIndexes)
        {
            int rowIdx = pageOffset + index.row() + 1; // 转换为1-based的绝对行号
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
        if (timeSetId <= 0)
        {
            return; // No valid timeset selected
        }

        QList<int> rowsToUpdate;
        // 修复BUG：无论是否选中行，都使用对话框中设置的范围
        int fromRow = dialog.getStartRow(); // 0-based
        int toRow = dialog.getEndRow();     // 0-based

        // Ensure the range is valid.
        if (fromRow < 0 || toRow < fromRow || toRow >= rowCount)
        {
            QMessageBox::warning(this, "范围无效", "指定的行范围无效。");
            return;
        }

        for (int i = fromRow; i <= toRow; ++i)
        {
            rowsToUpdate.append(i);
        }

        // Call the function to perform the update
        fillTimeSetForVectorTable(timeSetId, rowsToUpdate);
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
        int pageOffset = m_currentPage * m_pageSize; // 分页偏移
        foreach (const QModelIndex &index, selectedIndexes)
        {
            int rowIdx = pageOffset + index.row() + 1; // 转换为1-based的绝对行号
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

        QList<int> rowsToUpdate;
        // 修复BUG：无论是否选中行，都使用对话框中设置的范围
        int fromRow = dialog.getStartRow(); // 0-based
        int toRow = dialog.getEndRow();     // 0-based

        // Ensure the range is valid
        if (fromRow < 0 || toRow < fromRow || toRow >= rowCount)
        {
            QMessageBox::warning(this, "范围无效", "指定的行范围无效。");
            return;
        }

        for (int i = fromRow; i <= toRow; ++i)
        {
            rowsToUpdate.append(i);
        }

        // Replace TimeSet
        replaceTimeSetForVectorTable(fromTimeSetId, toTimeSetId, rowsToUpdate);
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
        QMessageBox::warning(this, tr("警告"), tr("请先打开或创建一个项目数据库"));
        return;
    }

    // 弹出输入对话框
    bool ok;
    QString pinName = QInputDialog::getText(this, tr("添加管脚"),
                                            tr("请输入管脚名称:"), QLineEdit::Normal,
                                            "", &ok);
    if (ok && !pinName.isEmpty())
    {
        // 添加到数据库
        QList<QString> pins;
        pins << pinName;
        if (addPinsToDatabase(pins))
        {
            statusBar()->showMessage(tr("管脚 \"%1\" 添加成功").arg(pinName));

            // 刷新侧边导航栏
            refreshSidebarNavigator();
        }
    }
}

// 删除管脚
void MainWindow::deletePins()
{
    qDebug() << "MainWindow::deletePins - 开始删除管脚";

    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        QMessageBox::warning(this, tr("警告"), tr("请先打开或创建一个项目数据库"));
        return;
    }

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();

    // 查询现有管脚
    QSqlQuery query(db);
    query.exec("SELECT id, pin_name FROM pin_list ORDER BY pin_name");

    // 构建管脚列表
    QMap<int, QString> pinMap;
    while (query.next())
    {
        int id = query.value(0).toInt();
        QString name = query.value(1).toString();
        pinMap.insert(id, name);
    }

    if (pinMap.isEmpty())
    {
        QMessageBox::information(this, tr("提示"), tr("当前没有可删除的管脚"));
        return;
    }

    // 创建选择对话框
    QDialog dialog(this);
    dialog.setWindowTitle(tr("选择要删除的管脚"));
    dialog.setMinimumWidth(350);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QLabel *label = new QLabel(tr("请选择要删除的管脚:"), &dialog);
    layout->addWidget(label);

    // 使用带复选框的滚动区域
    QScrollArea *scrollArea = new QScrollArea(&dialog);
    scrollArea->setWidgetResizable(true);
    QWidget *scrollContent = new QWidget(scrollArea);
    QVBoxLayout *checkBoxLayout = new QVBoxLayout(scrollContent);

    QMap<int, QCheckBox *> checkBoxes;
    for (auto it = pinMap.begin(); it != pinMap.end(); ++it)
    {
        QCheckBox *checkBox = new QCheckBox(it.value(), scrollContent);
        checkBoxes[it.key()] = checkBox;
        checkBoxLayout->addWidget(checkBox);
    }
    scrollContent->setLayout(checkBoxLayout);
    scrollArea->setWidget(scrollContent);
    layout->addWidget(scrollArea);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);

    // 显示对话框并等待用户操作
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    // 收集选中的管脚
    QList<int> pinIdsToDelete;
    QStringList pinNamesToDelete;
    for (auto it = pinMap.begin(); it != pinMap.end(); ++it)
    {
        if (checkBoxes.value(it.key())->isChecked())
        {
            pinIdsToDelete.append(it.key());
            pinNamesToDelete.append(it.value());
        }
    }

    if (pinIdsToDelete.isEmpty())
    {
        QMessageBox::information(this, tr("提示"), tr("未选择任何管脚"));
        return;
    }

    // 检查管脚使用情况
    QMap<QString, QStringList> pinUsageMap; // pinName -> [table1, table2]
    QSqlQuery findUsageQuery(db);

    for (int pinId : pinIdsToDelete)
    {
        // 根据新的 schema, 检查 VectorTableColumnConfiguration, 仅当管脚可见时才视为"正在使用"
        findUsageQuery.prepare(
            "SELECT m.table_name "
            "FROM VectorTableColumnConfiguration c "
            "JOIN VectorTableMasterRecord m ON c.master_record_id = m.id "
            "WHERE c.column_type = 'PIN_STATE_ID' AND json_extract(c.data_properties, '$.pin_list_id') = ? AND c.IsVisible = 1");
        findUsageQuery.addBindValue(pinId);

        if (findUsageQuery.exec())
        {
            while (findUsageQuery.next())
            {
                QString pinName = pinMap.value(pinId);
                QString tableName = findUsageQuery.value(0).toString();
                pinUsageMap[pinName].append(tableName);
            }
        }
        else
        {
            qWarning() << "检查管脚使用情况失败:" << findUsageQuery.lastError().text();
        }
    }

    // 根据使用情况弹出不同确认对话框
    if (!pinUsageMap.isEmpty())
    {
        // 如果有管脚被使用，显示详细警告，并阻止删除
        QString warningText = tr("警告：以下管脚正在被一个或多个向量表使用，无法删除：\n");
        for (auto it = pinUsageMap.begin(); it != pinUsageMap.end(); ++it)
        {
            warningText.append(tr("\n管脚 \"%1\" 被用于: %2").arg(it.key()).arg(it.value().join(", ")));
        }
        warningText.append(tr("\n\n请先从相应的向量表设置中移除这些管脚，然后再尝试删除。"));
        QMessageBox::warning(this, tr("无法删除管脚"), warningText);
        return;
    }

    // 如果没有管脚被使用，弹出标准删除确认
    QString confirmQuestion = tr("您确定要删除以下 %1 个管脚吗？\n\n%2\n\n此操作不可恢复，并将删除所有相关的配置（如Pin Settings, Group Settings等）。")
                                  .arg(pinNamesToDelete.size())
                                  .arg(pinNamesToDelete.join("\n"));
    if (QMessageBox::question(this, tr("确认删除"), confirmQuestion, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
    {
        return;
    }

    // 执行删除操作 (借鉴 PinSettingsDialog 的逻辑)
    db.transaction();
    bool success = true;
    QString errorMsg;

    try
    {
        QSqlQuery deleteQuery(db);
        for (int pinId : pinIdsToDelete)
        {
            // 注意: 这里的删除逻辑需要非常完整，确保所有关联数据都被清除
            // 1. 从 timeset_settings 删除
            deleteQuery.prepare("DELETE FROM timeset_settings WHERE pin_id = ?");
            deleteQuery.addBindValue(pinId);
            if (!deleteQuery.exec())
                throw QString(deleteQuery.lastError().text());

            // 2. 从 pin_group_members 删除
            deleteQuery.prepare("DELETE FROM pin_group_members WHERE pin_id = ?");
            deleteQuery.addBindValue(pinId);
            if (!deleteQuery.exec())
                throw QString(deleteQuery.lastError().text());

            // 3. 从 pin_settings 删除
            deleteQuery.prepare("DELETE FROM pin_settings WHERE pin_id = ?");
            deleteQuery.addBindValue(pinId);
            if (!deleteQuery.exec())
                throw QString(deleteQuery.lastError().text());

            // 4. 从 pin_list 删除 (主表)
            deleteQuery.prepare("DELETE FROM pin_list WHERE id = ?");
            deleteQuery.addBindValue(pinId);
            if (!deleteQuery.exec())
                throw QString(deleteQuery.lastError().text());
        }
    }
    catch (const QString &e)
    {
        success = false;
        errorMsg = e;
        db.rollback();
    }

    if (success)
    {
        db.commit();
        QMessageBox::information(this, tr("成功"), tr("已成功删除选中的管脚。"));
        // 刷新侧边导航栏和可能打开的视图
        refreshSidebarNavigator();
        // 如果有其他需要同步的视图，也在这里调用
    }
    else
    {
        QMessageBox::critical(this, tr("数据库错误"), tr("删除管脚时发生错误: %1").arg(errorMsg));
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

    // 获取当前选中的向量表
    int tabIndex = m_vectorTabWidget->currentIndex();
    if (tabIndex < 0 || !m_tabToTableId.contains(tabIndex))
    {
        qWarning() << funcName << " - 没有选中有效的向量表";
        return;
    }

    int tableId = m_tabToTableId[tabIndex];

    // 清除当前表的数据缓存
    VectorDataHandler::instance().clearTableDataCache(tableId);

    // 重新加载当前页面数据
    loadCurrentPage();

    // 刷新侧边导航栏
    refreshSidebarNavigator();

    // 显示刷新成功消息
    statusBar()->showMessage("向量表数据已刷新", 3000); // 显示3秒
    qDebug() << funcName << " - 向量表数据刷新完成";
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
        return;
    }
    int tableId = m_vectorTableSelector->currentData().toInt();
    QString tableName = m_vectorTableSelector->currentText();

    qDebug() << "MainWindow::setupVectorTablePins - 打开管脚设置对话框，表ID:" << tableId << "，表名:" << tableName;

    // 1. 获取更改前的完整列配置
    QList<Vector::ColumnInfo> oldColumns = VectorDataHandler::instance().getAllColumnInfo(tableId);

    // 2. 创建并显示管脚设置对话框
    VectorPinSettingsDialog dialog(tableId, tableName, this);

    if (dialog.exec() == QDialog::Accepted)
    {
        qDebug() << "MainWindow::setupVectorTablePins - 用户确认了管脚设置，调用数据迁移器。";
        // 3. 调用迁移器处理后续所有逻辑（比较、迁移、提示）
        DataMigrator::migrateDataIfNeeded(tableId, oldColumns, this);
    }
    else
    {
        qDebug() << "MainWindow::setupVectorTablePins - 用户取消了管脚设置。";
    }

    // 4. 无论成功、失败还是取消，都刷新UI以保证与数据库状态一致
    reloadAndRefreshVectorTable(tableId);
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
        // 不再显示第二个提示对话框，避免重复提示
    }
    else
    {
        qDebug() << "MainWindow::openPinSettingsDialog - 用户取消了管脚设置";
    }
}

// 跳转到指定行
void MainWindow::gotoLine()
{
    const QString funcName = "MainWindow::gotoLine";
    qDebug() << funcName << " - 开始跳转到指定行";

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

    // 获取当前表ID
    int tableId = m_vectorTableSelector->currentData().toInt();

    // 获取表的总行数
    int totalRowCount = VectorDataHandler::instance().getVectorTableRowCount(tableId);

    if (totalRowCount <= 0)
    {
        QMessageBox::warning(this, "警告", "当前向量表没有数据");
        return;
    }

    qDebug() << funcName << " - 当前表格总共有" << totalRowCount << "行数据";

    // 输入对话框获取行号
    bool ok;
    int targetLine = QInputDialog::getInt(this, "跳转到行",
                                          "请输入行号 (1-" + QString::number(totalRowCount) + "):",
                                          1, 1, totalRowCount, 1, &ok);

    if (!ok)
    {
        qDebug() << funcName << " - 用户取消了跳转";
        return;
    }

    qDebug() << funcName << " - 用户输入的行号:" << targetLine;

    // 计算目标行所在的页码
    int targetPage = (targetLine - 1) / m_pageSize;
    int rowInPage = (targetLine - 1) % m_pageSize;

    qDebug() << funcName << " - 目标行" << targetLine << "在第" << (targetPage + 1) << "页，页内行号:" << (rowInPage + 1);

    // 如果不是当前页，需要切换页面
    if (targetPage != m_currentPage)
    {
        m_currentPage = targetPage;
        loadCurrentPage();
    }

    // 在页面中选中行（使用页内索引）
    if (rowInPage >= 0 && rowInPage < m_vectorTableWidget->rowCount())
    {
        // 清除当前选择
        m_vectorTableWidget->clearSelection();

        // 选中目标行
        m_vectorTableWidget->selectRow(rowInPage);

        // 滚动到目标行
        m_vectorTableWidget->scrollTo(m_vectorTableWidget->model()->index(rowInPage, 0), QAbstractItemView::PositionAtCenter);

        qDebug() << funcName << " - 已跳转到第" << targetLine << "行 (第" << (targetPage + 1) << "页，页内行:" << (rowInPage + 1) << ")";
        statusBar()->showMessage(tr("已跳转到第 %1 行（第 %2 页）").arg(targetLine).arg(targetPage + 1));
    }
    else
    {
        QMessageBox::warning(this, "错误", "无效的行号");
        qDebug() << funcName << " - 无效的行号:" << targetLine << ", 页内索引:" << rowInPage;
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

    // 恢复侧边栏状态
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
}

// 处理表格单元格变更
void MainWindow::onTableCellChanged(int row, int column)
{
    qDebug() << "MainWindow::onTableCellChanged - 单元格变更: 行=" << row << ", 列=" << column;
    onTableRowModified(row);
}

// 处理表格行修改
void MainWindow::onTableRowModified(int row)
{
    // 获取当前选中的向量表ID
    int tabIndex = m_vectorTabWidget->currentIndex();
    if (tabIndex < 0 || !m_tabToTableId.contains(tabIndex))
    {
        qWarning() << "MainWindow::onTableRowModified - 没有选中有效的向量表";
        return;
    }

    int tableId = m_tabToTableId[tabIndex];

    // 计算实际数据库中的行索引（考虑分页）
    int actualRowIndex = m_currentPage * m_pageSize + row;
    qDebug() << "MainWindow::onTableRowModified - 标记表ID:" << tableId << "的行:" << actualRowIndex << "为已修改";

    // 标记行为已修改
    VectorDataHandler::instance().markRowAsModified(tableId, actualRowIndex);
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

// 刷新侧边导航栏数据
void MainWindow::refreshSidebarNavigator()
{
    if (!m_sidebarTree || m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        return;
    }

    // 临时保存选中状态
    QMap<QString, QString> selectedItems;
    for (int i = 0; i < m_sidebarTree->topLevelItemCount(); i++)
    {
        QTreeWidgetItem *root = m_sidebarTree->topLevelItem(i);
        QString rootType = root->data(0, Qt::UserRole).toString();

        for (int j = 0; j < root->childCount(); j++)
        {
            QTreeWidgetItem *child = root->child(j);
            if (child->isSelected())
            {
                selectedItems[rootType] = child->data(0, Qt::UserRole).toString();
            }
        }

        // 清空子节点，准备重新加载
        while (root->childCount() > 0)
        {
            delete root->takeChild(0);
        }
    }

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();

    // 获取管脚列表
    QTreeWidgetItem *pinRoot = nullptr;
    for (int i = 0; i < m_sidebarTree->topLevelItemCount(); i++)
    {
        if (m_sidebarTree->topLevelItem(i)->data(0, Qt::UserRole).toString() == "pins")
        {
            pinRoot = m_sidebarTree->topLevelItem(i);
            break;
        }
    }

    if (pinRoot)
    {
        QSqlQuery pinQuery(db);
        // 修改查询语句，获取所有管脚，不限于被使用的管脚
        pinQuery.exec("SELECT id, pin_name FROM pin_list ORDER BY pin_name");
        while (pinQuery.next())
        {
            int pinId = pinQuery.value(0).toInt();
            QString pinName = pinQuery.value(1).toString();

            QTreeWidgetItem *pinItem = new QTreeWidgetItem(pinRoot);
            pinItem->setText(0, pinName);
            pinItem->setData(0, Qt::UserRole, QString::number(pinId));

            // 恢复选中状态
            if (selectedItems.contains("pins") && selectedItems["pins"] == QString::number(pinId))
            {
                pinItem->setSelected(true);
            }
        }
    }

    // 获取TimeSet列表
    QTreeWidgetItem *timeSetRoot = nullptr;
    for (int i = 0; i < m_sidebarTree->topLevelItemCount(); i++)
    {
        if (m_sidebarTree->topLevelItem(i)->data(0, Qt::UserRole).toString() == "timesets")
        {
            timeSetRoot = m_sidebarTree->topLevelItem(i);
            break;
        }
    }

    if (timeSetRoot)
    {
        QSqlQuery timeSetQuery(db);
        // 修改查询语句，获取所有实际使用的TimeSet
        timeSetQuery.exec("SELECT id, timeset_name FROM timeset_list ORDER BY timeset_name");
        while (timeSetQuery.next())
        {
            int timeSetId = timeSetQuery.value(0).toInt();
            QString timeSetName = timeSetQuery.value(1).toString();

            QTreeWidgetItem *timeSetItem = new QTreeWidgetItem(timeSetRoot);
            timeSetItem->setText(0, timeSetName);
            timeSetItem->setData(0, Qt::UserRole, QString::number(timeSetId));

            // 恢复选中状态
            if (selectedItems.contains("timesets") && selectedItems["timesets"] == QString::number(timeSetId))
            {
                timeSetItem->setSelected(true);
            }
        }
    }

    // 获取向量表列表
    QTreeWidgetItem *vectorTableRoot = nullptr;
    for (int i = 0; i < m_sidebarTree->topLevelItemCount(); i++)
    {
        if (m_sidebarTree->topLevelItem(i)->data(0, Qt::UserRole).toString() == "vectortables")
        {
            vectorTableRoot = m_sidebarTree->topLevelItem(i);
            break;
        }
    }

    if (vectorTableRoot)
    {
        QSqlQuery tableQuery(db);
        tableQuery.exec("SELECT id, table_name FROM vector_tables ORDER BY table_name");
        while (tableQuery.next())
        {
            int tableId = tableQuery.value(0).toInt();
            QString tableName = tableQuery.value(1).toString();

            QTreeWidgetItem *tableItem = new QTreeWidgetItem(vectorTableRoot);
            tableItem->setText(0, tableName);
            tableItem->setData(0, Qt::UserRole, QString::number(tableId));

            // 恢复选中状态
            if (selectedItems.contains("vectortables") && selectedItems["vectortables"] == QString::number(tableId))
            {
                tableItem->setSelected(true);
            }
        }
    }

    // 获取标签列表（从所有向量表中获取唯一的Label值）
    QTreeWidgetItem *labelRoot = nullptr;
    for (int i = 0; i < m_sidebarTree->topLevelItemCount(); i++)
    {
        if (m_sidebarTree->topLevelItem(i)->data(0, Qt::UserRole).toString() == "labels")
        {
            labelRoot = m_sidebarTree->topLevelItem(i);
            break;
        }
    }

    if (labelRoot)
    {
        // 获取所有向量表
        QSet<QString> uniqueLabels;
        QSqlQuery tablesQuery(db);
        tablesQuery.exec("SELECT id FROM vector_tables");

        while (tablesQuery.next())
        {
            int tableId = tablesQuery.value(0).toInt();

            // 从每个表获取Label列信息和二进制文件
            QString binFileName;
            QList<Vector::ColumnInfo> columns;
            int schemaVersion = 0;
            int rowCount = 0;

            if (loadVectorTableMeta(tableId, binFileName, columns, schemaVersion, rowCount))
            {
                // 查找Label列的索引
                int labelColumnIndex = -1;
                for (int i = 0; i < columns.size(); i++)
                {
                    if (columns[i].name.toLower() == "label")
                    {
                        labelColumnIndex = i;
                        break;
                    }
                }

                if (labelColumnIndex >= 0)
                {
                    // 从二进制文件中读取Label数据
                    QString errorMsg;
                    QString binFilePath = "";

                    // 使用SQL查询获取二进制文件路径，而不是调用私有方法
                    QSqlQuery getBinFileQuery(db);
                    getBinFileQuery.prepare("SELECT binary_file FROM vector_tables WHERE id = ?");
                    getBinFileQuery.addBindValue(tableId);

                    if (getBinFileQuery.exec() && getBinFileQuery.next())
                    {
                        QString binFileName = getBinFileQuery.value(0).toString();
                        // 获取数据库文件路径
                        QFileInfo dbFileInfo(db.databaseName());
                        QString dbDir = dbFileInfo.absolutePath();
                        QString dbName = dbFileInfo.baseName();
                        // 构造二进制文件目录路径
                        QString binDirName = dbName + "_vbindata";
                        QDir binDir(QDir(dbDir).absoluteFilePath(binDirName));
                        // 构造完整的二进制文件路径
                        binFilePath = binDir.absoluteFilePath(binFileName);
                    }

                    if (!binFilePath.isEmpty())
                    {
                        QList<Vector::RowData> rowData;
                        if (Persistence::BinaryFileHelper::readAllRowsFromBinary(binFilePath, columns, schemaVersion, rowData))
                        {
                            for (const auto &row : rowData)
                            {
                                if (labelColumnIndex < row.size())
                                {
                                    QString label = row[labelColumnIndex].toString();
                                    if (!label.isEmpty())
                                    {
                                        uniqueLabels.insert(label);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // 添加唯一标签
        QList<QString> sortedLabels;
        // 将QSet中的元素转换为QList - 使用正确的方式从QSet获取元素
        for (const QString &label : uniqueLabels)
        {
            sortedLabels.append(label);
        }
        std::sort(sortedLabels.begin(), sortedLabels.end());

        for (const QString &label : sortedLabels)
        {
            QTreeWidgetItem *labelItem = new QTreeWidgetItem(labelRoot);
            labelItem->setText(0, label);
            labelItem->setData(0, Qt::UserRole, label);

            // 恢复选中状态
            if (selectedItems.contains("labels") && selectedItems["labels"] == label)
            {
                labelItem->setSelected(true);
            }
        }
    }

    // 展开所有根节点
    for (int i = 0; i < m_sidebarTree->topLevelItemCount(); i++)
    {
        m_sidebarTree->topLevelItem(i)->setExpanded(true);
    }
}

// 侧边栏项目点击事件处理
void MainWindow::onSidebarItemClicked(QTreeWidgetItem *item, int column)
{
    if (!item)
        return;

    if (item->parent() == nullptr) // 根节点点击
    {
        QString rootType = item->data(0, Qt::UserRole).toString();

        if (rootType == "pins")
        {
            // 不打开设置对话框，只展开或收起节点
            item->setExpanded(!item->isExpanded());
            // 原来打开设置对话框的代码注释掉
            // openPinSettingsDialog();
        }
        else if (rootType == "timesets")
        {
            // 不打开设置对话框，只展开或收起节点
            item->setExpanded(!item->isExpanded());
            // 原来打开设置对话框的代码注释掉
            // openTimeSetSettingsDialog();
        }
        // 向量表和标签根节点处理
        else
        {
            item->setExpanded(!item->isExpanded());
        }
        return;
    }

    // 子节点点击处理
    QTreeWidgetItem *parentItem = item->parent();
    QString itemType = parentItem->data(0, Qt::UserRole).toString();
    QString itemValue = item->data(0, Qt::UserRole).toString();

    if (itemType == "pins")
    {
        onPinItemClicked(item, column);
    }
    else if (itemType == "timesets")
    {
        onTimeSetItemClicked(item, column);
    }
    else if (itemType == "vectortables")
    {
        onVectorTableItemClicked(item, column);
    }
    else if (itemType == "labels")
    {
        onLabelItemClicked(item, column);
    }
}

// 管脚项目点击事件
void MainWindow::onPinItemClicked(QTreeWidgetItem *item, int column)
{
    if (!item)
        return;

    int pinId = item->data(0, Qt::UserRole).toString().toInt();

    // 查找当前表中与此管脚相关的所有列
    if (m_vectorTableWidget && m_vectorTableWidget->columnCount() > 0)
    {
        bool found = false;
        QString pinName = item->text(0);

        // 遍历所有列，寻找与此管脚名匹配的列
        for (int col = 0; col < m_vectorTableWidget->columnCount(); col++)
        {
            QTableWidgetItem *headerItem = m_vectorTableWidget->horizontalHeaderItem(col);
            // 修改匹配逻辑：检查列头是否以管脚名开头，而不是精确匹配
            if (headerItem && headerItem->text().startsWith(pinName))
            {
                // 找到了匹配的列，高亮显示该列
                m_vectorTableWidget->clearSelection();
                m_vectorTableWidget->selectColumn(col);

                // 滚动到该列
                m_vectorTableWidget->scrollTo(m_vectorTableWidget->model()->index(0, col));

                found = true;
                break;
            }
        }

        if (!found)
        {
            QMessageBox::information(this, "提示", QString("当前表中没有找到管脚 '%1' 对应的列").arg(pinName));
        }
    }
    else
    {
        QMessageBox::information(this, "提示", "没有打开的向量表，无法定位管脚");
    }
}

// TimeSet项目点击事件
void MainWindow::onTimeSetItemClicked(QTreeWidgetItem *item, int column)
{
    if (!item)
        return;

    int timeSetId = item->data(0, Qt::UserRole).toString().toInt();
    QString timeSetName = item->text(0);

    // 在当前表中查找并高亮使用此TimeSet的所有行
    if (m_vectorTableWidget && m_vectorTableWidget->rowCount() > 0)
    {
        bool found = false;
        m_vectorTableWidget->setSelectionMode(QAbstractItemView::MultiSelection);

        // 假设TimeSet列是第三列（索引2），您可能需要根据实际情况进行调整
        int timeSetColumn = -1;
        for (int col = 0; col < m_vectorTableWidget->columnCount(); col++)
        {
            QTableWidgetItem *headerItem = m_vectorTableWidget->horizontalHeaderItem(col);
            if (headerItem && headerItem->text().toLower() == "timeset")
            {
                timeSetColumn = col;
                break;
            }
        }

        if (timeSetColumn >= 0)
        {
            // 遍历所有行，查找使用此TimeSet的行
            for (int row = 0; row < m_vectorTableWidget->rowCount(); row++)
            {
                QTableWidgetItem *item = m_vectorTableWidget->item(row, timeSetColumn);
                if (item && item->text() == timeSetName)
                {
                    m_vectorTableWidget->selectRow(row);
                    if (!found)
                    {
                        // 滚动到第一个找到的行
                        m_vectorTableWidget->scrollTo(m_vectorTableWidget->model()->index(row, 0));
                        found = true;
                    }
                }
            }

            if (!found)
            {
                QMessageBox::information(this, "提示", QString("当前表中没有使用TimeSet '%1' 的行").arg(timeSetName));
            }
        }
        else
        {
            QMessageBox::information(this, "提示", "当前表中没有找到TimeSet列");
        }

        // 恢复为ExtendedSelection选择模式
        m_vectorTableWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    }
    else
    {
        QMessageBox::information(this, "提示", "没有打开的向量表，无法定位TimeSet");
    }
}

// 向量表项目点击事件
void MainWindow::onVectorTableItemClicked(QTreeWidgetItem *item, int column)
{
    if (!item)
        return;

    int tableId = item->data(0, Qt::UserRole).toString().toInt();

    // 在combobox中选中对应的向量表
    for (int i = 0; i < m_vectorTableSelector->count(); i++)
    {
        if (m_vectorTableSelector->itemData(i).toInt() == tableId)
        {
            m_vectorTableSelector->setCurrentIndex(i);
            break;
        }
    }
}

// 标签项目点击事件
void MainWindow::onLabelItemClicked(QTreeWidgetItem *item, int column)
{
    if (!item)
        return;

    QString labelText = item->data(0, Qt::UserRole).toString();

    // 在当前表中查找并跳转到对应标签的行
    if (m_vectorTableWidget && m_vectorTableWidget->rowCount() > 0)
    {
        for (int row = 0; row < m_vectorTableWidget->rowCount(); row++)
        {
            // 假设第一列是Label列
            QTableWidgetItem *labelItem = m_vectorTableWidget->item(row, 0);
            if (labelItem && labelItem->text() == labelText)
            {
                m_vectorTableWidget->selectRow(row);
                m_vectorTableWidget->scrollToItem(labelItem);
                break;
            }
        }
    }
}

// 辅助方法：从数据库加载向量表元数据
bool MainWindow::loadVectorTableMeta(int tableId, QString &binFileName, QList<Vector::ColumnInfo> &columns, int &schemaVersion, int &rowCount)
{
    const QString funcName = "MainWindow::loadVectorTableMeta";
    qDebug() << funcName << " - 查询表ID:" << tableId;
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << funcName << " - 数据库未打开";
        return false;
    }
    // 1. 查询主记录表
    QSqlQuery metaQuery(db);
    metaQuery.prepare("SELECT binary_data_filename, data_schema_version, row_count FROM VectorTableMasterRecord WHERE id = ?");
    metaQuery.addBindValue(tableId);
    if (!metaQuery.exec() || !metaQuery.next())
    {
        qWarning() << funcName << " - 查询主记录失败, 表ID:" << tableId << ", 错误:" << metaQuery.lastError().text();
        return false;
    }
    binFileName = metaQuery.value(0).toString();
    schemaVersion = metaQuery.value(1).toInt();
    rowCount = metaQuery.value(2).toInt();
    qDebug() << funcName << " - 文件名:" << binFileName << ", schemaVersion:" << schemaVersion << ", rowCount:" << rowCount;

    // 2. 查询列结构 - 只加载IsVisible=1的列
    QSqlQuery colQuery(db);
    colQuery.prepare("SELECT id, column_name, column_order, column_type, data_properties, IsVisible FROM VectorTableColumnConfiguration WHERE master_record_id = ? AND IsVisible = 1 ORDER BY column_order");
    colQuery.addBindValue(tableId);
    if (!colQuery.exec())
    {
        qWarning() << funcName << " - 查询列结构失败, 错误:" << colQuery.lastError().text();
        return false;
    }

    columns.clear();
    while (colQuery.next())
    {
        Vector::ColumnInfo col;
        col.id = colQuery.value(0).toInt();
        col.vector_table_id = tableId;
        col.name = colQuery.value(1).toString();
        col.order = colQuery.value(2).toInt();
        col.original_type_str = colQuery.value(3).toString();
        col.type = Vector::columnDataTypeFromString(col.original_type_str);
        col.is_visible = colQuery.value(5).toBool();

        QString propStr = colQuery.value(4).toString();
        if (!propStr.isEmpty())
        {
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(propStr.toUtf8(), &err);
            qDebug().nospace() << funcName << " - JSON Parsing Details for Column: '" << col.name
                               << "', Input: '" << propStr
                               << "', ErrorCode: " << err.error
                               << " (ErrorStr: " << err.errorString()
                               << "), IsObject: " << doc.isObject();

            if (err.error == QJsonParseError::NoError && doc.isObject())
            {
                col.data_properties = doc.object();
            }
            else
            {
                qWarning().nospace() << funcName << " - 列属性JSON解析判定为失败 (条件分支), 列: '" << col.name
                                     << "', Input: '" << propStr
                                     << "', ErrorCode: " << err.error
                                     << " (ErrorStr: " << err.errorString()
                                     << "), IsObject: " << doc.isObject();
            }
        }
        columns.append(col);
    }
    return true;
}

// Add the new private helper method
void MainWindow::updateBinaryHeaderColumnCount(int tableId)
{
    const QString funcName = "MainWindow::updateBinaryHeaderColumnCount";
    qDebug() << funcName << "- Attempting to update binary header column count for table ID:" << tableId;

    QString errorMessage;
    QList<Vector::ColumnInfo> columns;
    int dbSchemaVersion = -1;
    QString binaryFileNameBase; // Base name like "table_1_data.vbindata"

    DatabaseManager *dbManager = DatabaseManager::instance(); // Corrected: Pointer type
    if (!dbManager->isDatabaseConnected())
    { // Corrected: ->isDatabaseConnected()
        qWarning() << funcName << "- Database not open.";
        return;
    }
    QSqlDatabase db = dbManager->database();

    // 1. Get master record info (schema version, binary file name)
    QSqlQuery masterQuery(db);
    // Corrected: VectorTableMasterRecord table name, and use 'id' for tableId
    masterQuery.prepare("SELECT data_schema_version, binary_data_filename FROM VectorTableMasterRecord WHERE id = :tableId");
    masterQuery.bindValue(":tableId", tableId);

    if (!masterQuery.exec())
    {
        qWarning() << funcName << "- Failed to execute query for VectorTableMasterRecord:" << masterQuery.lastError().text();
        return;
    }

    if (masterQuery.next())
    {
        dbSchemaVersion = masterQuery.value("data_schema_version").toInt();        // Corrected column name
        binaryFileNameBase = masterQuery.value("binary_data_filename").toString(); // Corrected column name
    }
    else
    {
        qWarning() << funcName << "- No VectorTableMasterRecord found for table ID:" << tableId;
        return;
    }

    if (binaryFileNameBase.isEmpty())
    {
        qWarning() << funcName << "- Binary file name is empty in master record for table ID:" << tableId;
        return;
    }

    // 2. Get column configurations from DB to count actual columns
    QSqlQuery columnQuery(db);
    // Query to get the actual number of columns configured for this table in the database
    // This includes standard columns and any pin-specific columns
    // Corrected: VectorTableColumnConfiguration table name, master_record_id for tableId relation
    QString columnSql = QString(
        "SELECT COUNT(*) FROM VectorTableColumnConfiguration WHERE master_record_id = ?"); // 使用位置占位符

    if (!columnQuery.prepare(columnSql))
    {
        qWarning() << funcName << "- CRITICAL_WARNING: Failed to PREPARE query for actual column count. SQL:" << columnSql
                   << ". Error:" << columnQuery.lastError().text();
        return;
    }
    columnQuery.addBindValue(tableId); // 使用 addBindValue

    int actualColumnCount = 0;
    if (columnQuery.exec())
    {
        if (columnQuery.next())
        {
            actualColumnCount = columnQuery.value(0).toInt();
        }
        // No 'else' here, if query returns no rows, actualColumnCount remains 0, which is handled below.
    }
    else
    {
        qWarning() << funcName << "- CRITICAL_WARNING: Failed to EXECUTE query for actual column count. TableId:" << tableId
                   << ". SQL:" << columnSql << ". Error:" << columnQuery.lastError().text()
                   << "(Reason: DB query for actual column count failed after successful prepare)";
        return;
    }

    qDebug() << funcName << "- Actual column count from DB for tableId" << tableId << "is" << actualColumnCount;

    if (actualColumnCount == 0 && tableId > 0)
    {
        qWarning() << funcName << "- No columns found in DB configuration for table ID:" << tableId << ". Header update may not be meaningful (or it's a new table). Continuing.";
    }

    // 3. Construct full binary file path
    // QString projectDbPath = dbManager->getCurrentDatabasePath(); // Incorrect method
    QString projectDbPath = m_currentDbPath; // Use MainWindow's member
    QString projBinDataDir = Utils::PathUtils::getProjectBinaryDataDirectory(projectDbPath);
    QString binFilePath = projBinDataDir + QDir::separator() + binaryFileNameBase;

    QFile file(binFilePath);
    if (!file.exists())
    {
        qWarning() << funcName << "- Binary file does not exist, cannot update header:" << binFilePath;
        return;
    }

    if (!file.open(QIODevice::ReadWrite))
    {
        qWarning() << funcName << "- Failed to open binary file for ReadWrite:" << binFilePath << file.errorString();
        return;
    }

    BinaryFileHeader header;
    bool existingHeaderRead = Persistence::BinaryFileHelper::readBinaryHeader(&file, header);

    if (existingHeaderRead)
    {
        // Corrected member access to use column_count_in_file
        if (header.column_count_in_file == actualColumnCount && header.data_schema_version == dbSchemaVersion)
        {
            qDebug() << funcName << "- Header column count (" << header.column_count_in_file
                     << ") and schema version (" << header.data_schema_version
                     << ") already match DB. No update needed for table" << tableId;
            file.close();
            return;
        }
        // Preserve creation time and row count from existing header
        header.column_count_in_file = actualColumnCount; // Corrected to column_count_in_file
        header.data_schema_version = dbSchemaVersion;    // Update schema version from DB
        header.timestamp_updated = QDateTime::currentSecsSinceEpoch();
    }
    else
    {
        qWarning() << funcName << "- Failed to read existing header from" << binFilePath << ". This is unexpected if addNewVectorTable created it. Re-initializing header for update.";
        header.magic_number = Persistence::VEC_BINDATA_MAGIC;
        header.file_format_version = Persistence::CURRENT_FILE_FORMAT_VERSION;
        header.data_schema_version = dbSchemaVersion;
        header.row_count_in_file = 0;
        header.column_count_in_file = actualColumnCount; // Corrected to column_count_in_file
        header.timestamp_created = QDateTime::currentSecsSinceEpoch();
        header.timestamp_updated = QDateTime::currentSecsSinceEpoch();
        header.compression_type = 0;
        // Removed memset calls for header.reserved and header.future_use as they are not members
    }

    file.seek(0);
    if (Persistence::BinaryFileHelper::writeBinaryHeader(&file, header))
    {
        qInfo() << funcName << "- Successfully updated binary file header for table" << tableId
                << ". Path:" << binFilePath
                << ". New ColumnCount:" << actualColumnCount
                << ", SchemaVersion:" << dbSchemaVersion;
    }
    else
    {
        qWarning() << funcName << "- Failed to write updated binary file header for table" << tableId
                   << ". Path:" << binFilePath;
    }
    file.close();

    // Corrected cache invalidation method name
    VectorDataHandler::instance().clearTableDataCache(tableId);
    // Clearing data cache is often sufficient. If specific metadata cache for columns/schema
    // exists and needs explicit invalidation, that would require a specific method in VectorDataHandler.
    // For now, assuming clearTableDataCache() and subsequent reloads handle it.
}

// Add implementation for reloadAndRefreshVectorTable
void MainWindow::reloadAndRefreshVectorTable(int tableId)
{
    const QString funcName = "MainWindow::reloadAndRefreshVectorTable";
    qDebug() << funcName << "- Reloading and refreshing UI for table ID:" << tableId;

    // 首先清除表格数据缓存，确保获取最新数据
    VectorDataHandler::instance().clearTableDataCache(tableId);

    // 1. Ensure the table is selected in the ComboBox and TabWidget
    int comboBoxIndex = m_vectorTableSelector->findData(tableId);
    if (comboBoxIndex != -1)
    {
        if (m_vectorTableSelector->currentIndex() != comboBoxIndex)
        {
            m_vectorTableSelector->setCurrentIndex(comboBoxIndex); // This should trigger onVectorTableSelectionChanged
        }
        else
        {
            // If it's already selected, force a refresh of its data
            onVectorTableSelectionChanged(comboBoxIndex);
        }
    }
    else
    {
        qWarning() << funcName << "- Table ID" << tableId << "not found in selector. Cannot refresh.";
        // Fallback: reload all tables if the specific one isn't found (might be a new table not yet in UI)
        loadVectorTable();
    }

    // 2. Refresh the sidebar (in case table names or other project components changed)
    refreshSidebarNavigator();
}

// Add this new function implementation
QList<Vector::ColumnInfo> MainWindow::getCurrentColumnConfiguration(int tableId)
{
    const QString funcName = "MainWindow::getCurrentColumnConfiguration";
    QList<Vector::ColumnInfo> columns;
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << funcName << "- 数据库未打开";
        return columns;
    }

    QSqlQuery colQuery(db);
    // 获取所有列，无论是否可见，因为迁移需要处理所有物理列
    colQuery.prepare("SELECT id, column_name, column_order, column_type, data_properties, IsVisible "
                     "FROM VectorTableColumnConfiguration WHERE master_record_id = ? ORDER BY column_order");
    colQuery.addBindValue(tableId);

    if (!colQuery.exec())
    {
        qWarning() << funcName << "- 查询列结构失败, 表ID:" << tableId << ", 错误:" << colQuery.lastError().text();
        return columns;
    }

    while (colQuery.next())
    {
        Vector::ColumnInfo col;
        col.id = colQuery.value(0).toInt(); // This is the id from VectorTableColumnConfiguration table
        col.vector_table_id = tableId;      // The master_record_id it belongs to
        col.name = colQuery.value(1).toString();
        col.order = colQuery.value(2).toInt();
        col.original_type_str = colQuery.value(3).toString();               // Store the original string
        col.type = Vector::columnDataTypeFromString(col.original_type_str); // Convert to enum

        QString propStr = colQuery.value(4).toString();
        if (!propStr.isEmpty())
        {
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(propStr.toUtf8(), &err);
            if (err.error == QJsonParseError::NoError && doc.isObject())
            {
                col.data_properties = doc.object();
            }
            else
            {
                qWarning() << funcName << " - 解析列属性JSON失败 for column " << col.name << ", error: " << err.errorString();
            }
        }
        col.is_visible = colQuery.value(5).toBool();
        columns.append(col);
    }
    qDebug() << funcName << "- 为表ID:" << tableId << "获取了" << columns.size() << "列配置。";
    return columns;
}

// Add this new function implementation
bool MainWindow::areColumnConfigurationsDifferentSimplified(const QList<Vector::ColumnInfo> &config1, const QList<Vector::ColumnInfo> &config2)
{
    if (config1.size() != config2.size())
    {
        qDebug() << "areColumnConfigurationsDifferentSimplified: Sizes differ - config1:" << config1.size() << "config2:" << config2.size();
        return true;
    }

    QSet<QString> names1, names2;
    for (const auto &col : config1)
    {
        names1.insert(col.name);
    }
    for (const auto &col : config2)
    {
        names2.insert(col.name);
    }

    if (names1 != names2)
    {
        qDebug() << "areColumnConfigurationsDifferentSimplified: Names differ.";
        qDebug() << "Config1 names:" << names1;
        qDebug() << "Config2 names:" << names2;
        return true;
    }

    // Optional: Check for type changes for same-named columns if necessary for your definition of "different"
    // For now, just size and name set equality is enough for "simplified"

    return false; // Configurations are considered the same by this simplified check
}

// Add this new function implementation
QList<Vector::RowData> MainWindow::adaptRowDataToNewColumns(const QList<Vector::RowData> &oldRowDataList,
                                                            const QList<Vector::ColumnInfo> &oldColumns,
                                                            const QList<Vector::ColumnInfo> &newColumns)
{
    const QString funcName = "MainWindow::adaptRowDataToNewColumns";
    QList<Vector::RowData> newRowDataList;
    newRowDataList.reserve(oldRowDataList.size());

    // Create a map of old column names to their index for efficient lookup
    QMap<QString, int> oldColumnNameToIndex;
    for (int i = 0; i < oldColumns.size(); ++i)
    {
        oldColumnNameToIndex[oldColumns[i].name] = i;
    }

    // Create a map of old column names to their type for type-aware default values (optional enhancement)
    QMap<QString, Vector::ColumnDataType> oldColumnNameToType;
    for (const auto &col : oldColumns)
    {
        oldColumnNameToType[col.name] = col.type;
    }

    for (const Vector::RowData &oldRow : oldRowDataList)
    {
        // Vector::RowData newRow(newColumns.size()); // Initialize newRow with newColumns.size() default QVariants - Incorrect
        Vector::RowData newRow;
        newRow.reserve(newColumns.size()); // Good practice to reserve if size is known

        for (int colIdx = 0; colIdx < newColumns.size(); ++colIdx)
        { // Changed loop variable name for clarity
            const Vector::ColumnInfo &newCol = newColumns[colIdx];
            QVariant cellData; // Temporary variable to hold the data for the current cell

            if (oldColumnNameToIndex.contains(newCol.name))
            {
                // Column exists in old data, copy it
                int oldIndex = oldColumnNameToIndex[newCol.name];
                if (oldIndex >= 0 && oldIndex < oldRow.size())
                { // Bounds check
                    cellData = oldRow[oldIndex];
                }
                else
                {
                    qWarning() << funcName << "- Old column index out of bounds for column:" << newCol.name << "oldIndex:" << oldIndex << "oldRow.size:" << oldRow.size();
                    if (newCol.type == Vector::ColumnDataType::PIN_STATE_ID)
                    {
                        cellData = "X";
                    }
                    else
                    {
                        cellData = QVariant();
                    }
                }
            }
            else
            {
                // Column is new, fill with default value
                if (newCol.type == Vector::ColumnDataType::PIN_STATE_ID)
                {
                    cellData = "X";
                }
                else if (newCol.type == Vector::ColumnDataType::TEXT)
                {
                    cellData = QString();
                }
                else if (newCol.type == Vector::ColumnDataType::INTEGER ||
                         newCol.type == Vector::ColumnDataType::INSTRUCTION_ID ||
                         newCol.type == Vector::ColumnDataType::TIMESET_ID)
                {
                    cellData = 0;
                }
                else if (newCol.type == Vector::ColumnDataType::REAL)
                {
                    cellData = 0.0;
                }
                else if (newCol.type == Vector::ColumnDataType::BOOLEAN)
                {
                    cellData = false;
                }
                else
                {
                    qWarning() << funcName << "- Unhandled new column type for default value:" << newCol.name << "type:" << newCol.original_type_str;
                    cellData = QVariant();
                }
            }
            newRow.append(cellData); // Append the determined cell data
        }
        newRowDataList.append(newRow);
    }
    qDebug() << funcName << "- Data adaptation complete. Processed" << oldRowDataList.size() << "rows, produced" << newRowDataList.size() << "rows for new structure.";
    return newRowDataList;
}

void MainWindow::updateMenuState()
{
    bool projectOpen = !m_currentDbPath.isEmpty();
    m_newProjectAction->setEnabled(!projectOpen);
    m_openProjectAction->setEnabled(!projectOpen);
    m_closeProjectAction->setEnabled(projectOpen);
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
