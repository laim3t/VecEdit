// ==========================================================
//  Headers for: mainwindow_pins_timeset.cpp
// ==========================================================
#include "app/mainwindow.h"

// Qt Widgets & Core
#include <QMessageBox>
#include <QInputDialog>
#include <QDialog>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTimer>

// Project-specific headers
#include "database/databasemanager.h"
#include "pin/vectorpinsettingsdialog.h"
#include "pin/pinsettingsdialog.h"
#include "timeset/filltimesetdialog.h"
#include "timeset/replacetimesetdialog.h"
#include "migration/datamigrator.h"
#include "common/utils/pathutils.h"
#include "../database/binaryfilehelper.h"

void MainWindow::fillTimeSetForVectorTable(int timeSetId, const QList<int> &selectedUiRows)
{
    // 添加调试日志
    qDebug() << "填充TimeSet开始 - TimeSet ID:" << timeSetId;
    if (!selectedUiRows.isEmpty())
    {
        QStringList rowList;
        for (int row : selectedUiRows)
        {
            rowList << QString::number(row);
        }
        qDebug() << "填充TimeSet - 针对UI行:" << rowList.join(',');
    }
    else
    {
        qDebug() << "填充TimeSet - 未选择行，将应用于整个表";
    }

    // 检查TimeSet ID有效性
    if (timeSetId <= 0)
    {
        QMessageBox::warning(this, tr("错误"), tr("TimeSet ID无效"));
        qDebug() << "填充TimeSet参数无效 - TimeSet ID:" << timeSetId;
        return;
    }

    // 获取当前向量表ID
    int currentIndex = m_vectorTableSelector->currentIndex();
    if (currentIndex < 0)
    {
        qDebug() << "填充TimeSet失败 - 未选择向量表";
        return;
    }
    int tableId = m_vectorTableSelector->itemData(currentIndex).toInt();
    QString tableName = m_vectorTableSelector->currentText();
    qDebug() << "填充TimeSet - 当前向量表ID:" << tableId << ", 名称:" << tableName;

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接失败"));
        qDebug() << "填充TimeSet失败 - 数据库连接失败";
        return;
    }

    // 获取TimeSet名称用于日志
    QSqlQuery nameQuery(db);
    nameQuery.prepare("SELECT timeset_name FROM timeset_list WHERE id = ?");
    nameQuery.addBindValue(timeSetId);
    QString timeSetName = "未知";
    if (nameQuery.exec() && nameQuery.next())
    {
        timeSetName = nameQuery.value(0).toString();
    }
    qDebug() << "填充TimeSet - 使用TimeSet:" << timeSetName << "(" << timeSetId << ")";

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
            qDebug() << "填充TimeSet - 查询二进制文件名失败:" << errorText;
            throw std::runtime_error(("查询二进制文件名失败: " + errorText).toStdString());
        }

        QString binFileName = fileQuery.value(0).toString();
        if (binFileName.isEmpty())
        {
            qDebug() << "填充TimeSet - 二进制文件名为空，无法进行填充操作";
            throw std::runtime_error("向量表未配置二进制文件存储，无法进行填充操作");
        }

        // 2. 解析二进制文件路径
        // 使用PathUtils获取项目二进制数据目录
        QString projectBinaryDataDir = Utils::PathUtils::getProjectBinaryDataDirectory(m_currentDbPath);
        if (projectBinaryDataDir.isEmpty())
        {
            QString errorMsg = QString("无法为数据库 '%1' 生成二进制数据目录路径").arg(m_currentDbPath);
            qWarning() << "填充TimeSet - " << errorMsg;
            throw std::runtime_error(errorMsg.toStdString());
        }

        qDebug() << "填充TimeSet - 项目二进制数据目录:" << projectBinaryDataDir;

        // 相对路径转绝对路径
        QString absoluteBinFilePath;
        if (QFileInfo(binFileName).isRelative())
        {
            absoluteBinFilePath = QDir(projectBinaryDataDir).absoluteFilePath(binFileName);
            qDebug() << "填充TimeSet - 相对路径转换为绝对路径:" << binFileName << " -> " << absoluteBinFilePath;
        }
        else
        {
            absoluteBinFilePath = binFileName;
        }

        // 3. 检查二进制文件是否存在
        if (!QFile::exists(absoluteBinFilePath))
        {
            qWarning() << "填充TimeSet - 二进制文件不存在:" << absoluteBinFilePath;
            throw std::runtime_error(("二进制文件不存在: " + absoluteBinFilePath).toStdString());
        }

        // 4. 查询列定义，找出TimeSet列
        QSqlQuery colQuery(db);
        colQuery.prepare("SELECT id, column_name, column_order, column_type, data_properties, IsVisible "
                         "FROM VectorTableColumnConfiguration "
                         "WHERE master_record_id = ? ORDER BY column_order");
        colQuery.addBindValue(tableId);

        if (!colQuery.exec())
        {
            QString errorText = colQuery.lastError().text();
            qWarning() << "填充TimeSet - 查询列定义失败:" << errorText;
            throw std::runtime_error(("查询列定义失败: " + errorText).toStdString());
        }

        QList<Vector::ColumnInfo> columns;
        int timeSetColumnIndex = -1; // 用于标记TimeSet列的索引
        int instructionIdColumnIndex = -1;

        while (colQuery.next())
        {
            Vector::ColumnInfo colInfo;
            colInfo.id = colQuery.value(0).toInt();
            colInfo.name = colQuery.value(1).toString();
            colInfo.order = colQuery.value(2).toInt();
            colInfo.original_type_str = colQuery.value(3).toString();

            // 解析data_properties
            QString dataPropertiesStr = colQuery.value(4).toString();
            QJsonDocument jsonDoc = QJsonDocument::fromJson(dataPropertiesStr.toUtf8());
            if (!jsonDoc.isNull() && jsonDoc.isObject())
            {
                colInfo.data_properties = jsonDoc.object();
            }
            else
            {
                // 如果解析失败，创建一个空的QJsonObject
                colInfo.data_properties = QJsonObject();
            }

            colInfo.is_visible = colQuery.value(5).toBool();

            // 映射列类型字符串到枚举
            if (colInfo.original_type_str == "TEXT")
                colInfo.type = Vector::ColumnDataType::TEXT;
            else if (colInfo.original_type_str == "INTEGER")
                colInfo.type = Vector::ColumnDataType::INTEGER;
            else if (colInfo.original_type_str == "BOOLEAN")
                colInfo.type = Vector::ColumnDataType::BOOLEAN;
            else if (colInfo.original_type_str == "INSTRUCTION_ID")
            {
                colInfo.type = Vector::ColumnDataType::INSTRUCTION_ID;
                instructionIdColumnIndex = columns.size();
            }
            else if (colInfo.original_type_str == "TIMESET_ID")
            {
                colInfo.type = Vector::ColumnDataType::TIMESET_ID;
                timeSetColumnIndex = columns.size();
            }
            else if (colInfo.original_type_str == "PIN_STATE_ID")
                colInfo.type = Vector::ColumnDataType::PIN_STATE_ID;
            else
                colInfo.type = Vector::ColumnDataType::TEXT; // 默认类型

            columns.append(colInfo);
        }

        if (timeSetColumnIndex == -1)
        {
            qWarning() << "填充TimeSet - 未找到TimeSet列，尝试修复表结构";

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
                qWarning() << "填充TimeSet - 添加TimeSet列失败:" << addTimeSetColQuery.lastError().text();
                throw std::runtime_error(("未找到TimeSet类型的列，且尝试添加时失败。无法执行填充操作。"));
            }

            qDebug() << "填充TimeSet - 成功添加TimeSet列，重新获取列定义";

            // 重新查询列定义
            colQuery.clear();
            colQuery.prepare("SELECT id, column_name, column_order, column_type, data_properties, IsVisible "
                             "FROM VectorTableColumnConfiguration "
                             "WHERE master_record_id = ? ORDER BY column_order");
            colQuery.addBindValue(tableId);

            if (!colQuery.exec())
            {
                qWarning() << "填充TimeSet - 重新查询列定义失败:" << colQuery.lastError().text();
                throw std::runtime_error(("查询列定义失败: " + colQuery.lastError().text()).toStdString());
            }

            // 重新解析列定义
            columns.clear();
            timeSetColumnIndex = -1;
            instructionIdColumnIndex = -1;

            while (colQuery.next())
            {
                Vector::ColumnInfo colInfo;
                colInfo.id = colQuery.value(0).toInt();
                colInfo.name = colQuery.value(1).toString();
                colInfo.order = colQuery.value(2).toInt();
                colInfo.original_type_str = colQuery.value(3).toString();

                // 解析data_properties
                QString dataPropertiesStr = colQuery.value(4).toString();
                QJsonDocument jsonDoc = QJsonDocument::fromJson(dataPropertiesStr.toUtf8());
                if (!jsonDoc.isNull() && jsonDoc.isObject())
                {
                    colInfo.data_properties = jsonDoc.object();
                }
                else
                {
                    colInfo.data_properties = QJsonObject();
                }

                colInfo.is_visible = colQuery.value(5).toBool();

                // 映射列类型字符串到枚举
                if (colInfo.original_type_str == "TEXT")
                    colInfo.type = Vector::ColumnDataType::TEXT;
                else if (colInfo.original_type_str == "INTEGER")
                    colInfo.type = Vector::ColumnDataType::INTEGER;
                else if (colInfo.original_type_str == "BOOLEAN")
                    colInfo.type = Vector::ColumnDataType::BOOLEAN;
                else if (colInfo.original_type_str == "INSTRUCTION_ID")
                {
                    colInfo.type = Vector::ColumnDataType::INSTRUCTION_ID;
                    instructionIdColumnIndex = columns.size();
                }
                else if (colInfo.original_type_str == "TIMESET_ID")
                {
                    colInfo.type = Vector::ColumnDataType::TIMESET_ID;
                    timeSetColumnIndex = columns.size();
                }
                else if (colInfo.original_type_str == "PIN_STATE_ID")
                    colInfo.type = Vector::ColumnDataType::PIN_STATE_ID;
                else
                    colInfo.type = Vector::ColumnDataType::TEXT; // 默认类型

                columns.append(colInfo);
            }

            // 再次检查是否找到TimeSet列
            if (timeSetColumnIndex == -1)
            {
                qWarning() << "填充TimeSet - 修复后仍未找到TimeSet列，放弃操作";
                throw std::runtime_error(("修复后仍未找到TimeSet类型的列，无法执行填充操作"));
            }

            qDebug() << "填充TimeSet - 成功修复表结构并找到TimeSet列，索引:" << timeSetColumnIndex;
        }

        // 5. 读取二进制文件数据
        QList<Vector::RowData> allRows;
        int schemaVersion = 1; // 默认值

        if (!Persistence::BinaryFileHelper::readAllRowsFromBinary(absoluteBinFilePath, columns, schemaVersion, allRows))
        {
            qWarning() << "填充TimeSet - 读取二进制文件失败:" << absoluteBinFilePath;
            QString errorMsg = QString("从二进制文件读取数据失败");
            throw std::runtime_error(errorMsg.toStdString());
        }

        qDebug() << "填充TimeSet - 从二进制文件读取到" << allRows.size() << "行数据";

        // 如果行数为0，提示用户
        if (allRows.size() == 0)
        {
            qWarning() << "填充TimeSet - 二进制文件中没有数据，无需填充";
            QMessageBox::warning(this, tr("警告"), tr("向量表中没有数据，无需填充"));
            db.rollback();
            return;
        }

        // 6. 更新内存中的TimeSet数据
        int updatedRowCount = 0;
        for (int i = 0; i < allRows.size(); i++)
        {
            Vector::RowData &rowData = allRows[i];

            // 检查行数据大小是否合法
            if (rowData.size() <= timeSetColumnIndex)
            {
                qWarning() << "填充TimeSet - 行" << i << "数据列数" << rowData.size()
                           << "小于TimeSet列索引" << timeSetColumnIndex << "，跳过此行";
                continue;
            }

            // 检查当前行是否是要更新的行
            bool shouldProcessThisRow = selectedUiRows.isEmpty(); // 如果没有选择行，处理所有行

            // 如果用户选择了特定行，检查当前行是否在选择范围内
            if (!shouldProcessThisRow && i < allRows.size())
            {
                // 注意：selectedUiRows中存储的是UI中的行索引，这与二进制文件中的行索引i可能不完全一致
                shouldProcessThisRow = selectedUiRows.contains(i);

                if (shouldProcessThisRow)
                {
                    qDebug() << "填充TimeSet - 行 " << i << " 在用户选择的行列表中";
                }
            }

            if (shouldProcessThisRow)
            {
                // 直接更新为新的TimeSet ID
                int oldTimeSetId = rowData[timeSetColumnIndex].toInt();
                rowData[timeSetColumnIndex] = timeSetId;
                updatedRowCount++;
                qDebug() << "填充TimeSet - 已更新行 " << i << " 的TimeSet ID 从 " << oldTimeSetId << " 到 " << timeSetId;
            }
        }

        qDebug() << "填充TimeSet - 内存中更新了" << updatedRowCount << "行数据";

        // 7. 写回二进制文件
        if (!Persistence::BinaryFileHelper::writeAllRowsToBinary(absoluteBinFilePath, columns, schemaVersion, allRows))
        {
            qWarning() << "填充TimeSet - 写入二进制文件失败:" << absoluteBinFilePath;
            QString errorMsg = QString("将更新后的数据写回二进制文件失败");
            throw std::runtime_error(errorMsg.toStdString());
        }

        qDebug() << "填充TimeSet - 已成功写入二进制文件:" << absoluteBinFilePath;

        // 更新数据库中的记录
        QList<int> idsToUpdate; // 存储要更新的数据库行的ID

        if (!selectedUiRows.isEmpty())
        {
            // 1. 获取表中所有行的ID，按sort_index排序
            QList<int> allRowIds;
            QSqlQuery idQuery(db);
            QString idSql = QString("SELECT id FROM vector_table_data WHERE table_id = %1 ORDER BY sort_index").arg(tableId);
            qDebug() << "填充TimeSet - 查询所有行ID:" << idSql;
            if (!idQuery.exec(idSql))
            {
                throw std::runtime_error(("查询行ID失败: " + idQuery.lastError().text()).toStdString());
            }
            while (idQuery.next())
            {
                allRowIds.append(idQuery.value(0).toInt());
            }
            qDebug() << "填充TimeSet - 数据库中共有" << allRowIds.size() << "行";

            if (allRows.size() > allRowIds.size())
            {
                qWarning() << "数据不一致：二进制文件有" << allRows.size() << "行，但数据库只有" << allRowIds.size() << "行。将补全缺失的数据库记录。";
                QSqlQuery insertQuery(db);
                insertQuery.prepare("INSERT INTO vector_table_data (table_id, sort_index, timeset_id, instruction_id, comment, label) "
                                    "VALUES (?, ?, ?, ?, '', '')");

                int existingRows = allRowIds.size();
                for (int i = existingRows; i < allRows.size(); ++i)
                {
                    // 确保行数据存在且TimeSet/Instruction列索引有效
                    if (i < allRows.size() && timeSetColumnIndex >= 0 && timeSetColumnIndex < allRows[i].size() && instructionIdColumnIndex >= 0 && instructionIdColumnIndex < allRows[i].size())
                    {
                        insertQuery.bindValue(0, tableId);
                        insertQuery.bindValue(1, i); // sort_index
                        insertQuery.bindValue(2, allRows[i][timeSetColumnIndex].toInt());
                        insertQuery.bindValue(3, allRows[i][instructionIdColumnIndex].toInt());

                        if (!insertQuery.exec())
                        {
                            throw std::runtime_error(("补全数据库行失败: " + insertQuery.lastError().text()).toStdString());
                        }
                        allRowIds.append(insertQuery.lastInsertId().toInt());
                    }
                    else
                    {
                        qWarning() << "跳过为行" << i << "补全数据库记录，因为数据格式无效或缺少必要的列(TimeSet/Instruction)。";
                    }
                }
                qDebug() << "成功补全" << (allRowIds.size() - existingRows) << "行数据库记录。";
            }

            // 2. 根据选中的UI行索引，找到对应的数据库ID
            foreach (int uiRow, selectedUiRows)
            {
                if (uiRow >= 0 && uiRow < allRowIds.size())
                {
                    idsToUpdate.append(allRowIds[uiRow]);
                }
                else
                {
                    qDebug() << "填充TimeSet警告 - UI行索引" << uiRow << "无效，忽略";
                }
            }
            if (idsToUpdate.isEmpty())
            {
                qDebug() << "填充TimeSet - 没有有效的数据库ID需要更新，操作取消";
                db.rollback(); // 不需要执行事务，直接回滚
                return;
            }
            qDebug() << "填充TimeSet - 准备更新的数据库ID:" << idsToUpdate;
        }

        // 准备更新SQL
        QSqlQuery query(db);
        QString updateSQL;
        if (!idsToUpdate.isEmpty())
        {
            // 如果有选定行，则基于ID更新
            QString idPlaceholders = QString("?,").repeated(idsToUpdate.size());
            idPlaceholders.chop(1); // 移除最后一个逗号
            updateSQL = QString("UPDATE vector_table_data SET timeset_id = ? WHERE id IN (%1)").arg(idPlaceholders);
            query.prepare(updateSQL);
            query.addBindValue(timeSetId);
            foreach (int id, idsToUpdate)
            {
                query.addBindValue(id);
            }
            qDebug() << "填充TimeSet - 执行SQL (按ID):" << updateSQL;
            qDebug() << "参数: timesetId=" << timeSetId << ", IDs=" << idsToUpdate;
        }
        else
        {
            // 如果没有选定行，则更新整个表
            updateSQL = "UPDATE vector_table_data SET timeset_id = :timesetId WHERE table_id = :tableId";
            query.prepare(updateSQL);
            query.bindValue(":timesetId", timeSetId);
            query.bindValue(":tableId", tableId);
            qDebug() << "填充TimeSet - 执行SQL (全表):" << updateSQL;
            qDebug() << "参数: timesetId=" << timeSetId << ", tableId=" << tableId;
        }

        if (!query.exec())
        {
            QString errorText = query.lastError().text();
            qDebug() << "填充TimeSet失败 - SQL错误:" << errorText;
            throw std::runtime_error(errorText.toStdString());
        }

        int rowsAffected = query.numRowsAffected();
        qDebug() << "填充TimeSet - 已更新" << rowsAffected << "行";

        // 提交事务
        if (!db.commit())
        {
            QString errorText = db.lastError().text();
            qDebug() << "填充TimeSet失败 - 提交事务失败:" << errorText;
            throw std::runtime_error(errorText.toStdString());
        }

        // 注意：不要在这里调用onVectorTableSelectionChanged，因为它会重置页码
        qDebug() << "填充TimeSet - 准备重新加载表格数据，将保留当前页码:" << m_currentPage;

        // 显示成功消息
        QMessageBox::information(this, tr("成功"), tr("已将 %1 填充到选中区域，共更新了 %2 行数据").arg(timeSetName).arg(updatedRowCount));
        qDebug() << "填充TimeSet - 操作成功完成";

        // 保存当前页码并仅刷新当前页数据，不改变页码状态
        // 直接加载当前页数据
        qDebug() << "填充TimeSet - 刷新当前页数据，保持在页码:" << m_currentPage;
        loadCurrentPage();

        // 更新分页信息显示
        updatePaginationInfo();

        // 更新波形图以反映TimeSet变更
        if (m_isWaveformVisible && m_waveformPlot)
        {
            qDebug() << "MainWindow::fillTimeSetForVectorTable - 更新波形图以反映TimeSet变更";

            // 强制刷新数据库连接，确保没有缓存问题
            QSqlDatabase refreshDb = DatabaseManager::instance()->database();
            if (refreshDb.isOpen())
            {
                qDebug() << "MainWindow::fillTimeSetForVectorTable - 刷新数据库缓存";
                refreshDb.transaction();
                refreshDb.commit();
            }

            // 短暂延迟以确保数据库变更已经完成
            QTimer::singleShot(100, this, [this]()
                               {
                qDebug() << "MainWindow::fillTimeSetForVectorTable - 延迟更新波形图";
                updateWaveformView(); });

            // 同时也立即更新一次
            updateWaveformView();
        }
    }
    catch (const std::exception &e)
    {
        // 回滚事务
        db.rollback();
        qDebug() << "填充TimeSet - 操作失败，已回滚事务:" << e.what();

        // 显示错误消息
        QMessageBox::critical(this, tr("错误"), tr("填充TimeSet失败: %1").arg(e.what()));
    }
}

// 显示填充TimeSet对话框
void MainWindow::showFillTimeSetDialog()
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
    int rowCount = m_vectorTableModel->rowCount();
    if (rowCount <= 0)
    {
        QMessageBox::warning(this, "操作失败", "当前向量表没有数据行");
        return;
    }

    // 显示填充TimeSet对话框
    FillTimeSetDialog dialog(this);
    // 设置向量表总行数
    dialog.setVectorRowCount(rowCount);

    // 获取选中的单元格并计算行范围
    QModelIndexList selectedIndexes = m_vectorTableView->selectionModel()->selectedIndexes();
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
        int timeSetId = dialog.getSelectedTimeSetId();
        if (timeSetId <= 0)
        {
            return; // No valid timeset selected
        }

        QList<int> rowsToUpdate;
        // 修复BUG：无论是否选中行，都使用对话框中设置的范围
        int fromRow = dialog.getStartRow(); // 0-based
        int toRow = dialog.getEndRow();     // 0-based

        // Ensure the range is valid.
        if (fromRow < 0 || toRow < fromRow || toRow >= rowCount)
        {
            QMessageBox::warning(this, "范围无效", "指定的行范围无效。");
            return;
        }

        for (int i = fromRow; i <= toRow; ++i)
        {
            rowsToUpdate.append(i);
        }

        // Call the function to perform the update
        fillTimeSetForVectorTable(timeSetId, rowsToUpdate);
    }
}

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
    int tableId = m_vectorTableSelector->itemData(currentIndex).toInt();
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
        colQuery.prepare("SELECT id, column_name, column_order, column_type, data_properties, IsVisible "
                         "FROM VectorTableColumnConfiguration "
                         "WHERE master_record_id = ? ORDER BY column_order");
        colQuery.addBindValue(tableId);

        if (!colQuery.exec())
        {
            QString errorText = colQuery.lastError().text();
            qWarning() << "替换TimeSet - 查询列定义失败:" << errorText;
            throw std::runtime_error(("查询列定义失败: " + errorText).toStdString());
        }

        QList<Vector::ColumnInfo> columns;
        int timeSetColumnIndex = -1; // 用于标记TimeSet列的索引
        int instructionIdColumnIndex = -1;

        while (colQuery.next())
        {
            Vector::ColumnInfo colInfo;
            colInfo.id = colQuery.value(0).toInt();
            colInfo.name = colQuery.value(1).toString();
            colInfo.order = colQuery.value(2).toInt();
            colInfo.original_type_str = colQuery.value(3).toString();

            // 解析data_properties
            QString dataPropertiesStr = colQuery.value(4).toString();
            QJsonDocument jsonDoc = QJsonDocument::fromJson(dataPropertiesStr.toUtf8());
            if (!jsonDoc.isNull() && jsonDoc.isObject())
            {
                colInfo.data_properties = jsonDoc.object();
            }
            else
            {
                // 如果解析失败，创建一个空的QJsonObject
                colInfo.data_properties = QJsonObject();
            }

            colInfo.is_visible = colQuery.value(5).toBool();

            // 映射列类型字符串到枚举
            if (colInfo.original_type_str == "TEXT")
                colInfo.type = Vector::ColumnDataType::TEXT;
            else if (colInfo.original_type_str == "INTEGER")
                colInfo.type = Vector::ColumnDataType::INTEGER;
            else if (colInfo.original_type_str == "BOOLEAN")
                colInfo.type = Vector::ColumnDataType::BOOLEAN;
            else if (colInfo.original_type_str == "INSTRUCTION_ID")
            {
                colInfo.type = Vector::ColumnDataType::INSTRUCTION_ID;
                instructionIdColumnIndex = columns.size();
            }
            else if (colInfo.original_type_str == "TIMESET_ID")
            {
                colInfo.type = Vector::ColumnDataType::TIMESET_ID;
                timeSetColumnIndex = columns.size(); // 记录TimeSet列的索引
            }
            else if (colInfo.original_type_str == "PIN_STATE_ID")
                colInfo.type = Vector::ColumnDataType::PIN_STATE_ID;
            else
                colInfo.type = Vector::ColumnDataType::TEXT; // 默认类型

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
            colQuery.clear();
            colQuery.prepare("SELECT id, column_name, column_order, column_type, data_properties, IsVisible "
                             "FROM VectorTableColumnConfiguration "
                             "WHERE master_record_id = ? ORDER BY column_order");
            colQuery.addBindValue(tableId);

            if (!colQuery.exec())
            {
                qWarning() << "替换TimeSet - 重新查询列定义失败:" << colQuery.lastError().text();
                throw std::runtime_error(("查询列定义失败: " + colQuery.lastError().text()).toStdString());
            }

            // 重新解析列定义
            columns.clear();
            timeSetColumnIndex = -1;
            instructionIdColumnIndex = -1;

            while (colQuery.next())
            {
                Vector::ColumnInfo colInfo;
                colInfo.id = colQuery.value(0).toInt();
                colInfo.name = colQuery.value(1).toString();
                colInfo.order = colQuery.value(2).toInt();
                colInfo.original_type_str = colQuery.value(3).toString();

                // 解析data_properties
                QString dataPropertiesStr = colQuery.value(4).toString();
                QJsonDocument jsonDoc = QJsonDocument::fromJson(dataPropertiesStr.toUtf8());
                if (!jsonDoc.isNull() && jsonDoc.isObject())
                {
                    colInfo.data_properties = jsonDoc.object();
                }
                else
                {
                    colInfo.data_properties = QJsonObject();
                }

                colInfo.is_visible = colQuery.value(5).toBool();

                // 映射列类型字符串到枚举
                if (colInfo.original_type_str == "TEXT")
                    colInfo.type = Vector::ColumnDataType::TEXT;
                else if (colInfo.original_type_str == "INTEGER")
                    colInfo.type = Vector::ColumnDataType::INTEGER;
                else if (colInfo.original_type_str == "BOOLEAN")
                    colInfo.type = Vector::ColumnDataType::BOOLEAN;
                else if (colInfo.original_type_str == "INSTRUCTION_ID")
                {
                    colInfo.type = Vector::ColumnDataType::INSTRUCTION_ID;
                    instructionIdColumnIndex = columns.size();
                }
                else if (colInfo.original_type_str == "TIMESET_ID")
                {
                    colInfo.type = Vector::ColumnDataType::TIMESET_ID;
                    timeSetColumnIndex = columns.size(); // 记录TimeSet列的索引
                }
                else if (colInfo.original_type_str == "PIN_STATE_ID")
                    colInfo.type = Vector::ColumnDataType::PIN_STATE_ID;
                else
                    colInfo.type = Vector::ColumnDataType::TEXT; // 默认类型

                columns.append(colInfo);
            }

            // 再次检查是否找到TimeSet列
            if (timeSetColumnIndex == -1)
            {
                qWarning() << "替换TimeSet - 修复后仍未找到TimeSet列，放弃操作";
                throw std::runtime_error(("修复后仍未找到TimeSet类型的列，无法执行替换操作"));
            }

            qDebug() << "替换TimeSet - 成功修复表结构并找到TimeSet列，索引:" << timeSetColumnIndex;
        }

        // 5. 读取二进制文件数据
        QList<Vector::RowData> allRows;
        int schemaVersion = 1; // 默认值

        if (!Persistence::BinaryFileHelper::readAllRowsFromBinary(absoluteBinFilePath, columns, schemaVersion, allRows))
        {
            qWarning() << "替换TimeSet - 读取二进制文件失败:" << absoluteBinFilePath;
            QString errorMsg = QString("从二进制文件读取数据失败");
            throw std::runtime_error(errorMsg.toStdString());
        }

        qDebug() << "替换TimeSet - 从二进制文件读取到" << allRows.size() << "行数据";

        // 如果行数为0，提示用户
        if (allRows.size() == 0)
        {
            qWarning() << "替换TimeSet - 二进制文件中没有数据，无需替换";
            QMessageBox::warning(this, tr("警告"), tr("向量表中没有数据，无需替换"));
            db.rollback();
            return;
        }

        // 输出所有行的TimeSet ID和预期替换值
        qDebug() << "替换TimeSet - 预期将TimeSet ID " << fromTimeSetId << " 替换为 " << toTimeSetId;
        qDebug() << "替换TimeSet - TimeSet列索引: " << timeSetColumnIndex;
        for (int i = 0; i < allRows.size(); i++)
        {
            if (allRows[i].size() > timeSetColumnIndex)
            {
                int rowTimeSetId = allRows[i][timeSetColumnIndex].toInt();
                qDebug() << "替换TimeSet - 行 " << i << " 的TimeSet ID: " << rowTimeSetId
                         << (rowTimeSetId == fromTimeSetId ? " (匹配)" : " (不匹配)");
            }
        }

        // 创建名称到ID的映射
        QMap<QString, int> timeSetNameToIdMap;
        QSqlQuery allTimeSetQuery(db);
        if (allTimeSetQuery.exec("SELECT id, timeset_name FROM timeset_list"))
        {
            while (allTimeSetQuery.next())
            {
                int id = allTimeSetQuery.value(0).toInt();
                QString name = allTimeSetQuery.value(1).toString();
                timeSetNameToIdMap[name] = id;
                qDebug() << "替换TimeSet - TimeSet名称映射: " << name << " -> ID: " << id;
            }
        }

        // 6. 更新内存中的TimeSet数据
        int updatedRowCount = 0;
        for (int i = 0; i < allRows.size(); i++)
        {
            Vector::RowData &rowData = allRows[i];

            // 检查行数据大小是否合法
            if (rowData.size() <= timeSetColumnIndex)
            {
                qWarning() << "替换TimeSet - 行" << i << "数据列数" << rowData.size()
                           << "小于TimeSet列索引" << timeSetColumnIndex << "，跳过此行";
                continue;
            }

            // 检查当前行是否是要更新的行
            bool shouldProcessThisRow = selectedUiRows.isEmpty(); // 如果没有选择行，处理所有行

            // 如果用户选择了特定行，检查当前行是否在选择范围内
            if (!shouldProcessThisRow && i < allRows.size())
            {
                // 注意：selectedUiRows中存储的是UI中的行索引，这与二进制文件中的行索引i可能不完全一致
                // 如果用户选择了某行，我们将处理它，无论它在二进制文件中的顺序如何
                shouldProcessThisRow = selectedUiRows.contains(i);

                if (shouldProcessThisRow)
                {
                    qDebug() << "替换TimeSet - 行 " << i << " 在用户选择的行列表中";
                }
            }

            if (shouldProcessThisRow)
            {
                int currentTimeSetId = rowData[timeSetColumnIndex].toInt();
                qDebug() << "替换TimeSet - 处理行 " << i << "，当前TimeSet ID: " << currentTimeSetId
                         << "，fromTimeSetId: " << fromTimeSetId;

                // 尝试直接通过ID匹配
                if (currentTimeSetId == fromTimeSetId)
                {
                    // 更新TimeSet ID
                    rowData[timeSetColumnIndex] = toTimeSetId;
                    updatedRowCount++;
                    qDebug() << "替换TimeSet - 已更新行 " << i << " 的TimeSet ID 从 " << fromTimeSetId << " 到 " << toTimeSetId;
                }
                // 尝试通过名称匹配（获取当前ID对应的名称，检查是否与fromTimeSetName匹配）
                else
                {
                    // 获取当前ID对应的名称
                    QString currentTimeSetName = "未知";
                    QSqlQuery currentTSNameQuery(db);
                    currentTSNameQuery.prepare("SELECT timeset_name FROM timeset_list WHERE id = ?");
                    currentTSNameQuery.addBindValue(currentTimeSetId);
                    if (currentTSNameQuery.exec() && currentTSNameQuery.next())
                    {
                        currentTimeSetName = currentTSNameQuery.value(0).toString();
                    }

                    // 如果当前TimeSet名称与fromTimeSetName匹配，则更新
                    if (currentTimeSetName == fromTimeSetName)
                    {
                        // 更新TimeSet ID
                        rowData[timeSetColumnIndex] = toTimeSetId;
                        updatedRowCount++;
                        qDebug() << "替换TimeSet - 已通过名称匹配更新行 " << i
                                 << " 的TimeSet从 " << currentTimeSetName << " (ID:" << currentTimeSetId
                                 << ") 到 " << toTimeSetName << " (ID:" << toTimeSetId << ")";
                    }
                }
            }
        }

        qDebug() << "替换TimeSet - 内存中更新了" << updatedRowCount << "行数据";

        if (updatedRowCount == 0)
        {
            QString selectMessage;
            if (selectedUiRows.isEmpty())
            {
                selectMessage = tr("所有行");
            }
            else
            {
                QStringList rowStrings;
                for (int row : selectedUiRows)
                {
                    rowStrings << QString::number(row + 1); // 转换为1-based显示给用户
                }
                selectMessage = tr("选中的行 (%1)").arg(rowStrings.join(", "));
            }

            qDebug() << "替换TimeSet - 在" << selectMessage << "中没有找到使用ID " << fromTimeSetId
                     << " 或名称 " << fromTimeSetName << " 的TimeSet行";

            // 检查二进制文件中的行使用的TimeSet IDs
            QSet<int> usedTimeSetIds;
            for (int i = 0; i < allRows.size(); i++)
            {
                // 只统计选中行或全部行（如果没有选择）
                if (selectedUiRows.isEmpty() || selectedUiRows.contains(i))
                {
                    if (allRows[i].size() > timeSetColumnIndex)
                    {
                        int usedId = allRows[i][timeSetColumnIndex].toInt();
                        usedTimeSetIds.insert(usedId);
                    }
                }
            }

            qDebug() << "替换TimeSet - 在" << selectMessage << "中使用的TimeSet IDs:" << usedTimeSetIds.values();

            // 获取每个使用的ID对应的名称
            QStringList usedTimeSetNames;
            for (int usedId : usedTimeSetIds)
            {
                QSqlQuery nameQuery(db);
                nameQuery.prepare("SELECT timeset_name FROM timeset_list WHERE id = ?");
                nameQuery.addBindValue(usedId);
                QString tsName = "未知";
                if (nameQuery.exec() && nameQuery.next())
                {
                    tsName = nameQuery.value(0).toString();
                    usedTimeSetNames.append(tsName);
                }

                qDebug() << "替换TimeSet - 在" << selectMessage << "中使用的TimeSet ID: " << usedId << ", 名称: " << tsName;
            }

            // 提示用户没有找到匹配的TimeSet
            QString usedInfo;
            if (!usedTimeSetNames.isEmpty())
            {
                usedInfo = tr("表中使用的TimeSet有: %1").arg(usedTimeSetNames.join(", "));
            }
            else
            {
                usedInfo = tr("表中没有使用任何TimeSet");
            }

            QMessageBox::information(this, tr("提示"),
                                     tr("在%1中没有找到使用 %2 (ID: %3) 的数据行需要替换。\n%4")
                                         .arg(selectMessage)
                                         .arg(fromTimeSetName)
                                         .arg(fromTimeSetId)
                                         .arg(usedInfo));
            db.rollback();
            return;
        }

        // 7. 写回二进制文件
        if (!Persistence::BinaryFileHelper::writeAllRowsToBinary(absoluteBinFilePath, columns, schemaVersion, allRows))
        {
            qWarning() << "替换TimeSet - 写入二进制文件失败:" << absoluteBinFilePath;
            QString errorMsg = QString("将更新后的数据写回二进制文件失败");
            throw std::runtime_error(errorMsg.toStdString());
        }

        qDebug() << "替换TimeSet - 已成功写入二进制文件:" << absoluteBinFilePath;

        // 8. 更新数据库中的TimeSet元数据
        qDebug() << "替换TimeSet - 开始更新数据库元数据...";
        QList<int> allRowIds;
        QSqlQuery idQuery(db);
        QString idSql = QString("SELECT id FROM vector_table_data WHERE table_id = %1 ORDER BY sort_index").arg(tableId);
        if (!idQuery.exec(idSql))
        {
            throw std::runtime_error(("查询行ID失败: " + idQuery.lastError().text()).toStdString());
        }
        while (idQuery.next())
        {
            allRowIds.append(idQuery.value(0).toInt());
        }

        // 自动修复：检查并补全缺失的数据库行
        if (allRows.size() > allRowIds.size())
        {
            qWarning() << "数据不一致：二进制文件有" << allRows.size() << "行，但数据库只有" << allRowIds.size() << "行。将补全缺失的数据库记录。";
            QSqlQuery insertQuery(db);
            insertQuery.prepare("INSERT INTO vector_table_data (table_id, sort_index, timeset_id, instruction_id, comment, label) "
                                "VALUES (?, ?, ?, ?, '', '')");
            int existingRows = allRowIds.size();
            for (int i = existingRows; i < allRows.size(); ++i)
            {
                if (i < allRows.size() && timeSetColumnIndex >= 0 && timeSetColumnIndex < allRows[i].size() && instructionIdColumnIndex >= 0 && instructionIdColumnIndex < allRows[i].size())
                {
                    insertQuery.bindValue(0, tableId);
                    insertQuery.bindValue(1, i);
                    insertQuery.bindValue(2, allRows[i][timeSetColumnIndex].toInt());
                    insertQuery.bindValue(3, allRows[i][instructionIdColumnIndex].toInt());
                    if (!insertQuery.exec())
                    {
                        throw std::runtime_error(("补全数据库行失败: " + insertQuery.lastError().text()).toStdString());
                    }
                    allRowIds.append(insertQuery.lastInsertId().toInt());
                }
                else
                {
                    qWarning() << "跳过为行" << i << "补全数据库记录，因为数据格式无效或缺少必要的列(TimeSet/Instruction)。";
                }
            }
            qDebug() << "成功补全" << (allRowIds.size() - existingRows) << "行数据库记录。";
        }

        // 更新现有行的TimeSet ID
        QSqlQuery updateQuery(db);
        updateQuery.prepare("UPDATE vector_table_data SET timeset_id = ? WHERE id = ?");
        int dbUpdatedCount = 0;
        for (int i = 0; i < allRows.size(); ++i)
        {
            if (i >= allRowIds.size() || i >= allRows.size())
                continue;

            bool wasProcessed = false;
            if (selectedUiRows.isEmpty())
            {
                wasProcessed = true; // Process all if no selection
            }
            else
            {
                wasProcessed = selectedUiRows.contains(i);
            }

            // 只更新被替换的行
            if (wasProcessed)
            {
                int currentDbId = allRowIds[i];
                int newTimeSetId = allRows[i][timeSetColumnIndex].toInt();
                updateQuery.bindValue(0, newTimeSetId);
                updateQuery.bindValue(1, currentDbId);
                if (updateQuery.exec())
                {
                    if (updateQuery.numRowsAffected() > 0)
                    {
                        dbUpdatedCount++;
                    }
                }
                else
                {
                    qWarning() << "更新数据库失败，行索引:" << i << ", DB ID:" << currentDbId << ", 错误:" << updateQuery.lastError().text();
                }
            }
        }
        qDebug() << "替换TimeSet - 数据库元数据更新完成，共更新" << dbUpdatedCount << "行。";

        // 提交事务
        if (!db.commit())
        {
            QString errorText = db.lastError().text();
            qDebug() << "替换TimeSet失败 - 提交事务失败:" << errorText;
            throw std::runtime_error(errorText.toStdString());
        }

        // 注意：不要在这里调用onVectorTableSelectionChanged，因为它会重置页码
        qDebug() << "替换TimeSet - 准备重新加载表格数据，将保留当前页码:" << m_currentPage;

        // 显示成功消息
        QMessageBox::information(this, tr("成功"), tr("已将 %1 替换为 %2，共更新了 %3 行数据").arg(fromTimeSetName).arg(toTimeSetName).arg(updatedRowCount));
        qDebug() << "替换TimeSet - 操作成功完成，共更新" << updatedRowCount << "行数据";

        // 保存当前页码并仅刷新当前页数据，不改变页码状态
        // 直接加载当前页数据
        qDebug() << "替换TimeSet - 刷新当前页数据，保持在页码:" << m_currentPage;
        loadCurrentPage();

        // 更新分页信息显示
        updatePaginationInfo();

        // 更新波形图以反映TimeSet变更
        if (m_isWaveformVisible && m_waveformPlot)
        {
            qDebug() << "MainWindow::replaceTimeSetForVectorTable - 更新波形图以反映TimeSet变更";

            // 强制刷新数据库连接，确保没有缓存问题
            QSqlDatabase refreshDb = DatabaseManager::instance()->database();
            if (refreshDb.isOpen())
            {
                qDebug() << "MainWindow::replaceTimeSetForVectorTable - 刷新数据库缓存";
                refreshDb.transaction();
                refreshDb.commit();
            }

            // 短暂延迟以确保数据库变更已经完成
            QTimer::singleShot(100, this, [this]()
                               {
                qDebug() << "MainWindow::replaceTimeSetForVectorTable - 延迟更新波形图";
                updateWaveformView(); });

            // 同时也立即更新一次
            updateWaveformView();
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
    int rowCount = m_vectorTableModel->rowCount();
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
    QModelIndexList selectedIndexes = m_vectorTableView->selectionModel()->selectedIndexes();
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

// 添加单个管脚
void MainWindow::addSinglePin()
{
    qDebug() << "MainWindow::addSinglePin - 开始添加单个管脚";

    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        QMessageBox::warning(this, tr("警告"), tr("请先打开或创建一个项目数据库"));
        return;
    }

    // 弹出输入对话框
    bool ok;
    QString pinName = QInputDialog::getText(this, tr("添加管脚"),
                                            tr("请输入管脚名称:"), QLineEdit::Normal,
                                            "", &ok);
    if (ok && !pinName.isEmpty())
    {
        // 添加到数据库
        QList<QString> pins;
        pins << pinName;
        if (addPinsToDatabase(pins))
        {
            statusBar()->showMessage(tr("管脚 \"%1\" 添加成功").arg(pinName));

            // 刷新侧边导航栏
            refreshSidebarNavigator();
        }
    }
}

// 删除管脚
void MainWindow::deletePins()
{
    qDebug() << "MainWindow::deletePins - 开始删除管脚";

    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        QMessageBox::warning(this, tr("警告"), tr("请先打开或创建一个项目数据库"));
        return;
    }

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();

    // 查询现有管脚
    QSqlQuery query(db);
    query.exec("SELECT id, pin_name FROM pin_list ORDER BY pin_name");

    // 构建管脚列表
    QMap<int, QString> pinMap;
    while (query.next())
    {
        int id = query.value(0).toInt();
        QString name = query.value(1).toString();
        pinMap.insert(id, name);
    }

    if (pinMap.isEmpty())
    {
        QMessageBox::information(this, tr("提示"), tr("当前没有可删除的管脚"));
        return;
    }

    // 创建选择对话框
    QDialog dialog(this);
    dialog.setWindowTitle(tr("选择要删除的管脚"));
    dialog.setMinimumWidth(350);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QLabel *label = new QLabel(tr("请选择要删除的管脚:"), &dialog);
    layout->addWidget(label);

    // 使用带复选框的滚动区域
    QScrollArea *scrollArea = new QScrollArea(&dialog);
    scrollArea->setWidgetResizable(true);
    QWidget *scrollContent = new QWidget(scrollArea);
    QVBoxLayout *checkBoxLayout = new QVBoxLayout(scrollContent);

    QMap<int, QCheckBox *> checkBoxes;
    for (auto it = pinMap.begin(); it != pinMap.end(); ++it)
    {
        QCheckBox *checkBox = new QCheckBox(it.value(), scrollContent);
        checkBoxes[it.key()] = checkBox;
        checkBoxLayout->addWidget(checkBox);
    }
    scrollContent->setLayout(checkBoxLayout);
    scrollArea->setWidget(scrollContent);
    layout->addWidget(scrollArea);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);

    // 显示对话框并等待用户操作
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    // 收集选中的管脚
    QList<int> pinIdsToDelete;
    QStringList pinNamesToDelete;
    for (auto it = pinMap.begin(); it != pinMap.end(); ++it)
    {
        if (checkBoxes.value(it.key())->isChecked())
        {
            pinIdsToDelete.append(it.key());
            pinNamesToDelete.append(it.value());
        }
    }

    if (pinIdsToDelete.isEmpty())
    {
        QMessageBox::information(this, tr("提示"), tr("未选择任何管脚"));
        return;
    }

    // 检查管脚使用情况
    QMap<QString, QStringList> pinUsageMap; // pinName -> [table1, table2]
    QSqlQuery findUsageQuery(db);

    for (int pinId : pinIdsToDelete)
    {
        // 根据新的 schema, 检查 VectorTableColumnConfiguration, 仅当管脚可见时才视为"正在使用"
        // 修复：同时检查 'pin_id' 和 'pin_list_id'，以兼容新旧两种数据格式
        findUsageQuery.prepare(
            "SELECT m.table_name "
            "FROM VectorTableColumnConfiguration c "
            "JOIN VectorTableMasterRecord m ON c.master_record_id = m.id "
            "WHERE c.column_type = 'PIN_STATE_ID' AND c.IsVisible = 1 AND "
            "(json_extract(c.data_properties, '$.pin_list_id') = ? OR json_extract(c.data_properties, '$.pin_id') = ?)");
        findUsageQuery.addBindValue(pinId);
        findUsageQuery.addBindValue(pinId);

        if (findUsageQuery.exec())
        {
            while (findUsageQuery.next())
            {
                QString pinName = pinMap.value(pinId);
                QString tableName = findUsageQuery.value(0).toString();
                pinUsageMap[pinName].append(tableName);
            }
        }
        else
        {
            qWarning() << "检查管脚使用情况失败:" << findUsageQuery.lastError().text();
        }
    }

    // 根据使用情况弹出不同确认对话框
    if (!pinUsageMap.isEmpty())
    {
        // 如果有管脚被使用，显示详细警告，并阻止删除
        QString warningText = tr("警告：以下管脚正在被一个或多个向量表使用，无法删除：\n");
        for (auto it = pinUsageMap.begin(); it != pinUsageMap.end(); ++it)
        {
            warningText.append(tr("\n管脚 \"%1\" 被用于: %2").arg(it.key()).arg(it.value().join(", ")));
        }
        warningText.append(tr("\n\n请先从相应的向量表设置中移除这些管脚，然后再尝试删除。"));
        QMessageBox::warning(this, tr("无法删除管脚"), warningText);
        return;
    }

    // 如果没有管脚被使用，弹出标准删除确认
    QString confirmQuestion = tr("您确定要删除以下 %1 个管脚吗？\n\n%2\n\n此操作不可恢复，并将删除所有相关的配置（如Pin Settings, Group Settings等）。")
                                  .arg(pinNamesToDelete.size())
                                  .arg(pinNamesToDelete.join("\n"));
    if (QMessageBox::question(this, tr("确认删除"), confirmQuestion, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
    {
        return;
    }

    // 执行删除操作 (借鉴 PinSettingsDialog 的逻辑)
    db.transaction();
    bool success = true;
    QString errorMsg;

    try
    {
        QSqlQuery updateQuery(db);
        QSqlQuery deleteQuery(db);

        for (int pinId : pinIdsToDelete)
        {
            // 在删除管脚之前，更新所有使用该管脚的列，将它们设置为不可见
            updateQuery.prepare("UPDATE VectorTableColumnConfiguration SET IsVisible = 0 WHERE column_type = 'PIN_STATE_ID' AND json_extract(data_properties, '$.pin_list_id') = ?");
            updateQuery.addBindValue(pinId);
            if (!updateQuery.exec())
            {
                throw QString("无法将管脚列标记为不可见: " + updateQuery.lastError().text());
            }
            qDebug() << "Pin Deletion - Marked columns for pinId" << pinId << "as not visible.";

            // 注意: 这里的删除逻辑需要非常完整，确保所有关联数据都被清除
            // 1. 从 timeset_settings 删除
            deleteQuery.prepare("DELETE FROM timeset_settings WHERE pin_id = ?");
            deleteQuery.addBindValue(pinId);
            if (!deleteQuery.exec())
                throw QString(deleteQuery.lastError().text());

            // 2. 从 pin_group_members 删除
            deleteQuery.prepare("DELETE FROM pin_group_members WHERE pin_id = ?");
            deleteQuery.addBindValue(pinId);
            if (!deleteQuery.exec())
                throw QString(deleteQuery.lastError().text());

            // 3. 从 pin_settings 删除
            deleteQuery.prepare("DELETE FROM pin_settings WHERE pin_id = ?");
            deleteQuery.addBindValue(pinId);
            if (!deleteQuery.exec())
                throw QString(deleteQuery.lastError().text());

            // 4. 从 pin_list 删除 (主表)
            deleteQuery.prepare("DELETE FROM pin_list WHERE id = ?");
            deleteQuery.addBindValue(pinId);
            if (!deleteQuery.exec())
                throw QString(deleteQuery.lastError().text());
        }
    }
    catch (const QString &e)
    {
        success = false;
        errorMsg = e;
        db.rollback();
    }

    if (success)
    {
        db.commit();
        QMessageBox::information(this, tr("成功"), tr("已成功删除选中的管脚。"));
        // 刷新侧边导航栏和可能打开的视图
        refreshSidebarNavigator();
        // 如果有其他需要同步的视图，也在这里调用
    }
    else
    {
        QMessageBox::critical(this, tr("数据库错误"), tr("删除管脚时发生错误: %1").arg(errorMsg));
    }
}

bool MainWindow::addPinsToDatabase(const QList<QString> &pinNames)
{
    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        return false;
    }

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();

    // 开始事务
    db.transaction();

    // 循环添加每个管脚
    bool success = true;
    for (const QString &pinName : pinNames)
    {
        QSqlQuery query(db);
        query.prepare("INSERT INTO pin_list (pin_name, pin_note, pin_nav_note) VALUES (?, ?, ?)");
        query.addBindValue(pinName);
        query.addBindValue(""); // pin_note为空
        query.addBindValue(""); // pin_nav_note为空

        if (!query.exec())
        {
            qDebug() << "添加管脚失败:" << query.lastError().text();
            success = false;
            break;
        }
    }

    // 提交或回滚事务
    if (success)
    {
        db.commit();
        return true;
    }
    else
    {
        db.rollback();
        return false;
    }
}

void MainWindow::setupVectorTablePins()
{
    qDebug() << "MainWindow::setupVectorTablePins - 开始设置向量表管脚";

    // 获取当前选择的向量表
    if (m_vectorTableSelector->count() == 0 || m_vectorTableSelector->currentIndex() < 0)
    {
        QMessageBox::warning(this, "错误", "请先选择一个向量表");
        return;
    }
    int tableId = m_vectorTableSelector->currentData().toInt();
    QString tableName = m_vectorTableSelector->currentText();

    qDebug() << "MainWindow::setupVectorTablePins - 打开管脚设置对话框，表ID:" << tableId << "，表名:" << tableName;

    // 1. 获取更改前的完整列配置
    QList<Vector::ColumnInfo> oldColumns = VectorDataHandler::instance().getAllColumnInfo(tableId);

    // 2. 创建并显示管脚设置对话框
    VectorPinSettingsDialog dialog(tableId, tableName, this);

    if (dialog.exec() == QDialog::Accepted)
    {
        qDebug() << "MainWindow::setupVectorTablePins - 用户确认了管脚设置，调用数据迁移器。";
        // 3. 调用迁移器处理后续所有逻辑（比较、迁移、提示）
        DataMigrator::migrateDataIfNeeded(tableId, oldColumns, this);
    }
    else
    {
        qDebug() << "MainWindow::setupVectorTablePins - 用户取消了管脚设置。";
    }

    // 4. 无论成功、失败还是取消，都刷新UI以保证与数据库状态一致
    reloadAndRefreshVectorTable(tableId);
}

// 打开管脚设置对话框
void MainWindow::openPinSettingsDialog()
{
    qDebug() << "MainWindow::openPinSettingsDialog - 打开管脚设置对话框";

    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        QMessageBox::warning(this, "警告", "请先打开或创建一个项目数据库");
        return;
    }

    // 创建并显示管脚设置对话框
    PinSettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted)
    {
        qDebug() << "MainWindow::openPinSettingsDialog - 管脚设置已更新";
        // 刷新当前向量表（如果有）
        if (m_vectorTableSelector->count() > 0 && m_vectorTableSelector->currentIndex() >= 0)
        {
            onVectorTableSelectionChanged(m_vectorTableSelector->currentIndex());
            loadCurrentPage(); // 确保数据被加载显示
        }
        // 不再显示第二个提示对话框，避免重复提示
    }
    else
    {
        qDebug() << "MainWindow::openPinSettingsDialog - 用户取消了管脚设置";
    }
}

void MainWindow::showPinGroupDialog()
{
    qDebug() << "MainWindow::showPinGroupDialog - 显示管脚分组对话框";

    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        qDebug() << "MainWindow::showPinGroupDialog - 没有打开的数据库";
        QMessageBox::warning(this, tr("警告"), tr("请先打开或创建项目"));
        return;
    }

    // 使用对话框管理器显示分组对话框
    bool success = m_dialogManager->showPinGroupDialog();

    if (success)
    {
        qDebug() << "MainWindow::showPinGroupDialog - 分组创建成功";
        statusBar()->showMessage(tr("管脚分组已成功添加"));
    }
    else
    {
        qDebug() << "MainWindow::showPinGroupDialog - 分组创建取消或失败";
    }
}

// Add the new private helper method
void MainWindow::updateBinaryHeaderColumnCount(int tableId)
{
    const QString funcName = "MainWindow::updateBinaryHeaderColumnCount";
    qDebug() << funcName << "- Attempting to update binary header column count for table ID:" << tableId;

    QString errorMessage;
    QList<Vector::ColumnInfo> columns;
    int dbSchemaVersion = -1;
    QString binaryFileNameBase; // Base name like "table_1_data.vbindata"

    DatabaseManager *dbManager = DatabaseManager::instance(); // Corrected: Pointer type
    if (!dbManager->isDatabaseConnected())
    { // Corrected: ->isDatabaseConnected()
        qWarning() << funcName << "- Database not open.";
        return;
    }
    QSqlDatabase db = dbManager->database();

    // 1. Get master record info (schema version, binary file name)
    QSqlQuery masterQuery(db);
    // Corrected: VectorTableMasterRecord table name, and use 'id' for tableId
    masterQuery.prepare("SELECT data_schema_version, binary_data_filename FROM VectorTableMasterRecord WHERE id = :tableId");
    masterQuery.bindValue(":tableId", tableId);

    if (!masterQuery.exec())
    {
        qWarning() << funcName << "- Failed to execute query for VectorTableMasterRecord:" << masterQuery.lastError().text();
        return;
    }

    if (masterQuery.next())
    {
        dbSchemaVersion = masterQuery.value("data_schema_version").toInt();        // Corrected column name
        binaryFileNameBase = masterQuery.value("binary_data_filename").toString(); // Corrected column name
    }
    else
    {
        qWarning() << funcName << "- No VectorTableMasterRecord found for table ID:" << tableId;
        return;
    }

    if (binaryFileNameBase.isEmpty())
    {
        qWarning() << funcName << "- Binary file name is empty in master record for table ID:" << tableId;
        return;
    }

    // 2. Get column configurations from DB to count actual columns
    QSqlQuery columnQuery(db);
    // Query to get the actual number of columns configured for this table in the database
    // This includes standard columns and any pin-specific columns
    // Corrected: VectorTableColumnConfiguration table name, master_record_id for tableId relation
    QString columnSql = QString(
        "SELECT COUNT(*) FROM VectorTableColumnConfiguration WHERE master_record_id = ?"); // 使用位置占位符

    if (!columnQuery.prepare(columnSql))
    {
        qWarning() << funcName << "- CRITICAL_WARNING: Failed to PREPARE query for actual column count. SQL:" << columnSql
                   << ". Error:" << columnQuery.lastError().text();
        return;
    }
    columnQuery.addBindValue(tableId); // 使用 addBindValue

    int actualColumnCount = 0;
    if (columnQuery.exec())
    {
        if (columnQuery.next())
        {
            actualColumnCount = columnQuery.value(0).toInt();
        }
        // No 'else' here, if query returns no rows, actualColumnCount remains 0, which is handled below.
    }
    else
    {
        qWarning() << funcName << "- CRITICAL_WARNING: Failed to EXECUTE query for actual column count. TableId:" << tableId
                   << ". SQL:" << columnSql << ". Error:" << columnQuery.lastError().text()
                   << "(Reason: DB query for actual column count failed after successful prepare)";
        return;
    }

    qDebug() << funcName << "- Actual column count from DB for tableId" << tableId << "is" << actualColumnCount;

    if (actualColumnCount == 0 && tableId > 0)
    {
        qWarning() << funcName << "- No columns found in DB configuration for table ID:" << tableId << ". Header update may not be meaningful (or it's a new table). Continuing.";
    }

    // 3. Construct full binary file path
    // QString projectDbPath = dbManager->getCurrentDatabasePath(); // Incorrect method
    QString projectDbPath = m_currentDbPath; // Use MainWindow's member
    QString projBinDataDir = Utils::PathUtils::getProjectBinaryDataDirectory(projectDbPath);
    QString binFilePath = projBinDataDir + QDir::separator() + binaryFileNameBase;

    QFile file(binFilePath);
    if (!file.exists())
    {
        qWarning() << funcName << "- Binary file does not exist, cannot update header:" << binFilePath;
        return;
    }

    if (!file.open(QIODevice::ReadWrite))
    {
        qWarning() << funcName << "- Failed to open binary file for ReadWrite:" << binFilePath << file.errorString();
        return;
    }

    BinaryFileHeader header;
    bool existingHeaderRead = Persistence::BinaryFileHelper::readBinaryHeader(&file, header);

    if (existingHeaderRead)
    {
        // Corrected member access to use column_count_in_file
        if (header.column_count_in_file == actualColumnCount && header.data_schema_version == dbSchemaVersion)
        {
            qDebug() << funcName << "- Header column count (" << header.column_count_in_file
                     << ") and schema version (" << header.data_schema_version
                     << ") already match DB. No update needed for table" << tableId;
            file.close();
            return;
        }
        // Preserve creation time and row count from existing header
        header.column_count_in_file = actualColumnCount; // Corrected to column_count_in_file
        header.data_schema_version = dbSchemaVersion;    // Update schema version from DB
        header.timestamp_updated = QDateTime::currentSecsSinceEpoch();
    }
    else
    {
        qWarning() << funcName << "- Failed to read existing header from" << binFilePath << ". This is unexpected if addNewVectorTable created it. Re-initializing header for update.";
        header.magic_number = Persistence::VEC_BINDATA_MAGIC;
        header.file_format_version = Persistence::CURRENT_FILE_FORMAT_VERSION;
        header.data_schema_version = dbSchemaVersion;
        header.row_count_in_file = 0;
        header.column_count_in_file = actualColumnCount; // Corrected to column_count_in_file
        header.timestamp_created = QDateTime::currentSecsSinceEpoch();
        header.timestamp_updated = QDateTime::currentSecsSinceEpoch();
        header.compression_type = 0;
        // Removed memset calls for header.reserved and header.future_use as they are not members
    }

    file.seek(0);
    if (Persistence::BinaryFileHelper::writeBinaryHeader(&file, header))
    {
        qInfo() << funcName << "- Successfully updated binary file header for table" << tableId
                << ". Path:" << binFilePath
                << ". New ColumnCount:" << actualColumnCount
                << ", SchemaVersion:" << dbSchemaVersion;
    }
    else
    {
        qWarning() << funcName << "- Failed to write updated binary file header for table" << tableId
                   << ". Path:" << binFilePath;
    }
    file.close();

    // Corrected cache invalidation method name
    VectorDataHandler::instance().clearTableDataCache(tableId);
    // Clearing data cache is often sufficient. If specific metadata cache for columns/schema
    // exists and needs explicit invalidation, that would require a specific method in VectorDataHandler.
    // For now, assuming clearTableDataCache() and subsequent reloads handle it.
}
