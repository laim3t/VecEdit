#include "databaseviewdialog.h"
#include "databasemanager.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QSqlQueryModel>
#include <QDebug>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QBoxLayout>
#include <QHeaderView>
#include <QGroupBox>
#include <QToolBar>
#include <QToolButton>
#include <QIcon>
#include <QStyle>
#include <QApplication>
#include <QLineEdit>
#include <QFileDialog>
#include <QTextStream>
#include <QMessageBox>
#include <QDir>

DatabaseViewDialog::DatabaseViewDialog(QWidget *parent)
    : QDialog(parent), tableModel(nullptr)
{
    setWindowTitle("数据库表查看器");
    resize(1000, 700);
    setMinimumSize(800, 600);
    setupUI();
    updateDatabaseView();
}

DatabaseViewDialog::~DatabaseViewDialog()
{
    if (tableModel)
    {
        delete tableModel;
        tableModel = nullptr;
    }
}

void DatabaseViewDialog::setupUI()
{
    // 创建主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(6);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // 创建顶部工具栏
    QToolBar *toolBar = new QToolBar(this);
    toolBar->setIconSize(QSize(24, 24));
    toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    // 添加刷新按钮到工具栏
    QToolButton *refreshButton = new QToolButton(this);
    refreshButton->setText("刷新数据表");
    refreshButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_BrowserReload));
    refreshButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    refreshButton->setIconSize(QSize(24, 24));
    connect(refreshButton, &QToolButton::clicked, this, &DatabaseViewDialog::onRefreshButtonClicked);
    toolBar->addWidget(refreshButton);

    toolBar->addSeparator();

    // 添加过滤控件
    QWidget *filterWidget = new QWidget(toolBar);
    QHBoxLayout *filterLayout = new QHBoxLayout(filterWidget);
    filterLayout->setContentsMargins(5, 0, 5, 0);
    filterLayout->setSpacing(5);

    QLabel *filterLabel = new QLabel("表过滤:", filterWidget);
    tableFilterCombo = new QComboBox(filterWidget);
    tableFilterCombo->addItem("全部表");
    tableFilterCombo->addItem("系统表");
    tableFilterCombo->addItem("用户表");
    tableFilterCombo->setCurrentIndex(2); // 默认显示用户表
    connect(tableFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DatabaseViewDialog::onFilterChanged);

    filterLayout->addWidget(filterLabel);
    filterLayout->addWidget(tableFilterCombo);
    toolBar->addWidget(filterWidget);

    mainLayout->addWidget(toolBar);

    // 创建主分割窗口
    splitter = new QSplitter(Qt::Horizontal);
    splitter->setChildrenCollapsible(false);

    // 创建左侧数据表树 - 放在一个组合框内
    QGroupBox *tablesGroup = new QGroupBox("数据库表");
    QVBoxLayout *tablesLayout = new QVBoxLayout(tablesGroup);

    tableTreeWidget = new QTreeWidget;
    tableTreeWidget->setHeaderLabels(QStringList() << "表名"
                                                   << "行数");
    tableTreeWidget->setMinimumWidth(250);
    tableTreeWidget->setColumnCount(2);
    tableTreeWidget->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tableTreeWidget->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tableTreeWidget->setAlternatingRowColors(true);
    connect(tableTreeWidget, &QTreeWidget::itemClicked, this, &DatabaseViewDialog::onTableTreeItemClicked);

    tablesLayout->addWidget(tableTreeWidget);

    // 创建右侧数据视图区域
    QWidget *viewContainer = new QWidget;
    QVBoxLayout *viewLayout = new QVBoxLayout(viewContainer);

    dataViewHeader = new QLabel("选择左侧的表查看数据");
    dataViewHeader->setStyleSheet("font-weight: bold; font-size: 14px; padding: 8px;");
    dataViewHeader->setAlignment(Qt::AlignCenter);

    tableView = new QTableView;
    tableView->setEditTriggers(QTableView::NoEditTriggers); // 只读模式
    tableView->horizontalHeader()->setStretchLastSection(true);
    tableView->setAlternatingRowColors(true);
    tableView->setSortingEnabled(true);
    tableView->verticalHeader()->setVisible(false);

    // 应用表格样式
    TableStyleManager::applyTableStyle(tableView);

    viewLayout->addWidget(dataViewHeader);
    viewLayout->addWidget(tableView);

    // 添加组件到拆分器
    splitter->addWidget(tablesGroup);
    splitter->addWidget(viewContainer);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);

    // 添加拆分器到主布局
    mainLayout->addWidget(splitter);

    // 创建底部信息面板
    QGroupBox *infoPanel = new QGroupBox("数据信息");
    QGridLayout *infoPanelLayout = new QGridLayout(infoPanel);

    // 添加数据库路径信息
    QLabel *dbPathLabel = new QLabel("数据库路径:");
    dbPathLabel->setStyleSheet("font-weight: bold;");
    dbPathInfo = new QLabel(DatabaseManager::instance()->database().databaseName());
    dbPathInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);
    dbPathInfo->setCursor(Qt::IBeamCursor);
    dbPathInfo->setStyleSheet("background-color: #f8f8f8; padding: 3px; border: 1px solid #ddd;");

    // 添加选中项信息
    QLabel *selectionLabel = new QLabel("选中项:");
    selectionLabel->setStyleSheet("font-weight: bold;");
    selectionInfo = new QLabel("无");

    // 添加查询输入框
    QLabel *queryLabel = new QLabel("执行SQL:");
    queryLabel->setStyleSheet("font-weight: bold;");
    queryInput = new QLineEdit;
    queryInput->setPlaceholderText("输入SQL查询语句（例如：SELECT * FROM 表名 WHERE 条件）");
    QPushButton *executeButton = new QPushButton("执行");
    executeButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_CommandLink));
    connect(executeButton, &QPushButton::clicked, this, &DatabaseViewDialog::executeQuery);

    // 设置布局
    infoPanelLayout->addWidget(dbPathLabel, 0, 0);
    infoPanelLayout->addWidget(dbPathInfo, 0, 1, 1, 3);
    infoPanelLayout->addWidget(selectionLabel, 1, 0);
    infoPanelLayout->addWidget(selectionInfo, 1, 1, 1, 3);
    infoPanelLayout->addWidget(queryLabel, 2, 0);
    infoPanelLayout->addWidget(queryInput, 2, 1, 1, 2);
    infoPanelLayout->addWidget(executeButton, 2, 3);

    // 添加底部面板到主布局
    mainLayout->addWidget(infoPanel);

    // 创建状态栏
    statusBar = new QStatusBar;
    mainLayout->addWidget(statusBar);

    // 创建按钮布局
    QHBoxLayout *buttonLayout = new QHBoxLayout;

    // 添加附加按钮
    QPushButton *exportButton = new QPushButton("导出数据");
    exportButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileDialogToParent));
    connect(exportButton, &QPushButton::clicked, this, &DatabaseViewDialog::exportData);

    QPushButton *printButton = new QPushButton("打印表格");
    printButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    connect(printButton, &QPushButton::clicked, this, &DatabaseViewDialog::printTable);

    // 添加关闭按钮
    closeButton = new QPushButton("关闭");
    closeButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton));
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    buttonLayout->addWidget(exportButton);
    buttonLayout->addWidget(printButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);

    // 添加按钮布局到主布局
    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
}

void DatabaseViewDialog::updateDatabaseView()
{
    // 清除旧数据
    clearDatabaseView();

    // 检查数据库连接
    if (!DatabaseManager::instance()->isDatabaseConnected())
    {
        statusBar->showMessage("没有打开的数据库");
        return;
    }

    // 获取数据库中的所有表
    QSqlDatabase db = DatabaseManager::instance()->database();
    QStringList tables = db.tables();

    if (tables.isEmpty())
    {
        statusBar->showMessage("数据库中没有表");
        return;
    }

    // 填充表树
    int userTableCount = 0;
    for (const QString &tableName : tables)
    {
        // 判断是否为系统表（以sqlite_开头）
        bool isSystemTable = tableName.startsWith("sqlite_");

        // 根据过滤器决定是否显示
        int filterIndex = tableFilterCombo->currentIndex();
        if ((filterIndex == 1 && !isSystemTable) || // 只显示系统表但这不是系统表
            (filterIndex == 2 && isSystemTable))    // 只显示用户表但这是系统表
        {
            continue;
        }

        QTreeWidgetItem *item = new QTreeWidgetItem(tableTreeWidget);
        item->setText(0, tableName);

        // 系统表使用不同样式
        if (isSystemTable)
        {
            QFont font = item->font(0);
            font.setItalic(true);
            item->setFont(0, font);
            item->setForeground(0, QBrush(QColor(90, 90, 90)));
        }
        else
        {
            userTableCount++;
        }

        // 获取表的行数
        QSqlQuery countQuery(db);
        if (countQuery.exec("SELECT COUNT(*) FROM " + tableName))
        {
            if (countQuery.next())
            {
                int rowCount = countQuery.value(0).toInt();
                item->setText(1, QString::number(rowCount));
                item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
                item->setToolTip(0, QString("表 %1\n包含 %2 行数据").arg(tableName).arg(rowCount));
            }
        }

        // 获取表结构作为子节点
        QSqlQuery columnsQuery(db);
        if (columnsQuery.exec("PRAGMA table_info(" + tableName + ")"))
        {
            while (columnsQuery.next())
            {
                QString columnName = columnsQuery.value(1).toString();
                QString columnType = columnsQuery.value(2).toString();
                QString pkFlag = columnsQuery.value(5).toBool() ? "主键" : "";

                QTreeWidgetItem *columnItem = new QTreeWidgetItem(item);
                columnItem->setText(0, columnName);

                // 设置列类型和主键标志
                QString typeInfo = columnType;
                if (!pkFlag.isEmpty())
                {
                    typeInfo += " (" + pkFlag + ")";
                    QFont font = columnItem->font(0);
                    font.setBold(true);
                    columnItem->setFont(0, font);
                }
                columnItem->setText(1, typeInfo);

                // 设置列信息的工具提示
                columnItem->setToolTip(0, QString("列名: %1\n类型: %2\n%3").arg(columnName).arg(columnType).arg(pkFlag.isEmpty() ? "" : "主键"));
            }
        }
    }

    // 展开第一级
    tableTreeWidget->expandToDepth(0);
    tableTreeWidget->sortByColumn(0, Qt::AscendingOrder);

    // 如果有表，默认选中第一个非系统表
    if (userTableCount > 0)
    {
        for (int i = 0; i < tableTreeWidget->topLevelItemCount(); ++i)
        {
            QTreeWidgetItem *item = tableTreeWidget->topLevelItem(i);
            if (item && !item->text(0).startsWith("sqlite_"))
            {
                tableTreeWidget->setCurrentItem(item);
                onTableTreeItemClicked(item, 0);
                break;
            }
        }
    }

    statusBar->showMessage(QString("已加载 %1 个表（%2 个用户表，%3 个系统表）")
                               .arg(tables.count())
                               .arg(userTableCount)
                               .arg(tables.count() - userTableCount));
}

void DatabaseViewDialog::displayTableContent(const QString &tableName)
{
    // 确保数据库已连接
    if (!DatabaseManager::instance()->isDatabaseConnected())
    {
        return;
    }

    // 更新表头信息
    dataViewHeader->setText(QString("表: %1").arg(tableName));

    // 清除旧模型
    if (tableModel)
    {
        delete tableModel;
        tableModel = nullptr;
    }

    // 创建新表格模型
    tableModel = new QSqlTableModel(this, DatabaseManager::instance()->database());
    tableModel->setTable(tableName);
    tableModel->setEditStrategy(QSqlTableModel::OnManualSubmit);

    // 加载数据
    if (!tableModel->select())
    {
        statusBar->showMessage(QString("加载表 %1 失败: %2").arg(tableName).arg(tableModel->lastError().text()));
        return;
    }

    // 设置列标题
    for (int col = 0; col < tableModel->columnCount(); ++col)
    {
        QString fieldName = tableModel->record().fieldName(col);
        tableModel->setHeaderData(col, Qt::Horizontal, fieldName);

        // 设置主键列加粗
        QSqlQuery pkQuery(DatabaseManager::instance()->database());
        if (pkQuery.exec("PRAGMA table_info(" + tableName + ")"))
        {
            while (pkQuery.next())
            {
                if (pkQuery.value(1).toString() == fieldName && pkQuery.value(5).toBool())
                {
                    QFont headerFont;
                    headerFont.setBold(true);
                    tableModel->setHeaderData(col, Qt::Horizontal, fieldName, Qt::DisplayRole);
                    tableModel->setHeaderData(col, Qt::Horizontal, headerFont, Qt::FontRole);
                    tableModel->setHeaderData(col, Qt::Horizontal, "主键字段", Qt::ToolTipRole);
                    break;
                }
            }
        }
    }

    // 应用模型到视图
    tableView->setModel(tableModel);
    tableView->resizeColumnsToContents();

    // 显示信息
    statusBar->showMessage(QString("已加载表 %1 (%2 行，%3 列)")
                               .arg(tableName)
                               .arg(tableModel->rowCount())
                               .arg(tableModel->columnCount()));
}

void DatabaseViewDialog::clearDatabaseView()
{
    // 清除表树
    tableTreeWidget->clear();

    // 重置表头信息
    dataViewHeader->setText("选择左侧的表查看数据");

    // 清除表格视图
    if (tableModel)
    {
        delete tableModel;
        tableModel = nullptr;
    }
    tableView->setModel(nullptr);
}

void DatabaseViewDialog::onTableTreeItemClicked(QTreeWidgetItem *item, int column)
{
    // 检查是否是顶级项（表名）
    if (item && item->parent() == nullptr)
    {
        QString tableName = item->text(0);
        displayTableContent(tableName);

        // 更新选中项信息
        selectionInfo->setText(QString("表: %1 (%2 行)").arg(tableName).arg(item->text(1)));
    }
    else if (item && item->parent() != nullptr)
    {
        // 列项被点击，显示列详情
        QString columnName = item->text(0);
        QString tableName = item->parent()->text(0);
        selectionInfo->setText(QString("表: %1, 列: %2, 类型: %3").arg(tableName).arg(columnName).arg(item->text(1)));
    }
}

void DatabaseViewDialog::onRefreshButtonClicked()
{
    updateDatabaseView();
}

void DatabaseViewDialog::onFilterChanged(int index)
{
    // 记录当前选中的表
    QString currentTable;
    if (tableTreeWidget->currentItem() && tableTreeWidget->currentItem()->parent() == nullptr)
    {
        currentTable = tableTreeWidget->currentItem()->text(0);
    }

    // 更新表视图
    updateDatabaseView();

    // 如果之前有选中的表，尝试重新选中它
    if (!currentTable.isEmpty())
    {
        for (int i = 0; i < tableTreeWidget->topLevelItemCount(); ++i)
        {
            QTreeWidgetItem *item = tableTreeWidget->topLevelItem(i);
            if (item && item->text(0) == currentTable)
            {
                tableTreeWidget->setCurrentItem(item);
                onTableTreeItemClicked(item, 0);
                break;
            }
        }
    }

    // 更新状态栏
    QString filterType;
    switch (index)
    {
    case 0:
        filterType = "全部表";
        break;
    case 1:
        filterType = "系统表";
        break;
    case 2:
        filterType = "用户表";
        break;
    }
    statusBar->showMessage(QString("过滤模式: %1 - %2")
                               .arg(filterType)
                               .arg(statusBar->currentMessage()));
}

// 实现查询执行功能
void DatabaseViewDialog::executeQuery()
{
    QString query = queryInput->text().trimmed();
    if (query.isEmpty())
    {
        statusBar->showMessage("请输入有效的SQL查询语句");
        return;
    }

    // 清除旧模型
    if (tableModel)
    {
        delete tableModel;
        tableModel = nullptr;
    }

    // 创建自定义查询模型
    QSqlQueryModel *queryModel = new QSqlQueryModel(this);
    queryModel->setQuery(query, DatabaseManager::instance()->database());

    if (queryModel->lastError().isValid())
    {
        statusBar->showMessage("查询错误: " + queryModel->lastError().text());
        delete queryModel;
        return;
    }

    // 设置表头
    for (int i = 0; i < queryModel->columnCount(); ++i)
    {
        queryModel->setHeaderData(i, Qt::Horizontal, queryModel->record().fieldName(i));
    }

    // 设置视图
    tableView->setModel(queryModel);
    tableView->resizeColumnsToContents();

    // 更新表头信息
    dataViewHeader->setText("查询结果: " + query);

    // 显示记录数
    statusBar->showMessage(QString("查询执行成功，返回 %1 条记录").arg(queryModel->rowCount()));
}

// 实现数据导出功能
void DatabaseViewDialog::exportData()
{
    if (!tableView->model())
    {
        QMessageBox::warning(this, "警告", "没有数据可导出！");
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this, "导出数据",
                                                    QDir::homePath() + "/导出数据.csv",
                                                    "CSV文件 (*.csv);;所有文件 (*)");
    if (fileName.isEmpty())
    {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::critical(this, "错误", "无法打开文件进行写入！");
        return;
    }

    QTextStream out(&file);

    // 导出表头
    QStringList headers;
    QAbstractItemModel *model = tableView->model();
    for (int i = 0; i < model->columnCount(); ++i)
    {
        headers << model->headerData(i, Qt::Horizontal).toString();
    }
    out << headers.join(",") << "\n";

    // 导出数据
    for (int row = 0; row < model->rowCount(); ++row)
    {
        QStringList rowData;
        for (int col = 0; col < model->columnCount(); ++col)
        {
            QString data = model->data(model->index(row, col)).toString();
            // 如果数据中包含逗号，用引号括起来
            if (data.contains(",") || data.contains("\"") || data.contains("\n"))
            {
                data.replace("\"", "\"\""); // 转义引号
                data = "\"" + data + "\"";
            }
            rowData << data;
        }
        out << rowData.join(",") << "\n";
    }

    file.close();
    statusBar->showMessage(QString("数据已成功导出到 %1").arg(fileName));
}

// 实现打印表格功能
void DatabaseViewDialog::printTable()
{
    if (!tableView->model())
    {
        QMessageBox::warning(this, "警告", "没有数据可打印！");
        return;
    }

    // 这里只是一个简单的打印预览实现
    // 在实际应用中需要使用QPrinter和QPrintDialog完成更复杂的打印任务
    QMessageBox::information(this, "打印表格",
                             "打印功能在此演示版本中未完全实现。\n"
                             "在完整版本中，这里会打开一个打印对话框，允许您打印当前的表格数据。");

    // 如需实现完整的打印功能，可以参考以下代码：
    /*
    QPrinter printer;
    QPrintDialog printDialog(&printer, this);
    if (printDialog.exec() == QDialog::Accepted) {
        QPainter painter(&printer);
        // 使用painter绘制表格内容
        // ...
    }
    */

    statusBar->showMessage("打印功能已触发");
}