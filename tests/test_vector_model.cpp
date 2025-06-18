#include <QApplication>
#include <QMainWindow>
#include <QTableView>
#include <QHeaderView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QComboBox>
#include <QMessageBox>
#include <QFileDialog>
#include <QStatusBar>
#include <QElapsedTimer>
#include <QDebug>
#include <QCoreApplication>
#include <QSqlDatabase>

#include "vector/vectortablemodel.h"
#include "database/databasemanager.h"
#include "common/logger.h"

/**
 * @class TestWindow
 * @brief 测试VectorTableModel的专用窗口类
 *
 * 提供一个简单的UI来展示并测试VectorTableModel的功能和性能
 */
class TestWindow : public QMainWindow
{
    Q_OBJECT

public:
    TestWindow(QWidget *parent = nullptr)
        : QMainWindow(parent), m_model(nullptr)
    {
        setWindowTitle("VectorTableModel 测试工具");
        setupUI();
        resize(1024, 768);

        // 设置日志级别
        Logger::instance().setLogLevel(Logger::LogLevel::Debug);
    }

private slots:
    void onOpenDatabase()
    {
        // 让用户选择数据库文件
        QString dbPath = QFileDialog::getOpenFileName(this,
                                                      "打开数据库", "", "SQLite数据库 (*.db);;所有文件 (*.*)");

        if (dbPath.isEmpty())
        {
            return;
        }

        // 先关闭已有连接以避免问题
        DatabaseManager::instance()->closeDatabase();

        // 打开数据库
        if (!DatabaseManager::instance()->openExistingDatabase(dbPath))
        {
            QMessageBox::critical(this, "错误",
                                  QString("无法打开数据库: %1").arg(DatabaseManager::instance()->lastError()));
            return;
        }

        // 验证连接状态
        if (!DatabaseManager::instance()->isDatabaseConnected())
        {
            QMessageBox::critical(this, "错误", "数据库连接无效");
            return;
        }

        // 获取可用的表
        QSqlDatabase db = DatabaseManager::instance()->database();
        if (!db.isOpen())
        {
            QMessageBox::critical(this, "错误", "数据库未打开");
            return;
        }

        QSqlQuery query(db);
        qDebug() << "查询数据库:" << db.databaseName() << "是否有效:" << db.isValid() << "是否打开:" << db.isOpen();

        // 首先查看表结构
        QStringList tableNames;
        QSqlQuery pragmaQuery(db);
        if (pragmaQuery.exec("PRAGMA table_info(vector_tables)"))
        {
            qDebug() << "vector_tables表的结构:";
            while (pragmaQuery.next())
            {
                QString columnName = pragmaQuery.value(1).toString();
                qDebug() << "  - 列:" << columnName;
                tableNames << columnName;
            }
        }

        // 根据实际表结构构建查询
        QString nameColumn = "tablename"; // 假设实际列名是tablename而不是name
        if (tableNames.contains("name"))
        {
            nameColumn = "name";
        }
        else if (tableNames.contains("table_name"))
        {
            nameColumn = "table_name";
        }

        // 执行查询
        QString sql = QString("SELECT id, %1 FROM vector_tables ORDER BY id").arg(nameColumn);
        qDebug() << "执行SQL:" << sql;

        if (!query.exec(sql))
        {
            QMessageBox::warning(this, "警告",
                                 QString("无法获取表列表: %1").arg(query.lastError().text()));
            return;
        }

        // 清空表下拉菜单并填充新数据
        m_tableCombo->clear();
        while (query.next())
        {
            int id = query.value(0).toInt();
            QString name = query.value(1).toString();
            if (name.isEmpty())
            {
                name = tr("表%1").arg(id); // 如果没有名称，使用默认名称
            }
            m_tableCombo->addItem(QString("%1: %2").arg(id).arg(name), id);
        }

        if (m_tableCombo->count() > 0)
        {
            m_tableCombo->setEnabled(true);
            m_loadTableButton->setEnabled(true);

            // 更新UI信息
            statusBar()->showMessage(QString("数据库已打开: %1").arg(dbPath), 5000);
        }
        else
        {
            QMessageBox::information(this, "信息", "数据库中未找到向量表");
        }
    }

    void onLoadTable()
    {
        if (m_tableCombo->count() == 0 || !m_tableCombo->isEnabled())
        {
            return;
        }

        // 验证数据库连接仍然有效
        if (!DatabaseManager::instance()->isDatabaseConnected())
        {
            QMessageBox::critical(this, "错误", "数据库连接已丢失，请重新打开数据库");
            return;
        }

        // 获取选中的表ID
        int tableId = m_tableCombo->currentData().toInt();

        // 确保模型已创建
        if (m_model)
        {
            delete m_model; // 删除旧模型以释放资源
            m_model = nullptr;
        }

        m_model = new Vector::VectorTableModel(this);

        // 显示加载进度提示
        statusBar()->showMessage("正在加载表结构...");

        // 计时开始
        QElapsedTimer timer;
        timer.start();

        // 加载表
        if (!m_model->setTable(tableId))
        {
            QMessageBox::critical(this, "错误",
                                  QString("无法加载表ID: %1").arg(tableId));
            statusBar()->clearMessage();
            return;
        }

        // 将模型应用到视图
        m_tableView->setModel(m_model);

        // 设置列宽
        m_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        for (int i = 0; i < m_model->columnCount(); ++i)
        {
            m_tableView->setColumnWidth(i, 100); // 默认宽度
        }

        // 计算并显示加载时间
        qint64 elapsed = timer.elapsed();

        // 更新UI信息
        m_rowCountLabel->setText(QString("总行数: %1").arg(m_model->totalRows()));
        m_colCountLabel->setText(QString("总列数: %1").arg(m_model->columnCount()));
        m_loadTimeLabel->setText(QString("加载时间: %1 ms").arg(elapsed));

        // 启用行跳转控件
        m_rowSpinBox->setRange(1, m_model->totalRows());
        m_rowSpinBox->setEnabled(true);
        m_jumpButton->setEnabled(true);

        // 更新状态栏
        statusBar()->showMessage(
            QString("表加载完成: %1行, %2列, 用时%3毫秒")
                .arg(m_model->totalRows())
                .arg(m_model->columnCount())
                .arg(elapsed),
            10000);
    }

    void onJumpToRow()
    {
        if (!m_model || !m_tableView)
        {
            return;
        }

        // 获取目标行
        int targetRow = m_rowSpinBox->value() - 1; // UI显示从1开始，但模型从0开始

        // 确保行有效
        if (targetRow < 0 || targetRow >= m_model->totalRows())
        {
            QMessageBox::warning(this, "警告", "无效的行索引");
            return;
        }

        // 滚动到目标行
        QModelIndex index = m_model->index(targetRow, 0);
        m_tableView->scrollTo(index, QAbstractItemView::PositionAtCenter);

        // 选择该行
        m_tableView->selectRow(targetRow);

        // 更新状态栏
        statusBar()->showMessage(QString("已跳转到行: %1").arg(targetRow + 1), 3000);
    }

private:
    void setupUI()
    {
        // 创建中央部件和布局
        QWidget *centralWidget = new QWidget(this);
        QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

        // 顶部工具栏
        QHBoxLayout *toolLayout = new QHBoxLayout();

        // 打开数据库按钮
        QPushButton *openDbButton = new QPushButton("打开数据库", this);
        connect(openDbButton, &QPushButton::clicked, this, &TestWindow::onOpenDatabase);
        toolLayout->addWidget(openDbButton);

        // 表选择下拉框
        m_tableCombo = new QComboBox(this);
        m_tableCombo->setEnabled(false);
        m_tableCombo->setMinimumWidth(200);
        toolLayout->addWidget(m_tableCombo);

        // 加载表按钮
        m_loadTableButton = new QPushButton("加载表", this);
        m_loadTableButton->setEnabled(false);
        connect(m_loadTableButton, &QPushButton::clicked, this, &TestWindow::onLoadTable);
        toolLayout->addWidget(m_loadTableButton);

        // 行跳转控件
        QLabel *rowLabel = new QLabel("跳转到行:", this);
        toolLayout->addWidget(rowLabel);

        m_rowSpinBox = new QSpinBox(this);
        m_rowSpinBox->setRange(1, 1000000); // 初始范围，后续会更新
        m_rowSpinBox->setEnabled(false);
        toolLayout->addWidget(m_rowSpinBox);

        m_jumpButton = new QPushButton("跳转", this);
        m_jumpButton->setEnabled(false);
        connect(m_jumpButton, &QPushButton::clicked, this, &TestWindow::onJumpToRow);
        toolLayout->addWidget(m_jumpButton);

        // 添加工具栏到主布局
        mainLayout->addLayout(toolLayout);

        // 创建表格视图
        m_tableView = new QTableView(this);
        m_tableView->setAlternatingRowColors(true);
        m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);

        // 设置垂直表头为固定高度模式，提高滚动性能
        m_tableView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        m_tableView->verticalHeader()->setDefaultSectionSize(30); // 行高30像素

        // 添加表格视图到主布局
        mainLayout->addWidget(m_tableView);

        // 底部状态面板
        QHBoxLayout *statusLayout = new QHBoxLayout();

        m_rowCountLabel = new QLabel("总行数: 0", this);
        m_colCountLabel = new QLabel("总列数: 0", this);
        m_loadTimeLabel = new QLabel("加载时间: 0 ms", this);

        statusLayout->addWidget(m_rowCountLabel);
        statusLayout->addWidget(m_colCountLabel);
        statusLayout->addWidget(m_loadTimeLabel);
        statusLayout->addStretch();

        // 添加状态面板到主布局
        mainLayout->addLayout(statusLayout);

        // 设置中央部件
        setCentralWidget(centralWidget);

        // 创建状态栏
        statusBar()->showMessage("就绪", 3000);
    }

private:
    // UI控件
    QTableView *m_tableView;
    QComboBox *m_tableCombo;
    QPushButton *m_loadTableButton;
    QSpinBox *m_rowSpinBox;
    QPushButton *m_jumpButton;
    QLabel *m_rowCountLabel;
    QLabel *m_colCountLabel;
    QLabel *m_loadTimeLabel;

    // 数据模型
    Vector::VectorTableModel *m_model;
};

// 由于使用了Q_OBJECT，需要包含moc文件
#include "test_vector_model.moc"

/**
 * 测试程序入口
 */
int main(int argc, char *argv[])
{
    // 设置库路径以确保能找到SQL驱动
    QStringList paths = QCoreApplication::libraryPaths();
    paths.prepend(QCoreApplication::applicationDirPath() + "/sqldrivers");
    paths.prepend(QCoreApplication::applicationDirPath());
    QCoreApplication::setLibraryPaths(paths);

    // 启动应用
    QApplication app(argc, argv);

    // 确保SQL驱动可用
    qDebug() << "可用的SQL驱动:" << QSqlDatabase::drivers();
    if (!QSqlDatabase::drivers().contains("QSQLITE"))
    {
        QMessageBox::critical(nullptr, "错误", "SQLite驱动未找到！请确保sqldrivers目录中包含qsqlite.dll");
        return 1;
    }

    // 设置应用程序信息
    app.setApplicationName("VectorTableModel测试工具");
    app.setOrganizationName("VecEdit");

    // 创建并显示测试窗口
    TestWindow window;
    window.show();

    return app.exec();
}