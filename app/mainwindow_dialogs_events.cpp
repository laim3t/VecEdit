// ==========================================================
//  Headers for: mainwindow_dialogs_events.cpp
// ==========================================================
#include "app/mainwindow.h"

// Qt Widgets & Core
#include <QMessageBox>
#include <QDebug>
#include <QInputDialog>
#include <QTableWidget>
#include <QMenu>
#include <QTreeWidget>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QHeaderView>
#include <QFileInfo>

// Project-specific headers
#include "database/databasemanager.h"
#include "common/dialogmanager.h"
#include "timeset/timesetdialog.h"
#include "vector/vectordatahandler.h"
#include "common/tablestylemanager.h"
#include "vector/vectortabledelegate.h"

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

    // 清空波形图管脚选择器，以便在加载表格后重新填充
    if (m_waveformPinSelector)
    {
        m_waveformPinSelector->clear();
    }

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

        // 更新波形图视图
        if (m_isWaveformVisible && m_waveformPlot)
        {
            updateWaveformView();
        }

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

// 显示管脚列的右键菜单
void MainWindow::showPinColumnContextMenu(const QPoint &pos)
{
    if (!m_vectorTableWidget)
        return;

    // 获取右键点击的单元格位置
    QTableWidgetItem *item = m_vectorTableWidget->itemAt(pos);
    if (!item)
        return;

    int row = item->row();
    int col = item->column();

    // 获取当前表ID
    int currentTableId = m_vectorTableSelector->currentData().toInt();
    QList<Vector::ColumnInfo> columns = getCurrentColumnConfiguration(currentTableId);

    // 检查是否是管脚列
    bool isPinColumn = false;
    if (col < columns.size() && columns[col].type == Vector::ColumnDataType::PIN_STATE_ID)
    {
        isPinColumn = true;
    }

    // 如果不是管脚列，不显示菜单
    if (!isPinColumn)
        return;

    QMenu contextMenu(this);

    // 添加"跳转至波形图"选项
    if (m_isWaveformVisible)
    {
        QString pinName = m_vectorTableWidget->horizontalHeaderItem(col)->text();
        QAction *jumpToWaveformAction = contextMenu.addAction(tr("跳转至波形图"));
        connect(jumpToWaveformAction, &QAction::triggered, this, [this, row, pinName]()
                { jumpToWaveformPoint(row, pinName); });

        // 添加分隔线
        contextMenu.addSeparator();
    }

    QAction *fillVectorAction = contextMenu.addAction(tr("向量填充"));
    connect(fillVectorAction, &QAction::triggered, this, [this]()
            {
        // 直接调用MainWindow的showFillVectorDialog方法
        this->showFillVectorDialog(); });

    contextMenu.exec(m_vectorTableWidget->viewport()->mapToGlobal(pos));
}

// 实现跳转到波形图指定点的函数
void MainWindow::jumpToWaveformPoint(int rowIndex, const QString &pinName)
{
    // 确保波形图是可见的
    if (!m_isWaveformVisible)
    {
        toggleWaveformView(true);
    }

    // 选择正确的管脚
    for (int i = 0; i < m_waveformPinSelector->count(); i++)
    {
        if (m_waveformPinSelector->itemData(i).toString() == pinName)
        {
            if (m_waveformPinSelector->currentIndex() != i)
            {
                m_waveformPinSelector->setCurrentIndex(i);
                // onWaveformPinSelectionChanged 会自动更新波形图
            }
            else
            {
                // 如果已经是当前选中的管脚，手动更新波形图
                updateWaveformView();
            }
            break;
        }
    }

    // 确保rowIndex在可见范围内
    if (m_waveformPlot)
    {
        double currentMin = m_waveformPlot->xAxis->range().lower;
        double currentMax = m_waveformPlot->xAxis->range().upper;
        double rangeSize = currentMax - currentMin;

        // 如果点不在当前可见范围内，调整范围
        if (rowIndex < currentMin || rowIndex > currentMax)
        {
            // 计算新的范围，使rowIndex在中间
            double newMin = qMax(0.0, rowIndex - rangeSize / 2);
            double newMax = newMin + rangeSize;

            // 确保不超过数据范围
            if (newMax > m_vectorTableWidget->rowCount())
            {
                newMax = m_vectorTableWidget->rowCount();
                newMin = qMax(0.0, newMax - rangeSize);
            }

            // 确保最小值始终为0（不显示负坐标）
            newMin = qMax(0.0, newMin);

            m_waveformPlot->xAxis->setRange(newMin, newMax);
        }

        // 高亮显示选中的点
        highlightWaveformPoint(rowIndex);
    }
}

// 更新向量列属性栏
void MainWindow::updateVectorColumnProperties(int row, int column)
{
    // 检查是否有向量表被打开
    if (!m_vectorTableWidget || m_vectorTableWidget->columnCount() == 0)
    {
        return;
    }

    // 检查是否有Tab被打开
    if (m_vectorTabWidget->count() == 0 || m_vectorTabWidget->currentIndex() < 0)
    {
        return;
    }

    // 重置16进制输入框的验证状态
    if (m_pinValueField)
    {
        m_pinValueField->setStyleSheet("");
        m_pinValueField->setToolTip("");
        m_pinValueField->setProperty("invalid", false);
    }

    // 获取当前表的列配置信息
    int currentTableId = m_tabToTableId[m_vectorTabWidget->currentIndex()];
    QList<Vector::ColumnInfo> columns = getCurrentColumnConfiguration(currentTableId);

    // 检查列索引是否有效
    if (column < 0 || column >= columns.size())
    {
        return;
    }

    // 获取列类型
    Vector::ColumnDataType colType = columns[column].type;

    // 处理管脚列
    if (colType == Vector::ColumnDataType::PIN_STATE_ID)
    {
        // 获取管脚名称（从列标题获取而不是从单元格获取）
        QTableWidgetItem *headerItem = m_vectorTableWidget->horizontalHeaderItem(column);
        if (headerItem)
        {
            // 获取表头文本并只提取第一行（管脚名称）
            QString headerText = headerItem->text();
            // 按换行符分割文本，取第一行
            QString pinName = headerText.split("\n").at(0);

            // 更新管脚名称标签
            if (m_pinNameLabel)
            {
                m_pinNameLabel->setText(pinName);
            }

            // 更新列名称旁的管脚标签
            if (m_columnNamePinLabel)
            {
                m_columnNamePinLabel->setText(pinName);
            }

            // 获取当前选中的行
            m_currentSelectedRows.clear();
            QList<QTableWidgetItem *> selectedItems = m_vectorTableWidget->selectedItems();
            foreach (QTableWidgetItem *item, selectedItems)
            {
                if (item->column() == column && !m_currentSelectedRows.contains(item->row()))
                {
                    m_currentSelectedRows.append(item->row());
                }
            }

            // 保存当前选中的列
            m_currentHexValueColumn = column;

            // 计算并显示16进制值
            calculateAndDisplayHexValue(m_currentSelectedRows, column);

            // 设置默认错误个数为0
            if (m_errorCountField)
            {
                m_errorCountField->setText("0");
            }

            // 启用连续勾选框
            if (m_continuousSelectCheckBox)
            {
                m_continuousSelectCheckBox->setEnabled(true);
            }
        }
    }
    // 处理非管脚列（清空所有显示）
    else
    {
        // 清空管脚名称标签
        if (m_pinNameLabel)
        {
            m_pinNameLabel->setText("");
        }

        // 清空列名称旁的管脚标签
        if (m_columnNamePinLabel)
        {
            m_columnNamePinLabel->setText("");
        }

        // 清空16进制值输入框
        if (m_pinValueField)
        {
            m_pinValueField->clear();
        }

        // 清空错误个数字段
        if (m_errorCountField)
        {
            m_errorCountField->setText("");
        }

        // 禁用连续勾选框
        if (m_continuousSelectCheckBox)
        {
            m_continuousSelectCheckBox->setEnabled(false);
            m_continuousSelectCheckBox->setChecked(false);
        }

        // 重置当前选中列和行
        m_currentHexValueColumn = -1;
        m_currentSelectedRows.clear();
    }
}

// 计算16进制值并显示在向量列属性栏中
void MainWindow::calculateAndDisplayHexValue(const QList<int> &selectedRows, int column)
{
    // 如果没有选择行或列无效，则直接返回
    if (selectedRows.isEmpty() || column < 0 || !m_vectorTableWidget)
    {
        if (m_pinValueField)
        {
            m_pinValueField->clear();
        }
        return;
    }

    // 只处理前8行数据
    QList<int> processRows = selectedRows;
    if (processRows.size() > 8)
    {
        // 按行号排序，确保从上到下处理
        std::sort(processRows.begin(), processRows.end());
        // 只保留前8行
        processRows = processRows.mid(0, 8);
    }

    // 收集选中单元格的内容
    QStringList cellValues;
    bool only01 = true; // 是否只包含0和1
    bool onlyHL = true; // 是否只包含H和L

    for (int row : processRows)
    {
        QTableWidgetItem *item = m_vectorTableWidget->item(row, column);
        if (!item)
            continue;

        QString cellValue = item->text().trimmed();
        cellValues.append(cellValue);

        // 检查是否只包含0和1
        if (!cellValue.isEmpty() && cellValue != "0" && cellValue != "1")
        {
            only01 = false;
        }

        // 检查是否只包含H和L
        if (!cellValue.isEmpty() && cellValue != "H" && cellValue != "L")
        {
            onlyHL = false;
        }
    }

    QString hexResult;

    // 情况A：纯0和1
    if (only01 && !cellValues.isEmpty())
    {
        QString binaryStr;
        for (const QString &value : cellValues)
        {
            binaryStr += value;
        }

        bool ok;
        int decimal = binaryStr.toInt(&ok, 2);
        if (ok)
        {
            // 根据行数决定16进制格式
            if (processRows.size() <= 4)
            {
                // 少于等于4行，不补零
                hexResult = QString("0x%1").arg(decimal, 0, 16).toUpper().replace("0X", "0x");
            }
            else
            {
                // 超过4行，格式化为两位16进制，不足补0
                hexResult = QString("0x%1").arg(decimal, 2, 16, QChar('0')).toUpper().replace("0X", "0x");
            }
        }
    }
    // 情况B：纯H和L
    else if (onlyHL && !cellValues.isEmpty())
    {
        QString binaryStr;
        for (const QString &value : cellValues)
        {
            if (value == "H")
                binaryStr += "1";
            else if (value == "L")
                binaryStr += "0";
        }

        bool ok;
        int decimal = binaryStr.toInt(&ok, 2);
        if (ok)
        {
            // 根据行数决定16进制格式
            if (processRows.size() <= 4)
            {
                // 少于等于4行，不补零
                hexResult = QString("+0x%1").arg(decimal, 0, 16).toUpper().replace("+0X", "+0x");
            }
            else
            {
                // 超过4行，格式化为两位16进制，不足补0
                hexResult = QString("+0x%1").arg(decimal, 2, 16, QChar('0')).toUpper().replace("+0X", "+0x");
            }
        }
    }
    // 情况C：混合或特殊字符
    else
    {
        // 将所有单元格值连接起来，不截断长度
        hexResult = cellValues.join("");

        // 如果结果太长，可以考虑显示提示信息，表明这是多个单元格的混合值
        if (hexResult.length() > 15)
        {
            // 超过15个字符时截断并添加省略号
            hexResult = hexResult.left(12) + "...";
        }
    }

    // 显示结果
    if (m_pinValueField)
    {
        m_pinValueField->setText(hexResult);
    }
}

// 处理16进制值编辑后的同步操作
void MainWindow::onHexValueEdited()
{
    // 获取输入的16进制值
    QString hexValue = m_pinValueField->text().trimmed();
    if (hexValue.isEmpty())
        return;

    // 如果输入无效，则不执行任何操作
    if (m_pinValueField->property("invalid").toBool())
    {
        return;
    }

    // 使用已保存的列和行信息，如果不存在则尝试获取当前选中内容
    QList<int> selectedRows = m_currentSelectedRows;
    int selectedColumn = m_currentHexValueColumn;

    // 如果没有保存的列信息或行信息，则尝试从当前选中项获取
    if (selectedColumn < 0 || selectedRows.isEmpty())
    {
        selectedRows.clear();
        bool sameColumn = true;

        QList<QTableWidgetItem *> selectedItems = m_vectorTableWidget->selectedItems();
        if (selectedItems.isEmpty())
            return;

        selectedColumn = selectedItems.first()->column();

        // 检查是否所有选择都在同一列
        for (QTableWidgetItem *item : selectedItems)
        {
            if (item->column() != selectedColumn)
            {
                sameColumn = false;
                break;
            }

            if (!selectedRows.contains(item->row()))
                selectedRows.append(item->row());
        }

        if (!sameColumn || selectedRows.isEmpty())
            return;
    }

    // 确保行按从上到下排序
    std::sort(selectedRows.begin(), selectedRows.end());

    // 获取当前表的列配置信息
    if (m_vectorTabWidget->currentIndex() < 0)
        return;

    int currentTableId = m_tabToTableId[m_vectorTabWidget->currentIndex()];
    QList<Vector::ColumnInfo> columns = getCurrentColumnConfiguration(currentTableId);

    // 检查列索引是否有效且是否为PIN_STATE_ID列
    if (selectedColumn < 0 || selectedColumn >= columns.size())
        return;

    Vector::ColumnDataType colType = columns[selectedColumn].type;
    if (colType != Vector::ColumnDataType::PIN_STATE_ID)
        return;

    // 判断格式类型和提取16进制值
    bool useHLFormat = false;
    QString hexDigits;
    bool validFormat = false;

    // 统一将输入转为小写以便处理
    QString lowerHexValue = hexValue.toLower();

    if (lowerHexValue.startsWith("+0x"))
    {
        useHLFormat = true;
        hexDigits = lowerHexValue.mid(3);
        validFormat = true;
    }
    else if (lowerHexValue.startsWith("0x"))
    {
        useHLFormat = false;
        hexDigits = lowerHexValue.mid(2);
        validFormat = true;
    }
    else
    {
        return;
    }

    if (!validFormat)
    {
        return;
    }

    QRegExp hexRegex("^[0-9a-fA-F]{1,2}$");
    if (!hexRegex.exactMatch(hexDigits))
    {
        return;
    }

    bool ok;
    int decimalValue = hexDigits.toInt(&ok, 16);
    if (!ok)
    {
        return;
    }

    // 1. 转换为8位二进制字符串
    QString binaryStr = QString::number(decimalValue, 2).rightJustified(8, '0');

    // 2. 确定要操作的行数 (最多8行)
    int rowsToChange = qMin(selectedRows.size(), 8);

    // 3. 从8位字符串中截取右边的部分
    QString finalBinaryStr = binaryStr.right(rowsToChange);

    // 4. 将最终的二进制字符串覆写到选中的单元格
    m_vectorTableWidget->blockSignals(true);

    for (int i = 0; i < finalBinaryStr.length(); ++i)
    {
        int row = selectedRows[i];
        QChar bit = finalBinaryStr[i];
        QString newValue;
        if (useHLFormat)
        {
            newValue = (bit == '1' ? "H" : "L");
        }
        else
        {
            newValue = bit;
        }

        QTableWidgetItem *item = m_vectorTableWidget->item(row, selectedColumn);
        if (item)
        {
            item->setText(newValue);
        }
        else
        {
            item = new QTableWidgetItem(newValue);
            m_vectorTableWidget->setItem(row, selectedColumn, item);
        }
    }

    m_vectorTableWidget->blockSignals(false);

    // 手动触发一次数据变更的逻辑，以便undo/redo和保存状态能够更新
    if (!selectedRows.isEmpty())
    {
        onTableRowModified(selectedRows.first());
    }

    // --- 新增：处理回车后的跳转和选择逻辑 ---

    // 1. 确定最后一个被影响的行和总行数
    int lastAffectedRow = -1;
    if (selectedRows.size() <= 8)
    {
        lastAffectedRow = selectedRows.last();
    }
    else
    {
        lastAffectedRow = selectedRows[7]; // n > 8 时，只影响前8行
    }

    int totalRowCount = m_vectorTableWidget->rowCount();
    int nextRow = lastAffectedRow + 1;

    // 如果下一行超出表格范围，则不执行任何操作
    if (nextRow >= totalRowCount)
    {
        return;
    }

    // 2. 清除当前选择
    m_vectorTableWidget->clearSelection();

    // 3. 根据"连续"模式设置新选择
    if (m_continuousSelectCheckBox && m_continuousSelectCheckBox->isChecked())
    {
        // 连续模式开启
        int selectionStartRow = nextRow;
        int selectionEndRow = qMin(selectionStartRow + 7, totalRowCount - 1);

        // 创建选择范围
        QTableWidgetSelectionRange range(selectionStartRow, m_currentHexValueColumn,
                                         selectionEndRow, m_currentHexValueColumn);
        m_vectorTableWidget->setCurrentItem(m_vectorTableWidget->item(selectionStartRow, m_currentHexValueColumn));
        m_vectorTableWidget->setRangeSelected(range, true);

        // 确保新选区可见
        m_vectorTableWidget->scrollToItem(m_vectorTableWidget->item(selectionStartRow, m_currentHexValueColumn), QAbstractItemView::PositionAtTop);

        // 将焦点设置回输入框
        m_pinValueField->setFocus();
        m_pinValueField->selectAll();
    }
    else
    {
        // 连续模式关闭
        m_vectorTableWidget->setCurrentCell(nextRow, m_currentHexValueColumn);
    }
}

void MainWindow::on_action_triggered(bool checked)
{
    // 这个槽函数当前没有具体操作，可以根据需要进行扩展
    qDebug() << "Action triggered, checked:" << checked;
}

void MainWindow::onProjectStructureItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    // 双击时展开或折叠项目
    if (item)
    {
        item->setExpanded(!item->isExpanded());
    }
}

void MainWindow::updateWindowTitle(const QString &dbPath)
{
    if (dbPath.isEmpty())
    {
        setWindowTitle(tr("向量编辑器"));
    }
    else
    {
        QFileInfo fileInfo(dbPath);
        setWindowTitle(tr("向量编辑器 - %1").arg(fileInfo.fileName()));
    }
}

// 实时验证16进制输入
void MainWindow::validateHexInput(const QString &text)
{
    // 如果为空则重置状态
    if (text.isEmpty())
    {
        m_pinValueField->setStyleSheet("");
        m_pinValueField->setToolTip("");
        m_pinValueField->setProperty("invalid", false);
        return;
    }

    // 获取当前选中的行数
    int selectedRowCount = m_currentSelectedRows.size();
    if (selectedRowCount == 0)
    {
        // 如果没有选中行，尝试从当前选择获取
        QList<QTableWidgetItem *> selectedItems = m_vectorTableWidget->selectedItems();
        if (!selectedItems.isEmpty())
        {
            QSet<int> rowSet;
            int firstColumn = selectedItems.first()->column();
            bool sameColumn = true;

            for (QTableWidgetItem *item : selectedItems)
            {
                if (item->column() != firstColumn)
                {
                    sameColumn = false;
                    break;
                }
                rowSet.insert(item->row());
            }

            if (sameColumn)
            {
                selectedRowCount = rowSet.size();
            }
        }
    }

    // 没有选中任何行，不进行验证
    if (selectedRowCount == 0)
    {
        m_pinValueField->setStyleSheet("");
        m_pinValueField->setToolTip("");
        m_pinValueField->setProperty("invalid", false);
        return;
    }

    // 如果文本长度超过6，并且不是由calculateAndDisplayHexValue函数设置的，
    // 则可能是用户正在手动输入过长的内容
    if (text.length() > 6)
    {
        // 检查是否像是用户输入的16进制值（以0x或+0x开头）
        if (text.startsWith("0x", Qt::CaseInsensitive) || text.startsWith("+0x", Qt::CaseInsensitive))
        {
            m_pinValueField->setStyleSheet("border: 2px solid red");
            m_pinValueField->setToolTip(tr("输入错误：16进制值前缀后最多只能有2位数字 (0-9, A-F)"));
            m_pinValueField->setProperty("invalid", true);
            return;
        }
        // 如果不是典型的16进制输入格式，可能是显示多个单元格的内容，不进行验证
        // 此时将验证状态设为有效
        m_pinValueField->setStyleSheet("");
        m_pinValueField->setToolTip("");
        m_pinValueField->setProperty("invalid", false);
        return;
    }

    // --- 验证逻辑简化 ---

    // 解析输入值
    QString hexDigits;
    bool validFormat = false;

    // 统一将输入转为小写以便处理
    QString lowerText = text.toLower();

    if (lowerText.startsWith("+0x"))
    {
        hexDigits = lowerText.mid(3);
        validFormat = true;
    }
    else if (lowerText.startsWith("0x"))
    {
        hexDigits = lowerText.mid(2);
        validFormat = true;
    }

    if (!validFormat)
    {
        m_pinValueField->setStyleSheet("");
        m_pinValueField->setToolTip("");
        m_pinValueField->setProperty("invalid", false);
        return;
    }

    QRegExp hexRegex("^[0-9a-f]{1,2}$");
    if (!hexRegex.exactMatch(hexDigits))
    {
        m_pinValueField->setStyleSheet("border: 2px solid red");
        m_pinValueField->setToolTip(tr("输入错误：16进制值必须是1-2位的有效16进制数字 (0-9, A-F)"));
        m_pinValueField->setProperty("invalid", true);
        return;
    }

    // 所有验证通过
    m_pinValueField->setStyleSheet("");
    m_pinValueField->setToolTip("");
    m_pinValueField->setProperty("invalid", false);
}