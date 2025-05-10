#include "vectordatahandler.h"
#include "database/databasemanager.h"
#include "pin/pinvalueedit.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSet>
#include <QDebug>
#include <algorithm>

// 静态单例实例
VectorDataHandler &VectorDataHandler::instance()
{
    static VectorDataHandler instance;
    return instance;
}

VectorDataHandler::VectorDataHandler() : m_cancelRequested(0)
{
}

VectorDataHandler::~VectorDataHandler()
{
}

void VectorDataHandler::cancelOperation()
{
    m_cancelRequested.storeRelease(1);
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
    // 重置取消标志
    m_cancelRequested.storeRelease(0);

    qDebug() << "VectorDataHandler::insertVectorRows - 开始插入向量行，表ID:" << tableId
             << "行数:" << rowCount << "数据表行数:" << dataTable->rowCount();

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

    qDebug() << "VectorDataHandler::insertVectorRows - 重复次数:" << repeatTimes;

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

    // 提前获取pin_options表中的所有可能值，避免重复查询
    QMap<QString, int> pinOptionsCache;
    QSqlQuery pinOptionsQuery(db);
    if (pinOptionsQuery.exec("SELECT id, pin_value FROM pin_options"))
    {
        while (pinOptionsQuery.next())
        {
            int id = pinOptionsQuery.value(0).toInt();
            QString value = pinOptionsQuery.value(1).toString();
            pinOptionsCache[value] = id;
        }
    }

    // 默认值为X (id=5)，确保即使查询失败也有默认值
    if (!pinOptionsCache.contains("X"))
    {
        pinOptionsCache["X"] = 5;
    }

    qDebug() << "VectorDataHandler::insertVectorRows - 开始批量处理插入";

    // 进度报告变量
    const int PROGRESS_DATA_ROWS = 40;  // 数据行占总进度的40%
    const int PROGRESS_PIN_VALUES = 60; // 管脚值占总进度的60%
    int dataProgress = 0;

    // 预处理SQL语句，只准备一次
    QSqlQuery dataQuery(db);
    dataQuery.prepare("INSERT INTO vector_table_data "
                      "(table_id, instruction_id, timeset_id, label, capture, ext, comment, sort_index) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");

    QSqlQuery pinValueQuery(db);
    pinValueQuery.prepare("INSERT INTO vector_table_pin_values "
                          "(vector_data_id, vector_pin_id, pin_level) "
                          "VALUES (?, ?, ?)");

    // 使用批处理来提高性能
    int batchSize = 1000; // 设置一个合理的批处理大小
    int currentBatch = 0;
    int totalProcessed = 0;
    int maxBatches = (repeatTimes * rowDataCount + batchSize - 1) / batchSize;

    QList<QVariantList> dataParams;
    for (int i = 0; i < 8; i++)
    { // vector_table_data表有8个参数
        dataParams.append(QVariantList());
    }

    QList<QVariantList> pinValueParams;
    for (int i = 0; i < 3; i++)
    { // vector_table_pin_values表有3个参数
        pinValueParams.append(QVariantList());
    }

    // 收集要插入的数据行的ID
    QList<int> vectorDataIds;

    // 根据重复次数添加行数据
    for (int repeat = 0; repeat < repeatTimes; repeat++)
    {
        for (int row = 0; row < rowDataCount; row++)
        {
            // 检查是否取消
            if (m_cancelRequested.loadAcquire())
            {
                qDebug() << "VectorDataHandler::insertVectorRows - 操作被用户取消";
                errorMessage = "操作被用户取消";
                db.rollback();
                return false;
            }

            // 更新UI进度
            if ((repeat * rowDataCount + row) % 5000 == 0 || totalProcessed == 0)
            {
                int progress = totalProcessed * PROGRESS_DATA_ROWS / rowCount;
                emit progressUpdated(progress);
                qDebug() << "VectorDataHandler::insertVectorRows - 已处理"
                         << totalProcessed << "/" << rowCount << "行，进度:" << progress << "%";
            }

            // vector_table_data参数
            dataParams[0].append(tableId);
            dataParams[1].append(1); // 默认instruction_id为1
            dataParams[2].append(timesetId);
            dataParams[3].append("");                                             // label默认为空
            dataParams[4].append(0);                                              // capture默认为0
            dataParams[5].append("");                                             // ext默认为空
            dataParams[6].append("");                                             // comment默认为空
            dataParams[7].append(actualStartIndex + repeat * rowDataCount + row); // 排序索引

            currentBatch++;
            totalProcessed++;

            // 当达到批处理大小或处理完所有数据时，执行批量插入
            if (currentBatch >= batchSize || totalProcessed >= rowCount)
            {
                qDebug() << "VectorDataHandler::insertVectorRows - 执行批量插入，批次:"
                         << (totalProcessed / batchSize) << "/" << maxBatches;

                // 批量插入vector_table_data
                for (int i = 0; i < 8; i++)
                {
                    dataQuery.addBindValue(dataParams[i]);
                }

                if (!dataQuery.execBatch())
                {
                    errorMessage = "批量添加向量行数据失败：" + dataQuery.lastError().text();
                    qDebug() << "VectorDataHandler::insertVectorRows - 错误:" << errorMessage;
                    db.rollback();
                    return false;
                }

                // 获取所有插入的ID
                // 注意：这是一个简化处理，实际应该使用QSqlDriver::lastInsertId()或适当的SQL查询
                QSqlQuery idQuery(db);
                QString idQueryStr = QString(
                                         "SELECT id FROM vector_table_data WHERE table_id = %1 ORDER BY id DESC LIMIT %2")
                                         .arg(tableId)
                                         .arg(currentBatch);

                if (idQuery.exec(idQueryStr))
                {
                    while (idQuery.next())
                    {
                        vectorDataIds.prepend(idQuery.value(0).toInt()); // 倒序插入，保持正确顺序
                    }
                }
                else
                {
                    errorMessage = "获取插入ID失败：" + idQuery.lastError().text();
                    qDebug() << "VectorDataHandler::insertVectorRows - 错误:" << errorMessage;
                    db.rollback();
                    return false;
                }

                // 清空数据参数列表，准备下一批
                for (int i = 0; i < 8; i++)
                {
                    dataParams[i].clear();
                }

                currentBatch = 0;

                // 更新进度
                int progress = totalProcessed * PROGRESS_DATA_ROWS / rowCount;
                if (progress > dataProgress)
                {
                    dataProgress = progress;
                    emit progressUpdated(dataProgress);
                }
            }
        }
    }

    qDebug() << "VectorDataHandler::insertVectorRows - 行数据插入完成，开始处理管脚值";
    emit progressUpdated(PROGRESS_DATA_ROWS); // 数据行插入已完成

    // 重置批处理计数
    currentBatch = 0;
    totalProcessed = 0;
    int totalPinValues = vectorDataIds.size() * selectedPins.size();
    maxBatches = (totalPinValues + batchSize - 1) / batchSize;

    // 为每个管脚添加vector_table_pin_values记录
    for (int dataIdx = 0; dataIdx < vectorDataIds.size(); dataIdx++)
    {
        int vectorDataId = vectorDataIds[dataIdx];
        int originalRowIdx = dataIdx % rowDataCount; // 获取原始数据表中的行索引

        for (int col = 0; col < selectedPins.size(); col++)
        {
            // 检查是否取消
            if (m_cancelRequested.loadAcquire())
            {
                qDebug() << "VectorDataHandler::insertVectorRows - 操作被用户取消";
                errorMessage = "操作被用户取消";
                db.rollback();
                return false;
            }

            // 更新进度
            if (totalProcessed % 10000 == 0 || totalProcessed == 0)
            {
                int progress = PROGRESS_DATA_ROWS + (totalProcessed * PROGRESS_PIN_VALUES / totalPinValues);
                emit progressUpdated(progress);
                qDebug() << "VectorDataHandler::insertVectorRows - 已处理管脚值"
                         << totalProcessed << "/" << totalPinValues << "个，总进度:" << progress << "%";
            }

            int pinId = selectedPins[col].first;

            // 获取单元格中的输入框
            PinValueLineEdit *pinEdit = qobject_cast<PinValueLineEdit *>(dataTable->cellWidget(originalRowIdx, col));

            // 获取管脚值
            QString pinValue = pinEdit ? pinEdit->text() : "X";
            if (pinValue.isEmpty())
            {
                pinValue = "X"; // 如果为空，默认使用X
            }

            // 从缓存中获取pin_option_id
            int pinOptionId = pinOptionsCache.value(pinValue, 5); // 默认为X (id=5)

            // 添加到批处理参数
            pinValueParams[0].append(vectorDataId);
            pinValueParams[1].append(pinId);
            pinValueParams[2].append(pinOptionId);

            currentBatch++;
            totalProcessed++;

            // 当达到批处理大小或处理完所有数据时，执行批量插入
            if (currentBatch >= batchSize || totalProcessed >= totalPinValues)
            {
                qDebug() << "VectorDataHandler::insertVectorRows - 执行管脚值批量插入，批次:"
                         << (totalProcessed / batchSize) << "/" << maxBatches;

                // 批量插入vector_table_pin_values
                for (int i = 0; i < 3; i++)
                {
                    pinValueQuery.addBindValue(pinValueParams[i]);
                }

                if (!pinValueQuery.execBatch())
                {
                    errorMessage = "批量添加管脚值失败：" + pinValueQuery.lastError().text();
                    qDebug() << "VectorDataHandler::insertVectorRows - 错误:" << errorMessage;
                    db.rollback();
                    return false;
                }

                // 清空参数列表，准备下一批
                for (int i = 0; i < 3; i++)
                {
                    pinValueParams[i].clear();
                }

                currentBatch = 0;

                // 更新进度
                int progress = PROGRESS_DATA_ROWS + (totalProcessed * PROGRESS_PIN_VALUES / totalPinValues);
                emit progressUpdated(progress);
            }
        }
    }

    qDebug() << "VectorDataHandler::insertVectorRows - 所有数据插入完成，准备提交事务";
    emit progressUpdated(99); // 设为99%，等提交成功后再设为100%

    if (success)
    {
        if (db.commit())
        {
            qDebug() << "VectorDataHandler::insertVectorRows - 事务提交成功";
            emit progressUpdated(100); // 操作完成，进度100%
            return true;
        }
        else
        {
            errorMessage = "提交事务失败：" + db.lastError().text();
            qDebug() << "VectorDataHandler::insertVectorRows - 错误:" << errorMessage;
            db.rollback();
            return false;
        }
    }
    else
    {
        db.rollback();
        qDebug() << "VectorDataHandler::insertVectorRows - 事务回滚";
        return false;
    }
}

bool VectorDataHandler::deleteVectorRowsInRange(int tableId, int fromRow, int toRow, QString &errorMessage)
{
    qDebug() << "VectorDataHandler::deleteVectorRowsInRange - 开始删除范围内的向量行，表ID：" << tableId
             << "，从行：" << fromRow << "，到行：" << toRow;

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        errorMessage = "数据库未打开";
        qDebug() << "VectorDataHandler::deleteVectorRowsInRange - 错误：" << errorMessage;
        return false;
    }

    db.transaction();

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

        qDebug() << "VectorDataHandler::deleteVectorRowsInRange - 总行数：" << allDataIds.size();

        // 调整索引范围
        if (fromRow < 1)
            fromRow = 1;

        if (toRow > allDataIds.size())
            toRow = allDataIds.size();

        if (fromRow > toRow)
        {
            int temp = fromRow;
            fromRow = toRow;
            toRow = temp;
        }

        qDebug() << "VectorDataHandler::deleteVectorRowsInRange - 调整后范围：" << fromRow << "到" << toRow;

        // 获取要删除的行ID
        QList<int> dataIdsToDelete;
        for (int row = fromRow; row <= toRow; row++)
        {
            int index = row - 1; // 将1-based转换为0-based索引
            if (index >= 0 && index < allDataIds.size())
            {
                dataIdsToDelete.append(allDataIds[index]);
            }
        }

        if (dataIdsToDelete.isEmpty())
        {
            throw QString("没有找到对应选中范围的数据ID");
        }

        qDebug() << "VectorDataHandler::deleteVectorRowsInRange - 要删除的行数：" << dataIdsToDelete.size();

        // 删除选中范围内的数据
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
        qDebug() << "VectorDataHandler::deleteVectorRowsInRange - 成功删除范围内的行";
        return true;
    }
    catch (const QString &error)
    {
        // 回滚事务
        db.rollback();
        errorMessage = error;
        qDebug() << "VectorDataHandler::deleteVectorRowsInRange - 错误：" << errorMessage;
        return false;
    }
}

bool VectorDataHandler::gotoLine(int tableId, int lineNumber)
{
    qDebug() << "VectorDataHandler::gotoLine - 准备跳转到向量表" << tableId << "的第" << lineNumber << "行";

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qDebug() << "VectorDataHandler::gotoLine - 错误：数据库未打开";
        return false;
    }

    // 首先检查表格是否存在
    QSqlQuery tableCheckQuery(db);
    tableCheckQuery.prepare("SELECT table_name FROM vector_tables WHERE id = ?");
    tableCheckQuery.addBindValue(tableId);

    if (!tableCheckQuery.exec() || !tableCheckQuery.next())
    {
        qDebug() << "VectorDataHandler::gotoLine - 错误：找不到指定的向量表 ID:" << tableId;
        return false;
    }

    QString tableName = tableCheckQuery.value(0).toString();
    qDebug() << "VectorDataHandler::gotoLine - 向量表名称:" << tableName;

    // 检查行号是否有效
    int totalRows = getVectorTableRowCount(tableId);
    if (lineNumber < 1 || lineNumber > totalRows)
    {
        qDebug() << "VectorDataHandler::gotoLine - 错误：行号" << lineNumber << "超出范围（1-" << totalRows << "）";
        return false;
    }

    // 找到对应的向量数据ID
    QSqlQuery dataQuery(db);
    dataQuery.prepare("SELECT id FROM vector_table_data WHERE table_id = ? ORDER BY sort_index LIMIT 1 OFFSET ?");
    dataQuery.addBindValue(tableId);
    dataQuery.addBindValue(lineNumber - 1); // 数据库OFFSET是0-based，而行号是1-based

    if (!dataQuery.exec() || !dataQuery.next())
    {
        qDebug() << "VectorDataHandler::gotoLine - 错误：无法获取第" << lineNumber << "行的数据 ID";
        qDebug() << "SQL错误：" << dataQuery.lastError().text();
        return false;
    }

    int dataId = dataQuery.value(0).toInt();
    qDebug() << "VectorDataHandler::gotoLine - 找到第" << lineNumber << "行的数据 ID:" << dataId;

    // 如果需要滚动到指定行，可以在这里记录dataId，然后通过UI组件使用这个ID来定位和滚动
    // 但在大多数情况下，我们只需要知道行数，因为我们会在UI层面使用selectRow方法来选中行

    qDebug() << "VectorDataHandler::gotoLine - 跳转成功：表" << tableName << "的第" << lineNumber << "行";
    return true;
}