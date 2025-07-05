#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QFile>
#include <QDataStream>
#include <QVariant>
#include <QTimer>
#include <QProgressDialog>
#include "common/binary_file_format.h"
#include "common/utils/pathutils.h"

void MainWindow::replaceTimeSetForVectorTable(int fromTimeSetId, int toTimeSetId, const QList<int> &selectedUiRows)
{
    qDebug() << "替换TimeSet - 开始替换过程";

    // 输出选择的行信息
    if (selectedUiRows.isEmpty())
    {
        qDebug() << "替换TimeSet - 用户未选择特定行，将对所有行进行操作";
    }
    else
    {
        QStringList rowsList;
        for (int row : selectedUiRows)
        {
            rowsList << QString::number(row);
        }
        qDebug() << "替换TimeSet - 用户选择了以下行：" << rowsList.join(", ");
    }

    int currentIndex = m_vectorTableSelector->currentIndex();
    if (currentIndex < 0)
    {
        QMessageBox::warning(this, tr("警告"), tr("请先选择一个向量表"));
        qDebug() << "替换TimeSet - 未选择向量表，操作取消";
        return;
    }
    int tableId = m_vectorTableSelector->currentData().toInt();
    QString tableName = m_vectorTableSelector->currentText();
    qDebug() << "替换TimeSet - 当前向量表ID:" << tableId << ", 名称:" << tableName;

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接失败"));
        qDebug() << "替换TimeSet失败 - 数据库连接失败";
        return;
    }

    // 获取TimeSet名称用于日志
    QSqlQuery fromNameQuery(db);
    fromNameQuery.prepare("SELECT timeset_name FROM timeset_list WHERE id = ?");
    fromNameQuery.addBindValue(fromTimeSetId);
    QString fromTimeSetName = "未知";
    if (fromNameQuery.exec() && fromNameQuery.next())
    {
        fromTimeSetName = fromNameQuery.value(0).toString();
    }

    QSqlQuery toNameQuery(db);
    toNameQuery.prepare("SELECT timeset_name FROM timeset_list WHERE id = ?");
    toNameQuery.addBindValue(toTimeSetId);
    QString toTimeSetName = "未知";
    if (toNameQuery.exec() && toNameQuery.next())
    {
        toTimeSetName = toNameQuery.value(0).toString();
    }
    qDebug() << "替换TimeSet - 查找:" << fromTimeSetName << "(" << fromTimeSetId << ") 替换:" << toTimeSetName << "(" << toTimeSetId << ")";

    // 判断是否使用新的数据处理器
    if (m_useNewDataHandler)
    {
        try
        {
            // 使用高性能批量处理方法
            QString errorMsg;
            QProgressDialog progress("正在替换TimeSet数据...", "取消", 0, 100, this);
            progress.setWindowModality(Qt::WindowModal);
            progress.setMinimumDuration(500); // 如果操作少于500ms就不显示进度对话框

            // 连接进度信号
            connect(m_robustDataHandler, &RobustVectorDataHandler::progressUpdated,
                    &progress, &QProgressDialog::setValue);

            progress.show();

            // 调用批量替换TimeSet方法
            bool success = m_robustDataHandler->batchReplaceTimeSet(tableId, fromTimeSetId, toTimeSetId, selectedUiRows, errorMsg);

            // 断开进度信号连接
            disconnect(m_robustDataHandler, &RobustVectorDataHandler::progressUpdated,
                       &progress, &QProgressDialog::setValue);

            if (!success)
            {
                QMessageBox::critical(this, tr("错误"), tr("替换TimeSet失败: %1").arg(errorMsg));
                qWarning() << "替换TimeSet - 批量操作失败:" << errorMsg;
                return;
            }

            // 刷新表格显示
            m_robustDataHandler->clearTableDataCache(tableId);
            bool refreshSuccess = m_robustDataHandler->loadVectorTablePageDataForModel(
                tableId, m_vectorTableModel, m_currentPage, m_pageSize);

            if (!refreshSuccess)
            {
                qWarning() << "替换TimeSet - 刷新表格数据失败";
            }

            // 更新分页信息
            updatePaginationInfo();

            // 更新波形图以反映TimeSet变更
            if (m_isWaveformVisible && m_waveformPlot)
            {
                QTimer::singleShot(100, this, [this]()
                                   { updateWaveformView(); });
            }

            QMessageBox::information(this, tr("完成"),
                                     tr("已将TimeSet %1 替换为 %2").arg(fromTimeSetName).arg(toTimeSetName));

            qDebug() << "替换TimeSet - 操作成功完成";
        }
        catch (const std::exception &e)
        {
            QMessageBox::critical(this, tr("错误"), tr("替换TimeSet失败: %1").arg(e.what()));
            qDebug() << "替换TimeSet - 异常:" << e.what();
        }
    }
    else
    {
        // 原有的实现保持不变，适用于旧的数据处理器
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
                qDebug() << "替换TimeSet - 查询二进制文件名失败:" << errorText;
                throw std::runtime_error(("查询二进制文件名失败: " + errorText).toStdString());
            }

            QString binFileName = fileQuery.value(0).toString();
            if (binFileName.isEmpty())
            {
                qDebug() << "替换TimeSet - 二进制文件名为空，无法进行替换操作";
                throw std::runtime_error("向量表未配置二进制文件存储，无法进行替换操作");
            }

            // 2. 解析二进制文件路径
            // 使用PathUtils获取项目二进制数据目录
            QString projectBinaryDataDir = Utils::PathUtils::getProjectBinaryDataDirectory(m_currentDbPath);
            if (projectBinaryDataDir.isEmpty())
            {
                QString errorMsg = QString("无法为数据库 '%1' 生成二进制数据目录路径").arg(m_currentDbPath);
                qWarning() << "替换TimeSet - " << errorMsg;
                throw std::runtime_error(errorMsg.toStdString());
            }

            qDebug() << "替换TimeSet - 项目二进制数据目录:" << projectBinaryDataDir;

            // 相对路径转绝对路径
            QString absoluteBinFilePath;
            if (QFileInfo(binFileName).isRelative())
            {
                absoluteBinFilePath = QDir(projectBinaryDataDir).absoluteFilePath(binFileName);
                qDebug() << "替换TimeSet - 相对路径转换为绝对路径:" << binFileName << " -> " << absoluteBinFilePath;
            }
            else
            {
                absoluteBinFilePath = binFileName;
            }

            // 3. 检查二进制文件是否存在
            if (!QFile::exists(absoluteBinFilePath))
            {
                qWarning() << "替换TimeSet - 二进制文件不存在:" << absoluteBinFilePath;
                throw std::runtime_error(("二进制文件不存在: " + absoluteBinFilePath).toStdString());
            }

            // 4. 查询列定义，找出TimeSet列
            QSqlQuery colQuery(db);
            colQuery.prepare("SELECT id, column_name, column_order, column_type "
                             "FROM VectorTableColumnConfiguration "
                             "WHERE master_record_id = ? ORDER BY column_order");
            colQuery.addBindValue(tableId);

            if (!colQuery.exec())
            {
                QString errorText = colQuery.lastError().text();
                qDebug() << "替换TimeSet - 查询列配置失败:" << errorText;
                throw std::runtime_error(("查询列配置失败: " + errorText).toStdString());
            }

            // 5. 遍历列定义，找到TimeSet列
            int timeSetColumnIndex = -1;
            QList<Vector::ColumnInfo> columns;

            while (colQuery.next())
            {
                int columnId = colQuery.value(0).toInt();
                QString columnName = colQuery.value(1).toString();
                int columnOrder = colQuery.value(2).toInt();
                QString columnType = colQuery.value(3).toString();

                Vector::ColumnInfo colInfo;
                colInfo.id = columnId;
                colInfo.vector_table_id = tableId;
                colInfo.name = columnName;
                colInfo.order = columnOrder;
                colInfo.original_type_str = columnType;

                if (columnType == "TIMESET_ID")
                {
                    timeSetColumnIndex = columnOrder;
                    colInfo.type = Vector::ColumnDataType::TIMESET_ID;
                }
                else if (columnType == "INSTRUCTION_ID")
                {
                    colInfo.type = Vector::ColumnDataType::INSTRUCTION_ID;
                }
                else if (columnType == "PIN_STATE_ID")
                {
                    colInfo.type = Vector::ColumnDataType::PIN_STATE_ID;
                }
                else
                {
                    colInfo.type = Vector::ColumnDataType::TEXT; // 默认类型
                }

                columns.append(colInfo);
            }

            if (timeSetColumnIndex == -1)
            {
                qWarning() << "替换TimeSet - 未找到TimeSet列，尝试修复表结构";

                // 尝试添加缺失的TimeSet列
                QSqlQuery addTimeSetColQuery(db);
                addTimeSetColQuery.prepare("INSERT INTO VectorTableColumnConfiguration "
                                           "(master_record_id, column_name, column_order, column_type, data_properties, IsVisible) "
                                           "VALUES (?, ?, ?, ?, ?, 1)");

                // 获取当前最大列序号
                int maxOrder = -1;
                QSqlQuery maxOrderQuery(db);
                maxOrderQuery.prepare("SELECT MAX(column_order) FROM VectorTableColumnConfiguration WHERE master_record_id = ?");
                maxOrderQuery.addBindValue(tableId);
                if (maxOrderQuery.exec() && maxOrderQuery.next())
                {
                    maxOrder = maxOrderQuery.value(0).toInt();
                }

                // 如果无法获取最大列序号，默认放在第2位置
                if (maxOrder < 0)
                {
                    maxOrder = 1; // 添加在位置2 (索引为2，实际是第3列)
                }

                // 添加TimeSet列
                addTimeSetColQuery.addBindValue(tableId);
                addTimeSetColQuery.addBindValue("TimeSet");
                addTimeSetColQuery.addBindValue(2); // 固定在位置2 (通常TimeSet是第3列)
                addTimeSetColQuery.addBindValue("TIMESET_ID");
                addTimeSetColQuery.addBindValue("{}");

                if (!addTimeSetColQuery.exec())
                {
                    qWarning() << "替换TimeSet - 添加TimeSet列失败:" << addTimeSetColQuery.lastError().text();
                    throw std::runtime_error(("未找到TimeSet类型的列，且尝试添加时失败。无法执行替换操作。"));
                }

                qDebug() << "替换TimeSet - 成功添加TimeSet列，重新获取列定义";

                // 重新查询列定义
                columns.clear();
                colQuery.prepare("SELECT id, column_name, column_order, column_type "
                                 "FROM VectorTableColumnConfiguration "
                                 "WHERE master_record_id = ? ORDER BY column_order");
                colQuery.addBindValue(tableId);

                if (!colQuery.exec())
                {
                    QString errorText = colQuery.lastError().text();
                    qDebug() << "替换TimeSet - 重新查询列配置失败:" << errorText;
                    throw std::runtime_error(("重新查询列配置失败: " + errorText).toStdString());
                }

                // 再次遍历列定义，找到TimeSet列
                while (colQuery.next())
                {
                    int columnId = colQuery.value(0).toInt();
                    QString columnName = colQuery.value(1).toString();
                    int columnOrder = colQuery.value(2).toInt();
                    QString columnType = colQuery.value(3).toString();

                    Vector::ColumnInfo colInfo;
                    colInfo.id = columnId;
                    colInfo.vector_table_id = tableId;
                    colInfo.name = columnName;
                    colInfo.order = columnOrder;
                    colInfo.original_type_str = columnType;

                    if (columnType == "TIMESET_ID")
                    {
                        timeSetColumnIndex = columnOrder;
                        colInfo.type = Vector::ColumnDataType::TIMESET_ID;
                    }
                    else if (columnType == "INSTRUCTION_ID")
                    {
                        colInfo.type = Vector::ColumnDataType::INSTRUCTION_ID;
                    }
                    else if (columnType == "PIN_STATE_ID")
                    {
                        colInfo.type = Vector::ColumnDataType::PIN_STATE_ID;
                    }
                    else
                    {
                        colInfo.type = Vector::ColumnDataType::TEXT; // 默认类型
                    }

                    columns.append(colInfo);
                }

                if (timeSetColumnIndex == -1)
                {
                    qWarning() << "替换TimeSet - 添加TimeSet列后仍找不到该列，放弃操作";
                    throw std::runtime_error("添加TimeSet列后仍找不到该列，无法执行替换操作。");
                }
            }

            qDebug() << "替换TimeSet - 找到TimeSet列，索引为:" << timeSetColumnIndex;

            // 6. 读取向量表所有行数据
            QFile file(absoluteBinFilePath);
            if (!file.open(QIODevice::ReadWrite))
            {
                qWarning() << "替换TimeSet - 无法以读写模式打开二进制文件:" << file.errorString();
                throw std::runtime_error(("无法以读写模式打开二进制文件: " + file.errorString()).toStdString());
            }

            // 读取文件头
            QDataStream stream(&file);
            quint32 magic;
            stream >> magic;

            if (magic != Persistence::VEC_BINDATA_MAGIC)
            {
                qWarning() << "替换TimeSet - 文件头魔数校验失败:" << QString::number(magic, 16);
                file.close();
                throw std::runtime_error("文件格式错误，不是有效的向量数据文件");
            }

            quint32 headerSize, version, totalRows;
            stream >> headerSize >> version >> totalRows;
            qDebug() << "替换TimeSet - 文件头信息: 版本=" << version << ", 总行数=" << totalRows;

            // 7. 计算需要检查的行索引
            QList<int> rowIndexesToCheck;
            if (selectedUiRows.isEmpty())
            {
                // 如果未选择特定行，则检查所有行
                for (quint32 i = 0; i < totalRows; ++i)
                {
                    rowIndexesToCheck.append(i);
                }
            }
            else
            {
                // 使用用户选择的行
                rowIndexesToCheck = selectedUiRows;
            }

            qDebug() << "替换TimeSet - 需要检查的行数:" << rowIndexesToCheck.size();

            // 8. 逐行读取、检查并修改
            int updatedRows = 0;
            for (int rowIdx : rowIndexesToCheck)
            {
                // 计算行在文件中的位置 (首先定位到数据开始部分)
                qint64 dataStartPos = headerSize; // 一般是 16 字节
                file.seek(dataStartPos);

                // 跳过之前的所有行
                for (int i = 0; i < rowIdx; ++i)
                {
                    quint32 rowSize;
                    stream >> rowSize;
                    file.seek(file.pos() + rowSize);
                }

                // 读取当前行数据
                quint32 rowSize;
                stream >> rowSize;
                qint64 rowDataPos = file.pos();

                QByteArray rowData = file.read(rowSize);
                if (rowData.size() != rowSize)
                {
                    qWarning() << "替换TimeSet - 读取行数据失败，行:" << rowIdx;
                    continue;
                }

                // 反序列化行数据
                QDataStream rowStream(rowData);
                QList<QVariant> row;
                while (!rowStream.atEnd())
                {
                    quint8 typeCode;
                    rowStream >> typeCode;

                    switch (typeCode)
                    {
                    case 0: // NULL
                        row.append(QVariant());
                        break;
                    case 1: // Int
                    {
                        qint32 value;
                        rowStream >> value;
                        row.append(value);
                        break;
                    }
                    case 2: // Double
                    {
                        double value;
                        rowStream >> value;
                        row.append(value);
                        break;
                    }
                    case 3: // String
                    {
                        QString value;
                        rowStream >> value;
                        row.append(value);
                        break;
                    }
                    // 其他类型...
                    default:
                        qWarning() << "替换TimeSet - 未知数据类型:" << typeCode;
                        row.append(QVariant());
                        break;
                    }
                }

                // 确保行数据长度足够
                while (row.size() <= timeSetColumnIndex)
                {
                    row.append(QVariant());
                }

                // 检查并替换TimeSet列的值
                if (row[timeSetColumnIndex].toInt() == fromTimeSetId)
                {
                    // 更新为新的TimeSet ID
                    row[timeSetColumnIndex] = toTimeSetId;

                    // 重新序列化行数据
                    QByteArray newRowData;
                    QDataStream newRowStream(&newRowData, QIODevice::WriteOnly);
                    for (const QVariant &value : row)
                    {
                        if (!value.isValid() || value.isNull())
                        {
                            newRowStream << (quint8)0; // NULL
                        }
                        else if (value.type() == QVariant::Int)
                        {
                            newRowStream << (quint8)1; // Int
                            newRowStream << value.toInt();
                        }
                        else if (value.type() == QVariant::Double)
                        {
                            newRowStream << (quint8)2; // Double
                            newRowStream << value.toDouble();
                        }
                        else
                        {
                            newRowStream << (quint8)3; // String
                            newRowStream << value.toString();
                        }
                    }

                    // 判断是否需要重新定位行
                    if (newRowData.size() <= rowSize)
                    {
                        // 可以原地更新
                        file.seek(rowDataPos);
                        qint64 written = file.write(newRowData);
                        if (written != newRowData.size())
                        {
                            qWarning() << "替换TimeSet - 写入行数据失败，行:" << rowIdx;
                            continue;
                        }

                        // 如果新数据比旧数据短，填充0以保持一致长度
                        if (newRowData.size() < rowSize)
                        {
                            QByteArray padding(rowSize - newRowData.size(), 0);
                            file.write(padding);
                        }
                    }
                    else
                    {
                        // 需要追加到文件末尾
                        // 先定位到文件末尾
                        file.seek(file.size());
                        qint64 newPos = file.pos();

                        // 写入新行数据
                        quint32 newRowSize = newRowData.size();
                        QDataStream outStream(&file);
                        outStream << newRowSize;
                        qint64 written = file.write(newRowData);
                        if (written != newRowData.size())
                        {
                            qWarning() << "替换TimeSet - 写入行数据失败，行:" << rowIdx;
                            continue;
                        }

                        // 更新行索引
                        QSqlQuery updateIndexQuery(db);
                        updateIndexQuery.prepare("UPDATE VectorTableRowIndex SET offset = ?, size = ? WHERE master_record_id = ? AND logical_row_order = ?");
                        updateIndexQuery.addBindValue(newPos);
                        updateIndexQuery.addBindValue(newRowSize + sizeof(quint32));
                        updateIndexQuery.addBindValue(tableId);
                        updateIndexQuery.addBindValue(rowIdx);
                        if (!updateIndexQuery.exec())
                        {
                            qWarning() << "替换TimeSet - 更新行索引失败，行:" << rowIdx << ", 错误:" << updateIndexQuery.lastError().text();
                            continue;
                        }
                    }

                    updatedRows++;
                }
            }

            file.close();

            // 提交事务
            db.commit();

            qDebug() << "替换TimeSet - 成功替换" << updatedRows << "行数据";
            QMessageBox::information(this, "操作成功", QString("成功将 %1 行数据的TimeSet从 %2 替换为 %3").arg(updatedRows).arg(fromTimeSetName).arg(toTimeSetName));

            // 刷新当前页面
            VectorDataHandler::instance().clearTableDataCache(tableId);

            // 根据当前视图模式刷新
            bool refreshSuccess = false;
            bool isUsingNewView = (m_vectorStackedWidget->currentIndex() == 1);

            if (isUsingNewView && m_vectorTableModel)
            {
                // 新视图 (QTableView)
                qDebug() << "替换TimeSet - 使用新视图刷新当前页数据，保持在页码:" << m_currentPage;
                refreshSuccess = VectorDataHandler::instance().loadVectorTablePageDataForModel(
                    tableId, m_vectorTableModel, m_currentPage, m_pageSize);
            }
            else
            {
                // 旧视图 (QTableWidget)
                qDebug() << "替换TimeSet - 使用旧视图刷新当前页数据，保持在页码:" << m_currentPage;
                refreshSuccess = VectorDataHandler::instance().loadVectorTablePageData(
                    tableId, m_vectorTableWidget, m_currentPage, m_pageSize);
            }

            if (!refreshSuccess)
            {
                qWarning() << "替换TimeSet - 刷新表格数据失败";
            }

            // 更新分页信息显示
            updatePaginationInfo();

            // 更新波形图以反映TimeSet变更
            if (m_isWaveformVisible && m_waveformPlot)
            {
                QTimer::singleShot(100, this, [this]()
                                   {
                qDebug() << "MainWindow::replaceTimeSetForVectorTable - 延迟更新波形图";
                updateWaveformView(); });
            }
        }
        catch (const std::exception &e)
        {
            // 回滚事务
            db.rollback();
            qDebug() << "替换TimeSet - 操作失败，已回滚事务:" << e.what();

            // 显示错误消息
            QMessageBox::critical(this, tr("错误"), tr("替换TimeSet失败: %1").arg(e.what()));
        }
    }
}

void MainWindow::showReplaceTimeSetDialog()
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

    // 显示替换TimeSet对话框
    ReplaceTimeSetDialog dialog(this);
    // 设置向量表总行数
    dialog.setVectorRowCount(rowCount);

    // 获取选中的单元格并计算行范围
    QModelIndexList selectedIndexes = m_vectorTableWidget->selectionModel()->selectedIndexes();
    if (!selectedIndexes.isEmpty())
    {
        // 找出最小和最大行号（1-based）
        int minRow = INT_MAX;
        int maxRow = 0;
        int pageOffset = m_currentPage * m_pageSize; // 分页偏移
        foreach (const QModelIndex &index, selectedIndexes)
        {
            int rowIdx = pageOffset + index.row() + 1; // 转换为1-based的绝对行号
            minRow = qMin(minRow, rowIdx);
            maxRow = qMax(maxRow, rowIdx);
        }
        if (minRow <= maxRow)
        {
            dialog.setSelectedRange(minRow, maxRow);
        }
    }

    if (dialog.exec() == QDialog::Accepted)
    {
        int fromTimeSetId = dialog.getFromTimeSetId();
        int toTimeSetId = dialog.getToTimeSetId();

        QList<int> rowsToUpdate;
        // 修复BUG：无论是否选中行，都使用对话框中设置的范围
        int fromRow = dialog.getStartRow(); // 0-based
        int toRow = dialog.getEndRow();     // 0-based

        // Ensure the range is valid
        if (fromRow < 0 || toRow < fromRow || toRow >= rowCount)
        {
            QMessageBox::warning(this, "范围无效", "指定的行范围无效。");
            return;
        }

        for (int i = fromRow; i <= toRow; ++i)
        {
            rowsToUpdate.append(i);
        }

        // Replace TimeSet
        replaceTimeSetForVectorTable(fromTimeSetId, toTimeSetId, rowsToUpdate);
    }
}
