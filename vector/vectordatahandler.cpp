#include "vectordatahandler.h"
#include "database/databasemanager.h"
#include "pin/pinvalueedit.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSet>
#include <QDebug>
#include <algorithm>

VectorDataHandler::VectorDataHandler()
{
}

bool VectorDataHandler::loadVectorTableData(int tableId, QTableWidget *tableWidget)
{
    if (!tableWidget)
        return false;

    // 清空表格
    tableWidget->clear();
    tableWidget->setRowCount(0);
    tableWidget->setColumnCount(0);

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        return false;
    }

    // 1. 获取表的管脚信息以设置表头
    QSqlQuery pinsQuery(db);
    pinsQuery.prepare("SELECT vtp.id, pl.pin_name, vtp.pin_channel_count, topt.type_name "
                      "FROM vector_table_pins vtp "
                      "JOIN pin_list pl ON vtp.pin_id = pl.id "
                      "JOIN type_options topt ON vtp.pin_type = topt.id "
                      "WHERE vtp.table_id = ? "
                      "ORDER BY pl.pin_name");
    pinsQuery.addBindValue(tableId);

    QList<int> pinIds;
    QMap<int, int> pinIdToColumn; // 管脚ID到列索引的映射

    int columnIndex = 0;

    // 固定列: 标签，指令，TimeSet，Capture，Ext，Comment
    tableWidget->setColumnCount(6);
    tableWidget->setHorizontalHeaderItem(0, new QTableWidgetItem("Label"));
    tableWidget->setHorizontalHeaderItem(1, new QTableWidgetItem("Instruction"));
    tableWidget->setHorizontalHeaderItem(2, new QTableWidgetItem("TimeSet"));
    tableWidget->setHorizontalHeaderItem(3, new QTableWidgetItem("Capture"));
    tableWidget->setHorizontalHeaderItem(4, new QTableWidgetItem("Ext"));
    tableWidget->setHorizontalHeaderItem(5, new QTableWidgetItem("Comment"));

    columnIndex = 6;

    if (pinsQuery.exec())
    {
        while (pinsQuery.next())
        {
            int pinId = pinsQuery.value(0).toInt();
            QString pinName = pinsQuery.value(1).toString();
            int channelCount = pinsQuery.value(2).toInt();
            QString typeName = pinsQuery.value(3).toString();

            pinIds.append(pinId);
            pinIdToColumn[pinId] = columnIndex;

            // 添加管脚列
            tableWidget->insertColumn(columnIndex);

            // 创建自定义表头
            QTableWidgetItem *headerItem = new QTableWidgetItem();
            headerItem->setText(pinName + "\nx" + QString::number(channelCount) + "\n" + typeName);
            headerItem->setTextAlignment(Qt::AlignCenter);
            QFont headerFont = headerItem->font();
            headerFont.setBold(true);
            headerItem->setFont(headerFont);

            tableWidget->setHorizontalHeaderItem(columnIndex, headerItem);
            columnIndex++;
        }
    }

    // 2. 获取向量表数据
    QSqlQuery dataQuery(db);
    dataQuery.prepare("SELECT vtd.id, vtd.label, io.instruction_value, tl.timeset_name, "
                      "vtd.capture, vtd.ext, vtd.comment, vtd.sort_index "
                      "FROM vector_table_data vtd "
                      "JOIN instruction_options io ON vtd.instruction_id = io.id "
                      "LEFT JOIN timeset_list tl ON vtd.timeset_id = tl.id "
                      "WHERE vtd.table_id = ? "
                      "ORDER BY vtd.sort_index");
    dataQuery.addBindValue(tableId);

    if (dataQuery.exec())
    {
        QMap<int, int> vectorDataIdToRow; // 向量数据ID到行索引的映射
        int rowIndex = 0;

        while (dataQuery.next())
        {
            int vectorDataId = dataQuery.value(0).toInt();
            QString label = dataQuery.value(1).toString();
            QString instruction = dataQuery.value(2).toString();
            QString timeset = dataQuery.value(3).toString();
            QString capture = dataQuery.value(4).toString();
            QString ext = dataQuery.value(5).toString();
            QString comment = dataQuery.value(6).toString();

            // 添加新行
            tableWidget->insertRow(rowIndex);

            // 设置固定列数据
            tableWidget->setItem(rowIndex, 0, new QTableWidgetItem(label));
            tableWidget->setItem(rowIndex, 1, new QTableWidgetItem(instruction));
            tableWidget->setItem(rowIndex, 2, new QTableWidgetItem(timeset));

            // 修改Capture列显示逻辑，当值为"0"时显示为空白
            QString captureDisplay = (capture == "0") ? "" : capture;
            tableWidget->setItem(rowIndex, 3, new QTableWidgetItem(captureDisplay));

            tableWidget->setItem(rowIndex, 4, new QTableWidgetItem(ext));
            tableWidget->setItem(rowIndex, 5, new QTableWidgetItem(comment));

            vectorDataIdToRow[vectorDataId] = rowIndex;
            rowIndex++;
        }

        // 3. 获取管脚数值 - 直接从数据库中查询pin_options表获取值
        QSqlQuery valueQuery(db);
        QString valueQueryStr = QString(
                                    "SELECT vtd.id AS vector_data_id, "
                                    "       vtp.id AS vector_pin_id, "
                                    "       po.pin_value "
                                    "FROM vector_table_data vtd "
                                    "JOIN vector_table_pin_values vtpv ON vtd.id = vtpv.vector_data_id "
                                    "JOIN vector_table_pins vtp ON vtpv.vector_pin_id = vtp.id "
                                    "JOIN pin_options po ON vtpv.pin_level = po.id "
                                    "WHERE vtd.table_id = %1 "
                                    "ORDER BY vtd.sort_index")
                                    .arg(tableId);

        if (valueQuery.exec(valueQueryStr))
        {
            int count = 0;
            while (valueQuery.next())
            {
                int vectorDataId = valueQuery.value(0).toInt();
                int vectorPinId = valueQuery.value(1).toInt();
                QString pinValue = valueQuery.value(2).toString();
                count++;

                // 找到对应的行和列
                if (vectorDataIdToRow.contains(vectorDataId) && pinIdToColumn.contains(vectorPinId))
                {
                    int row = vectorDataIdToRow[vectorDataId];
                    int col = pinIdToColumn[vectorPinId];

                    // 设置单元格值
                    tableWidget->setItem(row, col, new QTableWidgetItem(pinValue));
                }
            }

            // 如果没有找到任何记录，尝试直接使用pin_level值
            if (count == 0)
            {
                QSqlQuery fallbackQuery(db);
                QString fallbackQueryStr = QString(
                                               "SELECT vtd.id AS vector_data_id, "
                                               "       vtp.id AS vector_pin_id, "
                                               "       vtpv.pin_level "
                                               "FROM vector_table_data vtd "
                                               "JOIN vector_table_pin_values vtpv ON vtd.id = vtpv.vector_data_id "
                                               "JOIN vector_table_pins vtp ON vtpv.vector_pin_id = vtp.id "
                                               "WHERE vtd.table_id = %1 "
                                               "ORDER BY vtd.sort_index")
                                               .arg(tableId);

                if (fallbackQuery.exec(fallbackQueryStr))
                {
                    while (fallbackQuery.next())
                    {
                        int vectorDataId = fallbackQuery.value(0).toInt();
                        int vectorPinId = fallbackQuery.value(1).toInt();
                        int pinLevelId = fallbackQuery.value(2).toInt();

                        // 尝试从pin_options表获取实际值
                        QSqlQuery getPinValue(db);
                        QString pinValue;

                        getPinValue.prepare("SELECT pin_value FROM pin_options WHERE id = ?");
                        getPinValue.addBindValue(pinLevelId);

                        if (getPinValue.exec() && getPinValue.next())
                        {
                            pinValue = getPinValue.value(0).toString();
                        }
                        else
                        {
                            // 如果获取失败，直接使用ID作为字符串
                            pinValue = QString::number(pinLevelId);
                        }

                        // 找到对应的行和列
                        if (vectorDataIdToRow.contains(vectorDataId) && pinIdToColumn.contains(vectorPinId))
                        {
                            int row = vectorDataIdToRow[vectorDataId];
                            int col = pinIdToColumn[vectorPinId];

                            // 设置单元格值
                            tableWidget->setItem(row, col, new QTableWidgetItem(pinValue));
                        }
                    }
                }
            }
        }
    }

    // 调整列宽
    tableWidget->resizeColumnsToContents();

    // 确保所有管脚单元格都有默认值"X"
    for (int row = 0; row < tableWidget->rowCount(); ++row)
    {
        for (int col = 6; col < tableWidget->columnCount(); ++col)
        {
            QTableWidgetItem *item = tableWidget->item(row, col);
            if (!item || item->text().isEmpty())
            {
                tableWidget->setItem(row, col, new QTableWidgetItem("X"));
            }
        }
    }

    return true;
}

bool VectorDataHandler::saveVectorTableData(int tableId, QTableWidget *tableWidget, QString &errorMessage)
{
    if (!tableWidget)
    {
        errorMessage = "表格控件为空";
        return false;
    }

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        errorMessage = "数据库未打开";
        return false;
    }

    // 开始事务
    db.transaction();
    QSqlQuery query(db);

    try
    {
        // 清除现有数据 - 先删除关联的pin_values，再删除主数据
        query.prepare("DELETE FROM vector_table_pin_values WHERE vector_data_id IN "
                      "(SELECT id FROM vector_table_data WHERE table_id = ?)");
        query.addBindValue(tableId);
        if (!query.exec())
        {
            throw QString("无法清除关联的管脚值数据: " + query.lastError().text());
        }

        query.prepare("DELETE FROM vector_table_data WHERE table_id = ?");
        query.addBindValue(tableId);
        if (!query.exec())
        {
            throw QString("无法清除现有数据: " + query.lastError().text());
        }

        // 获取已选择的管脚列表
        QList<int> pinIds;
        QList<QString> pinNames;
        query.prepare("SELECT vtp.id, pl.pin_name FROM vector_table_pins vtp "
                      "JOIN pin_list pl ON vtp.pin_id = pl.id "
                      "WHERE vtp.table_id = ? ORDER BY pl.pin_name");
        query.addBindValue(tableId);
        if (!query.exec())
        {
            throw QString("无法获取管脚列表: " + query.lastError().text());
        }

        while (query.next())
        {
            pinIds.append(query.value(0).toInt());
            pinNames.append(query.value(1).toString());
        }

        if (pinIds.isEmpty())
        {
            throw QString("没有找到任何关联的管脚");
        }

        // 逐行保存数据
        for (int row = 0; row < tableWidget->rowCount(); ++row)
        {
            // 获取基本信息
            QString label = tableWidget->item(row, 0) ? tableWidget->item(row, 0)->text() : "";
            QString instruction = tableWidget->item(row, 1) ? tableWidget->item(row, 1)->text() : "";
            QString timeSet = tableWidget->item(row, 2) ? tableWidget->item(row, 2)->text() : "";
            QString capture = tableWidget->item(row, 3) ? tableWidget->item(row, 3)->text() : "";
            QString ext = tableWidget->item(row, 4) ? tableWidget->item(row, 4)->text() : "";
            QString comment = tableWidget->item(row, 5) ? tableWidget->item(row, 5)->text() : "";

            // 获取指令ID
            int instructionId = 1; // 默认为1 (VECTOR)
            if (!instruction.isEmpty())
            {
                QSqlQuery instrQuery(db);
                instrQuery.prepare("SELECT id FROM instruction_options WHERE instruction_value = ?");
                instrQuery.addBindValue(instruction);
                if (instrQuery.exec() && instrQuery.next())
                {
                    instructionId = instrQuery.value(0).toInt();
                }
            }

            // 获取时间集ID
            int timeSetId = -1;
            if (!timeSet.isEmpty())
            {
                QSqlQuery timeSetQuery(db);
                timeSetQuery.prepare("SELECT id FROM timeset_list WHERE timeset_name = ?");
                timeSetQuery.addBindValue(timeSet);
                if (timeSetQuery.exec() && timeSetQuery.next())
                {
                    timeSetId = timeSetQuery.value(0).toInt();
                }
            }

            // 插入行数据
            QSqlQuery insertRowQuery(db);
            insertRowQuery.prepare("INSERT INTO vector_table_data "
                                   "(table_id, label, instruction_id, timeset_id, capture, ext, comment, sort_index) "
                                   "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
            insertRowQuery.addBindValue(tableId);
            insertRowQuery.addBindValue(label);
            insertRowQuery.addBindValue(instructionId);
            insertRowQuery.addBindValue(timeSetId > 0 ? timeSetId : QVariant());
            insertRowQuery.addBindValue(capture == "Y" ? 1 : 0);
            insertRowQuery.addBindValue(ext);
            insertRowQuery.addBindValue(comment);
            insertRowQuery.addBindValue(row);

            if (!insertRowQuery.exec())
            {
                throw QString("保存行 " + QString::number(row + 1) + " 失败: " + insertRowQuery.lastError().text());
            }

            // 获取插入的行ID
            int rowId = insertRowQuery.lastInsertId().toInt();

            // 保存管脚数据
            for (int i = 0; i < pinIds.size(); ++i)
            {
                int pinCol = i + 6; // 管脚从第6列开始
                QString pinValue = tableWidget->item(row, pinCol) ? tableWidget->item(row, pinCol)->text() : "X";

                // 如果值为空，使用默认值"X"
                if (pinValue.isEmpty())
                {
                    pinValue = "X";
                }

                // 获取pin_option_id
                int pinOptionId = 5; // 默认为X (id=5)
                QSqlQuery pinOptionQuery(db);
                pinOptionQuery.prepare("SELECT id FROM pin_options WHERE pin_value = ?");
                pinOptionQuery.addBindValue(pinValue);
                if (pinOptionQuery.exec() && pinOptionQuery.next())
                {
                    pinOptionId = pinOptionQuery.value(0).toInt();
                }
                else
                {
                    // 如果查询失败或没有找到对应的值，确保使用默认值X的ID
                    pinOptionId = 5; // X的ID固定为5
                }

                // 插入管脚数据
                QSqlQuery pinDataQuery(db);
                pinDataQuery.prepare("INSERT INTO vector_table_pin_values "
                                     "(vector_data_id, vector_pin_id, pin_level) VALUES (?, ?, ?)");
                pinDataQuery.addBindValue(rowId);
                pinDataQuery.addBindValue(pinIds[i]);
                pinDataQuery.addBindValue(pinOptionId);

                if (!pinDataQuery.exec())
                {
                    throw QString("保存管脚 " + pinNames[i] + " 数据失败: " + pinDataQuery.lastError().text());
                }
            }
        }

        // 提交事务
        db.commit();
        return true;
    }
    catch (const QString &error)
    {
        // 错误处理和回滚
        db.rollback();
        errorMessage = error;
        return false;
    }
}

void VectorDataHandler::addVectorRow(QTableWidget *table, const QStringList &pinOptions, int rowIdx)
{
    table->setRowCount(rowIdx + 1);

    // 添加每个管脚的文本输入框
    for (int col = 0; col < table->columnCount(); col++)
    {
        PinValueLineEdit *pinEdit = new PinValueLineEdit(table);

        // 默认设置为"X"
        pinEdit->setText("X");

        table->setCellWidget(rowIdx, col, pinEdit);
    }
}

bool VectorDataHandler::deleteVectorTable(int tableId, QString &errorMessage)
{
    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        errorMessage = "数据库未打开";
        return false;
    }

    db.transaction();
    QSqlQuery query(db);

    try
    {
        // 先删除与该表关联的管脚值数据
        query.prepare("DELETE FROM vector_table_pin_values WHERE vector_data_id IN "
                      "(SELECT id FROM vector_table_data WHERE table_id = ?)");
        query.addBindValue(tableId);
        if (!query.exec())
        {
            throw QString("删除管脚值数据失败: " + query.lastError().text());
        }

        // 删除向量表数据
        query.prepare("DELETE FROM vector_table_data WHERE table_id = ?");
        query.addBindValue(tableId);
        if (!query.exec())
        {
            throw QString("删除向量表数据失败: " + query.lastError().text());
        }

        // 删除向量表管脚配置
        query.prepare("DELETE FROM vector_table_pins WHERE table_id = ?");
        query.addBindValue(tableId);
        if (!query.exec())
        {
            throw QString("删除向量表管脚配置失败: " + query.lastError().text());
        }

        // 最后删除向量表记录
        query.prepare("DELETE FROM vector_tables WHERE id = ?");
        query.addBindValue(tableId);
        if (!query.exec())
        {
            throw QString("删除向量表记录失败: " + query.lastError().text());
        }

        // 提交事务
        db.commit();
        return true;
    }
    catch (const QString &error)
    {
        // 回滚事务
        db.rollback();
        errorMessage = error;
        return false;
    }
}

bool VectorDataHandler::deleteVectorRows(int tableId, const QList<int> &rowIndexes, QString &errorMessage)
{
    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        errorMessage = "数据库未打开";
        return false;
    }

    db.transaction();
    bool success = true;

    try
    {
        // 查询表中所有数据行，按排序索引顺序
        QList<int> allDataIds;
        QSqlQuery dataQuery(db);
        dataQuery.prepare("SELECT id FROM vector_table_data WHERE table_id = ? ORDER BY sort_index");
        dataQuery.addBindValue(tableId);

        if (!dataQuery.exec())
        {
            throw QString("获取向量数据ID失败: " + dataQuery.lastError().text());
        }

        // 将所有数据ID按顺序放入列表
        while (dataQuery.next())
        {
            allDataIds.append(dataQuery.value(0).toInt());
        }

        // 检查是否有足够的数据行
        if (allDataIds.isEmpty())
        {
            throw QString("没有找到可删除的数据行");
        }

        // 根据选中的行索引获取对应的数据ID
        QList<int> dataIdsToDelete;
        for (int row : rowIndexes)
        {
            if (row >= 0 && row < allDataIds.size())
            {
                dataIdsToDelete.append(allDataIds[row]);
            }
        }

        if (dataIdsToDelete.isEmpty())
        {
            throw QString("没有找到对应选中行的数据ID");
        }

        // 删除选中行的数据
        QSqlQuery deleteQuery(db);
        for (int dataId : dataIdsToDelete)
        {
            // 先删除关联的管脚值
            deleteQuery.prepare("DELETE FROM vector_table_pin_values WHERE vector_data_id = ?");
            deleteQuery.addBindValue(dataId);
            if (!deleteQuery.exec())
            {
                throw QString("删除数据ID " + QString::number(dataId) + " 的管脚值失败: " + deleteQuery.lastError().text());
            }

            // 再删除向量数据行
            deleteQuery.prepare("DELETE FROM vector_table_data WHERE id = ?");
            deleteQuery.addBindValue(dataId);
            if (!deleteQuery.exec())
            {
                throw QString("删除数据ID " + QString::number(dataId) + " 失败: " + deleteQuery.lastError().text());
            }
        }

        // 提交事务
        db.commit();
        return true;
    }
    catch (const QString &error)
    {
        // 回滚事务
        db.rollback();
        errorMessage = error;
        return false;
    }
}

int VectorDataHandler::getVectorTableRowCount(int tableId)
{
    // 查询当前向量表中的总行数
    int totalRowsInFile = 0;
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery rowCountQuery(db);

    rowCountQuery.prepare("SELECT COUNT(*) FROM vector_table_data WHERE table_id = ?");
    rowCountQuery.addBindValue(tableId);

    if (rowCountQuery.exec() && rowCountQuery.next())
    {
        totalRowsInFile = rowCountQuery.value(0).toInt();
    }

    return totalRowsInFile;
}

bool VectorDataHandler::insertVectorRows(int tableId, int startIndex, int rowCount, int timesetId,
                                         QTableWidget *dataTable, bool appendToEnd,
                                         const QList<QPair<int, QPair<QString, QPair<int, QString>>>> &selectedPins,
                                         QString &errorMessage)
{
    // 保存向量行数据
    QSqlDatabase db = DatabaseManager::instance()->database();
    db.transaction();

    bool success = true;

    // 获取实际数据行数
    int rowDataCount = dataTable->rowCount();

    // 检查行数设置
    if (rowCount < rowDataCount)
    {
        errorMessage = "设置的总行数小于实际添加的行数据数量！";
        db.rollback();
        return false;
    }

    if (rowCount % rowDataCount != 0)
    {
        errorMessage = "设置的总行数必须是行数据数量的整数倍！";
        db.rollback();
        return false;
    }

    // 计算重复次数
    int repeatTimes = rowCount / rowDataCount;

    // 获取插入位置
    int actualStartIndex = startIndex;

    // 如果不是添加到最后，需要先将目标位置及之后的数据往后移动
    if (!appendToEnd)
    {
        QSqlQuery updateQuery(db);
        updateQuery.prepare("UPDATE vector_table_data SET sort_index = sort_index + ? "
                            "WHERE table_id = ? AND sort_index >= ?");
        updateQuery.addBindValue(rowCount);
        updateQuery.addBindValue(tableId);
        updateQuery.addBindValue(actualStartIndex);

        if (!updateQuery.exec())
        {
            errorMessage = "更新现有数据索引失败：" + updateQuery.lastError().text();
            db.rollback();
            return false;
        }
    }

    // 根据重复次数添加行数据
    for (int repeat = 0; repeat < repeatTimes; repeat++)
    {
        for (int row = 0; row < rowDataCount; row++)
        {
            // 添加vector_table_data记录
            QSqlQuery dataQuery(db);
            dataQuery.prepare("INSERT INTO vector_table_data "
                              "(table_id, instruction_id, timeset_id, label, capture, ext, comment, sort_index) "
                              "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
            dataQuery.addBindValue(tableId);
            dataQuery.addBindValue(1); // 默认instruction_id为1
            dataQuery.addBindValue(timesetId);
            dataQuery.addBindValue("");                                             // label默认为空
            dataQuery.addBindValue(0);                                              // capture默认为0
            dataQuery.addBindValue("");                                             // ext默认为空
            dataQuery.addBindValue("");                                             // comment默认为空
            dataQuery.addBindValue(actualStartIndex + repeat * rowDataCount + row); // 计算实际排序索引

            if (!dataQuery.exec())
            {
                errorMessage = "添加向量行数据失败：" + dataQuery.lastError().text();
                success = false;
                break;
            }

            int vectorDataId = dataQuery.lastInsertId().toInt();

            // 为每个管脚添加vector_table_pin_values记录
            for (int col = 0; col < selectedPins.size(); col++)
            {
                int pinId = selectedPins[col].first;

                // 获取单元格中的输入框
                PinValueLineEdit *pinEdit = qobject_cast<PinValueLineEdit *>(dataTable->cellWidget(row, col));
                if (!pinEdit)
                    continue;

                QString pinValue = pinEdit->text();
                if (pinValue.isEmpty())
                    pinValue = "X"; // 如果为空，默认使用X

                // 获取pin_option_id
                int pinOptionId = 5; // 默认为X (id=5)
                QSqlQuery pinOptionQuery(db);
                pinOptionQuery.prepare("SELECT id FROM pin_options WHERE pin_value = ?");
                pinOptionQuery.addBindValue(pinValue);
                if (pinOptionQuery.exec() && pinOptionQuery.next())
                {
                    pinOptionId = pinOptionQuery.value(0).toInt();
                }

                QSqlQuery pinValueQuery(db);
                pinValueQuery.prepare("INSERT INTO vector_table_pin_values "
                                      "(vector_data_id, vector_pin_id, pin_level) "
                                      "VALUES (?, ?, ?)");
                pinValueQuery.addBindValue(vectorDataId);
                pinValueQuery.addBindValue(pinId);
                pinValueQuery.addBindValue(pinOptionId);

                if (!pinValueQuery.exec())
                {
                    errorMessage = "保存管脚值失败：" + pinValueQuery.lastError().text();
                    success = false;
                    break;
                }
            }

            if (!success)
                break;
        }

        if (!success)
            break;
    }

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