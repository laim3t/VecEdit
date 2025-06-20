// ==========================================================
//  Headers for: mainwindow_project_tables.cpp
// ==========================================================
#include "app/mainwindow.h"

// Qt Widgets & Core
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QSettings>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

// Project-specific headers
#include "database/databasemanager.h"
#include "vector/vectordatahandler.h"
#include "common/utils/pathutils.h"

void MainWindow::createNewProject()
{
    closeCurrentProject();

    // 步骤1：获取保存路径
    QSettings settings(QDir::homePath() + "/.vecedit/settings.ini", QSettings::IniFormat);
    QString lastPath = settings.value("lastUsedPath", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();
    QString dbPath = QFileDialog::getSaveFileName(this, "保存项目数据库", lastPath + "/VecEditProject.db", "SQLite数据库 (*.db)");
    if (dbPath.isEmpty()) {
        return;
    }
    settings.setValue("lastUsedPath", QFileInfo(dbPath).absolutePath());

    // 步骤2：初始化数据库
    QString schemaPath = ":/db/schema.sql";
    if (DatabaseManager::instance()->initializeNewDatabase(dbPath, schemaPath)) {
        m_currentDbPath = dbPath;
        updateWindowTitle(dbPath);
        statusBar()->showMessage("数据库创建成功: " + dbPath);

        // 步骤3：依次显示对话框
        if (showAddPinsDialog()) {
            if (showTimeSetDialog(true)) {
                addNewVectorTable();
            }
        }

        // 步骤4：刷新UI
        loadVectorTable();
        updateMenuState();
        refreshSidebarNavigator();
    } else {
        QMessageBox::critical(this, "错误", "创建项目数据库失败：\n" + DatabaseManager::instance()->lastError());
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
        // 如果有未保存的内容，提示用户保存
        if (m_hasUnsavedChanges)
        {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this,
                "保存修改",
                "关闭项目前，是否保存当前未保存的内容？",
                QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

            if (reply == QMessageBox::Yes)
            {
                saveVectorTableData();

                // 如果保存后仍有未保存内容（可能保存失败），询问是否继续关闭
                if (m_hasUnsavedChanges)
                {
                    QMessageBox::StandardButton continueReply = QMessageBox::question(
                        this,
                        "保存失败",
                        "保存失败，是否仍然关闭项目？",
                        QMessageBox::Yes | QMessageBox::No);

                    if (continueReply == QMessageBox::No)
                    {
                        return; // 取消关闭项目
                    }
                }
            }
            else if (reply == QMessageBox::Cancel)
            {
                return; // 取消关闭项目
            }
        }

        // 重置未保存内容标志
        m_hasUnsavedChanges = false;

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

void MainWindow::loadVectorTable()
{
    qDebug() << "--- [Load Vector Table Start] 开始加载向量表 ---";

    // 清空当前选择框
    qDebug() << "--- [Load Vector Table Step] 清空向量表选择器";
    m_vectorTableSelector->clear();

    // 清空Tab页签
    qDebug() << "--- [Load Vector Table Step] 清空Tab页签";
    m_vectorTabWidget->clear();
    m_tabToTableId.clear();

    // 获取数据库连接
    qDebug() << "--- [Load Vector Table Step] 获取数据库连接";
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qDebug() << "--- [Load Vector Table Error] 数据库未打开";
        statusBar()->showMessage("错误：数据库未打开");
        qDebug() << "--- [Load Vector Table End] 因数据库未打开而失败 ---";
        return;
    }

    qDebug() << "--- [Load Vector Table Step] 数据库连接正常，当前数据库:" << db.databaseName();

    // 刷新itemDelegate的缓存，确保TimeSet选项是最新的
    qDebug() << "--- [Load Vector Table Step] 刷新TimeSet选项缓存";
    if (m_itemDelegate)
    {
        m_itemDelegate->refreshCache();
    }

    // 查询所有向量表
    qDebug() << "--- [Load Vector Table Step] 开始查询所有向量表";
    QSqlQuery tableQuery(db);
    if (tableQuery.exec("SELECT id, table_name FROM vector_tables ORDER BY table_name"))
    {
        qDebug() << "--- [Load Vector Table Step] 向量表查询执行成功";
        int count = 0;
        while (tableQuery.next())
        {
            int tableId = tableQuery.value(0).toInt();
            QString tableName = tableQuery.value(1).toString();
            qDebug() << "--- [Load Vector Table Step] 找到向量表:" << tableName << "ID:" << tableId;

            // 添加到下拉选择框
            qDebug() << "--- [Load Vector Table Step] 将表添加到选择框:" << tableName;
            m_vectorTableSelector->addItem(tableName, tableId);

            // 添加到Tab页签
            qDebug() << "--- [Load Vector Table Step] 调用addVectorTableTab:" << tableName;
            addVectorTableTab(tableId, tableName);

            count++;
        }

        qDebug() << "--- [Load Vector Table Step] 总共找到" << count << "个向量表";
    }
    else
    {
        qDebug() << "--- [Load Vector Table Error] 向量表查询失败:" << tableQuery.lastError().text();
    }

    // 如果有向量表，显示向量表窗口，否则显示欢迎窗口
    if (m_vectorTableSelector->count() > 0)
    {
        qDebug() << "--- [Load Vector Table Step] 有向量表，显示向量表窗口";
        m_welcomeWidget->setVisible(false);
        m_vectorTableContainer->setVisible(true);

        // 默认选择第一个表
        qDebug() << "--- [Load Vector Table Step] 默认选择第一个表";
        m_vectorTableSelector->setCurrentIndex(0);
        qDebug() << "--- [Load Vector Table End] 成功加载了向量表 ---";
    }
    else
    {
        qDebug() << "--- [Load Vector Table Step] 没有找到向量表，显示欢迎窗口";
        m_welcomeWidget->setVisible(true);
        m_vectorTableContainer->setVisible(false);
        QMessageBox::information(this, "提示", "没有找到向量表，请先创建向量表");
        qDebug() << "--- [Load Vector Table End] 无向量表可加载 ---";
    }
}

void MainWindow::addNewVectorTable()
{
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        QMessageBox::warning(this, "警告", "请先打开或创建一个项目数据库");
        return;
    }

    // 弹出向量表命名对话框
    QDialog dialog(this);
    dialog.setWindowTitle("创建向量表向导");
    dialog.setFixedSize(320, 120);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    // 名称标签和输入框
    QLabel *nameLabel = new QLabel("向量表名称:", &dialog);
    layout->addWidget(nameLabel);

    QLineEdit *nameLineEdit = new QLineEdit(&dialog);
    nameLineEdit->setPlaceholderText("请输入向量表名称");
    layout->addWidget(nameLineEdit);

    // 按钮
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttonBox);

    // 连接信号和槽
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        QString tableName = nameLineEdit->text().trimmed();
        if (tableName.isEmpty()) {
            QMessageBox::warning(this, "无效名称", "向量表名称不能为空。");
            return;
        }

        // ========= 事务总指挥部：开始 =========
        QSqlDatabase db = DatabaseManager::instance()->database();
        if (!db.transaction()) {
            qCritical() << "Failed to start transaction in addNewVectorTable.";
            QMessageBox::critical(this, "数据库错误", "无法开始创建新表的事务。");
            return;
        }
        qDebug() << "--- [Sub-Workflow] addNewVectorTable: Transaction started.";

        bool overallSuccess = false;
        
        // 检查表名是否已存在
        QSqlQuery checkQuery(db);
        checkQuery.prepare("SELECT COUNT(*) FROM vector_tables WHERE table_name = ?");
        checkQuery.addBindValue(tableName);

        if (!checkQuery.exec() || !checkQuery.next()) {
            qDebug() << "--- [Sub-Workflow] addNewVectorTable: 检查表名失败:" << checkQuery.lastError().text();
            db.rollback();
            QMessageBox::critical(this, "数据库错误", "检查表名是否存在时发生错误。");
            return;
        }
        
        int count = checkQuery.value(0).toInt();
        if (count > 0) {
            db.rollback();
            QMessageBox::warning(this, "错误", "已存在同名向量表");
            return;
        }

        // 插入新表记录
        QSqlQuery insertQuery(db);
        insertQuery.prepare("INSERT INTO vector_tables (table_name) VALUES (?)");
        insertQuery.addBindValue(tableName);

        if (!insertQuery.exec()) {
            db.rollback();
            QMessageBox::critical(this, "数据库错误", "无法在数据库中创建新向量表: " + insertQuery.lastError().text());
            return;
        }
        
        int newTableId = insertQuery.lastInsertId().toInt();
        qDebug() << "--- [Sub-Workflow] addNewVectorTable: 新向量表创建成功，ID:" << newTableId << ", 名称:" << tableName;

        // 使用 PathUtils 获取项目特定的二进制数据目录
        QString projectBinaryDataDir = Utils::PathUtils::getProjectBinaryDataDirectory(m_currentDbPath);
        if (projectBinaryDataDir.isEmpty()) {
            db.rollback();
            QMessageBox::critical(this, "错误", QString("无法为数据库 '%1' 生成二进制数据目录路径。").arg(m_currentDbPath));
            return;
        }

        QDir dataDir(projectBinaryDataDir);
        if (!dataDir.exists() && !dataDir.mkpath(".")) {
            db.rollback();
            QMessageBox::critical(this, "错误", "无法创建项目二进制数据目录: " + projectBinaryDataDir);
            return;
        }

        // 构造二进制文件名
        QString binaryOnlyFileName = QString("table_%1_data.vbindata").arg(newTableId);
        QString absoluteBinaryFilePath = QDir::cleanPath(projectBinaryDataDir + QDir::separator() + binaryOnlyFileName);
        absoluteBinaryFilePath = QDir::toNativeSeparators(absoluteBinaryFilePath);

        // 创建VectorTableMasterRecord记录
        QSqlQuery insertMasterQuery(db);
        insertMasterQuery.prepare("INSERT INTO VectorTableMasterRecord "
                                  "(original_vector_table_id, table_name, binary_data_filename, "
                                  "file_format_version, data_schema_version, row_count, column_count) "
                                  "VALUES (?, ?, ?, ?, ?, ?, ?)");
        insertMasterQuery.addBindValue(newTableId);
        insertMasterQuery.addBindValue(tableName);
        insertMasterQuery.addBindValue(binaryOnlyFileName);
        insertMasterQuery.addBindValue(Persistence::CURRENT_FILE_FORMAT_VERSION);
        insertMasterQuery.addBindValue(1);  // 初始数据 schema version
        insertMasterQuery.addBindValue(0);  // row_count
        insertMasterQuery.addBindValue(0);  // column_count

        if (!insertMasterQuery.exec()) {
            db.rollback();
            QMessageBox::critical(this, "错误", "创建VectorTableMasterRecord记录失败: " + insertMasterQuery.lastError().text());
            return;
        }

        // 添加默认列配置
        if (!addDefaultColumnConfigurations(newTableId)) {
            db.rollback();
            QMessageBox::critical(this, "错误", "无法添加默认列配置");
            return;
        }

        // 创建空的二进制文件
        QFile binaryFile(absoluteBinaryFilePath);
        if (!binaryFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            db.rollback();
            QMessageBox::critical(this, "错误", "创建向量表失败: 无法创建二进制数据文件: " + binaryFile.errorString());
            return;
        }

        // 创建并写入文件头
        BinaryFileHeader header;
        header.magic_number = Persistence::VEC_BINDATA_MAGIC;
        header.file_format_version = Persistence::CURRENT_FILE_FORMAT_VERSION;
        header.row_count_in_file = 0;
        header.column_count_in_file = 0;
        header.data_schema_version = 1;
        header.timestamp_created = QDateTime::currentSecsSinceEpoch();
        header.timestamp_updated = header.timestamp_created;

        bool headerWriteSuccess = Persistence::BinaryFileHelper::writeBinaryHeader(&binaryFile, header);
        binaryFile.close();

        if (!headerWriteSuccess) {
            QFile::remove(absoluteBinaryFilePath);
            db.rollback();
            QMessageBox::critical(this, "错误", "创建向量表失败: 无法写入二进制文件头");
            return;
        }

        // 添加到下拉框和Tab页签
        m_vectorTableSelector->addItem(tableName, newTableId);
        addVectorTableTab(newTableId, tableName);

        // 选中新添加的表
        int newIndex = m_vectorTableSelector->findData(newTableId);
        if (newIndex >= 0) {
            m_vectorTableSelector->setCurrentIndex(newIndex);
        }

        // 按顺序执行向导步骤，任何一步失败都会导致整体失败
        if (showPinSelectionDialog(newTableId, tableName)) {
            qDebug() << "--- [Sub-Workflow] addNewVectorTable: Pin selection successful.";
            if (showVectorDataDialog(db, newTableId, tableName)) {
                qDebug() << "--- [Sub-Workflow] addNewVectorTable: Vector data entry successful.";
                overallSuccess = true; // 仅当所有步骤都成功时，才算成功
            } else {
                qDebug() << "--- [Sub-Workflow] addNewVectorTable: Vector data entry was cancelled or failed.";
            }
        } else {
            qDebug() << "--- [Sub-Workflow] addNewVectorTable: Pin selection was cancelled or failed.";
        }

        // ========= 事务总指挥部：结束 =========
        if (overallSuccess) {
            if (!db.commit()) {
                qCritical() << "Failed to commit transaction in addNewVectorTable: " << db.lastError().text();
                QMessageBox::critical(this, "数据库错误", "提交新表事务失败。");
                db.rollback();
                // 失败后依然要刷新UI，以防万一有部分UI状态不一致
                refreshSidebarNavigator();
                refreshVectorTableData();
            } else {
                qDebug() << "--- [Sub-Workflow] addNewVectorTable: Transaction committed successfully.";

                // ========= 100% CERTAINTY PROBE: PART 1 =========
                qDebug() << "[CERTAINTY_CHECK] In addNewVectorTable, immediately after commit.";
                QSqlQuery mainWindowValidator(db); // 使用刚刚提交事务的同一个db句柄
                mainWindowValidator.prepare("SELECT row_count FROM VectorTableMasterRecord WHERE id = ?");
                mainWindowValidator.addBindValue(newTableId);
                if (mainWindowValidator.exec() && mainWindowValidator.next()) {
                    int count = mainWindowValidator.value(0).toInt();
                    qDebug() << "[CERTAINTY_CHECK] SUCCESS: MainWindow's validator read row_count =" << count << "for tableId" << newTableId;
                } else {
                    qWarning() << "[CERTAINTY_CHECK] FAILURE: MainWindow's validator FAILED to read row_count. Error:" << mainWindowValidator.lastError().text();
                }
                // ===================================================

                qDebug() << "[UI_REFRESH_DEBUG] Starting UI refresh sequence...";
                
                refreshSidebarNavigator();
                qDebug() << "[UI_REFRESH_DEBUG] After refreshSidebarNavigator()";
                
                refreshVectorTableData();
                qDebug() << "[UI_REFRESH_DEBUG] After refreshVectorTableData()";

                // 手动触发一次刷新来加载新表
                int newIndex = m_vectorTableSelector->findData(newTableId);
                if (newIndex != -1) {
                    qDebug() << "[UI_REFRESH_DEBUG] Found new table at index:" << newIndex << ". Blocking signals and setting index.";
                    // 更新下拉框的选中项，这会触发 onVectorTableSelectionChanged
                    // 在该函数中，模型会被 setTable() 重置
                    m_vectorTableSelector->blockSignals(true);
                    m_vectorTableSelector->setCurrentIndex(newIndex);
                    m_vectorTableSelector->blockSignals(false);
                    
                    // **关键修复**: 在模型重置后，我们必须手动加载第一页的数据
                    qDebug() << "[UI_REFRESH_DEBUG] Calling m_vectorTableModel->setTable(" << newTableId << ")";
                    m_vectorTableModel->setTable(newTableId);
                    qDebug() << "[UI_REFRESH_DEBUG] Returned from setTable. Model row count is now:" << m_vectorTableModel->rowCount();
                    
                    qDebug() << "[UI_REFRESH_DEBUG] Manually calling loadCurrentPage() to ensure data is displayed";
                    loadCurrentPage();
                } else {
                    qDebug() << "[UI_REFRESH_DEBUG] COULD NOT FIND new table with ID" << newTableId << "in selector.";
                }
                
                qDebug() << "[UI_REFRESH_DEBUG] UI refresh sequence finished.";
            }
        } else {
            qDebug() << "--- [Sub-Workflow] addNewVectorTable: Rolling back transaction due to failure or cancellation.";
            db.rollback();
            if (!tableName.isEmpty()) { 
                 QMessageBox::warning(this, "操作已取消", "新建向量表的操作已被中途取消或失败，所有更改已自动回滚。");
            }
            // 失败或取消后，依然要刷新UI
            refreshSidebarNavigator();
            refreshVectorTableData();
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

    // 检查是否为最后一张表
    if (m_vectorTableSelector->count() <= 1)
    {
        QMessageBox::warning(this, "禁止删除", "此表为最后一张表，禁止删除。");
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

    // 删除向量表
    QString errorMessage;
    // 直接使用数据库API删除表
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery query(db);

    // 开始事务
    db.transaction();

    // 删除表定义
    query.prepare("DELETE FROM VectorTable WHERE id = ?");
    query.addBindValue(tableId);
    bool success = query.exec();

    // 删除相关数据
    if (success)
    {
        query.prepare("DELETE FROM VectorTableColumnConfiguration WHERE master_record_id = ?");
        query.addBindValue(tableId);
        success = success && query.exec();
    }

    if (success)
    {
        query.prepare("DELETE FROM vector_data WHERE table_id = ?");
        query.addBindValue(tableId);
        success = success && query.exec();
    }

    // 提交或回滚事务
    if (success)
    {
        db.commit();
    }
    else
    {
        db.rollback();
        errorMessage = "数据库操作失败：" + query.lastError().text();
    }

    if (success)
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
            // 如果没有剩余表，清空表格模型并显示欢迎界面
            if (m_vectorTableModel)
            {
                m_vectorTableModel->cleanup();
            }
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

void MainWindow::addVectorTableTab(int tableId, const QString &tableName)
{
    qDebug() << "--- [Add Tab Start] 开始添加向量表Tab页签:" << tableName << "ID:" << tableId << "---";

    try {
        // 创建新的部件
        qDebug() << "--- [Add Tab Step] 创建新的Tab部件";
        QWidget* newTabWidget = new QWidget();

    // 添加到Tab页签
        qDebug() << "--- [Add Tab Step] 调用addTab添加标签页";
        int index = m_vectorTabWidget->addTab(newTabWidget, tableName);
        qDebug() << "--- [Add Tab Step] Tab添加成功，索引:" << index;

    // 存储映射关系
        qDebug() << "--- [Add Tab Step] 保存Tab索引和表ID的映射关系";
    m_tabToTableId[index] = tableId;
        qDebug() << "--- [Add Tab End] Tab页添加成功 ---";
    } catch (const std::exception& e) {
        qDebug() << "--- [Add Tab Error] 添加Tab页时发生异常:" << e.what() << "---";
    } catch (...) {
        qDebug() << "--- [Add Tab Error] 添加Tab页时发生未知异常 ---";
    }
}

// 打开指定ID的向量表
void MainWindow::openVectorTable(int tableId, const QString &tableName)
{
    qDebug() << "MainWindow::openVectorTable - 打开向量表:" << tableName << "ID:" << tableId;

    // 检查表是否已经打开
    int existingTabIndex = -1;
    for (auto it = m_tabToTableId.begin(); it != m_tabToTableId.end(); ++it)
    {
        if (it.value() == tableId)
        {
            existingTabIndex = it.key();
            break;
        }
    }

    if (existingTabIndex >= 0 && existingTabIndex < m_vectorTabWidget->count())
    {
        // 表已经打开，切换到对应的Tab
        qDebug() << "MainWindow::openVectorTable - 表已打开，切换到Tab索引:" << existingTabIndex;
        m_vectorTabWidget->setCurrentIndex(existingTabIndex);
        return;
    }

    // 表未打开，添加到下拉框和Tab
    // 检查表是否已经在下拉框中
    int comboIndex = m_vectorTableSelector->findData(tableId);
    if (comboIndex < 0)
    {
        // 表不在下拉框中，添加它
        qDebug() << "MainWindow::openVectorTable - 向下拉框添加表:" << tableName;
        m_vectorTableSelector->addItem(tableName, tableId);
        comboIndex = m_vectorTableSelector->count() - 1;
    }

    // 添加Tab页签
    qDebug() << "MainWindow::openVectorTable - 添加Tab页签:" << tableName;
    addVectorTableTab(tableId, tableName);

    // 选中新添加的表
    qDebug() << "MainWindow::openVectorTable - 选中表在下拉框中的索引:" << comboIndex;
    m_vectorTableSelector->setCurrentIndex(comboIndex);

    // 确保向量表窗口可见
    if (m_welcomeWidget->isVisible())
    {
        m_welcomeWidget->setVisible(false);
        m_vectorTableContainer->setVisible(true);
    }
}


