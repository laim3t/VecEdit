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
#include <limits> // 添加 std::numeric_limits 支持

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
        
        // 安全检查：行数是否过大
        if (header.row_count_in_file > std::numeric_limits<int>::max() || header.row_count_in_file > 10000000) {
            qCritical() << funcName << " - 文件头中的行数过大: " << header.row_count_in_file 
                       << ", 超出安全限制或int最大值. 将限制行数处理.";
            // 这里直接返回错误，防止巨大数据导致内存分配失败
            file.close();
            return false;
        }
        
        // 安全预分配内存以提高性能
        try {
            rows.reserve(static_cast<int>(header.row_count_in_file));
        } catch (const std::bad_alloc&) {
            qCritical() << funcName << " - 内存分配失败，无法为" << header.row_count_in_file << "行数据分配内存";
            file.close();
            return false;
        }
        
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
            
            // 添加对行大小的安全检查
            const quint32 MAX_REASONABLE_ROW_SIZE = 1 * 1024 * 1024; // 1MB 是合理的单行最大值
            if (rowLen > MAX_REASONABLE_ROW_SIZE) 
            {
                qCritical() << funcName << " - 检测到异常大的行大小:" << rowLen 
                           << "字节，超过合理限制" << MAX_REASONABLE_ROW_SIZE << "字节，行:" << i << ". 可能是文件损坏或格式错误.";
                return false;
            }
            
            try {
            rowBytes.resize(rowLen);
            } catch (const std::bad_alloc&) {
                qCritical() << funcName << " - 内存分配失败，无法为行" << i << "分配" << rowLen << "字节的内存";
                return false;
            }
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

#include "vectordatahandler_core.cpp"
#include "vectordatahandler_cache.cpp"
#include "vectordatahandler_access.cpp"
#include "vectordatahandler_modify_save.cpp"
#include "vectordatahandler_modify_rowops.cpp"
#include "vectordatahandler_utils.cpp"
