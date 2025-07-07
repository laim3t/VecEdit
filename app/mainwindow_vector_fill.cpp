// ==========================================================
//  Headers for: mainwindow_vector_fill.cpp
// ==========================================================
#include "app/mainwindow.h"

// Qt Widgets & Core
#include <QMessageBox>
#include <QTableWidget>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QFile>
#include <QProgressDialog>
#include <QSignalBlocker>
#include <QElapsedTimer>
#include <QCoreApplication>

// Project-specific headers
#include "vector/vectordatahandler.h"
#include "vector/fillvectordialog.h" // 假设存在这个对话框
#include "database/databasemanager.h"
#include "common/utils/pathutils.h"

// 显示填充向量对话框
void MainWindow::showFillVectorDialog()
{
    // 获取当前选择的向量表
    int currentIndex = m_vectorTableSelector->currentIndex();
    if (currentIndex < 0)
    {
        QMessageBox::warning(this, "操作失败", "没有选中的向量表");
        return;
    }

    // 获取表ID
    int tableId = m_vectorTableSelector->currentData().toInt();

    // 获取向量表行数
    int rowCount;
    if (m_useNewDataHandler)
    {
        rowCount = m_robustDataHandler->getVectorTableRowCount(tableId);
    }
    else
    {
        rowCount = VectorDataHandler::instance().getVectorTableRowCount(tableId);
    }
    if (rowCount <= 0)
    {
        QMessageBox::warning(this, "操作失败", "当前向量表没有数据行");
        return;
    }

    // 根据当前使用的视图类型获取选中的单元格
    QModelIndexList selectedIndexes;
    int targetColumn = -1;
    QString headerText;

    // 检查当前使用的是旧视图(QTableWidget)还是新视图(QTableView)
    if (m_vectorStackedWidget->currentIndex() == 0)
    {
        // 使用旧视图 (QTableWidget)
        selectedIndexes = m_vectorTableWidget->selectionModel()->selectedIndexes();

        if (!selectedIndexes.isEmpty())
        {
            targetColumn = selectedIndexes.first().column();
            // 获取列标题
            QTableWidgetItem *headerItem = m_vectorTableWidget->horizontalHeaderItem(targetColumn);
            headerText = headerItem ? headerItem->text() : "";
        }
    }
    else
    {
        // 使用新视图 (QTableView)
        selectedIndexes = m_vectorTableView->selectionModel()->selectedIndexes();

        if (!selectedIndexes.isEmpty())
        {
            targetColumn = selectedIndexes.first().column();
            // 获取列标题 (从模型获取)
            headerText = m_vectorTableModel->headerData(targetColumn, Qt::Horizontal).toString();
        }
    }

    if (selectedIndexes.isEmpty())
    {
        QMessageBox::warning(this, "操作失败", "请先选择要填充的单元格");
        return;
    }

    // 检查是否选择了同一列的单元格
    int firstColumn = selectedIndexes.first().column();
    bool sameColumn = true;
    for (const QModelIndex &index : selectedIndexes)
    {
        if (index.column() != firstColumn)
        {
            sameColumn = false;
            break;
        }
    }

    if (!sameColumn)
    {
        QMessageBox::warning(this, "操作失败", "请只选择同一列的单元格");
        return;
    }

    // 显示填充向量对话框
    FillVectorDialog dialog(this);
    // 设置向量表总行数
    dialog.setVectorRowCount(rowCount);

    // 获取选中单元格的值作为模式初始值
    QList<QString> selectedValues;
    int pageOffset = m_currentPage * m_pageSize; // 分页偏移

    // 找出最小和最大行号（1-based）
    int minRow = INT_MAX;
    int maxRow = 0;

    // 将选中的单元格按行号排序
    QMap<int, QString> sortedValues;

    // 根据当前视图模式获取单元格值
    if (m_vectorStackedWidget->currentIndex() == 0)
    {
        // 旧视图模式 (QTableWidget)
        foreach (const QModelIndex &index, selectedIndexes)
        {
            int rowIdx = pageOffset + index.row() + 1; // 转换为1-based的绝对行号
            minRow = qMin(minRow, rowIdx);
            maxRow = qMax(maxRow, rowIdx);

            // 获取单元格值
            QTableWidgetItem *item = m_vectorTableWidget->item(index.row(), index.column());
            QString cellValue = item ? item->text() : "";

            // 保存到排序map
            sortedValues[rowIdx] = cellValue;
        }
    }
    else
    {
        // 新视图模式 (QTableView)
        foreach (const QModelIndex &index, selectedIndexes)
        {
            int rowIdx = pageOffset + index.row() + 1; // 转换为1-based的绝对行号
            minRow = qMin(minRow, rowIdx);
            maxRow = qMax(maxRow, rowIdx);

            // 从模型获取单元格值
            QString cellValue = m_vectorTableModel->data(index, Qt::DisplayRole).toString();

            // 保存到排序map
            sortedValues[rowIdx] = cellValue;
        }
    }

    // 按顺序添加到列表
    for (auto it = sortedValues.begin(); it != sortedValues.end(); ++it)
    {
        selectedValues.append(it.value());
    }

    // 设置模式表格的初始数据
    dialog.setSelectedCellsData(selectedValues);

    // 设置范围
    if (minRow <= maxRow)
    {
        dialog.setSelectedRange(minRow, maxRow);
    }

    // 获取标签数据
    QList<QPair<QString, int>> labelRows;

    // 尝试找到Label列（通常是第一列）
    int labelColumnIndex = -1;
    for (int col = 0; col < m_vectorTableWidget->columnCount(); ++col)
    {
        QTableWidgetItem *headerItem = m_vectorTableWidget->horizontalHeaderItem(col);
        if (headerItem && (headerItem->text().toLower() == "label" || headerItem->text() == "标签"))
        {
            labelColumnIndex = col;
            break;
        }
    }

    // 如果找到了Label列，收集标签和对应的行号
    if (labelColumnIndex >= 0)
    {
        for (int row = 0; row < m_vectorTableWidget->rowCount(); ++row)
        {
            QTableWidgetItem *labelItem = m_vectorTableWidget->item(row, labelColumnIndex);
            if (labelItem && !labelItem->text().trimmed().isEmpty())
            {
                // 将UI行号转换为绝对行号（1-based）
                int absoluteRow = pageOffset + row + 1;
                labelRows.append(QPair<QString, int>(labelItem->text().trimmed(), absoluteRow));
            }
        }
    }

    // 设置标签列表到对话框
    dialog.setLabelList(labelRows);

    if (dialog.exec() == QDialog::Accepted)
    {
        // 获取循环填充模式
        QList<QString> patternValues = dialog.getPatternValues();
        if (patternValues.isEmpty())
        {
            QMessageBox::warning(this, "填充失败", "填充模式为空");
            return;
        }

        // 使用对话框中设置的范围
        int fromRow = dialog.getStartRow(); // 0-based
        int toRow = dialog.getEndRow();     // 0-based

        // 确保范围有效
        if (fromRow < 0 || toRow < fromRow || toRow >= rowCount)
        {
            QMessageBox::warning(this, "范围无效", "指定的行范围无效。");
            return;
        }

        // 创建并立即显示进度对话框，避免界面卡死
        QProgressDialog progress("正在准备填充向量数据...", "取消", 0, 100, this);
        progress.setWindowModality(Qt::WindowModal);
        progress.setMinimumDuration(0); // 立即显示
        progress.setValue(0);
        progress.show();
        QCoreApplication::processEvents(); // 确保UI更新

        // 准备要更新的行和值的映射
        QMap<int, QString> rowValueMap;
        int patternSize = patternValues.size();

        // 使用模式进行循环填充，同时更新进度
        int totalRows = toRow - fromRow + 1;
        for (int i = fromRow; i <= toRow; ++i)
        {
            int patternIndex = (i - fromRow) % patternSize;
            rowValueMap[i] = patternValues[patternIndex];

            // 每处理1000行更新一次进度
            if ((i - fromRow) % 1000 == 0 || i == toRow)
            {
                int percent = ((i - fromRow + 1) * 10) / totalRows; // 占总进度的10%
                progress.setValue(percent);
                progress.setLabelText(QString("正在准备填充数据...(%1/%2)").arg(i - fromRow + 1).arg(totalRows));
                QCoreApplication::processEvents(); // 确保UI更新

                // 检查是否取消操作
                if (progress.wasCanceled())
                {
                    return;
                }
            }
        }

        // 进度条文本更新
        progress.setLabelText("正在填充向量数据...");
        progress.setValue(10);             // 数据准备完成，占10%进度
        QCoreApplication::processEvents(); // 确保UI更新

        // 调用执行循环填充的函数，将进度对话框传递过去
        fillVectorWithPattern(rowValueMap, &progress);
    }
}

// 使用模式对向量表进行循环填充
void MainWindow::fillVectorWithPattern(const QMap<int, QString> &rowValueMap, QProgressDialog *progress)
{
    qDebug() << "向量填充 - 开始模式循环填充过程";

    // 输出填充信息
    qDebug() << "向量填充 - 填充行数: " << rowValueMap.size();

    // 根据当前使用的是新轨道还是旧轨道选择不同的实现
    if (m_useNewDataHandler)
    {
        // 使用新轨道的实现
        fillVectorWithPatternNewTrack(rowValueMap, progress);
    }
    else
    {
        // 原来旧轨道的实现
        fillVectorWithPatternOldTrack(rowValueMap);
    }
}

// 新轨道实现的填充向量功能 - 使用RobustVectorDataHandler和Model/View架构
void MainWindow::fillVectorWithPatternNewTrack(const QMap<int, QString> &rowValueMap, QProgressDialog *progress)
{
    qDebug() << "向量填充(新轨道) - 开始模式循环填充过程";

    // 获取当前选择的向量表
    int currentIndex = m_vectorTableSelector->currentIndex();
    if (currentIndex < 0)
    {
        QMessageBox::warning(this, tr("警告"), tr("请先选择一个向量表"));
        qDebug() << "向量填充(新轨道) - 未选择向量表，操作取消";
        return;
    }

    int tableId = m_vectorTableSelector->itemData(currentIndex).toInt();
    QString tableName = m_vectorTableSelector->currentText();
    qDebug() << "向量填充(新轨道) - 当前向量表ID:" << tableId << ", 名称:" << tableName;

    // 获取选中的列
    QModelIndexList selectedIndexes = m_vectorTableView->selectionModel()->selectedIndexes();
    if (selectedIndexes.isEmpty())
    {
        QMessageBox::warning(this, tr("警告"), tr("请选择要填充的单元格"));
        qDebug() << "向量填充(新轨道) - 未选择单元格，操作取消";
        return;
    }

    // 确保所有选中的单元格在同一列
    int targetColumn = selectedIndexes.first().column();
    bool sameColumn = true;
    for (const QModelIndex &index : selectedIndexes)
    {
        if (index.column() != targetColumn)
        {
            sameColumn = false;
            break;
        }
    }

    if (!sameColumn)
    {
        QMessageBox::warning(this, tr("警告"), tr("请只选择同一列的单元格"));
        qDebug() << "向量填充(新轨道) - 选择了多列单元格，操作取消";
        return;
    }

    // 获取列名
    QString targetColumnName = m_vectorTableModel->headerData(targetColumn, Qt::Horizontal).toString();
    // 如果是多行列标题，只取第一行
    targetColumnName = targetColumnName.section('\n', 0, 0);
    qDebug() << "向量填充(新轨道) - 目标列名:" << targetColumnName;

    // 方案一：延迟UI更新机制
    // 1. 暂停UI更新
    m_vectorTableView->setUpdatesEnabled(false);

    // 2. 禁用选择变更信号
    QSignalBlocker blocker(m_vectorTableView->selectionModel());

    try
    {
        // 获取当前向量表的所有列信息
        QList<Vector::ColumnInfo> columns = m_robustDataHandler->getAllColumnInfo(tableId);

        // 找到目标列在列配置中的索引
        int targetColumnIndex = -1;
        for (int i = 0; i < columns.size(); i++)
        {
            if (columns[i].name == targetColumnName)
            {
                targetColumnIndex = i;
                break;
            }
        }

        if (targetColumnIndex < 0)
        {
            throw std::runtime_error(QString("找不到对应列: %1").arg(targetColumnName).toStdString());
        }

        // 将字符串值映射转换为QVariant值映射
        QMap<int, QVariant> rowVariantMap;
        QMap<int, QString>::const_iterator it;
        for (it = rowValueMap.begin(); it != rowValueMap.end(); ++it)
        {
            rowVariantMap[it.key()] = QVariant(it.value());
        }

        // 使用传入的进度对话框而不是创建新的
        QProgressDialog *progressDialog = progress;
        bool usingExternalProgress = (progressDialog != nullptr);

        // 如果没有传入进度对话框，则创建一个新的
        if (!progressDialog)
        {
            progressDialog = new QProgressDialog("正在填充向量数据...", "取消", 0, 100, this);
            progressDialog->setWindowModality(Qt::WindowModal);
            progressDialog->setMinimumDuration(0); // 立即显示
            progressDialog->show();
        }
        else
        {
            // 使用传入的进度对话框，更新文本
            progressDialog->setLabelText("正在填充向量数据...");
        }

        // 确保UI已更新
        QCoreApplication::processEvents();

        // 连接进度信号
        connect(m_robustDataHandler, &RobustVectorDataHandler::progressUpdated,
                progressDialog, &QProgressDialog::setValue);

        // 使用优化版的批量更新方法（批量追加-批量索引更新模式）
        QString errorMsg;
        QElapsedTimer timer;
        timer.start();

        qDebug() << "向量填充(新轨道) - 开始使用批量追加-批量索引更新模式，行数：" << rowVariantMap.size();

        if (!m_robustDataHandler->batchUpdateVectorColumnOptimized(tableId, targetColumnIndex, rowVariantMap, errorMsg))
        {
            qWarning() << "向量填充(新轨道) - 批量更新失败:" << errorMsg;
            throw std::runtime_error(QString("填充失败: %1").arg(errorMsg).toStdString());
        }

        qDebug() << "向量填充(新轨道) - 批量追加-批量索引更新模式完成，耗时：" << timer.elapsed() << "ms";

        // 断开进度信号连接
        disconnect(m_robustDataHandler, &RobustVectorDataHandler::progressUpdated,
                   progressDialog, &QProgressDialog::setValue);

        // 如果是我们创建的进度对话框，需要删除它
        if (!usingExternalProgress)
        {
            delete progressDialog;
        }

        // 直接更新模型数据，而不是重新加载整个表
        int currentPage = m_currentPage;
        int pageSize = m_pageSize;

        // 只加载当前页的数据，减少数据加载量
        bool refreshSuccess = m_robustDataHandler->loadVectorTablePageDataForModel(
            tableId, m_vectorTableModel, currentPage, pageSize);

        if (!refreshSuccess)
        {
            qWarning() << "向量填充(新轨道) - 刷新表格数据失败";
        }

        // 批量选中所有行（一次性操作，而不是逐行选中）
        QItemSelection selection;
        for (auto it = rowValueMap.begin(); it != rowValueMap.end(); ++it)
        {
            int rowIdx = it.key();
            // 检查行是否在当前页面
            int pageOffset = currentPage * pageSize;

            if (rowIdx >= pageOffset && rowIdx < pageOffset + pageSize)
            {
                // 将数据库索引转换为UI表格索引
                int uiRowIdx = rowIdx - pageOffset;

                // 添加到选择集合中
                if (uiRowIdx >= 0 && uiRowIdx < m_vectorTableModel->rowCount())
                {
                    QModelIndex index = m_vectorTableModel->index(uiRowIdx, targetColumn);
                    selection.select(index, index);
                }
            }
        }

        // 3. 恢复UI更新（blocker在作用域结束时自动解除阻塞）
        m_vectorTableView->setUpdatesEnabled(true);

        // 4. 一次性设置选择状态，而不是逐行设置
        m_vectorTableView->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);

        // 5. 自动执行全局刷新，确保所有数据和UI都更新到最新状态
        qDebug() << "向量填充(新轨道) - 自动执行全局刷新";
        refreshVectorTableData();

        // 显示完成消息
        QMessageBox::information(this, tr("完成"), tr("向量填充完成"));

        qDebug() << "向量填充(新轨道) - 操作成功完成";
    }
    catch (const std::exception &e)
    {
        // 即使发生异常也要恢复UI更新
        m_vectorTableView->setUpdatesEnabled(true);

        // 尝试刷新表格，确保UI显示最新状态
        try
        {
            qDebug() << "向量填充(新轨道) - 异常后尝试刷新表格";
            refreshVectorTableData();
        }
        catch (...)
        {
            qWarning() << "向量填充(新轨道) - 异常后刷新表格失败";
        }

        QMessageBox::critical(this, tr("错误"), tr("向量填充失败: %1").arg(e.what()));
        qDebug() << "向量填充(新轨道) - 异常:" << e.what();
    }
}

// 旧轨道实现的填充向量功能 - 直接使用原有的实现代码
void MainWindow::fillVectorWithPatternOldTrack(const QMap<int, QString> &rowValueMap)
{
    qDebug() << "向量填充(旧轨道) - 开始模式循环填充过程";

    // 获取当前选择的向量表
    int currentIndex = m_vectorTableSelector->currentIndex();
    if (currentIndex < 0)
    {
        QMessageBox::warning(this, tr("警告"), tr("请先选择一个向量表"));
        qDebug() << "向量填充(旧轨道) - 未选择向量表，操作取消";
        return;
    }

    int tableId = m_vectorTableSelector->itemData(currentIndex).toInt();
    QString tableName = m_vectorTableSelector->currentText();
    qDebug() << "向量填充(旧轨道) - 当前向量表ID:" << tableId << ", 名称:" << tableName;

    // 获取选中的列 - 根据当前使用的视图
    QModelIndexList selectedIndexes;
    int targetColumn = -1;
    bool isUsingNewView = (m_vectorStackedWidget->currentIndex() == 1);

    if (isUsingNewView)
    {
        // 新视图 (QTableView)
        selectedIndexes = m_vectorTableView->selectionModel()->selectedIndexes();
    }
    else
    {
        // 旧视图 (QTableWidget)
        selectedIndexes = m_vectorTableWidget->selectionModel()->selectedIndexes();
    }

    if (selectedIndexes.isEmpty())
    {
        QMessageBox::warning(this, tr("警告"), tr("请选择要填充的单元格"));
        qDebug() << "向量填充(旧轨道) - 未选择单元格，操作取消";
        return;
    }

    // 确保所有选中的单元格在同一列
    targetColumn = selectedIndexes.first().column();
    bool sameColumn = true;
    for (const QModelIndex &index : selectedIndexes)
    {
        if (index.column() != targetColumn)
        {
            sameColumn = false;
            break;
        }
    }

    if (!sameColumn)
    {
        QMessageBox::warning(this, tr("警告"), tr("请只选择同一列的单元格"));
        qDebug() << "向量填充(旧轨道) - 选择了多列单元格，操作取消";
        return;
    }

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接失败"));
        qDebug() << "向量填充(旧轨道)失败 - 数据库连接失败";
        return;
    }

    // 开始事务
    db.transaction();

    try
    {
        // 1. 查询表对应的二进制文件路径
        QSqlQuery fileQuery(db);
        fileQuery.prepare("SELECT binary_data_filename FROM VectorTableMasterRecord WHERE id = ?");
        fileQuery.addBindValue(tableId);
        if (!fileQuery.exec() || !fileQuery.next())
        {
            QString errorText = fileQuery.lastError().text();
            qDebug() << "向量填充(旧轨道) - 查询二进制文件名失败:" << errorText;
            throw std::runtime_error(("查询二进制文件名失败: " + errorText).toStdString());
        }

        QString binFileName = fileQuery.value(0).toString();
        if (binFileName.isEmpty())
        {
            qDebug() << "向量填充(旧轨道) - 二进制文件名为空，无法进行填充操作";
            throw std::runtime_error("向量表未配置二进制文件存储，无法进行填充操作");
        }

        // 2. 解析二进制文件路径
        // 使用PathUtils获取项目二进制数据目录
        QString projectBinaryDataDir = Utils::PathUtils::getProjectBinaryDataDirectory(m_currentDbPath);
        if (projectBinaryDataDir.isEmpty())
        {
            QString errorMsg = QString("无法为数据库 '%1' 生成二进制数据目录路径").arg(m_currentDbPath);
            qWarning() << "向量填充(旧轨道) - " << errorMsg;
            throw std::runtime_error(errorMsg.toStdString());
        }

        qDebug() << "向量填充(旧轨道) - 项目二进制数据目录:" << projectBinaryDataDir;

        // 相对路径转绝对路径
        QString absoluteBinFilePath;
        if (QFileInfo(binFileName).isRelative())
        {
            absoluteBinFilePath = QDir(projectBinaryDataDir).absoluteFilePath(binFileName);
            qDebug() << "向量填充(旧轨道) - 相对路径转换为绝对路径:" << binFileName << " -> " << absoluteBinFilePath;
        }
        else
        {
            absoluteBinFilePath = binFileName;
        }

        // 3. 检查二进制文件是否存在
        if (!QFile::exists(absoluteBinFilePath))
        {
            qWarning() << "向量填充(旧轨道) - 二进制文件不存在:" << absoluteBinFilePath;
            throw std::runtime_error(("二进制文件不存在: " + absoluteBinFilePath).toStdString());
        }

        // 4. 查询列定义，找出对应UI列的数据库列
        QSqlQuery colQuery(db);
        colQuery.prepare("SELECT id, column_name, column_order, column_type "
                         "FROM VectorTableColumnConfiguration "
                         "WHERE master_record_id = ? ORDER BY column_order");
        colQuery.addBindValue(tableId);

        if (!colQuery.exec())
        {
            QString errorText = colQuery.lastError().text();
            qDebug() << "向量填充(旧轨道) - 查询列配置失败:" << errorText;
            throw std::runtime_error(("查询列配置失败: " + errorText).toStdString());
        }

        // 收集列信息
        QList<Vector::ColumnInfo> columns;
        while (colQuery.next())
        {
            Vector::ColumnInfo col;
            col.id = colQuery.value(0).toInt();
            col.name = colQuery.value(1).toString();
            col.order = colQuery.value(2).toInt();
            col.type = Vector::columnDataTypeFromString(colQuery.value(3).toString());

            columns.append(col);
        }

        // 5. 打开二进制文件进行操作
        QFile file(absoluteBinFilePath);
        if (!file.open(QIODevice::ReadWrite))
        {
            qWarning() << "向量填充(旧轨道) - 无法打开二进制文件:" << absoluteBinFilePath;
            throw std::runtime_error(("无法打开二进制文件: " + absoluteBinFilePath).toStdString());
        }

        // 记录UI表头列名和实际二进制文件列之间的映射关系
        int targetBinaryColumn = -1;
        QString targetColumnName;

        // 根据当前视图获取列标题
        if (isUsingNewView)
        {
            // 新视图 (QTableView)
            targetColumnName = m_vectorTableModel->headerData(targetColumn, Qt::Horizontal).toString();
            // 如果是多行列标题，只取第一行
            targetColumnName = targetColumnName.section('\n', 0, 0);
        }
        else
        {
            // 旧视图 (QTableWidget)
            QTableWidgetItem *headerItem = m_vectorTableWidget->horizontalHeaderItem(targetColumn);
            QString headerText = headerItem ? headerItem->text() : "";
            targetColumnName = headerText.section('\n', 0, 0); // 获取第一行作为列名
        }

        qDebug() << "向量填充(旧轨道) - 目标列名 (处理后):" << targetColumnName;

        for (int i = 0; i < columns.size(); i++)
        {
            if (columns[i].name == targetColumnName)
            {
                targetBinaryColumn = i;
                break;
            }
        }

        if (targetBinaryColumn < 0)
        {
            qWarning() << "向量填充(旧轨道) - 找不到对应列:" << targetColumnName;
            throw std::runtime_error(("找不到对应列: " + targetColumnName).toStdString());
        }

        // 6. 循环读取和修改每一行
        QList<Vector::RowData> allRows;
        if (!Persistence::BinaryFileHelper::readAllRowsFromBinary(absoluteBinFilePath, columns, 1, allRows))
        {
            qWarning() << "向量填充(旧轨道) - 读取二进制数据失败";
            throw std::runtime_error("读取二进制数据失败");
        }

        // 对每一个需要更新的行，应用模式值
        QMap<int, QString>::const_iterator it;
        for (it = rowValueMap.begin(); it != rowValueMap.end(); ++it)
        {
            int rowIdx = it.key();
            QString value = it.value();

            // 确保行索引有效
            if (rowIdx >= 0 && rowIdx < allRows.size())
            {
                // 修改数据
                QVariant newValue(value);

                // 确保列索引有效
                if (targetBinaryColumn < allRows[rowIdx].size())
                {
                    // 更新数据
                    allRows[rowIdx][targetBinaryColumn] = newValue;
                    qDebug() << "向量填充(旧轨道) - 设置行" << rowIdx << "列" << targetBinaryColumn << "的值为" << value;
                }
                else
                {
                    qWarning() << "向量填充(旧轨道) - 列索引超出范围:" << targetBinaryColumn;
                }
            }
            else
            {
                qWarning() << "向量填充(旧轨道) - 行索引超出范围:" << rowIdx;
            }
        }

        // 回写到二进制文件
        if (!Persistence::BinaryFileHelper::writeAllRowsToBinary(absoluteBinFilePath, columns, 1, allRows))
        {
            qWarning() << "向量填充(旧轨道) - 写入二进制数据失败";
            throw std::runtime_error("写入二进制数据失败");
        }

        // 提交事务
        db.commit();

        // 刷新表格显示
        int currentPage = m_currentPage;

        // 根据当前视图刷新表格
        bool refreshSuccess = false;
        if (isUsingNewView)
        {
            // 新视图 (QTableView)
            if (m_useNewDataHandler)
            {
                refreshSuccess = m_robustDataHandler->loadVectorTablePageDataForModel(
                    tableId, m_vectorTableModel, currentPage, m_pageSize);
            }
            else
            {
                refreshSuccess = VectorDataHandler::instance().loadVectorTablePageDataForModel(
                    tableId, m_vectorTableModel, currentPage, m_pageSize);
            }
        }
        else
        {
            // 旧视图 (QTableWidget)
            if (m_useNewDataHandler)
            {
                refreshSuccess = m_robustDataHandler->loadVectorTablePageData(
                    tableId, m_vectorTableWidget, currentPage, m_pageSize);
            }
            else
            {
                refreshSuccess = VectorDataHandler::instance().loadVectorTablePageData(
                    tableId, m_vectorTableWidget, currentPage, m_pageSize);
            }
        }

        if (!refreshSuccess)
        {
            qWarning() << "向量填充(旧轨道) - 刷新表格数据失败";
        }

        // 选中原来的行
        for (auto it = rowValueMap.begin(); it != rowValueMap.end(); ++it)
        {
            int rowIdx = it.key();
            // 检查行是否在当前页面
            int pageSize = m_pageSize;
            int pageOffset = currentPage * pageSize;

            if (rowIdx >= pageOffset && rowIdx < pageOffset + pageSize)
            {
                // 将数据库索引转换为UI表格索引
                int uiRowIdx = rowIdx - pageOffset;

                if (isUsingNewView)
                {
                    // 新视图选中行和列
                    if (uiRowIdx >= 0 && uiRowIdx < m_vectorTableModel->rowCount())
                    {
                        QModelIndex index = m_vectorTableModel->index(uiRowIdx, targetColumn);
                        m_vectorTableView->selectionModel()->select(index, QItemSelectionModel::Select);
                    }
                }
                else
                {
                    // 旧视图选中行和列
                    if (uiRowIdx >= 0 && uiRowIdx < m_vectorTableWidget->rowCount())
                    {
                        QModelIndex index = m_vectorTableWidget->model()->index(uiRowIdx, targetColumn);
                        m_vectorTableWidget->selectionModel()->select(index, QItemSelectionModel::Select);
                    }
                }
            }
        }

        QMessageBox::information(this, tr("完成"), tr("向量填充完成"));
        qDebug() << "向量填充(旧轨道) - 操作成功完成";
    }
    catch (const std::exception &e)
    {
        // 回滚事务
        db.rollback();
        QMessageBox::critical(this, tr("错误"), tr("向量填充失败: %1").arg(e.what()));
        qDebug() << "向量填充(旧轨道) - 异常:" << e.what();
    }
}

void MainWindow::fillVectorForVectorTable(const QString &value, const QList<int> &selectedUiRows)
{
    qDebug() << "向量填充 - 开始填充过程 (单值填充方法)";

    // 创建行-值映射
    QMap<int, QString> rowValueMap;
    foreach (int row, selectedUiRows)
    {
        rowValueMap[row] = value;
    }

    // 只有新轨道才需要进度对话框
    if (m_useNewDataHandler)
    {
        // 创建进度对话框
        QProgressDialog progress("正在填充向量数据...", "取消", 0, 100, this);
        progress.setWindowModality(Qt::WindowModal);
        progress.setMinimumDuration(0); // 立即显示
        progress.show();
        QCoreApplication::processEvents(); // 确保UI更新

        // 调用模式填充方法，传递进度对话框
        fillVectorWithPattern(rowValueMap, &progress);
    }
    else
    {
        // 旧轨道不使用进度对话框
        fillVectorWithPattern(rowValueMap);
    }
}