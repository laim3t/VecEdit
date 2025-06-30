#ifndef BINARY_FILE_HELPER_H
#define BINARY_FILE_HELPER_H

#include "common/binary_file_format.h"   // For BinaryFileHeader structure
#include "common/binary_field_lengths.h" // 添加对固定长度定义的引用
#include <QString>
#include <QIODevice>
#include <QByteArray>
#include <QDebug> // For qDebug()
#include <QFile>  // 添加QFile的包含声明
#include <QMap>   // 添加QMap用于缓存结构
#include <QMutex> // 添加QMutex用于线程安全
#include "vector/vector_data_types.h"

// Forward declarations for placeholder types (replace with actual types later)
// class MemoryRowObject; // Placeholder for the actual C++ data structure for a row
// class ColumnSchema;    // Placeholder for column definition/schema information

namespace Vector
{
    struct ColumnInfo;
    using RowData = QList<QVariant>;
} // namespace Vector

namespace Persistence
{

    /**
     * @brief Helper class for working with binary file I/O.
     *
     * This class provides static utility methods for reading and writing
     * binary files, particularly those with the BinaryFileHeader structure.
     * It also handles serialization and deserialization of row data to and
     * from binary form.
     */
    class BinaryFileHelper
    {
    public:
        // This is a static utility class, disallow instantiation.
        BinaryFileHelper() = delete;
        ~BinaryFileHelper() = delete;
        BinaryFileHelper(const BinaryFileHelper &) = delete;
        BinaryFileHelper &operator=(const BinaryFileHelper &) = delete;

        // --- Core Header and Row I/O ---
        static bool readBinaryHeader(QIODevice *device, BinaryFileHeader &header);
        static bool writeBinaryHeader(QIODevice *device, const BinaryFileHeader &header);
        static bool serializeRow(const Vector::RowData &rowData, const QList<Vector::ColumnInfo> &columns, QByteArray &serializedRow);
        static bool deserializeRow(const QByteArray &bytes, const QList<Vector::ColumnInfo> &columns, int fileVersion, Vector::RowData &rowData);

        // --- Full File Read/Write ---

        // [New Track - Indexed Read for RobustVectorDataHandler]
        static bool readAllRowsFromBinary(const QString &binFilePath,
                                          const QList<Vector::ColumnInfo> &columns,
                                          int schemaVersion, QList<QList<QVariant>> &rows,
                                          int masterRecordId);

        // [Old Track - Fixed-Width Read for backward compatibility]
        static bool readAllRowsFromBinary(const QString &binFilePath,
                                          const QList<Vector::ColumnInfo> &columns,
                                          int schemaVersion, QList<QList<QVariant>> &rows);

        static bool writeAllRowsToBinary(const QString &binFilePath, const QList<Vector::ColumnInfo> &columns,
                                         int schemaVersion, const QList<Vector::RowData> &rows);

        // --- Incremental/Paged Read/Write ---
        static bool readPageDataFromBinary(const QString &absoluteBinFilePath,
                                           const QList<Vector::ColumnInfo> &columns,
                                           int schemaVersion,
                                           int startRow,
                                           int numRows,
                                           QList<Vector::RowData> &pageRows);

        static bool updateRowsInBinary(const QString &binFilePath, const QList<Vector::ColumnInfo> &columns,
                                       int schemaVersion, const QMap<int, Vector::RowData> &modifiedRows);

        static bool robustUpdateRowsInBinary(const QString &binFilePath,
                                             const QList<Vector::ColumnInfo> &columns,
                                             int schemaVersion,
                                             const QMap<int, Vector::RowData> &modifiedRows);

        static bool insertRowsInBinary(const QString &binFilePath, const QList<Vector::ColumnInfo> &columns,
                                       int schemaVersion, int startRow, const QList<Vector::RowData> &rowsToInsert,
                                       QString &errorMessage);

        // --- Header/Metadata Manipulation ---
        static bool updateRowCountInHeader(const QString &absoluteBinFilePath, int newRowCount);

        // --- Cache Management ---
        static void clearRowOffsetCache(const QString &binFilePath);
        static void clearAllRowOffsetCaches();
        static void updateRowOffsetCache(const QString &binFilePath,
                                         const QSet<int> &modifiedRows,
                                         bool preserveIndex = true);

    private:
        /**
         * @brief 获取列数据类型对应的固定长度
         * @param type 列数据类型
         * @return 对应数据类型的固定长度（字节数）
         */
        static int getFixedLengthForType(Vector::ColumnDataType type);

        /**
         * @brief 获取给定数据类型和列名在二进制存储中的固定长度
         *
         * 如果是特殊字段（如Label、Comment、EXT），将返回它们的特定长度
         * 否则根据数据类型返回默认长度
         *
         * @param type 列数据类型
         * @param columnName 列名称，用于判断是否为特殊字段
         * @return 字段的固定存储长度
         */
        static int getFixedLengthForType(Vector::ColumnDataType type, const QString &columnName = QString());

        /**
         * @brief 读取行的大小信息并进行合理性验证
         *
         * 专门用于robustUpdateRowsInBinary，提供更健壮的行大小读取
         *
         * @param stream 二进制数据流
         * @param file 文件对象
         * @param maxReasonableSize 合理的最大行大小
         * @param[out] rowSize 输出读取到的行大小
         * @return bool 读取是否成功
         */
        static bool readRowSizeWithValidation(QDataStream &stream, QFile &file, quint32 maxReasonableSize, quint32 &rowSize);

        /**
         * @brief 二进制文件行偏移缓存
         *
         * 存储格式：
         * key: 文件路径
         * value: 文件中每行的偏移位置和数据大小
         */
        struct RowPositionInfo
        {
            qint64 offset;     // 行起始位置(包括大小字段)
            quint32 dataSize;  // 该行数据的大小(不包括大小字段)
            quint64 timestamp; // 缓存创建时间(秒), 用于判断缓存新鲜度
        };

        using RowOffsetCache = QVector<RowPositionInfo>;

        // 文件路径 -> 行偏移缓存
        static QMap<QString, RowOffsetCache> s_fileRowOffsetCache;

        // 保护缓存的互斥锁，用于多线程环境
        static QMutex s_cacheMutex;

        /**
         * @brief 获取指定文件的行偏移缓存
         *
         * 如果缓存不存在或已过期，会返回空 QVector
         *
         * @param binFilePath 二进制文件路径
         * @param fileLastModified 文件最后修改时间，用于验证缓存有效性
         * @return RowOffsetCache 行偏移缓存，无效时返回空 QVector
         */
        static RowOffsetCache getRowOffsetCache(const QString &binFilePath, const QDateTime &fileLastModified);

        /**
         * @brief 设置指定文件的行偏移缓存
         *
         * @param binFilePath 二进制文件路径
         * @param rowPositions 行偏移信息
         */
        static void setRowOffsetCache(const QString &binFilePath, const RowOffsetCache &rowPositions);

        /**
         * @brief 将行偏移缓存保存为持久化索引文件
         *
         * @param binFilePath 二进制数据文件路径
         * @param rowPositions 行偏移信息
         * @return 是否成功保存索引文件
         */
        static bool saveRowOffsetIndex(const QString &binFilePath, const RowOffsetCache &rowPositions);

        /**
         * @brief 从持久化索引文件加载行偏移缓存
         *
         * @param binFilePath 二进制数据文件路径
         * @param fileLastModified 数据文件最后修改时间，用于验证索引有效性
         * @return 行偏移缓存，无效时返回空 QVector
         */
        static RowOffsetCache loadRowOffsetIndex(const QString &binFilePath, const QDateTime &fileLastModified);

        /**
         * @brief 获取索引文件路径
         *
         * @param binFilePath 二进制数据文件路径
         * @return 对应的索引文件路径
         */
        static QString getIndexFilePath(const QString &binFilePath);

        static qint64 calculateExpectedRowSize(const QList<Vector::ColumnInfo> &columns);
    };

} // namespace Persistence

#endif // BINARY_FILE_HELPER_H