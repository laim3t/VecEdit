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
#include "vector/fillvectordialog.h"
#include "vector/addrowdialog.h"
#include "common/utils/pathutils.h"

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

    // 保存结果变量
    bool saveSuccess = false;
    QString errorMessage;

    // 根据当前使用的视图类型选择不同的保存方法
    if (m_vectorStackedWidget->currentWidget() == m_vectorTableView)
    {
        // 使用Model/View架构保存数据
        if (m_vectorTableModel)
        {
            saveSuccess = m_vectorTableModel->saveData(errorMessage);
        }
        else
        {
            errorMessage = "数据模型未初始化";
            saveSuccess = false;
        }
    }
    else
    {
        // 使用旧的QTableWidget方式保存数据
        // 获取当前选择的QTableWidget
        QWidget *currentTabWidget = m_vectorTabWidget->currentWidget();
        QTableWidget *targetTableWidget = nullptr;

        if (currentTabWidget)
        {
            // 查找当前Tab中的QTableWidget
            targetTableWidget = currentTabWidget->findChild<QTableWidget *>();
        }

        if (!targetTableWidget)
        {
            // 回退到主表格控件
            targetTableWidget = m_vectorTableWidget;
            qDebug() << funcName << " - 未找到当前Tab页中的TableWidget, 回退到 m_vectorTableWidget";
        }

        if (!targetTableWidget)
        {
            QMessageBox::critical(this, "保存失败", "无法确定要保存的表格控件。");
            qCritical() << funcName << " - 无法确定要保存的表格控件。";
            savingDialog.close();
            return;
        }
        qDebug() << funcName << " - 目标表格控件已确定。";

        // 保存当前表格的状态信息
        int currentPage = m_currentPage;
        int pageSize = m_pageSize;

        // 性能优化：检查是否在分页模式下，是否有待保存的修改
        if (m_totalRows > pageSize)
        {
            qDebug() << funcName << " - 检测到分页模式，准备保存数据";

            // 优化：直接传递分页信息到VectorDataHandler，避免创建临时表格
            if (m_useNewDataHandler)
            {
                saveSuccess = m_robustDataHandler->saveVectorTableDataPaged(
                    tableId,
                    targetTableWidget,
                    currentPage,
                    pageSize,
                    m_totalRows,
                    errorMessage);
            }
            else
            {
                saveSuccess = VectorDataHandler::instance().saveVectorTableDataPaged(
                    tableId,
                    targetTableWidget,
                    currentPage,
                    pageSize,
                    m_totalRows,
                    errorMessage);
            }
        }
        else
        {
            // 非分页模式，统一使用 saveVectorTableDataPaged 进行保存
            qDebug() << funcName << " - 非分页模式，但仍调用 saveVectorTableDataPaged 以启用增量更新尝试";
            int currentRowCount = targetTableWidget ? targetTableWidget->rowCount() : 0;
            if (m_useNewDataHandler)
            {
                saveSuccess = m_robustDataHandler->saveVectorTableDataPaged(
                    tableId,
                    targetTableWidget,
                    0,               // currentPage 设为 0
                    currentRowCount, // pageSize 设为当前行数
                    currentRowCount, // totalRows 设为当前行数
                    errorMessage);
            }
            else
            {
                saveSuccess = VectorDataHandler::instance().saveVectorTableDataPaged(
                    tableId,
                    targetTableWidget,
                    0,               // currentPage 设为 0
                    currentRowCount, // pageSize 设为当前行数
                    currentRowCount, // totalRows 设为当前行数
                    errorMessage);
            }
        }
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
            if (m_useNewDataHandler)
            {
                m_robustDataHandler->clearModifiedRows(tableId);
            }
            else
            {
                VectorDataHandler::instance().clearModifiedRows(tableId);
            }

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

    // 根据当前视图类型选择不同的添加行方法
    if (m_vectorStackedWidget->currentWidget() == m_vectorTableView)
    {
        // 使用Model/View架构添加行
        addRowToCurrentVectorTableModel();
    }
    else
    {
        // 使用旧的QTableWidget方式添加行
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
}

// 为当前选中的向量表添加行(Model/View架构)
void MainWindow::addRowToCurrentVectorTableModel()
{
    if (!m_vectorTableModel)
    {
        QMessageBox::warning(this, "错误", "数据模型未初始化。");
        return;
    }

    // 创建并显示新的、轻量级的添加行对话框
    AddRowDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted)
    {
        // 获取用户输入的行数
        int rowCount = dialog.getRowCount();
        if (rowCount <= 0)
        {
            return; // 用户输入了无效的行数
        }

        // 获取当前选中的行，如果没有选中行，则在末尾添加
        int insertionRow = m_vectorTableModel->rowCount();
        QModelIndexList selectedRows = m_vectorTableView->selectionModel()->selectedRows();
        if (!selectedRows.isEmpty())
        {
            // 在第一个选中行的位置插入
            insertionRow = selectedRows.first().row();
        }

        qDebug() << "MainWindow::addRowToCurrentVectorTableModel - "
                 << "准备在第" << insertionRow << "行插入" << rowCount << "行";

        // 调用模型的insertRows方法
        // 模型将负责处理数据持久化和通知视图更新
        if (!m_vectorTableModel->insertRows(insertionRow, rowCount))
        {
            QMessageBox::critical(this, "错误", "添加行失败。请检查日志获取详细信息。");
        }
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

    // 获取选中的行
    QSet<int> selectedRowSet;

    // 根据当前视图类型获取选中的行
    if (m_vectorStackedWidget->currentWidget() == m_vectorTableView)
    {
        // 使用Model/View架构获取选中的行
        QModelIndexList selectedIndexes = m_vectorTableView->selectionModel()->selectedRows();

        // 如果selectedRows()没有返回结果，则尝试selectedIndexes()
        if (selectedIndexes.isEmpty())
        {
            selectedIndexes = m_vectorTableView->selectionModel()->selectedIndexes();
        }

        // 从获取到的索引中提取行号
        foreach (const QModelIndex &index, selectedIndexes)
        {
            selectedRowSet.insert(index.row());
        }
    }
    else
    {
        // 使用旧的QTableWidget方式获取选中的行
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

    // 根据当前视图类型执行删除操作
    QString errorMessage;
    bool success = false;

    if (m_vectorStackedWidget->currentWidget() == m_vectorTableView)
    {
        // 使用Model/View架构删除行
        if (m_vectorTableModel)
        {
            // 添加当前页偏移以获取绝对行索引
            QList<int> absoluteRows;
            int pageOffset = m_vectorTableModel->currentPage() * m_vectorTableModel->pageSize();
            for (int row : selectedRows)
            {
                absoluteRows.append(row + pageOffset);
            }

            success = m_vectorTableModel->deleteSelectedRows(absoluteRows, errorMessage);
        }
        else
        {
            errorMessage = "数据模型未初始化";
            success = false;
        }
    }
    else
    {
        // 使用旧的方式删除行
        qDebug() << funcName << " - 开始调用VectorDataHandler::deleteVectorRows，参数：tableId=" << tableId << "，行数=" << selectedRows.size();
        if (m_useNewDataHandler)
        {
            success = m_robustDataHandler->deleteVectorRows(tableId, selectedRows, errorMessage);
        }
        else
        {
            success = VectorDataHandler::instance().deleteVectorRows(tableId, selectedRows, errorMessage);
        }
    }

    // 处理删除结果
    if (success)
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
    int totalRows;
    if (m_useNewDataHandler)
    {
        totalRows = m_robustDataHandler->getVectorTableRowCount(tableId);
    }
    else
    {
        totalRows = VectorDataHandler::instance().getVectorTableRowCount(tableId);
    }
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
    QModelIndexList selectedIndexes;
    int pageOffset = 0;

    // 根据当前视图类型获取选中的行
    if (m_vectorStackedWidget->currentWidget() == m_vectorTableView)
    {
        // 使用Model/View架构获取选中的行
        selectedIndexes = m_vectorTableView->selectionModel()->selectedRows();
        pageOffset = m_vectorTableModel->currentPage() * m_vectorTableModel->pageSize();
    }
    else
    {
        // 使用旧的QTableWidget方式获取选中的行
        selectedIndexes = m_vectorTableWidget->selectionModel()->selectedRows();
        pageOffset = m_currentPage * m_pageSize;
    }

    if (!selectedIndexes.isEmpty())
    {
        if (selectedIndexes.size() == 1)
        {
            // 只选中了一行
            int row = selectedIndexes.first().row() + 1 + pageOffset; // 将0-based转为1-based并加上页偏移
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
                int row = index.row() + 1 + pageOffset; // 将0-based转为1-based并加上页偏移
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
        bool success = false;

        if (m_vectorStackedWidget->currentWidget() == m_vectorTableView)
        {
            // 使用Model/View架构删除行范围
            if (m_vectorTableModel)
            {
                success = m_vectorTableModel->deleteRowsInRange(fromRow, toRow, errorMessage);
            }
            else
            {
                errorMessage = "数据模型未初始化";
                success = false;
            }
        }
        else
        {
            // 使用旧的方式删除行范围
            if (m_useNewDataHandler)
            {
                success = m_robustDataHandler->deleteVectorRowsInRange(tableId, fromRow, toRow, errorMessage);
            }
            else
            {
                success = VectorDataHandler::instance().deleteVectorRowsInRange(tableId, fromRow, toRow, errorMessage);
            }
        }

        if (success)
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
    if (m_useNewDataHandler)
    {
        m_robustDataHandler->clearTableDataCache(tableId);
    }
    else
    {
        VectorDataHandler::instance().clearTableDataCache(tableId);
    }

    // 重新获取总行数
    if (m_useNewDataHandler)
    {
        m_totalRows = m_robustDataHandler->getVectorTableRowCount(tableId);
    }
    else
    {
        m_totalRows = VectorDataHandler::instance().getVectorTableRowCount(tableId);
    }

    // 重新计算总页数
    m_totalPages = (m_totalRows + m_pageSize - 1) / m_pageSize; // 向上取整

    // 确保当前页码在有效范围内
    if (m_currentPage < 0)
        m_currentPage = 0;
    if (m_currentPage >= m_totalPages && m_totalPages > 0)
        m_currentPage = m_totalPages - 1;

    qDebug() << funcName << " - 刷新后总行数:" << m_totalRows
             << ", 总页数:" << m_totalPages
             << ", 当前页:" << m_currentPage;

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

    // 检查是否使用新视图 (QTableView)
    bool isUsingNewView = (m_vectorStackedWidget && m_vectorStackedWidget->currentWidget() == m_vectorTableView);

    // 在新轨道下隐藏分页控件（新视图不需要手动分页功能）
    if (isUsingNewView && m_useNewDataHandler)
    {
        m_paginationWidget->setVisible(false);
        qDebug() << funcName << " - 新轨道模式，隐藏分页控件";
        return;
    }
    else
    {
        // 旧轨道模式，显示分页控件
        m_paginationWidget->setVisible(true);
    }

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
    if (m_useNewDataHandler)
    {
        m_totalRows = m_robustDataHandler->getVectorTableRowCount(tableId);
    }
    else
    {
        m_totalRows = VectorDataHandler::instance().getVectorTableRowCount(tableId);
    }

    // 计算总页数
    m_totalPages = (m_totalRows + m_pageSize - 1) / m_pageSize; // 向上取整

    // 确保当前页码在有效范围内
    if (m_currentPage < 0)
        m_currentPage = 0;
    if (m_currentPage >= m_totalPages && m_totalPages > 0)
        m_currentPage = m_totalPages - 1;

    qDebug() << funcName << " - 总行数:" << m_totalRows << "，总页数:" << m_totalPages << "，当前页:" << m_currentPage;

    // 检查是否使用新视图 (QTableView)
    bool isUsingNewView = (m_vectorStackedWidget && m_vectorStackedWidget->currentWidget() == m_vectorTableView);

    // 根据当前使用的视图类型选择不同的加载方法
    bool success = false;
    if (isUsingNewView)
    {
        // 使用Model/View架构加载数据
        qDebug() << funcName << " - 使用Model/View架构加载数据，表ID:" << tableId;
        if (m_vectorTableModel)
        {
            if (m_useNewDataHandler)
            {
                // 新轨道模式：一次性加载所有数据，忽略分页
                qDebug() << funcName << " - 新轨道模式：一次性加载所有数据";
                m_vectorTableModel->loadAllData(tableId);
            }
            else
            {
                // 旧数据处理器模式：仍然使用分页
                qDebug() << funcName << " - 旧数据处理器模式：加载页面数据";
                // 确保模型使用与MainWindow相同的页面大小
                if (m_vectorTableModel->pageSize() != m_pageSize)
                {
                    qDebug() << funcName << " - 更新模型的页面大小从" << m_vectorTableModel->pageSize() << "到" << m_pageSize;
                    // 使用新添加的setPageSize方法
                    m_vectorTableModel->setPageSize(m_pageSize);
                }
                m_vectorTableModel->loadPage(tableId, m_currentPage);
            }
            success = true; // 假设loadPage总是成功
            qDebug() << funcName << " - 新表格模型数据加载完成";
        }
        else
        {
            qWarning() << funcName << " - 表格模型未初始化，无法加载数据";
            success = false;
        }
    }
    else
    {
        // 使用旧的QTableWidget方式加载数据
        qDebug() << funcName << " - 使用QTableWidget加载数据，表ID:" << tableId << "，页码:" << m_currentPage;
        if (m_useNewDataHandler)
        {
            success = m_robustDataHandler->loadVectorTablePageData(tableId, m_vectorTableWidget, m_currentPage, m_pageSize);
        }
        else
        {
            success = VectorDataHandler::instance().loadVectorTablePageData(tableId, m_vectorTableWidget, m_currentPage, m_pageSize);
        }
    }

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
        // 在切换页面前自动保存当前页面的修改
        saveCurrentTableData();

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
        // 在切换页面前自动保存当前页面的修改
        saveCurrentTableData();

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

    // 在改变页面大小前自动保存当前页面的修改
    saveCurrentTableData();

    // 保存当前页的第一行在整个数据集中的索引
    int currentFirstRow = m_currentPage * m_pageSize;

    // 更新页面大小
    m_pageSize = newSize;

    // 计算新的页码
    m_currentPage = currentFirstRow / m_pageSize;

    // 获取当前选中的向量表ID
    int tabIndex = m_vectorTabWidget->currentIndex();
    if (tabIndex < 0 || !m_tabToTableId.contains(tabIndex))
    {
        qWarning() << funcName << " - 没有选中有效的向量表";
        return;
    }

    int tableId = m_tabToTableId[tabIndex];

    // 重新计算总页数
    if (m_useNewDataHandler)
    {
        m_totalRows = m_robustDataHandler->getVectorTableRowCount(tableId);
    }
    else
    {
        m_totalRows = VectorDataHandler::instance().getVectorTableRowCount(tableId);
    }
    m_totalPages = (m_totalRows + m_pageSize - 1) / m_pageSize; // 向上取整

    // 确保当前页码在有效范围内
    if (m_currentPage < 0)
        m_currentPage = 0;
    if (m_currentPage >= m_totalPages && m_totalPages > 0)
        m_currentPage = m_totalPages - 1;

    qDebug() << funcName << " - 更新后总行数:" << m_totalRows << "，总页数:" << m_totalPages << "，当前页:" << m_currentPage;

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

    // 保存结果变量
    QString errorMessage;
    bool saveSuccess = false;

    // 根据当前使用的视图类型选择不同的保存方法
    if (m_vectorStackedWidget->currentWidget() == m_vectorTableView)
    {
        // 使用Model/View架构保存数据
        if (m_vectorTableModel)
        {
            saveSuccess = m_vectorTableModel->saveData(errorMessage);
        }
        else
        {
            qWarning() << funcName << " - 数据模型未初始化，无法保存";
            return;
        }
    }
    else
    {
        // 使用旧的QTableWidget方式保存数据
        if (!m_vectorTableWidget)
        {
            qDebug() << funcName << " - 表格控件未初始化，不进行保存";
            return;
        }

        // 使用分页保存模式
        if (m_useNewDataHandler)
        {
            saveSuccess = m_robustDataHandler->saveVectorTableDataPaged(
                tableId,
                m_vectorTableWidget,
                m_currentPage,
                m_pageSize,
                m_totalRows,
                errorMessage);
        }
        else
        {
            saveSuccess = VectorDataHandler::instance().saveVectorTableDataPaged(
                tableId,
                m_vectorTableWidget,
                m_currentPage,
                m_pageSize,
                m_totalRows,
                errorMessage);
        }
    }

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
    if (m_useNewDataHandler)
    {
        return m_robustDataHandler->isRowModified(tableId, -1);
    }
    else
    {
        return VectorDataHandler::instance().isRowModified(tableId, -1);
    }
}
