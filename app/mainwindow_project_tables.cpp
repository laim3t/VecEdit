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
    // 确保使用旧的数据处理器架构
    m_useNewDataHandler = false;
    if (m_vectorTableModel)
    {
        m_vectorTableModel->setUseNewDataHandler(false);
    }

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

void MainWindow::createNewProjectWithNewArch()
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
                                                  lastPath + "/VecEditProject_NewArch.db",
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
        setWindowTitle(QString("VecEdit - 矢量测试编辑器 [新架构] [%1]").arg(QFileInfo(dbPath).fileName()));
        statusBar()->showMessage(tr("数据库创建成功(新架构): %1").arg(dbPath));

        // 设置使用新的数据处理器架构
        m_useNewDataHandler = true;
        if (m_vectorTableModel)
        {
            m_vectorTableModel->setUseNewDataHandler(true);
        }

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
                qDebug() << "MainWindow::createNewProjectWithNewArch - 检查向量表数量:" << count;
            }
        }

        if (!hasVectorTable && timeSetAdded)
        {
            qDebug() << "MainWindow::createNewProjectWithNewArch - 未找到向量表，自动显示创建向量表对话框";
            addNewVectorTable();
        }

        // 显示创建成功的消息
        QString message;
        if (pinsAdded && timeSetAdded)
        {
            message = tr("项目数据库创建成功(新架构)！管脚和TimeSet已添加。\n您可以通过\"查看\"菜单打开数据库查看器");
        }
        else if (pinsAdded)
        {
            message = tr("项目数据库创建成功(新架构)！管脚已添加。\n您可以通过\"查看\"菜单打开数据库查看器");
        }
        else
        {
            message = tr("项目数据库创建成功(新架构)！\n您可以通过\"查看\"菜单打开数据库查看器");
        }

        // 加载向量表数据
        loadVectorTable();

        // 更新菜单状态
        updateMenuState();

        // QMessageBox::information(this, tr("成功"), tr("已使用新架构创建项目，将使用RobustVectorDataHandler进行数据处理"));
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

void MainWindow::openExistingProjectWithNewArch()
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

    // 设置使用新数据处理器
    m_useNewDataHandler = true;
    if (m_vectorTableModel)
    {
        m_vectorTableModel->setUseNewDataHandler(true);
    }

    // 使用DatabaseManager打开数据库
    if (DatabaseManager::instance()->openExistingDatabase(dbPath))
    {
        m_currentDbPath = dbPath;
        setWindowTitle(QString("VecEdit - 矢量测试编辑器 (新架构) [%1]").arg(QFileInfo(dbPath).fileName()));

        // 显示当前数据库版本
        int version = DatabaseManager::instance()->getCurrentDatabaseVersion();
        statusBar()->showMessage(tr("数据库已打开(新架构): %1 (版本: %2)").arg(dbPath).arg(version));

        // 确保二进制数据目录存在
        QString binaryDataDir = Utils::PathUtils::getProjectBinaryDataDirectory(dbPath);
        QDir binDir(binaryDataDir);
        if (!binDir.exists())
        {
            binDir.mkpath(".");
            qDebug() << "创建二进制数据目录：" << binaryDataDir;
        }

        // 确保二进制文件兼容性
        QString errorMsg;
        if (!m_robustDataHandler->ensureBinaryFilesCompatibility(dbPath, errorMsg))
        {
            qWarning() << "确保二进制文件兼容性失败:" << errorMsg;
            // 这里不会中断流程，因为有些表可能没有问题
        }

        // 加载向量表数据
        loadVectorTable();

        // 检查和修复所有向量表的列配置
        checkAndFixAllVectorTables();

        // 刷新侧边导航栏
        refreshSidebarNavigator();

        // 设置窗口标题
        setWindowTitle(tr("向量编辑器 (新架构) [%1]").arg(QFileInfo(dbPath).fileName()));

        // 更新菜单状态
        updateMenuState();
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

        // 重置向量表模型状态
        if (m_vectorTableModel)
        {
            // 重置模型，使其与当前表脱离关联
            m_vectorTableModel->resetModel();
            qDebug() << "MainWindow::closeCurrentProject - 已重置向量表模型状态";
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
    const QString funcName = "MainWindow::loadVectorTable";
    qDebug() << funcName << " - 开始加载向量表";

    // 清空当前选择框
    m_vectorTableSelector->clear();

    // 清空Tab页签
    m_vectorTabWidget->clear();
    m_tabToTableId.clear();

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qDebug() << funcName << " - 错误：数据库未打开";
        statusBar()->showMessage("错误：数据库未打开");
        return;
    }

    qDebug() << funcName << " - 数据库已打开，开始查询向量表";

    // 刷新itemDelegate的缓存，确保TimeSet选项是最新的
    if (m_itemDelegate)
    {
        qDebug() << funcName << " - 刷新TimeSet选项缓存";
        m_itemDelegate->refreshCache();
    }

    // 查询所有向量表
    QSqlQuery tableQuery(db);
    if (tableQuery.exec("SELECT id, table_name FROM vector_tables ORDER BY table_name"))
    {
        qDebug() << funcName << " - 向量表查询执行成功";
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
            qDebug() << funcName << " - 找到向量表:" << tableName << "ID:" << tableId;
        }

        qDebug() << funcName << " - 总共找到" << count << "个向量表";
    }
    else
    {
        qDebug() << funcName << " - 向量表查询失败:" << tableQuery.lastError().text();
    }

    // 根据当前使用的视图和数据处理器设置分页控件的可见性
    bool isUsingNewView = (m_vectorStackedWidget && m_vectorStackedWidget->currentWidget() == m_vectorTableView);
    if (isUsingNewView && m_useNewDataHandler)
    {
        // 新轨道模式下隐藏分页控件
        if (m_paginationWidget)
        {
            m_paginationWidget->setVisible(false);
            m_prevPageButton->setVisible(false);
            m_nextPageButton->setVisible(false);
            m_pageInfoLabel->setVisible(false);
            m_pageSizeSelector->setVisible(false);
            m_pageJumper->setVisible(false);
            m_jumpButton->setVisible(false);
            qDebug() << funcName << " - 新轨道模式，隐藏所有分页控件";
        }
    }
    else
    {
        // 旧轨道或旧视图模式：显示分页控件
        if (m_paginationWidget)
        {
            m_paginationWidget->setVisible(true);
            m_prevPageButton->setVisible(true);
            m_nextPageButton->setVisible(true);
            m_pageInfoLabel->setVisible(true);
            m_pageSizeSelector->setVisible(true);
            m_pageJumper->setVisible(true);
            m_jumpButton->setVisible(true);
            qDebug() << funcName << " - 显示所有分页控件";
        }
    }

    // 如果有向量表，显示向量表窗口，否则显示欢迎窗口
    if (m_vectorTableSelector->count() > 0)
    {
        qDebug() << funcName << " - 有向量表，显示向量表窗口";
        m_welcomeWidget->setVisible(false);
        m_vectorTableContainer->setVisible(true);

        // 默认选择第一个表
        m_vectorTableSelector->setCurrentIndex(0);
    }
    else
    {
        qDebug() << funcName << " - 没有找到向量表，显示欢迎窗口";
        m_welcomeWidget->setVisible(true);
        m_vectorTableContainer->setVisible(false);
        QMessageBox::information(this, "提示", "没有找到向量表，请先创建向量表");
    }
}
