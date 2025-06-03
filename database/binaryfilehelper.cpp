#include "binaryfilehelper.h"
#include "common/logger.h"
#include <QDataStream>
#include <QJsonDocument>
#include "vector/vector_data_types.h"
#include <QFileInfo>
#include <QDebug>
#include <string.h>
#include <QDir>
#include <cstdio>    // Added for fprintf
#include <QDateTime> // Added for QDateTime

// Macro for direct stderr logging
#define BFH_LOG_STDERR(funcName, format, ...)                                                           \
    do                                                                                                  \
    {                                                                                                   \
        fprintf(stderr, "[BFH_STDERR][%s] %s: " format "\n",                                            \
                QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz").toStdString().c_str(), \
                funcName, ##__VA_ARGS__);                                                               \
        fflush(stderr);                                                                                 \
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
                    if (pinValue != "0" && pinValue != "1" && pinValue != "X" && pinValue != "L" && pinValue != "H" && pinValue != "Z")
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

        file.close();

        // 添加完整的性能统计信息
        QDateTime endTime = QDateTime::currentDateTime();
        qint64 totalElapsedMs = startTime.msecsTo(endTime);
        double avgRowsPerSecond = (totalElapsedMs > 0) ? (rowsWritten * 1000.0 / totalElapsedMs) : 0;

        qDebug() << funcName << "- 文件已关闭. 总计写入:" << rowsWritten << "行, 预期:"
                 << rows.size() << "行. 耗时:" << (totalElapsedMs / 1000.0) << "秒, 平均速度:"
                 << avgRowsPerSecond << "行/秒";

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

            if (rowIndex < 0 || rowIndex >= rowPositions.size())
            {
                qCritical() << funcName << "- CRITICAL_ERROR: (Defensive Check) 行索引" << rowIndex << "在rowPositions中越界 (Size: " << rowPositions.size() << "). 文件:" << binFilePath
                            << " (Reason: Row index out of bounds for rowPositions during update loop)";
                file.close();
                return false;
            }
            const RowInfo &originalRowInfo = rowPositions[rowIndex];

            QByteArray serializedNewRow;
            if (!serializeRow(newRowData, columns, serializedNewRow))
            {
                qCritical() << funcName << "- CRITICAL_ERROR: 序列化新行数据失败，行索引:" << rowIndex << ", 文件:" << binFilePath
                            << " (Reason: serializeRow returned false)";
                file.close();
                return false;
            }

            if (static_cast<quint32>(serializedNewRow.size()) != originalRowInfo.data_size)
            {
                qCritical() << funcName << "- CRITICAL_ERROR: 行" << rowIndex << "的新数据长度(" << serializedNewRow.size()
                            << ")与原始数据长度(" << originalRowInfo.data_size << ")不匹配. 文件:" << binFilePath
                            << " (Reason: Data length mismatch)";
                return false;
            }

            qint64 seekPos = originalRowInfo.offset + sizeof(quint32);
            if (!file.seek(seekPos))
            {
                qCritical() << funcName << "- CRITICAL_ERROR: Seek到行" << rowIndex << "的数据位置(" << seekPos << ")失败. 文件:" << binFilePath
                            << ", OS错误:" << file.errorString() << " (Reason: File seek failed for row update)";
                file.close();
                return false;
            }

            qint64 bytesWritten = file.write(serializedNewRow);
            if (bytesWritten != serializedNewRow.size())
            {
                qCritical() << funcName << "- CRITICAL_ERROR: 写入新行数据失败，行索引:" << rowIndex << ". 预期写入:" << serializedNewRow.size()
                            << ", 实际写入:" << bytesWritten << ". 文件:" << binFilePath
                            << ", OS错误:" << file.errorString() << " (Reason: File write failed for row update)";
                file.close();
                return false;
            }
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
                file.close();
                return false;
            }
            if (!writeBinaryHeader(&file, header))
            {
                qCritical() << funcName << "- CRITICAL_ERROR: 更新文件头的时间戳失败 (Reason: writeBinaryHeader for timestamp update failed), 文件:" << binFilePath;
                file.close();
                return false;
            }
            qDebug() << funcName << "- 文件头时间戳已成功更新.";
        }
        else if (modifiedRows.size() > invalidRowIndicesForUpdate.size())
        {
            qWarning() << funcName << "- 警告: 有 " << (modifiedRows.size() - invalidRowIndicesForUpdate.size())
                       << " 个有效修改行，但成功更新数量为0. 可能所有有效行都因某种原因（如长度不匹配）未能更新.";
            if ((modifiedRows.size() - invalidRowIndicesForUpdate.size()) > 0 && rowsUpdatedSuccessfully == 0)
            {
                qCritical() << funcName << "- CRITICAL_ERROR: 有效修改行存在，但无任何行被成功更新。上层应已处理此情况。";
                file.close();
                return false;
            }
        }

        file.close();
        qDebug() << funcName << "- 文件已关闭. 总计成功更新 " << rowsUpdatedSuccessfully << " 行.";

        return rowsUpdatedSuccessfully == static_cast<quint32>(modifiedRows.size() - invalidRowIndicesForUpdate.size());
    }
} // namespace Persistence