#include "mainwindow.h"
#include "databasemanager.h"
#include "databaseviewdialog.h"
#include "pinlistdialog.h"
#include "timesetdialog.h"

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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUI();
    setupMenu();

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
            timeSetAdded = showTimeSetDialog();
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

    // 创建并显示数据库视图对话框
    DatabaseViewDialog *dialog = new DatabaseViewDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose); // 关闭时自动删除
    dialog->exec();
}

bool MainWindow::showAddPinsDialog()
{
    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        return false;
    }

    // 创建并显示管脚添加对话框
    PinListDialog dialog(this);

    if (dialog.exec() == QDialog::Accepted)
    {
        // 获取用户设置的管脚列表
        QList<QString> pinNames = dialog.getPinNames();

        // 添加到数据库
        if (!pinNames.isEmpty())
        {
            return addPinsToDatabase(pinNames);
        }
    }

    return false;
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
        statusBar()->showMessage(tr("已成功添加 %1 个管脚").arg(pinNames.count()));
    }
    else
    {
        db.rollback();
        statusBar()->showMessage(tr("添加管脚失败"));
    }

    return success;
}

bool MainWindow::showTimeSetDialog()
{
    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        return false;
    }

    // 创建并显示TimeSet对话框
    TimeSetDialog dialog(this);

    if (dialog.exec() == QDialog::Accepted)
    {
        statusBar()->showMessage(tr("TimeSet已成功添加"));
        return true;
    }

    return false;
}

void MainWindow::setupVectorTableUI()
{
    // 创建向量表容器
    m_vectorTableContainer = new QWidget(m_centralWidget);
    QVBoxLayout *containerLayout = new QVBoxLayout(m_vectorTableContainer);

    // 顶部控制栏
    QHBoxLayout *controlLayout = new QHBoxLayout();
    QLabel *tableLabel = new QLabel("向量表:", m_vectorTableContainer);
    m_vectorTableSelector = new QComboBox(m_vectorTableContainer);
    m_vectorTableSelector->setMinimumWidth(200);

    QPushButton *refreshButton = new QPushButton("刷新", m_vectorTableContainer);

    controlLayout->addWidget(tableLabel);
    controlLayout->addWidget(m_vectorTableSelector);
    controlLayout->addStretch();
    controlLayout->addWidget(refreshButton);

    // 向量表
    m_vectorTableWidget = new QTableWidget(m_vectorTableContainer);
    m_vectorTableWidget->setAlternatingRowColors(true);
    m_vectorTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_vectorTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_vectorTableWidget->horizontalHeader()->setStretchLastSection(true);
    m_vectorTableWidget->verticalHeader()->setVisible(true);

    // 将控件添加到容器布局
    containerLayout->addLayout(controlLayout);
    containerLayout->addWidget(m_vectorTableWidget);

    // 将容器添加到主布局
    QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout *>(m_centralWidget->layout());
    if (mainLayout)
    {
        mainLayout->addWidget(m_vectorTableContainer);
    }

    // 连接信号槽
    connect(m_vectorTableSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onVectorTableSelectionChanged);
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::loadVectorTable);
}

void MainWindow::loadVectorTable()
{
    // 清空当前选择框
    m_vectorTableSelector->clear();

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        statusBar()->showMessage("错误：数据库未打开");
        return;
    }

    // 查询所有向量表
    QSqlQuery tableQuery(db);
    if (tableQuery.exec("SELECT id, table_name FROM vector_tables ORDER BY table_name"))
    {
        while (tableQuery.next())
        {
            int tableId = tableQuery.value(0).toInt();
            QString tableName = tableQuery.value(1).toString();
            m_vectorTableSelector->addItem(tableName, tableId);
        }
    }

    // 如果有向量表，显示向量表窗口，否则显示欢迎窗口
    if (m_vectorTableSelector->count() > 0)
    {
        m_welcomeWidget->setVisible(false);
        m_vectorTableContainer->setVisible(true);

        // 默认选择第一个表
        m_vectorTableSelector->setCurrentIndex(0);
    }
    else
    {
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

    // 清空表格
    m_vectorTableWidget->clear();
    m_vectorTableWidget->setRowCount(0);
    m_vectorTableWidget->setColumnCount(0);

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        statusBar()->showMessage("错误：数据库未打开");
        return;
    }

    // 1. 获取表的管脚信息以设置表头
    QSqlQuery pinsQuery(db);
    pinsQuery.prepare("SELECT vtp.id, pl.pin_name, vtp.pin_channel_count, topt.type_name "
                      "FROM vector_table_pins vtp "
                      "JOIN pin_list pl ON vtp.pin_id = pl.id "
                      "JOIN type_options topt ON vtp.pin_type = topt.id "
                      "WHERE vtp.table_id = ? "
                      "ORDER BY pl.pin_name");
    pinsQuery.addBindValue(tableId);

    QList<int> pinIds;
    QMap<int, int> pinIdToColumn; // 管脚ID到列索引的映射

    int columnIndex = 0;

    // 固定列: 标签，指令，TimeSet，Capture，Ext，Comment
    m_vectorTableWidget->setColumnCount(6);
    m_vectorTableWidget->setHorizontalHeaderItem(0, new QTableWidgetItem("Label"));
    m_vectorTableWidget->setHorizontalHeaderItem(1, new QTableWidgetItem("Instruction"));
    m_vectorTableWidget->setHorizontalHeaderItem(2, new QTableWidgetItem("TimeSet"));
    m_vectorTableWidget->setHorizontalHeaderItem(3, new QTableWidgetItem("Capture"));
    m_vectorTableWidget->setHorizontalHeaderItem(4, new QTableWidgetItem("Ext"));
    m_vectorTableWidget->setHorizontalHeaderItem(5, new QTableWidgetItem("Comment"));

    columnIndex = 6;

    if (pinsQuery.exec())
    {
        while (pinsQuery.next())
        {
            int pinId = pinsQuery.value(0).toInt();
            QString pinName = pinsQuery.value(1).toString();
            int channelCount = pinsQuery.value(2).toInt();
            QString typeName = pinsQuery.value(3).toString();

            pinIds.append(pinId);
            pinIdToColumn[pinId] = columnIndex;

            // 添加管脚列
            m_vectorTableWidget->insertColumn(columnIndex);

            // 创建自定义表头
            QTableWidgetItem *headerItem = new QTableWidgetItem();
            headerItem->setText(pinName + "\nx" + QString::number(channelCount) + "\n" + typeName);
            headerItem->setTextAlignment(Qt::AlignCenter);
            QFont font = headerItem->font();
            font.setBold(true);
            headerItem->setFont(font);

            m_vectorTableWidget->setHorizontalHeaderItem(columnIndex, headerItem);
            columnIndex++;
        }
    }

    // 2. 获取向量表数据
    QSqlQuery dataQuery(db);
    dataQuery.prepare("SELECT vtd.id, vtd.label, io.instruction_value, tl.timeset_name, "
                      "vtd.capture, vtd.ext, vtd.comment, vtd.sort_index "
                      "FROM vector_table_data vtd "
                      "JOIN instruction_options io ON vtd.instruction_id = io.id "
                      "JOIN timeset_list tl ON vtd.timeset_id = tl.id "
                      "WHERE vtd.table_id = ? "
                      "ORDER BY vtd.sort_index");
    dataQuery.addBindValue(tableId);

    if (dataQuery.exec())
    {
        QMap<int, int> vectorDataIdToRow; // 向量数据ID到行索引的映射
        int rowIndex = 0;

        while (dataQuery.next())
        {
            int vectorDataId = dataQuery.value(0).toInt();
            QString label = dataQuery.value(1).toString();
            QString instruction = dataQuery.value(2).toString();
            QString timeset = dataQuery.value(3).toString();
            QString capture = dataQuery.value(4).toString();
            QString ext = dataQuery.value(5).toString();
            QString comment = dataQuery.value(6).toString();

            // 添加新行
            m_vectorTableWidget->insertRow(rowIndex);

            // 设置固定列数据
            m_vectorTableWidget->setItem(rowIndex, 0, new QTableWidgetItem(label));
            m_vectorTableWidget->setItem(rowIndex, 1, new QTableWidgetItem(instruction));
            m_vectorTableWidget->setItem(rowIndex, 2, new QTableWidgetItem(timeset));
            m_vectorTableWidget->setItem(rowIndex, 3, new QTableWidgetItem(capture));
            m_vectorTableWidget->setItem(rowIndex, 4, new QTableWidgetItem(ext));
            m_vectorTableWidget->setItem(rowIndex, 5, new QTableWidgetItem(comment));

            vectorDataIdToRow[vectorDataId] = rowIndex;
            rowIndex++;
        }

        // 3. 获取管脚数值 - 直接从数据库中查询pin_options表获取值
        QSqlQuery valueQuery(db);
        QString valueQueryStr = QString(
                                    "SELECT vtd.id AS vector_data_id, "
                                    "       vtp.id AS vector_pin_id, "
                                    "       po.pin_value "
                                    "FROM vector_table_data vtd "
                                    "JOIN vector_table_pin_values vtpv ON vtd.id = vtpv.vector_data_id "
                                    "JOIN vector_table_pins vtp ON vtpv.vector_pin_id = vtp.id "
                                    "JOIN pin_options po ON vtpv.pin_level = po.id "
                                    "WHERE vtd.table_id = %1 "
                                    "ORDER BY vtd.sort_index")
                                    .arg(tableId);

        if (valueQuery.exec(valueQueryStr))
        {
            int count = 0;
            while (valueQuery.next())
            {
                int vectorDataId = valueQuery.value(0).toInt();
                int vectorPinId = valueQuery.value(1).toInt();
                QString pinValue = valueQuery.value(2).toString();
                count++;

                // 找到对应的行和列
                if (vectorDataIdToRow.contains(vectorDataId) && pinIdToColumn.contains(vectorPinId))
                {
                    int row = vectorDataIdToRow[vectorDataId];
                    int col = pinIdToColumn[vectorPinId];

                    // 设置单元格值
                    m_vectorTableWidget->setItem(row, col, new QTableWidgetItem(pinValue));
                }
            }

            // 如果没有找到任何记录，尝试直接使用pin_level值
            if (count == 0)
            {
                QSqlQuery fallbackQuery(db);
                QString fallbackQueryStr = QString(
                                               "SELECT vtd.id AS vector_data_id, "
                                               "       vtp.id AS vector_pin_id, "
                                               "       vtpv.pin_level "
                                               "FROM vector_table_data vtd "
                                               "JOIN vector_table_pin_values vtpv ON vtd.id = vtpv.vector_data_id "
                                               "JOIN vector_table_pins vtp ON vtpv.vector_pin_id = vtp.id "
                                               "WHERE vtd.table_id = %1 "
                                               "ORDER BY vtd.sort_index")
                                               .arg(tableId);

                if (fallbackQuery.exec(fallbackQueryStr))
                {
                    while (fallbackQuery.next())
                    {
                        int vectorDataId = fallbackQuery.value(0).toInt();
                        int vectorPinId = fallbackQuery.value(1).toInt();
                        int pinLevelId = fallbackQuery.value(2).toInt();

                        // 尝试从pin_options表获取实际值
                        QSqlQuery getPinValue(db);
                        QString pinValue;

                        getPinValue.prepare("SELECT pin_value FROM pin_options WHERE id = ?");
                        getPinValue.addBindValue(pinLevelId);

                        if (getPinValue.exec() && getPinValue.next())
                        {
                            pinValue = getPinValue.value(0).toString();
                        }
                        else
                        {
                            // 如果获取失败，直接使用ID作为字符串
                            pinValue = QString::number(pinLevelId);
                        }

                        // 找到对应的行和列
                        if (vectorDataIdToRow.contains(vectorDataId) && pinIdToColumn.contains(vectorPinId))
                        {
                            int row = vectorDataIdToRow[vectorDataId];
                            int col = pinIdToColumn[vectorPinId];

                            // 设置单元格值
                            m_vectorTableWidget->setItem(row, col, new QTableWidgetItem(pinValue));
                        }
                    }
                }
            }
        }
        else
        {
            // 查询失败时输出错误信息
            statusBar()->showMessage(QString("获取管脚值失败: %1").arg(valueQuery.lastError().text()));
        }
    }

    // 调整列宽
    m_vectorTableWidget->resizeColumnsToContents();

    statusBar()->showMessage(QString("已加载向量表: %1").arg(m_vectorTableSelector->currentText()));
}
