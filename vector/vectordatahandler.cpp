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
#include <QSet>
#include <algorithm>

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
            else
            {
                // 其他类型的列，直接创建QTableWidgetItem
                QTableWidgetItem *newItem = new QTableWidgetItem();

                // 根据具体类型设置文本
                if (visibleCol.type == Vector::ColumnDataType::INTEGER ||
                    visibleCol.type == Vector::ColumnDataType::TIMESET_ID ||
                    visibleCol.type == Vector::ColumnDataType::INSTRUCTION_ID)
                {
                    newItem->setText(originalValue.isValid() ? QString::number(originalValue.toInt()) : "");
                    newItem->setData(Qt::UserRole, originalValue.isValid() ? originalValue.toInt() : 0);
                }
                else if (visibleCol.type == Vector::ColumnDataType::REAL)
                {
                    newItem->setText(originalValue.isValid() ? QString::number(originalValue.toDouble()) : "");
                    newItem->setData(Qt::UserRole, originalValue.isValid() ? originalValue.toDouble() : 0.0);
                }
                else if (visibleCol.type == Vector::ColumnDataType::BOOLEAN)
                {
                    newItem->setText(originalValue.toBool() ? "是" : "否");
                    newItem->setData(Qt::UserRole, originalValue.toBool());
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

VectorDataHandler::VectorDataHandler() : m_cancelRequested(0)
{
    // 构造函数不变
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
                 << ", 顺序=" << col.order << ", 可见=" << col.is_visible;
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

    allRows.reserve(tableRowCount);
    for (int row = 0; row < tableRowCount; ++row)
    {
        // 为每一行创建包含所有数据库列的数据（包括隐藏列）
        Vector::RowData rowData;
        rowData.resize(allColumns.size());

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

            // 根据列类型获取值
            QVariant value;

            // 如果是管脚列，需要从PinValueLineEdit控件获取值
            if (visibleColumn.type == Vector::ColumnDataType::PIN_STATE_ID)
            {
                PinValueLineEdit *pinEdit = qobject_cast<PinValueLineEdit *>(tableWidget->cellWidget(row, tableCol));
                if (pinEdit)
                {
                    value = pinEdit->text();
                    qDebug() << funcName << " - 从管脚控件读取值:" << value;
                }
                else
                {
                    // 如果没有找到控件（罕见情况），尝试从QTableWidgetItem获取
                    QTableWidgetItem *item = tableWidget->item(row, tableCol);
                    value = item ? item->data(Qt::DisplayRole) : "X";
                    qDebug() << funcName << " - 未找到管脚控件，从Item读取值:" << value;
                }

                // 如果值为空或无效，使用默认值X
                if (value.isNull() || !value.isValid() || value.toString().isEmpty())
                {
                    value = "X";
                }
            }
            else
            {
                // 其他类型的列，从QTableWidgetItem获取值
                QTableWidgetItem *item = tableWidget->item(row, tableCol);
                value = item ? item->data(Qt::DisplayRole) : QVariant();
            }

            // 将值放入原始列索引的位置，确保数据正确放置
            rowData[originalColIdx] = value;
            qDebug() << funcName << " - 行" << row << ", 表格列" << tableCol
                     << " -> 原始列索引" << originalColIdx << " (列名:" << allColumns[originalColIdx].name
                     << "), 值:" << value;
        }

        allRows.append(rowData);
    }
    qDebug() << funcName << " - 收集了 " << allRows.size() << " 行数据进行保存";

    // 5. 写入二进制文件 - 使用allColumns而不是visibleColumns
    qDebug() << funcName << " - 准备写入数据到二进制文件: " << absoluteBinFilePath;
    if (!Persistence::BinaryFileHelper::writeAllRowsToBinary(absoluteBinFilePath, allColumns, schemaVersion, allRows))
    {
        errorMessage = QString("写入二进制文件失败: %1").arg(absoluteBinFilePath);
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }
    qDebug() << funcName << " - 二进制文件写入成功";

    // 6. 更新数据库中的行数记录
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

    qDebug() << funcName << "- 开始插入向量行，表ID:" << tableId
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
    if (QFile::exists(absoluteBinFilePath))
    {
        qDebug() << funcName << "- 二进制文件存在，尝试加载现有数据:" << absoluteBinFilePath;

        // 如果是追加模式且文件已存在，不需要加载所有现有数据，只需要获取总行数
        if (appendToEnd)
        {
            // 只读取文件头获取行数
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
            // 非追加模式需要加载所有数据
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

    // 创建按列名检索的映射，提前准备好提高效率
    QMap<QString, int> columnNameMap;
    for (int i = 0; i < columns.size(); ++i)
    {
        columnNameMap[columns[i].name] = i;
        qDebug() << funcName << " - 列映射: 名称=" << columns[i].name << "，索引=" << i << "，类型=" << columns[i].original_type_str;
    }

    // 5. 批量处理数据写入，避免一次性生成大量数据占用内存
    const int BATCH_SIZE = 10000; // 每批处理的行数，可根据内存情况调整

    // 对于超过100万行的大数据，做特殊的进度上报处理
    bool isLargeDataset = (rowCount > 1000000);
    int progressStart = 25;
    int progressEnd = isLargeDataset ? 95 : 90; // 大数据集预留更多进度给文件写入操作

    // 计算总批次数
    int totalBatches = (rowCount + BATCH_SIZE - 1) / BATCH_SIZE; // 向上取整
    int currentBatch = 0;
    int totalRowsProcessed = 0;

    qDebug() << funcName << "- 开始批量处理，总批次:" << totalBatches << "，每批大小:" << BATCH_SIZE;

    // 创建已经处理的行数记录，用于断点续传
    QFile binaryFile(absoluteBinFilePath);
    bool fileOpenSuccess = false;
    QDataStream out; // 声明流对象但不立即初始化

    // 如果是追加模式，直接打开文件进行追加
    if (appendToEnd && existingRowCountFromMeta > 0)
    {
        fileOpenSuccess = binaryFile.open(QIODevice::ReadWrite);
        if (fileOpenSuccess)
        {
            binaryFile.seek(binaryFile.size()); // 移动到文件末尾
            out.setDevice(&binaryFile);
            out.setByteOrder(QDataStream::LittleEndian);
        }
        else
        {
            qWarning() << funcName << "- 追加模式下无法打开文件:" << binaryFile.errorString();
        }
    }

    // 预先创建一个模板行，存储标准列的默认值，减少重复计算
    Vector::RowData templateRow;
    templateRow.resize(columns.size());

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

    // 批量生成和保存数据
    while (totalRowsProcessed < rowCount && !m_cancelRequested.loadAcquire())
    {
        // 计算当前批次要处理的行数
        int currentBatchSize = qMin(BATCH_SIZE, rowCount - totalRowsProcessed);
        qDebug() << funcName << "- 处理批次 " << (currentBatch + 1) << "/" << totalBatches
                 << "，当前批次大小:" << currentBatchSize;

        // 为当前批次创建数据行
        QList<Vector::RowData> batchRows;
        batchRows.reserve(currentBatchSize);

        // 生成当前批次的数据
        for (int i = 0; i < currentBatchSize; ++i)
        {
            // 计算当前行在源数据表中的索引
            int srcRowIdx = (totalRowsProcessed + i) % sourceDataRowCount;

            // 使用模板行创建新行，减少重复初始化
            Vector::RowData newRow = templateRow;

            // 设置管脚列的值（从缓存中读取）
            for (int pinColIdx = 0; pinColIdx < selectedPins.size(); ++pinColIdx)
            {
                const auto &pinSelection = selectedPins[pinColIdx];
                const QString &pinName = pinSelection.second.first;
                if (!columnNameMap.contains(pinName))
                    continue;

                int targetColIdx = columnNameMap[pinName];
                newRow[targetColIdx] = pinDataCache[srcRowIdx][pinColIdx];
            }

            batchRows.append(newRow);
        }

        // 处理写入文件
        // 如果是第一批且非追加模式，或者文件还未打开，则重新创建文件
        if ((currentBatch == 0 && !appendToEnd) || !fileOpenSuccess)
        {
            // 关闭之前可能打开的文件
            if (binaryFile.isOpen())
                binaryFile.close();

            // 如果是第一批且非追加模式，则重新创建文件
            if (currentBatch == 0 && !appendToEnd)
            {
                // 处理非追加模式下的文件写入（合并现有数据与新数据）
                QList<Vector::RowData> combinedRows;

                // 如果需要保留文件前部分（在插入点之前的数据）
                if (actualInsertionIndex > 0 && !existingRows.isEmpty())
                {
                    for (int i = 0; i < actualInsertionIndex && i < existingRows.size(); i++)
                    {
                        combinedRows.append(existingRows[i]);
                    }
                }

                // 添加当前批次的新数据
                combinedRows.append(batchRows);

                // 如果有剩余的现有数据需要保留（非追加模式下插入点之后的数据）
                if (actualInsertionIndex < existingRows.size() && !appendToEnd)
                {
                    for (int i = actualInsertionIndex; i < existingRows.size(); i++)
                    {
                        combinedRows.append(existingRows[i]);
                    }
                }

                // 写入组合后的数据
                if (!Persistence::BinaryFileHelper::writeAllRowsToBinary(absoluteBinFilePath, columns, schemaVersion, combinedRows))
                {
                    errorMessage = QString("写入二进制文件失败: %1").arg(absoluteBinFilePath);
                    qWarning() << funcName << "-" << errorMessage;
                    emit progressUpdated(100); // End with error
                    return false;
                }

                // 已经写入了第一批数据，现在打开文件进行追加模式写入后续批次
                fileOpenSuccess = binaryFile.open(QIODevice::ReadWrite);
                if (fileOpenSuccess)
                {
                    binaryFile.seek(binaryFile.size());
                    out.setDevice(&binaryFile);
                    out.setByteOrder(QDataStream::LittleEndian);
                }
            }
            else
            {
                // 追加模式但文件未打开的情况
                if (!Persistence::BinaryFileHelper::writeAllRowsToBinary(absoluteBinFilePath, columns, schemaVersion, batchRows))
                {
                    errorMessage = QString("写入二进制文件失败: %1").arg(absoluteBinFilePath);
                    qWarning() << funcName << "-" << errorMessage;
                    emit progressUpdated(100); // End with error
                    return false;
                }

                // 已经写入了第一批数据，现在打开文件进行追加模式写入后续批次
                fileOpenSuccess = binaryFile.open(QIODevice::ReadWrite);
                if (fileOpenSuccess)
                {
                    binaryFile.seek(binaryFile.size());
                    out.setDevice(&binaryFile);
                    out.setByteOrder(QDataStream::LittleEndian);
                }
            }
        }
        else
        {
            // 已经打开文件进行追加操作，直接序列化并写入每一行
            for (const Vector::RowData &row : batchRows)
            {
                QByteArray serializedRowData;
                if (!Persistence::BinaryFileHelper::serializeRow(row, columns, serializedRowData))
                {
                    errorMessage = QString("序列化行数据失败");
                    if (binaryFile.isOpen())
                        binaryFile.close();
                    emit progressUpdated(100);
                    return false;
                }

                // 写入行大小和数据
                quint32 rowBlockSize = static_cast<quint32>(serializedRowData.size());
                out << rowBlockSize;
                if (out.status() != QDataStream::Ok)
                {
                    errorMessage = QString("写入行大小失败");
                    if (binaryFile.isOpen())
                        binaryFile.close();
                    emit progressUpdated(100);
                    return false;
                }

                qint64 bytesWritten = out.writeRawData(serializedRowData.constData(), rowBlockSize);
                if (bytesWritten != rowBlockSize || out.status() != QDataStream::Ok)
                {
                    errorMessage = QString("写入行数据失败");
                    if (binaryFile.isOpen())
                        binaryFile.close();
                    emit progressUpdated(100);
                    return false;
                }
            }

            // 定期刷新文件，避免缓冲区溢出
            if (currentBatch % 10 == 0)
            {
                binaryFile.flush();
            }
        }

        // 更新处理进度
        totalRowsProcessed += currentBatchSize;
        currentBatch++;

        // 更新进度条（非线性进度，大数据进度增长更慢）
        double progress = progressStart + ((double)totalRowsProcessed / rowCount) * (progressEnd - progressStart);
        emit progressUpdated(static_cast<int>(progress));

        // 每批次处理完后，主动让出CPU时间，减轻UI冻结
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    // 关闭打开的文件
    if (binaryFile.isOpen())
    {
        binaryFile.flush();
        binaryFile.close();
    }

    // 如果是追加模式或批量处理，更新二进制文件头中的行数信息
    if (appendToEnd || currentBatch > 1)
    {
        qDebug() << funcName << "- 更新二进制文件头中的行数信息";
        QFile headerUpdateFile(absoluteBinFilePath);
        if (headerUpdateFile.open(QIODevice::ReadWrite))
        {
            BinaryFileHeader header;
            if (Persistence::BinaryFileHelper::readBinaryHeader(&headerUpdateFile, header))
            {
                // 计算最终的行数
                int finalRowCount = appendToEnd ? (existingRowCountFromMeta + rowCount) : (!existingRows.isEmpty() ? (existingRows.size() + rowCount - (existingRows.size() - actualInsertionIndex)) : rowCount);

                // 更新文件头
                header.row_count_in_file = finalRowCount;
                header.timestamp_updated = static_cast<uint64_t>(QDateTime::currentSecsSinceEpoch());

                // 重置文件指针到开头
                headerUpdateFile.seek(0);

                // 写入更新后的文件头
                if (!Persistence::BinaryFileHelper::writeBinaryHeader(&headerUpdateFile, header))
                {
                    qWarning() << funcName << "- 警告: 无法更新二进制文件头中的行数信息";
                }
                else
                {
                    qDebug() << funcName << "- 成功更新二进制文件头中的行数为:" << finalRowCount;
                }
            }
            headerUpdateFile.close();
        }
        else
        {
            qWarning() << funcName << "- 警告: 无法打开文件更新文件头:" << headerUpdateFile.errorString();
        }
    }

    // 如果用户取消了操作
    if (m_cancelRequested.loadAcquire())
    {
        qDebug() << funcName << "- 操作被用户取消。";
        errorMessage = "操作被用户取消。";
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
    int finalRowCount = appendToEnd ? (existingRowCountFromMeta + rowCount) : (!existingRows.isEmpty() ? (existingRows.size() + rowCount - (existingRows.size() - actualInsertionIndex)) : rowCount);

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