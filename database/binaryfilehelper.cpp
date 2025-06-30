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
        if (inByteArray.isEmpty())
        {
            return false;
        }

        // 添加安全检查，防止尝试反序列化明显不正确的数据
        if (inByteArray.size() > 1000000) // 1MB的上限
        {
            qWarning() << "BinaryFileHelper::deserializeRow - 输入数据过大:" << inByteArray.size() << "bytes";
            return false;
        }

        try {
            QDataStream stream(inByteArray);
            stream.setVersion(QDataStream::Qt_5_15); // Ensure this matches the writing version

            // Clear the output list before reading into it
            outRowData.clear();

            // 检查数据开头是否有有效的位模式
            // (可选)如果未来添加数据校验，可以在这里添加

            // QDataStream can read the QList<QVariant> directly
            stream >> outRowData;

            // Check for errors during deserialization
            if (stream.status() != QDataStream::Ok)
            {
                // This could happen if the byte array is malformed or truncated
                qWarning() << "BinaryFileHelper::deserializeRow - QDataStream status is not Ok after reading. Status:" 
                          << stream.status();
                return false;
            }

            // Additional check to see if we've reached the end of the stream.
            // If not, it might indicate a problem (e.g., extra, unexpected data).
            if (!stream.atEnd())
            {
                qWarning() << "BinaryFileHelper::deserializeRow - Stream not at end after deserializing row data. Possible malformed data.";
                qWarning() << "BinaryFileHelper::deserializeRow - 已读取" << stream.device()->pos() << "字节，总大小" 
                          << stream.device()->size() << "字节";
            }

            // 基本的数据有效性检查
            if (outRowData.isEmpty()) {
                qWarning() << "BinaryFileHelper::deserializeRow - 反序列化结果为空行";
                return false;
            }

            // 检查每列的值是否是合理的QVariant
            for (int i = 0; i < outRowData.size(); ++i) {
                if (!outRowData[i].isValid()) {
                    qWarning() << "BinaryFileHelper::deserializeRow - 列" << i << "包含无效的QVariant";
                    return false;
                }
            }

            return true;
        }
        catch (const std::bad_alloc &e) {
            qCritical() << "BinaryFileHelper::deserializeRow - 内存分配失败:" << e.what();
            return false;
        }
        catch (const std::exception &e) {
            qCritical() << "BinaryFileHelper::deserializeRow - 反序列化异常:" << e.what();
            return false;
        }
        catch (...) {
            qCritical() << "BinaryFileHelper::deserializeRow - 未知异常";
            return false;
        }
    }

    bool Persistence::BinaryFileHelper::updateRowCountInHeader(const QString &absoluteBinFilePath, int newRowCount)
    {
        const QString funcName = "BinaryFileHelper::updateRowCountInHeader";

        QFile file(absoluteBinFilePath);
        if (!file.open(QIODevice::ReadWrite))
        {
            qWarning() << funcName << "- Failed to open file for update:" << file.errorString();
            return false;
        }

        // 计算 row_count_in_file 字段的精确偏移量
        // sizeof(magic_number -> uint32_t) + sizeof(file_format_version -> uint16_t) + sizeof(data_schema_version -> uint16_t)
        const qint64 offset = sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint16_t);

        // 跳转到该偏移量
        if (!file.seek(offset))
        {
            qWarning() << funcName << "- Failed to seek to row count position:" << file.errorString();
            file.close();
            return false;
        }

        // 使用 QDataStream 以正确的字节序写入
        QDataStream stream(&file);
        stream.setByteOrder(QDataStream::LittleEndian);

        // 将 newRowCount (int) 转换为 uint64_t 写入
        stream << static_cast<uint64_t>(newRowCount);

        if (stream.status() != QDataStream::Ok)
        {
            qWarning() << funcName << "- QDataStream error while writing new row count. Status:" << stream.status();
            file.close();
            return false;
        }

        file.close();
        qDebug() << funcName << "- Successfully updated row count in header to" << newRowCount << "for file:" << absoluteBinFilePath;
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
        qDebug() << funcName << " - 开始读取文件页数据:" << absoluteBinFilePath 
                 << "，起始行:" << startRow << "，行数:" << numRows;
        
        pageRows.clear();
        
        QFile file(absoluteBinFilePath);
        if (!file.open(QIODevice::ReadOnly))
        {
            qWarning() << funcName << " - 无法打开文件:" << absoluteBinFilePath << "，错误:" << file.errorString();
            return false;
        }
        
        // 读取并验证文件头
        BinaryFileHeader header;
        if (!readBinaryHeader(&file, header))
        {
            qWarning() << funcName << " - 无法读取或验证二进制文件头";
            file.close();
            return false;
        }
        
        qDebug() << funcName << " - 文件头读取成功，总行数:" << header.row_count_in_file;
        
        // 版本兼容性检查
        if (header.data_schema_version != schemaVersion)
        {
            qWarning() << funcName << " - 文件格式版本(" << header.data_schema_version 
                       << ")与预期版本(" << schemaVersion << ")不匹配";
            
            if (header.data_schema_version > schemaVersion)
            {
                qCritical() << funcName << " - 文件版本高于当前支持的版本，无法读取";
                file.close();
                return false;
            }
        }
        
        // 一些基本验证
        if (startRow < 0)
        {
            qWarning() << funcName << " - 无效的起始行:" << startRow << "，已调整为0";
            startRow = 0;
        }
        
        if (startRow >= header.row_count_in_file)
        {
            qDebug() << funcName << " - 起始行超出文件范围，文件总行数:" << header.row_count_in_file;
            file.close();
            return false;
        }
        
        int actualRowsToRead = (numRows <= 0) ? 
                              (header.row_count_in_file - startRow) : 
                              std::min(static_cast<int>(header.row_count_in_file - startRow), numRows);
        
        if (actualRowsToRead <= 0)
        {
            qDebug() << funcName << " - 没有行需要读取";
            file.close();
            return true; // 返回成功，因为没有行是有效请求
        }
        
        qDebug() << funcName << " - 准备读取" << actualRowsToRead << "行数据";
        
        // 获取文件位置，确定header的实际大小
        qint64 headerSize = file.pos();
        qint64 dataSize = file.size() - headerSize;
        
        qDebug() << funcName << " - 文件总大小:" << file.size() 
                << "，头大小:" << headerSize
                << "，数据大小:" << dataSize;
                
        // 行数为0的特殊情况处理
        if (header.row_count_in_file == 0 || dataSize <= 0)
        {
            qDebug() << funcName << " - 文件中没有行数据";
            file.close();
            return false;
        }
        
        // 使用数据流以便正确读取
        QDataStream in(&file);
        in.setByteOrder(QDataStream::LittleEndian);
        
        int rowsRead = 0;
        QDateTime startTime = QDateTime::currentDateTime();
        
        try {
            // 估计每行大小，但添加安全上限
            const int MAX_REASONABLE_ROW_SIZE = 10 * 1024 * 1024; // 10MB的合理上限
            int estimatedRowSize = static_cast<int>(dataSize / header.row_count_in_file);
            
            // 防止估计值过大或过小
            if (estimatedRowSize > 100000) estimatedRowSize = 10000;
            if (estimatedRowSize < 50) estimatedRowSize = 100;
            
            qDebug() << funcName << " - 预估每行大小:" << estimatedRowSize;
            
            // 策略1: 尝试读取整个文件作为单个大行（针对新轨道格式）
            if (header.row_count_in_file == 1) {
                file.seek(headerSize);
                QByteArray allData = file.readAll();
                if (!allData.isEmpty()) {
                    Vector::RowData singleRow;
                    if (deserializeRow(allData, singleRow)) {
                        qDebug() << funcName << " - 成功读取单行数据，大小:" << allData.size() << "字节";
                        pageRows.append(singleRow);
                        rowsRead = 1;
                    } else {
                        qWarning() << funcName << " - 单行数据反序列化失败";
                    }
                } else {
                    qWarning() << funcName << " - 文件数据部分为空";
                }
            } 
            // 策略2: 尝试读取前缀+数据格式（旧格式）
            else if (header.row_count_in_file <= 10000) { // 只对合理行数尝试这种方式
                file.seek(headerSize);
                
                // 尝试读取前几行确定格式
                bool isLegacyFormat = false;
                qint64 startPos = file.pos();
                
                // 读取前4字节作为可能的行大小
                quint32 possibleRowSize = 0;
                if (in.readRawData(reinterpret_cast<char*>(&possibleRowSize), sizeof(quint32)) == sizeof(quint32)) {
                    // 如果这看起来像一个合理的大小（不是巨大的值）
                    if (possibleRowSize > 0 && possibleRowSize < MAX_REASONABLE_ROW_SIZE) {
                        // 尝试跳过这些字节，看看是否还有数据
                        if (file.seek(startPos + 4 + possibleRowSize)) {
                            // 如果我们没到文件末尾，那么这可能是旧格式
                            if (file.pos() < file.size()) {
                                isLegacyFormat = true;
                                qDebug() << funcName << " - 检测到旧格式（前缀+数据）";
                            }
                        }
                    }
                }
                
                // 返回文件起始位置
                file.seek(headerSize);
                
                // 根据检测到的格式读取行
                if (isLegacyFormat) {
                    // 旧格式：每行前面有行大小前缀
                    // 先跳到期望的开始行
                    int currentRow = 0;
                    while (currentRow < startRow && !file.atEnd()) {
                        // 读取行大小
                        quint32 rowSize;
                        if (in.readRawData(reinterpret_cast<char*>(&rowSize), sizeof(quint32)) != sizeof(quint32)) {
                            qWarning() << funcName << " - 读取行大小失败，行:" << currentRow;
                            break;
                        }
                        
                        // 安全检查
                        if (rowSize > MAX_REASONABLE_ROW_SIZE) {
                            qWarning() << funcName << " - 行大小异常 (" << rowSize << "字节)，可能数据损坏";
                            break;
                        }
                        
                        // 跳过该行数据
                        if (!file.seek(file.pos() + rowSize)) {
                            qWarning() << funcName << " - 跳转到下一行失败";
                            break;
                        }
                        currentRow++;
                    }
                    
                    // 现在读取请求的行数
                    for (int i = 0; i < actualRowsToRead && !file.atEnd(); i++) {
                        // 读取行大小
                        quint32 rowSize;
                        if (in.readRawData(reinterpret_cast<char*>(&rowSize), sizeof(quint32)) != sizeof(quint32)) {
                            qWarning() << funcName << " - 读取行大小失败，行:" << (startRow + i);
                            break;
                        }
                        
                        // 安全检查
                        if (rowSize > MAX_REASONABLE_ROW_SIZE) {
                            qWarning() << funcName << " - 行大小异常 (" << rowSize << "字节)，可能数据损坏";
                            break;
                        }
                        
                        // 读取行数据
                        QByteArray rowData(rowSize, Qt::Uninitialized);
                        if (in.readRawData(rowData.data(), rowSize) != static_cast<int>(rowSize)) {
                            qWarning() << funcName << " - 读取行数据失败，行:" << (startRow + i);
                            break;
                        }
                        
                        // 反序列化行
                        Vector::RowData row;
                        if (deserializeRow(rowData, row)) {
                            pageRows.append(row);
                            rowsRead++;
                        } else {
                            qWarning() << funcName << " - 行数据反序列化失败，行:" << (startRow + i);
                        }
                    }
                } else {
                    // 新格式：假设数据是直接序列化的行块，没有大小前缀
                    // 这是适用于新轨道的代码路径
                    
                    // 读取整个数据块
                    file.seek(headerSize);
                    QByteArray allData = file.readAll();
                    
                    // 尝试反序列化所有行
                    try {
                        Vector::RowData allRows;
                        if (deserializeRow(allData, allRows)) {
                            qDebug() << funcName << " - 成功读取单块数据，包含" << allRows.size() << "列";
                            // 这里假设是单行多列数据 - "新轨道"格式
                            for (int i = 0; i < header.row_count_in_file; i++) {
                                pageRows.append(allRows);
                                rowsRead++;
                                if (rowsRead >= actualRowsToRead)
                                    break;
                            }
                        } else {
                            qWarning() << funcName << " - 无法反序列化数据块";
                        }
                    } catch (const std::exception &e) {
                        qWarning() << funcName << " - 反序列化数据块异常:" << e.what();
                    }
                }
            }
            
            // 如果上面的策略都失败了，尝试应急策略：固定块大小读取
            if (rowsRead == 0) {
                qDebug() << funcName << " - 使用固定块大小读取策略";
                
                file.seek(headerSize);
                int blockSize = 4096; // 使用4KB的常见块大小
                
                for (int i = 0; i < header.row_count_in_file && file.pos() < file.size(); i++) {
                    if (i < startRow) {
                        // 跳过不需要的行
                        file.seek(file.pos() + blockSize);
                        continue;
                    }
                    
                    if (rowsRead >= actualRowsToRead) {
                        break;
                    }
                    
                    QByteArray rowData = file.read(blockSize);
                    if (rowData.isEmpty()) break;
                    
                    Vector::RowData row;
                    if (deserializeRow(rowData, row)) {
                        pageRows.append(row);
                        rowsRead++;
                    }
                }
            }
        } catch (const std::bad_alloc &e) {
            qCritical() << funcName << " - 内存分配失败:" << e.what();
        } catch (const std::exception &e) {
            qCritical() << funcName << " - 异常:" << e.what();
        } catch (...) {
            qCritical() << funcName << " - 未知异常";
        }
        
        file.close();
        
        QDateTime endTime = QDateTime::currentDateTime();
        qint64 elapsedMs = startTime.msecsTo(endTime);
        double rowsPerSecond = (elapsedMs > 0) ? (rowsRead * 1000.0 / elapsedMs) : 0;
        
        qDebug() << funcName << " - 读取完成，成功读取" << rowsRead << "行，耗时:" 
                 << (elapsedMs / 1000.0) << "秒，速度:" << rowsPerSecond << "行/秒";
        
        return rowsRead > 0;
    }
} // namespace Persistence
