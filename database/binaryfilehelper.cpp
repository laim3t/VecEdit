#include "binaryfilehelper.h"
#include "common/logger.h" // Assuming Logger is in common/
#include <QDataStream>
#include <QJsonDocument>
#include "vector/vector_data_types.h"
#include <QFileInfo>
#include <QDebug>

namespace Persistence
{

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

    bool BinaryFileHelper::serializeRow(const Vector::RowData &rowData, const QList<Vector::ColumnInfo> &columns, QByteArray &serializedRow, bool useFixedLength)
    {
        const QString funcName = "BinaryFileHelper::serializeRow";
        
        // 仅在入口和出口记录日志，减少日志量
        qDebug().nospace() << funcName << " - 开始序列化行数据, 列数:" << columns.size() << ", 数据项数:" << rowData.size() << ", 使用固定长度:" << useFixedLength;
        
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
            
            // 移除循环中的日志，显著提高性能
            // qDebug().nospace() << funcName << " - 序列化第" << i << "列, 名称:" << col.name << ", 类型:" << static_cast<int>(col.type) << ", 值:" << val;

            // 使用列名称和类型获取该字段的固定长度
            int fieldLength = useFixedLength ? getFixedLengthForType(col.type, col.name) : 0;
            // 移除循环中的日志
            // qDebug() << funcName << " - 字段固定长度:" << fieldLength;

            switch (col.type)
            {
            case Vector::ColumnDataType::TEXT:
            {
                QString text = val.toString();
                if (useFixedLength)
                {
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
                }
                else
                {
                    // 可变长度格式
                    out << text;
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
        qDebug() << funcName << " - 序列化完成, 字节长度:" << serializedRow.size();
        return true;
    }

    bool BinaryFileHelper::deserializeRow(const QByteArray &bytes, const QList<Vector::ColumnInfo> &columns, int fileVersion, Vector::RowData &rowData, bool useFixedLength)
    {
        const QString funcName = "BinaryFileHelper::deserializeRow";
        qDebug() << funcName << " - 开始反序列化二进制数据，字节数:" << bytes.size() << ", 列数:" << columns.size() << ", 文件版本:" << fileVersion;

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
            int fieldLength = useFixedLength ? getFixedLengthForType(col.type, col.name) : 0;
            qDebug().nospace() << funcName << " - 处理列: " << col.name << ", 类型: " << static_cast<int>(col.type) << ", 固定长度: " << fieldLength;

            QVariant value;
            if (useFixedLength)
            {
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
                    value = QVariant();
                    break;
                }
            }
            else
            {
                // 使用可变长度格式
                switch (col.type)
                {
                case Vector::ColumnDataType::TEXT:
                {
                    QString text;
                    in >> text;
                    value = text;
                    break;
                }
                case Vector::ColumnDataType::PIN_STATE_ID:
                {
                    // 读取为字符串
                    QString s;
                    in >> s;
                    // 如果为空或无效，使用"X"
                    if (s.isEmpty() || s.isNull() || s.trimmed().isEmpty())
                    {
                        s = "X";
                        qDebug() << funcName << " - 管脚状态列值为空，使用默认值'X'，列名:" << col.name;
                    }
                    else
                    {
                        qDebug() << funcName << " - 管脚状态列读取到有效值:" << s << "，列名:" << col.name;
                    }
                    value = s;
                    break;
                }
                case Vector::ColumnDataType::INTEGER:
                case Vector::ColumnDataType::INSTRUCTION_ID:
                case Vector::ColumnDataType::TIMESET_ID:
                {
                    int v = 0;
                    in >> v;
                    value = v;
                    break;
                }
                case Vector::ColumnDataType::REAL:
                {
                    double d = 0.0;
                    in >> d;
                    value = d;
                    break;
                }
                case Vector::ColumnDataType::BOOLEAN:
                {
                    quint8 b = 0;
                    in >> b;
                    value = b != 0;
                    break;
                }
                case Vector::ColumnDataType::JSON_PROPERTIES:
                {
                    QString jsonStr;
                    in >> jsonStr;
                    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
                    value = doc.object();
                    break;
                }
                default:
                    qWarning() << funcName << " - 不支持的列类型:" << static_cast<int>(col.type) << ", 列名:" << col.name;
                    // 使用默认空值
                    value = QVariant();
                    break;
                }
            }

            rowData.append(value);
            qDebug().nospace() << funcName << " - 解析列: " << col.name << ", 值: " << value;
        }

        if (in.status() != QDataStream::Ok)
        {
            qWarning() << funcName << " - 反序列化过程中发生错误，QDataStream状态:" << in.status();
            return false;
        }

        return true;
    }

    bool BinaryFileHelper::readAllRowsFromBinary(const QString &binFilePath, const QList<Vector::ColumnInfo> &columns,
                                                 int schemaVersion, QList<Vector::RowData> &rows, bool useFixedLength)
    {
        const QString funcName = "BinaryFileHelper::readAllRowsFromBinary";
        qDebug() << funcName << "- Entry. File path:" << binFilePath << "DB Schema Version:" << schemaVersion << "Num Columns Expected:" << columns.size() << "Use Fixed Length:" << useFixedLength;
        rows.clear(); // Ensure the output list is empty initially

        QFile file(binFilePath);
        qDebug() << funcName << "- Attempting to open file for reading.";
        if (!file.open(QIODevice::ReadOnly))
        {
            qWarning() << funcName << "- Error: Failed to open file:" << binFilePath << "Error:" << file.errorString();
            return false;
        }
        qDebug() << funcName << "- File opened successfully.";

        BinaryFileHeader header;
        qDebug() << funcName << "- Attempting to read binary header.";
        if (!readBinaryHeader(&file, header))
        {
            qWarning() << funcName << "- Error: Failed to read or validate binary header.";
            file.close();
            return false;
        }
        qDebug() << funcName << "- Binary header read successfully. Details:";
        header.logDetails(funcName); // Log header details

        // --- Version Compatibility Check ---
        if (header.data_schema_version != schemaVersion)
        {
            qWarning() << funcName << "- Warning: File data schema version (" << header.data_schema_version
                       << ") differs from expected DB schema version (" << schemaVersion << "). Attempting compatibility.";
            // Implement compatibility logic here if needed, e.g., using different deserializeRow versions.
            // For now, we'll proceed but pass the file's schema version to deserializeRow.
            if (header.data_schema_version > schemaVersion)
            {
                qCritical() << funcName << "- Error: File data schema version is newer than DB schema version. Cannot load.";
                file.close();
                return false;
            }
            // Allow older file versions to be read using their corresponding schema version
        }
        // --- End Version Check ---

        // --- Column Count Sanity Check ---
        if (header.column_count_in_file != static_cast<uint32_t>(columns.size()))
        {
            qWarning() << funcName << "- Warning: Header column count (" << header.column_count_in_file
                       << ") differs from expected column count based on DB schema (" << columns.size() << ").";
            // Decide how to handle this. Abort? Try to proceed?
            // Let's try to proceed but log the warning. Deserialization might fail.
        }
        // --- End Column Count Check ---

        if (header.row_count_in_file == 0)
        {
            qDebug() << funcName << "- Header indicates 0 rows in the file. No data to read.";
            file.close();
            return true; // Successfully read a file with 0 rows as indicated by header
        }

        qDebug() << funcName << "- Header indicates" << header.row_count_in_file << "rows. Starting data deserialization loop.";

        QDataStream in(&file);
        in.setByteOrder(QDataStream::LittleEndian); // Ensure consistency
        // Set QDataStream version if necessary, although serialize/deserialize handle it per call might be better
        // in.setVersion(QDataStream::Qt_5_15); // Example

        quint64 actualRowsRead = 0;                               // Use quint64 to match header type
        rows.reserve(static_cast<int>(header.row_count_in_file)); // QList uses int for size/reserve

        while (!in.atEnd() && actualRowsRead < header.row_count_in_file)
        {
            // Read the size of the next row block
            quint32 rowByteSize;
            in >> rowByteSize;

            if (in.status() != QDataStream::Ok || rowByteSize == 0)
            {
                qWarning() << funcName << "- Error reading row size at position" << file.pos() << ". Status:" << in.status();
                break;
            }

            // Read the actual bytes for this row
            QByteArray rowBytes = file.read(rowByteSize);
            if (rowBytes.size() != static_cast<int>(rowByteSize))
            {
                qWarning() << funcName << "- Error reading row bytes. Expected:" << rowByteSize << "Got:" << rowBytes.size();
                break;
            }

            // Deserialize the row bytes into a row data object
            Vector::RowData rowData;
            if (deserializeRow(rowBytes, columns, header.data_schema_version, rowData, useFixedLength))
            {
                rows.append(rowData);
                actualRowsRead++;

                if (actualRowsRead % 1000 == 0)
                {
                    qDebug() << funcName << "- Progress:" << actualRowsRead << "rows read.";
                }
            }
            else
            {
                qWarning() << funcName << "- Failed to deserialize row at position" << actualRowsRead + 1 << ". Continuing to next row.";
                // Option: We could break here to abort on first error, but continuing allows for more resilience
            }
        }

        file.close();
        qDebug() << funcName << "- File closed. Total rows read:" << rows.size() << "Expected:" << header.row_count_in_file;

        if (actualRowsRead != header.row_count_in_file)
        {
            qWarning() << funcName << "- Warning: Actual rows read (" << actualRowsRead << ") does not match header row count (" << header.row_count_in_file << ")";
        }

        return !rows.isEmpty() || header.row_count_in_file == 0; // Consider empty file as success if header indicates 0 rows
    }

    bool BinaryFileHelper::writeAllRowsToBinary(const QString &binFilePath, const QList<Vector::ColumnInfo> &columns,
                                                int schemaVersion, const QList<Vector::RowData> &rows, bool useFixedLength)
    {
        const QString funcName = "BinaryFileHelper::writeAllRowsToBinary";
        // 减少日志输出
        qDebug() << funcName << "- 开始写入文件:" << binFilePath << ", 行数:" << rows.size();

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
        const int logInterval = 5000;

        for (const auto &rowData : rows)
        {
            QByteArray serializedRow;
            if (serializeRow(rowData, columns, serializedRow, useFixedLength))
            {
                // 写入行数据大小
                out << static_cast<quint32>(serializedRow.size());

                // 写入行数据
                if (file.write(serializedRow) == serializedRow.size())
                {
                    rowsWritten++;

                    // 降低日志频率
                    if (rowsWritten % logInterval == 0)
                    {
                        qDebug() << funcName << "- 进度:" << rowsWritten << "行已写入";
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
        qDebug() << funcName << "- 文件已关闭. 总计写入:" << rowsWritten << "行, 预期:" << rows.size() << "行";

        return rowsWritten == static_cast<quint64>(rows.size());
    }

    bool BinaryFileHelper::updateRowsInBinary(const QString &binFilePath, const QList<Vector::ColumnInfo> &columns,
                                            int schemaVersion, const QMap<int, Vector::RowData> &modifiedRows, 
                                            bool useFixedLength)
    {
        const QString funcName = "BinaryFileHelper::updateRowsInBinary";
        
        // 如果没有修改的行，直接返回成功
        if (modifiedRows.isEmpty()) {
            // qDebug() << funcName << "- 没有修改的行，直接返回成功";
            return true;
        }
        
        // 首先，检查文件是否存在
        QFile file(binFilePath);
        if (!file.exists()) {
            qWarning() << funcName << "- 二进制文件不存在，无法更新: " << binFilePath;
            return false;
        }
        
        // 打开文件用于读取
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << funcName << "- 无法打开文件进行读取: " << binFilePath << ", 错误: " << file.errorString();
            return false;
        }
        
        // 读取文件头
        BinaryFileHeader header;
        if (!readBinaryHeader(&file, header)) {
            qWarning() << funcName << "- 无法读取或验证二进制文件头";
            file.close();
            return false;
        }
        
        // 版本兼容性检查
        if (header.data_schema_version != schemaVersion) {
            qWarning() << funcName << "- 文件数据schema版本(" << header.data_schema_version
                       << ")与期望的DB schema版本(" << schemaVersion << ")不匹配";
            file.close();
            return false;
        }
        
        // 列数检查
        if (header.column_count_in_file != static_cast<uint32_t>(columns.size())) {
            qWarning() << funcName << "- 文件头列数(" << header.column_count_in_file
                      << ")与期望的列数(" << columns.size() << ")不匹配";
            file.close();
            return false;
        }
        
        // 获取每行数据在文件中的位置
        struct RowInfo {
            qint64 offset;      // 行数据在文件中的起始位置
            quint32 size;       // 行数据的大小
        };
        
        QVector<RowInfo> rowPositions;
        rowPositions.reserve(header.row_count_in_file);
        
        QDataStream in(&file);
        in.setByteOrder(QDataStream::LittleEndian);
        
        // 计算文件头后的位置，即第一行数据的开始位置
        qint64 currentPos = sizeof(BinaryFileHeader);
        file.seek(currentPos);
        
        // 记录每行数据的位置和大小
        for (quint64 i = 0; i < header.row_count_in_file; ++i) {
            quint32 rowSize;
            in >> rowSize;
            
            RowInfo rowInfo;
            rowInfo.offset = file.pos() - sizeof(quint32);  // 减去刚读取的大小字段的4字节
            rowInfo.size = rowSize + sizeof(quint32);       // 加上大小字段的4字节
            
            rowPositions.append(rowInfo);
            
            // 跳过这行数据，定位到下一行
            file.seek(file.pos() + rowSize);
        }
        
        // 关闭文件，准备重新打开用于读写
        file.close();
        
        // 检查要修改的行是否都在范围内
        int maxRowIndex = rowPositions.size() - 1;
        QList<int> validModifiedRows;
        
        for (auto it = modifiedRows.constBegin(); it != modifiedRows.constEnd(); ++it) {
            int rowIndex = it.key();
            if (rowIndex < 0 || rowIndex > maxRowIndex) {
                qWarning() << funcName << "- 行索引超出范围: " << rowIndex << ", 最大有效索引: " << maxRowIndex;
                continue;
            }
            validModifiedRows.append(rowIndex);
        }
        
        if (validModifiedRows.isEmpty()) {
            qWarning() << funcName << "- 没有有效的行需要更新";
            return false;
        }
        
        // 创建临时文件用于存储更新后的内容
        QString tempFilePath = binFilePath + ".tmp";
        QFile tempFile(tempFilePath);
        if (!tempFile.open(QIODevice::WriteOnly)) {
            qWarning() << funcName << "- 无法创建临时文件: " << tempFilePath << ", 错误: " << tempFile.errorString();
            return false;
        }
        
        // 重新打开原文件用于读取
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << funcName << "- 无法重新打开原文件进行读取: " << binFilePath << ", 错误: " << file.errorString();
            tempFile.close();
            return false;
        }
        
        // 更新文件头的时间戳
        header.timestamp_updated = QDateTime::currentSecsSinceEpoch();
        
        // 写入临时文件头
        if (!writeBinaryHeader(&tempFile, header)) {
            qWarning() << funcName << "- 写入临时文件头失败";
            tempFile.close();
            file.close();
            return false;
        }
        
        // 遍历所有行，更新修改过的行
        bool success = true;
        QDataStream outTemp(&tempFile);
        outTemp.setByteOrder(QDataStream::LittleEndian);
        
        // 设置更大的日志间隔
        const int logInterval = 5000;
        int processedRows = 0;
        
        for (int rowIdx = 0; rowIdx < rowPositions.size(); ++rowIdx) {
            if (modifiedRows.contains(rowIdx)) {
                // 这行需要更新，序列化新数据
                QByteArray serializedRow;
                if (!serializeRow(modifiedRows[rowIdx], columns, serializedRow, useFixedLength)) {
                    qWarning() << funcName << "- 序列化行数据失败，行索引: " << rowIdx;
                    success = false;
                    break;
                }
                
                // 写入行大小
                outTemp << static_cast<quint32>(serializedRow.size());
                
                // 写入行数据
                if (tempFile.write(serializedRow) != serializedRow.size()) {
                    qWarning() << funcName << "- 写入修改后的行数据失败，行索引: " << rowIdx;
                    success = false;
                    break;
                }
            } else {
                // 这行未修改，直接从原文件复制
                const RowInfo &rowInfo = rowPositions[rowIdx];
                file.seek(rowInfo.offset);
                QByteArray rowData = file.read(rowInfo.size);
                
                if (rowData.size() != rowInfo.size) {
                    qWarning() << funcName << "- 读取原始行数据失败，行索引: " << rowIdx;
                    success = false;
                    break;
                }
                
                if (tempFile.write(rowData) != rowData.size()) {
                    qWarning() << funcName << "- 写入未修改的行数据失败，行索引: " << rowIdx;
                    success = false;
                    break;
                }
            }
            
            // 记录处理进度
            processedRows++;
            if (processedRows % logInterval == 0) {
                qDebug() << funcName << "- 进度: 已处理 " << processedRows << " 行，总计 " << rowPositions.size() << " 行";
            }
        }
        
        // 关闭文件
        file.close();
        tempFile.close();
        
        // 如果成功，用临时文件替换原文件
        if (success) {
            // 首先删除原文件
            if (!QFile::remove(binFilePath)) {
                qWarning() << funcName << "- 无法删除原文件: " << binFilePath;
                return false;
            }
            
            // 然后重命名临时文件
            if (!QFile::rename(tempFilePath, binFilePath)) {
                qWarning() << funcName << "- 无法重命名临时文件: " << tempFilePath << " 到 " << binFilePath;
                return false;
            }
            
            qDebug() << funcName << "- 成功更新 " << modifiedRows.size() << " 行数据";
        } else {
            // 失败时删除临时文件
            QFile::remove(tempFilePath);
        }
        
        return success;
    }

} // namespace Persistence