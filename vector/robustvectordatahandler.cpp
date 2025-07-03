#include "robustvectordatahandler.h"
#include "database/databasemanager.h"
#include "pin/pinvalueedit.h"
#include "vector/vectordatahandler.h"
#include "common/utils/pathutils.h"
#include <QDebug>
#include <QFile>
#include <QScrollBar>
#include <QTableWidgetItem>
#include <QSqlQuery>
#include <QSqlError>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <stdexcept>
#include "database/binaryfilehelper.h"
#include "common/binary_file_format.h"
#include <QCryptographicHash>
#include <limits>
#include <algorithm>
#include <QDataStream>
#include <QSqlDatabase>

RobustVectorDataHandler &RobustVectorDataHandler::instance()
{
    static RobustVectorDataHandler inst;
    return inst;
}

RobustVectorDataHandler::RobustVectorDataHandler(QObject *parent) : QObject(parent) {}

RobustVectorDataHandler::~RobustVectorDataHandler() = default;

bool RobustVectorDataHandler::loadVectorTableData(int tableId, QTableWidget *tableWidget)
{
    qWarning() << "RobustVectorDataHandler::loadVectorTableData is not implemented yet.";
    return false;
}

bool RobustVectorDataHandler::saveVectorTableData(int tableId, QTableWidget *tableWidget, QString &errorMessage)
{
    qWarning() << "RobustVectorDataHandler::saveVectorTableData is not implemented yet.";
    return false;
}

bool RobustVectorDataHandler::saveVectorTableDataPaged(int tableId, QTableWidget *currentPageTable, int currentPage, int pageSize, int totalRows, QString &errorMessage)
{
    const QString funcName = "RobustVectorDataHandler::saveVectorTableDataPaged";
    qDebug() << funcName << " - 开始保存表ID:" << tableId << "的当前页数据";

    // 检查参数有效性
    if (!currentPageTable || pageSize <= 0 || currentPage < 0)
    {
        errorMessage = "无效的参数";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 获取表的元数据
    QString binFileName;
    QList<Vector::ColumnInfo> columns;
    int schemaVersion = 0;
    int storedRowCount = 0;

    if (!loadVectorTableMeta(tableId, binFileName, columns, schemaVersion, storedRowCount))
    {
        errorMessage = "无法加载表元数据";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 获取二进制文件路径
    QString binFilePath = resolveBinaryFilePath(tableId, errorMessage);
    if (binFilePath.isEmpty())
    {
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 检查是否有修改
    if (!isRowModified(tableId, -1))
    {
        qDebug() << funcName << " - 没有检测到修改，跳过保存";
        return true;
    }

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        errorMessage = "数据库未打开";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 开始事务
    if (!db.transaction())
    {
        errorMessage = "无法开始数据库事务: " + db.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    bool success = true;

    // 计算当前页的起始行和结束行
    int startRow = currentPage * pageSize;
    int endRow = qMin(startRow + pageSize, totalRows);
    int rowsInPage = endRow - startRow;

    // 确保表格行数与预期一致
    if (currentPageTable->rowCount() != rowsInPage)
    {
        errorMessage = QString("表格行数 (%1) 与预期行数 (%2) 不匹配").arg(currentPageTable->rowCount()).arg(rowsInPage);
        qWarning() << funcName << " - " << errorMessage;
        db.rollback();
        return false;
    }

    // 获取已修改的行
    QSet<int> modifiedRows;
    if (m_modifiedRows.contains(tableId))
    {
        modifiedRows = m_modifiedRows[tableId];
    }

    // 遍历当前页中的每一行
    for (int i = 0; i < rowsInPage; ++i)
    {
        int globalRowIndex = startRow + i;

        // 检查此行是否被修改过
        if (!modifiedRows.contains(globalRowIndex))
        {
            continue; // 跳过未修改的行
        }

        // 从表格中收集行数据
        Vector::RowData rowData;
        for (int col = 0; col < currentPageTable->columnCount() && col < columns.size(); ++col)
        {
            QTableWidgetItem *item = currentPageTable->item(i, col);
            QVariant value;

            if (item)
            {
                value = item->text();

                // 根据列类型转换数据
                switch (columns[col].type)
                {
                case Vector::ColumnDataType::INTEGER:
                    value = value.toInt();
                    break;
                case Vector::ColumnDataType::REAL:
                    value = value.toDouble();
                    break;
                case Vector::ColumnDataType::BOOLEAN:
                    value = (value.toString().toUpper() == "Y" || value.toString().toUpper() == "TRUE");
                    break;
                // 其他类型保持字符串形式
                default:
                    break;
                }
            }

            rowData.append(value);
        }

        // 更新行数据
        QString rowError;
        if (!updateVectorRow(tableId, globalRowIndex, rowData, rowError))
        {
            errorMessage = QString("更新行 %1 失败: %2").arg(globalRowIndex).arg(rowError);
            qWarning() << funcName << " - " << errorMessage;
            success = false;
            break;
        }
    }

    // 提交或回滚事务
    if (success)
    {
        if (!db.commit())
        {
            errorMessage = "提交事务失败: " + db.lastError().text();
            qWarning() << funcName << " - " << errorMessage;
            db.rollback();
            return false;
        }

        // 清除已修改行的标记
        clearModifiedRows(tableId);

        qDebug() << funcName << " - 成功保存表ID:" << tableId << "的当前页数据";
    }
    else
    {
        db.rollback();
        qWarning() << funcName << " - 保存失败，已回滚事务";
    }

    return success;
}

void RobustVectorDataHandler::addVectorRow(QTableWidget *table, const QStringList &pinOptions, int rowIdx)
{
    qWarning() << "RobustVectorDataHandler::addVectorRow is not implemented yet.";
}

void RobustVectorDataHandler::addVectorRows(QTableWidget *table, const QStringList &pinOptions, int startRowIdx, int count)
{
    qWarning() << "RobustVectorDataHandler::addVectorRows is not implemented yet.";
}

bool RobustVectorDataHandler::deleteVectorTable(int tableId, QString &errorMessage)
{
    qWarning() << "RobustVectorDataHandler::deleteVectorTable is not implemented yet.";
    return false;
}

bool RobustVectorDataHandler::deleteVectorRows(int tableId, const QList<int> &rowIndexes, QString &errorMessage)
{
    qWarning() << "RobustVectorDataHandler::deleteVectorRows is not implemented yet.";
    return false;
}

bool RobustVectorDataHandler::deleteVectorRowsInRange(int tableId, int fromRow, int toRow, QString &errorMessage)
{
    qWarning() << "RobustVectorDataHandler::deleteVectorRowsInRange is not implemented yet.";
    return false;
}

int RobustVectorDataHandler::getVectorTableRowCount(int tableId)
{
    QString binFileName;
    QList<Vector::ColumnInfo> columns;
    int schemaVersion = 0;
    int totalRowCount = 0;

    if (loadVectorTableMeta(tableId, binFileName, columns, schemaVersion, totalRowCount))
    {
        return totalRowCount;
    }

    qWarning() << "Failed to get row count for table" << tableId;
    return 0;
}

QList<Vector::ColumnInfo> RobustVectorDataHandler::getAllColumnInfo(int tableId)
{
    qWarning() << "RobustVectorDataHandler::getAllColumnInfo is not implemented yet.";
    return {};
}

int RobustVectorDataHandler::getSchemaVersion(int tableId)
{
    qWarning() << "RobustVectorDataHandler::getSchemaVersion is not implemented yet.";
    return 0;
}

QList<QList<QVariant>> RobustVectorDataHandler::getAllVectorRows(int tableId, bool &ok)
{
    const QString funcName = "RobustVectorDataHandler::getAllVectorRows";
    QString errorMessage;
    ok = false;

    // 1. 加载元数据
    QString binFileName;
    QList<Vector::ColumnInfo> columns;
    int schemaVersion = 0;
    int totalRowCount = 0;
    if (!loadVectorTableMeta(tableId, binFileName, columns, schemaVersion, totalRowCount))
    {
        qWarning() << funcName << " - Failed to load meta for table" << tableId;
        return {};
    }

    // 2. 获取二进制文件路径
    QString absoluteBinFilePath = resolveBinaryFilePath(tableId, errorMessage);
    if (absoluteBinFilePath.isEmpty())
    {
        qWarning() << funcName << " - " << errorMessage;
        return {};
    }

    // 3. 从二进制文件读取所有数据
    QList<QList<QVariant>> allRows;
    if (!Persistence::BinaryFileHelper::readPageDataFromBinary(absoluteBinFilePath, columns, schemaVersion, 0, totalRowCount, allRows))
    {
        qWarning() << funcName << " - Failed to read all rows from binary file for table" << tableId;
        return {};
    }

    qDebug() << funcName << " - Successfully read" << allRows.size() << "rows for table" << tableId;
    ok = true;
    return allRows;
}

bool RobustVectorDataHandler::insertVectorRows(int tableId, int logicalStartIndex, const QList<Vector::RowData> &rows, QString &errorMessage)
{
    const QString funcName = "RobustVectorDataHandler::insertVectorRows";
    QSqlDatabase db = DatabaseManager::instance()->database();

    int oldRowCount = getVectorTableRowCount(tableId);

    if (!db.transaction())
    {
        errorMessage = "Failed to start database transaction: " + db.lastError().text();
        qWarning() << funcName << "-" << errorMessage;
        return false;
    }

    // 1. 如果不是在末尾插入，需要先更新现有行的逻辑顺序 (此操作开销较大，但必须保留)
    if (logicalStartIndex < oldRowCount)
    {
        QSqlQuery shiftQuery(db);
        shiftQuery.prepare("UPDATE VectorTableRowIndex SET logical_row_order = logical_row_order + ? WHERE master_record_id = ? AND logical_row_order >= ?");
        shiftQuery.addBindValue(rows.count());
        shiftQuery.addBindValue(tableId);
        shiftQuery.addBindValue(logicalStartIndex);
        if (!shiftQuery.exec())
        {
            errorMessage = "Failed to shift existing row indexes: " + shiftQuery.lastError().text();
            qWarning() << funcName << "-" << errorMessage;
            db.rollback();
            return false;
        }
    }

    QString binFilePath = resolveBinaryFilePath(tableId, errorMessage);
    if (binFilePath.isEmpty())
    {
        db.rollback();
        return false;
    }

    QFile binFile(binFilePath);
    if (!binFile.open(QIODevice::Append))
    {
        errorMessage = "Failed to open binary file for appending: " + binFile.errorString();
        qWarning() << funcName << "-" << errorMessage;
        db.rollback();
        return false;
    }

    // 2. 准备批量写入
    QSqlQuery insertIndexQuery(db);
    insertIndexQuery.prepare(
        "INSERT INTO VectorTableRowIndex (master_record_id, logical_row_order, offset, size, is_active) "
        "VALUES (?, ?, ?, ?, 1)");

    bool success = true;
    const int totalRowsToInsert = rows.count();
    const int batchSize = 100000; // 设定合理的批次大小以平衡内存和性能

    for (int batchStart = 0; batchStart < totalRowsToInsert; batchStart += batchSize)
    {
        int currentBatchSize = qMin(batchSize, totalRowsToInsert - batchStart);

        QByteArray batchBinBuffer;
        QVariantList masterIds, logicalOrders, offsets, sizes;

        qint64 batchStartOffset = binFile.size();
        qint64 currentRelativeOffset = 0;

        // 在内存中准备一整个批次的数据
        for (int i = 0; i < currentBatchSize; ++i)
        {
            int currentRowIndexInTotal = batchStart + i;
            const Vector::RowData &row = rows.at(currentRowIndexInTotal);

            QByteArray rowByteArray;
            if (!Persistence::BinaryFileHelper::serializeRow(row, rowByteArray))
            {
                errorMessage = QString("Failed to serialize row at logical index %1.").arg(logicalStartIndex + currentRowIndexInTotal);
                success = false;
                break;
            }

            batchBinBuffer.append(rowByteArray);

            masterIds.append(tableId);
            logicalOrders.append(logicalStartIndex + currentRowIndexInTotal);
            offsets.append(batchStartOffset + currentRelativeOffset);
            sizes.append(rowByteArray.size());

            currentRelativeOffset += rowByteArray.size();
        }

        if (!success)
            break;

        // 批量写入二进制文件
        if (binFile.write(batchBinBuffer) != batchBinBuffer.size())
        {
            errorMessage = "Failed to write full batch to binary file.";
            success = false;
            break;
        }

        // 批量执行数据库插入
        insertIndexQuery.addBindValue(masterIds);
        insertIndexQuery.addBindValue(logicalOrders);
        insertIndexQuery.addBindValue(offsets);
        insertIndexQuery.addBindValue(sizes);

        if (!insertIndexQuery.execBatch())
        {
            errorMessage = "Failed to execute batch insert for indexes: " + insertIndexQuery.lastError().text();
            success = false;
            break;
        }
    }

    binFile.close();

    // 3. 更新主记录和文件头中的总行数
    if (success)
    {
        int newTotalRowCount = oldRowCount + rows.count();

        QSqlQuery updateQuery(db);
        updateQuery.prepare("UPDATE VectorTableMasterRecord SET row_count = ? WHERE id = ?");
        updateQuery.addBindValue(newTotalRowCount);
        updateQuery.addBindValue(tableId);
        if (!updateQuery.exec())
        {
            errorMessage = "Failed to update master table row count: " + updateQuery.lastError().text();
            success = false;
        }

        if (success)
        {
            if (!Persistence::BinaryFileHelper::updateRowCountInHeader(binFilePath, newTotalRowCount))
            {
                errorMessage = "Critical: Failed to update binary file header with new row count.";
                success = false;
            }
        }
    }

    // 4. 根据结果提交或回滚
    if (success)
    {
        if (!db.commit())
        {
            errorMessage = "Failed to commit database transaction: " + db.lastError().text();
            success = false;
            qWarning() << funcName << "- Commit failed:" << errorMessage;
            db.rollback();
        }
    }
    else
    {
        qWarning() << funcName << "-" << errorMessage;
        db.rollback();
    }

    return success;
}

bool RobustVectorDataHandler::updateVectorRow(int tableId, int rowIndex, const Vector::RowData &rowData, QString &errorMessage)
{
    const QString funcName = "RobustVectorDataHandler::updateVectorRow";
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        errorMessage = "数据库未连接";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    QString binFilePath = resolveBinaryFilePath(tableId, errorMessage);
    if (binFilePath.isEmpty())
    {
        return false;
    }

    // 1. 序列化行数据 - [FIX] 使用新的无列信息的序列化函数
    QByteArray serializedRow;
    if (!Persistence::BinaryFileHelper::serializeRow(rowData, serializedRow))
    {
        errorMessage = "序列化行数据失败";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 2. 获取旧的行索引信息 (offset 和 size)
    qint64 oldOffset = -1;
    qint64 oldSize = -1;
    QSqlQuery indexQuery(db);
    indexQuery.prepare("SELECT offset, size FROM VectorTableRowIndex WHERE master_record_id = ? AND logical_row_order = ? AND is_active = 1");
    indexQuery.addBindValue(tableId);
    indexQuery.addBindValue(rowIndex);
    if (!indexQuery.exec() || !indexQuery.next())
    {
        errorMessage = "无法获取行的旧索引信息: " + indexQuery.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }
    oldOffset = indexQuery.value(0).toLongLong();
    oldSize = indexQuery.value(1).toLongLong();

    // 3. 打开二进制文件准备写入
    QFile binFile(binFilePath);
    if (!binFile.open(QIODevice::ReadWrite))
    {
        errorMessage = "无法打开二进制文件进行写入: " + binFile.errorString();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 4. 决定写入策略：原地更新 或 追加写
    qint64 newOffset = oldOffset;
    qint64 newSize = serializedRow.size();

    // 策略A：如果新数据大小不大于旧数据大小，可以原地更新以节省空间
    if (newSize <= oldSize)
    {
        if (!binFile.seek(oldOffset))
        {
            errorMessage = "无法定位到二进制文件中的写入位置";
            qWarning() << funcName << " - " << errorMessage;
            binFile.close();
            return false;
        }
        qint64 bytesWritten = binFile.write(serializedRow);
        if (bytesWritten != newSize)
        {
            errorMessage = QString("写入数据失败，预期写入 %1 字节，实际写入 %2 字节").arg(newSize).arg(bytesWritten);
            qWarning() << funcName << " - " << errorMessage;
            binFile.close();
            return false;
        }
        // 如果新数据更小，后面会留下一段空白，这在我们的追加模型中是可以接受的
    }
    // 策略B：如果新数据更大，则在文件末尾追加
    else
    {
        newOffset = binFile.size();
        if (!binFile.seek(newOffset))
        {
            errorMessage = "无法定位到二进制文件的末尾";
            qWarning() << funcName << " - " << errorMessage;
            binFile.close();
            return false;
        }
        qint64 bytesWritten = binFile.write(serializedRow);
        if (bytesWritten != newSize)
        {
            errorMessage = QString("写入数据失败，预期写入 %1 字节，实际写入 %2 字节").arg(newSize).arg(bytesWritten);
            qWarning() << funcName << " - " << errorMessage;
            binFile.close();
            return false;
        }
    }

    binFile.close();

    // 5. 更新数据库中的索引
    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE VectorTableRowIndex SET offset = ?, size = ? WHERE master_record_id = ? AND logical_row_order = ? AND is_active = 1");
    updateQuery.addBindValue(newOffset);
    updateQuery.addBindValue(newSize);
    updateQuery.addBindValue(tableId);
    updateQuery.addBindValue(rowIndex);

    if (!updateQuery.exec())
    {
        errorMessage = "更新行索引失败: " + updateQuery.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    qDebug() << funcName << " - 成功更新表ID:" << tableId << "的行:" << rowIndex;
    return true;
}

bool RobustVectorDataHandler::gotoLine(int tableId, int lineNumber)
{
    qWarning() << "RobustVectorDataHandler::gotoLine is not implemented yet.";
    return false;
}

bool RobustVectorDataHandler::hideVectorTableColumn(int tableId, const QString &columnName, QString &errorMessage)
{
    qWarning() << "RobustVectorDataHandler::hideVectorTableColumn is not implemented yet.";
    return false;
}

bool RobustVectorDataHandler::showVectorTableColumn(int tableId, const QString &columnName, QString &errorMessage)
{
    qWarning() << "RobustVectorDataHandler::showVectorTableColumn is not implemented yet.";
    return false;
}

void RobustVectorDataHandler::cancelOperation()
{
    qWarning() << "RobustVectorDataHandler::cancelOperation is not implemented yet.";
}

void RobustVectorDataHandler::clearCache()
{
    qWarning() << "RobustVectorDataHandler::clearCache is not implemented yet.";
}

#include "robustvectordatahandler_1.cpp"