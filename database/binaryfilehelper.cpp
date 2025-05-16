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

    bool BinaryFileHelper::serializeRow(const Vector::RowData &rowData, const QList<Vector::ColumnInfo> &columns, QByteArray &serializedRow)
    {
        const QString funcName = "BinaryFileHelper::serializeRow";
        qDebug().nospace() << funcName << " - 开始序列化行数据, 列数:" << columns.size() << ", 数据项数:" << rowData.size();
        serializedRow.clear();
        if (rowData.size() != columns.size())
        {
            qWarning() << funcName << " - 列数与数据项数不一致!";
            return false;
        }
        QDataStream out(&serializedRow, QIODevice::WriteOnly);
        out.setByteOrder(QDataStream::LittleEndian);
        for (int i = 0; i < columns.size(); ++i)
        {
            const auto &col = columns[i];
            const QVariant &val = rowData[i];
            qDebug().nospace() << funcName << " - 序列化第" << i << "列, 名称:" << col.name << ", 类型:" << static_cast<int>(col.type) << ", 值:" << val;
            switch (col.type)
            {
            case Vector::ColumnDataType::TEXT:
                out << val.toString();
                break;
            case Vector::ColumnDataType::INTEGER:
            case Vector::ColumnDataType::INSTRUCTION_ID:
            case Vector::ColumnDataType::TIMESET_ID:
            case Vector::ColumnDataType::PIN_STATE_ID:
                out << val.toInt();
                break;
            case Vector::ColumnDataType::REAL:
                out << val.toDouble();
                break;
            case Vector::ColumnDataType::BOOLEAN:
                out << static_cast<quint8>(val.toBool() ? 1 : 0);
                break;
            case Vector::ColumnDataType::JSON_PROPERTIES:
                out << QString(QJsonDocument(val.toJsonObject()).toJson(QJsonDocument::Compact));
                break;
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
        qDebug() << funcName << " - 序列化完成, 字节长度:" << serializedRow.size();
        return true;
    }

    bool BinaryFileHelper::deserializeRow(const QByteArray &bytes, const QList<Vector::ColumnInfo> &columns, int fileVersion, Vector::RowData &rowData)
    {
        const QString funcName = "BinaryFileHelper::deserializeRow";
        qDebug().nospace() << funcName << " - 开始反序列化行数据, 列数:" << columns.size() << ", 字节长度:" << bytes.size() << ", 文件版本:" << fileVersion;
        rowData.clear();
        QDataStream in(bytes);
        in.setByteOrder(QDataStream::LittleEndian);

        // 为每个列创建一个空值
        for (int i = 0; i < columns.size(); ++i)
        {
            rowData.append(QVariant());
        }

        // 根据文件版本选择不同的反序列化方法
        if (fileVersion == 1)
        {
            // 版本1的反序列化逻辑
            for (int i = 0; i < columns.size(); ++i)
            {
                const auto &col = columns[i];
                QVariant val;
                qDebug().nospace() << funcName << " - 反序列化第" << i << "列, 名称:" << col.name << ", 类型:" << static_cast<int>(col.type);

                // 检查数据流是否已经到达末尾
                if (in.atEnd())
                {
                    qWarning() << funcName << " - 数据流已到达末尾，列:" << col.name << "使用默认值";
                    // 使用默认值
                    switch (col.type)
                    {
                    case Vector::ColumnDataType::TEXT:
                        val = QString("");
                        break;
                    case Vector::ColumnDataType::INTEGER:
                    case Vector::ColumnDataType::INSTRUCTION_ID:
                    case Vector::ColumnDataType::TIMESET_ID:
                    case Vector::ColumnDataType::PIN_STATE_ID:
                        val = 0;
                        break;
                    case Vector::ColumnDataType::REAL:
                        val = 0.0;
                        break;
                    case Vector::ColumnDataType::BOOLEAN:
                        val = false;
                        break;
                    case Vector::ColumnDataType::JSON_PROPERTIES:
                        val = QJsonObject();
                        break;
                    default:
                        val = QVariant();
                        break;
                    }
                    rowData[i] = val;
                    continue; // 继续处理下一列
                }

                // 尝试读取数据
                switch (col.type)
                {
                case Vector::ColumnDataType::TEXT:
                {
                    QString s;
                    in >> s;
                    val = s;
                    break;
                }
                case Vector::ColumnDataType::INTEGER:
                case Vector::ColumnDataType::INSTRUCTION_ID:
                case Vector::ColumnDataType::TIMESET_ID:
                case Vector::ColumnDataType::PIN_STATE_ID:
                {
                    int v = 0;
                    in >> v;
                    val = v;
                    break;
                }
                case Vector::ColumnDataType::REAL:
                {
                    double d = 0.0;
                    in >> d;
                    val = d;
                    break;
                }
                case Vector::ColumnDataType::BOOLEAN:
                {
                    quint8 b = 0;
                    in >> b;
                    val = b != 0;
                    break;
                }
                case Vector::ColumnDataType::JSON_PROPERTIES:
                {
                    QString jsonStr;
                    in >> jsonStr;
                    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
                    val = doc.object();
                    break;
                }
                default:
                    qWarning() << funcName << " - 不支持的列类型:" << static_cast<int>(col.type) << ", 列名:" << col.name;
                    // 使用默认空值
                    val = QVariant();
                    break;
                }

                // 检查读取是否成功
                if (in.status() != QDataStream::Ok)
                {
                    qWarning() << funcName << " - QDataStream 读取失败, 列:" << col.name << ", 状态:" << in.status() << ", 使用默认值";
                    // 使用默认值
                    switch (col.type)
                    {
                    case Vector::ColumnDataType::TEXT:
                        val = QString("X"); // 对于管脚状态，默认使用"X"
                        break;
                    case Vector::ColumnDataType::INTEGER:
                    case Vector::ColumnDataType::INSTRUCTION_ID:
                    case Vector::ColumnDataType::TIMESET_ID:
                    case Vector::ColumnDataType::PIN_STATE_ID:
                        val = 0;
                        break;
                    case Vector::ColumnDataType::REAL:
                        val = 0.0;
                        break;
                    case Vector::ColumnDataType::BOOLEAN:
                        val = false;
                        break;
                    case Vector::ColumnDataType::JSON_PROPERTIES:
                        val = QJsonObject();
                        break;
                    default:
                        val = QVariant();
                        break;
                    }
                    // 重置状态以继续处理后续列
                    in.resetStatus();
                }

                rowData[i] = val;
            }
        }
        else if (fileVersion > 1)
        {
            // 未来版本的反序列化逻辑
            qWarning() << funcName << " - 不支持的文件版本:" << fileVersion << ", 尝试以最高支持版本解析";
            // 这里可以实现对未来可能添加的新版本的支持
            // 现在就简单地按版本1处理
            return deserializeRow(bytes, columns, 1, rowData);
        }
        else
        {
            qWarning() << funcName << " - 无效的文件版本:" << fileVersion;
            return false;
        }

        qDebug() << funcName << " - 反序列化完成, 行数据项:" << rowData.size();
        return true;
    }

    bool BinaryFileHelper::readAllRowsFromBinary(const QString &binFilePath, const QList<Vector::ColumnInfo> &columns,
                                                 int schemaVersion, QList<Vector::RowData> &rows)
    {
        const QString funcName = "BinaryFileHelper::readAllRowsFromBinary";
        qDebug() << funcName << "- Entry. File path:" << binFilePath << "DB Schema Version:" << schemaVersion << "Num Columns Expected:" << columns.size();
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
            quint32 rowBlockSize = 0;
            in >> rowBlockSize;
            if (in.status() != QDataStream::Ok || rowBlockSize == 0)
            {
                qWarning() << funcName << "- Error reading row block size. Status:" << in.status() << "Size read:" << rowBlockSize << ". Read" << actualRowsRead << "rows so far.";
                file.close();
                return false;
            }

            // Read the row data block itself
            QByteArray rowDataBlock;
            rowDataBlock.resize(rowBlockSize);
            qint64 bytesRead = in.readRawData(rowDataBlock.data(), rowBlockSize);

            if (bytesRead != rowBlockSize || in.status() != QDataStream::Ok)
            {
                qWarning() << funcName << "- Error reading row data block. Expected size:" << rowBlockSize << ", Bytes read:" << bytesRead << ", Stream status:" << in.status() << ". Read" << actualRowsRead << "rows so far.";
                file.close();
                return false; // Treat stream read error as failure
            }

            // Deserialize the block using the specific deserializeRow function
            Vector::RowData deserializedRow;
            if (!deserializeRow(rowDataBlock, columns, header.data_schema_version, deserializedRow))
            {
                qWarning() << funcName << "- Failed to deserialize row" << actualRowsRead << ". Using empty row instead.";
                // 创建一个空行，填充默认值
                deserializedRow.clear();
                for (int i = 0; i < columns.size(); ++i)
                {
                    const auto &col = columns[i];
                    switch (col.type)
                    {
                    case Vector::ColumnDataType::TEXT:
                        deserializedRow.append(QString(""));
                        break;
                    case Vector::ColumnDataType::INTEGER:
                    case Vector::ColumnDataType::INSTRUCTION_ID:
                    case Vector::ColumnDataType::TIMESET_ID:
                    case Vector::ColumnDataType::PIN_STATE_ID:
                        deserializedRow.append(0);
                        break;
                    case Vector::ColumnDataType::REAL:
                        deserializedRow.append(0.0);
                        break;
                    case Vector::ColumnDataType::BOOLEAN:
                        deserializedRow.append(false);
                        break;
                    case Vector::ColumnDataType::JSON_PROPERTIES:
                        deserializedRow.append(QJsonObject());
                        break;
                    default:
                        deserializedRow.append(QVariant());
                        break;
                    }
                }
            }

            rows.append(deserializedRow);
            actualRowsRead++;
            // Verbose log (optional):
            // qDebug() << funcName << "- Successfully deserialized row" << actualRowsRead;

        } // End while loop

        qDebug() << funcName << "- Finished reading loop. Expected rows based on header:" << header.row_count_in_file << ". Actual rows successfully deserialized:" << actualRowsRead << ". Stream at end:" << in.atEnd();

        file.close();

        // Check if the number of rows read matches the header count *exactly*
        if (actualRowsRead != header.row_count_in_file)
        {
            qWarning() << funcName << "- CRITICAL WARNING: Number of rows successfully read (" << actualRowsRead << ") does not match header count (" << header.row_count_in_file << "). File IS likely truncated or corrupted.";
            // Return false because the file state is inconsistent with its header.
            return false;
        }
        // Optional check: ensure stream is exactly at the end if expected
        if (!in.atEnd() && actualRowsRead == header.row_count_in_file)
        {
            qDebug() << funcName << "- Warning: Reached expected row count but stream is not at end. Extra data present?";
            // Decide if this is an error or acceptable.
        }

        qDebug() << funcName << "- Exit. Returning" << rows.size() << "rows. Operation successful.";
        return true; // Only return true if header count matches rows read.
    }

    bool BinaryFileHelper::writeAllRowsToBinary(const QString &binFilePath, const QList<Vector::ColumnInfo> &columns,
                                                int schemaVersion, const QList<Vector::RowData> &rows)
    {
        const QString funcName = "BinaryFileHelper::writeAllRowsToBinary";
        qDebug() << funcName << "- Entry. File path:" << binFilePath << "DB Schema Version:" << schemaVersion << ". Attempting to write" << rows.size() << "rows with" << columns.size() << "columns.";

        QFile file(binFilePath);
        qDebug() << funcName << "- Attempting to open file for writing (Truncate).";
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            qWarning() << funcName << "- Error: Failed to open file for writing:" << binFilePath << "Error:" << file.errorString();
            return false;
        }
        qDebug() << funcName << "- File opened successfully.";

        // --- Prepare Header ---
        BinaryFileHeader headerToWrite;
        headerToWrite.magic_number = VBIN_MAGIC_NUMBER;                  // Set magic number
        headerToWrite.file_format_version = CURRENT_FILE_FORMAT_VERSION; // Use constant for current format version
        headerToWrite.data_schema_version = schemaVersion;               // Use the provided schema version
        headerToWrite.row_count_in_file = static_cast<uint64_t>(rows.size());
        headerToWrite.column_count_in_file = static_cast<uint32_t>(columns.size());
        headerToWrite.timestamp_created = static_cast<uint64_t>(QDateTime::currentSecsSinceEpoch());
        headerToWrite.timestamp_updated = headerToWrite.timestamp_created;
        headerToWrite.compression_type = 0; // Set compression type (e.g., 0 for none)
        // Ensure reserved bytes are initialized (e.g., to zero)
        memset(headerToWrite.reserved_bytes, 0, sizeof(headerToWrite.reserved_bytes));
        // --- End Prepare Header ---

        qDebug() << funcName << "- Attempting to write binary header.";
        headerToWrite.logDetails(funcName + " - Header to Write");
        if (!writeBinaryHeader(&file, headerToWrite))
        {
            qWarning() << funcName << "- Error: Failed to write binary header.";
            file.close();
            // Attempt to remove the potentially corrupt file
            if (QFile::exists(binFilePath))
            {
                QFile::remove(binFilePath);
                qDebug() << funcName << "- Removed partially written file due to header write error.";
            }
            return false;
        }
        qDebug() << funcName << "- Binary header written successfully.";

        if (rows.isEmpty())
        {
            qDebug() << funcName << "- No rows to write. File contains only the header.";
            file.flush();
            file.close();
            qDebug() << funcName << "- File flushed and closed. Exit (0 rows written).";
            return true;
        }

        qDebug() << funcName << "- Starting data serialization loop for" << rows.size() << "rows.";
        QDataStream out(&file);
        out.setByteOrder(QDataStream::LittleEndian); // Ensure consistency
        // Set QDataStream version if needed
        // out.setVersion(QDataStream::Qt_5_15);

        quint32 rowsWritten = 0;
        for (const Vector::RowData &row : rows)
        {
            // Verify row consistency (column count)
            if (static_cast<uint32_t>(row.size()) != headerToWrite.column_count_in_file)
            {
                qWarning() << funcName << "- Error: Row" << rowsWritten << "has" << row.size() << "columns, but header expects" << headerToWrite.column_count_in_file << ". Aborting write.";
                file.close();
                // Remove the inconsistent file
                if (QFile::exists(binFilePath))
                {
                    QFile::remove(binFilePath);
                    qDebug() << funcName << "- Removed inconsistent file due to row column count mismatch.";
                }
                return false;
            }

            // Serialize the row data using the specific serializeRow function
            QByteArray serializedRowData;
            if (!serializeRow(row, columns, serializedRowData))
            {
                qWarning() << funcName << "- Error serializing data for row" << rowsWritten << ". Aborting write.";
                file.close();
                if (QFile::exists(binFilePath))
                {
                    QFile::remove(binFilePath);
                    qDebug() << funcName << "- Removed inconsistent file due to row serialization error.";
                }
                return false;
            }

            // Write the size of the serialized row block first
            quint32 rowBlockSize = static_cast<quint32>(serializedRowData.size());
            out << rowBlockSize;
            if (out.status() != QDataStream::Ok)
            {
                qWarning() << funcName << "- Error writing row size for row" << rowsWritten << ". Status:" << out.status();
                file.close();
                if (QFile::exists(binFilePath))
                    QFile::remove(binFilePath);
                return false;
            }

            // Write the actual serialized row data block
            qint64 bytesWritten = out.writeRawData(serializedRowData.constData(), rowBlockSize);
            if (bytesWritten != rowBlockSize || out.status() != QDataStream::Ok)
            { // Check both bytes written and stream status
                qWarning() << funcName << "- Error writing row block for row" << rowsWritten << ". Expected:" << rowBlockSize << ", Written:" << bytesWritten << ", Status:" << out.status();
                file.close();
                if (QFile::exists(binFilePath))
                    QFile::remove(binFilePath); // Clean up potentially corrupt file
                return false;                   // Abort on stream error
            }
            rowsWritten++;
        } // End for loop

        qDebug() << funcName << "- Finished writing loop. Successfully wrote" << rowsWritten << "rows.";

        file.flush(); // Ensure all data is written to the OS buffer
        file.close(); // Close the file

        qDebug() << funcName << "- File flushed and closed. Exit.";
        return true;
    }

} // namespace Persistence