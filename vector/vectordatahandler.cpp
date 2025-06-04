#include "vectordatahandler.h"
#include "database/databasemanager.h"
#include "pin/pinvalueedit.h"
#include "database/binaryfilehelper.h"
#include "vector/vector_data_types.h"
#include "common/utils/pathutils.h"
#include "common/binary_file_format.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QDateTime>
#include <QFileInfo>
#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonParseError>
#include <QScrollBar>
#include <QSet>
#include <algorithm>
#include <QCryptographicHash>

namespace
{
    // 辅助函数：加载向量表元数据（文件名、列结构、schema版本、行数）
    bool loadVectorTableMeta(int tableId, QString &binFileName, QList<Vector::ColumnInfo> &columns, int &schemaVersion, int &rowCount)
    {
        const QString funcName = "loadVectorTableMeta";
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
            col.logDetails(funcName);
            columns.append(col);
        }
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
    qDebug() << funcName << " - 开始加载, 表ID:" << tableId;
    if (!tableWidget)
    {
        qWarning() << funcName << " - tableWidget 为空";
        return false;
    }

    // 初始化缓存（如果尚未初始化）
    if (!m_cacheInitialized)
    {
        initializeCache();
    }

    // 禁用表格更新，减少UI重绘，提升性能
    tableWidget->setUpdatesEnabled(false);
    tableWidget->horizontalHeader()->setUpdatesEnabled(false);
    tableWidget->verticalHeader()->setUpdatesEnabled(false);

    // 保存当前滚动条位置
    QScrollBar *vScrollBar = tableWidget->verticalScrollBar();
    QScrollBar *hScrollBar = tableWidget->horizontalScrollBar();
    int vScrollValue = vScrollBar ? vScrollBar->value() : 0;
    int hScrollValue = hScrollBar ? hScrollBar->value() : 0;

    // 阻止信号以避免不必要的更新
    tableWidget->blockSignals(true);

    // 原有的清理逻辑
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

        // 恢复更新和信号
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);
        return false;
    }
    qDebug() << funcName << " - 元数据加载成功, 列数:" << columns.size() << ", DB记录行数:" << rowCountFromMeta;

    // 如果没有列信息，也无法继续
    if (columns.isEmpty())
    {
        qWarning() << funcName << " - 表 " << tableId << " 没有列配置。";
        tableWidget->setColumnCount(0);
        tableWidget->setRowCount(0);

        // 恢复更新和信号
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);
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

        // 恢复更新和信号
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);
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

        // 恢复更新和信号
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);
        return false; // Return false as data is missing
    }

    // 3. 读取所有行的原始数据
    QList<Vector::RowData> allRowsOriginal;

    // 3.1 获取全部列信息（包括隐藏的）以加载完整的二进制数据
    QList<Vector::ColumnInfo> allColumns;
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery colQuery(db);
    colQuery.prepare("SELECT id, column_name, column_order, column_type, data_properties, IsVisible FROM VectorTableColumnConfiguration WHERE master_record_id = ? ORDER BY column_order");
    colQuery.addBindValue(tableId);
    if (!colQuery.exec())
    {
        qWarning() << funcName << " - 查询完整列结构失败, 错误:" << colQuery.lastError().text();
        tableWidget->setColumnCount(0);
        tableWidget->setRowCount(0);

        // 恢复更新和信号
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);
        return false;
    }

    // 构建列映射：原始列索引 -> 可见列索引
    // 这将帮助我们从二进制数据中正确提取数据到UI表中
    QMap<int, int> columnIndexMapping;
    int visibleColIndex = 0;

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
            if (err.error == QJsonParseError::NoError && doc.isObject())
            {
                col.data_properties = doc.object();
            }
        }

        allColumns.append(col);

        // 只为可见列创建映射
        if (col.is_visible)
        {
            // 原始索引 -> 可见索引的映射
            columnIndexMapping[col.order] = visibleColIndex++;
            qDebug() << funcName << " - 列映射: 原始索引" << col.order
                     << " -> 可见索引" << (visibleColIndex - 1)
                     << ", 列名:" << col.name;
        }
    }

    // 加载完整的二进制数据（包括隐藏列）
    if (!Persistence::BinaryFileHelper::readAllRowsFromBinary(absoluteBinFilePath, allColumns, schemaVersion, allRowsOriginal))
    {
        qWarning() << funcName << " - 二进制数据加载失败 (readAllRowsFromBinary 返回 false), 文件:" << absoluteBinFilePath;
        tableWidget->setColumnCount(0);
        tableWidget->setRowCount(0);

        // 恢复更新和信号
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);
            return false;
        }
    qDebug() << funcName << " - 从二进制文件加载了 " << allRowsOriginal.size() << " 行, 原始列数:" << allColumns.size() << ", 可见列数:" << columns.size();

    // 校验从文件读取的行数与数据库记录的行数 (可选，但推荐)
    if (allRowsOriginal.size() != rowCountFromMeta)
    {
        qWarning() << funcName << " - 文件中的行数 (" << allRowsOriginal.size()
                   << ") 与数据库元数据记录的行数 (" << rowCountFromMeta
                   << ") 不匹配！文件: " << absoluteBinFilePath;
        // Decide how to proceed. Trust the file? Trust the DB? Error out?
        // For now, let's trust the file content but log a warning.
        // Consider adding logic to update the DB row count if file is trusted.
    }

    // 4. 设置表头
    tableWidget->setColumnCount(columns.size());
    qDebug() << funcName << " - 设置表格列数:" << columns.size();

    QStringList headers;
    QMap<QString, QString> headerMapping; // 存储原始列名到表头显示文本的映射

    for (const auto &col : columns)
    {
        // 检查是否是管脚列
        if (col.type == Vector::ColumnDataType::PIN_STATE_ID && !col.data_properties.isEmpty())
        {
            // 获取管脚属性
            int channelCount = col.data_properties["channel_count"].toInt(1);
            int typeId = col.data_properties["type_id"].toInt(1);

            // 获取类型名称
            QString typeName = "In"; // 默认为输入类型
            QSqlQuery typeQuery(DatabaseManager::instance()->database());
            typeQuery.prepare("SELECT type_name FROM type_options WHERE id = ?");
            typeQuery.addBindValue(typeId);
            if (typeQuery.exec() && typeQuery.next())
            {
                typeName = typeQuery.value(0).toString();
            }

            // 创建带有管脚信息的表头
            QString headerText = col.name + "\nx" + QString::number(channelCount) + "\n" + typeName;
            headers << headerText;
            headerMapping[col.name] = headerText; // 保存映射关系
            qDebug() << funcName << " - 添加管脚列表头:" << headerText << "，原始列名:" << col.name << "，索引:" << headers.size() - 1;
        }
        else
        {
            // 标准列，直接使用列名
            headers << col.name;
            headerMapping[col.name] = col.name; // 标准列映射相同
            qDebug() << funcName << " - 添加标准列表头:" << col.name << "，索引:" << headers.size() - 1;
        }
    }
    tableWidget->setHorizontalHeaderLabels(headers);
    qDebug() << funcName << " - 设置表头完成，列数:" << headers.size() << "，列表:" << headers.join(", ");

    // 确保所有表头项居中对齐
    for (int i = 0; i < tableWidget->columnCount(); ++i)
    {
        QTableWidgetItem *headerItem = tableWidget->horizontalHeaderItem(i);
        if (headerItem)
        {
            headerItem->setTextAlignment(Qt::AlignCenter);
            qDebug() << funcName << " - 设置表头项居中对齐:" << i << "," << headerItem->text();
        }
    }

    // 确保列数与表头列表一致
    if (tableWidget->columnCount() != headers.size())
    {
        qWarning() << funcName << " - 警告：表格列数 (" << tableWidget->columnCount()
                   << ") 与表头列表数 (" << headers.size() << ") 不一致！";
        tableWidget->setColumnCount(headers.size());
    }

    // 5. 填充数据，根据映射关系处理
    tableWidget->setRowCount(allRowsOriginal.size());
    qDebug() << funcName << " - 准备填充 " << allRowsOriginal.size() << " 行到 QTableWidget";

    // 创建列ID到索引的映射，加速查找
    QMap<int, int> columnIdToIndexMap;
    for (int i = 0; i < allColumns.size(); ++i)
    {
        columnIdToIndexMap[allColumns[i].id] = i;
        qDebug() << funcName << " - 列ID映射: ID=" << allColumns[i].id << " -> 索引=" << i
                 << ", 名称=" << allColumns[i].name << ", 可见=" << allColumns[i].is_visible;
}

    for (int row = 0; row < allRowsOriginal.size(); ++row)
    {
        const Vector::RowData &originalRowData = allRowsOriginal[row];

        // 遍历可见列
        for (int visibleColIdx = 0; visibleColIdx < columns.size(); ++visibleColIdx)
        {
            const auto &visibleCol = columns[visibleColIdx];

            // 查找此可见列在原始数据中的索引位置
            if (!columnIdToIndexMap.contains(visibleCol.id))
            {
                qWarning() << funcName << " - 行" << row << "列" << visibleColIdx
                           << " (" << visibleCol.name << ") 在列ID映射中未找到，使用默认值";

                // 使用默认值
                QTableWidgetItem *newItem = new QTableWidgetItem();
                if (visibleCol.type == Vector::ColumnDataType::PIN_STATE_ID)
                {
                    newItem->setText("X");
                }
                else
                {
                    newItem->setText("");
                }
                tableWidget->setItem(row, visibleColIdx, newItem);
                continue;
            }

            // 获取原始列索引
            int originalColIdx = columnIdToIndexMap[visibleCol.id];

            qDebug() << funcName << " - 找到列映射: UI列" << visibleColIdx
                     << " (" << visibleCol.name << ") -> 原始列" << originalColIdx
                     << " (" << allColumns[originalColIdx].name << "), 类型:"
                     << (visibleCol.type == Vector::ColumnDataType::PIN_STATE_ID ? "管脚列" : "标准列");

            if (originalColIdx >= originalRowData.size())
            {
                qWarning() << funcName << " - 行" << row << "列" << visibleColIdx
                           << " (" << visibleCol.name << ") 超出原始数据范围，使用默认值";

                // 使用默认值
                QTableWidgetItem *newItem = new QTableWidgetItem();
                if (visibleCol.type == Vector::ColumnDataType::PIN_STATE_ID)
                {
                    newItem->setText("X");
                }
                else
                {
                    newItem->setText("");
                }
                tableWidget->setItem(row, visibleColIdx, newItem);
                continue;
            }

            // 获取原始值
            QVariant originalValue = originalRowData[originalColIdx];
            qDebug() << funcName << " - 行" << row << ", 列" << visibleColIdx
                     << " (" << visibleCol.name << ") 从原始列" << originalColIdx
                     << "获取值:" << originalValue;

            // 根据列类型处理数据
            if (visibleCol.type == Vector::ColumnDataType::PIN_STATE_ID)
            {
                // 设置管脚列的值（从缓存中读取）
                QString pinStateText;
                if (originalValue.isNull() || !originalValue.isValid() || originalValue.toString().isEmpty())
                {
                    pinStateText = "X"; // 默认值
                }
                else
                {
                    pinStateText = originalValue.toString();
                }

                // 创建PinValueLineEdit作为单元格控件
                PinValueLineEdit *pinEdit = new PinValueLineEdit(tableWidget);
                pinEdit->setText(pinStateText);
                tableWidget->setCellWidget(row, visibleColIdx, pinEdit);
                qDebug() << funcName << " - 行" << row << ", 列" << visibleColIdx
                         << " 设置管脚状态为:" << pinStateText;
            }
            else if (visibleCol.type == Vector::ColumnDataType::INSTRUCTION_ID)
            {
                // 处理INSTRUCTION_ID类型，从缓存获取指令文本
                int instructionId = originalValue.toInt();
                QString instructionText;

                // 从缓存中获取指令文本
                if (m_instructionCache.contains(instructionId))
                {
                    instructionText = m_instructionCache.value(instructionId);
                    qDebug() << funcName << " - 从缓存获取指令文本，ID:" << instructionId << ", 文本:" << instructionText;
                }
                else
                {
                    // 缓存中没有，从数据库获取
                    QSqlQuery instructionQuery(db);
                    instructionQuery.prepare("SELECT instruction_value FROM instruction_options WHERE id = ?");
                    instructionQuery.addBindValue(instructionId);

                    if (instructionQuery.exec() && instructionQuery.next())
                    {
                        instructionText = instructionQuery.value(0).toString();
                        // 添加到缓存
                        m_instructionCache[instructionId] = instructionText;
                        qDebug() << funcName << " - 添加指令到缓存，ID:" << instructionId << ", 文本:" << instructionText;
                    }
                    else
                    {
                        instructionText = QString("未知(%1)").arg(instructionId);
                        qWarning() << funcName << " - 无法获取指令文本，ID:" << instructionId;
                    }
                }

                QTableWidgetItem *newItem = new QTableWidgetItem(instructionText);
                newItem->setData(Qt::UserRole, instructionId);
                tableWidget->setItem(row, visibleColIdx, newItem);
                qDebug() << funcName << " - 行" << row << ", 列" << visibleColIdx
                         << " 设置指令为:" << instructionText << "(ID:" << instructionId << ")";
            }
            else if (visibleCol.type == Vector::ColumnDataType::TIMESET_ID)
            {
                // 处理TIMESET_ID类型，从缓存获取TimeSet文本
                int timesetId = originalValue.toInt();
                QString timesetText;

                // 从缓存中获取TimeSet文本
                if (m_timesetCache.contains(timesetId))
                {
                    timesetText = m_timesetCache.value(timesetId);
                    qDebug() << funcName << " - 从缓存获取TimeSet文本，ID:" << timesetId << ", 文本:" << timesetText;
                }
                else
                {
                    // 缓存中没有，从数据库获取
                    QSqlQuery timesetQuery(db);
                    timesetQuery.prepare("SELECT timeset_name FROM timeset_list WHERE id = ?");
                    timesetQuery.addBindValue(timesetId);

                    if (timesetQuery.exec() && timesetQuery.next())
                    {
                        timesetText = timesetQuery.value(0).toString();
                        // 添加到缓存
                        m_timesetCache[timesetId] = timesetText;
                        qDebug() << funcName << " - 添加TimeSet到缓存，ID:" << timesetId << ", 文本:" << timesetText;
                    }
                    else
                    {
                        timesetText = QString("未知(%1)").arg(timesetId);
                        qWarning() << funcName << " - 无法获取TimeSet文本，ID:" << timesetId;
                    }
                }

                QTableWidgetItem *newItem = new QTableWidgetItem(timesetText);
                newItem->setData(Qt::UserRole, timesetId);
                tableWidget->setItem(row, visibleColIdx, newItem);
                qDebug() << funcName << " - 行" << row << ", 列" << visibleColIdx
                         << " 设置TimeSet为:" << timesetText << "(ID:" << timesetId << ")";
            }
            else if (visibleCol.type == Vector::ColumnDataType::BOOLEAN)
            {
                // 对于布尔值处理 (Capture)
                bool boolValue = originalValue.toBool();
                QString displayText = boolValue ? "Y" : "N";

                QTableWidgetItem *newItem = new QTableWidgetItem(displayText);
                newItem->setData(Qt::UserRole, boolValue);
                tableWidget->setItem(row, visibleColIdx, newItem);
                qDebug() << funcName << " - 行" << row << ", 列" << visibleColIdx
                         << " 设置布尔值为:" << displayText << "(" << boolValue << ")";
            }
            else
            {
                // 其他类型的列，直接创建QTableWidgetItem
                QTableWidgetItem *newItem = new QTableWidgetItem();
                QVariant originalValue = originalRowData[originalColIdx]; // 添加声明

                // 根据具体类型设置文本
                if (visibleCol.type == Vector::ColumnDataType::INTEGER)
                {
                    newItem->setText(originalValue.isValid() ? QString::number(originalValue.toInt()) : "");
                    newItem->setData(Qt::UserRole, originalValue.isValid() ? originalValue.toInt() : 0);
                }
                else if (visibleCol.type == Vector::ColumnDataType::REAL)
                {
                    newItem->setText(originalValue.isValid() ? QString::number(originalValue.toDouble()) : "");
                    newItem->setData(Qt::UserRole, originalValue.isValid() ? originalValue.toDouble() : 0.0);
                }
                else
                {
                    // TEXT 或其他类型，直接转换为字符串
                    newItem->setText(originalValue.toString());
                }

                tableWidget->setItem(row, visibleColIdx, newItem);
                qDebug() << funcName << " - 行" << row << ", 列" << visibleColIdx
                         << " 设置为:" << newItem->text();
            }
        }
    }

    // 在函数结束前恢复更新和信号
    tableWidget->blockSignals(false);

    // 恢复滚动条位置
    if (vScrollBar)
    {
        vScrollBar->setValue(vScrollValue);
    }
    if (hScrollBar)
    {
        hScrollBar->setValue(hScrollValue);
    }

    // 恢复UI更新
    tableWidget->verticalHeader()->setUpdatesEnabled(true);
    tableWidget->horizontalHeader()->setUpdatesEnabled(true);
    tableWidget->setUpdatesEnabled(true);

    // 强制刷新视图
    tableWidget->viewport()->update();

    qDebug() << funcName << " - 表格填充完成, 总行数:" << tableWidget->rowCount()
             << ", 总列数:" << tableWidget->columnCount();
    return true;
}

// 静态单例实例
VectorDataHandler &VectorDataHandler::instance()
{
    static VectorDataHandler instance;
    return instance;
}

VectorDataHandler::VectorDataHandler() : m_cancelRequested(0), m_cacheInitialized(false)
{
    // 构造函数不变
}

VectorDataHandler::~VectorDataHandler()
{
    qDebug() << "VectorDataHandler::~VectorDataHandler - 析构";
    clearCache();
}

void VectorDataHandler::cancelOperation()
{
    m_cancelRequested.storeRelease(1);
}

void VectorDataHandler::clearCache()
{
    qDebug() << "VectorDataHandler::clearCache - 清除所有缓存数据";
    m_instructionCache.clear();
    m_timesetCache.clear();
    m_cacheInitialized = false;
}

void VectorDataHandler::initializeCache()
{
    if (m_cacheInitialized)
    {
        qDebug() << "VectorDataHandler::initializeCache - 缓存已经初始化，跳过";
        return;
    }

    qDebug() << "VectorDataHandler::initializeCache - 开始初始化缓存";

    // 加载指令和TimeSet缓存
    loadInstructionCache();
    loadTimesetCache();

    m_cacheInitialized = true;
    qDebug() << "VectorDataHandler::initializeCache - 缓存初始化完成";
}

void VectorDataHandler::loadInstructionCache()
{
    qDebug() << "VectorDataHandler::loadInstructionCache - 开始加载指令缓存";
    m_instructionCache.clear();

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << "VectorDataHandler::loadInstructionCache - 数据库未打开";
        return;
    }

    // 一次性获取所有指令
    QSqlQuery allInstructionsQuery(db);
    if (allInstructionsQuery.exec("SELECT id, instruction_value FROM instruction_options"))
    {
        int count = 0;
        while (allInstructionsQuery.next())
        {
            int id = allInstructionsQuery.value(0).toInt();
            QString value = allInstructionsQuery.value(1).toString();
            m_instructionCache[id] = value;
            count++;
        }
        qDebug() << "VectorDataHandler::loadInstructionCache - 已加载" << count << "个指令到缓存";
    }
    else
    {
        qWarning() << "VectorDataHandler::loadInstructionCache - 查询失败:" << allInstructionsQuery.lastError().text();
    }
}

void VectorDataHandler::loadTimesetCache()
{
    qDebug() << "VectorDataHandler::loadTimesetCache - 开始加载TimeSet缓存";
    m_timesetCache.clear();

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << "VectorDataHandler::loadTimesetCache - 数据库未打开";
        return;
    }

    // 一次性获取所有TimeSet
    QSqlQuery allTimesetsQuery(db);
    if (allTimesetsQuery.exec("SELECT id, timeset_name FROM timeset_list"))
    {
        int count = 0;
        while (allTimesetsQuery.next())
        {
            int id = allTimesetsQuery.value(0).toInt();
            QString name = allTimesetsQuery.value(1).toString();
            m_timesetCache[id] = name;
            count++;
        }
        qDebug() << "VectorDataHandler::loadTimesetCache - 已加载" << count << "个TimeSet到缓存";
    }
    else
    {
        qWarning() << "VectorDataHandler::loadTimesetCache - 查询失败:" << allTimesetsQuery.lastError().text();
    }
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

    // 初始化缓存（如果尚未初始化）
    if (!m_cacheInitialized)
    {
        initializeCache();
    }

    // 1. 获取列信息和预期 schema version (从元数据 - 只包含可见列)
    QString ignoredBinFileName; // We resolve the path separately
    QList<Vector::ColumnInfo> visibleColumns;
    int schemaVersion = 0;
    int ignoredRowCount = 0;
    if (!loadVectorTableMeta(tableId, ignoredBinFileName, visibleColumns, schemaVersion, ignoredRowCount))
    {
        errorMessage = "元数据加载失败，无法确定列结构和版本";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 如果没有列配置，则无法保存
    if (visibleColumns.isEmpty())
    {
        errorMessage = QString("表 %1 没有可见的列配置，无法保存数据。").arg(tableId);
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }
    qDebug() << funcName << " - 可见列元数据加载成功, 列数:" << visibleColumns.size() << ", Schema版本:" << schemaVersion;

    // 1.1 加载所有列信息（包括隐藏列）- 这对于正确序列化至关重要
    QList<Vector::ColumnInfo> allColumns;
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery allColQuery(db);
    allColQuery.prepare("SELECT id, column_name, column_order, column_type, data_properties, IsVisible FROM VectorTableColumnConfiguration WHERE master_record_id = ? ORDER BY column_order");
    allColQuery.addBindValue(tableId);
    if (!allColQuery.exec())
    {
        errorMessage = "查询完整列结构失败: " + allColQuery.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    while (allColQuery.next())
    {
        Vector::ColumnInfo col;
        col.id = allColQuery.value(0).toInt();
        col.vector_table_id = tableId;
        col.name = allColQuery.value(1).toString();
        col.order = allColQuery.value(2).toInt();
        col.original_type_str = allColQuery.value(3).toString();
        col.type = Vector::columnDataTypeFromString(col.original_type_str);
        col.is_visible = allColQuery.value(5).toBool();

        QString propStr = allColQuery.value(4).toString();
        if (!propStr.isEmpty())
        {
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(propStr.toUtf8(), &err);
            if (err.error == QJsonParseError::NoError && doc.isObject())
            {
                col.data_properties = doc.object();
            }
        }

        allColumns.append(col);
        qDebug() << funcName << " - 加载列: ID=" << col.id << ", 名称=" << col.name
                 << ", 顺序=" << col.order << ", 可见=" << col.is_visible << ", 类型=" << col.original_type_str;
    }

    // 构建ID到列索引的映射，用于后续查找
    QMap<int, int> columnIdToIndexMap;
    for (int i = 0; i < allColumns.size(); ++i)
    {
        columnIdToIndexMap[allColumns[i].id] = i;
        qDebug() << funcName << " - 列ID映射: ID=" << allColumns[i].id << " -> 索引=" << i
                 << ", 名称=" << allColumns[i].name;
    }

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

    // 3. 建立表格控件列与数据库可见列之间的映射关系
    int tableColCount = tableWidget->columnCount();
    int visibleDbColCount = visibleColumns.size();
    qDebug() << funcName << " - 表格列数:" << tableColCount << ", 数据库可见列数:" << visibleDbColCount
             << ", 数据库总列数:" << allColumns.size();

    // 确保表头与数据库列名一致，构建映射关系
    QMap<int, int> tableColToVisibleDbColMap; // 键: 表格列索引, 值: 数据库可见列索引

    for (int tableCol = 0; tableCol < tableColCount; ++tableCol)
    {
        QString tableHeader = tableWidget->horizontalHeaderItem(tableCol)->text();
        // 对于包含换行符的列名（如管脚列），只取第一行作为管脚名
        QString simplifiedHeader = tableHeader.split("\n").first();

        qDebug() << funcName << " - 处理表格列" << tableCol << ", 原始表头:" << tableHeader
                 << ", 简化后:" << simplifiedHeader;

        // 查找匹配的数据库可见列
        bool found = false;
        for (int dbCol = 0; dbCol < visibleDbColCount; ++dbCol)
        {
            // 对于管脚列，只比较管脚名部分
            if (visibleColumns[dbCol].name == simplifiedHeader ||
                (visibleColumns[dbCol].type == Vector::ColumnDataType::PIN_STATE_ID &&
                 tableHeader.startsWith(visibleColumns[dbCol].name + "\n")))
            {
                tableColToVisibleDbColMap[tableCol] = dbCol;
                qDebug() << funcName << " - 映射表格列" << tableCol << "(" << tableHeader << ") -> 数据库可见列" << dbCol << "(" << visibleColumns[dbCol].name << ")";
                found = true;
                break;
            }
        }
        if (!found)
        {
            qWarning() << funcName << " - 警告: 找不到表格列" << tableCol << "(" << tableHeader << ")对应的数据库可见列";
            errorMessage = QString("无法找到表格列 '%1' 对应的数据库可见列").arg(tableHeader);
            return false;
        }
    }

    // 如果映射关系不完整，无法保存
    if (tableColToVisibleDbColMap.size() != tableColCount)
    {
        errorMessage = QString("表格列与数据库可见列映射不完整，无法保存");
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 4. 收集所有行数据
    QList<Vector::RowData> allRows;
    int tableRowCount = tableWidget->rowCount();
    qDebug() << funcName << " - 从 QTableWidget (行:" << tableRowCount << ", 列:" << tableColCount << ") 收集数据";

    // 预先获取指令和TimeSet的ID映射以便转换
    QMap<QString, int> instructionNameToIdMap;
    QMap<QString, int> timesetNameToIdMap;

    // 获取指令名称到ID的映射
    QSqlQuery instructionQuery(db);
    if (instructionQuery.exec("SELECT id, instruction_value FROM instruction_options"))
    {
        while (instructionQuery.next())
        {
            int id = instructionQuery.value(0).toInt();
            QString name = instructionQuery.value(1).toString();
            instructionNameToIdMap[name] = id;

            // 同时更新缓存
            if (!m_instructionCache.contains(id) || m_instructionCache[id] != name)
            {
                m_instructionCache[id] = name;
                qDebug() << funcName << " - 更新指令缓存: " << name << " -> ID:" << id;
            }

            qDebug() << funcName << " - 指令映射: " << name << " -> ID:" << id;
        }
    }
    else
    {
        qWarning() << funcName << " - 获取指令映射失败: " << instructionQuery.lastError().text();
    }

    // 获取TimeSet名称到ID的映射
    QSqlQuery timesetQuery(db);
    if (timesetQuery.exec("SELECT id, timeset_name FROM timeset_list"))
    {
        while (timesetQuery.next())
        {
            int id = timesetQuery.value(0).toInt();
            QString name = timesetQuery.value(1).toString();
            timesetNameToIdMap[name] = id;

            // 同时更新缓存
            if (!m_timesetCache.contains(id) || m_timesetCache[id] != name)
            {
                m_timesetCache[id] = name;
                qDebug() << funcName << " - 更新TimeSet缓存: " << name << " -> ID:" << id;
            }

            qDebug() << funcName << " - TimeSet映射: " << name << " -> ID:" << id;
        }
    }
    else
    {
        qWarning() << funcName << " - 获取TimeSet映射失败: " << timesetQuery.lastError().text();
    }

    allRows.reserve(tableRowCount);
    for (int row = 0; row < tableRowCount; ++row)
    {
        // 为每一行创建包含所有数据库列的数据（包括隐藏列）
        Vector::RowData rowData;
        // rowData.resize(allColumns.size()); // QT6写法
        int targetSize1 = allColumns.size(); // QT5写法开始 (修正变量名)
        while (rowData.size() < targetSize1) {
            rowData.append(QVariant());
        }
        // QT5写法结束

        // 设置所有列的默认值
        for (int colIdx = 0; colIdx < allColumns.size(); ++colIdx)
        {
            const auto &col = allColumns[colIdx];
            if (col.type == Vector::ColumnDataType::PIN_STATE_ID)
            {
                rowData[colIdx] = "X"; // 管脚列默认为X
            }
            else
            {
                rowData[colIdx] = QVariant(); // 其他列使用默认空值
            }
        }

        // 从表格中读取可见列的实际值
        for (int tableCol = 0; tableCol < tableColCount; ++tableCol)
        {
            int visibleDbCol = tableColToVisibleDbColMap[tableCol];
            const auto &visibleColumn = visibleColumns[visibleDbCol];

            // 找到此可见列对应的原始列索引
            if (!columnIdToIndexMap.contains(visibleColumn.id))
            {
                qWarning() << funcName << " - 警告: 可见列ID" << visibleColumn.id
                           << "(" << visibleColumn.name << ")未在原始列映射中找到";
                continue;
            }

            int originalColIdx = columnIdToIndexMap[visibleColumn.id];

            // 处理单元格内容 - 检查是否为管脚列（单元格控件）
            if (visibleColumn.type == Vector::ColumnDataType::PIN_STATE_ID)
            {
                // 对于管脚列，先尝试从单元格控件获取值
                PinValueLineEdit *pinEdit = qobject_cast<PinValueLineEdit *>(tableWidget->cellWidget(row, tableCol));
                if (pinEdit)
                {
                    QString pinStateText = pinEdit->text();
                    if (pinStateText.isEmpty())
                    {
                        pinStateText = "X"; // 默认值
                    }
                    rowData[originalColIdx] = pinStateText;
                    qDebug() << funcName << " - 行" << row << "列" << tableCol << " 管脚状态(从编辑器): " << pinStateText;
                    continue; // 已处理，跳过后面的QTableWidgetItem检查
                }

                // 如果没有找到编辑控件，也尝试从QTableWidgetItem获取值
                // 这处理了一些非标准情况，比如用户直接修改了单元格而非通过编辑控件
                QTableWidgetItem *item = tableWidget->item(row, tableCol);
                if (item && !item->text().isEmpty())
                {
                    QString pinStateText = item->text();
                    rowData[originalColIdx] = pinStateText;
                    qDebug() << funcName << " - 行" << row << "列" << tableCol << " 管脚状态(从单元格): " << pinStateText;
                    continue;
                }
            }

            // 对于非管脚列或管脚列单元格控件不存在的情况，尝试从QTableWidgetItem获取值
            QTableWidgetItem *item = tableWidget->item(row, tableCol);
            if (!item)
            {
                qDebug() << funcName << " - 行" << row << "列" << tableCol << "项为空，设置为默认值";
                continue; // 使用默认值
            }

            // 根据列类型处理不同格式的数据
            QString cellText = item->text();
            if (visibleColumn.type == Vector::ColumnDataType::INSTRUCTION_ID)
            {
                // 将指令名称转换为ID存储
                if (instructionNameToIdMap.contains(cellText))
                {
                    int instructionId = instructionNameToIdMap[cellText];
                    rowData[originalColIdx] = instructionId;
                    qDebug() << funcName << " - 行" << row << "列" << tableCol << " 指令: " << cellText << " -> ID:" << instructionId;
                }
                else
                {
                    qWarning() << funcName << " - 行" << row << "列" << tableCol << " 未找到指令ID映射: " << cellText;
                    rowData[originalColIdx] = -1; // 默认未知ID
                }
            }
            else if (visibleColumn.type == Vector::ColumnDataType::TIMESET_ID)
            {
                // 将TimeSet名称转换为ID存储
                if (timesetNameToIdMap.contains(cellText))
                {
                    int timesetId = timesetNameToIdMap[cellText];
                    rowData[originalColIdx] = timesetId;
                    qDebug() << funcName << " - 行" << row << "列" << tableCol << " TimeSet: " << cellText << " -> ID:" << timesetId;
                }
                else
                {
                    qWarning() << funcName << " - 行" << row << "列" << tableCol << " 未找到TimeSet ID映射: " << cellText;
                    rowData[originalColIdx] = -1; // 默认未知ID
                }
            }
            else if (visibleColumn.type == Vector::ColumnDataType::BOOLEAN)
            {
                // 对于布尔值处理 (Capture)
                bool boolValue = rowData[originalColIdx].toBool();
                QString displayText = boolValue ? "Y" : "N";

                QTableWidgetItem *newItem = new QTableWidgetItem(displayText);
                newItem->setData(Qt::UserRole, boolValue);
                tableWidget->setItem(row, tableCol, newItem);
                qDebug() << funcName << " - 行" << row << ", 列" << tableCol
                         << " 设置布尔值为:" << displayText << "(" << boolValue << ")";
            }
            else
            {
                // 其他类型的列，直接创建QTableWidgetItem
                QTableWidgetItem *newItem = new QTableWidgetItem();
                QVariant originalValue = rowData[originalColIdx]; // 添加声明

                // 根据具体类型设置文本
                if (visibleColumn.type == Vector::ColumnDataType::INTEGER)
                {
                    newItem->setText(originalValue.isValid() ? QString::number(originalValue.toInt()) : "");
                    newItem->setData(Qt::UserRole, originalValue.isValid() ? originalValue.toInt() : 0);
                }
                else if (visibleColumn.type == Vector::ColumnDataType::REAL)
                {
                    newItem->setText(originalValue.isValid() ? QString::number(originalValue.toDouble()) : "");
                    newItem->setData(Qt::UserRole, originalValue.isValid() ? originalValue.toDouble() : 0.0);
                }
                else
                {
                    // TEXT 或其他类型，直接转换为字符串
                    newItem->setText(originalValue.toString());
                }

                tableWidget->setItem(row, tableCol, newItem);
                qDebug() << funcName << " - 行" << row << ", 列" << tableCol
                         << " 设置为:" << newItem->text();
            }
        }

        allRows.append(rowData);
    }

    // 5. 写入二进制文件 - 使用allColumns而不是visibleColumns
    qDebug() << funcName << " - 准备写入数据到二进制文件: " << absoluteBinFilePath;
    if (!Persistence::BinaryFileHelper::writeAllRowsToBinary(absoluteBinFilePath, allColumns, schemaVersion, allRows))
    {
        errorMessage = QString("写入二进制文件失败: %1").arg(absoluteBinFilePath);
        qWarning() << funcName << "-" << errorMessage;
        return false;
    }
    qDebug() << funcName << " - 二进制文件写入成功";

    // 6. 更新数据库中的行数记录
    if (!db.transaction())
    {
        errorMessage = "无法开始数据库事务以更新主记录。";
        qWarning() << funcName << "-" << errorMessage;
        return false;
    }

    // 计算最终的行数
    int finalRowCount = allRows.size();

    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE VectorTableMasterRecord SET row_count = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?");
    updateQuery.addBindValue(finalRowCount);
    updateQuery.addBindValue(tableId);

    if (!updateQuery.exec())
    {
        errorMessage = QString("更新数据库中的行数记录失败: %1").arg(updateQuery.lastError().text());
        qWarning() << funcName << "-" << errorMessage;
        db.rollback();
        return false;
    }

    if (!db.commit())
    {
        errorMessage = "提交数据库事务失败。";
        qWarning() << funcName << "-" << errorMessage;
        db.rollback();
        return false;
    }

    qDebug() << funcName << "- 数据库元数据行数已更新为:" << finalRowCount << " for table ID:" << tableId;
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

void VectorDataHandler::addVectorRows(QTableWidget *table, const QStringList &pinOptions, int startRowIdx, int count)
{
    const QString funcName = "VectorDataHandler::addVectorRows";
    qDebug() << funcName << "- 开始批量添加" << count << "行，从索引" << startRowIdx << "开始";

    if (count <= 0)
    {
        qDebug() << funcName << "- 要添加的行数必须大于0，当前值:" << count;
        return;
    }

    // 预先设置行数，避免多次调整
    int newRowCount = startRowIdx + count;
    table->setRowCount(newRowCount);

    // 禁用表格更新，提高性能
    table->setUpdatesEnabled(false);

    // 批量添加行
    for (int row = startRowIdx; row < newRowCount; row++)
    {
        // 添加每个管脚的文本输入框
        for (int col = 0; col < table->columnCount(); col++)
        {
            PinValueLineEdit *pinEdit = new PinValueLineEdit(table);
            pinEdit->setText("X");
            table->setCellWidget(row, col, pinEdit);
        }

        // 每处理100行，让出一些CPU时间，避免UI完全冻结
        if ((row - startRowIdx) % 100 == 0 && row > startRowIdx)
        {
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        }
    }

    // 恢复表格更新
    table->setUpdatesEnabled(true);

    qDebug() << funcName << "- 批量添加完成，总行数:" << table->rowCount();
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
    const QString funcName = "VectorDataHandler::deleteVectorRows";
    qDebug() << funcName << "- 开始删除向量行，表ID:" << tableId << "，选中行数:" << rowIndexes.size();

    // 初始化缓存（如果尚未初始化）
    if (!m_cacheInitialized)
    {
        initializeCache();
    }

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        errorMessage = "数据库未打开";
        qWarning() << funcName << "- 错误:" << errorMessage;
        return false;
    }

    // 检查表是否存在
    QSqlQuery tableCheckQuery(db);
    tableCheckQuery.prepare("SELECT table_name FROM vector_tables WHERE id = ?");
    tableCheckQuery.addBindValue(tableId);

    if (!tableCheckQuery.exec() || !tableCheckQuery.next())
    {
        errorMessage = QString("找不到ID为 %1 的向量表").arg(tableId);
        qWarning() << funcName << "- 错误:" << errorMessage;
        return false;
    }

    QString tableName = tableCheckQuery.value(0).toString();
    qDebug() << funcName << "- 向量表名称:" << tableName;

    // 检查选中的行索引是否有效
    if (rowIndexes.isEmpty())
    {
        errorMessage = "未选择任何行";
        qWarning() << funcName << "- 错误:" << errorMessage;
        return false;
    }

    // 1. 加载元数据和二进制文件路径
    QString binFileName;
    QList<Vector::ColumnInfo> columns;
    int schemaVersion = 0;
    int currentRowCount = 0;

    if (!loadVectorTableMeta(tableId, binFileName, columns, schemaVersion, currentRowCount))
    {
        errorMessage = QString("无法加载表 %1 的元数据").arg(tableId);
        qWarning() << funcName << "- 错误:" << errorMessage;
        return false;
    }

    // 如果元数据显示表中没有行
    if (currentRowCount <= 0)
    {
        errorMessage = "表中没有数据行可删除";
        qWarning() << funcName << "- 错误:" << errorMessage;
        return false;
    }

    qDebug() << funcName << "- 从元数据获取的行数:" << currentRowCount;

    // 2. 解析二进制文件路径
    QString resolveError;
    QString absoluteBinFilePath = resolveBinaryFilePath(tableId, resolveError);
    if (absoluteBinFilePath.isEmpty())
    {
        errorMessage = "无法解析二进制文件路径: " + resolveError;
        qWarning() << funcName << "- 错误:" << errorMessage;
        return false;
    }

    // 3. 检查文件是否存在
    QFile binFile(absoluteBinFilePath);
    if (!binFile.exists())
    {
        errorMessage = "找不到二进制数据文件: " + absoluteBinFilePath;
        qWarning() << funcName << "- 错误:" << errorMessage;
        return false;
    }

    // 4. 读取所有行数据
    QList<Vector::RowData> allRows;
    if (!Persistence::BinaryFileHelper::readAllRowsFromBinary(absoluteBinFilePath, columns, schemaVersion, allRows))
    {
        errorMessage = "从二进制文件读取数据失败";
        qWarning() << funcName << "- 错误:" << errorMessage;
        return false;
    }

    qDebug() << funcName << "- 从二进制文件读取到的行数:" << allRows.size();

    // 验证实际读取的行数与元数据中的行数是否一致
    if (allRows.size() != currentRowCount)
    {
        qWarning() << funcName << " - 警告: 元数据中的行数(" << currentRowCount
                   << ")与二进制文件中的行数(" << allRows.size() << ")不一致";
    }

    // 5. 检查行索引是否有效
    QList<int> validRowIndexes;
    for (int rowIndex : rowIndexes)
    {
        if (rowIndex >= 0 && rowIndex < allRows.size())
        {
            validRowIndexes.append(rowIndex);
            qDebug() << funcName << "- 将删除行索引:" << rowIndex;
        }
        else
        {
            qWarning() << funcName << "- 忽略无效的行索引:" << rowIndex << "，总行数:" << allRows.size();
        }
    }

    if (validRowIndexes.isEmpty())
    {
        errorMessage = "没有有效的行索引可删除";
        qWarning() << funcName << "- 错误:" << errorMessage;
        return false;
    }

    // 6. 删除指定的行（从大到小排序，避免索引变化影响删除）
    std::sort(validRowIndexes.begin(), validRowIndexes.end(), std::greater<int>());

    for (int rowIndex : validRowIndexes)
    {
        allRows.removeAt(rowIndex);
        qDebug() << funcName << "- 已删除行索引:" << rowIndex;
    }

    // 7. 将更新后的数据写回文件
    if (!Persistence::BinaryFileHelper::writeAllRowsToBinary(absoluteBinFilePath, columns, schemaVersion, allRows))
    {
        errorMessage = "将更新后的数据写回二进制文件失败";
        qWarning() << funcName << "- 错误:" << errorMessage;
        return false;
    }

    // 8. 更新数据库中的行数记录
    QSqlQuery updateRowCountQuery(db);
    updateRowCountQuery.prepare("UPDATE VectorTableMasterRecord SET row_count = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?");
    updateRowCountQuery.addBindValue(allRows.size());
    updateRowCountQuery.addBindValue(tableId);

    if (!updateRowCountQuery.exec())
    {
        errorMessage = "更新数据库中的行数记录失败: " + updateRowCountQuery.lastError().text();
        qWarning() << funcName << "- 错误:" << errorMessage;
        return false;
    }

    qDebug() << funcName << "- 成功删除了" << validRowIndexes.size() << "行，剩余行数:" << allRows.size();
    return true;
}

int VectorDataHandler::getVectorTableRowCount(int tableId)
{
    const QString funcName = "VectorDataHandler::getVectorTableRowCount";
    qDebug() << funcName << " - 获取表ID为" << tableId << "的行数";

    // 首先尝试从元数据中获取行数
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << funcName << " - 数据库未打开";
        return 0;
    }

    QSqlQuery metaQuery(db);
    metaQuery.prepare("SELECT row_count FROM VectorTableMasterRecord WHERE id = ?");
    metaQuery.addBindValue(tableId);

    if (metaQuery.exec() && metaQuery.next())
    {
        int rowCount = metaQuery.value(0).toInt();
        qDebug() << funcName << " - 从元数据获取的行数:" << rowCount;

        // 如果元数据中的行数大于0，直接返回
        if (rowCount > 0)
        {
            return rowCount;
        }
    }
    else
    {
        qWarning() << funcName << " - 查询元数据行数失败:" << metaQuery.lastError().text();
    }

    // 如果元数据中没有行数或行数为0，尝试从二进制文件头中获取
    QString binFileName;
    QList<Vector::ColumnInfo> columns;
    int schemaVersion = 0;
    int rowCountFromMeta = 0;

    if (!loadVectorTableMeta(tableId, binFileName, columns, schemaVersion, rowCountFromMeta))
    {
        qWarning() << funcName << " - 无法加载元数据";
        return 0;
    }

    // 如果已经从loadVectorTableMeta获取到行数，返回它
    if (rowCountFromMeta > 0)
    {
        qDebug() << funcName << " - 从loadVectorTableMeta获取的行数:" << rowCountFromMeta;
        return rowCountFromMeta;
    }

    // 最后尝试直接从二进制文件头中获取
    QString resolveError;
    QString absoluteBinFilePath = resolveBinaryFilePath(tableId, resolveError);
    if (absoluteBinFilePath.isEmpty())
    {
        qWarning() << funcName << " - 无法解析二进制文件路径:" << resolveError;
        return 0;
    }

    QFile binFile(absoluteBinFilePath);
    if (!binFile.exists())
    {
        qWarning() << funcName << " - 二进制文件不存在:" << absoluteBinFilePath;
        return 0;
    }

    if (binFile.open(QIODevice::ReadOnly))
    {
        BinaryFileHeader header;
        if (Persistence::BinaryFileHelper::readBinaryHeader(&binFile, header))
        {
            qDebug() << funcName << " - 从二进制文件头获取的行数:" << header.row_count_in_file;
            binFile.close();
            return header.row_count_in_file;
        }
        else
        {
            qWarning() << funcName << " - 读取二进制文件头失败";
            binFile.close();
        }
    }
    else
    {
        qWarning() << funcName << " - 无法打开二进制文件:" << binFile.errorString();
    }

    // 如果所有方法都失败，尝试读取整个文件来计算行数
    QList<Vector::RowData> allRows;
    if (Persistence::BinaryFileHelper::readAllRowsFromBinary(absoluteBinFilePath, columns, schemaVersion, allRows))
    {
        int rowsCount = allRows.size();
        qDebug() << funcName << " - 通过读取整个文件获取的行数:" << rowsCount;
        return rowsCount;
    }

    qWarning() << funcName << " - 所有尝试都失败，返回0行";
    return 0;
}

bool VectorDataHandler::insertVectorRows(int tableId, int startIndex, int rowCount, int timesetId,
                                         QTableWidget *dataTable, bool appendToEnd,
                                         const QList<QPair<int, QPair<QString, QPair<int, QString>>>> &selectedPins,
                                         QString &errorMessage)
{
    const QString funcName = "VectorDataHandler::insertVectorRows";
    m_cancelRequested.storeRelease(0);
    emit progressUpdated(0); // Start

    qDebug() << funcName << "- 开始插入向量行，表ID:" << tableId
             << "目标行数:" << rowCount << "源数据表行数:" << dataTable->rowCount()
             << "TimesetID:" << timesetId << "Append:" << appendToEnd << "StartIndex:" << startIndex;

    // 初始化缓存（如果尚未初始化）
    if (!m_cacheInitialized)
    {
        initializeCache();
    }

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

    // 2. 检查和验证行数据
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
        emit progressUpdated(100); // End with error
        return false;
    }
    else // rowCount == 0 and sourceDataRowCount == 0
    {
        repeatTimes = 0;
    }

    qDebug() << funcName << "- 计算重复次数:" << repeatTimes << " (基于请求总行数 " << rowCount << " 和源数据表行数 " << sourceDataRowCount << ")";

    if (dataTable->columnCount() != selectedPins.size() && sourceDataRowCount > 0)
    {
        errorMessage = QString("对话框提供的列数 (%1) 与选中管脚数 (%2) 不匹配。").arg(dataTable->columnCount()).arg(selectedPins.size());
        qWarning() << funcName << "-" << errorMessage;
        emit progressUpdated(100); // End with error
        return false;
    }

    emit progressUpdated(15);

    // 3. 创建二进制文件或读取已有数据
    QList<Vector::RowData> existingRows;

    // 仅在非追加模式下读取现有数据
    if (!appendToEnd && QFile::exists(absoluteBinFilePath))
    {
        qDebug() << funcName << "- 非追加模式，文件存在，尝试加载现有数据:" << absoluteBinFilePath;

        // 确定是否需要读取整个文件
        if (startIndex >= 0 && startIndex <= existingRowCountFromMeta)
        {
            if (!Persistence::BinaryFileHelper::readAllRowsFromBinary(absoluteBinFilePath, columns, schemaVersion, existingRows))
            {
                qWarning() << funcName << "- 无法从现有二进制文件读取行数据. File:" << absoluteBinFilePath << ". 将作为空文件处理。";
                existingRows.clear();
            }
            else
            {
                qDebug() << funcName << "- 从二进制文件成功加载" << existingRows.size() << "行现有数据.";
            }
        }
    }
    else if (appendToEnd && QFile::exists(absoluteBinFilePath))
    {
        // 在追加模式下，只需要确认文件存在并获取行数
        QFile file(absoluteBinFilePath);
        if (file.open(QIODevice::ReadOnly))
        {
            BinaryFileHeader header;
            if (Persistence::BinaryFileHelper::readBinaryHeader(&file, header))
            {
                existingRowCountFromMeta = header.row_count_in_file;
                qDebug() << funcName << "- 追加模式: 从文件头获取现有行数:" << existingRowCountFromMeta;
            }
            file.close();
        }
    }
    else
    {
        qDebug() << funcName << "- 二进制文件不存在:" << absoluteBinFilePath << ". 将创建新文件.";
    }

    emit progressUpdated(20); // 文件准备完成

    // 4. 确定实际插入位置
    int actualInsertionIndex = appendToEnd ? existingRowCountFromMeta : startIndex;
    if (actualInsertionIndex < 0)
        actualInsertionIndex = 0;
    if (actualInsertionIndex > existingRowCountFromMeta)
        actualInsertionIndex = existingRowCountFromMeta;

    // 在非追加模式下，记录插入位置，确保正确处理
    if (!appendToEnd)
    {
        qDebug() << funcName << "- 非追加模式，插入位置:" << actualInsertionIndex
                 << "，现有行数:" << existingRowCountFromMeta
                 << "，新增行数:" << rowCount;
    }

    // 创建按列名检索的映射，提前准备好提高效率
    QMap<QString, int> columnNameMap;
    for (int i = 0; i < columns.size(); ++i)
    {
        columnNameMap[columns[i].name] = i;
        qDebug() << funcName << " - 列映射: 名称=" << columns[i].name << "，索引=" << i << "，类型=" << columns[i].original_type_str;
    }

    // 5. 批量处理数据写入，避免一次性生成大量数据占用内存
    // 增大批处理大小，提高吞吐量
    const int BATCH_SIZE = 50000; // 增大批处理大小，提高性能

    // 对于超过100万行的大数据，做特殊的进度上报处理
    bool isLargeDataset = (rowCount > 1000000);
    int progressStart = 25;
    int progressEnd = isLargeDataset ? 95 : 90; // 大数据集预留更多进度给文件写入操作

    // 计算总批次数
    int totalBatches = (rowCount + BATCH_SIZE - 1) / BATCH_SIZE; // 向上取整
    int currentBatch = 0;
    int totalRowsProcessed = 0;

    qDebug() << funcName << "- 开始批量处理，总批次:" << totalBatches << "，每批大小:" << BATCH_SIZE;

    // 创建临时文件，用于高效写入
    QString tempFilePath = absoluteBinFilePath + ".tmp";
    QFile tempFile(tempFilePath);

    // 预先创建一个模板行，存储标准列的默认值，减少重复计算
    Vector::RowData templateRow;
    // templateRow.resize(columns.size()); // QT6写法
    int targetSize2 = columns.size(); // QT5写法开始
    while (templateRow.size() < targetSize2) {
        templateRow.append(QVariant());
    }
    // QT5写法结束

    // 初始化模板行的标准列
    if (columnNameMap.contains("Label"))
        templateRow[columnNameMap["Label"]] = "";

    if (columnNameMap.contains("Instruction"))
        templateRow[columnNameMap["Instruction"]] = 1; // 使用默认指令ID

    if (columnNameMap.contains("TimeSet"))
        templateRow[columnNameMap["TimeSet"]] = timesetId;

    if (columnNameMap.contains("Capture"))
        templateRow[columnNameMap["Capture"]] = "N"; // 默认不捕获

    if (columnNameMap.contains("Ext"))
        templateRow[columnNameMap["Ext"]] = "";

    if (columnNameMap.contains("Comment"))
        templateRow[columnNameMap["Comment"]] = "";

    // 创建数据临时缓存，用于存储从UITable读取的数据
    QVector<QVector<QString>> pinDataCache;
    pinDataCache.resize(sourceDataRowCount);
    for (int i = 0; i < sourceDataRowCount; i++)
    {
        pinDataCache[i].resize(selectedPins.size());

        // 读取界面表格中的数据并缓存
        for (int pinColIdx = 0; pinColIdx < selectedPins.size(); ++pinColIdx)
        {
            PinValueLineEdit *pinEdit = qobject_cast<PinValueLineEdit *>(dataTable->cellWidget(i, pinColIdx));
            QString pinValueStr = pinEdit ? pinEdit->text() : "X";
            if (pinValueStr.isEmpty())
                pinValueStr = "X";
            pinDataCache[i][pinColIdx] = pinValueStr;
        }
    }

    // 优化：为非追加模式，预先处理现有数据和插入点数据
    BinaryFileHeader header;
    QDataStream out;
    bool fileOpenSuccess = false;

    if (!appendToEnd && !existingRows.isEmpty())
    {
        // 非追加模式，需要处理前部分和后部分的现有数据
        if (!tempFile.open(QIODevice::WriteOnly))
        {
            errorMessage = QString("无法创建临时文件: %1, 错误: %2").arg(tempFilePath).arg(tempFile.errorString());
            qWarning() << funcName << "-" << errorMessage;
            emit progressUpdated(100);
            return false;
        }

        // 准备文件头
        header.magic_number = VBIN_MAGIC_NUMBER;
        header.file_format_version = Persistence::CURRENT_FILE_FORMAT_VERSION;
        header.data_schema_version = schemaVersion;
        header.row_count_in_file = existingRows.size() + rowCount;
        header.column_count_in_file = static_cast<uint32_t>(columns.size());
        header.timestamp_created = static_cast<uint64_t>(QDateTime::currentSecsSinceEpoch());
        header.timestamp_updated = header.timestamp_created;
        header.compression_type = 0;
        memset(header.reserved_bytes, 0, sizeof(header.reserved_bytes));

        // 写入文件头
        if (!Persistence::BinaryFileHelper::writeBinaryHeader(&tempFile, header))
        {
            errorMessage = "无法写入临时文件头";
            qWarning() << funcName << "-" << errorMessage;
            tempFile.close();
            QFile::remove(tempFilePath);
            emit progressUpdated(100);
            return false;
        }

        // 写入插入点之前的现有行
        out.setDevice(&tempFile);
        out.setByteOrder(QDataStream::LittleEndian);

        // 确保actualInsertionIndex不超出现有行的范围
        if (actualInsertionIndex > existingRows.size())
        {
            actualInsertionIndex = existingRows.size();
        }

        // 写入插入点之前的现有行
        for (int i = 0; i < actualInsertionIndex && i < existingRows.size(); i++)
        {
            QByteArray serializedRowData;
            if (!Persistence::BinaryFileHelper::serializeRow(existingRows[i], columns, serializedRowData))
            {
                errorMessage = "序列化现有行数据失败";
                tempFile.close();
                QFile::remove(tempFilePath);
                emit progressUpdated(100);
                return false;
            }

            quint32 rowBlockSize = static_cast<quint32>(serializedRowData.size());
            out << rowBlockSize;
            qint64 bytesWritten = out.writeRawData(serializedRowData.constData(), rowBlockSize);

            if (bytesWritten != rowBlockSize || out.status() != QDataStream::Ok)
            {
                errorMessage = "写入现有行数据失败";
                tempFile.close();
                QFile::remove(tempFilePath);
                emit progressUpdated(100);
                return false;
            }
        }

        fileOpenSuccess = true;
    }
    else if (appendToEnd)
    {
        // 追加模式，直接打开现有文件进行追加或创建新文件
        if (QFile::exists(absoluteBinFilePath))
        {
            bool validFileHeader = false;
            QFile origFile(absoluteBinFilePath);

            // 首先尝试只读方式打开文件验证文件头
            if (origFile.open(QIODevice::ReadOnly))
            {
                qDebug() << funcName << "- 尝试读取现有文件头进行验证";
                if (Persistence::BinaryFileHelper::readBinaryHeader(&origFile, header))
                {
                    validFileHeader = true;
                    qDebug() << funcName << "- 成功验证现有文件头，行数:" << header.row_count_in_file;
                }
                else
                {
                    qWarning() << funcName << "- 现有文件头无效或损坏，将创建新文件";
                }
                origFile.close();
            }

            // 如果文件头有效，创建临时文件并复制原文件内容
            if (validFileHeader)
            {
                // 创建临时文件并复制原文件内容
                if (!QFile::copy(absoluteBinFilePath, tempFilePath))
                {
                    errorMessage = QString("无法复制现有文件到临时文件: %1 -> %2").arg(absoluteBinFilePath).arg(tempFilePath);
                    qWarning() << funcName << "-" << errorMessage;
                    emit progressUpdated(100);
                    return false;
                }

                // 打开临时文件进行追加
                if (!tempFile.open(QIODevice::ReadWrite))
                {
                    errorMessage = QString("无法打开临时文件进行追加: %1, 错误: %2").arg(tempFilePath).arg(tempFile.errorString());
                    qWarning() << funcName << "-" << errorMessage;
                    QFile::remove(tempFilePath);
                    emit progressUpdated(100);
                    return false;
                }

                // 更新文件头信息
                header.row_count_in_file += rowCount;
                header.timestamp_updated = static_cast<uint64_t>(QDateTime::currentSecsSinceEpoch());

                // 重写文件头
                tempFile.seek(0);
                if (!Persistence::BinaryFileHelper::writeBinaryHeader(&tempFile, header))
                {
                    errorMessage = "无法更新临时文件头";
                    qWarning() << funcName << "-" << errorMessage;
                    tempFile.close();
                    QFile::remove(tempFilePath);
                    emit progressUpdated(100);
                    return false;
                }

                // 移动到文件末尾进行追加
                tempFile.seek(tempFile.size());
                out.setDevice(&tempFile);
                out.setByteOrder(QDataStream::LittleEndian);
                fileOpenSuccess = true;
                qDebug() << funcName << "- 成功准备临时文件进行追加，当前位置:" << tempFile.pos();
            }
            else
            {
                // 文件存在但头部无效，作为新文件处理
                qDebug() << funcName << "- 文件存在但头部无效，作为新文件处理";
                if (!tempFile.open(QIODevice::WriteOnly))
                {
                    errorMessage = QString("无法创建临时文件: %1, 错误: %2").arg(tempFilePath).arg(tempFile.errorString());
                    qWarning() << funcName << "-" << errorMessage;
                    emit progressUpdated(100);
                    return false;
                }

                // 准备文件头
                header.magic_number = VBIN_MAGIC_NUMBER;
                header.file_format_version = Persistence::CURRENT_FILE_FORMAT_VERSION;
                header.data_schema_version = schemaVersion;
                header.row_count_in_file = rowCount;
                header.column_count_in_file = static_cast<uint32_t>(columns.size());
                header.timestamp_created = static_cast<uint64_t>(QDateTime::currentSecsSinceEpoch());
                header.timestamp_updated = header.timestamp_created;
                header.compression_type = 0;
                memset(header.reserved_bytes, 0, sizeof(header.reserved_bytes));

                // 写入文件头
                if (!Persistence::BinaryFileHelper::writeBinaryHeader(&tempFile, header))
                {
                    errorMessage = "无法写入临时文件头";
                    qWarning() << funcName << "-" << errorMessage;
                    tempFile.close();
                    QFile::remove(tempFilePath);
                    emit progressUpdated(100);
                    return false;
                }

                out.setDevice(&tempFile);
                out.setByteOrder(QDataStream::LittleEndian);
                fileOpenSuccess = true;
            }
        }
        else
        {
            // 文件不存在，创建新文件
            if (!tempFile.open(QIODevice::WriteOnly))
            {
                errorMessage = QString("无法创建新文件: %1, 错误: %2").arg(tempFilePath).arg(tempFile.errorString());
                qWarning() << funcName << "-" << errorMessage;
                emit progressUpdated(100);
                return false;
            }

            // 准备文件头
            header.magic_number = VBIN_MAGIC_NUMBER;
            header.file_format_version = Persistence::CURRENT_FILE_FORMAT_VERSION;
            header.data_schema_version = schemaVersion;
            header.row_count_in_file = rowCount;
            header.column_count_in_file = static_cast<uint32_t>(columns.size());
            header.timestamp_created = static_cast<uint64_t>(QDateTime::currentSecsSinceEpoch());
            header.timestamp_updated = header.timestamp_created;
            header.compression_type = 0;
            memset(header.reserved_bytes, 0, sizeof(header.reserved_bytes));

            // 写入文件头
            if (!Persistence::BinaryFileHelper::writeBinaryHeader(&tempFile, header))
            {
                errorMessage = "无法写入新文件头";
                qWarning() << funcName << "-" << errorMessage;
                tempFile.close();
                QFile::remove(tempFilePath);
                emit progressUpdated(100);
                return false;
            }

            out.setDevice(&tempFile);
            out.setByteOrder(QDataStream::LittleEndian);
            fileOpenSuccess = true;
        }
    }
    else
    {
        // 非追加模式，创建新文件
        if (!tempFile.open(QIODevice::WriteOnly))
        {
            errorMessage = QString("无法创建新文件: %1, 错误: %2").arg(tempFilePath).arg(tempFile.errorString());
            qWarning() << funcName << "-" << errorMessage;
            emit progressUpdated(100);
            return false;
        }

        // 准备文件头
        header.magic_number = VBIN_MAGIC_NUMBER;
        header.file_format_version = Persistence::CURRENT_FILE_FORMAT_VERSION;
        header.data_schema_version = schemaVersion;
        header.row_count_in_file = rowCount;
        header.column_count_in_file = static_cast<uint32_t>(columns.size());
        header.timestamp_created = static_cast<uint64_t>(QDateTime::currentSecsSinceEpoch());
        header.timestamp_updated = header.timestamp_created;
        header.compression_type = 0;
        memset(header.reserved_bytes, 0, sizeof(header.reserved_bytes));

        // 写入文件头
        if (!Persistence::BinaryFileHelper::writeBinaryHeader(&tempFile, header))
        {
            errorMessage = "无法写入新文件头";
            qWarning() << funcName << "-" << errorMessage;
            tempFile.close();
            QFile::remove(tempFilePath);
            emit progressUpdated(100);
            return false;
        }

        out.setDevice(&tempFile);
        out.setByteOrder(QDataStream::LittleEndian);
        fileOpenSuccess = true;
    }

    if (!fileOpenSuccess)
    {
        errorMessage = "无法准备文件进行写入";
        qWarning() << funcName << "-" << errorMessage;
        emit progressUpdated(100);
        return false;
    }

    // 预先生成并缓存所有可能的行数据
    QList<QByteArray> serializedRowCache;
    if (sourceDataRowCount > 0 && sourceDataRowCount <= 1000) // 只对较小的模式进行缓存
    {
        qDebug() << funcName << "- 预生成行数据缓存";
        // serializedRowCache.resize(sourceDataRowCount); // QT6写法
        int targetSize3 = sourceDataRowCount; // QT5写法开始
        while (serializedRowCache.size() < targetSize3) {
            serializedRowCache.append(QByteArray());
        }
        // QT5写法结束

        for (int i = 0; i < sourceDataRowCount; ++i)
        {
            // 从模板行创建新行
            Vector::RowData newRow = templateRow;

            // 设置管脚列的值
            for (int pinColIdx = 0; pinColIdx < selectedPins.size(); ++pinColIdx)
            {
                const auto &pinSelection = selectedPins[pinColIdx];
                const QString &pinName = pinSelection.second.first;
                if (!columnNameMap.contains(pinName))
                    continue;

                int targetColIdx = columnNameMap[pinName];
                newRow[targetColIdx] = pinDataCache[i][pinColIdx];
            }

            // 序列化行数据并缓存
            QByteArray serializedRowData;
            if (!Persistence::BinaryFileHelper::serializeRow(newRow, columns, serializedRowData))
            {
                errorMessage = "序列化行数据失败";
                tempFile.close();
                QFile::remove(tempFilePath);
                emit progressUpdated(100);
                return false;
            }

            serializedRowCache[i] = serializedRowData;
        }
    }

    // 批量生成和保存数据
    while (totalRowsProcessed < rowCount && !m_cancelRequested.loadAcquire())
    {
        // 计算当前批次要处理的行数
        int currentBatchSize = qMin(BATCH_SIZE, rowCount - totalRowsProcessed);
        qDebug() << funcName << "- 处理批次 " << (currentBatch + 1) << "/" << totalBatches << "，当前批次大小:" << currentBatchSize;

        // 使用缓存的预生成数据或实时生成
        bool useCache = sourceDataRowCount > 0 && sourceDataRowCount <= 1000;

        // 优化：批量准备所有序列化数据，减少IO次数
        QVector<QByteArray> batchSerializedData;
        batchSerializedData.reserve(currentBatchSize);
        quint64 totalBatchDataSize = 0;

        // 先准备当前批次所有行的序列化数据
        for (int i = 0; i < currentBatchSize; ++i)
        {
            // 计算当前行在源数据表中的索引
            int srcRowIdx = (totalRowsProcessed + i) % sourceDataRowCount;
            QByteArray serializedData;

            if (useCache && !serializedRowCache.isEmpty())
            {
                // 使用缓存的序列化数据
                serializedData = serializedRowCache[srcRowIdx];
            }
            else
            {
                // 从模板行创建新行
                Vector::RowData newRow = templateRow;

                // 设置管脚列的值
                for (int pinColIdx = 0; pinColIdx < selectedPins.size(); ++pinColIdx)
                {
                    const auto &pinSelection = selectedPins[pinColIdx];
                    const QString &pinName = pinSelection.second.first;
                    if (!columnNameMap.contains(pinName))
                        continue;

                    int targetColIdx = columnNameMap[pinName];
                    newRow[targetColIdx] = pinDataCache[srcRowIdx][pinColIdx];
                }

                // 序列化行数据
                if (!Persistence::BinaryFileHelper::serializeRow(newRow, columns, serializedData))
                {
                    errorMessage = "序列化行数据失败";
                    tempFile.close();
                    QFile::remove(tempFilePath);
                    emit progressUpdated(100);
                    return false;
                }
            }

            totalBatchDataSize += sizeof(quint32) + serializedData.size(); // 行大小(4字节) + 行数据
            batchSerializedData.append(serializedData);
        }

        // 一次性写入当前批次所有数据
        for (int i = 0; i < currentBatchSize; ++i)
        {
            const QByteArray &serializedData = batchSerializedData[i];
            quint32 rowBlockSize = static_cast<quint32>(serializedData.size());
            out << rowBlockSize;
            qint64 bytesWritten = out.writeRawData(serializedData.constData(), rowBlockSize);

            if (bytesWritten != rowBlockSize || out.status() != QDataStream::Ok)
            {
                errorMessage = "写入行数据失败";
                tempFile.close();
                QFile::remove(tempFilePath);
                emit progressUpdated(100);
                return false;
            }
        }

        // 定期刷新文件，避免缓冲区溢出，但不要太频繁影响性能
        if (currentBatch % 10 == 0) // 减少刷新频率，提高写入性能
        {
            tempFile.flush();
            qDebug() << funcName << "- 已刷新文件，当前已处理" << totalRowsProcessed + currentBatchSize << "行";
        }

        // 更新处理进度
        totalRowsProcessed += currentBatchSize;
        currentBatch++;

        // 更新进度条（非线性进度，大数据进度增长更慢）
        double progress = progressStart + ((double)totalRowsProcessed / rowCount) * (progressEnd - progressStart);
        emit progressUpdated(static_cast<int>(progress));

        // 每批次处理完后，主动让出CPU时间，减轻UI冻结，但在大数据集情况下减少让出频率
        if (currentBatch % (isLargeDataset ? 20 : 10) == 0) // 减少让出CPU频率，提高处理速度
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    // 在非追加模式下，还需要写入插入点之后的现有数据
    if (!appendToEnd && actualInsertionIndex < existingRows.size())
    {
        qDebug() << funcName << "- 写入插入点之后的现有数据，从索引" << actualInsertionIndex << "开始，共"
                 << (existingRows.size() - actualInsertionIndex) << "行";

        for (int i = actualInsertionIndex; i < existingRows.size(); i++)
        {
            QByteArray serializedRowData;
            if (!Persistence::BinaryFileHelper::serializeRow(existingRows[i], columns, serializedRowData))
            {
                errorMessage = "序列化插入点之后的行数据失败";
                tempFile.close();
                QFile::remove(tempFilePath);
                emit progressUpdated(100);
                return false;
            }

            quint32 rowBlockSize = static_cast<quint32>(serializedRowData.size());
            out << rowBlockSize;
            qint64 bytesWritten = out.writeRawData(serializedRowData.constData(), rowBlockSize);

            if (bytesWritten != rowBlockSize || out.status() != QDataStream::Ok)
            {
                errorMessage = "写入插入点之后的行数据失败";
                tempFile.close();
                QFile::remove(tempFilePath);
                emit progressUpdated(100);
                return false;
            }
        }
    }

    // 刷新文件并关闭
    tempFile.flush();
    tempFile.close();

    // 如果用户取消了操作
    if (m_cancelRequested.loadAcquire())
    {
        qDebug() << funcName << "- 操作被用户取消。";
        errorMessage = "操作被用户取消。";
        QFile::remove(tempFilePath);
        emit progressUpdated(100);
        return false;
    }

    // 替换原文件
    if (QFile::exists(absoluteBinFilePath))
    {
        if (!QFile::remove(absoluteBinFilePath))
        {
            errorMessage = QString("无法删除原文件: %1").arg(absoluteBinFilePath);
            qWarning() << funcName << "-" << errorMessage;
            QFile::remove(tempFilePath);
            emit progressUpdated(100);
            return false;
        }
    }

    if (!QFile::rename(tempFilePath, absoluteBinFilePath))
    {
        errorMessage = QString("无法重命名临时文件: %1 -> %2").arg(tempFilePath).arg(absoluteBinFilePath);
        qWarning() << funcName << "-" << errorMessage;
        QFile::remove(tempFilePath);
        emit progressUpdated(100);
        return false;
    }

    emit progressUpdated(isLargeDataset ? 95 : 90); // 文件写入完成

    // 更新数据库中的行数记录
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.transaction())
    {
        errorMessage = "无法开始数据库事务以更新主记录。";
        qWarning() << funcName << "-" << errorMessage;
        emit progressUpdated(100);
        return false;
    }

    // 计算最终的行数
    int finalRowCount;
    if (appendToEnd)
    {
        finalRowCount = existingRowCountFromMeta + rowCount;
    }
    else
    {
        // 非追加模式，最终行数 = 原有行数 + 新增行数
        finalRowCount = existingRows.size() + rowCount;
    }

    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE VectorTableMasterRecord SET row_count = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?");
    updateQuery.addBindValue(finalRowCount);
    updateQuery.addBindValue(tableId);

    if (!updateQuery.exec())
    {
        errorMessage = QString("更新数据库中的行数记录失败: %1").arg(updateQuery.lastError().text());
        qWarning() << funcName << "-" << errorMessage;
        db.rollback();
        emit progressUpdated(100);
        return false;
    }

    if (!db.commit())
    {
        errorMessage = "提交数据库事务失败。";
        qWarning() << funcName << "-" << errorMessage;
        db.rollback();
        emit progressUpdated(100);
        return false;
    }

    qDebug() << funcName << "- 数据库元数据行数已更新为:" << finalRowCount << " for table ID:" << tableId;
    emit progressUpdated(100); // 操作完成
    qDebug() << funcName << "- 向量行数据操作成功完成。";
    return true;
}

bool VectorDataHandler::deleteVectorRowsInRange(int tableId, int fromRow, int toRow, QString &errorMessage)
{
    const QString funcName = "VectorDataHandler::deleteVectorRowsInRange";
    qDebug() << funcName << " - 开始删除范围内的向量行，表ID：" << tableId
             << "，从行：" << fromRow << "，到行：" << toRow;

    // 初始化缓存（如果尚未初始化）
    if (!m_cacheInitialized)
    {
        initializeCache();
    }

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        errorMessage = "数据库未打开";
        qWarning() << funcName << " - 错误：" << errorMessage;
        return false;
    }

    // 检查表是否存在
    QSqlQuery tableCheckQuery(db);
    tableCheckQuery.prepare("SELECT table_name FROM vector_tables WHERE id = ?");
    tableCheckQuery.addBindValue(tableId);

    if (!tableCheckQuery.exec() || !tableCheckQuery.next())
    {
        errorMessage = QString("找不到ID为 %1 的向量表").arg(tableId);
        qWarning() << funcName << " - 错误:" << errorMessage;
        return false;
    }

    QString tableName = tableCheckQuery.value(0).toString();
    qDebug() << funcName << " - 向量表名称:" << tableName;

    // 检查输入的行范围是否有效
    if (fromRow <= 0 || toRow <= 0)
    {
        errorMessage = "无效的行范围：起始行和结束行必须大于0";
        qWarning() << funcName << " - 错误：" << errorMessage;
        return false;
    }

    // 1. 加载元数据和二进制文件路径
    QString binFileName;
    QList<Vector::ColumnInfo> columns;
    int schemaVersion = 0;
    int currentRowCount = 0;

    if (!loadVectorTableMeta(tableId, binFileName, columns, schemaVersion, currentRowCount))
    {
        errorMessage = QString("无法加载表 %1 的元数据").arg(tableId);
        qWarning() << funcName << " - 错误：" << errorMessage;
        return false;
    }

    // 如果元数据显示表中没有行
    if (currentRowCount <= 0)
    {
        errorMessage = "表中没有数据行可删除";
        qWarning() << funcName << " - 错误：" << errorMessage;
        return false;
    }

    qDebug() << funcName << " - 从元数据获取的行数:" << currentRowCount;

    // 2. 解析二进制文件路径
    QString resolveError;
    QString absoluteBinFilePath = resolveBinaryFilePath(tableId, resolveError);
    if (absoluteBinFilePath.isEmpty())
    {
        errorMessage = "无法解析二进制文件路径: " + resolveError;
        qWarning() << funcName << " - 错误：" << errorMessage;
        return false;
    }

    // 3. 检查文件是否存在
    QFile binFile(absoluteBinFilePath);
    if (!binFile.exists())
    {
        errorMessage = "找不到二进制数据文件: " + absoluteBinFilePath;
        qWarning() << funcName << " - 错误：" << errorMessage;
        return false;
    }

    // 4. 读取所有行数据
    QList<Vector::RowData> allRows;
    if (!Persistence::BinaryFileHelper::readAllRowsFromBinary(absoluteBinFilePath, columns, schemaVersion, allRows))
    {
        errorMessage = "从二进制文件读取数据失败";
        qWarning() << funcName << " - 错误：" << errorMessage;
        return false;
    }

    qDebug() << funcName << " - 从二进制文件读取到的行数:" << allRows.size();

    // 验证实际读取的行数与元数据中的行数是否一致
    if (allRows.size() != currentRowCount)
    {
        qWarning() << funcName << " - 警告: 元数据中的行数(" << currentRowCount
                   << ")与二进制文件中的行数(" << allRows.size() << ")不一致";
    }

    // 5. 检查行范围是否有效
    if (fromRow > toRow || fromRow < 1 || toRow > allRows.size())
    {
        errorMessage = QString("无效的行范围：从%1到%2，表总行数为%3").arg(fromRow).arg(toRow).arg(allRows.size());
        qWarning() << funcName << " - 错误：" << errorMessage;
        return false;
    }

    // 调整为基于0的索引
    int startIndex = fromRow - 1;
    int endIndex = toRow - 1;

    // 计算要删除的行数
    int rowsToDelete = endIndex - startIndex + 1;
    qDebug() << funcName << "- 将删除从索引" << startIndex << "到" << endIndex << "的" << rowsToDelete << "行";

    // 6. 删除指定范围的行（从大到小删除，避免索引变化）
    for (int i = endIndex; i >= startIndex; i--)
    {
        allRows.removeAt(i);
        qDebug() << funcName << "- 已删除行索引:" << i;
    }

    // 7. 将更新后的数据写回文件
    if (!Persistence::BinaryFileHelper::writeAllRowsToBinary(absoluteBinFilePath, columns, schemaVersion, allRows))
    {
        errorMessage = "将更新后的数据写回二进制文件失败";
        qWarning() << funcName << " - 错误：" << errorMessage;
        return false;
    }

    // 8. 更新数据库中的行数记录
    QSqlQuery updateRowCountQuery(db);
    updateRowCountQuery.prepare("UPDATE VectorTableMasterRecord SET row_count = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?");
    updateRowCountQuery.addBindValue(allRows.size());
    updateRowCountQuery.addBindValue(tableId);

    if (!updateRowCountQuery.exec())
    {
        errorMessage = "更新数据库中的行数记录失败: " + updateRowCountQuery.lastError().text();
        qWarning() << funcName << " - 错误：" << errorMessage;
        return false;
    }

    qDebug() << funcName << " - 成功删除了范围内的 " << rowsToDelete << " 行，剩余行数: " << allRows.size();
    return true;
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

bool VectorDataHandler::hideVectorTableColumn(int tableId, const QString &columnName, QString &errorMessage)
{
    const QString funcName = "VectorDataHandler::hideVectorTableColumn";
    qDebug() << funcName << " - 开始逻辑删除列, 表ID:" << tableId << ", 列名:" << columnName;

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        errorMessage = "数据库未打开";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 检查列是否存在
    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT id FROM VectorTableColumnConfiguration WHERE master_record_id = ? AND column_name = ?");
    checkQuery.addBindValue(tableId);
    checkQuery.addBindValue(columnName);

    if (!checkQuery.exec())
    {
        errorMessage = "查询列信息失败: " + checkQuery.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    if (!checkQuery.next())
    {
        errorMessage = QString("找不到表 %1 中的列 '%2'").arg(tableId).arg(columnName);
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    int columnId = checkQuery.value(0).toInt();
    qDebug() << funcName << " - 找到列ID:" << columnId;

    // 更新IsVisible字段为0（逻辑删除）
    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE VectorTableColumnConfiguration SET IsVisible = 0 WHERE id = ?");
    updateQuery.addBindValue(columnId);

    if (!updateQuery.exec())
    {
        errorMessage = "更新列可见性失败: " + updateQuery.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    qDebug() << funcName << " - 成功将列 '" << columnName << "' 标记为不可见 (IsVisible=0)";
    return true;
}

bool VectorDataHandler::showVectorTableColumn(int tableId, const QString &columnName, QString &errorMessage)
{
    const QString funcName = "VectorDataHandler::showVectorTableColumn";
    qDebug() << funcName << " - 开始恢复列显示, 表ID:" << tableId << ", 列名:" << columnName;

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        errorMessage = "数据库未打开";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 检查列是否存在
    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT id FROM VectorTableColumnConfiguration WHERE master_record_id = ? AND column_name = ?");
    checkQuery.addBindValue(tableId);
    checkQuery.addBindValue(columnName);

    if (!checkQuery.exec())
    {
        errorMessage = "查询列信息失败: " + checkQuery.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    if (!checkQuery.next())
    {
        errorMessage = QString("找不到表 %1 中的列 '%2'").arg(tableId).arg(columnName);
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    int columnId = checkQuery.value(0).toInt();
    qDebug() << funcName << " - 找到列ID:" << columnId;

    // 更新IsVisible字段为1（恢复显示）
    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE VectorTableColumnConfiguration SET IsVisible = 1 WHERE id = ?");
    updateQuery.addBindValue(columnId);

    if (!updateQuery.exec())
    {
        errorMessage = "更新列可见性失败: " + updateQuery.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    qDebug() << funcName << " - 成功将列 '" << columnName << "' 标记为可见 (IsVisible=1)";
    return true;
}

// 实现分页加载数据方法
bool VectorDataHandler::loadVectorTablePageData(int tableId, QTableWidget *tableWidget, int pageIndex, int pageSize)
{
    const QString funcName = "VectorDataHandler::loadVectorTablePageData";
    qDebug() << funcName << " - 开始加载分页数据, 表ID:" << tableId
             << ", 页码:" << pageIndex << ", 每页行数:" << pageSize;

    if (!tableWidget)
    {
        qWarning() << funcName << " - tableWidget 为空";
        return false;
    }

    // 初始化缓存（如果尚未初始化）
    if (!m_cacheInitialized)
    {
        initializeCache();
    }

    // 禁用表格更新，减少UI重绘，提升性能
    tableWidget->setUpdatesEnabled(false);
    tableWidget->horizontalHeader()->setUpdatesEnabled(false);
    tableWidget->verticalHeader()->setUpdatesEnabled(false);

    // 保存当前滚动条位置
    QScrollBar *vScrollBar = tableWidget->verticalScrollBar();
    QScrollBar *hScrollBar = tableWidget->horizontalScrollBar();
    int vScrollValue = vScrollBar ? vScrollBar->value() : 0;
    int hScrollValue = hScrollBar ? hScrollBar->value() : 0;

    // 阻止信号以避免不必要的更新
    tableWidget->blockSignals(true);

    // 清理现有内容，但保留表头
    tableWidget->clearContents();

    // 1. 读取元数据 (需要获取列信息和总行数)
    QString binFileNameFromMeta; // 从元数据获取的文件名
    QList<Vector::ColumnInfo> columns;
    int schemaVersion = 0;
    int totalRowCount = 0;

    if (!loadVectorTableMeta(tableId, binFileNameFromMeta, columns, schemaVersion, totalRowCount))
    {
        qWarning() << funcName << " - 元数据加载失败, 表ID:" << tableId;
        // 清理表格，表示失败
        tableWidget->setRowCount(0);

        // 恢复更新和信号
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);
        return false;
    }
    qDebug() << funcName << " - 元数据加载成功, 列数:" << columns.size() << ", 总行数:" << totalRowCount;

    // 计算分页参数
    int totalPages = (totalRowCount + pageSize - 1) / pageSize; // 向上取整
    if (pageIndex < 0)
        pageIndex = 0;
    if (pageIndex >= totalPages && totalPages > 0)
        pageIndex = totalPages - 1;

    int startRow = pageIndex * pageSize;
    int rowsToLoad = qMin(pageSize, totalRowCount - startRow);

    qDebug() << funcName << " - 分页参数: 总页数=" << totalPages
             << ", 当前页=" << pageIndex << ", 起始行=" << startRow
             << ", 加载行数=" << rowsToLoad;

    // 设置表格行数
    tableWidget->setRowCount(rowsToLoad);

    // 如果没有数据，直接返回成功
    if (rowsToLoad <= 0)
    {
        qDebug() << funcName << " - 当前页没有数据, 直接返回";

        // 恢复更新和信号
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);
        return true;
    }

    // 如果列数为0，也无法继续
    if (columns.isEmpty())
    {
        qWarning() << funcName << " - 表 " << tableId << " 没有列配置。";
        tableWidget->setRowCount(0);

        // 恢复更新和信号
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);
        return true; // 返回true，因为元数据已加载，只是表为空
    }

    // 2. 解析二进制文件路径
    QString errorMsg;
    QString absoluteBinFilePath = resolveBinaryFilePath(tableId, errorMsg);
    if (absoluteBinFilePath.isEmpty())
    {
        qWarning() << funcName << " - 无法解析二进制文件路径: " << errorMsg;
        // 设置错误状态
        tableWidget->setRowCount(0);

        // 恢复更新和信号
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);
        return false;
    }

    // 检查文件是否存在
    if (!QFile::exists(absoluteBinFilePath))
    {
        qWarning() << funcName << " - 二进制文件不存在: " << absoluteBinFilePath;
        tableWidget->setRowCount(0);

        // 恢复更新和信号
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);
        return false;
    }

    // 3. 设置表格列（如果需要）
    if (tableWidget->columnCount() != columns.size())
    {
        tableWidget->setColumnCount(columns.size());

        // 设置表头
        QStringList headers;
        for (const auto &col : columns)
        {
            // 根据列类型设置表头
            if (col.type == Vector::ColumnDataType::PIN_STATE_ID && !col.data_properties.isEmpty())
            {
                // 获取管脚属性
                int channelCount = col.data_properties["channel_count"].toInt(1);
                int typeId = col.data_properties["type_id"].toInt(1);

                // 获取类型名称
                QString typeName = "In"; // 默认为输入类型
                QSqlQuery typeQuery(DatabaseManager::instance()->database());
                typeQuery.prepare("SELECT type_name FROM type_options WHERE id = ?");
                typeQuery.addBindValue(typeId);
                if (typeQuery.exec() && typeQuery.next())
                {
                    typeName = typeQuery.value(0).toString();
                }

                // 创建带有管脚信息的表头
                QString headerText = col.name + "\nx" + QString::number(channelCount) + "\n" + typeName;
                headers << headerText;
            }
            else
            {
                // 标准列，直接使用列名
                headers << col.name;
            }
        }

        tableWidget->setHorizontalHeaderLabels(headers);

        // 设置表头居中对齐
        for (int i = 0; i < tableWidget->columnCount(); ++i)
        {
            QTableWidgetItem *headerItem = tableWidget->horizontalHeaderItem(i);
            if (headerItem)
            {
                headerItem->setTextAlignment(Qt::AlignCenter);
            }
        }
    }

    // 4. 读取指定范围的行数据
    QList<Vector::RowData> pageRows;
    QList<Vector::ColumnInfo> allColumns;

    // 获取完整列信息（包括隐藏列）
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery colQuery(db);
    colQuery.prepare("SELECT id, column_name, column_order, column_type, data_properties, IsVisible FROM VectorTableColumnConfiguration WHERE master_record_id = ? ORDER BY column_order");
    colQuery.addBindValue(tableId);

    if (!colQuery.exec())
    {
        qWarning() << funcName << " - 查询完整列结构失败, 错误:" << colQuery.lastError().text();
        tableWidget->setRowCount(0);

        // 恢复更新和信号
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);
        return false;
    }

    // 构建列映射
    QMap<int, int> columnIndexMapping;
    int visibleColIndex = 0;

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
            if (err.error == QJsonParseError::NoError && doc.isObject())
            {
                col.data_properties = doc.object();
            }
        }

        allColumns.append(col);

        // 只为可见列创建映射
        if (col.is_visible)
        {
            // 原始索引 -> 可见索引的映射
            columnIndexMapping[col.order] = visibleColIndex++;
        }
    }

    // 使用BinaryFileHelper读取指定范围的行数据
    QFile file(absoluteBinFilePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << funcName << " - 无法打开二进制文件:" << absoluteBinFilePath;
        tableWidget->setRowCount(0);

        // 恢复更新和信号
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);
        return false;
    }

    // 读取文件头
    BinaryFileHeader header;
    if (!Persistence::BinaryFileHelper::readBinaryHeader(&file, header))
    {
        qWarning() << funcName << " - 无法读取二进制文件头:" << absoluteBinFilePath;
        file.close();
        tableWidget->setRowCount(0);

        // 恢复更新和信号
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);
        return false;
    }

    // 验证行范围
    if (startRow >= header.row_count_in_file)
    {
        qWarning() << funcName << " - 起始行(" << startRow << ")超出文件总行数(" << header.row_count_in_file << ")";
        file.close();
        tableWidget->setRowCount(0);

        // 恢复更新和信号
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);
        return false;
    }

    // 准备数据结构
    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);

    // 跳过文件头

    // 跳过前面的行数据
    for (int i = 0; i < startRow; ++i)
    {
        quint32 rowBlockSize = 0;
        in >> rowBlockSize;
        if (in.status() != QDataStream::Ok || rowBlockSize == 0)
        {
            qWarning() << funcName << " - 读取行" << i << "的块大小失败";
            file.close();
            tableWidget->setRowCount(0);

            // 恢复更新和信号
            tableWidget->blockSignals(false);
            tableWidget->verticalHeader()->setUpdatesEnabled(true);
            tableWidget->horizontalHeader()->setUpdatesEnabled(true);
            tableWidget->setUpdatesEnabled(true);
            return false;
        }

        // 跳过这一行的数据
        if (!file.seek(file.pos() + rowBlockSize))
        {
            qWarning() << funcName << " - 无法跳过行" << i << "的数据";
            file.close();
            tableWidget->setRowCount(0);

            // 恢复更新和信号
            tableWidget->blockSignals(false);
            tableWidget->verticalHeader()->setUpdatesEnabled(true);
            tableWidget->horizontalHeader()->setUpdatesEnabled(true);
            tableWidget->setUpdatesEnabled(true);
            return false;
        }
    }

    // 读取当前页的行数据
    pageRows.reserve(rowsToLoad);

    // 记录加载开始时间，用于计算性能统计
    QDateTime startTime = QDateTime::currentDateTime();
    int lastReportedPercent = -1;
    const int LOG_INTERVAL = 100; // 分页加载使用较小的日志间隔

    for (int i = 0; i < rowsToLoad; ++i)
    {
        quint32 rowBlockSize = 0;
        in >> rowBlockSize;
        if (in.status() != QDataStream::Ok || rowBlockSize == 0)
        {
            qWarning() << funcName << " - 读取块大小失败，行:" << (startRow + i);
            break;
        }

        // 读取这一行的数据
        QByteArray rowDataBlock;
        rowDataBlock.resize(rowBlockSize);
        qint64 bytesRead = in.readRawData(rowDataBlock.data(), rowBlockSize);

        if (bytesRead != rowBlockSize)
        {
            qWarning() << funcName << " - 读取行数据失败，行:" << (startRow + i);
            break;
        }

        // 反序列化这一行
        Vector::RowData rowData;
        if (!Persistence::BinaryFileHelper::deserializeRow(rowDataBlock, allColumns, header.data_schema_version, rowData))
        {
            qWarning() << funcName << " - 反序列化行失败，行:" << (startRow + i);
            // 创建一个空行
            rowData.clear();
            for (int j = 0; j < allColumns.size(); ++j)
            {
                rowData.append(QVariant());
            }
        }

        pageRows.append(rowData);

        // 记录进度
        if (rowsToLoad > LOG_INTERVAL)
        {
            int percent = static_cast<int>((i + 1) * 100 / rowsToLoad);
            if (percent != lastReportedPercent && (i % LOG_INTERVAL == LOG_INTERVAL - 1 || percent - lastReportedPercent >= 10))
            {
                QDateTime currentTime = QDateTime::currentDateTime();
                qint64 elapsedMs = startTime.msecsTo(currentTime);
                double rowsPerSecond = (elapsedMs > 0) ? ((i + 1) * 1000.0 / elapsedMs) : 0;
                qDebug() << funcName << " - 页面数据加载进度: " << (i + 1) << "/" << rowsToLoad
                         << " (" << percent << "%), 速度: " << rowsPerSecond << " 行/秒";
                lastReportedPercent = percent;
            }
        }
    }

    QDateTime endTime = QDateTime::currentDateTime();
    qint64 totalElapsedMs = startTime.msecsTo(endTime);
    if (pageRows.size() > 0 && totalElapsedMs > 100)
    { // 仅当加载时间超过100毫秒时记录
        double avgRowsPerSecond = (totalElapsedMs > 0) ? (pageRows.size() * 1000.0 / totalElapsedMs) : 0;
        qDebug() << funcName << " - 页面数据加载完成: 读取" << pageRows.size() << "行, 耗时: "
                 << (totalElapsedMs / 1000.0) << "秒, 平均速度: " << avgRowsPerSecond << "行/秒";
    }

    file.close();

    // 5. 填充表格
    // 创建列ID到索引的映射，加速查找
    QMap<int, int> columnIdToIndexMap;
    for (int i = 0; i < allColumns.size(); ++i)
    {
        columnIdToIndexMap[allColumns[i].id] = i;
    }

    for (int row = 0; row < pageRows.size(); ++row)
    {
        const Vector::RowData &originalRowData = pageRows[row];

        // 遍历可见列
        for (int visibleColIdx = 0; visibleColIdx < columns.size(); ++visibleColIdx)
        {
            const auto &visibleCol = columns[visibleColIdx];

            // 查找此可见列在原始数据中的索引位置
            if (!columnIdToIndexMap.contains(visibleCol.id))
            {
                // 使用默认值
                QTableWidgetItem *newItem = new QTableWidgetItem();
                if (visibleCol.type == Vector::ColumnDataType::PIN_STATE_ID)
                {
                    newItem->setText("X");
                }
                else
                {
                    newItem->setText("");
                }
                tableWidget->setItem(row, visibleColIdx, newItem);
                continue;
            }

            // 获取原始列索引
            int originalColIdx = columnIdToIndexMap[visibleCol.id];

            if (originalColIdx >= originalRowData.size())
            {
                // 使用默认值
                QTableWidgetItem *newItem = new QTableWidgetItem();
                if (visibleCol.type == Vector::ColumnDataType::PIN_STATE_ID)
                {
                    newItem->setText("X");
                }
                else
                {
                    newItem->setText("");
                }
                tableWidget->setItem(row, visibleColIdx, newItem);
                continue;
            }

            // 获取原始值
            QVariant originalValue = originalRowData[originalColIdx];

            // 根据列类型处理数据
            if (visibleCol.type == Vector::ColumnDataType::PIN_STATE_ID)
            {
                // 设置管脚列的值
                QString pinStateText;
                if (originalValue.isNull() || !originalValue.isValid() || originalValue.toString().isEmpty())
                {
                    pinStateText = "X"; // 默认值
                }
                else
                {
                    pinStateText = originalValue.toString();
                }

                // 创建PinValueLineEdit作为单元格控件
                PinValueLineEdit *pinEdit = new PinValueLineEdit(tableWidget);
                pinEdit->setText(pinStateText);
                tableWidget->setCellWidget(row, visibleColIdx, pinEdit);
            }
            else if (visibleCol.type == Vector::ColumnDataType::INSTRUCTION_ID)
            {
                // 处理INSTRUCTION_ID类型，从缓存获取指令文本
                int instructionId = originalValue.toInt();
                QString instructionText;

                // 从缓存中获取指令文本
                if (m_instructionCache.contains(instructionId))
                {
                    instructionText = m_instructionCache.value(instructionId);
                }
                else
                {
                    // 缓存中没有，从数据库获取
                    QSqlQuery instructionQuery(db);
                    instructionQuery.prepare("SELECT instruction_value FROM instruction_options WHERE id = ?");
                    instructionQuery.addBindValue(instructionId);

                    if (instructionQuery.exec() && instructionQuery.next())
                    {
                        instructionText = instructionQuery.value(0).toString();
                        // 添加到缓存
                        m_instructionCache[instructionId] = instructionText;
                    }
                    else
                    {
                        instructionText = QString("未知(%1)").arg(instructionId);
                    }
                }

                QTableWidgetItem *newItem = new QTableWidgetItem(instructionText);
                newItem->setData(Qt::UserRole, instructionId);
                tableWidget->setItem(row, visibleColIdx, newItem);
            }
            else if (visibleCol.type == Vector::ColumnDataType::TIMESET_ID)
            {
                // 处理TIMESET_ID类型，从缓存获取TimeSet文本
                int timesetId = originalValue.toInt();
                QString timesetText;

                // 从缓存中获取TimeSet文本
                if (m_timesetCache.contains(timesetId))
                {
                    timesetText = m_timesetCache.value(timesetId);
                }
                else
                {
                    // 缓存中没有，从数据库获取
                    QSqlQuery timesetQuery(db);
                    timesetQuery.prepare("SELECT timeset_name FROM timeset_list WHERE id = ?");
                    timesetQuery.addBindValue(timesetId);

                    if (timesetQuery.exec() && timesetQuery.next())
                    {
                        timesetText = timesetQuery.value(0).toString();
                        // 添加到缓存
                        m_timesetCache[timesetId] = timesetText;
                    }
                    else
                    {
                        timesetText = QString("未知(%1)").arg(timesetId);
                    }
                }

                QTableWidgetItem *newItem = new QTableWidgetItem(timesetText);
                newItem->setData(Qt::UserRole, timesetId);
                tableWidget->setItem(row, visibleColIdx, newItem);
            }
            else if (visibleCol.type == Vector::ColumnDataType::BOOLEAN)
            {
                // 对于布尔值处理 (Capture)
                bool boolValue = originalValue.toBool();
                QString displayText = boolValue ? "Y" : "N";

                QTableWidgetItem *newItem = new QTableWidgetItem(displayText);
                newItem->setData(Qt::UserRole, boolValue);
                tableWidget->setItem(row, visibleColIdx, newItem);
                qDebug() << funcName << " - 行" << row << ", 列" << visibleColIdx
                         << " 设置布尔值为:" << displayText << "(" << boolValue << ")";
            }
            else
            {
                // 其他类型的列，直接创建QTableWidgetItem
                QTableWidgetItem *newItem = new QTableWidgetItem();
                QVariant originalValue = originalRowData[originalColIdx]; // 添加声明

                // 根据具体类型设置文本
                if (visibleCol.type == Vector::ColumnDataType::INTEGER)
                {
                    newItem->setText(originalValue.isValid() ? QString::number(originalValue.toInt()) : "");
                    newItem->setData(Qt::UserRole, originalValue.isValid() ? originalValue.toInt() : 0);
                }
                else if (visibleCol.type == Vector::ColumnDataType::REAL)
                {
                    newItem->setText(originalValue.isValid() ? QString::number(originalValue.toDouble()) : "");
                    newItem->setData(Qt::UserRole, originalValue.isValid() ? originalValue.toDouble() : 0.0);
                }
                else
                {
                    // TEXT 或其他类型，直接转换为字符串
                    newItem->setText(originalValue.toString());
                }

                tableWidget->setItem(row, visibleColIdx, newItem);
                qDebug() << funcName << " - 行" << row << ", 列" << visibleColIdx
                         << " 设置为:" << newItem->text();
            }
        }
    }

    // 显示行号(从startRow+1开始，而不是从1开始)
    for (int i = 0; i < rowsToLoad; i++)
    {
        QTableWidgetItem *rowHeaderItem = new QTableWidgetItem(QString::number(startRow + i + 1));
        tableWidget->setVerticalHeaderItem(i, rowHeaderItem);
    }

    // 在函数结束前恢复更新和信号
    tableWidget->blockSignals(false);

    // 恢复滚动条位置
    if (vScrollBar)
    {
        vScrollBar->setValue(vScrollValue);
    }
    if (hScrollBar)
    {
        hScrollBar->setValue(hScrollValue);
    }

    // 恢复UI更新
    tableWidget->verticalHeader()->setUpdatesEnabled(true);
    tableWidget->horizontalHeader()->setUpdatesEnabled(true);
    tableWidget->setUpdatesEnabled(true);

    // 强制刷新视图
    tableWidget->viewport()->update();

    qDebug() << funcName << " - 分页数据加载完成，共" << pageRows.size() << "行";
    return true;
}

// 实现标记行已修改的方法
void VectorDataHandler::markRowAsModified(int tableId, int rowIndex)
{
    const QString funcName = "VectorDataHandler::markRowAsModified";

    // 添加到修改行集合中
    m_modifiedRows[tableId].insert(rowIndex);
    qDebug() << funcName << " - 已标记表ID:" << tableId << "的行:" << rowIndex << "为已修改，当前修改行数:"
             << m_modifiedRows[tableId].size();
}

// 清除指定表的所有修改标记
void VectorDataHandler::clearModifiedRows(int tableId)
{
    const QString funcName = "VectorDataHandler::clearModifiedRows";

    int clearedCount = 0;
    if (m_modifiedRows.contains(tableId))
    {
        clearedCount = m_modifiedRows[tableId].size();
        m_modifiedRows[tableId].clear();
    }

    qDebug() << funcName << " - 已清除表ID:" << tableId << "的所有修改标记，共" << clearedCount << "行";
}

// 检查行是否被修改过
bool VectorDataHandler::isRowModified(int tableId, int rowIndex)
{
    return m_modifiedRows.contains(tableId) && m_modifiedRows[tableId].contains(rowIndex);
}

// 实现优化版本的保存分页数据函数
bool VectorDataHandler::saveVectorTableDataPaged(int tableId, QTableWidget *currentPageTable,
                                                 int currentPage, int pageSize, int totalRows,
                                                 QString &errorMessage)
{
    const QString funcName = "VectorDataHandler::saveVectorTableDataPaged";

    // 只记录关键日志，减少日志量提升性能
    qDebug() << funcName << " - 开始保存分页数据, 表ID:" << tableId
             << ", 当前页:" << currentPage << ", 页大小:" << pageSize;

    if (!currentPageTable)
    {
        errorMessage = "表控件为空";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 初始化缓存（如果尚未初始化）
    if (!m_cacheInitialized)
    {
        initializeCache();
    }

    // 1. 获取列信息和预期 schema version (从元数据 - 只包含可见列)
    QString binFileName;
    QList<Vector::ColumnInfo> visibleColumns;
    int schemaVersion = 0;
    int rowCount = 0;
    if (!loadVectorTableMeta(tableId, binFileName, visibleColumns, schemaVersion, rowCount))
    {
        errorMessage = "元数据加载失败，无法确定列结构和版本";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 如果没有列配置，则无法保存
    if (visibleColumns.isEmpty())
    {
        errorMessage = QString("表 %1 没有可见的列配置，无法保存数据。").arg(tableId);
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 1.1 加载所有列信息（包括隐藏列）- 这对于正确序列化至关重要
    QList<Vector::ColumnInfo> allColumns;
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery allColQuery(db);
    allColQuery.prepare("SELECT id, column_name, column_order, column_type, data_properties, IsVisible FROM VectorTableColumnConfiguration WHERE master_record_id = ? ORDER BY column_order");
    allColQuery.addBindValue(tableId);
    if (!allColQuery.exec())
    {
        errorMessage = "查询完整列结构失败: " + allColQuery.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    while (allColQuery.next())
    {
        Vector::ColumnInfo col;
        col.id = allColQuery.value(0).toInt();
        col.vector_table_id = tableId;
        col.name = allColQuery.value(1).toString();
        col.order = allColQuery.value(2).toInt();
        col.original_type_str = allColQuery.value(3).toString();
        col.type = Vector::columnDataTypeFromString(col.original_type_str);
        col.is_visible = allColQuery.value(5).toBool();

        QString propStr = allColQuery.value(4).toString();
        if (!propStr.isEmpty())
        {
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(propStr.toUtf8(), &err);
            if (err.error == QJsonParseError::NoError && doc.isObject())
            {
                col.data_properties = doc.object();
            }
        }

        allColumns.append(col);
    }

    // 构建ID到列索引的映射
    QMap<int, int> columnIdToIndexMap;
    for (int i = 0; i < allColumns.size(); ++i)
    {
        columnIdToIndexMap[allColumns[i].id] = i;
    }

    // 2. 解析二进制文件路径
    QString resolveErrorMsg;
    QString absoluteBinFilePath = resolveBinaryFilePath(tableId, resolveErrorMsg);
    if (absoluteBinFilePath.isEmpty())
    {
        errorMessage = "无法解析二进制文件路径: " + resolveErrorMsg;
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 检查目标目录是否存在
    QFileInfo binFileInfo(absoluteBinFilePath);
    QDir binDir = binFileInfo.dir();
    if (!binDir.exists() && !binDir.mkpath("."))
    {
        errorMessage = QString("无法创建目标二进制目录: %1").arg(binDir.absolutePath());
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 3. 建立表格控件列与数据库可见列之间的映射关系
    int tableColCount = currentPageTable->columnCount();
    int visibleDbColCount = visibleColumns.size();

    // 确保表头与数据库列名一致，构建映射关系
    QMap<int, int> tableColToVisibleDbColMap; // 键: 表格列索引, 值: 数据库可见列索引

    for (int tableCol = 0; tableCol < tableColCount; ++tableCol)
    {
        QString tableHeader = currentPageTable->horizontalHeaderItem(tableCol)->text();
        // 对于包含换行符的列名（如管脚列），只取第一行作为管脚名
        QString simplifiedHeader = tableHeader.split("\n").first();

        // 查找匹配的数据库可见列
        bool found = false;
        for (int dbCol = 0; dbCol < visibleDbColCount; ++dbCol)
        {
            // 对于管脚列，只比较管脚名部分
            if (visibleColumns[dbCol].name == simplifiedHeader ||
                (visibleColumns[dbCol].type == Vector::ColumnDataType::PIN_STATE_ID &&
                 tableHeader.startsWith(visibleColumns[dbCol].name + "\n")))
            {
                tableColToVisibleDbColMap[tableCol] = dbCol;
                found = true;
                break;
            }
        }
        if (!found)
        {
            errorMessage = QString("无法找到表格列 '%1' 对应的数据库可见列").arg(tableHeader);
            qWarning() << funcName << " - " << errorMessage;
            return false;
        }
    }

    // 如果映射关系不完整，无法保存
    if (tableColToVisibleDbColMap.size() != tableColCount)
    {
        errorMessage = QString("表格列与数据库可见列映射不完整，无法保存");
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 4. 性能优化: 使用缓存机制避免每次都从文件读取所有行数据
    QList<Vector::RowData> allRows;

    // 检查缓存是否有效
    bool useCachedData = isTableDataCacheValid(tableId, absoluteBinFilePath);

    if (useCachedData)
    {
        // 使用缓存数据
        allRows = m_tableDataCache[tableId];
        qDebug() << funcName << " - 使用表" << tableId << "的缓存数据，包含" << allRows.size() << "行";
    }
    else
    {
        // 缓存无效，从文件读取所有行
        qDebug() << funcName << " - 表" << tableId << "的缓存无效或不存在，从文件读取";

        // 从二进制文件读取所有行
        if (!readAllRowsFromBinary(absoluteBinFilePath, allColumns, schemaVersion, allRows))
        {
            // 如果读取失败，可能是文件不存在或格式有问题，尝试创建新数据
            qWarning() << funcName << " - 读取现有二进制数据失败，将创建新数据";
            allRows.clear();

            // 确保行数量足够
            allRows.reserve(totalRows);
            for (int row = 0; row < totalRows; ++row)
            {
                Vector::RowData rowData;
                // rowData.resize(allColumns.size()); // QT6写法
                int targetSize4 = allColumns.size(); // QT5写法开始
                while (rowData.size() < targetSize4) {
                    rowData.append(QVariant());
                }
                // QT5写法结束

                // 设置所有列的默认值
                for (int colIdx = 0; colIdx < allColumns.size(); ++colIdx)
                {
                    const auto &col = allColumns[colIdx];
                    if (col.type == Vector::ColumnDataType::PIN_STATE_ID)
                    {
                        rowData[colIdx] = "X"; // 管脚列默认为X
                    }
                    else
                    {
                        rowData[colIdx] = QVariant(); // 其他列使用默认空值
                    }
                }

                allRows.append(rowData);
            }
        }

        // 更新缓存
        updateTableDataCache(tableId, allRows, absoluteBinFilePath);
    }

    // 确保数据行数与totalRows匹配
    if (allRows.size() < totalRows)
    {
        int additionalRows = totalRows - allRows.size();

        for (int i = 0; i < additionalRows; ++i)
        {
            Vector::RowData rowData;
            // rowData.resize(allColumns.size()); // QT6写法
            int targetSize5 = allColumns.size(); // QT5写法开始
            while (rowData.size() < targetSize5) {
                rowData.append(QVariant());
            }
            // QT5写法结束

            // 设置默认值
            for (int colIdx = 0; colIdx < allColumns.size(); ++colIdx)
            {
                const auto &col = allColumns[colIdx];
                if (col.type == Vector::ColumnDataType::PIN_STATE_ID)
                {
                    rowData[colIdx] = "X";
                }
                else
                {
                    rowData[colIdx] = QVariant();
                }
            }

            allRows.append(rowData);
        }

        // 如果添加了行，更新缓存
        if (additionalRows > 0)
        {
            updateTableDataCache(tableId, allRows, absoluteBinFilePath);
        }
    }

    // 预先获取指令和TimeSet的ID映射以便转换
    QMap<QString, int> instructionNameToIdMap;
    QMap<QString, int> timesetNameToIdMap;

    // 获取指令名称到ID的映射
    QSqlQuery instructionQuery(db);
    if (instructionQuery.exec("SELECT id, instruction_value FROM instruction_options"))
    {
        while (instructionQuery.next())
        {
            int id = instructionQuery.value(0).toInt();
            QString name = instructionQuery.value(1).toString();
            instructionNameToIdMap[name] = id;

            // 更新缓存
            if (!m_instructionCache.contains(id) || m_instructionCache[id] != name)
            {
                m_instructionCache[id] = name;
            }
        }
    }

    // 获取TimeSet名称到ID的映射
    QSqlQuery timesetQuery(db);
    if (timesetQuery.exec("SELECT id, timeset_name FROM timeset_list"))
    {
        while (timesetQuery.next())
        {
            int id = timesetQuery.value(0).toInt();
            QString name = timesetQuery.value(1).toString();
            timesetNameToIdMap[name] = id;

            // 更新缓存
            if (!m_timesetCache.contains(id) || m_timesetCache[id] != name)
            {
                m_timesetCache[id] = name;
            }
        }
    }

    // 5. 更新当前页的数据到内存中，而不创建临时表格
    int startRowIndex = currentPage * pageSize;
    int rowsInCurrentPage = currentPageTable->rowCount();
    int modifiedCount = 0; // 记录修改的行数

    // 确保不会越界
    if (startRowIndex >= allRows.size())
    {
        errorMessage = QString("当前页起始行(%1)超出总行数(%2)").arg(startRowIndex).arg(allRows.size());
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 更新当前页的行数据到allRows
    for (int rowInPage = 0; rowInPage < rowsInCurrentPage; ++rowInPage)
    {
        int rowInFullData = startRowIndex + rowInPage;
        if (rowInFullData >= allRows.size())
        {
            break; // 防止越界
        }

        bool rowModified = false; // 标记此行是否有修改

        // 为行创建一份新数据
        Vector::RowData &rowData = allRows[rowInFullData];

        // 从当前页表格更新数据
        for (int tableCol = 0; tableCol < tableColCount; ++tableCol)
        {
            int visibleDbCol = tableColToVisibleDbColMap[tableCol];
            const auto &visibleColumn = visibleColumns[visibleDbCol];

            // 找到此可见列对应的原始列索引
            if (!columnIdToIndexMap.contains(visibleColumn.id))
            {
                continue; // 跳过找不到的列
            }

            int originalColIdx = columnIdToIndexMap[visibleColumn.id];
            QVariant oldValue = rowData[originalColIdx]; // 保存旧值用于比较

            // 处理单元格内容 - 特殊处理管脚列（单元格控件）
            if (visibleColumn.type == Vector::ColumnDataType::PIN_STATE_ID)
            {
                // 对于管脚列，优先从单元格控件获取值
                PinValueLineEdit *pinEdit = qobject_cast<PinValueLineEdit *>(currentPageTable->cellWidget(rowInPage, tableCol));
                if (pinEdit)
                {
                    QString pinStateText = pinEdit->text();
                    if (pinStateText.isEmpty())
                    {
                        pinStateText = "X"; // 默认值
                    }

                    // 检查数据是否有变更
                    if (oldValue.toString() != pinStateText)
                    {
                    rowData[originalColIdx] = pinStateText;
                        rowModified = true;
                    }

                    continue; // 已处理，跳过下面的处理
                }

                // 如果没有找到编辑控件，尝试从QTableWidgetItem获取值
                QTableWidgetItem *item = currentPageTable->item(rowInPage, tableCol);
                if (item && !item->text().isEmpty())
                {
                    QString pinStateText = item->text();

                    // 检查数据是否有变更
                    if (oldValue.toString() != pinStateText)
                    {
                        rowData[originalColIdx] = pinStateText;
                        rowModified = true;
                    }

                    continue;
                }
            }
            else
            {
                // 非管脚列
                QTableWidgetItem *item = currentPageTable->item(rowInPage, tableCol);
                if (!item)
                {
                    continue; // 不更新空单元格
                }

                // 获取单元格文本
                QString cellText = item->text();
                QVariant newValue;

                // 根据列类型处理不同格式的数据
                if (visibleColumn.type == Vector::ColumnDataType::INSTRUCTION_ID)
                {
                    // 将指令名称转换为ID存储
                    if (instructionNameToIdMap.contains(cellText))
                    {
                        int instructionId = instructionNameToIdMap[cellText];
                        newValue = instructionId;
                    }
                    else
                    {
                        newValue = -1; // 默认未知ID
                    }
                }
                else if (visibleColumn.type == Vector::ColumnDataType::TIMESET_ID)
                {
                    // 将TimeSet名称转换为ID存储
                    if (timesetNameToIdMap.contains(cellText))
                    {
                        int timesetId = timesetNameToIdMap[cellText];
                        newValue = timesetId;
                    }
                    else
                    {
                        newValue = -1; // 默认未知ID
                    }
                }
                else if (visibleColumn.type == Vector::ColumnDataType::BOOLEAN)
                {
                    // 布尔值处理
                    newValue = (cellText.toUpper() == "Y");
                }
                else
                {
                    // 普通文本或数字值
                    newValue = cellText;
                }

                // 检查数据是否有变更
                if (oldValue != newValue)
                {
                    rowData[originalColIdx] = newValue;
                    rowModified = true;
                }
            }
        }

        if (rowModified)
        {
            modifiedCount++;
            // 标记行为已修改
            markRowAsModified(tableId, rowInFullData);
        }
    }

    // 如果没有检测到任何数据变更，可以直接返回成功
    if (modifiedCount == 0 && m_modifiedRows.value(tableId, QSet<int>()).isEmpty()) // 确保同时检查modifiedCount和m_modifiedRows
    {
        errorMessage = "没有检测到数据变更 (modifiedCount is 0 and m_modifiedRows is empty for tableId)，跳过保存";
        qInfo() << funcName << " - " << errorMessage; // 改为qInfo以便区分
        return true;
    }
    qDebug() << funcName << " - 检测到 modifiedCount:" << modifiedCount << "，或 m_modifiedRows 非空。准备保存。";

    // 6. 写入二进制文件 - 使用优化的方式
    qDebug() << funcName << " - 准备写入数据到二进制文件，报告的 modifiedCount:" << modifiedCount;

    QMap<int, Vector::RowData> modifiedRowsMap;
    if (m_modifiedRows.contains(tableId))
    {
        const QSet<int> &rowsActuallyMarked = m_modifiedRows.value(tableId);
        qDebug() << funcName << " - m_modifiedRows for table" << tableId << "contains" << rowsActuallyMarked.size() << "indices:" << rowsActuallyMarked;
        for (int rowIndex : rowsActuallyMarked)
        {
            if (rowIndex >= 0 && rowIndex < allRows.size())
            {
                modifiedRowsMap[rowIndex] = allRows[rowIndex];
            }
            else
            {
                qWarning() << funcName << " - Invalid rowIndex" << rowIndex << "found in m_modifiedRows. Skipping.";
            }
        }
    }
    else
    {
        qDebug() << funcName << " - m_modifiedRows does not contain tableId" << tableId;
    }

    // 备用逻辑检查：如果UI报告有修改，但内部标记的修改行集合为空或未填充到map
    if (modifiedRowsMap.isEmpty() && modifiedCount > 0)
    {
        qWarning() << funcName << " - WARNING: modifiedRowsMap is empty BUT modifiedCount is" << modifiedCount
                   << ". This might indicate an issue with how m_modifiedRows is populated or cleared."
                   << "Falling back to considering all rows in the current page as modified for the map.";
        // 重新从当前页构建（这通常不应该发生如果 markRowAsModified 正常工作）
        for (int rowInPage = 0; rowInPage < rowsInCurrentPage; ++rowInPage)
        {
            int rowInFullData = startRowIndex + rowInPage;
            if (rowInFullData >= 0 && rowInFullData < allRows.size())
            {
                // 只添加当前页内且在 allRows 范围内的行
                if (isRowModified(tableId, rowInFullData))
                { // 再次检查是否真的被标记为修改
                    modifiedRowsMap[rowInFullData] = allRows[rowInFullData];
                }
            }
        }
        qDebug() << funcName << " - After fallback population from current page, modifiedRowsMap.size() is now:" << modifiedRowsMap.size();
    }

    qDebug() << funcName << " - Final check before deciding write strategy: modifiedCount =" << modifiedCount
             << ", modifiedRowsMap.size() =" << modifiedRowsMap.size();

    bool writeSuccess = false;
    if (modifiedCount > 0)
    { // 主要条件：确实有修改发生
        if (!modifiedRowsMap.isEmpty())
        {
            qInfo() << funcName << " - Attempting INCREMENTAL update with" << modifiedRowsMap.size() << "rows in modifiedRowsMap.";
            // 使用新的健壮版本的增量更新函数
            writeSuccess = Persistence::BinaryFileHelper::robustUpdateRowsInBinary(absoluteBinFilePath, allColumns, schemaVersion, modifiedRowsMap);

            if (!writeSuccess)
            {
                qWarning() << funcName << " - ROBUST INCREMENTAL update FAILED. Attempting FULL rewrite.";
                // 在这里，我们期望 robustUpdateRowsInBinary 内部已经打印了详细日志指明具体失败原因
                writeSuccess = Persistence::BinaryFileHelper::writeAllRowsToBinary(absoluteBinFilePath, allColumns, schemaVersion, allRows);
                if (writeSuccess)
                {
                    qInfo() << funcName << " - FULL rewrite successful after robust incremental update failed.";
                }
                else
                {
                    qCritical() << funcName << " - CRITICAL: FULL rewrite ALSO FAILED after robust incremental update failed. Data NOT saved for file:" << absoluteBinFilePath;
                }
            }
            else
            {
                qInfo() << funcName << " - ROBUST INCREMENTAL update successful.";
            }
        }
        else // modifiedRowsMap is empty, but modifiedCount > 0
        {
            qWarning() << funcName << " - modifiedRowsMap is EMPTY, but modifiedCount is" << modifiedCount
                       << ". This is unexpected if changes were properly tracked."
                       << "Proceeding with FULL rewrite to ensure data integrity.";
            writeSuccess = Persistence::BinaryFileHelper::writeAllRowsToBinary(absoluteBinFilePath, allColumns, schemaVersion, allRows);
            if (writeSuccess)
            {
                qInfo() << funcName << " - FULL rewrite successful (due to empty modifiedRowsMap but positive modifiedCount).";
            }
            else
            {
                qCritical() << funcName << " - CRITICAL: FULL rewrite FAILED (due to empty modifiedRowsMap but positive modifiedCount). Data NOT saved for file:" << absoluteBinFilePath;
            }
        }
    }
    else
    { // modifiedCount == 0 (and m_modifiedRows was also empty from earlier check)
        qInfo() << funcName << " - No modifications detected (modifiedCount is 0), no save operation performed.";
        //  errorMessage = "没有检测到数据变更，跳过保存"; // Set earlier
        return true; // No changes, so considered a "successful" save of no changes.
    }

    if (!writeSuccess)
    {
        errorMessage = QString("写入二进制文件失败 (final check): %1").arg(absoluteBinFilePath);
        // qWarning 已经被上面的逻辑覆盖，这里可以不用重复，除非上面的逻辑有遗漏
        // qWarning() << funcName << " - " << errorMessage; // Redundant if qCritical used above for write failures
        return false;
    }
    else
    {
        qInfo() << funcName << " - Data saved successfully to" << absoluteBinFilePath << "(Cache will be updated).";
        updateTableDataCache(tableId, allRows, absoluteBinFilePath); // 更新缓存
    }

    // 7. 更新数据库中的行数和时间戳记录 (这部分逻辑保持不变)
    if (!db.transaction())
    {
        errorMessage = "无法开始数据库事务以更新主记录。";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 计算最终的行数
    int finalRowCount = allRows.size();

    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE VectorTableMasterRecord SET row_count = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?");
    updateQuery.addBindValue(finalRowCount);
    updateQuery.addBindValue(tableId);

    if (!updateQuery.exec())
    {
        errorMessage = QString("更新数据库中的行数记录失败: %1").arg(updateQuery.lastError().text());
        qWarning() << funcName << " - " << errorMessage;
        db.rollback();
        return false;
    }

    if (!db.commit())
    {
        errorMessage = "提交数据库事务失败。";
        qWarning() << funcName << " - " << errorMessage;
        db.rollback();
        return false;
    }

    // 清除当前表的修改标记
    clearModifiedRows(tableId);

    errorMessage = QString("已成功保存数据，更新了 %1 行").arg(modifiedCount);
    qDebug() << funcName << " - 保存成功，修改行数:" << modifiedCount << ", 总行数:" << finalRowCount;

    return true;
}

void VectorDataHandler::clearTableDataCache(int tableId)
{
    const QString funcName = "VectorDataHandler::clearTableDataCache";

    if (m_tableDataCache.contains(tableId))
    {
        qDebug() << funcName << " - 清除表" << tableId << "的数据缓存";
        m_tableDataCache.remove(tableId);
        m_tableCacheTimestamp.remove(tableId);
        m_tableBinaryFilePath.remove(tableId);
        m_tableBinaryFileMD5.remove(tableId);
    }
}

void VectorDataHandler::clearAllTableDataCache()
{
    const QString funcName = "VectorDataHandler::clearAllTableDataCache";
    qDebug() << funcName << " - 清除所有表数据缓存";

    m_tableDataCache.clear();
    m_tableCacheTimestamp.clear();
    m_tableBinaryFilePath.clear();
    m_tableBinaryFileMD5.clear();
}

// 检查表数据缓存是否有效
bool VectorDataHandler::isTableDataCacheValid(int tableId, const QString &binFilePath)
{
    const QString funcName = "VectorDataHandler::isTableDataCacheValid";

    // 检查是否有缓存
    if (!m_tableDataCache.contains(tableId))
    {
        qDebug() << funcName << " - 表" << tableId << "没有缓存数据";
        return false;
    }

    // 检查文件路径是否匹配
    if (m_tableBinaryFilePath.value(tableId) != binFilePath)
    {
        qDebug() << funcName << " - 表" << tableId << "的文件路径已变更";
        return false;
    }

    // 检查文件是否存在
    QFile file(binFilePath);
    if (!file.exists())
    {
        qDebug() << funcName << " - 表" << tableId << "的二进制文件不存在";
        return false;
    }

    // 检查文件修改时间是否晚于缓存时间
    QFileInfo fileInfo(binFilePath);
    QDateTime fileModTime = fileInfo.lastModified();
    QDateTime cacheTime = m_tableCacheTimestamp.value(tableId);

    if (cacheTime.isValid() && fileModTime > cacheTime)
    {
        qDebug() << funcName << " - 表" << tableId << "的文件已被修改，缓存已过期";
        return false;
    }

    // 可选：检查MD5校验和
    if (m_tableBinaryFileMD5.contains(tableId))
    {
        if (file.open(QIODevice::ReadOnly))
        {
            QCryptographicHash hash(QCryptographicHash::Md5);
            if (hash.addData(&file))
            {
                QByteArray fileHash = hash.result();
                if (fileHash != m_tableBinaryFileMD5.value(tableId))
                {
                    qDebug() << funcName << " - 表" << tableId << "的文件内容已变更，MD5不匹配";
                    file.close();
                    return false;
                }
            }
            file.close();
        }
    }

    qDebug() << funcName << " - 表" << tableId << "的缓存有效";
    return true;
}

// 更新表数据缓存
void VectorDataHandler::updateTableDataCache(int tableId, const QList<Vector::RowData> &rows, const QString &binFilePath)
{
    const QString funcName = "VectorDataHandler::updateTableDataCache";

    // 更新缓存数据
    m_tableDataCache[tableId] = rows;

    // 更新时间戳
    m_tableCacheTimestamp[tableId] = QDateTime::currentDateTime();

    // 更新文件路径
    m_tableBinaryFilePath[tableId] = binFilePath;

    // 可选：更新MD5校验和
    QFile file(binFilePath);
    if (file.open(QIODevice::ReadOnly))
    {
        QCryptographicHash hash(QCryptographicHash::Md5);
        if (hash.addData(&file))
        {
            m_tableBinaryFileMD5[tableId] = hash.result();
        }
        file.close();
    }

    qDebug() << funcName << " - 已更新表" << tableId << "的缓存，" << rows.size() << "行数据";
}