#pragma once

#include <QFile>
#include <QList>
#include <QVariant>
#include <QMap>
#include <QMutex>

namespace Persistence
{

    // 魔数定义，用于识别文件类型和数据块
    constexpr quint64 ROBUST_VEC_DB_MAGIC = 0x5645435F44423032; // "VEC_DB02"
    constexpr quint32 DATA_CHUNK_SYNC_MARKER = 0xDEADBEEF;

    // 文件格式版本
    constexpr quint32 FILE_FORMAT_VERSION = 2;

    // 数据块类型
    enum class ChunkType : quint8
    {
        DataRow = 0x01,
        Deleted = 0xFE, // 标记一个块为已删除/过时
    };

#pragma pack(push, 1)
    // 文件头结构体
    struct RobustFileHeader
    {
        quint64 magicNumber = ROBUST_VEC_DB_MAGIC;
        quint32 fileFormatVersion = FILE_FORMAT_VERSION;
        quint64 indexAreaOffset = 0;
        quint64 indexAreaSize = 0;
        quint64 dataAreaOffset = 0;
        quint64 lastCompactTimestamp = 0;
        quint8 reserved[224]; // 凑齐256字节

        bool isValid() const { return magicNumber == ROBUST_VEC_DB_MAGIC; }
    };

    // 数据块头部
    struct DataChunkHeader
    {
        quint32 syncMarker = DATA_CHUNK_SYNC_MARKER;
        quint32 payloadSize = 0;
        ChunkType chunkType = ChunkType::DataRow;
    };

    // 索引条目
    struct IndexEntry
    {
        quint32 rowId = 0;
        quint64 chunkOffset = 0;
        quint32 chunkPayloadSize = 0;
    };
#pragma pack(pop)

    // 为简化，行数据类型别名
    using RowData = QList<QVariant>;

    class RobustBinaryHelper
    {
    public:
        RobustBinaryHelper();

        // === 文件级操作 ===
        static bool createFile(const QString &filePath);
        static bool openFile(const QString &filePath);
        static bool closeFile();

        // === 数据行操作 ===
        static bool addRow(const RowData &rowData, quint32 &rowId);
        static bool updateRow(quint32 rowId, const RowData &rowData);
        static bool deleteRow(quint32 rowId);
        static bool readRow(quint32 rowId, RowData &rowData);

        // === 查询操作 ===
        static QList<quint32> getAllRowIds();
        static int getRowCount();

        // === 维护操作 ===
        static bool compactFile();

    private:
        // === 内部辅助函数 ===
        static bool readHeader(QFile &file, RobustFileHeader &header);
        static bool writeHeader(QFile &file, const RobustFileHeader &header);

        static bool readIndex(QFile &file, QMap<quint32, IndexEntry> &indexMap);
        static bool writeIndex(QFile &file, const QMap<quint32, IndexEntry> &indexMap);

        static bool findNextChunk(QFile &file, DataChunkHeader &chunkHeader, qint64 &chunkOffset);
        static bool readChunk(QFile &file, qint64 offset, DataChunkHeader &header, QByteArray &payload);
        static bool writeChunk(QFile &file, qint64 offset, ChunkType type, const QByteArray &payload);

        static quint32 calculateCRC32(const QByteArray &data);

        // === 成员变量 ===
        // 使用静态成员来简化，但在多线程场景下需要更复杂的管理
        static QFile m_file;
        static RobustFileHeader m_header;
        static QMap<quint32, IndexEntry> m_indexMap;
        static QMutex m_mutex;
        static quint32 m_nextRowId;
    };

} // namespace Persistence