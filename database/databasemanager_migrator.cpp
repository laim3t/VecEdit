#include "databasemanager.h"
#include "binaryfilehelper.h"
#include "common/binary_file_format.h"
#include "vector/vector_data_types.h"

#include <QDataStream>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

bool DatabaseManager::migrateVectorDataToBinaryFiles()
{
    const QString funcName = "DatabaseManager::migrateVectorDataToBinaryFiles";
    qInfo() << funcName << " - 开始将向量表数据从SQLite迁移到二进制文件...";

    if (!m_db.isOpen())
    {
        m_lastError = "数据库未打开，无法执行数据迁移";
        qCritical() << funcName << " - " << m_lastError;
        return false;
    }

    // 1. 查询所有已映射的向量表
    QSqlQuery queryMappedTables(m_db);
    if (!queryMappedTables.exec("SELECT id, original_vector_table_id, binary_data_filename FROM VectorTableMasterRecord WHERE original_vector_table_id IS NOT NULL"))
    {
        m_lastError = "查询映射表失败: " + queryMappedTables.lastError().text();
        qCritical() << funcName << " - " << m_lastError;
        return false;
    }

    QSqlDatabase::database().transaction();
    bool allSuccess = true;

    while (queryMappedTables.next() && allSuccess)
    {
        int newTableId = queryMappedTables.value(0).toInt();
        int oldTableId = queryMappedTables.value(1).toInt();
        QString binaryFileName = queryMappedTables.value(2).toString();

        qInfo() << funcName << " - 处理表迁移: 新ID=" << newTableId << ", 旧ID=" << oldTableId << ", 二进制文件=" << binaryFileName;

        // 文件路径处理
        // 如果是相对路径，则需要将其解析为绝对路径
        QString absoluteBinaryFilePath = binaryFileName;
        if (QDir::isRelativePath(binaryFileName))
        {
            QString dbFilePath = m_dbFilePath;
            QFileInfo dbFileInfo(dbFilePath);
            QString dbDirPath = dbFileInfo.absolutePath();

            absoluteBinaryFilePath = QDir(dbDirPath).absoluteFilePath(binaryFileName);
            qDebug() << funcName << " - 解析相对路径: " << binaryFileName << " -> " << absoluteBinaryFilePath;
        }

        // 确保目录存在
        QFileInfo fileInfo(absoluteBinaryFilePath);
        QDir dir = fileInfo.dir();
        if (!dir.exists())
        {
            qDebug() << funcName << " - 创建目录: " << dir.absolutePath();
            if (!dir.mkpath("."))
            {
                m_lastError = "无法创建二进制文件目录: " + dir.absolutePath();
                qCritical() << funcName << " - " << m_lastError;
                allSuccess = false;
                break;
            }
        }

        // 2. 获取列配置
        QList<Vector::ColumnInfo> columns;
        QSqlQuery queryColumns(m_db);
        queryColumns.prepare("SELECT id, column_name, column_order, column_type, data_properties FROM VectorTableColumnConfiguration WHERE master_record_id = ? ORDER BY column_order");
        queryColumns.addBindValue(newTableId);

        if (!queryColumns.exec())
        {
            m_lastError = "查询列配置失败: " + queryColumns.lastError().text();
            qCritical() << funcName << " - " << m_lastError;
            allSuccess = false;
            break;
        }

        while (queryColumns.next())
        {
            Vector::ColumnInfo col;
            col.id = queryColumns.value(0).toInt();
            col.vector_table_id = newTableId;
            col.name = queryColumns.value(1).toString();
            col.order = queryColumns.value(2).toInt();
            col.original_type_str = queryColumns.value(3).toString();
            col.type = Vector::columnDataTypeFromString(col.original_type_str);

            QString dataPropertiesStr = queryColumns.value(4).toString();
            if (!dataPropertiesStr.isEmpty())
            {
                QJsonDocument doc = QJsonDocument::fromJson(dataPropertiesStr.toUtf8());
                col.data_properties = doc.object();
            }

            columns.append(col);
        }

        if (columns.isEmpty())
        {
            qWarning() << funcName << " - 表 " << newTableId << " 没有列配置，跳过迁移";
            continue;
        }

        // 3. 从旧表读取数据
        QList<Vector::RowData> allRows;

        // 3.1 读取旧的vector_table_data表的基本行数据
        QSqlQuery queryOldData(m_db);
        queryOldData.prepare("SELECT id, label, instruction_id, timeset_id, capture, ext, comment, sort_index FROM vector_table_data WHERE table_id = ? ORDER BY sort_index");
        queryOldData.addBindValue(oldTableId);

        if (!queryOldData.exec())
        {
            m_lastError = "查询旧表数据失败: " + queryOldData.lastError().text();
            qCritical() << funcName << " - " << m_lastError;
            allSuccess = false;
            break;
        }

        QMap<int, int> oldRowIdToIndex; // 映射旧行ID到新行索引
        int rowCounter = 0;

        while (queryOldData.next())
        {
            int oldRowId = queryOldData.value(0).toInt();
            oldRowIdToIndex[oldRowId] = rowCounter++;

            Vector::RowData rowData;
            // 先填充预设大小，避免后续访问越界
            for (int i = 0; i < columns.size(); ++i)
            {
                rowData.append(QVariant());
            }

            // 根据列名找到对应索引并填充
            for (int i = 0; i < columns.size(); ++i)
            {
                const auto &col = columns[i];
                if (col.name == "Label")
                    rowData[i] = queryOldData.value("label");
                else if (col.name == "Instruction")
                    rowData[i] = queryOldData.value("instruction_id");
                else if (col.name == "Timeset")
                    rowData[i] = queryOldData.value("timeset_id");
                else if (col.name == "Capture")
                    rowData[i] = queryOldData.value("capture");
                else if (col.name == "Ext")
                    rowData[i] = queryOldData.value("ext");
                else if (col.name == "Comment")
                    rowData[i] = queryOldData.value("comment");
                // 引脚列将在后续填充
            }

            allRows.append(rowData);
        }

        if (allRows.isEmpty())
        {
            qInfo() << funcName << " - 表 " << oldTableId << " 没有行数据，创建空二进制文件";

            // 创建只有头部的空二进制文件
            QFile binaryFile(absoluteBinaryFilePath);
            if (!binaryFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
            {
                m_lastError = "创建空二进制文件失败: " + binaryFile.errorString();
                qCritical() << funcName << " - " << m_lastError;
                allSuccess = false;
                break;
            }

            BinaryFileHeader header;
            header.row_count_in_file = 0;
            header.column_count_in_file = columns.size();
            header.data_schema_version = 1;
            header.timestamp_created = QDateTime::currentSecsSinceEpoch();
            header.timestamp_updated = header.timestamp_created;

            if (!Persistence::BinaryFileHelper::writeBinaryHeader(&binaryFile, header))
            {
                m_lastError = "写入空二进制文件头失败";
                qCritical() << funcName << " - " << m_lastError;
                allSuccess = false;
                binaryFile.close();
                break;
            }

            binaryFile.close();
            continue;
        }

        // 3.2 读取引脚值数据
        // 先获取所有向量表引脚的ID映射
        QMap<int, int> pinIdToColIndex;
        for (int i = 0; i < columns.size(); ++i)
        {
            if (columns[i].original_type_str == "PIN_STATE_ID" && !columns[i].data_properties.isEmpty())
            {
                int pinId = columns[i].data_properties["pin_list_id"].toInt();
                if (pinId > 0)
                {
                    pinIdToColIndex[pinId] = i;
                    qDebug() << funcName << " - 映射引脚ID " << pinId << " 到列索引 " << i;
                }
            }
        }

        if (!pinIdToColIndex.isEmpty())
        {
            // 查询引脚值
            QSqlQuery queryPinValues(m_db);
            queryPinValues.prepare(
                "SELECT vtpv.vector_data_id, vtpv.pin_level, vtp.pin_id "
                "FROM vector_table_pin_values vtpv "
                "JOIN vector_table_pins vtp ON vtpv.vector_pin_id = vtp.id "
                "WHERE vtp.table_id = ?");
            queryPinValues.addBindValue(oldTableId);

            if (!queryPinValues.exec())
            {
                m_lastError = "查询引脚值失败: " + queryPinValues.lastError().text();
                qCritical() << funcName << " - " << m_lastError;
                allSuccess = false;
                break;
            }

            while (queryPinValues.next())
            {
                int rowId = queryPinValues.value(0).toInt();
                int pinLevel = queryPinValues.value(1).toInt();
                int pinId = queryPinValues.value(2).toInt();

                if (oldRowIdToIndex.contains(rowId) && pinIdToColIndex.contains(pinId))
                {
                    int rowIndex = oldRowIdToIndex[rowId];
                    int colIndex = pinIdToColIndex[pinId];

                    if (rowIndex < allRows.size() && colIndex < columns.size())
                    {
                        allRows[rowIndex][colIndex] = pinLevel;
                        qDebug() << funcName << " - 设置行 " << rowIndex << " 列 " << colIndex << " 值=" << pinLevel;
                    }
                }
            }
        }

        // 4. 将数据写入二进制文件
        QFile binaryFile(absoluteBinaryFilePath);
        if (!binaryFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            m_lastError = "打开二进制文件写入失败: " + binaryFile.errorString();
            qCritical() << funcName << " - " << m_lastError;
            allSuccess = false;
            break;
        }

        BinaryFileHeader header;
        header.row_count_in_file = allRows.size();
        header.column_count_in_file = columns.size();
        header.data_schema_version = 1;
        header.timestamp_created = QDateTime::currentSecsSinceEpoch();
        header.timestamp_updated = header.timestamp_created;

        if (!Persistence::BinaryFileHelper::writeBinaryHeader(&binaryFile, header))
        {
            m_lastError = "写入二进制文件头失败";
            qCritical() << funcName << " - " << m_lastError;
            allSuccess = false;
            binaryFile.close();
            break;
        }

        // 逐行序列化写入
        for (int i = 0; i < allRows.size(); ++i)
        {
            QByteArray rowBytes;
            if (!Persistence::BinaryFileHelper::serializeRow(allRows[i], columns, rowBytes))
            {
                m_lastError = QString("第%1行序列化失败").arg(i + 1);
                qCritical() << funcName << " - " << m_lastError;
                allSuccess = false;
                break;
            }

            QDataStream out(&binaryFile);
            out.setByteOrder(QDataStream::LittleEndian);
            quint32 rowLen = rowBytes.size();
            out << rowLen;

            if (binaryFile.write(rowBytes) != rowBytes.size())
            {
                m_lastError = QString("第%1行数据写入失败").arg(i + 1);
                qCritical() << funcName << " - " << m_lastError;
                allSuccess = false;
                break;
            }
        }

        binaryFile.close();

        if (allSuccess)
        {
            // 5. 更新主记录表中的行数
            QSqlQuery updateRowCount(m_db);
            updateRowCount.prepare("UPDATE VectorTableMasterRecord SET row_count = ? WHERE id = ?");
            updateRowCount.addBindValue(allRows.size());
            updateRowCount.addBindValue(newTableId);

            if (!updateRowCount.exec())
            {
                m_lastError = "更新主记录表行数失败: " + updateRowCount.lastError().text();
                qCritical() << funcName << " - " << m_lastError;
                allSuccess = false;
                break;
            }

            qInfo() << funcName << " - 表 " << oldTableId << " 迁移完成，共 " << allRows.size() << " 行数据";
        }
    }

    if (allSuccess)
    {
        QSqlDatabase::database().commit();
        qInfo() << funcName << " - 所有向量表数据迁移成功";
        return true;
    }
    else
    {
        QSqlDatabase::database().rollback();
        qCritical() << funcName << " - 数据迁移失败，已回滚事务";
        return false;
    }
}
