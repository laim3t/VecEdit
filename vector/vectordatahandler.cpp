#include "vectordatahandler.h"
#include "database/databasemanager.h"
#include "pin/pinvalueedit.h"
#include "database/binaryfilehelper.h"
#include "vector/vector_data_types.h"
#include "common/utils/pathutils.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSet>
#include <QDebug>
#include <algorithm>
#include <QFile>
#include <QJsonDocument>
#include <QDir>
#include <QFileInfo>

namespace
{
    // 辅助函数：加载向量表元数据（文件名、列结构、schema版本、行数）
    bool loadVectorTableMeta(int tableId, QString &binFileName, QList<Vector::ColumnInfo> &columns, int &schemaVersion, int &rowCount)
    {
        const QString funcName = "loadVectorTableMeta";
        qDebug() << funcName << " - Entry for tableId:" << tableId;
        QSqlDatabase db = DatabaseManager::instance()->database();
        if (!db.isOpen())
        {
            qWarning() << funcName << " - 数据库未打开";
            return false;
        }
        // 1. 查询主记录表
        QSqlQuery metaQuery(db);
        metaQuery.prepare("SELECT binary_data_filename, data_schema_version, row_count FROM VectorTableMasterRecord WHERE id = :id");
        metaQuery.bindValue(":id", tableId);
        if (!metaQuery.exec() || !metaQuery.next())
        {
            qWarning() << funcName << " - 查询主记录失败, 表ID:" << tableId << ", 错误:" << metaQuery.lastError().text();
            return false;
        }
        binFileName = metaQuery.value(0).toString();
        schemaVersion = metaQuery.value(1).toInt();
        rowCount = metaQuery.value(2).toInt();
        qDebug() << funcName << " - 文件名:" << binFileName << ", schemaVersion:" << schemaVersion << ", rowCount:" << rowCount;
        // 2. 查询列结构
        QSqlQuery colQuery(db);
        colQuery.prepare("SELECT id, column_name, column_order, column_type, data_properties FROM VectorTableColumnConfiguration WHERE master_record_id = :master_id ORDER BY column_order");
        colQuery.bindValue(":master_id", tableId);
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
            col.logDetails(funcName);
            columns.append(col);
        }
        qDebug() << funcName << " - Finished processing columns for master_record_id:" << tableId << ". Total columns loaded:" << columns.size();
        return true;
    }
    // 辅助函数：从二进制文件读取所有行
    bool readAllRowsFromBinary(const QString &binFileName, const QList<Vector::ColumnInfo> &columns, int schemaVersion, QList<Vector::RowData> &rows)
    {
        const QString funcName = "readAllRowsFromBinary";

        // 获取数据库路径，用于解析相对路径
        QSqlDatabase db = DatabaseManager::instance()->database();
        QString dbFilePath = db.databaseName();
        QFileInfo dbFileInfo(dbFilePath);
        QString dbDir = dbFileInfo.absolutePath();

        // 确保使用正确的绝对路径
        QString absoluteBinFilePath;
        QFileInfo binFileInfo(binFileName);
        if (binFileInfo.isRelative())
        {
            absoluteBinFilePath = dbDir + QDir::separator() + binFileName;
            absoluteBinFilePath = QDir::toNativeSeparators(absoluteBinFilePath);
            qDebug() << funcName << " - 相对路径转换为绝对路径:" << binFileName << " -> " << absoluteBinFilePath;
        }
        else
        {
            absoluteBinFilePath = binFileName;
        }

        qDebug() << funcName << " - 打开文件:" << absoluteBinFilePath;
        QFile file(absoluteBinFilePath);
        if (!file.open(QIODevice::ReadOnly))
        {
            qWarning() << funcName << " - 无法打开文件:" << absoluteBinFilePath << ", 错误:" << file.errorString();
            return false;
        }
        BinaryFileHeader header;
        if (!Persistence::BinaryFileHelper::readBinaryHeader(&file, header))
        {
            qWarning() << funcName << " - 文件头读取失败";
            return false;
        }
        if (header.data_schema_version != schemaVersion)
        {
            qWarning() << funcName << " - 文件schema版本与数据库不一致! 文件:" << header.data_schema_version << ", DB:" << schemaVersion;

            // 版本兼容性处理
            if (header.data_schema_version > schemaVersion)
            {
                qCritical() << funcName << " - 文件版本高于数据库版本，无法加载!";
                file.close();
                return false;
            }

            // 如果文件版本低于数据库版本，可以尝试兼容加载
            qInfo() << funcName << " - 文件版本低于数据库版本，将尝试兼容加载。";
            // 后续的反序列化函数会根据fileVersion参数适配低版本数据
        }
        rows.clear();
        for (quint64 i = 0; i < header.row_count_in_file; ++i)
        {
            QByteArray rowBytes;
            QDataStream in(&file);
            in.setByteOrder(QDataStream::LittleEndian);
            // 先记录当前位置
            qint64 pos = file.pos();
            // 读取一行（假设每行长度不定，需先约定写入方式。此处假设每行前有长度）
            quint32 rowLen = 0;
            in >> rowLen;
            if (in.status() != QDataStream::Ok || rowLen == 0)
            {
                qWarning() << funcName << " - 行长度读取失败, 行:" << i;
                return false;
            }
            rowBytes.resize(rowLen);
            if (file.read(rowBytes.data(), rowLen) != rowLen)
            {
                qWarning() << funcName << " - 行数据读取失败, 行:" << i;
                return false;
            }
            Vector::RowData rowData;
            if (!Persistence::BinaryFileHelper::deserializeRow(rowBytes, columns, header.data_schema_version, rowData))
            {
                qWarning() << funcName << " - 行反序列化失败, 行:" << i;
                return false;
            }
            rows.append(rowData);
        }
        qDebug() << funcName << " - 读取完成, 总行数:" << rows.size();
        return true;
    }
} // 匿名命名空间

/**
 * @brief 私有辅助函数：解析给定表ID的二进制文件的绝对路径。
 *
 * @param tableId 向量表的ID。
 * @param[out] errorMsg 如果发生错误，将填充错误消息。
 * @return QString 如果成功，则为二进制文件的绝对路径；否则为空字符串。
 */
QString VectorDataHandler::resolveBinaryFilePath(int tableId, QString &errorMsg)
{
    const QString funcName = "VectorDataHandler::resolveBinaryFilePath";
    qDebug() << funcName << " - 开始解析表ID的二进制文件路径:" << tableId;

    // 1. 获取该表存储的纯二进制文件名
    QString justTheFileName;
    QList<Vector::ColumnInfo> columns; // We don't need columns here, but loadVectorTableMeta requires it
    int schemaVersion = 0;
    int rowCount = 0; // Not needed here either

    if (!loadVectorTableMeta(tableId, justTheFileName, columns, schemaVersion, rowCount))
    {
        errorMsg = QString("无法加载表 %1 的元数据以获取二进制文件名").arg(tableId);
        qWarning() << funcName << " - " << errorMsg;
        return QString();
    }

    // 检查获取到的文件名是否为空
    if (justTheFileName.isEmpty())
    {
        errorMsg = QString("表 %1 的元数据中缺少二进制文件名").arg(tableId);
        qWarning() << funcName << " - " << errorMsg;
        // 这可能表示一个新创建但尚未完全初始化的表，或者一个损坏的记录。
        // 根据业务逻辑，可能需要返回错误或允许创建/处理。
        // 对于加载和保存，缺少文件名通常是一个错误。
        return QString();
    }

    // 检查文件名是否包含路径分隔符 (指示旧格式或错误数据)
    if (justTheFileName.contains('\\') || justTheFileName.contains('/'))
    {
        qWarning() << funcName << " - 表 " << tableId << " 的二进制文件名 '" << justTheFileName
                   << "' 包含路径分隔符，这可能是旧格式或错误数据。尝试提取文件名部分。";
        // 尝试仅提取文件名部分作为临时解决方案
        QFileInfo fileInfo(justTheFileName);
        justTheFileName = fileInfo.fileName();
        if (justTheFileName.isEmpty())
        {
            errorMsg = QString("无法从表 %1 的记录 '%2' 中提取有效的文件名").arg(tableId).arg(fileInfo.filePath());
            qWarning() << funcName << " - " << errorMsg;
            return QString();
        }
        qWarning() << funcName << " - 提取的文件名: " << justTheFileName << ". 建议运行数据迁移来修复数据库记录。";
        // 注意：这里没有实际移动文件，只是在内存中使用了提取的文件名。
        // 一个完整的迁移工具应该处理文件移动。
    }

    // 2. 获取当前数据库路径
    QSqlDatabase db = DatabaseManager::instance()->database();
    QString currentDbPath = db.databaseName();
    if (currentDbPath.isEmpty())
    {
        errorMsg = "无法获取当前数据库路径";
        qWarning() << funcName << " - " << errorMsg;
        return QString();
    }

    // 3. 使用 PathUtils 获取项目二进制数据目录
    QString projectBinaryDataDir = Utils::PathUtils::getProjectBinaryDataDirectory(currentDbPath);
    if (projectBinaryDataDir.isEmpty())
    {
        errorMsg = QString("无法为数据库 '%1' 生成项目二进制数据目录").arg(currentDbPath);
        qWarning() << funcName << " - " << errorMsg;
        return QString();
    }

    // 4. 拼接得到绝对路径
    // 标准化路径格式并确保使用正确的分隔符
    QString absoluteBinFilePath = QDir::cleanPath(projectBinaryDataDir + QDir::separator() + justTheFileName);
    absoluteBinFilePath = QDir::toNativeSeparators(absoluteBinFilePath);

    qDebug() << funcName << " - 解析得到的绝对路径: " << absoluteBinFilePath
             << " (DB: " << currentDbPath << ", File: " << justTheFileName << ")";

    errorMsg.clear(); // Clear error message on success
    return absoluteBinFilePath;
}

// 重构后的主流程
bool VectorDataHandler::loadVectorTableData(int tableId, QTableWidget *tableWidget)
{
    const QString funcName = "VectorDataHandler::loadVectorTableData";
    qDebug() << funcName << " - Entry for tableId:" << tableId;
    if (!tableWidget)
    {
        qWarning() << funcName << " - tableWidget 为空";
        return false;
    }

    tableWidget->clearContents(); // Use clearContents instead of clear to keep headers
    tableWidget->setRowCount(0);
    // Don't clear columns/headers here if they might already be set,
    // or reset them based on loaded meta later.

    // 1. 读取元数据 (仍然需要，用于获取列信息等)
    QString binFileNameFromMeta; // Variable name emphasizes it's from metadata
    QList<Vector::ColumnInfo> columns;
    int schemaVersion = 0;
    int rowCountFromMeta = 0; // Variable name emphasizes it's from metadata
    if (!loadVectorTableMeta(tableId, binFileNameFromMeta, columns, schemaVersion, rowCountFromMeta))
    {
        qWarning() << funcName << " - 元数据加载失败, 表ID:" << tableId;
        // Clear the table to indicate failure
        tableWidget->setColumnCount(0);
        tableWidget->setRowCount(0);
        return false;
    }
    qDebug() << funcName << " - 元数据加载成功, 列数:" << columns.size() << ", DB记录行数:" << rowCountFromMeta;

    // 如果没有列信息，也无法继续
    if (columns.isEmpty())
    {
        qWarning() << funcName << " - 表 " << tableId << " 没有列配置。";
        tableWidget->setColumnCount(0);
        tableWidget->setRowCount(0);
        // Consider if this is an error or just an empty table state
        return true; // Return true as metadata loaded, but table is empty/unconfigured
    }

    // 2. 解析二进制文件路径
    QString errorMsg;
    QString absoluteBinFilePath = resolveBinaryFilePath(tableId, errorMsg);
    if (absoluteBinFilePath.isEmpty())
    {
        qWarning() << funcName << " - 无法解析二进制文件路径: " << errorMsg;
        // Indicate error state, e.g., clear table, show message
        tableWidget->setColumnCount(0);
        tableWidget->setRowCount(0);
        return false;
    }

    // 检查文件是否存在，如果不存在，对于加载操作来说通常是一个错误，
    // 除非我们允许一个没有二进制文件的表（可能仅元数据）。
    // 新创建的表应该有一个空的带头部的二进制文件。
    if (!QFile::exists(absoluteBinFilePath))
    {
        qWarning() << funcName << " - 二进制文件不存在: " << absoluteBinFilePath;
        // Decide how to handle this. Is it an error? Or just means 0 rows?
        // Let's assume for now it might be a newly created table where binary file creation failed
        // or an old table where the file was lost. Treat as error for now.
        tableWidget->setColumnCount(0);
        tableWidget->setRowCount(0);
        return false; // Return false as data is missing
    }

    // 3. 读取所有行
    QList<Vector::RowData> allRows;
    if (!Persistence::BinaryFileHelper::readAllRowsFromBinary(absoluteBinFilePath, columns, schemaVersion, allRows))
    {
        qWarning() << funcName << " - 二进制数据加载失败 (readAllRowsFromBinary 返回 false), 文件:" << absoluteBinFilePath;
        // Clear table on failure
        tableWidget->setColumnCount(0);
        tableWidget->setRowCount(0);
        return false;
    }
    qDebug() << funcName << " - 从二进制文件加载了 " << allRows.size() << " 行";

    // 校验从文件读取的行数与数据库记录的行数 (可选，但推荐)
    if (allRows.size() != rowCountFromMeta)
    {
        qWarning() << funcName << " - 文件中的行数 (" << allRows.size()
                   << ") 与数据库元数据记录的行数 (" << rowCountFromMeta
                   << ") 不匹配！文件: " << absoluteBinFilePath;
        // Decide how to proceed. Trust the file? Trust the DB? Error out?
        // For now, let's trust the file content but log a warning.
        // Consider adding logic to update the DB row count if file is trusted.
    }

    // 4. 设置表头
    tableWidget->setColumnCount(columns.size());
    QStringList headers;
    for (const auto &col : columns)
    {
        headers << col.name;
    }
    tableWidget->setHorizontalHeaderLabels(headers);

    // 5. 填充数据
    tableWidget->setRowCount(allRows.size());
    qDebug() << funcName << " - 准备填充 " << allRows.size() << " 行到 QTableWidget";
    for (int row = 0; row < allRows.size(); ++row)
    {
        const auto &rowData = allRows[row];
        // Ensure rowData has enough columns, though readAllRowsFromBinary should guarantee this if successful
        if (rowData.size() != columns.size())
        {
            qWarning() << funcName << " - 数据行 " << row << " 的列数 (" << rowData.size()
                       << ") 与表头列数 (" << columns.size() << ") 不匹配! 文件:" << absoluteBinFilePath;
            // Skip this row or handle error appropriately
            continue;
        }
        for (int col = 0; col < columns.size(); ++col)
        {
            // 优化: 避免重复创建 QTableWidgetItem，如果已有则更新
            QTableWidgetItem *item = tableWidget->item(row, col);
            if (!item)
            {
                item = new QTableWidgetItem();
                tableWidget->setItem(row, col, item);
            }
            item->setData(Qt::DisplayRole, rowData.value(col)); // Use DisplayRole
        }
    }
    qDebug() << funcName << " - 加载完成, 行数:" << tableWidget->rowCount() << ", 列数:" << tableWidget->columnCount();
    return true;
}

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
    qDebug() << "VectorDataHandler::~VectorDataHandler - 析构";
}

void VectorDataHandler::cancelOperation()
{
    m_cancelRequested.storeRelease(1);
}

bool VectorDataHandler::saveVectorTableData(int tableId, QTableWidget *tableWidget, QString &errorMessage)
{
    const QString funcName = "VectorDataHandler::saveVectorTableData";
    qDebug() << funcName << " - 开始保存, 表ID:" << tableId;
    if (!tableWidget)
    {
        errorMessage = "表控件为空";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 1. 获取列信息和预期 schema version (从元数据)
    QString ignoredBinFileName; // We resolve the path separately
    QList<Vector::ColumnInfo> columns;
    int schemaVersion = 0;
    int ignoredRowCount = 0;
    if (!loadVectorTableMeta(tableId, ignoredBinFileName, columns, schemaVersion, ignoredRowCount))
    {
        errorMessage = "元数据加载失败，无法确定列结构和版本";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 如果没有列配置，则无法保存
    if (columns.isEmpty())
    {
        errorMessage = QString("表 %1 没有列配置，无法保存数据。").arg(tableId);
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }
    qDebug() << funcName << " - 元数据加载成功, 列数:" << columns.size() << ", Schema版本:" << schemaVersion;

    // 2. 解析二进制文件路径
    QString resolveErrorMsg;
    QString absoluteBinFilePath = resolveBinaryFilePath(tableId, resolveErrorMsg);
    if (absoluteBinFilePath.isEmpty())
    {
        errorMessage = "无法解析二进制文件路径: " + resolveErrorMsg;
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 检查目标目录是否存在，如果不存在则尝试创建
    QFileInfo binFileInfo(absoluteBinFilePath);
    QDir binDir = binFileInfo.dir();
    if (!binDir.exists())
    {
        qInfo() << funcName << " - 目标二进制目录不存在，尝试创建:" << binDir.absolutePath();
        if (!binDir.mkpath("."))
        {
            errorMessage = QString("无法创建目标二进制目录: %1").arg(binDir.absolutePath());
            qWarning() << funcName << " - " << errorMessage;
            return false;
        }
    }

    // 3. 收集所有行数据
    QList<Vector::RowData> allRows;
    int tableRowCount = tableWidget->rowCount();
    int tableColCount = tableWidget->columnCount();
    qDebug() << funcName << " - 从 QTableWidget (行:" << tableRowCount << ", 列:" << tableColCount << ") 收集数据";

    // 检查 QTableWidget 的列数是否与元数据匹配
    if (tableColCount != columns.size())
    {
        errorMessage = QString("表格控件的列数 (%1) 与数据库元数据的列数 (%2) 不匹配。").arg(tableColCount).arg(columns.size());
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    allRows.reserve(tableRowCount);
    for (int row = 0; row < tableRowCount; ++row)
    {
        Vector::RowData rowData;
        rowData.reserve(columns.size());
        for (int col = 0; col < columns.size(); ++col)
        {
            QTableWidgetItem *item = tableWidget->item(row, col);
            // 获取数据，确保使用正确的角色 (DisplayRole) 并处理空项
            QVariant value = item ? item->data(Qt::DisplayRole) : QVariant();
            // TODO: 根据 column[col].type 进行类型转换或验证?
            rowData.append(value);
        }
        allRows.append(rowData);
    }
    qDebug() << funcName << " - 收集了 " << allRows.size() << " 行数据进行保存";

    // 4. 写入二进制文件
    qDebug() << funcName << " - 准备写入数据到二进制文件: " << absoluteBinFilePath;
    if (!Persistence::BinaryFileHelper::writeAllRowsToBinary(absoluteBinFilePath, columns, schemaVersion, allRows))
    {
        errorMessage = QString("写入二进制文件失败: %1").arg(absoluteBinFilePath);
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }
    qDebug() << funcName << " - 二进制文件写入成功";

    // 5. 更新数据库中的行数记录
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery updateQuery(db);

    // 为 SQL 语句添加 prepare 检查
    if (!updateQuery.prepare("UPDATE VectorTableMasterRecord SET row_count = ? WHERE id = ?"))
    {
        errorMessage = QString("数据库更新语句准备失败 (row_count): %1").arg(updateQuery.lastError().text());
        qWarning() << funcName << " - " << errorMessage;
        // 即使数据库更新失败，文件已经写入。根据需求决定是否返回 false。
        // 为了数据一致性，prepare失败通常应视为关键错误。
        return false;
    }

    updateQuery.addBindValue(allRows.size());
    updateQuery.addBindValue(tableId);
    if (!updateQuery.exec())
    {
        errorMessage = QString("更新数据库中的行数记录失败: %1").arg(updateQuery.lastError().text());
        qWarning() << funcName << " - " << errorMessage;
        // 即使数据库更新失败，文件已经写入，所以不算完全失败。
        // 可以选择返回true并记录警告，或者返回false。
        // 返回true，但errorMsg已设置，调用者可以检查。
    }
    else
    {
        qDebug() << funcName << " - 数据库元数据行数已更新为:" << allRows.size();
        errorMessage.clear(); // Clear error message on full success
    }

    return true;
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
    const QString funcName = "VectorDataHandler::insertVectorRows";
    m_cancelRequested.storeRelease(0);
    emit progressUpdated(0); // Start

    qDebug() << funcName << "- NEW BINARY IMPL - 开始插入向量行，表ID:" << tableId
             << "目标行数:" << rowCount << "源数据表行数:" << dataTable->rowCount()
             << "TimesetID:" << timesetId << "Append:" << appendToEnd << "StartIndex:" << startIndex;

    errorMessage.clear();
    emit progressUpdated(2); // Initial checks passed

    // 1. 加载元数据和现有行数据 (如果存在)
    QString binFileName;
    QList<Vector::ColumnInfo> columns;
    int schemaVersion = 0;
    int existingRowCountFromMeta = 0;

    if (!loadVectorTableMeta(tableId, binFileName, columns, schemaVersion, existingRowCountFromMeta))
    {
        errorMessage = QString("无法加载表 %1 的元数据。").arg(tableId);
        qWarning() << funcName << "-" << errorMessage;
        emit progressUpdated(100); // End with error
        return false;
    }
    qDebug() << funcName << "- 元数据加载成功. BinFile:" << binFileName << "SchemaVersion:" << schemaVersion << "Columns:" << columns.size() << "ExistingMetaRows:" << existingRowCountFromMeta;
    emit progressUpdated(5);

    if (columns.isEmpty())
    {
        errorMessage = QString("表 %1 没有列配置信息，无法插入数据.").arg(tableId);
        qWarning() << funcName << "-" << errorMessage;
        emit progressUpdated(100); // End with error
        return false;
    }

    QString absoluteBinFilePath;
    QString resolveError;
    absoluteBinFilePath = resolveBinaryFilePath(tableId, resolveError);
    if (absoluteBinFilePath.isEmpty())
    {
        errorMessage = QString("无法解析表 %1 的二进制文件路径: %2").arg(tableId).arg(resolveError);
        qWarning() << funcName << "-" << errorMessage;
        emit progressUpdated(100); // End with error
        return false;
    }
    qDebug() << funcName << "- 二进制文件绝对路径:" << absoluteBinFilePath;
    emit progressUpdated(8);

    QFileInfo binFileInfo(absoluteBinFilePath);
    QDir binDir = binFileInfo.dir();
    if (!binDir.exists())
    {
        qInfo() << funcName << "- 目标二进制目录不存在，尝试创建:" << binDir.absolutePath();
        if (!binDir.mkpath("."))
        {
            errorMessage = QString("无法创建目标二进制目录: %1").arg(binDir.absolutePath());
            qWarning() << funcName << "-" << errorMessage;
            emit progressUpdated(100); // End with error
            return false;
        }
    }
    emit progressUpdated(10);

    QList<Vector::RowData> allTableRows;
    if (QFile::exists(absoluteBinFilePath))
    {
        qDebug() << funcName << "- 二进制文件存在，尝试加载现有数据:" << absoluteBinFilePath;
        if (!Persistence::BinaryFileHelper::readAllRowsFromBinary(absoluteBinFilePath, columns, schemaVersion, allTableRows))
        {
            qWarning() << funcName << "- 无法从现有二进制文件读取行数据. File:" << absoluteBinFilePath << ". Treating as empty or starting fresh.";
            allTableRows.clear();
        }
        else
        {
            qDebug() << funcName << "- 从二进制文件成功加载" << allTableRows.size() << "行现有数据.";
        }
    }
    else
    {
        qDebug() << funcName << "- 二进制文件不存在:" << absoluteBinFilePath << ". 将创建新文件.";
    }
    emit progressUpdated(20); // Metadata and existing rows loaded

    // 2. 准备新的行数据
    int sourceDataRowCount = dataTable->rowCount();
    if (sourceDataRowCount == 0 && rowCount > 0)
    {
        errorMessage = "源数据表为空，但请求插入多于0行。";
        qWarning() << funcName << "-" << errorMessage;
        emit progressUpdated(100); // End with error
        return false;
    }

    if (rowCount < 0)
    {
        errorMessage = "请求的总行数不能为负。";
        qWarning() << funcName << "-" << errorMessage;
        emit progressUpdated(100); // End with error
        return false;
    }

    int repeatTimes = 1;
    if (sourceDataRowCount > 0)
    {
        if (rowCount % sourceDataRowCount != 0 && rowCount > sourceDataRowCount)
        {
            errorMessage = "请求的总行数必须是源数据表行数的整数倍 (如果大于源数据表行数)。";
            qWarning() << funcName << "-" << errorMessage;
            emit progressUpdated(100); // End with error
            return false;
        }
        if (rowCount == 0) // Special case: insert 0 of the pattern means 0 repeats
            repeatTimes = 0;
        else
            repeatTimes = rowCount / sourceDataRowCount;
    }
    else if (rowCount > 0 && sourceDataRowCount == 0) // Should have been caught by earlier check
    {
        // This case is an error.
        emit progressUpdated(100); // End with error
        return false;
    }
    else // rowCount == 0 and sourceDataRowCount == 0
    {
        repeatTimes = 0;
    }

    qDebug() << funcName << "- 计算重复次数:" << repeatTimes << " (基于请求总行数 " << rowCount << " 和源数据表行数 " << sourceDataRowCount << ")";

    QList<Vector::RowData> newRowsToInsert;
    newRowsToInsert.reserve(rowCount > 0 ? rowCount : 0); // Avoid negative reserve

    QMap<QString, int> columnNameMap;
    for (int i = 0; i < columns.size(); ++i)
    {
        columnNameMap[columns[i].name] = i;
    }

    if (dataTable->columnCount() != selectedPins.size() && sourceDataRowCount > 0)
    {
        errorMessage = QString("对话框提供的列数 (%1) 与选中管脚数 (%2) 不匹配。").arg(dataTable->columnCount()).arg(selectedPins.size());
        qWarning() << funcName << "-" << errorMessage;
        emit progressUpdated(100); // End with error
        return false;
    }

    const int totalIterationsForNewRows = qMax(1, repeatTimes * sourceDataRowCount); // Avoid division by zero for progress
    int currentIterationNewRows = 0;

    for (int rpt = 0; rpt < repeatTimes; ++rpt)
    {
        for (int srcRowIdx = 0; srcRowIdx < sourceDataRowCount; ++srcRowIdx)
        {
            if (m_cancelRequested.loadAcquire())
            {
                qDebug() << funcName << "- 操作被用户取消 (在行数据准备阶段)。";
                errorMessage = "操作被用户取消。";
                emit progressUpdated(100); // End
                return false;
            }

            Vector::RowData newRow;
            newRow.resize(columns.size());

            if (columnNameMap.contains("Instruction"))
                newRow[columnNameMap["Instruction"]] = 1;
            if (columnNameMap.contains("TimeSet"))
                newRow[columnNameMap["TimeSet"]] = timesetId;
            if (columnNameMap.contains("Label"))
                newRow[columnNameMap["Label"]] = "";
            if (columnNameMap.contains("Capture"))
                newRow[columnNameMap["Capture"]] = 0;
            if (columnNameMap.contains("Ext"))
                newRow[columnNameMap["Ext"]] = "";
            if (columnNameMap.contains("Comment"))
                newRow[columnNameMap["Comment"]] = "";

            for (int pinColIdx = 0; pinColIdx < selectedPins.size(); ++pinColIdx)
            {
                const auto &pinSelection = selectedPins[pinColIdx];
                const QString &pinName = pinSelection.second.first;
                if (!columnNameMap.contains(pinName))
                {
                    qWarning() << funcName << "- 警告: 表格列配置中未找到管脚列名:" << pinName << ". 跳过此管脚.";
                    continue;
                }
                int targetColIdx = columnNameMap[pinName];
                PinValueLineEdit *pinEdit = qobject_cast<PinValueLineEdit *>(dataTable->cellWidget(srcRowIdx, pinColIdx));
                QString pinValueStr = pinEdit ? pinEdit->text() : "X";
                if (pinValueStr.isEmpty())
                    pinValueStr = "X";
                newRow[targetColIdx] = pinValueStr;
            }
            newRowsToInsert.append(newRow);
            currentIterationNewRows++;
            if (rowCount > 0)
            { // Only update progress if there are rows to process
                emit progressUpdated(20 + static_cast<int>((static_cast<double>(currentIterationNewRows) / totalIterationsForNewRows) * 40.0));
            }
        }
    }
    qDebug() << funcName << "- 准备了" << newRowsToInsert.size() << "行新数据待插入/追加。";
    emit progressUpdated(60); // New rows prepared

    // 3. 修改 allTableRows
    int actualInsertionIndex = appendToEnd ? allTableRows.size() : startIndex;
    if (actualInsertionIndex < 0)
        actualInsertionIndex = 0;
    if (actualInsertionIndex > allTableRows.size())
        actualInsertionIndex = allTableRows.size();

    if (!appendToEnd)
    {
        qDebug() << funcName << "- 非追加模式，将在索引" << actualInsertionIndex << "处插入" << newRowsToInsert.size() << "行, 当前总行数:" << allTableRows.size();
        if (rowCount == 0 && sourceDataRowCount > 0)
        {
            if (startIndex < allTableRows.size())
            {
                qDebug() << funcName << "- 请求插入0行，非追加模式。将删除从索引" << startIndex << "开始的所有行。";
                allTableRows.erase(allTableRows.begin() + startIndex, allTableRows.end());
            }
        }
        else
        {
            // Use a temporary list for insertion to avoid iterator invalidation issues with QList::insert
            QList<Vector::RowData> tempRowsToKeepPrefix = allTableRows.mid(0, actualInsertionIndex);
            QList<Vector::RowData> tempRowsToKeepSuffix;
            if (actualInsertionIndex < allTableRows.size())
            { // Check if there's anything after insertion point
                tempRowsToKeepSuffix = allTableRows.mid(actualInsertionIndex);
            }
            allTableRows = tempRowsToKeepPrefix;
            allTableRows.append(newRowsToInsert);
            allTableRows.append(tempRowsToKeepSuffix);
        }
    }
    else
    {
        qDebug() << funcName << "- 追加模式，将添加" << newRowsToInsert.size() << "行到末尾。";
        allTableRows.append(newRowsToInsert);
    }
    qDebug() << funcName << "- 修改后，allTableRows 总行数:" << allTableRows.size();
    emit progressUpdated(65); // All table rows modified

    // 4. 写回二进制文件
    qDebug() << funcName << "- 准备写入" << allTableRows.size() << "行到二进制文件:" << absoluteBinFilePath;
    // For now, BinaryFileHelper::writeAllRowsToBinary doesn't have internal progress.
    // We emit progress before and after this potentially long operation.
    if (!Persistence::BinaryFileHelper::writeAllRowsToBinary(absoluteBinFilePath, columns, schemaVersion, allTableRows))
    {
        errorMessage = QString("写入二进制文件失败: %1").arg(absoluteBinFilePath);
        qWarning() << funcName << "-" << errorMessage;
        emit progressUpdated(100); // End with error
        return false;
    }
    qDebug() << funcName << "- 二进制文件写入成功: " << absoluteBinFilePath;
    emit progressUpdated(95); // Binary file written

    // 5. 更新数据库中的主记录
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.transaction())
    {
        errorMessage = "无法开始数据库事务以更新主记录。";
        qWarning() << funcName << "-" << errorMessage;
        emit progressUpdated(100); // End with error
        return false;
    }

    QSqlQuery updateQuery(db);
    if (!updateQuery.prepare("UPDATE VectorTableMasterRecord SET row_count = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?"))
    {
        errorMessage = QString("数据库更新语句准备失败 (row_count): %1").arg(updateQuery.lastError().text());
        qWarning() << funcName << "-" << errorMessage;
        db.rollback();
        emit progressUpdated(100); // End with error
        return false;
    }
    updateQuery.addBindValue(allTableRows.size());
    updateQuery.addBindValue(tableId);

    if (!updateQuery.exec())
    {
        errorMessage = QString("更新数据库中的行数记录失败: %1").arg(updateQuery.lastError().text());
        qWarning() << funcName << "-" << errorMessage;
        db.rollback();
        emit progressUpdated(100); // End with error
        return false;
    }

    if (!db.commit())
    {
        errorMessage = "提交数据库事务失败 (更新主记录后)。";
        qWarning() << funcName << "-" << errorMessage;
        db.rollback();
        emit progressUpdated(100); // End with error
        return false;
    }

    qDebug() << funcName << "- 数据库元数据行数已更新为:" << allTableRows.size() << " for table ID:" << tableId;
    emit progressUpdated(100); // 操作完成
    qDebug() << funcName << "- NEW BINARY IMPL - 向量行数据操作成功完成。";
    return true;
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