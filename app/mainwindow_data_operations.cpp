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
#include <QApplication>
#include <QProgressDialog>
#include <QElapsedTimer>
#include <QTimer>
#include <QVBoxLayout>

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

// 获取系统可用内存（平台相关实现）
qint64 MainWindow::getAvailableSystemMemory()
{
    qint64 availableMemory = 0;

// Windows平台实现
#ifdef Q_OS_WIN
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo))
    {
        availableMemory = static_cast<qint64>(memInfo.ullAvailPhys);
    }
    else
    {
        qWarning() << "获取可用内存失败，使用默认值";
        availableMemory = 2LL * 1024 * 1024 * 1024; // 默认2GB
    }
// Linux平台实现
#elif defined(Q_OS_LINUX)
    struct sysinfo memInfo;
    if (sysinfo(&memInfo) == 0)
    {
        availableMemory = static_cast<qint64>(memInfo.freeram) * memInfo.mem_unit;
    }
    else
    {
        qWarning() << "获取可用内存失败，使用默认值";
        availableMemory = 2LL * 1024 * 1024 * 1024; // 默认2GB
    }
// macOS平台实现
#elif defined(Q_OS_MAC)
    vm_statistics64_data_t vmStats;
    mach_msg_type_number_t infoCount = HOST_VM_INFO64_COUNT;
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vmStats, &infoCount) == KERN_SUCCESS)
    {
        availableMemory = static_cast<int64_t>(vmStats.free_count) * PAGE_SIZE;
    }
    else
    {
        qWarning() << "获取可用内存失败，使用默认值";
        availableMemory = 2LL * 1024 * 1024 * 1024; // 默认2GB
    }
#else
    // 默认返回值
    availableMemory = 2LL * 1024 * 1024 * 1024; // 假设有2GB可用内存
    qWarning() << "不支持的平台，使用默认内存值2GB";
#endif

    qDebug() << "系统可用内存:" << (availableMemory / 1024 / 1024) << "MB";
    return availableMemory;
}

// 为当前选中的向量表添加行(Model/View架构)
void MainWindow::addRowToCurrentVectorTableModel()
{
    const QString funcName = "MainWindow::addRowToCurrentVectorTableModel";

    if (!m_vectorTableModel)
    {
        qWarning() << funcName << " - 向量表模型未初始化";
        return;
    }

    // 创建并显示新的、轻量级的添加行对话框
    AddRowDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted)
    {
        int rowCount = dialog.getRowCount();
        if (rowCount <= 0)
        {
            return;
        }

        // 添加内存安全检查
        const qint64 estimatedMemoryPerRow = 200; // 每行估计内存占用（字节）
        const qint64 totalEstimatedMemory = static_cast<qint64>(rowCount) * estimatedMemoryPerRow;
        const qint64 availableMemory = getAvailableSystemMemory();
        const qint64 safetyThreshold = availableMemory * 0.7; // 70% 安全阈值

        qDebug() << funcName << " - 估计内存需求:" << (totalEstimatedMemory / 1024 / 1024) << "MB, 可用内存:"
                 << (availableMemory / 1024 / 1024) << "MB";

        // 确定插入位置
        int insertionRow = m_vectorTableModel->rowCount(); // 默认为末尾
        QModelIndexList selectedRows = m_vectorTableView->selectionModel()->selectedRows();
        if (!selectedRows.isEmpty())
        {
            insertionRow = selectedRows.first().row();
        }

        // 优化：根据系统性能动态调整批次大小
        const int optimalBatchSize = 250000; // 大幅提高批次大小，极大提升处理速度
        const int maxSafeBatchSize = qMin(static_cast<int>((safetyThreshold / estimatedMemoryPerRow) * 0.95), optimalBatchSize);
        const int batches = (rowCount + maxSafeBatchSize - 1) / maxSafeBatchSize; // 向上取整

        // 优化数据库性能
        {
            QSqlDatabase db = DatabaseManager::instance()->database();
            QSqlQuery pragmaQuery(db);
            pragmaQuery.exec("PRAGMA temp_store = MEMORY");      // 使用内存临时存储
            pragmaQuery.exec("PRAGMA journal_mode = MEMORY");    // 内存日志模式
            pragmaQuery.exec("PRAGMA synchronous = OFF");        // 关闭同步
            pragmaQuery.exec("PRAGMA cache_size = 500000");      // 极大增加缓存大小，显著提高性能
            pragmaQuery.exec("PRAGMA foreign_keys = OFF");       // 禁用外键约束
            pragmaQuery.exec("PRAGMA locking_mode = EXCLUSIVE"); // 独占锁定
        }

        int remainingRows = rowCount;
        int currentBatch = 1;
        int rowsInserted = 0;

        // 创建进度对话框，但增加更多细节
        QProgressDialog progress("添加行数据...", "取消", 0, rowCount, this);
        progress.setWindowModality(Qt::WindowModal);
        progress.setMinimumDuration(0); // 立即显示
        progress.setValue(0);
        // 设置进度条更新频率控制
        progress.setMinimumWidth(350); // 设置最小宽度，避免窗口大小变化
        progress.setAutoReset(false);
        progress.setAutoClose(false);
        progress.show();

        QElapsedTimer timer;
        timer.start();

        // 添加更新控制变量
        QElapsedTimer updateTimer;
        updateTimer.start();

        // 进度条更新平衡策略
        // 1. 对于小批量数据(< 100万行)，使用较短间隔保证流畅体验
        // 2. 对于大批量数据(>= 100万行)，使用较长间隔提高性能
        const int updateIntervalMs = (rowCount < 1000000) ? 300 : 700;

        // 优化UI更新策略：使用智能更新
        int lastDisplayedPercent = 0; // 记录上次显示的百分比

        while (remainingRows > 0 && !progress.wasCanceled())
        {
            int batchSize = qMin(maxSafeBatchSize, remainingRows);
            // 注释掉批次处理日志，提高性能
            // qDebug() << funcName << " - 批次 " << currentBatch << "/" << batches
            //          << "，添加" << batchSize << "行，起始位置:" << (insertionRow + rowsInserted);

            // 仅在更新间隔后才刷新进度对话框
            bool shouldUpdateUI = updateTimer.elapsed() >= updateIntervalMs;

            // 智能更新策略：如果进度百分比变化，也触发更新
            int currentPercent = static_cast<int>(static_cast<double>(rowsInserted) / rowCount * 100);
            shouldUpdateUI = shouldUpdateUI || (currentPercent > lastDisplayedPercent);

            if (shouldUpdateUI)
            {
                // 计算并显示估计剩余时间
                double elapsedSecs = timer.elapsed() / 1000.0;
                double rowsPerSecond = rowsInserted > 0 ? (rowsInserted / elapsedSecs) : 0;
                int estimatedSecsRemaining = rowsPerSecond > 0 ? static_cast<int>(remainingRows / rowsPerSecond) : 0;

                // 一次性更新文本和进度值
                progress.setLabelText(QString("正在添加行 (%1/%2)...\n已完成: %3/%4 行\n速度: %5行/秒\n预计剩余时间: %6秒")
                                          .arg(currentBatch)
                                          .arg(batches)
                                          .arg(rowsInserted)
                                          .arg(rowCount)
                                          .arg(static_cast<int>(rowsPerSecond))
                                          .arg(estimatedSecsRemaining));
                progress.setValue(rowsInserted);

                // 更新上次显示的百分比
                lastDisplayedPercent = currentPercent;

                // 重置更新定时器
                updateTimer.restart();

                // 处理事件，但不要过于频繁
                QApplication::processEvents();
            }

            if (m_vectorTableModel->insertRows(insertionRow + rowsInserted, batchSize))
            {
                rowsInserted += batchSize;
                remainingRows -= batchSize;
                currentBatch++;

                // 极大减少内存清理频率，每100批次才清理一次
                if (currentBatch % 100 == 0)
                {
                    // 强制执行垃圾回收
                    QApplication::processEvents();
                    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
                }
            }
            else
            {
                QMessageBox::critical(this, tr("错误"), tr("添加行批次 %1 失败！").arg(currentBatch));
                break;
            }
        }

        // 最终更新进度为100%
        progress.setValue(rowCount);
        double totalTime = timer.elapsed() / 1000.0;
#ifdef QT_DEBUG
        qDebug() << funcName << " - 成功添加共 " << rowsInserted << " 行，用时: " << totalTime << " 秒";
#endif

        if (progress.wasCanceled())
        {
            QMessageBox::information(this, tr("操作取消"),
                                     tr("添加行操作已取消。已成功添加 %1 行。").arg(rowsInserted));
        }
        else if (rowsInserted == rowCount)
        {
            QMessageBox::information(this, tr("添加成功"),
                                     tr("成功添加 %1 行数据，用时 %2 秒。").arg(rowsInserted).arg(totalTime, 0, 'f', 1));
        }
        else
        {
            QMessageBox::warning(this, tr("部分成功"),
                                 tr("只添加了 %1 行，还有 %2 行未添加。用时 %3 秒。")
                                     .arg(rowsInserted)
                                     .arg(rowCount - rowsInserted)
                                     .arg(totalTime, 0, 'f', 1));
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

    // 创建一个超时计时器，确保UI每100毫秒可以处理事件
    QElapsedTimer timer;
    timer.start();
    
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

    // 创建一个QTimer延迟加载当前页面，使UI线程可以先更新界面
    QTimer::singleShot(10, this, [this, tableId, funcName]() {
        // 重新加载当前页面数据
        loadCurrentPage();
        
        // 允许UI更新
        QCoreApplication::processEvents();
        
        // 延迟加载侧边栏导航，进一步避免UI阻塞
        QTimer::singleShot(10, this, [this, funcName]() {
            // 刷新侧边导航栏
            refreshSidebarNavigator();
            
            // 显示刷新成功消息
            statusBar()->showMessage("向量表数据已刷新", 3000); // 显示3秒
            qDebug() << funcName << " - 向量表数据刷新完成";
        });
    });
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

// 显示批量索引更新进度对话框
void MainWindow::showBatchIndexUpdateProgressDialog(int tableId, int columnIndex, QMap<int, QVariant> rowValueMap)
{
    // 创建进度对话框
    QProgressDialog *progressDialog = new QProgressDialog("正在进行批量索引更新...", "取消", 0, 100, this);
    progressDialog->setWindowTitle("批量索引更新进度");
    progressDialog->setWindowModality(Qt::WindowModal);
    progressDialog->setMinimumDuration(0);  // 立即显示
    progressDialog->setValue(0);
    
    // 添加额外信息标签
    QLabel *detailLabel = new QLabel("准备中...");
    detailLabel->setAlignment(Qt::AlignCenter);
    detailLabel->setMinimumWidth(300);
    
    // 获取进度对话框的布局，添加详情标签
    QVBoxLayout *layout = qobject_cast<QVBoxLayout*>(progressDialog->layout());
    if (layout) {
        layout->insertWidget(1, detailLabel);
    }
    
    progressDialog->show();
    QCoreApplication::processEvents();  // 确保UI更新
    
    // 获取RobustVectorDataHandler实例
    RobustVectorDataHandler *handler = &RobustVectorDataHandler::instance();
    
    // 更新详情标签的槽函数
    auto updateDetailLabel = [detailLabel, rowValueMap](int percentage) {
        int totalRows = rowValueMap.size();
        int processedRows = (percentage * totalRows) / 100;
        
        if (percentage <= 50) {
            // 批量追加阶段
            detailLabel->setText(QString("批量追加阶段: 已处理 %1 行，共 %2 行 (%3%)").
                              arg(processedRows).arg(totalRows).arg(percentage*2));
        } else {
            // 批量索引更新阶段
            detailLabel->setText(QString("批量索引更新阶段: 已处理 %1 行，共 %2 行 (%3%)").
                              arg(processedRows).arg(totalRows).arg((percentage-50)*2));
        }
    };
    
    // 连接进度信号到进度对话框的setValue槽
    QMetaObject::Connection progressConnection = 
        QObject::connect(handler, &RobustVectorDataHandler::progressUpdated, 
                    progressDialog, &QProgressDialog::setValue);
    
    // 连接进度信号到更新详情标签的槽
    QMetaObject::Connection detailConnection = 
        QObject::connect(handler, &RobustVectorDataHandler::progressUpdated, updateDetailLabel);
    
    // 执行批量更新操作
    QString errorMessage;
    bool success = handler->batchUpdateVectorColumnOptimized(tableId, columnIndex, rowValueMap, errorMessage);
    
    // 断开信号连接
    QObject::disconnect(progressConnection);
    QObject::disconnect(detailConnection);
    
    // 关闭进度对话框
    progressDialog->close();
    progressDialog->deleteLater();
    
    // 显示结果
    if (success) {
        QMessageBox::information(this, "批量更新完成", 
                               QString("成功更新了 %1 行数据").arg(rowValueMap.size()));
    } else {
        QMessageBox::critical(this, "批量更新失败", 
                            QString("更新过程中发生错误: %1").arg(errorMessage));
    }
}
