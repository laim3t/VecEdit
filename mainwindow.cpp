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
#include <QStyledItemDelegate>

// 实现自定义代理类
VectorTableItemDelegate::VectorTableItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
    // 清空缓存，确保每次创建代理时都重新从数据库获取选项
    m_instructionOptions.clear();
    m_timesetOptions.clear();
    m_pinOptions.clear();
}

QWidget *VectorTableItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // 根据列索引创建不同类型的编辑器
    int column = index.column();

    // 指令列
    if (column == 1)
    {
        QComboBox *editor = new QComboBox(parent);
        editor->addItems(getInstructionOptions());
        return editor;
    }
    // 时间集列
    else if (column == 2)
    {
        QComboBox *editor = new QComboBox(parent);
        editor->addItems(getTimesetOptions());
        return editor;
    }
    // 捕获列
    else if (column == 3)
    {
        QComboBox *editor = new QComboBox(parent);
        editor->addItem("");
        editor->addItem("Y");
        return editor;
    }
    // 管脚列
    else if (column >= 6)
    {
        QComboBox *editor = new QComboBox(parent);
        editor->addItems(getPinOptions());
        return editor;
    }

    // 其他列（标签、Ext、注释）使用默认文本编辑器
    return QStyledItemDelegate::createEditor(parent, option, index);
}

void VectorTableItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    int column = index.column();
    QString value = index.model()->data(index, Qt::EditRole).toString();

    // 如果是下拉框编辑器
    if ((column == 1) || (column == 2) || (column == 3) || (column >= 6))
    {
        QComboBox *comboBox = static_cast<QComboBox *>(editor);

        // 对于Capture列，特殊处理空值
        if (column == 3 && value.isEmpty())
        {
            comboBox->setCurrentIndex(0); // 设置为第一项（空值）
        }
        else
        {
            int comboIndex = comboBox->findText(value);
            if (comboIndex >= 0)
            {
                comboBox->setCurrentIndex(comboIndex);
            }
        }
    }
    else
    {
        // 其他使用默认设置
        QStyledItemDelegate::setEditorData(editor, index);
    }
}

void VectorTableItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    int column = index.column();

    // 如果是下拉框编辑器
    if ((column == 1) || (column == 2) || (column == 3) || (column >= 6))
    {
        QComboBox *comboBox = static_cast<QComboBox *>(editor);
        QString value = comboBox->currentText();
        model->setData(index, value, Qt::EditRole);
    }
    else
    {
        // 其他使用默认设置
        QStyledItemDelegate::setModelData(editor, model, index);
    }
}

QStringList VectorTableItemDelegate::getInstructionOptions() const
{
    // 如果已缓存，则直接返回
    if (!m_instructionOptions.isEmpty())
    {
        return m_instructionOptions;
    }

    // 从数据库获取指令选项
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery query(db);
    query.prepare("SELECT instruction_value FROM instruction_options ORDER BY id");
    if (query.exec())
    {
        while (query.next())
        {
            m_instructionOptions << query.value(0).toString();
        }
    }
    else
    {
        qWarning() << "获取指令选项失败:" << query.lastError().text();
    }

    return m_instructionOptions;
}

QStringList VectorTableItemDelegate::getTimesetOptions() const
{
    // 如果已缓存，则直接返回
    if (!m_timesetOptions.isEmpty())
    {
        return m_timesetOptions;
    }

    // 从数据库获取时间集选项
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery query(db);
    query.prepare("SELECT timeset_name FROM timeset_list ORDER BY id");
    if (query.exec())
    {
        while (query.next())
        {
            m_timesetOptions << query.value(0).toString();
        }
    }
    else
    {
        qWarning() << "获取TimeSet选项失败:" << query.lastError().text();
    }

    return m_timesetOptions;
}

QStringList VectorTableItemDelegate::getPinOptions() const
{
    // 如果已缓存，则直接返回
    if (!m_pinOptions.isEmpty())
    {
        return m_pinOptions;
    }

    // 从数据库获取管脚选项
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery query(db);
    query.prepare("SELECT pin_value FROM pin_options ORDER BY id");
    if (query.exec())
    {
        while (query.next())
        {
            m_pinOptions << query.value(0).toString();
        }
    }
    else
    {
        qWarning() << "获取管脚选项失败:" << query.lastError().text();
    }

    return m_pinOptions;
}

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
    QPushButton *saveButton = new QPushButton("保存", m_vectorTableContainer);

    controlLayout->addWidget(tableLabel);
    controlLayout->addWidget(m_vectorTableSelector);
    controlLayout->addStretch();
    controlLayout->addWidget(refreshButton);
    controlLayout->addWidget(saveButton);

    // 向量表
    m_vectorTableWidget = new QTableWidget(m_vectorTableContainer);
    m_vectorTableWidget->setAlternatingRowColors(true);
    m_vectorTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_vectorTableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_vectorTableWidget->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_vectorTableWidget->horizontalHeader()->setStretchLastSection(true);
    m_vectorTableWidget->verticalHeader()->setVisible(true);

    // 创建并设置自定义代理
    m_itemDelegate = new VectorTableItemDelegate(this);
    m_vectorTableWidget->setItemDelegate(m_itemDelegate);

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
    connect(saveButton, &QPushButton::clicked, this, &MainWindow::saveVectorTableData);
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

    // 检查数据库中的TimeSet数据
    QSqlQuery checkTimeSetQuery(db);
    if (checkTimeSetQuery.exec("SELECT COUNT(*) FROM timeset_list"))
    {
        if (checkTimeSetQuery.next())
        {
            int count = checkTimeSetQuery.value(0).toInt();
            qDebug() << "数据库中找到" << count << "个TimeSet记录";

            // 如果存在TimeSet记录，显示它们
            if (count > 0 && checkTimeSetQuery.exec("SELECT id, timeset_name FROM timeset_list"))
            {
                qDebug() << "TimeSet列表:";
                while (checkTimeSetQuery.next())
                {
                    qDebug() << "ID:" << checkTimeSetQuery.value(0).toInt()
                             << "名称:" << checkTimeSetQuery.value(1).toString();
                }
            }
        }
    }
    else
    {
        qWarning() << "检查TimeSet数据失败:" << checkTimeSetQuery.lastError().text();
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
                      "LEFT JOIN timeset_list tl ON vtd.timeset_id = tl.id "
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

            // 修改Capture列显示逻辑，当值为"0"时显示为空白
            QString captureDisplay = (capture == "0") ? "" : capture;
            m_vectorTableWidget->setItem(rowIndex, 3, new QTableWidgetItem(captureDisplay));

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

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        QMessageBox::critical(this, "保存失败", "数据库未打开");
        return;
    }

    // 开始事务
    db.transaction();
    QSqlQuery query(db);

    try
    {
        // 获取向量表ID
        int tableId = -1;
        query.prepare("SELECT id FROM vector_tables WHERE table_name = ?");
        query.addBindValue(currentTable);
        if (!query.exec() || !query.next())
        {
            throw QString("无法获取向量表ID: " + query.lastError().text());
        }
        tableId = query.value(0).toInt();

        // 清除现有数据 - 先删除关联的pin_values，再删除主数据
        query.prepare("DELETE FROM vector_table_pin_values WHERE vector_data_id IN "
                      "(SELECT id FROM vector_table_data WHERE table_id = ?)");
        query.addBindValue(tableId);
        if (!query.exec())
        {
            throw QString("无法清除关联的管脚值数据: " + query.lastError().text());
        }

        query.prepare("DELETE FROM vector_table_data WHERE table_id = ?");
        query.addBindValue(tableId);
        if (!query.exec())
        {
            throw QString("无法清除现有数据: " + query.lastError().text());
        }

        // 获取已选择的管脚列表
        QList<int> pinIds;
        QList<QString> pinNames;
        query.prepare("SELECT vtp.id, pl.pin_name FROM vector_table_pins vtp "
                      "JOIN pin_list pl ON vtp.pin_id = pl.id "
                      "WHERE vtp.table_id = ? ORDER BY pl.pin_name");
        query.addBindValue(tableId);
        if (!query.exec())
        {
            throw QString("无法获取管脚列表: " + query.lastError().text());
        }

        while (query.next())
        {
            pinIds.append(query.value(0).toInt());
            pinNames.append(query.value(1).toString());
        }

        if (pinIds.isEmpty())
        {
            throw QString("没有找到任何关联的管脚");
        }

        // 逐行保存数据
        for (int row = 0; row < m_vectorTableWidget->rowCount(); ++row)
        {
            // 获取基本信息
            QString label = m_vectorTableWidget->item(row, 0) ? m_vectorTableWidget->item(row, 0)->text() : "";
            QString instruction = m_vectorTableWidget->item(row, 1) ? m_vectorTableWidget->item(row, 1)->text() : "";
            QString timeSet = m_vectorTableWidget->item(row, 2) ? m_vectorTableWidget->item(row, 2)->text() : "";
            QString capture = m_vectorTableWidget->item(row, 3) ? m_vectorTableWidget->item(row, 3)->text() : "";
            QString ext = m_vectorTableWidget->item(row, 4) ? m_vectorTableWidget->item(row, 4)->text() : "";
            QString comment = m_vectorTableWidget->item(row, 5) ? m_vectorTableWidget->item(row, 5)->text() : "";

            // 获取指令ID
            int instructionId = 1; // 默认为1 (VECTOR)
            if (!instruction.isEmpty())
            {
                QSqlQuery instrQuery(db);
                instrQuery.prepare("SELECT id FROM instruction_options WHERE instruction_value = ?");
                instrQuery.addBindValue(instruction);
                if (instrQuery.exec() && instrQuery.next())
                {
                    instructionId = instrQuery.value(0).toInt();
                }
            }

            // 获取时间集ID
            int timeSetId = -1;
            if (!timeSet.isEmpty())
            {
                QSqlQuery timeSetQuery(db);
                timeSetQuery.prepare("SELECT id FROM timeset_list WHERE timeset_name = ?");
                timeSetQuery.addBindValue(timeSet);
                if (timeSetQuery.exec() && timeSetQuery.next())
                {
                    timeSetId = timeSetQuery.value(0).toInt();
                }
            }

            // 插入行数据
            QSqlQuery insertRowQuery(db);
            insertRowQuery.prepare("INSERT INTO vector_table_data "
                                   "(table_id, label, instruction_id, timeset_id, capture, ext, comment, sort_index) "
                                   "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
            insertRowQuery.addBindValue(tableId);
            insertRowQuery.addBindValue(label);
            insertRowQuery.addBindValue(instructionId);
            insertRowQuery.addBindValue(timeSetId > 0 ? timeSetId : QVariant(QVariant::Int));
            insertRowQuery.addBindValue(capture == "Y" ? 1 : 0);
            insertRowQuery.addBindValue(ext);
            insertRowQuery.addBindValue(comment);
            insertRowQuery.addBindValue(row);

            if (!insertRowQuery.exec())
            {
                throw QString("保存行 " + QString::number(row + 1) + " 失败: " + insertRowQuery.lastError().text());
            }

            // 获取插入的行ID
            int rowId = insertRowQuery.lastInsertId().toInt();

            // 保存管脚数据
            for (int i = 0; i < pinIds.size(); ++i)
            {
                int pinCol = i + 6; // 管脚从第6列开始
                QString pinValue = m_vectorTableWidget->item(row, pinCol) ? m_vectorTableWidget->item(row, pinCol)->text() : "";

                // 获取pin_option_id
                int pinOptionId = 5; // 默认为X (id=5)
                if (!pinValue.isEmpty())
                {
                    QSqlQuery pinOptionQuery(db);
                    pinOptionQuery.prepare("SELECT id FROM pin_options WHERE pin_value = ?");
                    pinOptionQuery.addBindValue(pinValue);
                    if (pinOptionQuery.exec() && pinOptionQuery.next())
                    {
                        pinOptionId = pinOptionQuery.value(0).toInt();
                    }
                }

                // 插入管脚数据
                QSqlQuery pinDataQuery(db);
                pinDataQuery.prepare("INSERT INTO vector_table_pin_values "
                                     "(vector_data_id, vector_pin_id, pin_level) VALUES (?, ?, ?)");
                pinDataQuery.addBindValue(rowId);
                pinDataQuery.addBindValue(pinIds[i]);
                pinDataQuery.addBindValue(pinOptionId);

                if (!pinDataQuery.exec())
                {
                    throw QString("保存管脚 " + pinNames[i] + " 数据失败: " + pinDataQuery.lastError().text());
                }
            }
        }

        // 提交事务
        db.commit();
        QMessageBox::information(this, "保存成功", "向量表数据已成功保存");
        statusBar()->showMessage("向量表数据已成功保存");
    }
    catch (const QString &errorMsg)
    {
        // 错误处理和回滚
        db.rollback();
        QMessageBox::critical(this, "保存失败", errorMsg);
        statusBar()->showMessage("保存失败: " + errorMsg);
        qDebug() << "保存失败: " << errorMsg;
    }
}
