#include "mainwindow.h"
#include "database/databasemanager.h"
#include "database/databaseviewdialog.h"
#include "pin/pinlistdialog.h"
#include "timeset/timesetdialog.h"
#include "timeset/filltimesetdialog.h"
#include "timeset/replacetimesetdialog.h"
#include "pin/pinvalueedit.h"
#include "vector/vectortabledelegate.h"
#include "vector/vectordatahandler.h"
#include "common/dialogmanager.h"
#include "pin/vectorpinsettingsdialog.h"
#include "pin/pinsettingsdialog.h"
#include "vector/deleterangevectordialog.h"
#include "common/tablestylemanager.h"
#include "common/binary_file_format.h"
#include "database/binaryfilehelper.h"
#include "common/utils/pathutils.h" // 修正包含路径

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QStandardPaths>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QLabel>
#include <QDebug>
#include <QStatusBar>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTableWidgetItem>
#include <QFont>
#include <QStyledItemDelegate>
#include <QComboBox>
#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QKeyEvent>
#include <QIcon>
#include <QFileInfo>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QSpinBox>
#include <QScrollArea>
#include <QTimer>
#include <QListWidget>
#include <QListWidgetItem>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QSplitter>
#include <QSizePolicy>
#include <QSettings>
#include <QCloseEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QToolBar>
#include <QToolButton>
#include <QVariant>
#include <QProgressDialog>
#include <QTreeView>
#include <QStandardItemModel>

#include "migration/datamigrator.h"

// 辅助函数：比较两个列配置列表，判断是否发生了可能影响二进制布局的实质性变化
// 只比较列的数量、名称和顺序。
static bool areColumnConfigurationsDifferentSimplified(const QList<Vector::ColumnInfo> &oldCols, const QList<Vector::ColumnInfo> &newCols)
{
    if (oldCols.size() != newCols.size())
    {
        return true; // 列数量不同，需要迁移
    }

    // 假设列是按 column_order 排序的
    for (int i = 0; i < oldCols.size(); ++i)
    {
        if (oldCols[i].name != newCols[i].name || oldCols[i].order != newCols[i].order)
        {
            return true; // 列名或顺序不同，需要迁移
        }
    }

    return false;
}

// 辅助函数：将旧的行数据适配到新的列结构
static QList<Vector::RowData> adaptRowDataToNewColumns(const QList<Vector::RowData> &oldData, const QList<Vector::ColumnInfo> &oldCols, const QList<Vector::ColumnInfo> &newCols)
{
    if (oldData.isEmpty())
    {
        return {}; // 如果没有旧数据，无需迁移
    }

    QList<Vector::RowData> newDataList;
    newDataList.reserve(oldData.size());

    for (const auto &oldRow : oldData)
    {
        // 为旧行数据创建一个 名称 -> 数据值 的映射，方便查找
        QMap<QString, QVariant> oldRowMap;
        for (int i = 0; i < oldCols.size(); ++i)
        {
            if (i < oldRow.size())
            {
                oldRowMap[oldCols[i].name] = oldRow[i];
            }
        }

        Vector::RowData newRow;
        for (int i = 0; i < newCols.size(); ++i)
        {
            newRow.append(QVariant());
        }

        for (int i = 0; i < newCols.size(); ++i)
        {
            const auto &newColInfo = newCols[i];

            if (oldRowMap.contains(newColInfo.name))
            {
                // 如果新列在旧数据中存在，则直接拷贝
                newRow[i] = oldRowMap[newColInfo.name];
            }
            else
            {
                // 如果是新增的列，则赋予默认值
                if (newColInfo.type == Vector::ColumnDataType::PIN_STATE_ID)
                {
                    newRow[i] = "X"; // 新增管脚列的默认值为 "X"
                }
                else
                {
                    // For other types, the default QVariant is already what we want
                    // so we don't need to do anything here.
                    // The list was already filled with default QVariants.
                    // newRow[i] = QVariant();
                }
            }
        }
        newDataList.append(newRow);
    }

    return newDataList;
}



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
        }

        // 不再重新加载当前页数据，保留用户的编辑状态
        qDebug() << funcName << " - 保存操作完成，保留用户当前的界面编辑状态";
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
    QModelIndexList selectedIndexes = m_vectorTableWidget->selectionModel()->selectedRows();
    if (selectedIndexes.isEmpty())
    {
        QMessageBox::warning(this, "删除失败", "请先选择要删除的行");
        qWarning() << funcName << " - 用户未选择任何行";
        return;
    }

    qDebug() << funcName << " - 用户选择了" << selectedIndexes.size() << "行";

    // 确认删除
    QMessageBox::StandardButton reply = QMessageBox::question(this, "确认删除",
                                                              QString("确定要删除选定的 %1 行吗？此操作不可撤销。").arg(selectedIndexes.size()),
                                                              QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes)
    {
        qDebug() << funcName << " - 用户取消了删除操作";
        return;
    }

    // 获取所有选中的行索引
    QList<int> selectedRows;
    foreach (const QModelIndex &index, selectedIndexes)
    {
        selectedRows.append(index.row());
        qDebug() << funcName << " - 选中的行索引：" << index.row();
    }

    // 使用数据处理器删除选中的行
    QString errorMessage;
    qDebug() << funcName << " - 开始调用VectorDataHandler::deleteVectorRows，参数：tableId=" << tableId << "，行数=" << selectedRows.size();
    if (VectorDataHandler::instance().deleteVectorRows(tableId, selectedRows, errorMessage))
    {
        QMessageBox::information(this, "删除成功", "已成功删除 " + QString::number(selectedRows.size()) + " 行数据");
        qDebug() << funcName << " - 删除操作成功完成";

        // 刷新表格
        onVectorTableSelectionChanged(currentIndex);
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


/**
 * @brief 为新创建的向量表添加默认的列配置
 *
 * @param tableId 向量表ID
 * @return bool 成功返回true，失败返回false
 */
bool MainWindow::addDefaultColumnConfigurations(int tableId)
{
    const QString funcName = "MainWindow::addDefaultColumnConfigurations";
    qDebug() << funcName << " - 开始为表ID " << tableId << " 添加默认列配置";

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qCritical() << funcName << " - 数据库未打开";
        return false;
    }

    db.transaction();

    try
    {
        QSqlQuery query(db);

        // 添加标准列配置
        // 1. 添加Label列
        query.prepare("INSERT INTO VectorTableColumnConfiguration "
                      "(master_record_id, column_name, column_order, column_type, data_properties) "
                      "VALUES (?, ?, ?, ?, ?)");
        query.addBindValue(tableId);
        query.addBindValue("Label");
        query.addBindValue(0);
        query.addBindValue("TEXT");
        query.addBindValue("{}");

        if (!query.exec())
        {
            throw QString("无法添加Label列: " + query.lastError().text());
        }

        // 2. 添加Instruction列
        query.prepare("INSERT INTO VectorTableColumnConfiguration "
                      "(master_record_id, column_name, column_order, column_type, data_properties) "
                      "VALUES (?, ?, ?, ?, ?)");
        query.addBindValue(tableId);
        query.addBindValue("Instruction");
        query.addBindValue(1);
        query.addBindValue("INSTRUCTION_ID");
        query.addBindValue("{}");

        if (!query.exec())
        {
            throw QString("无法添加Instruction列: " + query.lastError().text());
        }

        // 3. 添加TimeSet列
        query.prepare("INSERT INTO VectorTableColumnConfiguration "
                      "(master_record_id, column_name, column_order, column_type, data_properties) "
                      "VALUES (?, ?, ?, ?, ?)");
        query.addBindValue(tableId);
        query.addBindValue("TimeSet");
        query.addBindValue(2);
        query.addBindValue("TIMESET_ID");
        query.addBindValue("{}");

        if (!query.exec())
        {
            throw QString("无法添加TimeSet列: " + query.lastError().text());
        }

        // 4. 添加Capture列
        query.prepare("INSERT INTO VectorTableColumnConfiguration "
                      "(master_record_id, column_name, column_order, column_type, data_properties) "
                      "VALUES (?, ?, ?, ?, ?)");
        query.addBindValue(tableId);
        query.addBindValue("Capture");
        query.addBindValue(3);
        query.addBindValue("BOOLEAN");
        query.addBindValue("{}");

        if (!query.exec())
        {
            throw QString("无法添加Capture列: " + query.lastError().text());
        }

        // 5. 添加EXT列
        query.prepare("INSERT INTO VectorTableColumnConfiguration "
                      "(master_record_id, column_name, column_order, column_type, data_properties) "
                      "VALUES (?, ?, ?, ?, ?)");
        query.addBindValue(tableId);
        query.addBindValue("EXT");
        query.addBindValue(4);
        query.addBindValue("TEXT");
        query.addBindValue("{}");

        if (!query.exec())
        {
            throw QString("无法添加EXT列: " + query.lastError().text());
        }

        // 6. 添加Comment列
        query.prepare("INSERT INTO VectorTableColumnConfiguration "
                      "(master_record_id, column_name, column_order, column_type, data_properties) "
                      "VALUES (?, ?, ?, ?, ?)");
        query.addBindValue(tableId);
        query.addBindValue("Comment");
        query.addBindValue(5);
        query.addBindValue("TEXT");
        query.addBindValue("{}");

        if (!query.exec())
        {
            throw QString("无法添加Comment列: " + query.lastError().text());
        }

        // 更新主记录的列数
        query.prepare("UPDATE VectorTableMasterRecord SET column_count = ? WHERE id = ?");
        query.addBindValue(6); // 六个标准列
        query.addBindValue(tableId);

        if (!query.exec())
        {
            throw QString("无法更新主记录的列数: " + query.lastError().text());
        }

        // 提交事务
        if (!db.commit())
        {
            throw QString("无法提交事务: " + db.lastError().text());
        }

        qDebug() << funcName << " - 成功添加默认列配置";
        return true;
    }
    catch (const QString &error)
    {
        qCritical() << funcName << " - 错误: " << error;
        db.rollback();
        return false;
    }
}

/**
 * @brief 修复没有列配置的现有向量表
 *
 * 这是一个一次性工具方法，用于修复数据库中已经存在但缺少列配置的向量表
 *
 * @param tableId 要修复的向量表ID
 * @return bool 成功返回true，失败返回false
 */
bool MainWindow::fixExistingTableWithoutColumns(int tableId)
{
    const QString funcName = "MainWindow::fixExistingTableWithoutColumns";
    qDebug() << funcName << " - 开始修复表ID " << tableId << " 的列配置";

    // 首先检查这个表是否已经有列配置
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qCritical() << funcName << " - 数据库未打开";
        return false;
    }

    // 检查是否有列配置
    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT COUNT(*) FROM VectorTableColumnConfiguration WHERE master_record_id = ?");
    checkQuery.addBindValue(tableId);

    if (!checkQuery.exec() || !checkQuery.next())
    {
        qCritical() << funcName << " - 无法检查表ID " << tableId << " 的列配置: " << checkQuery.lastError().text();
        return false;
    }

    int columnCount = checkQuery.value(0).toInt();
    if (columnCount > 0)
    {
        qDebug() << funcName << " - 表ID " << tableId << " 已有 " << columnCount << " 个列配置，不需要修复";
        return true; // 已有列配置，不需要修复
    }

    // 添加默认列配置
    if (!addDefaultColumnConfigurations(tableId))
    {
        qCritical() << funcName << " - 无法为表ID " << tableId << " 添加默认列配置";
        return false;
    }

    // 查询与这个表关联的管脚
    QSqlQuery pinQuery(db);
    pinQuery.prepare("SELECT vtp.id, pl.pin_name, vtp.pin_id, vtp.pin_channel_count, vtp.pin_type "
                     "FROM vector_table_pins vtp "
                     "JOIN pin_list pl ON vtp.pin_id = pl.id "
                     "WHERE vtp.table_id = ?");
    pinQuery.addBindValue(tableId);

    if (!pinQuery.exec())
    {
        qCritical() << funcName << " - 无法查询表ID " << tableId << " 的管脚: " << pinQuery.lastError().text();
        return false;
    }

    // 设置事务
    db.transaction();

    try
    {
        // 从标准列开始，所以列序号从6开始（0-5已经由addDefaultColumnConfigurations添加）
        int columnOrder = 6;

        while (pinQuery.next())
        {
            int pinTableId = pinQuery.value(0).toInt();
            QString pinName = pinQuery.value(1).toString();
            int pinId = pinQuery.value(2).toInt();
            int channelCount = pinQuery.value(3).toInt();
            int pinType = pinQuery.value(4).toInt();

            // 构造JSON属性字符串
            QString jsonProps = QString("{\"pin_list_id\": %1, \"channel_count\": %2, \"type_id\": %3}")
                                    .arg(pinId)
                                    .arg(channelCount)
                                    .arg(pinType);

            // 添加管脚列配置
            QSqlQuery colInsertQuery(db);
            colInsertQuery.prepare("INSERT INTO VectorTableColumnConfiguration "
                                   "(master_record_id, column_name, column_order, column_type, data_properties) "
                                   "VALUES (?, ?, ?, ?, ?)");
            colInsertQuery.addBindValue(tableId);
            colInsertQuery.addBindValue(pinName);
            colInsertQuery.addBindValue(columnOrder++);
            colInsertQuery.addBindValue("PIN_STATE_ID");
            colInsertQuery.addBindValue(jsonProps);

            if (!colInsertQuery.exec())
            {
                throw QString("无法添加管脚列配置: " + colInsertQuery.lastError().text());
            }

            qDebug() << funcName << " - 已成功为表ID " << tableId << " 添加管脚列 " << pinName;
        }

        // 更新表的总列数
        QSqlQuery updateColumnCountQuery(db);
        updateColumnCountQuery.prepare("UPDATE VectorTableMasterRecord SET column_count = ? WHERE id = ?");
        updateColumnCountQuery.addBindValue(columnOrder); // 总列数
        updateColumnCountQuery.addBindValue(tableId);

        if (!updateColumnCountQuery.exec())
        {
            throw QString("无法更新列数: " + updateColumnCountQuery.lastError().text());
        }

        // 提交事务
        if (!db.commit())
        {
            throw QString("无法提交事务: " + db.lastError().text());
        }

        qDebug() << funcName << " - 成功修复表ID " << tableId << " 的列配置，共添加了 " << columnOrder << " 列";
        return true;
    }
    catch (const QString &error)
    {
        qCritical() << funcName << " - 错误: " << error;
        db.rollback();
        return false;
    }
}

void MainWindow::checkAndFixAllVectorTables()
{
    const QString funcName = "MainWindow::checkAndFixAllVectorTables";
    qDebug() << funcName << " - 开始检查和修复所有向量表的列配置";

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qCritical() << funcName << " - 数据库未打开";
        return;
    }

    // 获取所有向量表主记录
    QSqlQuery query(db);
    query.prepare("SELECT id FROM VectorTableMasterRecord");

    if (!query.exec())
    {
        qCritical() << funcName << " - 无法查询向量表主记录:" << query.lastError().text();
        return;
    }

    int fixedCount = 0;
    int totalCount = 0;

    while (query.next())
    {
        int tableId = query.value(0).toInt();
        totalCount++;

        // 检查是否有列配置
        QSqlQuery checkQuery(db);
        checkQuery.prepare("SELECT COUNT(*) FROM VectorTableColumnConfiguration WHERE master_record_id = ?");
        checkQuery.addBindValue(tableId);

        if (!checkQuery.exec() || !checkQuery.next())
        {
            qWarning() << funcName << " - 无法检查表ID " << tableId << " 的列配置:" << checkQuery.lastError().text();
            continue;
        }

        int columnCount = checkQuery.value(0).toInt();
        if (columnCount == 0)
        {
            qDebug() << funcName << " - 表ID " << tableId << " 没有列配置，尝试修复";
            if (fixExistingTableWithoutColumns(tableId))
            {
                fixedCount++;
                qDebug() << funcName << " - 成功修复表ID " << tableId << " 的列配置";
            }
            else
            {
                qWarning() << funcName << " - 修复表ID " << tableId << " 的列配置失败";
            }
        }
        else
        {
            qDebug() << funcName << " - 表ID " << tableId << " 有 " << columnCount << " 个列配置，不需要修复";
        }
    }

    qDebug() << funcName << " - 检查完成，共 " << totalCount << " 个表，修复了 " << fixedCount << " 个表";
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

// 辅助方法：从数据库加载向量表元数据
bool MainWindow::loadVectorTableMeta(int tableId, QString &binFileName, QList<Vector::ColumnInfo> &columns, int &schemaVersion, int &rowCount)
{
    const QString funcName = "MainWindow::loadVectorTableMeta";
    qDebug() << funcName << " - 查询表ID:" << tableId;
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << funcName << " - 数据库未打开";
        return false;
    }
    // 1. 查询主记录表
    QSqlQuery metaQuery(db);
    metaQuery.prepare("SELECT binary_data_filename, data_schema_version, row_count FROM VectorTableMasterRecord WHERE id = ?");
    metaQuery.addBindValue(tableId);
    if (!metaQuery.exec() || !metaQuery.next())
    {
        qWarning() << funcName << " - 查询主记录失败, 表ID:" << tableId << ", 错误:" << metaQuery.lastError().text();
        return false;
    }
    binFileName = metaQuery.value(0).toString();
    schemaVersion = metaQuery.value(1).toInt();
    rowCount = metaQuery.value(2).toInt();
    qDebug() << funcName << " - 文件名:" << binFileName << ", schemaVersion:" << schemaVersion << ", rowCount:" << rowCount;

    // 2. 查询列结构 - 只加载IsVisible=1的列
    QSqlQuery colQuery(db);
    colQuery.prepare("SELECT id, column_name, column_order, column_type, data_properties, IsVisible FROM VectorTableColumnConfiguration WHERE master_record_id = ? AND IsVisible = 1 ORDER BY column_order");
    colQuery.addBindValue(tableId);
    if (!colQuery.exec())
    {
        qWarning() << funcName << " - 查询列结构失败, 错误:" << colQuery.lastError().text();
        return false;
    }

    columns.clear();
    while (colQuery.next())
    {
        Vector::ColumnInfo col;
        col.id = colQuery.value(0).toInt();
        col.vector_table_id = tableId;
        col.name = colQuery.value(1).toString();
        col.order = colQuery.value(2).toInt();
        col.original_type_str = colQuery.value(3).toString();
        col.type = Vector::columnDataTypeFromString(col.original_type_str);
        col.is_visible = colQuery.value(5).toBool();

        QString propStr = colQuery.value(4).toString();
        if (!propStr.isEmpty())
        {
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(propStr.toUtf8(), &err);
            qDebug().nospace() << funcName << " - JSON Parsing Details for Column: '" << col.name
                               << "', Input: '" << propStr
                               << "', ErrorCode: " << err.error
                               << " (ErrorStr: " << err.errorString()
                               << "), IsObject: " << doc.isObject();

            if (err.error == QJsonParseError::NoError && doc.isObject())
            {
                col.data_properties = doc.object();
            }
            else
            {
                qWarning().nospace() << funcName << " - 列属性JSON解析判定为失败 (条件分支), 列: '" << col.name
                                     << "', Input: '" << propStr
                                     << "', ErrorCode: " << err.error
                                     << " (ErrorStr: " << err.errorString()
                                     << "), IsObject: " << doc.isObject();
            }
        }
        columns.append(col);
    }
    return true;
}

// Add implementation for reloadAndRefreshVectorTable
void MainWindow::reloadAndRefreshVectorTable(int tableId)
{
    const QString funcName = "MainWindow::reloadAndRefreshVectorTable";
    qDebug() << funcName << "- Reloading and refreshing UI for table ID:" << tableId;

    // 首先清除表格数据缓存，确保获取最新数据
    VectorDataHandler::instance().clearTableDataCache(tableId);

    // 1. Ensure the table is selected in the ComboBox and TabWidget
    int comboBoxIndex = m_vectorTableSelector->findData(tableId);
    if (comboBoxIndex != -1)
    {
        if (m_vectorTableSelector->currentIndex() != comboBoxIndex)
        {
            m_vectorTableSelector->setCurrentIndex(comboBoxIndex); // This should trigger onVectorTableSelectionChanged
        }
        else
        {
            // If it's already selected, force a refresh of its data
            onVectorTableSelectionChanged(comboBoxIndex);
        }
    }
    else
    {
        qWarning() << funcName << "- Table ID" << tableId << "not found in selector. Cannot refresh.";
        // Fallback: reload all tables if the specific one isn't found (might be a new table not yet in UI)
        loadVectorTable();
    }

    // 2. Refresh the sidebar (in case table names or other project components changed)
    refreshSidebarNavigator();
}

// Add this new function implementation
QList<Vector::ColumnInfo> MainWindow::getCurrentColumnConfiguration(int tableId)
{
    const QString funcName = "MainWindow::getCurrentColumnConfiguration";
    QList<Vector::ColumnInfo> columns;
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << funcName << "- 数据库未打开";
        return columns;
    }

    QSqlQuery colQuery(db);
    // 获取所有列，无论是否可见，因为迁移需要处理所有物理列
    colQuery.prepare("SELECT id, column_name, column_order, column_type, data_properties, IsVisible "
                     "FROM VectorTableColumnConfiguration WHERE master_record_id = ? ORDER BY column_order");
    colQuery.addBindValue(tableId);

    if (!colQuery.exec())
    {
        qWarning() << funcName << "- 查询列结构失败, 表ID:" << tableId << ", 错误:" << colQuery.lastError().text();
        return columns;
    }

    while (colQuery.next())
    {
        Vector::ColumnInfo col;
        col.id = colQuery.value(0).toInt(); // This is the id from VectorTableColumnConfiguration table
        col.vector_table_id = tableId;      // The master_record_id it belongs to
        col.name = colQuery.value(1).toString();
        col.order = colQuery.value(2).toInt();
        col.original_type_str = colQuery.value(3).toString();               // Store the original string
        col.type = Vector::columnDataTypeFromString(col.original_type_str); // Convert to enum

        QString propStr = colQuery.value(4).toString();
        if (!propStr.isEmpty())
        {
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(propStr.toUtf8(), &err);
            if (err.error == QJsonParseError::NoError && doc.isObject())
            {
                col.data_properties = doc.object();
            }
            else
            {
                qWarning() << funcName << " - 解析列属性JSON失败 for column " << col.name << ", error: " << err.errorString();
            }
        }
        col.is_visible = colQuery.value(5).toBool();
        columns.append(col);
    }
    qDebug() << funcName << "- 为表ID:" << tableId << "获取了" << columns.size() << "列配置。";
    return columns;
}

// Add this new function implementation
bool MainWindow::areColumnConfigurationsDifferentSimplified(const QList<Vector::ColumnInfo> &config1, const QList<Vector::ColumnInfo> &config2)
{
    if (config1.size() != config2.size())
    {
        qDebug() << "areColumnConfigurationsDifferentSimplified: Sizes differ - config1:" << config1.size() << "config2:" << config2.size();
        return true;
    }

    QSet<QString> names1, names2;
    for (const auto &col : config1)
    {
        names1.insert(col.name);
    }
    for (const auto &col : config2)
    {
        names2.insert(col.name);
    }

    if (names1 != names2)
    {
        qDebug() << "areColumnConfigurationsDifferentSimplified: Names differ.";
        qDebug() << "Config1 names:" << names1;
        qDebug() << "Config2 names:" << names2;
        return true;
    }

    // Optional: Check for type changes for same-named columns if necessary for your definition of "different"
    // For now, just size and name set equality is enough for "simplified"

    return false; // Configurations are considered the same by this simplified check
}

// Add this new function implementation
QList<Vector::RowData> MainWindow::adaptRowDataToNewColumns(const QList<Vector::RowData> &oldRowDataList,
                                                            const QList<Vector::ColumnInfo> &oldColumns,
                                                            const QList<Vector::ColumnInfo> &newColumns)
{
    const QString funcName = "MainWindow::adaptRowDataToNewColumns";
    QList<Vector::RowData> newRowDataList;
    newRowDataList.reserve(oldRowDataList.size());

    // Create a map of old column names to their index for efficient lookup
    QMap<QString, int> oldColumnNameToIndex;
    for (int i = 0; i < oldColumns.size(); ++i)
    {
        oldColumnNameToIndex[oldColumns[i].name] = i;
    }

    // Create a map of old column names to their type for type-aware default values (optional enhancement)
    QMap<QString, Vector::ColumnDataType> oldColumnNameToType;
    for (const auto &col : oldColumns)
    {
        oldColumnNameToType[col.name] = col.type;
    }

    for (const Vector::RowData &oldRow : oldRowDataList)
    {
        // Vector::RowData newRow(newColumns.size()); // Initialize newRow with newColumns.size() default QVariants - Incorrect
        Vector::RowData newRow;
        newRow.reserve(newColumns.size()); // Good practice to reserve if size is known

        for (int colIdx = 0; colIdx < newColumns.size(); ++colIdx)
        { // Changed loop variable name for clarity
            const Vector::ColumnInfo &newCol = newColumns[colIdx];
            QVariant cellData; // Temporary variable to hold the data for the current cell

            if (oldColumnNameToIndex.contains(newCol.name))
            {
                // Column exists in old data, copy it
                int oldIndex = oldColumnNameToIndex[newCol.name];
                if (oldIndex >= 0 && oldIndex < oldRow.size())
                { // Bounds check
                    cellData = oldRow[oldIndex];
                }
                else
                {
                    qWarning() << funcName << "- Old column index out of bounds for column:" << newCol.name << "oldIndex:" << oldIndex << "oldRow.size:" << oldRow.size();
                    if (newCol.type == Vector::ColumnDataType::PIN_STATE_ID)
                    {
                        cellData = "X";
                    }
                    else
                    {
                        cellData = QVariant();
                    }
                }
            }
            else
            {
                // Column is new, fill with default value
                if (newCol.type == Vector::ColumnDataType::PIN_STATE_ID)
                {
                    cellData = "X";
                }
                else if (newCol.type == Vector::ColumnDataType::TEXT)
                {
                    cellData = QString();
                }
                else if (newCol.type == Vector::ColumnDataType::INTEGER ||
                         newCol.type == Vector::ColumnDataType::INSTRUCTION_ID ||
                         newCol.type == Vector::ColumnDataType::TIMESET_ID)
                {
                    cellData = 0;
                }
                else if (newCol.type == Vector::ColumnDataType::REAL)
                {
                    cellData = 0.0;
                }
                else if (newCol.type == Vector::ColumnDataType::BOOLEAN)
                {
                    cellData = false;
                }
                else
                {
                    qWarning() << funcName << "- Unhandled new column type for default value:" << newCol.name << "type:" << newCol.original_type_str;
                    cellData = QVariant();
                }
            }
            newRow.append(cellData); // Append the determined cell data
        }
        newRowDataList.append(newRow);
    }
    qDebug() << funcName << "- Data adaptation complete. Processed" << oldRowDataList.size() << "rows, produced" << newRowDataList.size() << "rows for new structure.";
    return newRowDataList;
}

