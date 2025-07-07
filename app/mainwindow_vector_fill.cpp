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

// Qt Concurrent 并行处理框架
#include <QtConcurrent/QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>

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

    // 获取当前选择的向量表
    int currentIndex = m_vectorTableSelector->currentIndex();
    if (currentIndex < 0)
    {
        QMessageBox::warning(this, tr("警告"), tr("请先选择一个向量表"));
        qDebug() << "向量填充 - 未选择向量表，操作取消";
        return;
    }

    int tableId = m_vectorTableSelector->itemData(currentIndex).toInt();
    QString tableName = m_vectorTableSelector->currentText();
    qDebug() << "向量填充 - 当前向量表ID:" << tableId << ", 名称:" << tableName;

    // 根据当前使用的是新轨道还是旧轨道获取选中的列
    QModelIndexList selectedIndexes;
    int targetColumn = -1;
    QString targetColumnName;
    bool isUsingNewView = (m_vectorStackedWidget->currentIndex() == 1);

    if (isUsingNewView)
    {
        // 新视图 (QTableView)
        selectedIndexes = m_vectorTableView->selectionModel()->selectedIndexes();
        if (!selectedIndexes.isEmpty())
        {
            targetColumn = selectedIndexes.first().column();
            targetColumnName = m_vectorTableModel->headerData(targetColumn, Qt::Horizontal).toString();
            targetColumnName = targetColumnName.section('\n', 0, 0); // 如果是多行列标题，只取第一行
        }
    }
    else
    {
        // 旧视图 (QTableWidget)
        selectedIndexes = m_vectorTableWidget->selectionModel()->selectedIndexes();
        if (!selectedIndexes.isEmpty())
        {
            targetColumn = selectedIndexes.first().column();
            QTableWidgetItem *headerItem = m_vectorTableWidget->horizontalHeaderItem(targetColumn);
            QString headerText = headerItem ? headerItem->text() : "";
            targetColumnName = headerText.section('\n', 0, 0); // 获取第一行作为列名
        }
    }

    if (selectedIndexes.isEmpty() || targetColumn < 0)
    {
        QMessageBox::warning(this, tr("警告"), tr("请选择要填充的单元格"));
        qDebug() << "向量填充 - 未选择单元格，操作取消";
        return;
    }

    // 转换为QVariant映射
    QMap<int, QVariant> variantMap;
    for (auto it = rowValueMap.begin(); it != rowValueMap.end(); ++it)
    {
        variantMap[it.key()] = QVariant(it.value());
    }

    // 根据当前使用的是新轨道还是旧轨道准备工作
    int targetColumnIndex = -1;

    if (m_useNewDataHandler)
    {
        // 新轨道获取列索引
        QList<Vector::ColumnInfo> columns = m_robustDataHandler->getAllColumnInfo(tableId);
        for (int i = 0; i < columns.size(); i++)
        {
            if (columns[i].name == targetColumnName)
            {
                targetColumnIndex = i;
                break;
            }
        }
    }
    else
    {
        // 旧轨道获取列索引
        QList<Vector::ColumnInfo> columns = VectorDataHandler::instance().getAllColumnInfo(tableId);
        for (int i = 0; i < columns.size(); i++)
        {
            if (columns[i].name == targetColumnName)
            {
                targetColumnIndex = i;
                break;
            }
        }
    }

    if (targetColumnIndex < 0)
    {
        QMessageBox::warning(this, tr("错误"), tr("找不到对应列: %1").arg(targetColumnName));
        qDebug() << "向量填充 - 找不到对应列:" << targetColumnName;
        return;
    }

    // 确保进度对话框可用且设置正确
    bool usingExternalProgress = (progress != nullptr);
    QProgressDialog *progressDialog = progress;
    
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

    // 创建FutureWatcher监视异步操作
    using ResultType = QPair<bool, QString>; // 第一个是成功标志，第二个是错误消息
    QFutureWatcher<ResultType> *watcher = new QFutureWatcher<ResultType>(this);

    // 连接信号
    connect(watcher, &QFutureWatcher<ResultType>::finished, this, [this, watcher, progressDialog, usingExternalProgress, isUsingNewView]() {
        // 获取结果
        ResultType result = watcher->result();
        bool success = result.first;
        QString errorMsg = result.second;

        if (success)
        {
            // 不要直接调用refreshVectorTableData，而是使用QueuedConnection通过信号-槽方式更新UI
            QMetaObject::invokeMethod(this, "refreshVectorTableData", Qt::QueuedConnection);
            
            // 操作成功消息放入队列，只在状态栏显示而不弹出对话框
            QMetaObject::invokeMethod(this, [this]() {
                statusBar()->showMessage(tr("向量填充完成"), 3000);
                qDebug() << "向量填充 - 操作成功完成";
            }, Qt::QueuedConnection);
        }
        else
        {
            QMessageBox::critical(this, tr("错误"), tr("向量填充失败: %1").arg(errorMsg));
            qDebug() << "向量填充 - 异常:" << errorMsg;
            
            // 即使失败也尝试刷新表格，确保UI显示最新状态
            try
            {
                QMetaObject::invokeMethod(this, "refreshVectorTableData", Qt::QueuedConnection);
            }
            catch (...)
            {
                qWarning() << "向量填充 - 异常后刷新表格失败";
            }
        }

        // 如果是我们创建的进度对话框，需要删除它
        if (!usingExternalProgress && progressDialog)
        {
            progressDialog->setValue(100); // 确保进度条显示完成
            progressDialog->deleteLater();
        }

        // 释放watcher资源
        watcher->deleteLater();
    });

    // 启动并发任务
    if (m_useNewDataHandler)
    {
        // 创建一个中间对象用于在后台线程中转发信号
        QObject *signalProxy = new QObject();
        
        // 使用新轨道的异步实现
        QFuture<ResultType> future = QtConcurrent::run(
            [this, tableId, targetColumnIndex, variantMap, signalProxy, progressDialog]() -> ResultType {
                QString errorMsg;
                
                // 在工作线程中连接信号
                QObject::connect(m_robustDataHandler, &RobustVectorDataHandler::progressUpdated,
                                progressDialog, &QProgressDialog::setValue, Qt::QueuedConnection);
                
                // 执行批量更新
                bool success = m_robustDataHandler->batchUpdateVectorColumnOptimized(
                    tableId, targetColumnIndex, variantMap, errorMsg);
                
                // 断开信号连接
                QObject::disconnect(m_robustDataHandler, &RobustVectorDataHandler::progressUpdated,
                                   progressDialog, &QProgressDialog::setValue);
                
                // 清理资源
                signalProxy->deleteLater();
                
                return qMakePair(success, errorMsg);
            }
        );
        
        // 设置监视器
        watcher->setFuture(future);
    }
    else
    {
        // 使用旧轨道的异步实现
        QFuture<ResultType> future = QtConcurrent::run(
            [this, tableId, targetColumnIndex, variantMap, targetColumnName, progressDialog]() -> ResultType {
                try
                {
                    // 设置初始进度
                    if (progressDialog) {
                        QMetaObject::invokeMethod(progressDialog, "setValue", Qt::QueuedConnection,
                                                Q_ARG(int, 10));
                    }
                    
                    // 获取数据库连接
                    QSqlDatabase db = DatabaseManager::instance()->database();
                    if (!db.isOpen())
                    {
                        return qMakePair(false, QString("数据库连接失败"));
                    }

                    // 开始事务
                    db.transaction();

                    try
                    {
                        // 设置准备阶段进度
                        if (progressDialog) {
                            QMetaObject::invokeMethod(progressDialog, "setValue", Qt::QueuedConnection,
                                                    Q_ARG(int, 20));
                        }
                        
                        // 旧轨道的批量更新逻辑（参照原来的实现）
                        // 查询表对应的二进制文件路径...
                        // 以下是原来fillVectorWithPatternOldTrack的核心逻辑，移除UI交互部分
                        
                        // 1. 查询表对应的二进制文件路径
                        QSqlQuery fileQuery(db);
                        fileQuery.prepare("SELECT binary_data_filename FROM VectorTableMasterRecord WHERE id = ?");
                        fileQuery.addBindValue(tableId);
                        if (!fileQuery.exec() || !fileQuery.next())
                        {
                            QString errorText = fileQuery.lastError().text();
                            throw std::runtime_error(("查询二进制文件名失败: " + errorText).toStdString());
                        }

                        QString binFileName = fileQuery.value(0).toString();
                        if (binFileName.isEmpty())
                        {
                            throw std::runtime_error("向量表未配置二进制文件存储，无法进行填充操作");
                        }

                        // 2. 解析二进制文件路径
                        // 使用PathUtils获取项目二进制数据目录
                        QString projectBinaryDataDir = Utils::PathUtils::getProjectBinaryDataDirectory(m_currentDbPath);
                        if (projectBinaryDataDir.isEmpty())
                        {
                            QString errorMsg = QString("无法为数据库 '%1' 生成二进制数据目录路径").arg(m_currentDbPath);
                            throw std::runtime_error(errorMsg.toStdString());
                        }

                        // 相对路径转绝对路径
                        QString absoluteBinFilePath;
                        if (QFileInfo(binFileName).isRelative())
                        {
                            absoluteBinFilePath = QDir(projectBinaryDataDir).absoluteFilePath(binFileName);
                        }
                        else
                        {
                            absoluteBinFilePath = binFileName;
                        }

                        // 3. 检查二进制文件是否存在
                        if (!QFile::exists(absoluteBinFilePath))
                        {
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
                            throw std::runtime_error(("无法打开二进制文件: " + absoluteBinFilePath).toStdString());
                        }

                        // 记录UI表头列名和实际二进制文件列之间的映射关系
                        int targetBinaryColumn = -1;

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
                            throw std::runtime_error(("找不到对应列: " + targetColumnName).toStdString());
                        }

                        // 6. 循环读取和修改每一行
                        QList<Vector::RowData> allRows;
                        if (!Persistence::BinaryFileHelper::readAllRowsFromBinary(absoluteBinFilePath, columns, 1, allRows))
                        {
                            throw std::runtime_error("读取二进制数据失败");
                        }

                        // 设置读取完成进度
                        if (progressDialog) {
                            QMetaObject::invokeMethod(progressDialog, "setValue", Qt::QueuedConnection,
                                                   Q_ARG(int, 40));
                        }

                        // 对每一个需要更新的行，应用模式值
                        QMap<int, QVariant>::const_iterator it;
                        int processedRows = 0;
                        int totalRows = variantMap.size();
                        
                        for (it = variantMap.begin(); it != variantMap.end(); ++it)
                        {
                            int rowIdx = it.key();
                            QVariant value = it.value();

                            // 确保行索引有效
                            if (rowIdx >= 0 && rowIdx < allRows.size())
                            {
                                // 确保列索引有效
                                if (targetBinaryColumn < allRows[rowIdx].size())
                                {
                                    // 更新数据
                                    allRows[rowIdx][targetBinaryColumn] = value;
                                }
                            }
                            
                            // 每处理100行更新一次进度
                            processedRows++;
                            if (processedRows % 100 == 0 || processedRows == totalRows) {
                                int percentage = 40 + (processedRows * 30) / totalRows;
                                if (progressDialog) {
                                    QMetaObject::invokeMethod(progressDialog, "setValue", Qt::QueuedConnection,
                                                           Q_ARG(int, percentage));
                                }
                            }
                        }

                        // 设置写入前进度
                        if (progressDialog) {
                            QMetaObject::invokeMethod(progressDialog, "setValue", Qt::QueuedConnection,
                                                   Q_ARG(int, 70));
                        }

                        // 回写到二进制文件
                        if (!Persistence::BinaryFileHelper::writeAllRowsToBinary(absoluteBinFilePath, columns, 1, allRows))
                        {
                            throw std::runtime_error("写入二进制数据失败");
                        }

                        // 设置完成进度
                        if (progressDialog) {
                            QMetaObject::invokeMethod(progressDialog, "setValue", Qt::QueuedConnection,
                                                   Q_ARG(int, 100));
                        }

                        // 提交事务
                        db.commit();
                        return qMakePair(true, QString());
                    }
                    catch (const std::exception &e)
                    {
                        // 回滚事务
                        db.rollback();
                        return qMakePair(false, QString(e.what()));
                    }
                }
                catch (const std::exception &e)
                {
                    return qMakePair(false, QString(e.what()));
                }
            }
        );
        
        // 设置监视器
        watcher->setFuture(future);
    }

    // 连接进度条取消按钮（可选，取决于你的需求）
    if (progressDialog)
    {
        // 注意：QtConcurrent任务不能轻易取消，所以这里只是关闭进度对话框
        connect(progressDialog, &QProgressDialog::canceled, [progressDialog]() {
            progressDialog->setLabelText("正在完成当前操作，请稍候...");
            progressDialog->setCancelButton(nullptr); // 禁用取消按钮
        });
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

    // 创建进度对话框
    QProgressDialog progress("正在填充向量数据...", "取消", 0, 100, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0); // 立即显示
    progress.show();
    QCoreApplication::processEvents(); // 确保UI更新

    // 调用通用的填充方法，并传递进度对话框
    fillVectorWithPattern(rowValueMap, &progress);
}

// 用于连接RobustVectorDataHandler的进度信号到其他对象
void MainWindow::connectProgressSignal(RobustVectorDataHandler *handler, QObject *receiver, const char *slot)
{
    if (handler && receiver) {
        QObject::connect(handler, SIGNAL(progressUpdated(int)), receiver, slot);
    }
}