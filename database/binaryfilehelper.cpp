#include "binaryfilehelper.h"
#include "common/logger.h"
#include <QDataStream>
#include <QJsonDocument>
#include "vector/vector_data_types.h"
#include <QFileInfo>
#include <QDebug>
#include <cstring> // 使用 <cstring> 替代 <string.h>
#include <QDir>
#include <cstdio>    // Added for fprintf
#include <QDateTime> // Added for QDateTime
#include <QElapsedTimer>
#include <QThread>
#include <QtConcurrent/QtConcurrent>
#include <limits>            // 添加 std::numeric_limits 所需的头文件
#include "databasemanager.h" // For database access if needed
#include <QFile>
#include "common/binary_file_format.h" // For Header struct
#include <QSqlDatabase>
#include <QSqlQuery>

// 初始化静态成员
QMap<QString, Persistence::BinaryFileHelper::RowOffsetCache> Persistence::BinaryFileHelper::s_fileRowOffsetCache;
QMutex Persistence::BinaryFileHelper::s_cacheMutex;

// 索引文件魔数和版本
#define INDEX_FILE_MAGIC 0x56494458 // "VIDX" in ASCII
#define INDEX_FILE_VERSION 1

// Macro for direct stderr logging
#define BFH_LOG_STDERR(funcName, format, ...)                                                                          \
    do                                                                                                                 \
    {                                                                                                                  \
        char buffer[4096]; /* 增加缓冲区大小从默认的较小值到4KB */                                                     \
        int written = snprintf(buffer, sizeof(buffer), "[BFH_STDERR][%s] %s: " format "\n",                            \
                               QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz").toStdString().c_str(), \
                               funcName, ##__VA_ARGS__);                                                               \
        if (written >= 0)                                                                                              \
        {                                                                                                              \
            /* 检查是否有截断情况 */                                                                                   \
            if ((size_t)written >= sizeof(buffer))                                                                     \
            {                                                                                                          \
                fprintf(stderr, "%s... (消息被截断，完整消息过长)\n", buffer);                                         \
            }                                                                                                          \
            else                                                                                                       \
            {                                                                                                          \
                fprintf(stderr, "%s", buffer);                                                                         \
            }                                                                                                          \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            fprintf(stderr, "[BFH_STDERR_ERROR] 日志格式化失败\n");                                                    \
        }                                                                                                              \
        fflush(stderr);                                                                                                \
    } while (0)

namespace Persistence
{
    // 静态初始化代码，在程序启动时设置模块日志级别
    static struct BinaryFileHelperLoggerInitializer
    {
        BinaryFileHelperLoggerInitializer()
        {
            // 设置BinaryFileHelper模块的日志级别
            // 可根据需要调整级别：Debug, Info, Warning, Critical, Fatal
            Logger::instance().setModuleLogLevel("BinaryFileHelper", Logger::LogLevel::Debug);
        }
    } __binaryFileHelperLoggerInitializer;

    BinaryFileHelper::BinaryFileHelper()
    {
        // Constructor for future use, if needed for non-static members or setup.
        // For now, all public methods are static.
        qDebug() << "BinaryFileHelper instance created (though methods are static).";
    }

    bool BinaryFileHelper::readBinaryHeader(QIODevice *device, BinaryFileHeader &header)
    {
        const QString funcName = "BinaryFileHelper::readBinaryHeader";
        qDebug().nospace() << funcName << " - Attempting to read binary file header.";

        if (!device)
        {
            qWarning() << funcName << " - Error: Null QIODevice provided.";
            return false;
        }
        if (!device->isOpen() || !device->isReadable())
        {
            qWarning() << funcName << " - Error: Device is not open or not readable. Error: " << device->errorString();
            return false;
        }

        QDataStream in(device);
        in.setByteOrder(QDataStream::LittleEndian);

        qint64 initialPos = device->pos();

        BinaryFileHeader tempHeader;

        in >> tempHeader.magic_number;
        in >> tempHeader.file_format_version;
        in >> tempHeader.data_schema_version;
        in >> tempHeader.row_count_in_file;
        in >> tempHeader.column_count_in_file;
        in >> tempHeader.timestamp_created;
        in >> tempHeader.timestamp_updated;
        in >> tempHeader.compression_type;

        if (in.status() != QDataStream::Ok)
        {
            qWarning() << funcName << " - Error: QDataStream error after reading initial fields. Status: " << in.status();
            device->seek(initialPos);
            return false;
        }

        qint64 reservedBytesRead = in.readRawData(reinterpret_cast<char *>(tempHeader.reserved_bytes), sizeof(tempHeader.reserved_bytes));
        if (reservedBytesRead != sizeof(tempHeader.reserved_bytes))
        {
            qWarning() << funcName << " - Error: Failed to read reserved_bytes. Read: " << reservedBytesRead << ", Expected: " << sizeof(tempHeader.reserved_bytes) << " Stream status: " << in.status();
            device->seek(initialPos);
            return false;
        }

        if (in.status() != QDataStream::Ok)
        {
            qWarning() << funcName << " - Error: QDataStream encountered an error during header read. Status: " << in.status();
            device->seek(initialPos);
            return false;
        }

        if (!tempHeader.isValid())
        {
            qWarning() << funcName << " - Error: Invalid magic number in header. Expected: 0x" << QString::number(VBIN_MAGIC_NUMBER, 16) << ", Got: 0x" << QString::number(tempHeader.magic_number, 16);
            tempHeader.logDetails(funcName + " - Invalid Header Content");
            device->seek(initialPos);
            return false;
        }

        header = tempHeader;
        header.logDetails(funcName + " - Successfully Read Header");
        qDebug() << funcName << " - Binary file header read and validated successfully.";
        return true;
    }

    bool BinaryFileHelper::writeBinaryHeader(QIODevice *device, const BinaryFileHeader &header)
    {
        const QString funcName = "BinaryFileHelper::writeBinaryHeader";
        qDebug().nospace() << funcName << " - Attempting to write binary file header.";

        if (!device)
        {
            qWarning() << funcName << " - Error: Null QIODevice provided.";
            return false;
        }
        if (!device->isOpen() || !device->isWritable())
        {
            qWarning() << funcName << " - Error: Device is not open or not writable. Error: " << device->errorString();
            return false;
        }

        // Create a mutable copy to ensure the magic number is correct before writing
        BinaryFileHeader headerToWrite = header;
        // **强制**设置正确的魔数，覆盖传入值中可能存在的错误
        headerToWrite.magic_number = Persistence::VEC_BINDATA_MAGIC;

        // 现在使用 headerToWrite 进行检查和写入
        if (!headerToWrite.isValid()) // 这个检查理论上现在总会通过
        {
            qWarning() << funcName << " - Error: Attempting to write an invalid header (magic number mismatch after explicit set - THIS SHOULD NOT HAPPEN).";
            headerToWrite.logDetails(funcName + " - Invalid Header Content For Write (After Fix)");
            return false;
        }

        QDataStream out(device);
        out.setByteOrder(QDataStream::LittleEndian); // Match the byte order used in reading

        qint64 initialPos = device->pos();

        // Write fields individually using QDataStream operators
        out << headerToWrite.magic_number; // 使用修正后的 headerToWrite
        out << headerToWrite.file_format_version;
        out << headerToWrite.data_schema_version;
        out << headerToWrite.row_count_in_file;
        out << headerToWrite.column_count_in_file;
        out << headerToWrite.timestamp_created;
        out << headerToWrite.timestamp_updated;
        out << headerToWrite.compression_type;

        // For fixed-size arrays like reserved_bytes, write them carefully.
        qint64 reservedBytesWritten = out.writeRawData(reinterpret_cast<const char *>(headerToWrite.reserved_bytes), sizeof(headerToWrite.reserved_bytes));
        if (reservedBytesWritten != sizeof(headerToWrite.reserved_bytes))
        {
            qWarning() << funcName << " - Error: Failed to write reserved_bytes. Written: " << reservedBytesWritten << ", Expected: " << sizeof(headerToWrite.reserved_bytes) << " Stream status: " << out.status();
            device->seek(initialPos); // Attempt to rewind/truncate if possible, though partial writes are problematic.
            return false;
        }

        if (out.status() != QDataStream::Ok)
        {
            qWarning() << funcName << " - Error: QDataStream encountered an error during header write. Status: " << out.status();
            // It's hard to reliably revert a partial write to a generic QIODevice.
            // The caller might need to handle file truncation or deletion on failure.
            return false;
        }

        headerToWrite.logDetails(funcName + " - Successfully Written Header"); // Log the written header
        qDebug() << funcName << " - Binary file header written successfully.";
        return true;
    }

    int BinaryFileHelper::getFixedLengthForType(Vector::ColumnDataType type, const QString &columnName)
    {
        // 移除日志输出以提高性能
        // const QString funcName = "BinaryFileHelper::getFixedLengthForType";
        // qDebug() << funcName << " - 获取数据类型的固定长度, 类型:" << static_cast<int>(type) << ", 列名:" << columnName;

        // 首先检查是否是特殊命名的字段
        if (!columnName.isEmpty())
        {
            if (columnName.compare("Label", Qt::CaseInsensitive) == 0)
            {
                // qDebug() << funcName << " - 使用Label字段特定长度:" << LABEL_FIELD_MAX_LENGTH;
                return LABEL_FIELD_MAX_LENGTH;
            }
            else if (columnName.compare("Comment", Qt::CaseInsensitive) == 0)
            {
                // qDebug() << funcName << " - 使用Comment字段特定长度:" << COMMENT_FIELD_MAX_LENGTH;
                return COMMENT_FIELD_MAX_LENGTH;
            }
            else if (columnName.compare("EXT", Qt::CaseInsensitive) == 0)
            {
                // qDebug() << funcName << " - 使用EXT字段特定长度:" << EXT_FIELD_MAX_LENGTH;
                return EXT_FIELD_MAX_LENGTH;
            }
            else if (columnName.compare("Capture", Qt::CaseInsensitive) == 0)
            {
                // 使用Capture字段特定长度
                return CAPTURE_FIELD_MAX_LENGTH;
            }
        }

        // 如果不是特定命名字段，则根据数据类型返回默认长度
        switch (type)
        {
        case Vector::ColumnDataType::TEXT:
            return TEXT_FIELD_MAX_LENGTH;
        case Vector::ColumnDataType::PIN_STATE_ID:
            return PIN_STATE_FIELD_MAX_LENGTH;
        case Vector::ColumnDataType::INTEGER:
        case Vector::ColumnDataType::INSTRUCTION_ID:
        case Vector::ColumnDataType::TIMESET_ID:
            return INTEGER_FIELD_MAX_LENGTH;
        case Vector::ColumnDataType::REAL:
            return REAL_FIELD_MAX_LENGTH;
        case Vector::ColumnDataType::BOOLEAN:
            return BOOLEAN_FIELD_MAX_LENGTH;
        case Vector::ColumnDataType::JSON_PROPERTIES:
            return JSON_PROPERTIES_MAX_LENGTH;
        default:
            // 保留警告日志以便于问题排查
            qWarning() << "BinaryFileHelper::getFixedLengthForType - 未知数据类型:" << static_cast<int>(type) << ", 返回默认长度(TEXT)";
            return TEXT_FIELD_MAX_LENGTH;
        }
    }

#include "binaryfilehelper_1.cpp"
#include "binaryfilehelper_2.cpp"
#include "binaryfilehelper_3.cpp"
#include "binaryfilehelper_4.cpp"

    bool Persistence::BinaryFileHelper::serializeRow(const Vector::RowData &rowData, QByteArray &outByteArray)
    {
        // 使用"长度前缀"协议，每个字段前面有一个4字节的长度标记
        outByteArray.clear();
        QDataStream stream(&outByteArray, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        // 写入字段数量
        stream << (quint32)rowData.size();

        // 逐个写入每个字段
        for (const QVariant &value : rowData)
        {
            // 将字段序列化为二进制
            QByteArray fieldData;
            QDataStream fieldStream(&fieldData, QIODevice::WriteOnly);
            fieldStream.setByteOrder(QDataStream::LittleEndian);
            fieldStream << value;

            if (fieldStream.status() != QDataStream::Ok)
            {
                qWarning() << "serializeRow: Failed to serialize field value:" << value;
                return false;
            }

            // 写入字段长度和数据
            stream << (quint32)fieldData.size();
            stream.writeRawData(fieldData.constData(), fieldData.size());
        }

        return stream.status() == QDataStream::Ok;
    }

    bool Persistence::BinaryFileHelper::deserializeRow(const QByteArray &inByteArray, Vector::RowData &outRowData)
    {
        outRowData.clear();
        if (inByteArray.isEmpty())
            return true; // 空数据是有效的

        QDataStream stream(inByteArray);
        stream.setByteOrder(QDataStream::LittleEndian);

        // 读取字段数量
        quint32 fieldCount;
        stream >> fieldCount;

        if (stream.status() != QDataStream::Ok)
        {
            qWarning() << "deserializeRow: Failed to read field count.";
            return false;
        }

        // 安全检查：确保字段数量合理
        if (fieldCount > 1000)
        { // 1000列是一个合理的上限
            qWarning() << "deserializeRow: Unreasonable field count:" << fieldCount;
            return false;
        }

        // 读取每个字段
        for (quint32 i = 0; i < fieldCount; ++i)
        {
            // 读取字段长度
            quint32 fieldSize;
            stream >> fieldSize;

            if (stream.status() != QDataStream::Ok)
            {
                qWarning() << "deserializeRow: Failed to read field size for field" << i;
                return false;
            }

            // 安全检查：确保字段大小合理
            if (fieldSize > 10000000)
            { // 10MB是一个合理的上限
                qWarning() << "deserializeRow: Unreasonable field size:" << fieldSize << "for field" << i;
                return false;
            }

            // 读取字段数据
            QByteArray fieldData = stream.device()->read(fieldSize);
            if ((quint32)fieldData.size() != fieldSize)
            {
                qWarning() << "deserializeRow: Failed to read field data. Expected" << fieldSize << "got" << fieldData.size() << "for field" << i;
                return false;
            }

            // 反序列化字段值
            QDataStream fieldStream(fieldData);
            fieldStream.setByteOrder(QDataStream::LittleEndian);
            QVariant value;
            fieldStream >> value;

            if (fieldStream.status() != QDataStream::Ok)
            {
                qWarning() << "deserializeRow: Failed to deserialize field value for field" << i;
                return false;
            }

            outRowData.append(value);
        }

        return true;
    }

    bool Persistence::BinaryFileHelper::updateRowCountInHeader(const QString &absoluteBinFilePath, int newRowCount)
    {
        const QString funcName = "BinaryFileHelper::updateRowCountInHeader";
        qDebug() << funcName << " - 开始更新文件行数: " << absoluteBinFilePath << " 到 " << newRowCount;

        QFile file(absoluteBinFilePath);
        if (!file.open(QIODevice::ReadWrite))
        {
            qWarning() << funcName << " - 无法打开文件: " << absoluteBinFilePath << " Error: " << file.errorString();
            return false;
        }

        // 读取现有头部
        BinaryFileHeader header;
        if (!readBinaryHeader(&file, header))
        {
            qWarning() << funcName << " - 无法读取文件头";
            file.close();
            return false;
        }

        // 更新行数
        header.row_count_in_file = newRowCount;
        header.timestamp_updated = QDateTime::currentSecsSinceEpoch();

        // 重置文件指针到开始位置
        if (!file.seek(0))
        {
            qWarning() << funcName << " - 无法重置文件指针到开始位置";
            file.close();
            return false;
        }

        // 写入更新后的头部
        if (!writeBinaryHeader(&file, header))
        {
            qWarning() << funcName << " - 无法写入更新后的文件头";
            file.close();
            return false;
        }

        file.close();
        qDebug() << funcName << " - 成功更新文件 " << absoluteBinFilePath << " 的行数为 " << newRowCount;
        return true;
    }

    bool Persistence::BinaryFileHelper::initBinaryFile(const QString &filePath, int columnCount, int schemaVersion)
    {
        const QString funcName = "BinaryFileHelper::initBinaryFile";
        qDebug() << funcName << " - 初始化二进制文件: " << filePath;

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            qWarning() << funcName << " - 无法打开文件进行写入: " << filePath << " 错误: " << file.errorString();
            return false;
        }

        // 创建并初始化头部
        BinaryFileHeader header;
        header.magic_number = Persistence::VEC_BINDATA_MAGIC;
        header.file_format_version = Persistence::CURRENT_FILE_FORMAT_VERSION;
        header.data_schema_version = schemaVersion;
        header.row_count_in_file = 0; // 初始为0行
        header.column_count_in_file = columnCount;
        header.timestamp_created = QDateTime::currentSecsSinceEpoch();
        header.timestamp_updated = header.timestamp_created;
        header.compression_type = 0; // 无压缩
        memset(header.reserved_bytes, 0, sizeof(header.reserved_bytes));

        // 写入头部
        if (!writeBinaryHeader(&file, header))
        {
            qWarning() << funcName << " - 写入文件头失败";
            file.close();
            return false;
        }

        file.close();
        qDebug() << funcName << " - 成功初始化二进制文件: " << filePath;
        return true;
    }

    bool Persistence::BinaryFileHelper::readAndValidateHeader(QFile &file, Persistence::HeaderData &headerData, int expectedSchemaVersion)
    {
        const QString funcName = "BinaryFileHelper::readAndValidateHeader";

        // 确保文件已打开
        if (!file.isOpen())
        {
            qWarning() << funcName << " - 文件未打开";
            return false;
        }

        // 重置文件指针到开始位置
        if (!file.seek(0))
        {
            qWarning() << funcName << " - 无法重置文件指针到开始位置";
            return false;
        }

        // 读取完整头部
        BinaryFileHeader fullHeader;
        if (!readBinaryHeader(&file, fullHeader))
        {
            qWarning() << funcName << " - 读取文件头失败";
            return false;
        }

        // 提取简化的HeaderData
        headerData.magic_number = fullHeader.magic_number;
        headerData.file_format_version = fullHeader.file_format_version;
        headerData.data_schema_version = fullHeader.data_schema_version;
        headerData.row_count = fullHeader.row_count_in_file;
        headerData.column_count = fullHeader.column_count_in_file;

        // 验证魔数和版本
        if (!fullHeader.isValid())
        {
            qWarning() << funcName << " - 无效的魔数: 0x" << QString::number(headerData.magic_number, 16);
            return false;
        }

        // 检查Schema版本兼容性
        if (headerData.data_schema_version != expectedSchemaVersion)
        {
            qWarning() << funcName << " - Schema版本不匹配: 期望 " << expectedSchemaVersion
                       << ", 实际 " << headerData.data_schema_version;

            // 如果文件版本高于期望版本，则拒绝读取
            if (headerData.data_schema_version > expectedSchemaVersion)
            {
                qCritical() << funcName << " - 文件版本高于当前支持的版本，无法读取";
                return false;
            }
            // 如果文件版本低于期望版本，发出警告但继续
            else
            {
                qWarning() << funcName << " - 文件版本较旧，但继续处理";
            }
        }

        return true;
    }

    bool Persistence::BinaryFileHelper::readPageDataFromBinary(
        const QString &absoluteBinFilePath,
        const QList<Vector::ColumnInfo> &columns,
        int schemaVersion,
        int startRow,
        int numRows,
        QList<QList<QVariant>> &pageRows)
    {
        const QString funcName = "BinaryFileHelper::readPageDataFromBinary";
        pageRows.clear();

        QFile file(absoluteBinFilePath);
        if (!file.open(QIODevice::ReadOnly))
        {
            qWarning() << funcName << " - 无法打开文件:" << absoluteBinFilePath << "，错误:" << file.errorString();
            return false;
        }

        // 1. 获取数据库连接和表ID
        QSqlDatabase db = DatabaseManager::instance()->database();
        if (!db.isOpen())
        {
            qWarning() << funcName << " - 数据库未连接";
            return false;
        }

        QFileInfo binFileInfo(absoluteBinFilePath);

        // 直接通过二进制文件名查询表ID
        QSqlQuery tableIdQuery(db);
        tableIdQuery.prepare("SELECT id FROM VectorTableMasterRecord WHERE binary_data_filename = ?");
        tableIdQuery.addBindValue(binFileInfo.fileName());

        int tableId = -1;
        if (tableIdQuery.exec() && tableIdQuery.next())
        {
            tableId = tableIdQuery.value(0).toInt();
        }
        else
        {
            // 在这里也添加 lastError() 以提供更多上下文
            qWarning() << funcName << " - 无法通过文件名找到表ID:" << binFileInfo.fileName() << "错误:" << tableIdQuery.lastError().text();
            return false;
        }

        // 2. 查询分页所需的行索引信息
        QSqlQuery indexQuery(db);
        indexQuery.prepare(
            "SELECT offset, size FROM VectorTableRowIndex "
            "WHERE master_record_id = ? AND is_active = 1 "
            "ORDER BY logical_row_order "
            "LIMIT ? OFFSET ?");
        indexQuery.addBindValue(tableId);
        indexQuery.addBindValue(numRows);
        indexQuery.addBindValue(startRow);

        if (!indexQuery.exec())
        {
            qWarning() << funcName << " - 查询行索引失败:" << indexQuery.lastError().text();
            file.close();
            return false;
        }

        QList<QPair<qint64, qint64>> rowPositions;
        while (indexQuery.next())
        {
            rowPositions.append({indexQuery.value(0).toLongLong(), indexQuery.value(1).toLongLong()});
        }

        if (rowPositions.isEmpty() && numRows > 0)
        {
            qDebug() << funcName << " - 在请求的范围内没有找到行索引数据。";
            file.close();
            return true; // 没有数据也是一种成功状态
        }

        qDebug() << funcName << " - 成功查询到" << rowPositions.size() << "条行索引，准备从文件读取。";

        // 3. 根据索引信息，逐行读取和反序列化
        for (const auto &pos : rowPositions)
        {
            qint64 offset = pos.first;
            qint64 size = pos.second;

            if (size <= 0)
            {
                qWarning() << funcName << " - 无效的行大小(" << size << ")，跳过。";
                continue;
            }

            if (!file.seek(offset))
            {
                qWarning() << funcName << " - 无法定位到文件偏移:" << offset;
                continue;
            }

            QByteArray rowData = file.read(size);
            if (rowData.size() != size)
            {
                qWarning() << funcName << " - 读取行数据失败，预期:" << size << "字节，实际:" << rowData.size() << "字节。";
                continue;
            }

            Vector::RowData row;
            if (deserializeRow(rowData, row))
            {
                // 确保行数据与列数匹配
                while (row.size() < columns.size())
                {
                    row.append(QVariant()); // 添加空值以匹配列数
                }

                // 如果行数据超过列数，则截断
                if (row.size() > columns.size())
                {
                    row = row.mid(0, columns.size());
                }

                pageRows.append(row);
            }
            else
            {
                qWarning() << funcName << " - 行数据反序列化失败，偏移:" << offset << "大小:" << size;
                // 添加一个空行，以保持行数一致
                Vector::RowData emptyRow;
                for (int i = 0; i < columns.size(); ++i)
                {
                    emptyRow.append(QVariant());
                }
                pageRows.append(emptyRow);
            }
        }

        file.close();
        qDebug() << funcName << " - 读取完成，成功反序列化" << pageRows.size() << "行。";

        return true;
    }
} // namespace Persistence
