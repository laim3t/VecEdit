#include "robust_binary_helper.h"
#include <QDataStream>
#include <QDebug>
#include <QDateTime>
#include <QCryptographicHash>

namespace Persistence
{

    // QDataStream操作符实现
    QDataStream &operator<<(QDataStream &out, const IndexEntry &entry)
    {
        out << entry.rowId << entry.chunkOffset << entry.chunkPayloadSize;
        return out;
    }

    QDataStream &operator>>(QDataStream &in, IndexEntry &entry)
    {
        in >> entry.rowId >> entry.chunkOffset >> entry.chunkPayloadSize;
        return in;
    }

    QDataStream &operator<<(QDataStream &out, const QMap<unsigned int, IndexEntry> &map)
    {
        out << map.size();
        for (auto it = map.begin(); it != map.end(); ++it)
        {
            out << it.key() << it.value();
        }
        return out;
    }

    QDataStream &operator>>(QDataStream &in, QMap<unsigned int, IndexEntry> &map)
    {
        int size;
        in >> size;
        map.clear();
        for (int i = 0; i < size; ++i)
        {
            unsigned int key;
            IndexEntry value;
            in >> key >> value;
            map.insert(key, value);
        }
        return in;
    }

    // 初始化静态成员变量
    QFile RobustBinaryHelper::m_file;
    RobustFileHeader RobustBinaryHelper::m_header;
    QMap<quint32, IndexEntry> RobustBinaryHelper::m_indexMap;
    QMutex RobustBinaryHelper::m_mutex;
    quint32 RobustBinaryHelper::m_nextRowId = 1; // Row ID 从 1 开始

    RobustBinaryHelper::RobustBinaryHelper() {}

    // === 文件级操作 ===

    bool RobustBinaryHelper::createFile(const QString &filePath)
    {
        QMutexLocker locker(&m_mutex);

        if (m_file.isOpen())
        {
            qWarning() << "createFile: A file is already open.";
            return false;
        }

        m_file.setFileName(filePath);
        if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            qWarning() << "createFile: Failed to open file for writing:" << m_file.errorString();
            return false;
        }

        // 创建一个初始状态的Header
        RobustFileHeader initialHeader;
        initialHeader.dataAreaOffset = sizeof(RobustFileHeader);
        initialHeader.indexAreaOffset = sizeof(RobustFileHeader);
        initialHeader.indexAreaSize = 0;
        initialHeader.lastCompactTimestamp = QDateTime::currentSecsSinceEpoch();

        // 写入Header
        if (!writeHeader(m_file, initialHeader))
        {
            qWarning() << "createFile: Failed to write initial header.";
            m_file.close();
            return false;
        }

        m_file.close();
        qDebug() << "createFile: Successfully created new robust binary file at" << filePath;
        return true;
    }

    bool RobustBinaryHelper::openFile(const QString &filePath)
    {
        QMutexLocker locker(&m_mutex);
        if (m_file.isOpen())
        {
            qWarning() << "openFile: A file is already open.";
            return false;
        }

        m_file.setFileName(filePath);
        if (!m_file.open(QIODevice::ReadWrite))
        {
            qWarning() << "openFile: Failed to open file:" << m_file.errorString();
            return false;
        }

        // 1. 读取并验证Header
        if (!readHeader(m_file, m_header))
        {
            qWarning() << "openFile: Could not read a valid header from" << filePath;
            m_file.close();
            return false;
        }

        // 2. 读取索引
        if (!readIndex(m_file, m_indexMap))
        {
            qWarning() << "openFile: Could not read the index from" << filePath;
            m_file.close();
            return false;
        }

        // 3. 准备下一个可用的 RowId
        m_nextRowId = 1;
        if (!m_indexMap.isEmpty())
        {
            // QMap的键是自动排序的，所以最后一个键就是最大的ID
            m_nextRowId = m_indexMap.lastKey() + 1;
        }

        qDebug() << "openFile: Successfully opened file" << filePath << "with" << m_indexMap.size() << "rows.";
        return true;
    }

    bool RobustBinaryHelper::closeFile()
    {
        QMutexLocker locker(&m_mutex);
        if (!m_file.isOpen())
        {
            return true; // 已经关闭
        }

        // 1. 将最新的索引写回文件
        if (!writeIndex(m_file, m_indexMap))
        {
            qWarning() << "closeFile: CRITICAL - Failed to write index back to file. Data changes may be lost.";
            // 即使写入失败，我们还是尝试关闭文件
            m_file.close();
            return false;
        }

        // 2. 关闭文件并清理状态
        m_file.close();
        m_header = RobustFileHeader();
        m_indexMap.clear();
        m_nextRowId = 1;

        qDebug() << "closeFile: File closed successfully.";
        return true;
    }

    // === 数据行操作 ===

    bool RobustBinaryHelper::addRow(const RowData &rowData, quint32 &rowId)
    {
        QMutexLocker locker(&m_mutex);
        if (!m_file.isOpen())
        {
            qWarning() << "addRow: File is not open.";
            return false;
        }

        // 1. 序列化 rowData
        QByteArray payload;
        QDataStream stream(&payload, QIODevice::WriteOnly);
        stream << rowData;

        // 2. 确定新块的写入位置（当前数据区的末尾）
        // 注意：在真正多用户或复杂场景，需要更精细地管理数据区末尾指针
        qint64 newChunkOffset = m_header.dataAreaOffset;

        // 遍历索引找到数据区的实际末尾
        for (const auto &entry : m_indexMap)
        {
            qint64 chunkEnd = entry.chunkOffset + sizeof(DataChunkHeader) + entry.chunkPayloadSize + sizeof(quint32); // header + payload + checksum
            if (chunkEnd > newChunkOffset)
            {
                newChunkOffset = chunkEnd;
            }
        }

        // 3. 写入新块
        if (!writeChunk(m_file, newChunkOffset, ChunkType::DataRow, payload))
        {
            qWarning() << "addRow: Failed to write new data chunk.";
            return false;
        }

        // 4. 分配新ID并更新索引
        rowId = m_nextRowId++;
        IndexEntry newEntry;
        newEntry.rowId = rowId;
        newEntry.chunkOffset = newChunkOffset;
        newEntry.chunkPayloadSize = payload.size();
        m_indexMap.insert(rowId, newEntry);

        qDebug() << "addRow: Successfully added new row with ID" << rowId << "at offset" << newChunkOffset;
        return true;
    }

    bool RobustBinaryHelper::updateRow(quint32 rowId, const RowData &rowData)
    {
        QMutexLocker locker(&m_mutex);
        if (!m_file.isOpen())
        {
            qWarning() << "updateRow: File is not open.";
            return false;
        }

        // 1. 检查 rowId 是否存在
        if (!m_indexMap.contains(rowId))
        {
            qWarning() << "updateRow: Row with ID" << rowId << "not found.";
            return false;
        }

        // （可选但推荐）标记旧块为已删除
        // const IndexEntry& oldEntry = m_indexMap.value(rowId);
        // writeChunk(m_file, oldEntry.chunkOffset, ChunkType::Deleted, QByteArray());

        // 2. 序列化新数据
        QByteArray payload;
        QDataStream stream(&payload, QIODevice::WriteOnly);
        stream << rowData;

        // 3. 确定新块的写入位置（当前数据区的末尾）
        qint64 newChunkOffset = m_header.dataAreaOffset;
        for (const auto &entry : m_indexMap)
        {
            qint64 chunkEnd = entry.chunkOffset + sizeof(DataChunkHeader) + entry.chunkPayloadSize + sizeof(quint32);
            if (chunkEnd > newChunkOffset)
            {
                newChunkOffset = chunkEnd;
            }
        }

        // 4. 写入新块
        if (!writeChunk(m_file, newChunkOffset, ChunkType::DataRow, payload))
        {
            qWarning() << "updateRow: Failed to write new data chunk for row ID" << rowId;
            return false;
        }

        // 5. 更新索引
        IndexEntry &entryToUpdate = m_indexMap[rowId];
        entryToUpdate.chunkOffset = newChunkOffset;
        entryToUpdate.chunkPayloadSize = payload.size();

        qDebug() << "updateRow: Successfully updated row with ID" << rowId << ". New offset:" << newChunkOffset;
        return true;
    }

    bool RobustBinaryHelper::deleteRow(quint32 rowId)
    {
        QMutexLocker locker(&m_mutex);
        if (!m_file.isOpen())
        {
            qWarning() << "deleteRow: File is not open.";
            return false;
        }

        if (!m_indexMap.contains(rowId))
        {
            qWarning() << "deleteRow: Row with ID" << rowId << "not found.";
            return false;
        }

        // （可选但推荐）标记旧块为已删除
        const IndexEntry &entry = m_indexMap.value(rowId);

        // 创建一个空的payload，因为我们只更新块类型
        QByteArray emptyPayload;
        if (!writeChunk(m_file, entry.chunkOffset, ChunkType::Deleted, emptyPayload))
        {
            qWarning() << "deleteRow: Failed to mark chunk as deleted for row ID" << rowId;
            // 即使标记失败，我们仍然继续从索引中删除，以保证逻辑上的一致性
        }

        // 从索引中移除
        m_indexMap.remove(rowId);

        qDebug() << "deleteRow: Successfully deleted row with ID" << rowId;
        return true;
    }

    bool RobustBinaryHelper::readRow(quint32 rowId, RowData &rowData)
    {
        QMutexLocker locker(&m_mutex);
        if (!m_file.isOpen())
        {
            qWarning() << "readRow: File is not open.";
            return false;
        }

        // 1. 在索引中查找
        if (!m_indexMap.contains(rowId))
        {
            qWarning() << "readRow: Row with ID" << rowId << "not found in index.";
            return false;
        }
        const IndexEntry &entry = m_indexMap.value(rowId);

        // 2. 读取数据块
        DataChunkHeader header;
        QByteArray payload;
        if (!readChunk(m_file, entry.chunkOffset, header, payload))
        {
            qWarning() << "readRow: Failed to read data chunk for row ID" << rowId;
            return false;
        }

        // 检查块类型是否正确
        if (header.chunkType != ChunkType::DataRow)
        {
            qWarning() << "readRow: Chunk for row ID" << rowId << "is not a valid data row chunk (maybe deleted?).";
            return false;
        }

        // 3. 反序列化
        QDataStream stream(&payload, QIODevice::ReadOnly);
        stream >> rowData;

        if (stream.status() != QDataStream::Ok)
        {
            qWarning() << "readRow: Failed to deserialize row data for ID" << rowId;
            return false;
        }

        return true;
    }

    // === 查询操作 ===
    QList<quint32> RobustBinaryHelper::getAllRowIds()
    {
        QMutexLocker locker(&m_mutex);
        if (!m_file.isOpen())
        {
            qWarning() << "getAllRowIds: File is not open.";
            return QList<quint32>();
        }
        return m_indexMap.keys();
    }

    int RobustBinaryHelper::getRowCount()
    {
        QMutexLocker locker(&m_mutex);
        if (!m_file.isOpen())
        {
            return 0;
        }
        return m_indexMap.size();
    }

    // === 维护操作 ===

    bool RobustBinaryHelper::compactFile()
    {
        // TODO: 实现文件压缩的逻辑
        // 1. 创建一个临时文件
        // 2. 写入Header
        // 3. 遍历 m_indexMap，将所有有效的数据块依次读出并写入临时文件
        // 4. 更新索引，并把新的索引区写入临时文件
        // 5. 原子地替换掉旧文件
        return false;
    }

    // === 内部辅助函数 ===

    bool RobustBinaryHelper::readHeader(QFile &file, RobustFileHeader &header)
    {
        if (!file.seek(0))
        {
            qWarning() << "readHeader: Failed to seek to the beginning of the file.";
            return false;
        }

        QDataStream in(&file);
        qint64 bytesRead = in.readRawData(reinterpret_cast<char *>(&header), sizeof(RobustFileHeader));

        if (bytesRead != sizeof(RobustFileHeader) || !header.isValid())
        {
            qWarning() << "readHeader: Failed to read a valid header. Bytes read:" << bytesRead;
            return false;
        }

        return true;
    }

    bool RobustBinaryHelper::writeHeader(QFile &file, const RobustFileHeader &header)
    {
        if (!file.seek(0))
        {
            qWarning() << "writeHeader: Failed to seek to the beginning of the file.";
            return false;
        }

        QDataStream out(&file);
        qint64 bytesWritten = out.writeRawData(reinterpret_cast<const char *>(&header), sizeof(RobustFileHeader));

        if (bytesWritten != sizeof(RobustFileHeader))
        {
            qWarning() << "writeHeader: Failed to write the complete header. Bytes written:" << bytesWritten;
            return false;
        }

        return true;
    }

    bool RobustBinaryHelper::readIndex(QFile &file, QMap<quint32, IndexEntry> &indexMap)
    {
        RobustFileHeader header;
        if (!readHeader(file, header))
            return false;

        if (header.indexAreaSize == 0)
        {
            indexMap.clear();
            return true; // 空索引是正常的
        }

        if (!file.seek(header.indexAreaOffset))
        {
            qWarning() << "readIndex: Failed to seek to index area offset" << header.indexAreaOffset;
            return false;
        }

        QByteArray indexData = file.read(header.indexAreaSize);
        if (indexData.size() != header.indexAreaSize)
        {
            qWarning() << "readIndex: Failed to read complete index data.";
            return false;
        }

        QDataStream stream(&indexData, QIODevice::ReadOnly);
        stream >> indexMap;

        if (stream.status() != QDataStream::Ok)
        {
            qWarning() << "readIndex: Failed to deserialize index map.";
            indexMap.clear();
            return false;
        }

        return true;
    }

    bool RobustBinaryHelper::writeIndex(QFile &file, const QMap<quint32, IndexEntry> &indexMap)
    {
        RobustFileHeader header;
        if (!readHeader(file, header))
            return false;

        QByteArray indexData;
        QDataStream stream(&indexData, QIODevice::WriteOnly);
        stream << indexMap;

        // 索引总是写在数据区的末尾
        qint64 dataEndOffset = header.dataAreaOffset;
        for (const auto &entry : indexMap)
        {
            qint64 chunkEnd = entry.chunkOffset + sizeof(DataChunkHeader) + entry.chunkPayloadSize + sizeof(quint32);
            if (chunkEnd > dataEndOffset)
            {
                dataEndOffset = chunkEnd;
            }
        }

        if (!file.seek(dataEndOffset))
        {
            qWarning() << "writeIndex: Failed to seek to end of data area" << dataEndOffset;
            return false;
        }

        qint64 bytesWritten = file.write(indexData);
        if (bytesWritten != indexData.size())
        {
            qWarning() << "writeIndex: Failed to write complete index data.";
            return false;
        }

        // 更新Header并写回
        header.indexAreaOffset = dataEndOffset;
        header.indexAreaSize = indexData.size();
        return writeHeader(file, header);
    }

    bool RobustBinaryHelper::findNextChunk(QFile &file, DataChunkHeader &chunkHeader, qint64 &chunkOffset)
    {
        // (可选，高级功能) 实现从任意位置开始寻找下一个有效数据块的逻辑，用于文件恢复
        return false;
    }

    bool RobustBinaryHelper::readChunk(QFile &file, qint64 offset, DataChunkHeader &header, QByteArray &payload)
    {
        if (!file.seek(offset))
        {
            qWarning() << "readChunk: Failed to seek to offset" << offset;
            return false;
        }

        QDataStream in(&file);

        // 1. 读取块头
        qint64 headerBytesRead = in.readRawData(reinterpret_cast<char *>(&header), sizeof(DataChunkHeader));
        if (headerBytesRead != sizeof(DataChunkHeader) || header.syncMarker != DATA_CHUNK_SYNC_MARKER)
        {
            qWarning() << "readChunk: Failed to read a valid chunk header at offset" << offset;
            return false;
        }

        // 2. 读取数据负载
        payload.resize(header.payloadSize);
        qint64 payloadBytesRead = in.readRawData(payload.data(), header.payloadSize);
        if (payloadBytesRead != header.payloadSize)
        {
            qWarning() << "readChunk: Payload read error. Expected" << header.payloadSize << "bytes, got" << payloadBytesRead;
            return false;
        }

        // 3. 读取并验证校验和
        quint32 storedChecksum;
        in >> storedChecksum;
        if (in.status() != QDataStream::Ok)
        {
            qWarning() << "readChunk: Failed to read checksum.";
            return false;
        }

        quint32 calculatedChecksum = calculateCRC32(payload);
        if (storedChecksum != calculatedChecksum)
        {
            qWarning() << "readChunk: Checksum mismatch! Stored:" << storedChecksum << ", Calculated:" << calculatedChecksum;
            return false;
        }

        return true;
    }

    bool RobustBinaryHelper::writeChunk(QFile &file, qint64 offset, ChunkType type, const QByteArray &payload)
    {
        if (!file.seek(offset))
        {
            qWarning() << "writeChunk: Failed to seek to offset" << offset;
            return false;
        }

        DataChunkHeader header;
        header.chunkType = type;
        header.payloadSize = payload.size();

        quint32 checksum = calculateCRC32(payload);

        QDataStream out(&file);

        // 1. 写入块头
        out.writeRawData(reinterpret_cast<const char *>(&header), sizeof(DataChunkHeader));
        // 2. 写入数据负载
        out.writeRawData(payload.constData(), payload.size());
        // 3. 写入校验和
        out << checksum;

        if (out.status() != QDataStream::Ok)
        {
            qWarning() << "writeChunk: QDataStream error during chunk write.";
            return false;
        }

        return true;
    }

    quint32 RobustBinaryHelper::calculateCRC32(const QByteArray &data)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
        return QCryptographicHash::hash(data, QCryptographicHash::Crc32c).toUInt();
#else
        // 对于 Qt < 6.2, Crc32/Crc32c 不是内置的.
        // 您需要引入一个第三方库 (如 zlib) 或自己实现.
        // 这里暂时返回一个简单的校验和作为占位符.
        // 警告: 这不是一个可靠的校验和!
        quint32 checksum = 0;
        for (char byte : data)
        {
            checksum = (checksum * 31) + static_cast<quint8>(byte);
        }
        return checksum;
        // static bool warning_shown = false;
        // if (!warning_shown) {
        //     qWarning() << "calculateCRC32: Using a simple, non-robust checksum for compatibility with Qt < 6.2. Please implement a proper CRC32.";
        //     warning_shown = true;
        // }
#endif
    }

} // namespace Persistence