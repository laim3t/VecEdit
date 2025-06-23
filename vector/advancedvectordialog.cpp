#include "advancedvectordialog.h"
#include "vectortableview.h"
#include "vectortablemodel.h"
#include "../database/databasemanager.h"
#include "../common/utils/pathutils.h"

#include <QDebug>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QApplication>
#include <QScreen>
#include <QIcon>
#include <QFileInfo>
#include <QDir>
#include <QFile>

AdvancedVectorDialog::AdvancedVectorDialog(QWidget *parent)
    : QDialog(parent),
      m_vectorTableView(nullptr),
      m_vectorTableModel(nullptr),
      m_vectorTableSelector(nullptr),
      m_refreshButton(nullptr),
      m_saveButton(nullptr),
      m_statusBar(nullptr)
{
    qDebug() << "AdvancedVectorDialog::AdvancedVectorDialog - 创建高级向量表对话框";
    
    // 设置窗口属性
    setWindowTitle(tr("高级向量表视图"));
    setWindowFlags(Qt::Window | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint);
    
    // 设置窗口大小为屏幕的80%
    QScreen *screen = QApplication::primaryScreen();
    QSize screenSize = screen->availableSize();
    resize(screenSize.width() * 0.8, screenSize.height() * 0.8);
    
    // 设置界面
    setupUI();
    
    // 创建模型和视图
    m_vectorTableModel = new VectorTableModel(this);
    m_vectorTableView->setVectorTableModel(m_vectorTableModel);
    
    // 连接信号和槽
    setupConnections();
    
    // 检查数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库未连接，请先打开或创建一个项目"));
        updateStatusMessage(tr("错误: 数据库未连接"));
        return;
    }
    
    // 调试数据库状态
    qDebug() << "数据库连接名称:" << db.connectionName();
    qDebug() << "数据库类型:" << db.driverName();
    qDebug() << "数据库路径:" << db.databaseName();
    qDebug() << "数据库表:" << db.tables().join(", ");
    
    // 刷新向量表列表
    refreshVectorTablesList();
    
    // 初始状态消息
    updateStatusMessage(tr("就绪"));
}

AdvancedVectorDialog::~AdvancedVectorDialog()
{
    qDebug() << "AdvancedVectorDialog::~AdvancedVectorDialog - 销毁高级向量表对话框";
    
    // 模型和视图会由Qt的父子关系自动删除
}

void AdvancedVectorDialog::setupUI()
{
    qDebug() << "AdvancedVectorDialog::setupUI - 设置界面";
    
    // 创建主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // 创建工具栏
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    
    // 创建向量表选择器
    QLabel *selectorLabel = new QLabel(tr("选择向量表:"), this);
    m_vectorTableSelector = new QComboBox(this);
    m_vectorTableSelector->setMinimumWidth(200);
    
    // 创建刷新按钮
    m_refreshButton = new QPushButton(tr("刷新"), this);
    m_refreshButton->setIcon(QIcon::fromTheme("view-refresh"));
    
    // 创建保存按钮
    m_saveButton = new QPushButton(tr("保存"), this);
    m_saveButton->setIcon(QIcon::fromTheme("document-save"));
    
    // 添加控件到工具栏
    toolbarLayout->addWidget(selectorLabel);
    toolbarLayout->addWidget(m_vectorTableSelector);
    toolbarLayout->addWidget(m_refreshButton);
    toolbarLayout->addWidget(m_saveButton);
    toolbarLayout->addStretch();
    
    // 创建向量表视图
    m_vectorTableView = new VectorTableView(this);
    m_vectorTableView->setAlternatingRowColors(true);
    m_vectorTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_vectorTableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_vectorTableView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_vectorTableView->setSortingEnabled(true);
    
    // 创建状态栏
    m_statusBar = new QStatusBar(this);
    m_statusBar->setSizeGripEnabled(false);
    
    // 添加所有控件到主布局
    mainLayout->addLayout(toolbarLayout);
    mainLayout->addWidget(m_vectorTableView, 1);
    mainLayout->addWidget(m_statusBar);
    
    // 设置主布局
    setLayout(mainLayout);
}

void AdvancedVectorDialog::setupConnections()
{
    qDebug() << "AdvancedVectorDialog::setupConnections - 设置信号和槽连接";
    
    // 连接刷新按钮
    connect(m_refreshButton, &QPushButton::clicked, this, &AdvancedVectorDialog::refreshVectorTablesList);
    
    // 连接保存按钮
    connect(m_saveButton, &QPushButton::clicked, this, [this]() {
        if (m_vectorTableModel) {
            bool success = m_vectorTableModel->saveTable();
            if (success) {
                updateStatusMessage(tr("保存成功"));
            } else {
                updateStatusMessage(tr("保存失败"));
            }
        }
    });
    
    // 连接向量表选择器
    connect(m_vectorTableSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &AdvancedVectorDialog::loadSelectedVectorTable);
    
    // 连接视图和模型的信号
    if (m_vectorTableView && m_vectorTableModel) {
        // 当模型数据改变时，确保视图更新
        connect(m_vectorTableModel, &QAbstractTableModel::dataChanged, 
                m_vectorTableView, [this](const QModelIndex &topLeft, const QModelIndex &bottomRight) {
            qDebug() << "模型数据已更改，刷新视图";
            m_vectorTableView->update(topLeft);
            m_vectorTableView->update(bottomRight);
        });
        
        // 当模型结构改变时，确保视图重置
        connect(m_vectorTableModel, &QAbstractTableModel::modelReset, 
                m_vectorTableView, [this]() {
            qDebug() << "模型已重置，刷新整个视图";
            m_vectorTableView->reset();
            m_vectorTableView->resizeColumnsToContents();
            m_vectorTableView->resizeRowsToContents();
            
            // 更新状态栏
            if (m_vectorTableModel) {
                updateStatusMessage(tr("已加载向量表: %1 (行数: %2)").arg(
                    m_vectorTableSelector->currentText()).arg(m_vectorTableModel->rowCount()));
            }
        });
    }
}

void AdvancedVectorDialog::refreshVectorTablesList()
{
    qDebug() << "AdvancedVectorDialog::refreshVectorTablesList - 刷新向量表列表";
    
    // 保存当前选择
    int currentTableId = -1;
    if (m_vectorTableSelector->currentIndex() >= 0) {
        currentTableId = m_vectorTableSelector->currentData().toInt();
    }
    
    // 清空下拉框
    m_vectorTableSelector->clear();
    
    // 从数据库获取向量表列表
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库未连接"));
        updateStatusMessage(tr("错误: 数据库未连接"));
        return;
    }
    
    // 检查数据库连接
    qDebug() << "数据库连接名称:" << db.connectionName();
    qDebug() << "数据库类型:" << db.driverName();
    qDebug() << "数据库路径:" << db.databaseName();
    qDebug() << "数据库打开状态:" << (db.isOpen() ? "已打开" : "未打开");
    
    // 调试所有表
    QStringList tables = db.tables();
    qDebug() << "数据库包含的表:" << tables.join(", ");
    
    // 创建一个映射来存储表ID和名称
    QMap<int, QString> vectorTables;
    
    // 首先尝试从vector_tables表中获取数据
    if (tables.contains("vector_tables", Qt::CaseInsensitive)) {
        QSqlQuery query(db); // 显式指定使用db连接创建查询
        QString queryText = "SELECT id, table_name FROM vector_tables ORDER BY table_name";
        bool success = query.exec(queryText);
        
        if (success) {
            while (query.next()) {
                int tableId = query.value(0).toInt();
                QString tableName = query.value(1).toString();
                vectorTables[tableId] = tableName;
                qDebug() << "从vector_tables找到向量表:" << tableName << "(ID=" << tableId << ")";
            }
        } else {
            qWarning() << "查询vector_tables失败:" << query.lastError().text();
        }
    }
    
    // 然后尝试从VectorTableMasterRecord表中获取数据
    if (tables.contains("VectorTableMasterRecord", Qt::CaseInsensitive)) {
        QSqlQuery query(db); // 显式指定使用db连接创建查询
        QString queryText = "SELECT id, table_name FROM VectorTableMasterRecord ORDER BY table_name";
        bool success = query.exec(queryText);
        
        if (success) {
            while (query.next()) {
                int tableId = query.value(0).toInt();
                QString tableName = query.value(1).toString();
                vectorTables[tableId] = tableName;
                qDebug() << "从VectorTableMasterRecord找到向量表:" << tableName << "(ID=" << tableId << ")";
            }
        } else {
            qWarning() << "查询VectorTableMasterRecord失败:" << query.lastError().text();
        }
    }
    
    // 如果两个表都没有数据，显示错误
    if (vectorTables.isEmpty()) {
        QString message = tr("数据库中找不到任何向量表。可能的原因：\n\n"
                            "1. 数据库驱动程序未正确加载\n"
                            "2. 数据库连接出现问题\n"
                            "3. 数据库确实没有存储任何向量表\n\n"
                            "是否尝试创建新的向量表？");
                            
        QMessageBox::StandardButton reply = QMessageBox::question(this,
                                                                tr("未找到向量表"),
                                                                message,
                                                                QMessageBox::Yes | QMessageBox::No);
                                                                
        if (reply == QMessageBox::Yes) {
            // 可以在这里添加创建新表的逻辑，或者调用相关函数
            qDebug() << "用户选择创建新表，但功能尚未实现";
            QMessageBox::information(this, tr("提示"), tr("创建新表功能尚未实现，请稍后再试。"));
        }
        
        updateStatusMessage(tr("错误: 找不到任何向量表"));
        return;
    }
    
    // 填充下拉框
    int selectedIndex = -1;
    int index = 0;
    
    // 使用QMap的自动排序功能
    QMapIterator<int, QString> it(vectorTables);
    while (it.hasNext()) {
        it.next();
        int tableId = it.key();
        QString tableName = it.value();
        
        qDebug() << "添加向量表到下拉框:" << tableName << "(ID=" << tableId << ")";
        
        m_vectorTableSelector->addItem(tableName, tableId);
        
        // 如果是之前选择的表，记录索引
        if (tableId == currentTableId) {
            selectedIndex = index;
        }
        
        index++;
    }
    
    qDebug() << "总共找到" << index << "个向量表";
    
    // 如果有之前选择的表，恢复选择
    if (selectedIndex >= 0) {
        m_vectorTableSelector->setCurrentIndex(selectedIndex);
    } else if (m_vectorTableSelector->count() > 0) {
        // 否则选择第一个表
        m_vectorTableSelector->setCurrentIndex(0);
    } else {
        // 没有表，显示提示
        updateStatusMessage(tr("警告: 未找到任何向量表"));
        return;
    }
    
    updateStatusMessage(tr("已刷新向量表列表，共 %1 个表").arg(m_vectorTableSelector->count()));
}

void AdvancedVectorDialog::loadSelectedVectorTable()
{
    qDebug() << "AdvancedVectorDialog::loadSelectedVectorTable - 加载选中的向量表";
    
    // 获取选中的表ID
    if (m_vectorTableSelector->currentIndex() < 0) {
        updateStatusMessage(tr("未选择向量表"));
        return;
    }
    
    int tableId = m_vectorTableSelector->currentData().toInt();
    QString tableName = m_vectorTableSelector->currentText();
    
    qDebug() << "尝试加载表:" << tableName << "(ID=" << tableId << ")";
    
    if (tableId <= 0) {
        updateStatusMessage(tr("错误: 无效的表ID"));
        return;
    }
    
    // 检查模型是否创建
    if (!m_vectorTableModel) {
        QMessageBox::critical(this, tr("错误"), tr("内部错误: 向量表模型未初始化"));
        updateStatusMessage(tr("错误: 向量表模型未初始化"));
        return;
    }
    
    // 检查数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen()) {
        QMessageBox::critical(this, tr("错误"), tr("数据库未连接"));
        updateStatusMessage(tr("错误: 数据库未连接"));
        return;
    }
    
    // 检查表是否存在于VectorTableMasterRecord
    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT COUNT(*) FROM VectorTableMasterRecord WHERE id = ?");
    checkQuery.addBindValue(tableId);
    
    if (checkQuery.exec() && checkQuery.next()) {
        int count = checkQuery.value(0).toInt();
        qDebug() << "VectorTableMasterRecord中ID为" << tableId << "的记录数:" << count;
        
        if (count == 0) {
            qWarning() << "AdvancedVectorDialog::loadSelectedVectorTable - 表ID在VectorTableMasterRecord中不存在";
            
            // 检查是否需要迁移数据
            QMessageBox::StandardButton reply = QMessageBox::question(this, 
                tr("表结构不匹配"), 
                tr("表 %1 (ID=%2) 在VectorTableMasterRecord中不存在，可能需要数据迁移。是否尝试迁移数据？").arg(tableName).arg(tableId),
                QMessageBox::Yes | QMessageBox::No);
                
            if (reply == QMessageBox::Yes) {
                // 这里可以添加数据迁移代码，或者调用DatabaseManager中的迁移方法
                updateStatusMessage(tr("数据迁移功能尚未实现"));
            } else {
                updateStatusMessage(tr("已取消加载表"));
                return;
            }
        }
    } else {
        qWarning() << "AdvancedVectorDialog::loadSelectedVectorTable - 检查表是否存在失败:" 
                   << checkQuery.lastError().text() << ", SQL:" << checkQuery.lastQuery();
    }
    
    // 显示加载状态
    updateStatusMessage(tr("正在加载 %1...").arg(tableName));
    QApplication::processEvents();
    
    // 加载表数据
    bool success = m_vectorTableModel->loadTable(tableId);
    
    if (success) {
        qDebug() << "表加载成功，列数:" << m_vectorTableModel->columnCount() 
                << "，行数:" << m_vectorTableModel->rowCount();
        
        // 检查行数，如果为0，可能是二进制文件不存在，尝试保存表初始化它
        if (m_vectorTableModel->rowCount() == 0) {
            // 查询二进制文件是否存在
            QSqlQuery fileQuery(db);
            fileQuery.prepare("SELECT binary_data_filename FROM VectorTableMasterRecord WHERE id = ?");
            fileQuery.addBindValue(tableId);
            
            bool needsInitialSave = false;
            
            if (fileQuery.exec() && fileQuery.next()) {
                QString binFileName = fileQuery.value(0).toString();
                if (binFileName.isEmpty()) {
                    qDebug() << "二进制文件名为空，需要初始化";
                    needsInitialSave = true;
                } else {
                    // 检查文件是否存在
                    QString dbPath = db.databaseName();
                    QString projectBinaryDataDir = Utils::PathUtils::getProjectBinaryDataDirectory(dbPath);
                    if (!projectBinaryDataDir.isEmpty()) {
                        QString binaryFilePath;
                        if (QFileInfo(binFileName).isRelative()) {
                            binaryFilePath = QDir(projectBinaryDataDir).absoluteFilePath(binFileName);
                        } else {
                            binaryFilePath = binFileName;
                        }
                        
                        if (!QFile::exists(binaryFilePath)) {
                            qDebug() << "二进制文件不存在，需要初始化:" << binaryFilePath;
                            needsInitialSave = true;
                        }
                    }
                }
            }
            
            if (needsInitialSave) {
                qInfo() << "正在初始化二进制文件...";
                updateStatusMessage(tr("正在初始化二进制文件..."));
                QApplication::processEvents();
                
                // 添加示例行以便保存
                m_vectorTableModel->addRow(); // 添加一行
                
                // 保存表以创建二进制文件
                bool saveSuccess = m_vectorTableModel->saveTable();
                
                if (saveSuccess) {
                    qInfo() << "二进制文件初始化成功";
                    // 再次加载表以刷新界面
                    m_vectorTableModel->loadTable(tableId);
                } else {
                    qWarning() << "二进制文件初始化失败";
                    QMessageBox::warning(this, tr("警告"), 
                                        tr("二进制文件初始化失败，向量表可能无法正常显示数据"));
                }
            }
        }
        
        // 更新视图列宽等
        if (m_vectorTableView) {
            m_vectorTableView->resizeColumnsToContents();
            m_vectorTableView->resizeRowsToContents();
            
            // 确保视图更新
            m_vectorTableView->reset();
        }
        
        updateStatusMessage(tr("已加载向量表: %1 (行数: %2)").arg(tableName).arg(m_vectorTableModel->rowCount()));
    } else {
        qWarning() << "表加载失败";
        QMessageBox::warning(this, tr("警告"), 
                            tr("加载向量表 %1 失败，请检查控制台输出获取详细错误信息").arg(tableName));
        updateStatusMessage(tr("加载向量表失败: %1").arg(tableName));
    }
}

void AdvancedVectorDialog::updateStatusMessage(const QString &message)
{
    if (m_statusBar) {
        m_statusBar->showMessage(message);
    }
} 