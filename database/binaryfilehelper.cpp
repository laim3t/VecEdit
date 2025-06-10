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

    bool BinaryFileHelper::serializeRow(const Vector::RowData &rowData, const QList<Vector::ColumnInfo> &columns, QByteArray &serializedRow)
    {
        // 使用静态计数器控制日志频率
        static int serializeCounter = 0;
        static const int LOG_INTERVAL = 1000; // 每1000次输出一次日志

        const QString funcName = "BinaryFileHelper::serializeRow";

        // 只在特定间隔输出日志，减少日志量
        if (++serializeCounter % LOG_INTERVAL == 1 || serializeCounter <= 3)
        {
            qDebug() << funcName << " - 开始序列化行数据, 列数:" << columns.size()
                     << ", 计数器:" << serializeCounter;
        }

        serializedRow.clear();

        if (rowData.size() != columns.size())
        {
            qWarning() << funcName << " - 列数与数据项数不一致!";
            return false;
        }

        QByteArray tempData;
        QDataStream out(&tempData, QIODevice::WriteOnly);
        out.setByteOrder(QDataStream::LittleEndian);

        for (int i = 0; i < columns.size(); ++i)
        {
            const auto &col = columns[i];
            const QVariant &val = rowData[i];

            // 使用列名称和类型获取该字段的固定长度
            int fieldLength = getFixedLengthForType(col.type, col.name);
            // 移除循环中的日志
            // qDebug() << funcName << " - 字段固定长度:" << fieldLength;

            switch (col.type)
            {
            case Vector::ColumnDataType::TEXT:
            {
                QString text = val.toString();
                // 确保文本不超过字段的固定长度限制
                if (text.toUtf8().size() > fieldLength - 4) // 减去4字节用于存储长度字段
                {
                    qWarning() << funcName << " - 文本长度超出限制，将被截断!";
                    // 截断文本，确保UTF-8编码后不超过限制
                    while (text.toUtf8().size() > fieldLength - 4)
                    {
                        text.chop(1);
                    }
                }

                // 写入文本的实际长度
                qint32 textLength = text.toUtf8().size();
                out << textLength;

                // 写入文本内容
                out.writeRawData(text.toUtf8().constData(), textLength);

                // 填充剩余空间
                int paddingNeeded = fieldLength - 4 - textLength; // 4字节长度字段
                if (paddingNeeded > 0)
                {
                    QByteArray padding(paddingNeeded, '\0');
                    out.writeRawData(padding.constData(), paddingNeeded);
                }
                break;
            }
            case Vector::ColumnDataType::PIN_STATE_ID:
            {
                // 管脚状态总是1个字符
                QString pinValue;
                if (val.isNull() || !val.isValid() || val.toString().isEmpty())
                {
                    // 使用默认值'X'
                    pinValue = "X";
                    // 移除循环中的日志
                    // qDebug() << funcName << " - 管脚状态列为空或无效，使用默认值'X'，列名:" << col.name;
                }
                else
                {
                    pinValue = val.toString().left(1).toUpper(); // 只取第一个字符并转为大写
                    // 验证是否为有效的管脚状态
                    if (pinValue != "0" && pinValue != "1" && pinValue != "X" && pinValue != "L" &&
                        pinValue != "H" && pinValue != "S" && pinValue != "V" && pinValue != "M")
                    {
                        pinValue = "X"; // 无效的状态使用X
                        // 移除循环中的日志
                        // qDebug() << funcName << " - 管脚状态值无效，使用默认值'X'，列名:" << col.name;
                    }
                }
                // 直接写入字符
                char pinChar = pinValue.at(0).toLatin1();
                out.writeRawData(&pinChar, PIN_STATE_FIELD_MAX_LENGTH);
                break;
            }
            case Vector::ColumnDataType::INTEGER:
            case Vector::ColumnDataType::INSTRUCTION_ID:
            case Vector::ColumnDataType::TIMESET_ID:
            {
                int intValue = val.toInt();
                out << quint32(intValue);
                break;
            }
            case Vector::ColumnDataType::REAL:
            {
                double realValue = val.toDouble();
                out << realValue;
                break;
            }
            case Vector::ColumnDataType::BOOLEAN:
            {
                quint8 boolValue = val.toBool() ? 1 : 0;
                out << boolValue;
                break;
            }
            case Vector::ColumnDataType::JSON_PROPERTIES:
            {
                QString jsonStr = QString(QJsonDocument(val.toJsonObject()).toJson(QJsonDocument::Compact));
                if (jsonStr.length() > JSON_PROPERTIES_MAX_LENGTH / 2)
                {
                    qWarning() << funcName << " - JSON属性超出最大长度限制! 原长度:" << jsonStr.length() << ", 最大允许:" << JSON_PROPERTIES_MAX_LENGTH / 2;
                    // 这里我们选择截断JSON，但在实际应用中可能需要更优雅的处理
                    jsonStr = jsonStr.left(JSON_PROPERTIES_MAX_LENGTH / 2);
                }
                out << quint32(jsonStr.length()); // 先写入实际长度
                out.writeRawData(jsonStr.toUtf8().constData(), jsonStr.toUtf8().size());
                // 填充剩余空间
                int padding = JSON_PROPERTIES_MAX_LENGTH - jsonStr.toUtf8().size() - sizeof(quint32);
                if (padding > 0)
                {
                    QByteArray paddingData(padding, '\0');
                    out.writeRawData(paddingData.constData(), paddingData.size());
                }
                break;
            }
            default:
                qWarning() << funcName << " - 不支持的列类型:" << static_cast<int>(col.type) << ", 列名:" << col.name;
                return false;
            }

            if (out.status() != QDataStream::Ok)
            {
                qWarning() << funcName << " - QDataStream 写入失败, 列:" << col.name;
                return false;
            }
        }

        // 将临时数据复制到输出
        serializedRow = tempData;

        // 仅在特定间隔输出完成日志
        if (serializeCounter % LOG_INTERVAL == 1 || serializeCounter <= 3)
        {
            qDebug() << funcName << " - 序列化完成, 字节长度:" << serializedRow.size()
                     << ", 计数器:" << serializeCounter;
        }
        return true;
    }

    bool BinaryFileHelper::deserializeRow(const QByteArray &bytes, const QList<Vector::ColumnInfo> &columns, int fileVersion, Vector::RowData &rowData)
    {
        // 使用静态计数器控制日志频率
        static int deserializeCounter = 0;
        static const int LOG_INTERVAL = 1000; // 每1000次输出一次日志

        const QString funcName = "BinaryFileHelper::deserializeRow";
        // 只在特定间隔输出日志，减少日志量
        if (++deserializeCounter % LOG_INTERVAL == 1 || deserializeCounter <= 3)
        {
            // 只输出前几行和每LOG_INTERVAL行的日志
            qDebug() << funcName << " - 开始反序列化二进制数据，字节数:" << bytes.size()
                     << ", 列数:" << columns.size() << ", 文件版本:" << fileVersion
                     << ", 计数器:" << deserializeCounter;
        }

        rowData.clear();

        if (bytes.isEmpty())
        {
            qWarning() << funcName << " - 输入的二进制数据为空!";
            return false;
        }

        QDataStream in(bytes);
        in.setByteOrder(QDataStream::LittleEndian);

        for (const auto &col : columns)
        {
            // 获取该字段的固定长度
            int fieldLength = getFixedLengthForType(col.type, col.name);
            // 移除列级别的处理日志，减少日志输出
            // qDebug().nospace() << funcName << " - 处理列: " << col.name << ", 类型: " << static_cast<int>(col.type) << ", 固定长度: " << fieldLength;

            QVariant value;
            // 使用固定长度格式
            switch (col.type)
            {
            case Vector::ColumnDataType::TEXT:
            {
                // 读取文本实际长度
                qint32 textLength;
                in >> textLength;

                if (textLength < 0 || textLength > fieldLength - 4)
                {
                    qWarning() << funcName << " - 反序列化TEXT字段时长度无效:" << textLength << ", 列名:" << col.name;
                    textLength = qMin(textLength, fieldLength - 4);
                }

                // 读取实际文本内容
                QByteArray textData(textLength, Qt::Uninitialized);
                in.readRawData(textData.data(), textLength);
                value = QString::fromUtf8(textData);

                // 跳过填充字节
                int paddingToSkip = fieldLength - 4 - textLength;
                if (paddingToSkip > 0)
                {
                    in.skipRawData(paddingToSkip);
                }
                break;
            }
            case Vector::ColumnDataType::PIN_STATE_ID:
            {
                // 管脚状态是1个字符
                char pinState;
                in.readRawData(&pinState, PIN_STATE_FIELD_MAX_LENGTH);
                value = QString(QChar(pinState));
                break;
            }
            case Vector::ColumnDataType::INTEGER:
            case Vector::ColumnDataType::INSTRUCTION_ID:
            case Vector::ColumnDataType::TIMESET_ID:
            {
                qint32 intValue;
                in.readRawData(reinterpret_cast<char *>(&intValue), INTEGER_FIELD_MAX_LENGTH);
                value = intValue;
                break;
            }
            case Vector::ColumnDataType::REAL:
            {
                double doubleValue;
                in.readRawData(reinterpret_cast<char *>(&doubleValue), REAL_FIELD_MAX_LENGTH);
                value = doubleValue;
                break;
            }
            case Vector::ColumnDataType::BOOLEAN:
            {
                quint8 boolValue;
                in.readRawData(reinterpret_cast<char *>(&boolValue), BOOLEAN_FIELD_MAX_LENGTH);
                value = bool(boolValue);
                break;
            }
            case Vector::ColumnDataType::JSON_PROPERTIES:
            {
                // 读取JSON字符串长度
                qint32 jsonLength;
                in >> jsonLength;

                if (jsonLength < 0 || jsonLength > JSON_PROPERTIES_MAX_LENGTH - 4)
                {
                    qWarning() << funcName << " - 反序列化JSON字段时长度无效:" << jsonLength;
                    jsonLength = qMin(jsonLength, JSON_PROPERTIES_MAX_LENGTH - 4);
                }

                // 读取JSON字符串
                QByteArray jsonData(jsonLength, Qt::Uninitialized);
                in.readRawData(jsonData.data(), jsonLength);
                QString jsonString = QString::fromUtf8(jsonData);
                QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());

                if (doc.isNull() || !doc.isObject())
                {
                    qWarning() << funcName << " - 解析JSON数据失败，列名:" << col.name;
                    value = QJsonObject();
                }
                else
                {
                    value = doc.object();
                }

                // 跳过填充字节
                int paddingToSkip = JSON_PROPERTIES_MAX_LENGTH - 4 - jsonLength;
                if (paddingToSkip > 0)
                {
                    in.skipRawData(paddingToSkip);
                }
                break;
            }
            default:
                qWarning() << funcName << " - 不支持的列类型:" << static_cast<int>(col.type) << ", 列名:" << col.name;
                // 使用默认空值
                value = QVariant(); // Default to a null QVariant
                // 为了健壮性，即使遇到未知类型，也尝试跳过预期的固定长度，如果fieldLength在此处有意义
                // 但由于这是固定长度模式，我们必须读取或跳过fieldLength字节以保持流的同步
                // 注意：如果getFixedLengthForType为未知类型返回0或负值，这里需要额外处理
                if (fieldLength > 0)
                {
                    in.skipRawData(fieldLength); // 假设未知类型也占用了其定义的固定长度
                }
                else
                {
                    // 如果没有固定长度或长度无效，这可能是一个严重错误，可能导致后续数据错位
                    qWarning() << funcName << " - 未知类型的字段长度无效，可能导致数据解析错误，列名:" << col.name;
                    // 无法安全跳过，返回错误
                    return false;
                }
                break;
            }

            rowData.append(value);
            // 移除每列的详细解析日志
            // qDebug().nospace() << funcName << " - 解析列: " << col.name << ", 值: " << value;
        }

        if (in.status() != QDataStream::Ok)
        {
            qWarning() << funcName << " - 反序列化过程中发生错误，QDataStream状态:" << in.status();
            return false;
        }

        return true;
    }

    bool BinaryFileHelper::readAllRowsFromBinary(const QString &binFilePath, const QList<Vector::ColumnInfo> &columns,
                                                 int schemaVersion, QList<Vector::RowData> &rows)
    {
        const QString funcName = "BinaryFileHelper::readAllRowsFromBinary";
        qDebug() << funcName << "- 开始读取文件:" << binFilePath;
        rows.clear(); // Ensure the output list is empty initially

        QFile file(binFilePath);
        qDebug() << funcName << "- 尝试打开文件进行读取.";
        if (!file.open(QIODevice::ReadOnly))
        {
            qWarning() << funcName << "- 错误: 无法打开文件:" << binFilePath << "错误信息:" << file.errorString();
            return false;
        }
        qDebug() << funcName << "- 文件打开成功.";

        BinaryFileHeader header;
        qDebug() << funcName << "- 尝试读取二进制头信息.";
        if (!readBinaryHeader(&file, header))
        {
            qWarning() << funcName << "- 错误: 无法读取或验证二进制头信息.";
            file.close();
            return false;
        }
        qDebug() << funcName << "- 二进制头信息读取成功. 文件中有" << header.row_count_in_file << "行数据.";
        header.logDetails(funcName); // Log header details

        // --- Version Compatibility Check ---
        if (header.data_schema_version != schemaVersion)
        {
            qWarning() << funcName << "- 警告: 文件数据schema版本 (" << header.data_schema_version
                       << ") 与预期的DB schema版本不同 (" << schemaVersion << "). 尝试兼容处理.";
            // Implement compatibility logic here if needed, e.g., using different deserializeRow versions.
            // For now, we'll proceed but pass the file's schema version to deserializeRow.
            if (header.data_schema_version > schemaVersion)
            {
                qCritical() << funcName << "- 错误: 文件数据schema版本高于DB schema版本. 无法加载.";
                file.close();
                return false;
            }
            // Allow older file versions to be read using their corresponding schema version
        }
        // --- End Version Check ---

        // --- Column Count Sanity Check ---
        if (header.column_count_in_file != static_cast<uint32_t>(columns.size()))
        {
            qWarning() << funcName << "- 警告: 头信息列数 (" << header.column_count_in_file
                       << ") 与DB schema中的列数不匹配 (" << columns.size() << ").";
            // Decide how to handle this. Abort? Try to proceed?
            // Let's try to proceed but log the warning. Deserialization might fail.
        }
        // --- End Column Count Check ---

        if (header.row_count_in_file == 0)
        {
            qDebug() << funcName << "- 文件头显示文件中有0行. 无数据可读.";
            file.close();
            return true; // Successfully read a file with 0 rows as indicated by header
        }

        qDebug() << funcName << "- 文件头显示" << header.row_count_in_file << "行. 开始数据反序列化循环.";

        QDataStream in(&file);
        in.setByteOrder(QDataStream::LittleEndian); // Ensure consistency

        // 设置更合理的日志记录间隔
        const int LOG_INTERVAL = 5000; // 每5000行记录一次日志
        const QDateTime startTime = QDateTime::currentDateTime();

        quint64 actualRowsRead = 0;                               // Use quint64 to match header type
        rows.reserve(static_cast<int>(header.row_count_in_file)); // QList uses int for size/reserve

        // 用于输出完成百分比的计数器
        int lastReportedPercent = -1;

        while (!in.atEnd() && actualRowsRead < header.row_count_in_file)
        {
            // 保存当前位置，用于跟踪重定位
            qint64 currentPosition = file.pos();

            // Read the size of the next row block
            quint32 rowByteSize;
            in >> rowByteSize;

            if (in.status() != QDataStream::Ok || rowByteSize == 0)
            {
                qWarning() << funcName << "- 错误: 在位置" << file.pos() << "读取行大小失败. 状态:" << in.status();
                break;
            }

            // 检查是否是重定位标记 (0xFFFFFFFF)
            if (rowByteSize == 0xFFFFFFFF)
            {
                // 读取重定位位置
                qint64 redirectPosition;
                in >> redirectPosition;

                if (in.status() != QDataStream::Ok)
                {
                    qWarning() << funcName << "- 错误: 在位置" << file.pos() << "读取重定向位置失败. 状态:" << in.status();
                    break;
                }

                // 只在低频率输出重定向日志
                if (actualRowsRead % LOG_INTERVAL == 0)
                {
                    qDebug() << funcName << "- 检测到重定位指针，从位置 " << currentPosition << " 重定向到位置 " << redirectPosition;
                }

                // 保存当前位置，在处理完重定向后需要返回
                qint64 returnPosition = file.pos();

                // 跳转到重定位位置
                if (!file.seek(redirectPosition))
                {
                    qWarning() << funcName << "- 无法跳转到重定位位置 " << redirectPosition;
                    break;
                }

                // 读取重定位位置的行大小
                in >> rowByteSize;

                if (in.status() != QDataStream::Ok || rowByteSize == 0)
                {
                    qWarning() << funcName << "- 错误: 在重定向位置" << redirectPosition << "读取行大小失败. 状态:" << in.status();
                    break;
                }

                // 如果重定位位置又是一个重定位指针，这是错误的（避免循环引用）
                if (rowByteSize == 0xFFFFFFFF)
                {
                    qWarning() << funcName << "- 错误：重定位指针指向另一个重定位指针，可能存在循环引用";
                    break;
                }

                // 读取并处理重定位位置的数据
                QByteArray rowBytes = file.read(rowByteSize);
                if (rowBytes.size() != static_cast<int>(rowByteSize))
                {
                    qWarning() << funcName << "- 错误: 在重定向位置读取行数据失败. 预期:" << rowByteSize << "实际:" << rowBytes.size();
                    break;
                }

                // 反序列化重定位位置的行数据
                Vector::RowData rowData;
                if (deserializeRow(rowBytes, columns, header.data_schema_version, rowData))
                {
                    rows.append(rowData);
                    actualRowsRead++;

                    // 计算和显示进度
                    int percent = static_cast<int>((actualRowsRead * 100) / header.row_count_in_file);
                    if (percent != lastReportedPercent && (actualRowsRead % LOG_INTERVAL == 0 || percent - lastReportedPercent >= 10))
                    {
                        QDateTime currentTime = QDateTime::currentDateTime();
                        qint64 elapsedMs = startTime.msecsTo(currentTime);
                        double rowsPerSecond = (elapsedMs > 0) ? (actualRowsRead * 1000.0 / elapsedMs) : 0;
                        qDebug() << funcName << "- 进度:" << actualRowsRead << "行读取 ("
                                 << percent << "%), 速度:" << rowsPerSecond << "行/秒";
                        lastReportedPercent = percent;
                    }
                }
                else
                {
                    qWarning() << funcName << "- 在重定向位置反序列化行失败. 继续下一行.";
                }

                // 返回原位置继续处理后续行
                if (!file.seek(returnPosition))
                {
                    qWarning() << funcName << "- 无法返回原位置 " << returnPosition << " 继续处理";
                    break;
                }

                // 继续下一行的处理，跳过当前行的读取逻辑
                continue;
            }

            // Read the actual bytes for this row
            QByteArray rowBytes = file.read(rowByteSize);
            if (rowBytes.size() != static_cast<int>(rowByteSize))
            {
                qWarning() << funcName << "- 错误: 读取行数据失败. 预期:" << rowByteSize << "实际:" << rowBytes.size();
                break;
            }

            // Deserialize the row bytes into a row data object
            Vector::RowData rowData;
            if (deserializeRow(rowBytes, columns, header.data_schema_version, rowData))
            {
                rows.append(rowData);
                actualRowsRead++;

                // 计算和显示进度
                int percent = static_cast<int>((actualRowsRead * 100) / header.row_count_in_file);
                if (percent != lastReportedPercent && (actualRowsRead % LOG_INTERVAL == 0 || percent - lastReportedPercent >= 10))
                {
                    QDateTime currentTime = QDateTime::currentDateTime();
                    qint64 elapsedMs = startTime.msecsTo(currentTime);
                    double rowsPerSecond = (elapsedMs > 0) ? (actualRowsRead * 1000.0 / elapsedMs) : 0;
                    qDebug() << funcName << "- 进度:" << actualRowsRead << "行读取 ("
                             << percent << "%), 速度:" << rowsPerSecond << "行/秒";
                    lastReportedPercent = percent;
                }
            }
            else
            {
                qWarning() << funcName << "- 反序列化行失败，位置:" << actualRowsRead + 1 << ". 继续下一行.";
                // Option: We could break here to abort on first error, but continuing allows for more resilience
            }
        }

        file.close();

        // 计算并显示总体处理统计信息
        QDateTime endTime = QDateTime::currentDateTime();
        qint64 totalElapsedMs = startTime.msecsTo(endTime);
        double avgRowsPerSecond = (totalElapsedMs > 0) ? (actualRowsRead * 1000.0 / totalElapsedMs) : 0;

        qDebug() << funcName << "- 文件已关闭. 总计读取:" << rows.size() << "行, 预期:" << header.row_count_in_file
                 << "行. 耗时:" << (totalElapsedMs / 1000.0) << "秒, 平均速度:" << avgRowsPerSecond << "行/秒";

        if (actualRowsRead != header.row_count_in_file)
        {
            qWarning() << funcName << "- 警告: 实际读取行数 (" << actualRowsRead << ") 与文件头行数不匹配 (" << header.row_count_in_file << ")";
        }

        return !rows.isEmpty() || header.row_count_in_file == 0; // Consider empty file as success if header indicates 0 rows
    }

    bool BinaryFileHelper::writeAllRowsToBinary(const QString &binFilePath, const QList<Vector::ColumnInfo> &columns,
                                                int schemaVersion, const QList<Vector::RowData> &rows)
    {
        const QString funcName = "BinaryFileHelper::writeAllRowsToBinary";
        // 减少日志输出
        qDebug() << funcName << "- 开始写入文件:" << binFilePath << ", 行数:" << rows.size() << ", 使用固定长度: true";

        QFile file(binFilePath);
        if (!file.open(QIODevice::WriteOnly))
        {
            qWarning() << funcName << "- 无法打开文件进行写入:" << binFilePath << ", 错误:" << file.errorString();
            return false;
        }

        // 准备并写入文件头
        BinaryFileHeader header;
        header.magic_number = VEC_BINDATA_MAGIC;
        header.file_format_version = CURRENT_FILE_FORMAT_VERSION;
        header.data_schema_version = schemaVersion;
        header.row_count_in_file = rows.size();
        header.column_count_in_file = columns.size();
        header.timestamp_created = QDateTime::currentSecsSinceEpoch();
        header.timestamp_updated = header.timestamp_created;
        header.compression_type = 0;
        std::memset(header.reserved_bytes, 0, sizeof(header.reserved_bytes));

        if (!writeBinaryHeader(&file, header))
        {
            qWarning() << funcName << "- 写入文件头失败";
            file.close();
            return false;
        }

        quint64 rowsWritten = 0;
        QDataStream out(&file);
        out.setByteOrder(QDataStream::LittleEndian);

        // 设置更大的进度日志间隔
        const int LOG_INTERVAL = 5000;
        const QDateTime startTime = QDateTime::currentDateTime();
        int lastReportedPercent = -1;

        for (const auto &rowData : rows)
        {
            QByteArray serializedRow;
            if (serializeRow(rowData, columns, serializedRow))
            {
                // 写入行数据大小
                out << static_cast<quint32>(serializedRow.size());

                // 写入行数据
                if (file.write(serializedRow) == serializedRow.size())
                {
                    rowsWritten++;

                    // 优化日志频率，使用百分比进度和时间统计
                    if (rows.size() > LOG_INTERVAL)
                    {
                        int percent = static_cast<int>((rowsWritten * 100) / rows.size());
                        if (percent != lastReportedPercent && (rowsWritten % LOG_INTERVAL == 0 || percent - lastReportedPercent >= 10))
                        {
                            QDateTime currentTime = QDateTime::currentDateTime();
                            qint64 elapsedMs = startTime.msecsTo(currentTime);
                            double rowsPerSecond = (elapsedMs > 0) ? (rowsWritten * 1000.0 / elapsedMs) : 0;

                            qDebug() << funcName << "- 进度:" << rowsWritten << "/"
                                     << rows.size() << "行已写入 (" << percent << "%), 速度:"
                                     << rowsPerSecond << "行/秒";
                            lastReportedPercent = percent;
                        }
                    }
                }
                else
                {
                    qWarning() << funcName << "- 写入行数据失败，位置:" << rowsWritten + 1;
                    break;
                }
            }
            else
            {
                qWarning() << funcName << "- 序列化行数据失败，位置:" << rowsWritten + 1;
                break;
            }
        }

        // 说明: 成功写入所有行（或部分行）
        header.row_count_in_file = static_cast<unsigned int>(rows.size());
        bool writeHeaderSuccess = writeBinaryHeader(&file, header);
        if (!writeHeaderSuccess)
        {
            qWarning() << funcName << "- 警告: 写入文件头失败! 文件:" << binFilePath;
            // 继续执行, 因为数据已经写入了
        }

        file.close();

        // 全量重写会改变文件结构，需要清除行偏移缓存
        clearRowOffsetCache(binFilePath);

        qDebug() << funcName << "- 文件已关闭. 总计成功写入 " << rowsWritten << " 行.";
        if (rowsWritten != rows.size())
        {
            qWarning() << funcName << "- 部分行写入失败! 预期写入:" << rows.size()
                       << ", 实际写入:" << rowsWritten;
        }

        return rowsWritten == static_cast<quint64>(rows.size());
    }

    bool BinaryFileHelper::updateRowsInBinary(const QString &binFilePath, const QList<Vector::ColumnInfo> &columns,
                                              int schemaVersion, const QMap<int, Vector::RowData> &modifiedRows)
    {
        const char *funcName = "BinaryFileHelper::updateRowsInBinary"; // Use char* for BFH_LOG_STDERR
        BFH_LOG_STDERR(funcName, "ENTERED. File: %s, Modified rows: %d, SchemaVersion: %d",
                       binFilePath.toStdString().c_str(), modifiedRows.size(), schemaVersion);
        // Original qDebug:
        // qDebug() << funcName << "- 开始更新文件:" << binFilePath << ", 修改行数:" << modifiedRows.size() << ", 使用随机写入";

        // 添加用于跟踪损坏行的数据结构
        QSet<int> corruptedRows;

        if (modifiedRows.isEmpty())
        {
            BFH_LOG_STDERR(funcName, "No rows to modify. Exiting true.");
            // Original qDebug:
            // qDebug() << funcName << "- 没有需要修改的行，直接返回 true";
            return true;
        }

        QFile file(binFilePath);
        if (!file.exists())
        {
            BFH_LOG_STDERR(funcName, "CRITICAL_ERROR: File does not exist, cannot update: %s. Exiting false.", binFilePath.toStdString().c_str());
            // Original qCritical:
            // qCritical() << funcName << "- CRITICAL_ERROR: 二进制文件不存在，无法更新: " << binFilePath << " (Reason: File does not exist)";
            return false;
        }
        BFH_LOG_STDERR(funcName, "File exists: %s", binFilePath.toStdString().c_str());

        if (!file.open(QIODevice::ReadWrite))
        {
            BFH_LOG_STDERR(funcName, "CRITICAL_ERROR: Failed to open file in ReadWrite mode: %s, OS error: %s. Exiting false.",
                           binFilePath.toStdString().c_str(), file.errorString().toStdString().c_str());
            // Original qCritical:
            // qCritical() << funcName << "- CRITICAL_ERROR: 无法以读写模式打开文件: " << binFilePath << ", OS错误:" << file.errorString() << " (Reason: Failed to open file in ReadWrite mode)";
            return false;
        }
        BFH_LOG_STDERR(funcName, "File opened successfully in ReadWrite mode: %s", binFilePath.toStdString().c_str());
        // Original qDebug:
        // qDebug() << funcName << "- 文件已成功以读写模式打开:" << binFilePath;

        BinaryFileHeader header;
        // Note: readBinaryHeader itself will be instrumented later to use BFH_LOG_STDERR
        if (!readBinaryHeader(&file, header)) // Assuming readBinaryHeader does its own BFH_LOG_STDERR on failure path
        {
            BFH_LOG_STDERR(funcName, "CRITICAL_ERROR: readBinaryHeader failed for file: %s. Exiting false (logged by readBinaryHeader).", binFilePath.toStdString().c_str());
            // Original qCritical:
            // qCritical() << funcName << "- CRITICAL_ERROR: 读取或验证二进制文件头失败 (Reason: readBinaryHeader failed), 文件:" << binFilePath;
            file.close();
            return false;
        }
        BFH_LOG_STDERR(funcName, "Header read successfully. FileVer: %u, SchemaVer: %d, RowCount: %llu",
                       header.file_format_version, header.data_schema_version, header.row_count_in_file);
        // Original qDebug:
        // qDebug() << funcName << "- 文件头读取成功. 文件版本:" << header.file_format_version << "Schema版本:" << header.data_schema_version << "行数:" << header.row_count_in_file;

        if (header.data_schema_version != schemaVersion)
        {
            BFH_LOG_STDERR(funcName, "CRITICAL_ERROR: Schema version mismatch. File has %d, expected %d. File: %s. Exiting false.",
                           header.data_schema_version, schemaVersion, binFilePath.toStdString().c_str());
            // Original qCritical:
            // qCritical() << funcName << "- CRITICAL_ERROR: 文件数据schema版本(" << header.data_schema_version
            //            << ")与期望的DB schema版本(" << schemaVersion << ")不匹配, 文件:" << binFilePath << " (Reason: Schema version mismatch)";
            file.close();
            return false;
        }

        if (header.column_count_in_file != static_cast<uint32_t>(columns.size()))
        {
            BFH_LOG_STDERR(funcName, "CRITICAL_ERROR: Column count mismatch. File has %u, expected %zu. File: %s. Exiting false.",
                           header.column_count_in_file, static_cast<size_t>(columns.size()), binFilePath.toStdString().c_str());
            // Original qCritical:
            // qCritical() << funcName << "- CRITICAL_ERROR: 文件头列数(" << header.column_count_in_file
            //            << ")与期望的列数(" << columns.size() << ")不匹配, 文件:" << binFilePath << " (Reason: Column count mismatch)";
            file.close();
            return false;
        }
        BFH_LOG_STDERR(funcName, "Schema version and column count checks passed.");
        // Original qDebug:
        // qDebug() << funcName << "- Schema版本和列数检查通过.";

        struct RowInfo
        {
            qint64 offset;
            quint32 data_size;
            quint32 total_size;
        };

        QVector<RowInfo> rowPositions;
        if (header.row_count_in_file > 0)
        {
            rowPositions.reserve(header.row_count_in_file);
        }

        QDataStream in(&file);
        in.setByteOrder(QDataStream::LittleEndian);

        // 修正点1: 使用 sizeof(BinaryFileHeader)
        qint64 initialDataPos = sizeof(BinaryFileHeader);
        if (!file.seek(initialDataPos))
        {
            qCritical() << funcName << "- CRITICAL_ERROR: Seek到初始数据位置(" << initialDataPos << ")失败. 文件:" << binFilePath
                        << ", OS错误:" << file.errorString() << " (Reason: Initial file seek failed)";
            file.close();
            return false;
        }
        qDebug() << funcName << "- 定位到初始数据位置:" << initialDataPos;

        // 首先，遍历文件一次，记录所有行的原始偏移量和大小
        QList<RowInfo> rowOffsetsAndSizes;
        rowOffsetsAndSizes.reserve(header.row_count_in_file);

        for (quint64 i = 0; i < header.row_count_in_file; ++i)
        {
            qint64 currentRowOffset = file.pos(); // Position before reading row size
            quint32 rowDataSize;

            // --- BEGIN ENHANCED CHECKS ---
            if (file.atEnd())
            {
                qCritical() << funcName << "- CRITICAL_ERROR: FAILED_INCREMENTAL_UPDATE_CONDITION: Attempting to read size for row" << i
                            << "but already at EOF. File pos:" << file.pos() << "Header row count:" << header.row_count_in_file
                            << "File size:" << file.size() << ". File:" << binFilePath
                            << " (Reason: EOF before reading row size, file likely truncated or header.row_count_in_file inconsistent)";
                file.close();
                return false;
            }
            if (file.bytesAvailable() < static_cast<qint64>(sizeof(quint32)))
            {
                qCritical() << funcName << "- CRITICAL_ERROR: FAILED_INCREMENTAL_UPDATE_CONDITION: Not enough bytes (" << file.bytesAvailable()
                            << ") to read size (quint32) for row" << i << ". File pos:" << file.pos()
                            << "File size:" << file.size() << ". File:" << binFilePath
                            << " (Reason: Insufficient bytes for row size, file likely truncated or header.row_count_in_file inconsistent)";
                file.close();
                return false;
            }
            // --- END ENHANCED CHECKS ---

            in >> rowDataSize; // Read the size of the current row's data
            if (in.status() != QDataStream::Ok)
            {
                qCritical() << funcName << "- CRITICAL_ERROR: FAILED_INCREMENTAL_UPDATE_CONDITION: 读取行" << i << "的数据大小时发生流错误. Stream status:" << in.status()
                            << "(1 means ReadPastEnd). File pos before read attempt:" << currentRowOffset
                            << ". Bytes available before read attempt:" << (file.size() - currentRowOffset)
                            << ". File size:" << file.size()
                            << ". File:" << binFilePath
                            << " (Reason: QDataStream error reading row data size)";
                file.close();
                return false;
            }

            // --- BEGIN ABNORMAL SIZE CHECK ---
            // 检查行大小是否异常（例如16MB的错误值）
            // 设置一个合理的最大行大小限制，例如1MB
            const quint32 MAX_REASONABLE_ROW_SIZE = 1024 * 1024; // 1MB
            if (rowDataSize > MAX_REASONABLE_ROW_SIZE)
            {
                qWarning() << funcName << "- WARNING: Row" << i << "claims an abnormally large data size:" << rowDataSize
                           << "bytes, which exceeds the reasonable limit of" << MAX_REASONABLE_ROW_SIZE
                           << "bytes. This is likely due to file corruption. Will attempt to recover.";

                // 尝试查看实际可用数据量
                qint64 bytesAvailable = file.size() - file.pos();

                // 计算更合理的行大小，基于最大行大小限制和可用数据量
                quint32 adjustedSize = qMin(static_cast<quint32>(bytesAvailable), MAX_REASONABLE_ROW_SIZE);

                if (adjustedSize > 0)
                {
                    qWarning() << funcName << "- Adjusting row" << i << "size from" << rowDataSize << "to" << adjustedSize
                               << "bytes based on available data and reasonable limits.";
                    rowDataSize = adjustedSize;

                    // 记录这一行后续需要特殊处理
                    // 这里可以添加一个标记，如将该行加入到一个"需要完全重写"的列表中
                    corruptedRows.insert(i);
                }
                else
                {
                    qCritical() << funcName << "- CRITICAL_ERROR: FAILED_INCREMENTAL_UPDATE_CONDITION: No reasonable size could be determined for row" << i
                                << ". File pos:" << file.pos() << "File size:" << file.size()
                                << ". File:" << binFilePath;
                    file.close();
                    return false;
                }
            }
            // --- END ABNORMAL SIZE CHECK ---

            // --- BEGIN POST-READ CHECK ---
            if (file.bytesAvailable() < static_cast<qint64>(rowDataSize))
            {
                qCritical() << funcName << "- CRITICAL_ERROR: FAILED_INCREMENTAL_UPDATE_CONDITION: Row" << i << "claims data size" << rowDataSize
                            << "but only" << file.bytesAvailable() << "bytes remain in file after reading its size. File pos:" << file.pos()
                            << "File size:" << file.size() << ". File:" << binFilePath
                            << " (Reason: Insufficient bytes for claimed row data, file likely truncated or row size value corrupt)";
                file.close();
                return false;
            }
            // --- END POST-READ CHECK ---

            // Correctly create and append RowInfo object
            RowInfo currentRowDetails;
            currentRowDetails.offset = currentRowOffset;
            currentRowDetails.data_size = rowDataSize;
            currentRowDetails.total_size = sizeof(quint32) + rowDataSize; // Size of data + size of the size field itself
            rowOffsetsAndSizes.append(currentRowDetails);

            // Seek past this row's data (quint32 for size + data itself) to get to the next row's size marker
            qint64 nextRowPos = currentRowOffset + sizeof(quint32) + rowDataSize;
            if (nextRowPos > file.size() && i < header.row_count_in_file - 1)
            { // Check if seek would go past EOF unless it's the very last row
                qCritical() << funcName << "- CRITICAL_ERROR: FAILED_INCREMENTAL_UPDATE_CONDITION: Calculated seek offset" << nextRowPos
                            << "for row" << i << "exceeds file size" << file.size() << ". File:" << binFilePath
                            << " (Reason: Corrupt row size or truncated file before seeking past row data)";
                file.close();
                return false;
            }
            if (!file.seek(nextRowPos))
            {
                qCritical() << funcName << "- CRITICAL_ERROR: FAILED_INCREMENTAL_UPDATE_CONDITION: Failed to seek past row" << i << "data. Current pos:" << file.pos()
                            << "Target offset:" << nextRowPos << ". File size:" << file.size()
                            << ". File error:" << file.errorString() << ". File:" << binFilePath;
                file.close();
                return false;
            }
        }
        qDebug() << funcName << "- 所有" << header.row_count_in_file << "行的位置信息读取完毕.";

        int maxRowIndexInFile = (header.row_count_in_file > 0) ? (static_cast<int>(header.row_count_in_file) - 1) : -1;
        QList<int> invalidRowIndicesForUpdate;
        for (auto it = modifiedRows.constBegin(); it != modifiedRows.constEnd(); ++it)
        {
            int rowIndex = it.key();
            if (rowIndex < 0 || rowIndex > maxRowIndexInFile)
            {
                qWarning() << funcName << "- 警告: 请求更新的行索引" << rowIndex << "超出文件范围 [0," << maxRowIndexInFile << "]. 该行将被忽略.";
                invalidRowIndicesForUpdate.append(rowIndex);
            }
        }

        if (!invalidRowIndicesForUpdate.isEmpty() && invalidRowIndicesForUpdate.size() == modifiedRows.size())
        {
            qCritical() << funcName << "- CRITICAL_ERROR: 所有请求修改的行索引都无效或超出文件范围. 文件:" << binFilePath << " (Reason: All modified row indices are invalid)";
            file.close();
            return false;
        }
        qDebug() << funcName << "- 行索引范围检查完成. 无效待更新行数:" << invalidRowIndicesForUpdate.size();

        quint32 rowsUpdatedSuccessfully = 0;

        for (auto it = modifiedRows.constBegin(); it != modifiedRows.constEnd(); ++it)
        {
            int rowIndex = it.key();
            const Vector::RowData &newRowData = it.value();

            if (invalidRowIndicesForUpdate.contains(rowIndex))
            {
                qDebug() << funcName << "- 跳过更新无效/越界行索引:" << rowIndex;
                continue;
            }

            // 检查该行是否已被标记为损坏
            if (corruptedRows.contains(rowIndex))
            {
                qWarning() << funcName << "- 跳过更新损坏的行:" << rowIndex << "。该行将在后续完全重写中处理";
                continue;
            }

            if (rowIndex < 0 || rowIndex >= rowOffsetsAndSizes.size())
            {
                qCritical() << funcName << "- CRITICAL_ERROR: (Defensive Check) 行索引" << rowIndex << "在rowOffsetsAndSizes中越界 (Size: " << rowOffsetsAndSizes.size() << "). 文件:" << binFilePath
                            << " (Reason: Row index out of bounds for rowOffsetsAndSizes during update loop)";
                BFH_LOG_STDERR(funcName, "CRITICAL_ERROR: Row index %d out of bounds. rowOffsetsAndSizes.size = %d",
                               rowIndex, rowOffsetsAndSizes.size());
                file.close();
                return false;
            }

            const RowInfo &originalRowInfo = rowOffsetsAndSizes[rowIndex];

            BFH_LOG_STDERR(funcName, "Processing row %d. Offset: %lld, Data Size: %u",
                           rowIndex, originalRowInfo.offset, originalRowInfo.data_size);

            QByteArray serializedNewRow;
            if (!serializeRow(newRowData, columns, serializedNewRow))
            {
                qCritical() << funcName << "- CRITICAL_ERROR: 序列化新行数据失败，行索引:" << rowIndex << ", 文件:" << binFilePath
                            << " (Reason: serializeRow returned false)";
                BFH_LOG_STDERR(funcName, "CRITICAL_ERROR: Failed to serialize row %d", rowIndex);
                file.close();
                return false;
            }

            if (static_cast<quint32>(serializedNewRow.size()) != originalRowInfo.data_size)
            {
                // 记录大小不匹配详情但不立即失败，而是记录问题并继续
                qWarning() << funcName << "- 警告: 行" << rowIndex << "的新数据长度(" << serializedNewRow.size()
                           << ")与原始数据长度(" << originalRowInfo.data_size << ")不匹配. 文件:" << binFilePath;
                BFH_LOG_STDERR(funcName, "WARNING: Row %d data length mismatch. New size: %d, Original size: %u",
                               rowIndex, serializedNewRow.size(), originalRowInfo.data_size);

                // 记录不匹配数据的内容摘要，帮助调试
                QString originalDataSummary = "未读取原始数据";
                QString newDataSummary = QString("新数据前32字节: %1").arg(QString(serializedNewRow.size() > 32 ? serializedNewRow.left(32).toHex(' ') : serializedNewRow.toHex(' ')));

                BFH_LOG_STDERR(funcName, "Data mismatch details - %s | %s",
                               originalDataSummary.toStdString().c_str(),
                               newDataSummary.toStdString().c_str());

                // 标记这行更新失败，但允许继续处理其他行
                // 稍后可能会触发完全重写模式
                continue;
            }

            qint64 seekPos = originalRowInfo.offset + sizeof(quint32);
            if (!file.seek(seekPos))
            {
                qCritical() << funcName << "- CRITICAL_ERROR: Seek到行" << rowIndex << "的数据位置(" << seekPos << ")失败. 文件:" << binFilePath
                            << ", OS错误:" << file.errorString() << " (Reason: File seek failed for row update)";
                BFH_LOG_STDERR(funcName, "CRITICAL_ERROR: Failed to seek to row %d data position %lld. OS error: %s",
                               rowIndex, seekPos, file.errorString().toStdString().c_str());
                file.close();
                return false;
            }

            qint64 bytesWritten = file.write(serializedNewRow);
            if (bytesWritten != serializedNewRow.size())
            {
                qCritical() << funcName << "- CRITICAL_ERROR: 写入新行数据失败，行索引:" << rowIndex << ". 预期写入:" << serializedNewRow.size()
                            << ", 实际写入:" << bytesWritten << ". 文件:" << binFilePath
                            << ", OS错误:" << file.errorString() << " (Reason: File write failed for row update)";
                BFH_LOG_STDERR(funcName, "CRITICAL_ERROR: Failed to write row %d data. Expected: %d, Actual: %lld. OS error: %s",
                               rowIndex, serializedNewRow.size(), bytesWritten, file.errorString().toStdString().c_str());
                file.close();
                return false;
            }
            BFH_LOG_STDERR(funcName, "Successfully updated row %d at offset %lld, data size: %d",
                           rowIndex, originalRowInfo.offset, serializedNewRow.size());
            qDebug() << funcName << "- 成功更新行:" << rowIndex << "于Offset:" << originalRowInfo.offset << "新数据长度:" << serializedNewRow.size();
            rowsUpdatedSuccessfully++;
        }

        if (rowsUpdatedSuccessfully > 0)
        {
            header.timestamp_updated = QDateTime::currentSecsSinceEpoch();
            if (!file.seek(0))
            {
                qCritical() << funcName << "- CRITICAL_ERROR: Seek到文件开头失败 (更新文件头前). 文件:" << binFilePath
                            << ", OS错误:" << file.errorString() << " (Reason: File seek to beginning failed for header timestamp update)";
                BFH_LOG_STDERR(funcName, "CRITICAL_ERROR: Failed to seek to file beginning for header update. OS error: %s",
                               file.errorString().toStdString().c_str());
                file.close();
                return false;
            }
            if (!writeBinaryHeader(&file, header))
            {
                qCritical() << funcName << "- CRITICAL_ERROR: 更新文件头的时间戳失败 (Reason: writeBinaryHeader for timestamp update failed), 文件:" << binFilePath;
                BFH_LOG_STDERR(funcName, "CRITICAL_ERROR: Failed to update file header timestamp");
                file.close();
                return false;
            }
            BFH_LOG_STDERR(funcName, "Successfully updated file header timestamp");
            qDebug() << funcName << "- 文件头时间戳已成功更新.";
        }
        else if (modifiedRows.size() > invalidRowIndicesForUpdate.size())
        {
            qWarning() << funcName << "- 警告: 有 " << (modifiedRows.size() - invalidRowIndicesForUpdate.size())
                       << " 个有效修改行，但成功更新数量为0. 可能所有有效行都因某种原因（如长度不匹配）未能更新.";
            BFH_LOG_STDERR(funcName, "WARNING: Found %d valid rows to modify, but updated 0 rows successfully",
                           (modifiedRows.size() - invalidRowIndicesForUpdate.size()));
            if ((modifiedRows.size() - invalidRowIndicesForUpdate.size()) > 0 && rowsUpdatedSuccessfully == 0)
            {
                qCritical() << funcName << "- CRITICAL_ERROR: 有效修改行存在，但无任何行被成功更新。上层应已处理此情况。";
                BFH_LOG_STDERR(funcName, "CRITICAL_ERROR: Valid rows exist but none were updated successfully");
                file.close();
                return false;
            }
        }

        file.close();
        BFH_LOG_STDERR(funcName, "File closed. Successfully updated %u out of %d rows",
                       rowsUpdatedSuccessfully, (modifiedRows.size() - invalidRowIndicesForUpdate.size()));
        qDebug() << funcName << "- 文件已关闭. 总计成功更新 " << rowsUpdatedSuccessfully << " 行.";

        // 更新数据后清除缓存，确保下次读取时获取最新结构
        if (rowsUpdatedSuccessfully > 0)
        {
            clearRowOffsetCache(binFilePath);
        }

        // 计算成功更新率
        int validRowCount = modifiedRows.size() - invalidRowIndicesForUpdate.size();
        int attemptedRowCount = validRowCount - corruptedRows.size();

        // 如果有损坏的行，记录到日志，但不影响返回值（只要其他行成功更新）
        if (!corruptedRows.isEmpty())
        {
            qWarning() << funcName << "- 检测到" << corruptedRows.size() << "个损坏的行，这些行将在下次完全重写时更新";
            BFH_LOG_STDERR(funcName, "Detected %d corrupted rows that will be updated during a full rewrite later",
                           corruptedRows.size());

            // 只要有一些行成功更新，我们就认为操作成功（即使有一些损坏的行）
            if (rowsUpdatedSuccessfully > 0)
            {
                return true;
            }
        }

        // 原始逻辑：所有有效行都必须成功更新
        return rowsUpdatedSuccessfully == static_cast<quint32>(validRowCount);
    }

    bool BinaryFileHelper::readRowSizeWithValidation(QDataStream &stream, QFile &file, quint32 maxReasonableSize, quint32 &rowSize)
    {
        const char *funcName = "BinaryFileHelper::readRowSizeWithValidation";

        // 检查是否有足够的字节可读
        if (file.bytesAvailable() < static_cast<qint64>(sizeof(quint32)))
        {
            qCritical() << funcName << "- 错误: 文件剩余字节数(" << file.bytesAvailable()
                        << ")不足以读取行大小(需要4字节)";
            return false;
        }

        // 读取大小值
        stream >> rowSize;

        // 检查流状态
        if (stream.status() != QDataStream::Ok)
        {
            qCritical() << funcName << "- 错误: 读取行大小时流状态异常. 状态:" << stream.status();
            return false;
        }

        // 检查大小是否合理
        if (rowSize > maxReasonableSize)
        {
            qWarning() << funcName << "- 警告: 读取到异常大行大小:" << rowSize
                       << ", 超过最大合理值:" << maxReasonableSize << ". 将进行调整";

            // 根据剩余可用字节计算合理大小
            qint64 availableBytes = file.bytesAvailable();
            if (availableBytes <= 0)
            {
                qCritical() << funcName << "- 错误: 文件无剩余可用字节";
                return false;
            }

            // 限制在剩余字节数和最大合理大小之间
            rowSize = qMin(static_cast<quint32>(availableBytes), maxReasonableSize);
            qWarning() << funcName << "- 已将行大小调整为:" << rowSize << "字节";
        }

        // 最终验证确保大小不超过可用字节数
        if (rowSize > static_cast<quint32>(file.bytesAvailable()))
        {
            qCritical() << funcName << "- 错误: 行大小(" << rowSize
                        << ")超过剩余可用字节数(" << file.bytesAvailable() << ")";
            return false;
        }

        return true;
    }

    bool BinaryFileHelper::robustUpdateRowsInBinary(const QString &binFilePath,
                                                    const QList<Vector::ColumnInfo> &columns,
                                                    int schemaVersion,
                                                    const QMap<int, Vector::RowData> &modifiedRows)
    {
        const char *funcName = "BinaryFileHelper::robustUpdateRowsInBinary";

        // 输出日志，表明正在使用新的健壮版本
        qInfo() << funcName << "- 使用强健版本的增量更新，文件:" << binFilePath
                << "，修改行数:" << modifiedRows.size() << "，Schema版本:" << schemaVersion;

        // 记录每一步的开始时间，用于性能分析
        QElapsedTimer timer;
        timer.start();

        // ----- 第1步: 文件检查 -----

        if (modifiedRows.isEmpty())
        {
            qDebug() << funcName << "- 没有需要修改的行，直接返回成功";
            return true;
        }

        QFile file(binFilePath);

        if (!file.exists())
        {
            qCritical() << funcName << "- 文件不存在:" << binFilePath;
            return false;
        }

        if (!file.open(QIODevice::ReadWrite))
        {
            qCritical() << funcName << "- 无法以读写模式打开文件:" << binFilePath
                        << "，错误:" << file.errorString();
            return false;
        }

        qDebug() << funcName << "- 文件打开成功，耗时:" << timer.elapsed() << "ms";
        timer.restart();

        // ----- 第2步: 读取文件头 -----

        BinaryFileHeader header;
        if (!readBinaryHeader(&file, header))
        {
            qCritical() << funcName << "- 读取文件头失败";
            file.close();
            return false;
        }

        // 验证头信息
        if (header.data_schema_version != schemaVersion)
        {
            qCritical() << funcName << "- 文件Schema版本(" << header.data_schema_version
                        << ")与期望的版本(" << schemaVersion << ")不匹配";
            file.close();
            return false;
        }

        if (header.column_count_in_file != static_cast<uint32_t>(columns.size()))
        {
            qCritical() << funcName << "- 文件列数(" << header.column_count_in_file
                        << ")与期望的列数(" << columns.size() << ")不匹配";
            file.close();
            return false;
        }

        header.timestamp_updated = QDateTime::currentSecsSinceEpoch();
        qDebug() << funcName << "- 文件头验证成功，文件包含" << header.row_count_in_file
                 << "行数据，耗时:" << timer.elapsed() << "ms";
        timer.restart();

        // ----- 第3步: 扫描文件结构，建立行索引映射 -----

        // 建立行索引-> {偏移量, 大小} 的映射
        struct RowPosition
        {
            qint64 offset;    // 行起始位置(包括大小字段)
            quint32 dataSize; // 该行数据的大小(不包括大小字段)
        };

        QVector<RowPosition> rowPositions;
        rowPositions.reserve(header.row_count_in_file);

        // 设置合理的最大行大小限制
        const quint32 MAX_REASONABLE_ROW_SIZE = 1 * 1024 * 1024; // 1MB

        // 跟踪哪些行被跳过(因为损坏或其他问题)
        QSet<int> skippedRows;

        // 优先从持久化索引文件获取行偏移数据（如果没有再尝试内存缓存）
        QFileInfo fileInfo(binFilePath);
        QElapsedTimer cacheTimer;
        cacheTimer.start();

        bool usedCache = false;
        bool usedIndexFile = false;
        bool partialCacheUpdate = false;
        QSet<int> rowsToRescan;

        // 首先尝试从索引文件加载
        RowOffsetCache indexPositions = loadRowOffsetIndex(binFilePath, fileInfo.lastModified());
        if (!indexPositions.isEmpty() && indexPositions.size() == static_cast<int>(header.row_count_in_file))
        {
            qDebug() << funcName << "- 使用索引文件的行偏移数据，跳过文件扫描，耗时:" << cacheTimer.elapsed() << "ms";
            usedCache = true;
            usedIndexFile = true;

            // 检查是否有行需要重新扫描（timestamp=0的行）
            for (int i = 0; i < indexPositions.size(); i++)
            {
                if (indexPositions[i].timestamp == 0)
                {
                    rowsToRescan.insert(i);
                }
            }

            if (!rowsToRescan.isEmpty())
            {
                partialCacheUpdate = true;
                qDebug() << funcName << "- 检测到" << rowsToRescan.size() << "行需要重新扫描";
            }

            // 将索引数据转换为本地数据结构
            for (int i = 0; i < indexPositions.size(); i++)
            {
                RowPosition pos;
                pos.offset = indexPositions[i].offset;
                pos.dataSize = indexPositions[i].dataSize;
                rowPositions.append(pos);
            }

            // 同时更新内存缓存
            setRowOffsetCache(binFilePath, indexPositions);
        }
        else
        {
            // 如果索引文件不可用，则尝试从内存缓存获取
            cacheTimer.restart();
            RowOffsetCache cachedPositions = getRowOffsetCache(binFilePath, fileInfo.lastModified());

            // 如果内存缓存有效且行数与文件头一致，则直接使用缓存
            if (!cachedPositions.isEmpty() && cachedPositions.size() == static_cast<int>(header.row_count_in_file))
            {
                qDebug() << funcName << "- 使用内存缓存的行偏移数据，跳过文件扫描，耗时:" << cacheTimer.elapsed() << "ms";
                usedCache = true;

                // 检查是否有行需要重新扫描（timestamp=0的行）
                for (int i = 0; i < cachedPositions.size(); i++)
                {
                    if (cachedPositions[i].timestamp == 0)
                    {
                        rowsToRescan.insert(i);
                    }
                }

                if (!rowsToRescan.isEmpty())
                {
                    partialCacheUpdate = true;
                    qDebug() << funcName << "- 检测到" << rowsToRescan.size() << "行需要重新扫描";
                }

                // 将缓存数据转换为本地数据结构
                for (int i = 0; i < cachedPositions.size(); i++)
                {
                    RowPosition pos;
                    pos.offset = cachedPositions[i].offset;
                    pos.dataSize = cachedPositions[i].dataSize;
                    rowPositions.append(pos);
                }

                // 写入到索引文件以便下次使用
                if (!usedIndexFile)
                {
                    saveRowOffsetIndex(binFilePath, cachedPositions);
                }
            }
            else
            {
                // 缓存和索引文件都无效，需要扫描文件结构
                qDebug() << funcName << "- 未找到有效缓存或索引文件，执行完整扫描";

                // 优化点：使用多线程并行处理大文件扫描
                if (header.row_count_in_file > 100000) // 只对大文件使用并行优化
                {
                    QElapsedTimer parallelTimer;
                    parallelTimer.start();

                    qDebug() << funcName << "- 使用并行处理扫描大文件，行数:" << header.row_count_in_file;

                    // 确定CPU核心数，设置线程数
                    int threadCount = QThread::idealThreadCount();
                    if (threadCount <= 0)
                        threadCount = 4;                // 默认至少4个线程
                    threadCount = qMin(threadCount, 8); // 限制最大线程数

                    qDebug() << funcName << "- 使用" << threadCount << "个线程并行扫描";

                    // 计算每个线程处理的行数范围
                    quint64 rowsPerThread = header.row_count_in_file / threadCount;
                    quint64 remainingRows = header.row_count_in_file % threadCount;

                    // 使用互斥锁保护共享数据结构
                    QMutex mutex;

                    // 预分配空间，避免并发写入冲突
                    rowPositions.resize(header.row_count_in_file);

                    // 创建线程池
                    QThreadPool threadPool;
                    threadPool.setMaxThreadCount(threadCount);

                    // 记录每个线程扫描的起始位置
                    QVector<qint64> threadStartPositions(threadCount + 1);
                    threadStartPositions[0] = file.pos(); // 文件头后的位置

                    // 获取文件大小，用于估算偏移量
                    qint64 dataSize = file.size() - file.pos();
                    qint64 estimatedBytesPerThread = dataSize / threadCount;

                    // 第一阶段：估算每个线程的起始位置
                    for (int t = 1; t < threadCount; t++)
                    {
                        qint64 estimatedPos = file.pos() + t * estimatedBytesPerThread;
                        // 确保不超过文件大小
                        estimatedPos = qMin(estimatedPos, file.size());
                        threadStartPositions[t] = estimatedPos;
                    }
                    threadStartPositions[threadCount] = file.size();

                    // 第二阶段：找到每个线程起始处的准确行边界
                    if (threadCount > 1)
                    {
                        QVector<QPair<int, qint64>> correctedBoundaries;

                        for (int t = 1; t < threadCount; t++)
                        {
                            // 在估算位置附近寻找行边界
                            qint64 searchPos = threadStartPositions[t];

                            // 创建单独的文件句柄以避免干扰主文件位置
                            QFile boundaryFile(binFilePath);
                            if (!boundaryFile.open(QIODevice::ReadOnly))
                            {
                                qWarning() << funcName << "- 无法打开文件来确定线程边界";
                                continue;
                            }

                            // 从估算位置前移一点开始搜索，确保不会错过边界
                            qint64 backtrackPos = qMax(file.pos(), searchPos - 1024);
                            if (!boundaryFile.seek(backtrackPos))
                            {
                                boundaryFile.close();
                                continue;
                            }

                            QDataStream boundaryStream(&boundaryFile);
                            boundaryStream.setByteOrder(QDataStream::LittleEndian);

                            qint64 foundBoundary = backtrackPos;
                            int rowCount = 0;

                            // 读取并跳过完整的行，直到达到或超过估算位置
                            while (!boundaryFile.atEnd() && boundaryFile.pos() < searchPos + 1024)
                            {
                                quint32 rowSize;
                                qint64 rowStart = boundaryFile.pos();

                                if (readRowSizeWithValidation(boundaryStream, boundaryFile, MAX_REASONABLE_ROW_SIZE, rowSize))
                                {
                                    // 找到有效行边界
                                    if (rowStart >= searchPos)
                                    {
                                        foundBoundary = rowStart;
                                        break;
                                    }

                                    // 跳过这一行的数据
                                    if (!boundaryFile.seek(boundaryFile.pos() + rowSize))
                                    {
                                        break;
                                    }

                                    rowCount++;
                                }
                                else
                                {
                                    // 读取错误，向前移动一些字节再试
                                    if (!boundaryFile.seek(boundaryFile.pos() + 4))
                                    {
                                        break;
                                    }
                                }
                            }

                            correctedBoundaries.append(QPair<int, qint64>(t, foundBoundary));
                            boundaryFile.close();
                        }

                        // 更新线程边界
                        for (const auto &pair : correctedBoundaries)
                        {
                            threadStartPositions[pair.first] = pair.second;
                        }
                    }

                    // 第三阶段：创建并行任务
                    QVector<QFuture<void>> futures;
                    QVector<QVector<RowPosition>> threadResults(threadCount);

                    for (int t = 0; t < threadCount; t++)
                    {
                        qint64 startPos = threadStartPositions[t];
                        qint64 endPos = threadStartPositions[t + 1];

                        // 估算该范围内的行数
                        quint64 estimatedRows = (t == threadCount - 1) ? (rowsPerThread + remainingRows) : rowsPerThread;

                        threadResults[t].reserve(estimatedRows);

                        // 创建并启动任务
                        auto future = QtConcurrent::run([binFilePath, startPos, endPos, estimatedRows, MAX_REASONABLE_ROW_SIZE, t, funcName, &threadResults]()
                                                        {
                            // 为每个线程创建独立的文件句柄
                            QFile threadFile(binFilePath);
                            if (!threadFile.open(QIODevice::ReadOnly)) {
                                qWarning() << funcName << "- 线程" << t << "无法打开文件";
                                return;
                            }
                            
                            if (!threadFile.seek(startPos)) {
                                qWarning() << funcName << "- 线程" << t << "无法定位到起始位置:" << startPos;
                                threadFile.close();
                                return;
                            }
                            
                            QDataStream threadStream(&threadFile);
                            threadStream.setByteOrder(QDataStream::LittleEndian);
                            
                            QVector<RowPosition> &results = threadResults[t];
                            
                            // 扫描直到达到下一个线程的起始位置或文件结束
                            quint64 rowsScanned = 0;
                            while (threadFile.pos() < endPos && !threadFile.atEnd()) {
                                RowPosition pos;
                                pos.offset = threadFile.pos();
                                
                                quint32 rowSize;
                                if (!readRowSizeWithValidation(threadStream, threadFile, MAX_REASONABLE_ROW_SIZE, rowSize)) {
                                    // 读取失败，尝试跳过一些字节
                                    threadFile.seek(threadFile.pos() + 4);
                                    continue;
                                }
                                
                                pos.dataSize = rowSize;
                                results.append(pos);
                                
                                // 跳过行数据
                                if (!threadFile.seek(threadFile.pos() + rowSize)) {
                                    break;
                                }
                                
                                rowsScanned++;
                            }
                            
                            qDebug() << funcName << "- 线程" << t << "扫描完成，处理了" << rowsScanned << "行，从" 
                                     << startPos << "到" << threadFile.pos();
                            
                            threadFile.close(); });

                        futures.append(future);
                    }

                    // 等待所有线程完成
                    for (int t = 0; t < threadCount; t++)
                    {
                        futures[t].waitForFinished();
                    }

                    // 第四阶段：合并结果
                    rowPositions.clear();
                    rowPositions.reserve(header.row_count_in_file);

                    for (int t = 0; t < threadCount; t++)
                    {
                        rowPositions.append(threadResults[t]);
                    }

                    qDebug() << funcName << "- 并行扫描完成，总共找到" << rowPositions.size() << "行，耗时:"
                             << parallelTimer.elapsed() << "ms";

                    // 创建新的缓存
                    RowOffsetCache newCache;
                    newCache.reserve(rowPositions.size());

                    for (int i = 0; i < rowPositions.size(); i++)
                    {
                        RowPositionInfo cachePos;
                        cachePos.offset = rowPositions[i].offset;
                        cachePos.dataSize = rowPositions[i].dataSize;
                        cachePos.timestamp = QDateTime::currentSecsSinceEpoch();
                        newCache.append(cachePos);
                    }

                    // 设置缓存
                    setRowOffsetCache(binFilePath, newCache);

                    // 保存索引文件
                    saveRowOffsetIndex(binFilePath, newCache);
                }
                else
                {
                    // 原有的顺序扫描代码，用于小文件
                    QDataStream in(&file);
                    in.setByteOrder(QDataStream::LittleEndian);

                    // 从文件头后的位置开始扫描
                    qint64 currentPos = file.pos();

                    // 创建新的缓存
                    RowOffsetCache newCache;
                    newCache.reserve(header.row_count_in_file);

                    for (quint64 i = 0; i < header.row_count_in_file; ++i)
                    {
                        if (file.atEnd())
                        {
                            qWarning() << funcName << "- 文件意外结束，行索引:" << i
                                       << "，预期行数:" << header.row_count_in_file;
                            break;
                        }

                        RowPosition pos;
                        pos.offset = currentPos;

                        // 使用健壮的行大小读取方法
                        if (!readRowSizeWithValidation(in, file, MAX_REASONABLE_ROW_SIZE, pos.dataSize))
                        {
                            qWarning() << funcName << "- 行" << i << "的大小读取失败，将跳过该行";
                            skippedRows.insert(i);

                            // 尝试跳到下一个合理位置继续
                            if (i < header.row_count_in_file - 1)
                            {
                                // 估算平均行大小并跳过
                                qint64 avgRowSize = (file.size() - file.pos()) / (header.row_count_in_file - i);
                                qint64 nextPos = file.pos() + qMin<qint64>(avgRowSize, 1024); // 最多跳1KB

                                if (!file.seek(nextPos))
                                {
                                    qCritical() << funcName << "- 无法从损坏的行" << i << "恢复";
                                    break;
                                }

                                currentPos = file.pos();
                                continue;
                            }
                            else
                            {
                                // 最后一行，可以直接退出
                                break;
                            }
                        }

                        // 成功读取行大小，添加到位置映射中
                        rowPositions.append(pos);

                        // 同时添加到新的缓存中
                        RowPositionInfo cachePos;
                        cachePos.offset = pos.offset;
                        cachePos.dataSize = pos.dataSize;
                        cachePos.timestamp = QDateTime::currentSecsSinceEpoch();
                        newCache.append(cachePos);

                        // 计算下一行的位置
                        currentPos = pos.offset + sizeof(quint32) + pos.dataSize;

                        // 跳到下一行
                        if (i < header.row_count_in_file - 1 && !file.seek(currentPos))
                        {
                            qWarning() << funcName << "- 无法跳转到下一行位置，文件可能已损坏";
                            break;
                        }
                    }

                    // 如果成功扫描了所有行，且没有跳过的行，那么缓存数据
                    if (rowPositions.size() == header.row_count_in_file && skippedRows.isEmpty())
                    {
                        // 更新内存缓存
                        setRowOffsetCache(binFilePath, newCache);

                        // 同时保存到索引文件
                        saveRowOffsetIndex(binFilePath, newCache);

                        qDebug() << funcName << "- 将行偏移数据存入缓存，共" << newCache.size() << "行";
                    }
                }
            }
        }

        // 选择性重新扫描被标记的行
        if (partialCacheUpdate && !rowsToRescan.isEmpty() && rowPositions.size() == header.row_count_in_file)
        {
            qDebug() << funcName << "- 开始选择性重新扫描" << rowsToRescan.size() << "行";
            QElapsedTimer rescanTimer;
            rescanTimer.start();

            QDataStream in(&file);
            in.setByteOrder(QDataStream::LittleEndian);

            int successfulRescans = 0;

            foreach (int rowIndex, rowsToRescan)
            {
                if (rowIndex < 0 || rowIndex >= rowPositions.size())
                {
                    continue;
                }

                // 获取当前存储的位置信息，可能不准确
                RowPosition &pos = rowPositions[rowIndex];

                // 尝试定位到该行
                if (!file.seek(pos.offset))
                {
                    qWarning() << funcName << "- 无法定位到行" << rowIndex << "的位置:" << pos.offset;
                    continue;
                }

                // 读取行大小
                quint32 rowSize;
                if (!readRowSizeWithValidation(in, file, MAX_REASONABLE_ROW_SIZE, rowSize))
                {
                    qWarning() << funcName << "- 无法读取行" << rowIndex << "的大小";
                    continue;
                }

                // 更新大小信息
                if (rowSize != pos.dataSize)
                {
                    qDebug() << funcName << "- 行" << rowIndex << "的大小已更新:"
                             << pos.dataSize << " -> " << rowSize;
                    pos.dataSize = rowSize;
                }

                // 更新内存缓存和索引文件中的行信息
                if (s_fileRowOffsetCache.contains(binFilePath) &&
                    rowIndex < s_fileRowOffsetCache[binFilePath].size())
                {
                    s_fileRowOffsetCache[binFilePath][rowIndex].dataSize = rowSize;
                    s_fileRowOffsetCache[binFilePath][rowIndex].timestamp = QDateTime::currentSecsSinceEpoch();
                    successfulRescans++;
                }
            }

            qDebug() << funcName << "- 选择性重新扫描完成，更新了" << successfulRescans << "行，耗时:"
                     << rescanTimer.elapsed() << "ms";

            // 重新保存索引文件
            if (successfulRescans > 0)
            {
                saveRowOffsetIndex(binFilePath, s_fileRowOffsetCache[binFilePath]);
            }
        }

        qint64 scanTime = timer.elapsed();
        qDebug() << funcName << "- 文件结构" << (usedCache ? "从缓存获取" : "扫描完成") << "，成功定位了" << rowPositions.size()
                 << "行，跳过了" << skippedRows.size() << "行，耗时:" << scanTime << "ms";
        timer.restart();

        // ----- 第4步: 准备修改行数据 -----

        // 检查修改行的有效性，过滤出有效的修改行
        QMap<int, Vector::RowData> validModifiedRows;
        QSet<int> invalidRowIndices;

        for (auto it = modifiedRows.constBegin(); it != modifiedRows.constEnd(); ++it)
        {
            int rowIndex = it.key();

            // 有效行的条件:
            // 1. 行索引在有效范围内
            // 2. 该行没有被跳过(损坏)
            if (rowIndex >= 0 && rowIndex < rowPositions.size() && !skippedRows.contains(rowIndex))
            {
                validModifiedRows.insert(rowIndex, it.value());
            }
            else
            {
                invalidRowIndices.insert(rowIndex);
                qWarning() << funcName << "- 跳过无效的修改行:" << rowIndex;
            }
        }

        if (validModifiedRows.isEmpty())
        {
            qCritical() << funcName << "- 没有有效的修改行，更新失败";
            file.close();
            return false;
        }

        qDebug() << funcName << "- 共有" << validModifiedRows.size() << "个有效修改行，"
                 << invalidRowIndices.size() << "个无效修改行";

        // ----- 第5步: 序列化并更新每一行 -----

        // 跟踪成功更新的行数
        int successfulUpdates = 0;

        // 开始逐行更新
        for (auto it = validModifiedRows.constBegin(); it != validModifiedRows.constEnd(); ++it)
        {
            int rowIndex = it.key();
            const Vector::RowData &rowData = it.value();

            // 序列化行数据
            QByteArray serializedRow;
            if (!serializeRow(rowData, columns, serializedRow))
            {
                qWarning() << funcName << "- 行" << rowIndex << "序列化失败，跳过此行";
                continue;
            }

            const RowPosition &pos = rowPositions[rowIndex];

            // 检查大小是否匹配
            if (static_cast<quint32>(serializedRow.size()) != pos.dataSize)
            {
                qWarning() << funcName << "- 行" << rowIndex << "的新数据大小(" << serializedRow.size()
                           << ")与原大小(" << pos.dataSize << ")不匹配，跳过此行";
                continue;
            }

            // 定位到数据开始位置(跳过大小字段)
            qint64 dataOffset = pos.offset + sizeof(quint32);
            if (!file.seek(dataOffset))
            {
                qWarning() << funcName << "- 无法定位到行" << rowIndex << "的数据位置，跳过此行";
                continue;
            }

            // 写入新数据
            qint64 bytesWritten = file.write(serializedRow);
            if (bytesWritten != serializedRow.size())
            {
                qWarning() << funcName << "- 写入行" << rowIndex << "失败，预期:" << serializedRow.size()
                           << "，实际:" << bytesWritten << "，跳过此行";
                continue;
            }

            // 更新成功
            successfulUpdates++;
        }

        qDebug() << funcName << "- 行更新完成，成功:" << successfulUpdates
                 << "行，失败:" << (validModifiedRows.size() - successfulUpdates)
                 << "行，耗时:" << timer.elapsed() << "ms";
        timer.restart();

        // ----- 第6步: 更新文件头中的时间戳 -----

        // 更新文件头的时间戳并写回
        file.seek(0); // 回到文件开头
        if (!writeBinaryHeader(&file, header))
        {
            qWarning() << funcName << "- 更新文件头失败，但数据已修改";
            // 不直接失败，因为数据已经写入
        }

        file.close();
        qDebug() << funcName << "- 文件头更新完成，总耗时:" << (timer.elapsed() + timer.elapsed()) << "ms";

        // ----- 第7步: 更新缓存状态 -----

        // 如果有数据更新，使用智能更新缓存，而不是清除
        if (successfulUpdates > 0)
        {
            // 创建修改行的集合
            QSet<int> modifiedRowIndices;
            for (auto it = validModifiedRows.constBegin(); it != validModifiedRows.constEnd(); ++it)
            {
                modifiedRowIndices.insert(it.key());
            }

            qDebug() << funcName << "- 数据已修改，智能更新" << binFilePath << "的行偏移缓存";
            updateRowOffsetCache(binFilePath, modifiedRowIndices);
        }

        // ----- 第8步: 返回结果 -----

        // 只要有行成功更新就算成功
        bool success = (successfulUpdates > 0);
        if (success)
        {
            qInfo() << funcName << "- 增量更新成功完成，共更新" << successfulUpdates << "行";
        }
        else
        {
            qCritical() << funcName << "- 增量更新失败，没有任何行被成功更新";
        }

        return success;
    }

    // 行偏移缓存相关方法实现
    void BinaryFileHelper::clearAllRowOffsetCaches()
    {
        const QString funcName = "BinaryFileHelper::clearAllRowOffsetCaches";
        QMutexLocker locker(&s_cacheMutex);

        qDebug() << funcName << "- 清除所有文件的行偏移缓存，共" << s_fileRowOffsetCache.size() << "个文件";
        s_fileRowOffsetCache.clear();
    }

    BinaryFileHelper::RowOffsetCache BinaryFileHelper::getRowOffsetCache(const QString &binFilePath, const QDateTime &fileLastModified)
    {
        const QString funcName = "BinaryFileHelper::getRowOffsetCache";
        QMutexLocker locker(&s_cacheMutex);

        // 检查内存缓存是否存在
        if (!s_fileRowOffsetCache.contains(binFilePath))
        {
            // 尝试从索引文件读取
            RowOffsetCache indexCache = loadRowOffsetIndex(binFilePath, fileLastModified);
            if (!indexCache.isEmpty())
            {
                // 如果从索引文件读取成功，将其加入内存缓存
                qDebug() << funcName << "- 从索引文件加载了行偏移数据，共" << indexCache.size() << "行";
                s_fileRowOffsetCache[binFilePath] = indexCache;
                return indexCache;
            }

            return RowOffsetCache();
        }

        // 获取缓存
        const RowOffsetCache &cache = s_fileRowOffsetCache.value(binFilePath);

        // 检查缓存是否为空
        if (cache.isEmpty())
        {
            return RowOffsetCache();
        }

        // 检查文件修改时间是否与缓存创建时间一致
        // 我们使用第一个非零时间戳元素作为整个缓存的创建时间
        quint64 cacheTimestamp = 0;
        for (const auto &entry : cache)
        {
            if (entry.timestamp > 0)
            {
                cacheTimestamp = entry.timestamp;
                break;
            }
        }

        // 如果所有行都是timestamp=0，说明缓存完全失效
        if (cacheTimestamp == 0)
        {
            qDebug() << funcName << "- 缓存中所有行都已标记为过期:" << binFilePath;
            return RowOffsetCache();
        }

        quint64 fileTimestamp = fileLastModified.toSecsSinceEpoch();

        // 如果文件比缓存新，缓存无效
        if (fileTimestamp > cacheTimestamp)
        {
            qDebug() << funcName << "- 文件已更新，缓存已过期:" << binFilePath
                     << "文件时间:" << fileTimestamp << "缓存时间:" << cacheTimestamp;
            return RowOffsetCache();
        }

        // 检查是否有部分行的缓存已过期（timestamp=0）
        bool hasInvalidatedRows = false;
        int invalidCount = 0;
        for (const auto &entry : cache)
        {
            if (entry.timestamp == 0)
            {
                hasInvalidatedRows = true;
                invalidCount++;
            }
        }

        if (hasInvalidatedRows)
        {
            qDebug() << funcName << "- 缓存中有" << invalidCount << "行已标记为过期，需要重新扫描这些行";
            // 返回原始缓存，稍后在扫描过程中会处理这些过期行
        }

        qDebug() << funcName << "- 使用内存缓存的行偏移数据，共" << cache.size() << "行:" << binFilePath;
        return cache;
    }

    void BinaryFileHelper::setRowOffsetCache(const QString &binFilePath, const RowOffsetCache &rowPositions)
    {
        const QString funcName = "BinaryFileHelper::setRowOffsetCache";
        QMutexLocker locker(&s_cacheMutex);

        // 简单的缓存大小限制，防止内存无限增长
        const int MAX_CACHED_FILES = 50;

        // 如果缓存达到上限，移除最旧的一个
        if (s_fileRowOffsetCache.size() >= MAX_CACHED_FILES && !s_fileRowOffsetCache.contains(binFilePath))
        {
            // 找到最旧的缓存项并移除
            QString oldestFile;
            quint64 oldestTimestamp = UINT64_MAX;

            for (auto it = s_fileRowOffsetCache.begin(); it != s_fileRowOffsetCache.end(); ++it)
            {
                if (!it.value().isEmpty() && it.value().first().timestamp < oldestTimestamp)
                {
                    oldestTimestamp = it.value().first().timestamp;
                    oldestFile = it.key();
                }
            }

            if (!oldestFile.isEmpty())
            {
                qDebug() << funcName << "- 缓存已满，移除最旧的缓存项:" << oldestFile;
                s_fileRowOffsetCache.remove(oldestFile);
            }
        }

        // 存储新的缓存
        qDebug() << funcName << "- 添加行偏移缓存，文件:" << binFilePath << "，行数:" << rowPositions.size();
        s_fileRowOffsetCache[binFilePath] = rowPositions;

        // 同时保存到索引文件
        bool indexSaved = saveRowOffsetIndex(binFilePath, rowPositions);
        qDebug() << funcName << "- 索引文件保存" << (indexSaved ? "成功" : "失败");
    }

    QString BinaryFileHelper::getIndexFilePath(const QString &binFilePath)
    {
        return binFilePath + ".index";
    }

    bool BinaryFileHelper::saveRowOffsetIndex(const QString &binFilePath, const RowOffsetCache &rowPositions)
    {
        const QString funcName = "BinaryFileHelper::saveRowOffsetIndex";

        if (rowPositions.isEmpty())
        {
            qDebug() << funcName << "- 空缓存，不保存索引文件";
            return false;
        }

        QString indexFilePath = getIndexFilePath(binFilePath);
        QFile indexFile(indexFilePath);

        if (!indexFile.open(QIODevice::WriteOnly))
        {
            qWarning() << funcName << "- 无法创建索引文件:" << indexFilePath;
            return false;
        }

        QElapsedTimer timer;
        timer.start();

        // 优化：使用缓冲区提高写入性能
        const int BUFFER_SIZE = 1024 * 1024; // 1MB 缓冲区
        QByteArray buffer;
        buffer.reserve(BUFFER_SIZE);

        QDataStream out(&buffer, QIODevice::WriteOnly);
        out.setByteOrder(QDataStream::LittleEndian);

        // 写入魔数和版本
        out << quint32(INDEX_FILE_MAGIC);
        out << quint32(INDEX_FILE_VERSION);

        // 写入数据文件的最后修改时间
        QFileInfo dataFileInfo(binFilePath);
        out << quint64(dataFileInfo.lastModified().toSecsSinceEpoch());

        // 写入行数
        out << quint32(rowPositions.size());

        // 估算每行索引数据的大小
        const int ESTIMATED_BYTES_PER_ROW = 16; // 估计每行需要16字节 (offset 8字节 + dataSize 4字节 + 对齐/预留)
        const int ROWS_PER_FLUSH = BUFFER_SIZE / ESTIMATED_BYTES_PER_ROW;

        // 分批写入行偏移数据
        int rowsWritten = 0;
        int batchCount = 0;

        for (const auto &pos : rowPositions)
        {
            out << pos.offset;
            out << pos.dataSize;
            // 不写入timestamp，节省空间

            rowsWritten++;

            // 当缓冲区接近满时，刷新到文件
            if (rowsWritten % ROWS_PER_FLUSH == 0 || rowsWritten == rowPositions.size())
            {
                indexFile.write(buffer);
                buffer.clear();
                out.device()->seek(0); // 重置缓冲区流位置
                batchCount++;
            }
        }

        // 确保最后的数据被写入
        if (!buffer.isEmpty())
        {
            indexFile.write(buffer);
            batchCount++;
        }

        indexFile.close();

        if (out.status() != QDataStream::Ok)
        {
            qWarning() << funcName << "- 写入索引文件时出错:" << out.status();
            QFile::remove(indexFilePath);
            return false;
        }

        qDebug() << funcName << "- 成功创建索引文件:" << indexFilePath
                 << "，包含" << rowPositions.size() << "行索引"
                 << "，共" << batchCount << "批次，耗时:" << timer.elapsed() << "ms";
        return true;
    }

    BinaryFileHelper::RowOffsetCache BinaryFileHelper::loadRowOffsetIndex(const QString &binFilePath, const QDateTime &fileLastModified)
    {
        const QString funcName = "BinaryFileHelper::loadRowOffsetIndex";
        RowOffsetCache cache;

        QString indexFilePath = getIndexFilePath(binFilePath);
        QFile indexFile(indexFilePath);

        if (!indexFile.exists())
        {
            qDebug() << funcName << "- 索引文件不存在:" << indexFilePath;
            return cache;
        }

        if (!indexFile.open(QIODevice::ReadOnly))
        {
            qWarning() << funcName << "- 无法打开索引文件:" << indexFilePath;
            return cache;
        }

        QElapsedTimer timer;
        timer.start();

        // 获取文件大小
        qint64 fileSize = indexFile.size();
        if (fileSize < 20) // 至少需要头部信息
        {
            qWarning() << funcName << "- 索引文件太小，可能已损坏:" << indexFilePath;
            return cache;
        }

        // 优化：对大文件使用内存映射方式读取
        const qint64 MMAP_THRESHOLD = 10 * 1024 * 1024; // 10MB以上使用内存映射
        bool useMMap = (fileSize > MMAP_THRESHOLD);

        if (useMMap)
        {
            qDebug() << funcName << "- 使用内存映射方式读取大索引文件:" << fileSize / (1024 * 1024) << "MB";

            uchar *mappedData = indexFile.map(0, fileSize);
            if (!mappedData)
            {
                qWarning() << funcName << "- 内存映射失败，回退到常规读取";
                useMMap = false;
            }
            else
            {
                // 使用内存映射数据创建数据流
                QByteArray mappedByteArray = QByteArray::fromRawData(reinterpret_cast<char *>(mappedData), fileSize);
                QDataStream in(mappedByteArray);
                in.setByteOrder(QDataStream::LittleEndian);

                // 读取并验证魔数和版本
                quint32 magic, version;
                in >> magic;
                in >> version;

                if (magic != INDEX_FILE_MAGIC || version > INDEX_FILE_VERSION)
                {
                    qWarning() << funcName << "- 索引文件格式不兼容，魔数:" << magic << "，版本:" << version;
                    indexFile.unmap(mappedData);
                    return cache;
                }

                // 读取数据文件的最后修改时间并验证
                quint64 indexedFileTimestamp;
                in >> indexedFileTimestamp;

                quint64 actualFileTimestamp = fileLastModified.toSecsSinceEpoch();
                if (indexedFileTimestamp != actualFileTimestamp)
                {
                    qDebug() << funcName << "- 数据文件已被修改，索引文件已过期:"
                             << "索引时间:" << indexedFileTimestamp
                             << "，文件时间:" << actualFileTimestamp;
                    qDebug() << funcName << "- 尝试保留索引文件结构，后续对修改行单独处理";
                }

                // 读取行数
                quint32 rowCount;
                in >> rowCount;

                if (rowCount == 0 || rowCount > 50000000) // 设置合理的上限，防止损坏的索引文件
                {
                    qWarning() << funcName << "- 索引文件中的行数不合理:" << rowCount;
                    indexFile.unmap(mappedData);
                    return cache;
                }

                // 预分配空间提高性能
                cache.reserve(rowCount);
                quint64 currentTimestamp = QDateTime::currentSecsSinceEpoch();

                // 批量读取数据以提高性能
                for (quint32 i = 0; i < rowCount; i++)
                {
                    RowPositionInfo pos;
                    in >> pos.offset;
                    in >> pos.dataSize;
                    pos.timestamp = currentTimestamp;

                    if (in.status() != QDataStream::Ok)
                    {
                        qWarning() << funcName << "- 读取行" << i << "的索引数据时出错";
                        cache.clear();
                        indexFile.unmap(mappedData);
                        return cache;
                    }

                    cache.append(pos);
                }

                indexFile.unmap(mappedData);
                qDebug() << funcName << "- 成功从索引文件加载" << cache.size() << "行的偏移数据，耗时:"
                         << timer.elapsed() << "ms";
                return cache;
            }
        }

        // 如果内存映射失败或文件较小，使用传统方式读取
        if (!useMMap)
        {
            QDataStream in(&indexFile);
            in.setByteOrder(QDataStream::LittleEndian);

            // 读取并验证魔数和版本
            quint32 magic, version;
            in >> magic;
            in >> version;

            if (magic != INDEX_FILE_MAGIC || version > INDEX_FILE_VERSION)
            {
                qWarning() << funcName << "- 索引文件格式不兼容，魔数:" << magic << "，版本:" << version;
                return cache;
            }

            // 读取数据文件的最后修改时间并验证
            quint64 indexedFileTimestamp;
            in >> indexedFileTimestamp;

            quint64 actualFileTimestamp = fileLastModified.toSecsSinceEpoch();
            if (indexedFileTimestamp != actualFileTimestamp)
            {
                qDebug() << funcName << "- 数据文件已被修改，索引文件已过期:"
                         << "索引时间:" << indexedFileTimestamp
                         << "，文件时间:" << actualFileTimestamp;
                qDebug() << funcName << "- 尝试保留索引文件结构，后续对修改行单独处理";
            }

            // 读取行数
            quint32 rowCount;
            in >> rowCount;

            if (rowCount == 0 || rowCount > 50000000) // 设置合理的上限，防止损坏的索引文件
            {
                qWarning() << funcName << "- 索引文件中的行数不合理:" << rowCount;
                return cache;
            }

            // 预分配空间提高性能
            cache.reserve(rowCount);

            // 使用缓冲区批量读取以提高性能
            const int BATCH_SIZE = 10000;
            quint64 currentTimestamp = QDateTime::currentSecsSinceEpoch();

            // 批量读取数据以提高性能
            for (quint32 i = 0; i < rowCount; i += BATCH_SIZE)
            {
                // 计算当前批次实际大小
                quint32 batchEnd = qMin(i + BATCH_SIZE, rowCount);
                quint32 currentBatchSize = batchEnd - i;

                for (quint32 j = 0; j < currentBatchSize; j++)
                {
                    RowPositionInfo pos;
                    in >> pos.offset;
                    in >> pos.dataSize;
                    pos.timestamp = currentTimestamp;

                    if (in.status() != QDataStream::Ok)
                    {
                        qWarning() << funcName << "- 读取行" << (i + j) << "的索引数据时出错";
                        cache.clear();
                        return cache;
                    }

                    cache.append(pos);
                }
            }
        }

        qDebug() << funcName << "- 成功从索引文件加载" << cache.size() << "行的偏移数据，耗时:"
                 << timer.elapsed() << "ms";

        return cache;
    }

    void BinaryFileHelper::clearRowOffsetCache(const QString &binFilePath)
    {
        const QString funcName = "BinaryFileHelper::clearRowOffsetCache";
        QMutexLocker locker(&s_cacheMutex);

        qDebug() << funcName << "- 清除文件行偏移缓存:" << binFilePath;
        s_fileRowOffsetCache.remove(binFilePath);

        // 同时删除索引文件
        QString indexFilePath = getIndexFilePath(binFilePath);
        QFile indexFile(indexFilePath);
        if (indexFile.exists())
        {
            if (indexFile.remove())
            {
                qDebug() << funcName << "- 索引文件已删除:" << indexFilePath;
            }
            else
            {
                qWarning() << funcName << "- 无法删除索引文件:" << indexFilePath;
            }
        }
    }

    void BinaryFileHelper::updateRowOffsetCache(const QString &binFilePath,
                                                const QSet<int> &modifiedRows,
                                                bool preserveIndex)
    {
        const QString funcName = "BinaryFileHelper::updateRowOffsetCache";
        QMutexLocker locker(&s_cacheMutex);

        // 如果不需要保留索引，直接调用清除函数
        if (!preserveIndex || modifiedRows.size() > 100)
        {
            // 当修改行数超过100行时，也使用完整清除
            qDebug() << funcName << "- 修改行数较多(" << modifiedRows.size()
                     << ")或不需要保留索引，执行完整清除";
            clearRowOffsetCache(binFilePath);
            return;
        }

        // 检查内存缓存是否存在
        if (!s_fileRowOffsetCache.contains(binFilePath))
        {
            qDebug() << funcName << "- 内存中无此文件缓存，无需更新:" << binFilePath;
            return;
        }

        RowOffsetCache &cache = s_fileRowOffsetCache[binFilePath];

        // 如果缓存为空，直接返回
        if (cache.isEmpty())
        {
            qDebug() << funcName << "- 缓存为空，无需更新";
            return;
        }

        // 记录哪些行需要在后续扫描中更新
        QSet<int> invalidatedRows = modifiedRows;

        // 在内存缓存中标记修改的行
        int validUpdateCount = 0;
        foreach (int rowIndex, modifiedRows)
        {
            if (rowIndex >= 0 && rowIndex < cache.size())
            {
                // 在内存中将该行标记为过期（设置timestamp为0）
                cache[rowIndex].timestamp = 0;
                validUpdateCount++;
            }
        }

        qDebug() << funcName << "- 已在内存缓存中标记" << validUpdateCount
                 << "行为已修改状态，保留索引文件结构";

        // 注意：此处不更新索引文件，因为更新单个行的索引文件成本较高
        // 下次读取时，如果发现内存缓存中有timestamp=0的行，
        // 将触发仅对这些行的重新扫描，而不是整个文件
    }
} // namespace Persistence