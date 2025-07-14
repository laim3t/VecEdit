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
#include <QDateTime>

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
    const QString funcName = "RobustVectorDataHandler::deleteVectorTable";
    qDebug() << funcName << " - 开始删除向量表，表ID:" << tableId;

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        errorMessage = "数据库未打开";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 1. 获取表的二进制文件路径
    QString binFilePath;
    {
        QSqlQuery query(db);
        query.prepare("SELECT binary_data_filename, table_name FROM VectorTableMasterRecord WHERE id = ?");
        query.addBindValue(tableId);
        if (!query.exec() || !query.next())
        {
            errorMessage = "无法获取表 " + QString::number(tableId) + " 的记录: " + query.lastError().text();
            qWarning() << funcName << " - " << errorMessage;
            return false;
        }

        QString binFileName = query.value(0).toString();
        QString tableName = query.value(1).toString();

        if (binFileName.isEmpty())
        {
            errorMessage = "表 " + QString::number(tableId) + " 没有关联的二进制文件";
            qWarning() << funcName << " - " << errorMessage;
            return false;
        }

        qDebug() << funcName << " - 向量表名称:" << tableName << "，二进制文件名:" << binFileName;

        // 解析完整的二进制文件路径
        QString resolveError;
        binFilePath = resolveBinaryFilePath(tableId, resolveError);
        if (binFilePath.isEmpty())
        {
            errorMessage = "无法解析二进制文件路径: " + resolveError;
            qWarning() << funcName << " - " << errorMessage;
            return false;
        }
    }

    // 开启事务，确保数据一致性
    db.transaction();

    try
    {
        // 2. 删除VectorTableRowIndex表中的行索引记录
        QSqlQuery deleteRowsQuery(db);
        deleteRowsQuery.prepare("DELETE FROM VectorTableRowIndex WHERE master_record_id = ?");
        deleteRowsQuery.addBindValue(tableId);
        if (!deleteRowsQuery.exec())
        {
            throw QString("删除行索引记录失败: " + deleteRowsQuery.lastError().text());
        }
        qDebug() << funcName << " - 已删除行索引记录";

        // 3. 删除VectorTableColumnConfiguration表中的列配置
        QSqlQuery deleteColumnsQuery(db);
        deleteColumnsQuery.prepare("DELETE FROM VectorTableColumnConfiguration WHERE master_record_id = ?");
        deleteColumnsQuery.addBindValue(tableId);
        if (!deleteColumnsQuery.exec())
        {
            throw QString("删除列配置记录失败: " + deleteColumnsQuery.lastError().text());
        }
        qDebug() << funcName << " - 已删除列配置记录";

        // 4. 删除VectorTableMasterRecord表中的表记录
        QSqlQuery deleteMasterQuery(db);
        deleteMasterQuery.prepare("DELETE FROM VectorTableMasterRecord WHERE id = ?");
        deleteMasterQuery.addBindValue(tableId);
        if (!deleteMasterQuery.exec())
        {
            throw QString("删除主记录失败: " + deleteMasterQuery.lastError().text());
        }
        qDebug() << funcName << " - 已删除主记录";

        // 提交事务
        db.commit();
        qDebug() << funcName << " - 数据库记录删除成功，事务已提交";

        // 5. 从文件系统中删除二进制文件
        QFile binFile(binFilePath);
        if (binFile.exists())
        {
            if (binFile.remove())
            {
                qDebug() << funcName << " - 已删除二进制文件:" << binFilePath;
            }
            else
            {
                // 仅记录警告，不影响删除操作的结果
                qWarning() << funcName << " - 无法删除二进制文件:" << binFilePath << "，错误:" << binFile.errorString();
            }
        }
        else
        {
            qWarning() << funcName << " - 二进制文件不存在:" << binFilePath;
        }

        return true;
    }
    catch (const QString &error)
    {
        // 回滚事务
        db.rollback();
        errorMessage = error;
        qWarning() << funcName << " - 删除失败，事务已回滚: " << errorMessage;
        return false;
    }
}

bool RobustVectorDataHandler::deleteVectorRows(int tableId, const QList<int> &rowIndices, QString &errorMessage)
{
    const QString funcName = "RobustVectorDataHandler::deleteVectorRows";
    qDebug() << funcName << " - 开始删除指定行，表ID:" << tableId << "行数量:" << rowIndices.size();

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        errorMessage = "数据库未打开";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 开启事务，确保数据一致性
    db.transaction();

    try
    {
        // 1. 查询当前表的主记录
        QSqlQuery queryMaster(db);
        queryMaster.prepare("SELECT row_count FROM VectorTableMasterRecord WHERE id = ?");
        queryMaster.addBindValue(tableId);

        if (!queryMaster.exec() || !queryMaster.next())
        {
            errorMessage = "无法获取表 " + QString::number(tableId) + " 的主记录";
            qWarning() << funcName << " - " << errorMessage;
            db.rollback();
            return false;
        }

        int currentRowCount = queryMaster.value(0).toInt();

        // 2. 将选定的行标记为无效（将is_active设为0）
        QSqlQuery updateQuery(db);
        updateQuery.prepare("UPDATE VectorTableRowIndex SET is_active = 0 "
                            "WHERE master_record_id = ? AND logical_row_order = ?");

        int updatedRows = 0;
        for (int rowIndex : rowIndices)
        {
            // 确保行索引有效
            if (rowIndex < 0 || rowIndex >= currentRowCount)
            {
                qWarning() << funcName << " - 忽略无效行索引: " << rowIndex;
                continue;
            }

            // 更新行索引记录
            updateQuery.bindValue(0, tableId);
            updateQuery.bindValue(1, rowIndex);

            if (!updateQuery.exec())
            {
                qWarning() << funcName << " - 无法更新行 " << rowIndex << ": " << updateQuery.lastError().text();
                continue;
            }

            updatedRows++;
        }

        // 3. 更新主记录表中的行数（减去成功删除的行数）
        if (updatedRows > 0)
        {
            QSqlQuery updateMaster(db);
            updateMaster.prepare("UPDATE VectorTableMasterRecord SET row_count = row_count - ? WHERE id = ?");
            updateMaster.bindValue(0, updatedRows);
            updateMaster.bindValue(1, tableId);

            if (!updateMaster.exec())
            {
                errorMessage = "无法更新表 " + QString::number(tableId) + " 的行数: " + updateMaster.lastError().text();
                qWarning() << funcName << " - " << errorMessage;
                db.rollback();
                return false;
            }
        }

        // 4. 重新排列剩余行的索引，确保连续无空洞
        if (!reindexLogicalOrder(tableId, errorMessage))
        {
            qWarning() << funcName << " - 重新索引失败:" << errorMessage;
            db.rollback();
            return false;
        }

        // 5. 提交事务
        db.commit();

        // 6. 删除行后，更新二进制文件头以确保行数同步
        QString headerError;
        if (updateBinaryFileHeader(tableId, headerError))
        {
            qDebug() << funcName << " - 删除行后更新二进制文件头，确保行数同步";
        }
        else
        {
            qWarning() << funcName << " - 更新二进制文件头失败：" << headerError;
            // 不返回false，因为删除操作已经成功，这只是一个同步操作
        }

        qDebug() << funcName << " - 成功删除 " << updatedRows << " 行";
        return true;
    }
    catch (const std::exception &e)
    {
        errorMessage = "删除行时发生异常: " + QString(e.what());
        qWarning() << funcName << " - " << errorMessage;
        db.rollback();
        return false;
    }
}

bool RobustVectorDataHandler::deleteVectorRowsInRange(int tableId, int startRow, int endRow, QString &errorMessage)
{
    const QString funcName = "RobustVectorDataHandler::deleteVectorRowsInRange";
    qDebug() << funcName << " - 开始删除行范围，表ID:" << tableId << "起始行:" << startRow << "结束行:" << endRow;

    // 确保起始行不大于结束行
    if (startRow > endRow)
    {
        int temp = startRow;
        startRow = endRow;
        endRow = temp;
    }

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        errorMessage = "数据库未打开";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 1. 在删除行之前，先获取所有行数据，以便后续重组
    bool ok = false;
    QList<QList<QVariant>> allRowsBeforeDelete = getAllVectorRows(tableId, ok);
    if (!ok)
    {
        errorMessage = "无法获取删除前的所有行数据";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 检查行范围是否有效
    if (startRow < 0 || endRow >= allRowsBeforeDelete.size())
    {
        errorMessage = "无效的行范围";
        qWarning() << funcName << " - " << errorMessage << "，起始行:" << startRow << "，结束行:" << endRow << "，总行数:" << allRowsBeforeDelete.size();
        return false;
    }

    // 计算要删除的行数
    int rowsToDelete = endRow - startRow + 1;
    if (rowsToDelete <= 0)
    {
        errorMessage = "无效的行范围";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 2. 创建新的行数据列表，排除被删除的行
    QList<QList<QVariant>> rowsToKeep;

    // 遍历删除前的所有行
    for (int i = 0; i < allRowsBeforeDelete.size(); i++)
    {
        // 跳过被删除的行范围
        if (i >= startRow && i <= endRow)
        {
            continue;
        }

        // 保留其他行
        rowsToKeep.append(allRowsBeforeDelete[i]);
    }

    // 获取表的列信息
    QList<Vector::ColumnInfo> columns = getAllColumnInfo(tableId);
    if (columns.isEmpty())
    {
        errorMessage = "无法获取表列信息";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 开启事务，确保数据一致性
    db.transaction();

    try
    {
        // 3. 清除所有行索引记录
        QSqlQuery clearQuery(db);
        clearQuery.prepare("DELETE FROM VectorTableRowIndex WHERE master_record_id = ?");
        clearQuery.addBindValue(tableId);
        if (!clearQuery.exec())
        {
            errorMessage = "清除行索引记录失败: " + clearQuery.lastError().text();
            qWarning() << funcName << " - " << errorMessage;
            db.rollback();
            return false;
        }

        // 4. 更新主记录表中的行数（设置为保留的行数）
        QSqlQuery updateMaster(db);
        updateMaster.prepare("UPDATE VectorTableMasterRecord SET row_count = ? WHERE id = ?");
        updateMaster.addBindValue(rowsToKeep.size());
        updateMaster.addBindValue(tableId);

        if (!updateMaster.exec())
        {
            errorMessage = "无法更新表 " + QString::number(tableId) + " 的行数: " + updateMaster.lastError().text();
            qWarning() << funcName << " - " << errorMessage;
            db.rollback();
            return false;
        }

        // 5. 提交事务
        db.commit();

        // 6. 获取二进制文件路径
        QString binFilePath = resolveBinaryFilePath(tableId, errorMessage);
        if (binFilePath.isEmpty())
        {
            errorMessage = "无法获取二进制文件路径";
            qWarning() << funcName << " - " << errorMessage;
            return false;
        }

        // 7. 备份原始文件
        QString backupPath = binFilePath + ".bak";
        QFile::remove(backupPath); // 删除可能存在的旧备份
        QFile::copy(binFilePath, backupPath);

        // 8. 重新创建二进制文件
        QFile binFile(binFilePath);
        if (!binFile.open(QIODevice::ReadWrite | QIODevice::Truncate))
        {
            errorMessage = "无法打开二进制文件进行重写: " + binFile.errorString();
            qWarning() << funcName << " - " << errorMessage;
            return false;
        }

        // 9. 创建新的文件头
        BinaryFileHeader header;
        header.magic_number = Persistence::VEC_BINDATA_MAGIC;
        header.file_format_version = Persistence::CURRENT_FILE_FORMAT_VERSION;
        header.data_schema_version = getSchemaVersion(tableId);
        header.row_count_in_file = rowsToKeep.size(); // 使用实际的保留行数
        header.column_count_in_file = columns.size();
        header.timestamp_created = QDateTime::currentSecsSinceEpoch();
        header.timestamp_updated = QDateTime::currentSecsSinceEpoch();
        header.compression_type = 0;

        // 10. 写入新文件头
        if (!Persistence::BinaryFileHelper::writeBinaryHeader(&binFile, header))
        {
            errorMessage = "写入新文件头失败";
            qWarning() << funcName << " - " << errorMessage;
            binFile.close();
            return false;
        }
        binFile.close();

        // 11. 将保留的行插入到数据库和二进制文件中
        QString insertError;
        if (!insertVectorRows(tableId, 0, rowsToKeep, insertError))
        {
            errorMessage = "重新插入行数据失败: " + insertError;
            qWarning() << funcName << " - " << errorMessage;
            return false;
        }

        // 12. 重新索引逻辑顺序
        if (!reindexLogicalOrder(tableId, errorMessage))
        {
            qWarning() << funcName << " - 重新索引失败:" << errorMessage;
            return false;
        }

        qDebug() << funcName << " - 成功重新组织数据，确保二进制文件与数据库索引一致";
        qDebug() << funcName << " - 成功删除 " << rowsToDelete << " 行";
        return true;
    }
    catch (const std::exception &e)
    {
        errorMessage = "删除行范围时发生异常: " + QString(e.what());
        qWarning() << funcName << " - " << errorMessage;
        db.rollback();
        return false;
    }
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
    const QString funcName = "RobustVectorDataHandler::getAllColumnInfo";

    // 准备需要的变量来调用loadVectorTableMeta
    QString binFileName;
    QList<Vector::ColumnInfo> columns;
    int schemaVersion = 0;
    int totalRowCount = 0;

    // 调用loadVectorTableMeta加载表的元数据
    if (loadVectorTableMeta(tableId, binFileName, columns, schemaVersion, totalRowCount))
    {
        qDebug() << funcName << " - 成功获取表ID:" << tableId << "的列信息，共" << columns.size() << "列";
        return columns;
    }

    qWarning() << funcName << " - 无法获取表ID:" << tableId << "的列信息";
    return {};
}

int RobustVectorDataHandler::getSchemaVersion(int tableId)
{
    const QString funcName = "RobustVectorDataHandler::getSchemaVersion";
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << funcName << " - 数据库未连接";
        return 0;
    }

    QSqlQuery query(db);
    query.prepare("SELECT data_schema_version FROM VectorTableMasterRecord WHERE id = ?");
    query.addBindValue(tableId);

    if (query.exec() && query.next())
    {
        int version = query.value(0).toInt();
        qDebug() << funcName << " - 获取到表" << tableId << "的数据模式版本:" << version;
        return version;
    }

    qWarning() << funcName << " - 无法获取表" << tableId << "的数据模式版本";
    return 0;
}

QList<QList<QVariant>> RobustVectorDataHandler::getAllVectorRows(int tableId, bool &ok)
{
    qDebug() << "[DEBUG_TRACE] ==> 8. RobustVectorDataHandler::getAllVectorRows - Entered function.";
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
    qDebug() << "[DEBUG_TRACE] ==> 9. RobustVectorDataHandler::getAllVectorRows - Calling Persistence::BinaryFileHelper::readPageDataFromBinary()";
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
    bool rollbackTransaction = false;

    // 优化SQLite性能设置
    QSqlQuery pragmaQuery(db);
    pragmaQuery.exec("PRAGMA temp_store = MEMORY");      // 使用内存临时存储
    pragmaQuery.exec("PRAGMA journal_mode = MEMORY");    // 内存日志模式
    pragmaQuery.exec("PRAGMA synchronous = OFF");        // 关闭同步
    pragmaQuery.exec("PRAGMA cache_size = 500000");      // 极大增加缓存大小
    pragmaQuery.exec("PRAGMA foreign_keys = OFF");       // 临时禁用外键约束，提高插入速度
    pragmaQuery.exec("PRAGMA page_size = 8192");         // 增加页大小
    pragmaQuery.exec("PRAGMA mmap_size = 536870912");    // 使用内存映射，512MB
    pragmaQuery.exec("PRAGMA locking_mode = EXCLUSIVE"); // 独占锁定模式
    pragmaQuery.exec("PRAGMA count_changes = OFF");      // 禁用变更计数
    // 禁用自动索引
    pragmaQuery.exec("PRAGMA automatic_index = OFF");

    // 分块处理常量 - 极大增加以提高性能
    const int BATCH_SIZE = 300000;          // 极大提高批处理行数
    const int SQL_BATCH_SIZE = 10000;       // 大幅增加SQL批处理大小
    const int SERIALIZE_BATCH_SIZE = 10000; // 大幅增加序列化批处理大小

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

    // 使用顺序写入模式，优化文件IO
    // 注意：Qt没有直接的API来优化文件写入缓冲区大小
    // 我们依赖操作系统的默认优化和之前设置的SQLite参数

    // 2. 准备批量写入
    bool success = true;
    int rowsProcessed = 0;

    // 预编译SQL语句，以减少解析开销
    QSqlQuery batchInsertQuery(db);
    batchInsertQuery.prepare(
        "INSERT INTO VectorTableRowIndex (master_record_id, logical_row_order, offset, size, is_active) "
        "VALUES (?, ?, ?, ?, 1)");

    // 分批处理大量行，但使用单一事务
    while (rowsProcessed < rows.count())
    {
        int batchEnd = qMin(rowsProcessed + BATCH_SIZE, rows.count());
        // 注释掉批次日志，提高性能
        // qDebug() << funcName << "- 正在处理批次，从" << rowsProcessed << "到" << (batchEnd-1) << "，共" << (batchEnd - rowsProcessed) << "行";

        // 处理当前批次
        for (int i = rowsProcessed; i < batchEnd;)
        {
            // 一次序列化多行数据以减少日志输出和函数调用开销
            int serializeBatchEnd = qMin(i + SERIALIZE_BATCH_SIZE, batchEnd);
            QList<QByteArray> serializedBatch;
            serializedBatch.reserve(serializeBatchEnd - i);

            for (int j = i; j < serializeBatchEnd; j++)
            {
                const Vector::RowData &row = rows.at(j);
                QByteArray serializedRow;

                // 只对每100行输出一次日志，减少日志开销
                // 完全禁用日志，显著提高性能
                /*if (j % 100 == 0)
                {
                    qDebug() << funcName << "- 准备序列化行数据: " << j;
                }*/

                if (!Persistence::BinaryFileHelper::serializeRowSimple(row, serializedRow))
                {
                    errorMessage = QString("Failed to serialize row at logical index %1.").arg(logicalStartIndex + j);
                    success = false;
                    break;
                }
                serializedBatch.append(serializedRow);
            }

            if (!success)
                break;

            // 批量写入二进制文件
            QList<qint64> offsets;
            QList<qint64> sizes;
            QList<int> logicalIndices;
            offsets.reserve(serializedBatch.size());
            sizes.reserve(serializedBatch.size());
            logicalIndices.reserve(serializedBatch.size());

            for (int j = 0; j < serializedBatch.size(); j++)
            {
                qint64 offset = binFile.pos();
                const QByteArray &serializedRow = serializedBatch[j];
                qint64 size = serializedRow.size();

                qint64 bytesWritten = binFile.write(serializedRow);
                if (bytesWritten != size)
                {
                    errorMessage = QString("Failed to write row to binary file at logical index %1.").arg(logicalStartIndex + i + j);
                    success = false;
                    break;
                }

                // 收集SQL值用于批量插入
                offsets.append(offset);
                sizes.append(size);
                logicalIndices.append(logicalStartIndex + i + j);

                // 如果达到SQL批处理大小或是最后一批，执行SQL插入
                if (offsets.size() >= SQL_BATCH_SIZE || j == serializedBatch.size() - 1)
                {
                    // 使用参数绑定执行批量插入
                    {
                        // 使用之前预编译的语句，减少解析开销
                        QVariantList tableIds, logicalOrders, offsetValues, sizeValues;
                        tableIds.reserve(offsets.size());
                        logicalOrders.reserve(offsets.size());
                        offsetValues.reserve(offsets.size());
                        sizeValues.reserve(offsets.size());

                        for (int k = 0; k < offsets.size(); k++)
                        {
                            tableIds.append(tableId);
                            logicalOrders.append(logicalIndices[k]);
                            offsetValues.append(offsets[k]);
                            sizeValues.append(sizes[k]);
                        }

                        batchInsertQuery.addBindValue(tableIds);
                        batchInsertQuery.addBindValue(logicalOrders);
                        batchInsertQuery.addBindValue(offsetValues);
                        batchInsertQuery.addBindValue(sizeValues);

                        if (!batchInsertQuery.execBatch())
                        {
                            errorMessage = "Failed to insert row indexes into database: " + batchInsertQuery.lastError().text();
                            success = false;
                            break;
                        }
                    } // 使用作用域确保QSqlQuery被及时销毁

                    // 清空收集的数据，准备下一批
                    offsets.clear();
                    sizes.clear();
                    logicalIndices.clear();
                }
            }

            // 主动释放内存
            serializedBatch.clear();

            if (!success)
                break;
            i += serializeBatchEnd - i;

            // 极大减少UI更新频率，每处理100000行才更新一次，大幅提高性能
            if (i % 100000 == 0)
            {
                QApplication::processEvents(); // 保持UI响应
            }
        }

        // 执行剩余的SQL批处理
        // 注意：row_count的更新已移至最后统一执行，避免重复更新

        if (!success)
            break;
        rowsProcessed = batchEnd;

        // 强制垃圾回收
        if (rowsProcessed < rows.count() && success)
        {
            // 如果还有更多行要处理，先提交当前事务再开始新事务
            // 减少事务数量：仅在处理超过100万行后才提交事务，大幅提高性能
            if (rowsProcessed >= 1000000)
            {
                if (!db.commit())
                {
                    errorMessage = "Failed to commit batch transaction: " + db.lastError().text();
                    success = false;
                    break;
                }

                if (!db.transaction())
                {
                    errorMessage = "Failed to start new batch transaction: " + db.lastError().text();
                    success = false;
                    break;
                }
            }
        }
    }

    if (success)
    {
        // 更新主表记录中的行数
        QSqlQuery updateQuery(db);
        updateQuery.prepare("UPDATE VectorTableMasterRecord SET row_count = row_count + ? WHERE id = ?");
        updateQuery.addBindValue(rows.count());
        updateQuery.addBindValue(tableId);

        if (!updateQuery.exec())
        {
            errorMessage = "Failed to update master record row count: " + updateQuery.lastError().text();
            success = false;
        }
    }

    // 根据操作结果提交或回滚事务
    if (success)
    {
        if (!db.commit())
        {
            errorMessage = "Failed to commit final transaction: " + db.lastError().text();
            db.rollback();
            success = false;
        }
        else
        {
            // 在成功向数据库插入行之后，立即同步更新二进制文件的文件头
            // 以确保二进制文件头与数据库中的行数保持一致
            QString headerError;
            if (updateBinaryFileHeader(tableId, headerError))
            {
                qDebug() << funcName << "- 插入行后更新二进制文件头，确保行数同步";
            }
            else
            {
                qWarning() << funcName << "- 更新二进制文件头失败：" << headerError;
            }
        }
#ifdef QT_DEBUG
        qDebug() << funcName << "- 成功批量插入" << rows.count() << "行数据";
#endif
    }
    else
    {
        db.rollback();
        qWarning() << funcName << "- 批量插入失败：" << errorMessage;
    }

    // 关闭文件
    binFile.close();

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
    if (!Persistence::BinaryFileHelper::serializeRowSimple(rowData, serializedRow))
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

    // 更新行后，确保二进制文件头与数据库同步
    QString headerError;
    if (updateBinaryFileHeader(tableId, headerError))
    {
        qDebug() << funcName << " - 更新行后同步二进制文件头";
    }
    else
    {
        qWarning() << funcName << " - 更新二进制文件头失败：" << headerError;
        // 不返回false，因为更新行操作已经成功，这只是一个同步操作
    }

    qDebug() << funcName << " - 成功更新表ID:" << tableId << "的行:" << rowIndex;
    return true;
}

bool RobustVectorDataHandler::batchUpdateVectorRows(int tableId, const QMap<int, Vector::RowData> &rows, QString &errorMessage)
{
    const QString funcName = "RobustVectorDataHandler::batchUpdateVectorRows";
    qDebug() << funcName << " - 开始批量更新表ID:" << tableId << "的" << rows.size() << "行数据";

    if (rows.isEmpty())
    {
        qDebug() << funcName << " - 没有行需要更新，直接返回成功";
        return true;
    }

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        errorMessage = "数据库未连接";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 获取二进制文件路径
    QString binFilePath = resolveBinaryFilePath(tableId, errorMessage);
    if (binFilePath.isEmpty())
    {
        return false;
    }

    // 获取列信息
    QList<Vector::ColumnInfo> columns = getAllColumnInfo(tableId);

    // 获取schema版本
    int schemaVersion = getSchemaVersion(tableId);
    if (schemaVersion <= 0)
    {
        errorMessage = "无效的数据模式版本";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 设置数据库性能优化参数
    QSqlQuery pragmaQuery(db);
    pragmaQuery.exec("PRAGMA temp_store = MEMORY");      // 使用内存临时存储
    pragmaQuery.exec("PRAGMA journal_mode = MEMORY");    // 内存日志模式
    pragmaQuery.exec("PRAGMA synchronous = OFF");        // 关闭同步
    pragmaQuery.exec("PRAGMA cache_size = 500000");      // 增加缓存大小
    pragmaQuery.exec("PRAGMA foreign_keys = OFF");       // 禁用外键约束
    pragmaQuery.exec("PRAGMA locking_mode = EXCLUSIVE"); // 独占锁定

    // 开始批量更新行的事务
    if (!db.transaction())
    {
        errorMessage = "无法开始数据库事务: " + db.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 收集所有行索引信息
    QMap<int, QPair<qint64, qint64>> rowIndices; // 行索引 -> <offset, size>
    QSqlQuery indexQuery(db);

    // 由于IN查询可能导致SQL语句过长，改用一个个查询
    indexQuery.prepare("SELECT offset, size FROM VectorTableRowIndex WHERE master_record_id = ? AND logical_row_order = ? AND is_active = 1");

    // 依次查询每个行的索引信息
    bool querySuccess = true;
    for (auto it = rows.constBegin(); it != rows.constEnd(); ++it)
    {
        int rowIndex = it.key();

        indexQuery.addBindValue(tableId);
        indexQuery.addBindValue(rowIndex);

        if (!indexQuery.exec() || !indexQuery.next())
        {
            errorMessage = QString("无法获取行 %1 的索引信息: %2").arg(rowIndex).arg(indexQuery.lastError().text());
            qWarning() << funcName << " - " << errorMessage;
            querySuccess = false;
            break;
        }

        qint64 offset = indexQuery.value(0).toLongLong();
        qint64 size = indexQuery.value(1).toLongLong();
        rowIndices[rowIndex] = qMakePair(offset, size);

        indexQuery.finish(); // 准备下一次执行
    }

    if (!querySuccess)
    {
        db.rollback();
        return false;
    }

    // 打开二进制文件进行写入
    QFile binFile(binFilePath);
    if (!binFile.open(QIODevice::ReadWrite))
    {
        errorMessage = "无法打开二进制文件进行写入: " + binFile.errorString();
        qWarning() << funcName << " - " << errorMessage;
        db.rollback();
        return false;
    }

    // 准备批量更新SQL语句
    QSqlQuery batchUpdateQuery(db);
    batchUpdateQuery.prepare("UPDATE VectorTableRowIndex SET offset = ?, size = ? WHERE master_record_id = ? AND logical_row_order = ? AND is_active = 1");

    QVariantList offsetValues;
    QVariantList sizeValues;
    QVariantList tableIds;
    QVariantList rowIndexValues;

    // 记录行索引和数据需要更新到数据库
    QMap<int, QPair<qint64, qint64>> newIndices;

    // 获取文件当前大小，用于追加写入
    qint64 fileSize = binFile.size();

    // 处理每一行
    bool success = true;
    for (auto it = rows.constBegin(); it != rows.constEnd(); ++it)
    {
        int rowIndex = it.key();
        const Vector::RowData &rowData = it.value();

        // 序列化行数据
        QByteArray serializedRow;
        if (!Persistence::BinaryFileHelper::serializeRowSimple(rowData, serializedRow))
        {
            errorMessage = QString("序列化行 %1 数据失败").arg(rowIndex);
            qWarning() << funcName << " - " << errorMessage;
            success = false;
            break;
        }

        qint64 newSize = serializedRow.size();
        qint64 newOffset;

        // 如果此行存在索引信息
        if (rowIndices.contains(rowIndex))
        {
            qint64 oldOffset = rowIndices[rowIndex].first;
            qint64 oldSize = rowIndices[rowIndex].second;

            // 如果新数据大小不大于旧数据大小，可以原地更新以节省空间
            if (newSize <= oldSize)
            {
                newOffset = oldOffset;
                if (!binFile.seek(oldOffset))
                {
                    errorMessage = QString("无法定位到行 %1 的写入位置").arg(rowIndex);
                    qWarning() << funcName << " - " << errorMessage;
                    success = false;
                    break;
                }
            }
            else // 如果新数据更大，则在文件末尾追加
            {
                newOffset = fileSize;
                if (!binFile.seek(fileSize))
                {
                    errorMessage = QString("无法定位到文件末尾以追加行 %1 数据").arg(rowIndex);
                    qWarning() << funcName << " - " << errorMessage;
                    success = false;
                    break;
                }
                fileSize += newSize; // 更新文件大小
            }
        }
        else
        {
            errorMessage = QString("未找到行 %1 的索引信息").arg(rowIndex);
            qWarning() << funcName << " - " << errorMessage;
            success = false;
            break;
        }

        // 写入数据
        qint64 bytesWritten = binFile.write(serializedRow);
        if (bytesWritten != newSize)
        {
            errorMessage = QString("写入行 %1 数据失败，预期 %2 字节，实际 %3 字节").arg(rowIndex).arg(newSize).arg(bytesWritten);
            qWarning() << funcName << " - " << errorMessage;
            success = false;
            break;
        }

        // 记录新的索引信息
        newIndices[rowIndex] = qMakePair(newOffset, newSize);

        // 添加到批量更新列表
        offsetValues << newOffset;
        sizeValues << newSize;
        tableIds << tableId;
        rowIndexValues << rowIndex;
    }

    // 关闭文件
    binFile.close();

    // 如果所有行处理成功，更新数据库索引
    if (success)
    {
        batchUpdateQuery.addBindValue(offsetValues);
        batchUpdateQuery.addBindValue(sizeValues);
        batchUpdateQuery.addBindValue(tableIds);
        batchUpdateQuery.addBindValue(rowIndexValues);

        if (!batchUpdateQuery.execBatch())
        {
            errorMessage = "批量更新行索引失败: " + batchUpdateQuery.lastError().text();
            qWarning() << funcName << " - " << errorMessage;
            success = false;
        }
    }

    // 根据操作结果提交或回滚事务
    if (success)
    {
        if (!db.commit())
        {
            errorMessage = "提交数据库事务失败: " + db.lastError().text();
            qWarning() << funcName << " - " << errorMessage;
            db.rollback();
            return false;
        }

        // 只在所有行更新完成后执行一次文件头更新
        QString headerError;
        if (updateBinaryFileHeader(tableId, headerError))
        {
            qDebug() << funcName << " - 批量更新后同步二进制文件头成功";
        }
        else
        {
            qWarning() << funcName << " - 更新二进制文件头失败：" << headerError;
            // 不返回false，因为更新行操作已经成功，这只是一个同步操作
        }

        qDebug() << funcName << " - 成功批量更新表ID:" << tableId << "的" << rows.size() << "行数据";
    }
    else
    {
        db.rollback();
        qWarning() << funcName << " - 批量更新失败，已回滚事务";
    }

    return success;
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

// 重新排列行索引，确保连续无空洞
bool RobustVectorDataHandler::reindexLogicalOrder(int tableId, QString &errorMessage)
{
    const QString funcName = "RobustVectorDataHandler::reindexLogicalOrder";
    QSqlDatabase db = DatabaseManager::instance()->database();

    // 1. 获取所有有效的行，按当前的逻辑顺序排序
    QSqlQuery selectQuery(db);
    selectQuery.prepare("SELECT id FROM VectorTableRowIndex WHERE master_record_id = ? AND is_active = 1 ORDER BY logical_row_order ASC");
    selectQuery.addBindValue(tableId);

    if (!selectQuery.exec())
    {
        errorMessage = "Failed to select active rows for re-indexing: " + selectQuery.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    QList<int> activeRowIds;
    while (selectQuery.next())
    {
        activeRowIds.append(selectQuery.value(0).toInt());
    }

    // 2. 批量更新这些行的 logical_row_order 为从0开始的连续序列
    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE VectorTableRowIndex SET logical_row_order = ? WHERE id = ?");

    QVariantList newOrders;
    QVariantList idsToUpdate;

    for (int i = 0; i < activeRowIds.size(); ++i)
    {
        newOrders.append(i);
        idsToUpdate.append(activeRowIds.at(i));
    }

    if (!newOrders.isEmpty())
    {
        updateQuery.addBindValue(newOrders);
        updateQuery.addBindValue(idsToUpdate);

        if (!updateQuery.execBatch())
        {
            errorMessage = "Failed to batch update logical_row_order: " + updateQuery.lastError().text();
            qWarning() << funcName << " - " << errorMessage;
            return false;
        }
    }

    qDebug() << funcName << " - 成功为表ID" << tableId << "重新索引" << activeRowIds.size() << "行";
    return true;
}

#include "robustvectordatahandler_1.cpp"
#include "robustvectordatahandler_2.cpp"
#include "robustvectordatahandler_3.cpp"

// 更新二进制文件头中的行数和列数
bool RobustVectorDataHandler::updateBinaryFileHeader(int tableId, QString &errorMessage)
{
    const QString funcName = "RobustVectorDataHandler::updateBinaryFileHeader";
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        errorMessage = "数据库未连接";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 1. 获取表的元数据信息
    int actualColumnCount = 0;
    int currentRowCount = 0;
    int dbSchemaVersion = -1;
    QString binaryFileNameBase;

    // 获取主记录信息（模式版本、二进制文件名和行数）
    QSqlQuery masterQuery(db);
    masterQuery.prepare("SELECT data_schema_version, binary_data_filename, row_count FROM VectorTableMasterRecord WHERE id = ?");
    masterQuery.addBindValue(tableId);

    if (!masterQuery.exec())
    {
        errorMessage = "无法执行查询VectorTableMasterRecord: " + masterQuery.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    if (masterQuery.next())
    {
        dbSchemaVersion = masterQuery.value("data_schema_version").toInt();
        binaryFileNameBase = masterQuery.value("binary_data_filename").toString();
        currentRowCount = masterQuery.value("row_count").toInt();
        qDebug() << funcName << "- 从数据库获取表ID" << tableId << "的当前行数为" << currentRowCount;
    }
    else
    {
        errorMessage = "找不到表ID为" + QString::number(tableId) + "的主记录";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    if (binaryFileNameBase.isEmpty())
    {
        errorMessage = "表ID为" + QString::number(tableId) + "的主记录中二进制文件名为空";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 2. 获取数据库中配置的实际列数
    QSqlQuery columnQuery(db);
    QString columnSql = "SELECT COUNT(*) FROM VectorTableColumnConfiguration WHERE master_record_id = ?";

    if (!columnQuery.prepare(columnSql))
    {
        errorMessage = "准备查询实际列数失败: " + columnQuery.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }
    columnQuery.addBindValue(tableId);

    if (columnQuery.exec() && columnQuery.next())
    {
        actualColumnCount = columnQuery.value(0).toInt();
    }
    else
    {
        errorMessage = "执行查询实际列数失败: " + columnQuery.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    qDebug() << funcName << "- 从数据库获取表ID" << tableId << "的实际列数为" << actualColumnCount;

    // 3. 构造二进制文件的完整路径
    QString projectDbPath = db.databaseName();
    QString projBinDataDir = Utils::PathUtils::getProjectBinaryDataDirectory(projectDbPath);
    QString binFilePath = projBinDataDir + QDir::separator() + binaryFileNameBase;

    QFile file(binFilePath);
    if (!file.exists())
    {
        errorMessage = "二进制文件不存在，无法更新文件头: " + binFilePath;
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    if (!file.open(QIODevice::ReadWrite))
    {
        errorMessage = "无法以读写模式打开二进制文件: " + binFilePath + " " + file.errorString();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 4. 读取现有文件头或创建新的文件头
    BinaryFileHeader header;
    bool existingHeaderRead = Persistence::BinaryFileHelper::readBinaryHeader(&file, header);

    if (existingHeaderRead)
    {
        // 检查列数、行数和模式版本是否都匹配
        if (header.column_count_in_file == actualColumnCount &&
            header.row_count_in_file == currentRowCount &&
            header.data_schema_version == dbSchemaVersion)
        {
            qDebug() << funcName << "- 文件头的列数 (" << header.column_count_in_file
                     << "), 行数 (" << header.row_count_in_file
                     << ") 和模式版本 (" << header.data_schema_version
                     << ") 已经与数据库匹配。表ID " << tableId << " 无需更新";
            file.close();
            return true;
        }
        // 同时更新列数和行数
        header.column_count_in_file = actualColumnCount;
        header.row_count_in_file = currentRowCount;
        header.data_schema_version = dbSchemaVersion;
        header.timestamp_updated = QDateTime::currentSecsSinceEpoch();
    }
    else
    {
        qWarning() << funcName << "- 无法从 " << binFilePath << " 读取现有文件头。如果这是新创建的表，这是正常的。重新初始化文件头以进行更新。";
        header.magic_number = Persistence::VEC_BINDATA_MAGIC;
        header.file_format_version = Persistence::CURRENT_FILE_FORMAT_VERSION;
        header.data_schema_version = dbSchemaVersion;
        header.row_count_in_file = currentRowCount;
        header.column_count_in_file = actualColumnCount;
        header.timestamp_created = QDateTime::currentSecsSinceEpoch();
        header.timestamp_updated = QDateTime::currentSecsSinceEpoch();
        header.compression_type = 0;
    }

    // 5. 写回更新后的文件头
    file.seek(0);
    if (Persistence::BinaryFileHelper::writeBinaryHeader(&file, header))
    {
        qInfo() << funcName << "- 成功更新表 " << tableId << " 的二进制文件头"
                << ". 路径:" << binFilePath
                << ". 新列数:" << actualColumnCount
                << ", 行数:" << currentRowCount
                << ", 模式版本:" << dbSchemaVersion;
        file.close();
        return true;
    }
    else
    {
        errorMessage = "无法写入更新后的二进制文件头，表ID: " + QString::number(tableId) + ", 路径: " + binFilePath;
        qWarning() << funcName << " - " << errorMessage;
        file.close();
        return false;
    }
}