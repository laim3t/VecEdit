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
    QString schemaPath = ":/db/schema.sql";

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
    qDebug() << "MainWindow::addVectorTableTab - 添加向量表Tab页签:" << tableName;

    // 添加到Tab页签
    int index = m_vectorTabWidget->addTab(new QWidget(), tableName);

    // 存储映射关系
    m_tabToTableId[index] = tableId;
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
