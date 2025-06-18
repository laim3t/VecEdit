// ==========================================================
//  Headers for: mainwindow_data_operations.cpp
// ==========================================================
#include "app/mainwindow.h"

// Qt Widgets & Core
#include <QMessageBox>
#include <QTableWidget>
#include <QStatusBar>
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

// Project-specific headers
#include "database/databasemanager.h"
#include "vector/vectordatahandler.h"
#include "vector/deleterangevectordialog.h"
#include "common/dialogmanager.h"

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
    
    // 如果使用新的表格模型，数据已经实时保存
    if (m_isUsingNewTableModel && m_vectorTableModel) 
    {
        qDebug() << funcName << " - 使用优化表格模型模式，数据已经实时保存";
        
        // 清除修改标志
        m_hasUnsavedChanges = false;
        
        // 更新窗口标题
        updateWindowTitle(m_currentDbPath);
        
        // 显示消息
        statusBar()->showMessage("数据已保存");
        
        return;
    }

    // 以下代码只在传统模式下执行
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

            // 清除修改标志
            m_hasUnsavedChanges = false;

            // 清除所有修改行的标记
            VectorDataHandler::instance().clearModifiedRows(tableId);

            // 更新窗口标题
            updateWindowTitle(m_currentDbPath);
        }

        // 不再重新加载当前页数据，保留用户的编辑状态
        qDebug() << funcName << " - 保存操作完成，保留用户当前的界面编辑状态";

        // 保存成功后刷新侧边栏，确保所有标签同步
        refreshSidebarNavigator();
    }
    else
    {
        // 保存失败的情况
        QMessageBox::critical(this, "保存失败", errorMessage);
        statusBar()->showMessage("保存失败: " + errorMessage);
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

        // 刷新侧边栏导航树，确保Label同步
        refreshSidebarNavigator();
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

    // 获取选中的行（综合两种选择模式）
    QSet<int> selectedRowSet;
    QModelIndexList selectedIndexes = m_vectorTableWidget->selectionModel()->selectedRows();

    // 如果selectedRows()没有返回结果，则尝试selectedIndexes()
    if (selectedIndexes.isEmpty())
    {
        selectedIndexes = m_vectorTableWidget->selectionModel()->selectedIndexes();
    }

    // 从获取到的索引中提取行号
    foreach (const QModelIndex &index, selectedIndexes)
    {
        selectedRowSet.insert(index.row());
    }

    if (selectedRowSet.isEmpty())
    {
        QMessageBox::warning(this, "删除失败", "请先选择要删除的行");
        qWarning() << funcName << " - 用户未选择任何行";
        return;
    }

    qDebug() << funcName << " - 用户选择了" << selectedRowSet.size() << "行";

    // 确认删除
    QMessageBox::StandardButton reply = QMessageBox::question(this, "确认删除",
                                                              QString("确定要删除选定的 %1 行吗？此操作不可撤销。").arg(selectedRowSet.size()),
                                                              QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes)
    {
        qDebug() << funcName << " - 用户取消了删除操作";
        return;
    }

    // 获取所有选中的行索引
    QList<int> selectedRows = selectedRowSet.values();
    qDebug() << funcName << " - 选中的行索引：" << selectedRows;

    // 使用数据处理器删除选中的行
    QString errorMessage;
    qDebug() << funcName << " - 开始调用VectorDataHandler::deleteVectorRows，参数：tableId=" << tableId << "，行数=" << selectedRows.size();
    if (VectorDataHandler::instance().deleteVectorRows(tableId, selectedRows, errorMessage))
    {
        QMessageBox::information(this, "删除成功", "已成功删除 " + QString::number(selectedRows.size()) + " 行数据");
        qDebug() << funcName << " - 删除操作成功完成";

        // 刷新表格
        onVectorTableSelectionChanged(currentIndex);

        // 刷新侧边栏导航树，确保Label同步
        refreshSidebarNavigator();
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

            // 刷新侧边栏导航树，确保Label同步
            refreshSidebarNavigator();

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

void MainWindow::saveCurrentTableData()
{
    const QString funcName = "MainWindow::saveCurrentTableData";
    qDebug() << funcName << " - 开始保存当前页面数据";

    // 获取当前选择的向量表
    if (m_vectorTableSelector->currentIndex() < 0)
    {
        qDebug() << funcName << " - 无当前表，不进行保存";
        return;
    }

    // 获取表ID
    int tableId = m_vectorTableSelector->currentData().toInt();
    
    // 如果使用新的表格模型，数据已经实时保存
    if (m_isUsingNewTableModel && m_vectorTableModel) 
    {
        qDebug() << funcName << " - 使用优化表格模型模式，数据已经实时保存，无需额外保存";
        return;
    }
    
    // 传统模式：需要使用vectorTableWidget
    if (!m_vectorTableWidget)
    {
        qDebug() << funcName << " - 无表格控件，不进行保存";
        return;
    }

    // 保存结果变量
    QString errorMessage;

    // 使用分页保存模式
    bool saveSuccess = VectorDataHandler::instance().saveVectorTableDataPaged(
        tableId,
        m_vectorTableWidget,
        m_currentPage,
        m_pageSize,
        m_totalRows,
        errorMessage);

    if (!saveSuccess)
    {
        qWarning() << funcName << " - 保存失败: " << errorMessage;
        // 这里不弹出错误消息框，因为这是自动保存，不应打断用户操作
    }
    else
    {
        qDebug() << funcName << " - 保存成功";
    }
}

// 判断是否有未保存的内容
bool MainWindow::hasUnsavedChanges() const
{
    // 如果没有打开的数据库或没有选中的向量表，则没有未保存的内容
    if (m_currentDbPath.isEmpty() || m_vectorTableSelector->count() == 0)
    {
        return false;
    }
    
    // 直接返回内部标记，这个标记在新旧模式下都会被正确维护
    return m_hasUnsavedChanges;
    
    /* 
    // 下面是原有代码，保留作为参考
    // 获取当前选中的向量表ID
    int tableId = -1;
    int currentIdx = m_vectorTableSelector->currentIndex();
    if (currentIdx >= 0)
    {
        tableId = m_vectorTableSelector->currentData().toInt();
    }
    else
    {
        return false;
    }

    // 检查VectorDataHandler中是否有该表的修改行
    // 任何一行被修改，就表示有未保存的内容
    return VectorDataHandler::instance().isRowModified(tableId, -1);
    */
}
