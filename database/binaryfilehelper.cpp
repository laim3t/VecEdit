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
#include <limits> // 添加 std::numeric_limits 所需的头文件
#include "databasemanager.h" // For database access if needed
#include <QFile>
#include "common/binary_file_format.h" // For Header struct

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
        outByteArray.clear();
        QDataStream stream(&outByteArray, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_5_15); // Or your project's Qt version

        // QDataStream has native support for QList<QVariant>
        stream << rowData;

        return stream.status() == QDataStream::Ok;
    }

    bool Persistence::BinaryFileHelper::deserializeRow(const QByteArray &inByteArray, Vector::RowData &outRowData)
    {
        if (inByteArray.isEmpty()) {
            return false;
        }

        QDataStream stream(inByteArray);
        stream.setVersion(QDataStream::Qt_5_15); // Ensure this matches the writing version

        // Clear the output list before reading into it
        outRowData.clear();

        // QDataStream can read the QList<QVariant> directly
        stream >> outRowData;
        
        // Check for errors during deserialization
        if (stream.status() != QDataStream::Ok) {
            // This could happen if the byte array is malformed or truncated
            qWarning() << "BinaryFileHelper::deserializeRow - QDataStream status is not Ok after reading.";
            return false;
        }

        // Additional check to see if we've reached the end of the stream.
        // If not, it might indicate a problem (e.g., extra, unexpected data).
        if (!stream.atEnd()) {
            qWarning() << "BinaryFileHelper::deserializeRow - Stream not at end after deserializing row data. Possible malformed data.";
        }
        
        return true;
    }
} // namespace Persistence
