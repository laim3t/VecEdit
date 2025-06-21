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
        BinaryFileHelper();

        /**
         * @brief Reads the BinaryFileHeader from the given I/O device.
         * @param device The I/O device to read from. Must be open and readable.
         * @param header Output parameter to store the read header.
         * @return True if the header was read and validated successfully, false otherwise.
         */
        static bool readBinaryHeader(QIODevice *device, BinaryFileHeader &header);

        /**
         * @brief Writes the BinaryFileHeader to the given I/O device.
         * @param device The I/O device to write to. Must be open and writable.
         * @param header The header data to write.
         * @return True if the header was written successfully, false otherwise.
         */
        static bool writeBinaryHeader(QIODevice *device, const BinaryFileHeader &header);

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
         * @brief 序列化一行数据为二进制字节流
         * @param rowData 内存中的一行数据（与列顺序一致）
         * @param columns 列信息（顺序与 rowData 对应）
         * @param serializedRow 输出的二进制字节流
         * @return 成功返回 true，失败返回 false
         */
        static bool serializeRow(const Vector::RowData &rowData, const QList<Vector::ColumnInfo> &columns, QByteArray &serializedRow);

        /**
         * @brief 从二进制字节流反序列化为一行数据
         * @param bytes 输入的二进制字节流
         * @param columns 列信息（顺序与目标 rowData 对应）
         * @param fileVersion 文件头中的数据 schema 版本
         * @param rowData 输出的内存行数据
         * @return 成功返回 true，失败返回 false
         */
        static bool deserializeRow(const QByteArray &bytes, const QList<Vector::ColumnInfo> &columns, int fileVersion, Vector::RowData &rowData);

        /**
         * @brief 从二进制文件读取所有行数据
         *
         * 支持文件格式特性：
         * 1. 能够处理标准行数据格式
         * 2. 支持重定位标记(0xFFFFFFFF)识别，可以跟随重定位指针读取实际数据
         * 3. 在遇到重定位指针时，自动跳转到指定位置读取数据，然后返回继续处理后续行
         *
         * @param binFilePath 二进制文件的绝对路径
         * @param columns 列信息
         * @param schemaVersion 数据库中的schema版本
         * @param rows 输出参数，存储读取的所有行数据
         * @return bool 成功返回true，失败返回false
         */
        static bool readAllRowsFromBinary(const QString &binFilePath, const QList<Vector::ColumnInfo> &columns,
                                          int schemaVersion, QList<Vector::RowData> &rows);

        /**
         * @brief 将所有行数据写入二进制文件
         *
         * @param binFilePath 二进制文件的绝对路径
         * @param columns 列信息
         * @param schemaVersion 数据库中的schema版本
         * @param rows 要写入的所有行数据
         * @return bool 成功返回true，失败返回false
         */
        static bool writeAllRowsToBinary(const QString &binFilePath, const QList<Vector::ColumnInfo> &columns,
                                         int schemaVersion, const QList<Vector::RowData> &rows);

        /**
         * @brief 只更新二进制文件中被修改的行数据
         *
         * 与writeAllRowsToBinary不同，此方法只更新被修改的行，采用随机写入方式，避免重写整个文件
         *
         * 优化特点：
         * 1. 直接对原文件进行随机写入，无需创建临时文件
         * 2. 对于不需要更改的行数据，完全不读取，减少I/O操作
         * 3. 当新数据大于原数据空间时，使用重定位机制：
         *    - 将大数据写到文件末尾
         *    - 在原位置留下重定位指针(0xFFFFFFFF标记 + qint64位置)
         * 4. 能够处理大文件的部分更新，显著提高性能
         *
         * @param binFilePath 二进制文件的绝对路径
         * @param columns 列信息
         * @param schemaVersion 数据库中的schema版本
         * @param modifiedRows 键为行索引，值为行数据的Map
         * @return bool 成功返回true，失败返回false
         */
        static bool updateRowsInBinary(const QString &binFilePath, const QList<Vector::ColumnInfo> &columns,
                                       int schemaVersion, const QMap<int, Vector::RowData> &modifiedRows);

        /**
         * @brief 强健的增量更新实现，能够处理文件损坏和异常大小情况
         *
         * 这是一个全新实现的方法，用于替代原有的updateRowsInBinary，
         * 采用更保守和健壮的方式读取和更新二进制文件，确保在各种情况下都能正常工作。
         *
         * 特点：
         * 1. 不完全信任文件中的大小值，而是采用实际文件大小和预期格式进行验证
         * 2. 能够检测和处理文件损坏情况
         * 3. 即使部分行更新失败，也会尽可能完成其他行的更新
         * 4. 提供详细的诊断信息
         *
         * @param binFilePath 二进制文件路径
         * @param columns 列定义信息
         * @param schemaVersion 数据库Schema版本
         * @param modifiedRows 需要更新的行数据
         * @return bool 操作是否成功
         */
        static bool robustUpdateRowsInBinary(const QString &binFilePath,
                                             const QList<Vector::ColumnInfo> &columns,
                                             int schemaVersion,
                                             const QMap<int, Vector::RowData> &modifiedRows);

        /**
         * @brief 清除指定文件的行偏移缓存
         * 当文件被修改（如全量重写）后调用，确保缓存数据的一致性
         *
         * @param binFilePath 二进制文件路径
         */
        static void clearRowOffsetCache(const QString &binFilePath);

        /**
         * @brief 清除所有文件的行偏移缓存
         * 当应用需要释放内存或重置状态时调用
         */
        static void clearAllRowOffsetCaches();

        /**
         * @brief 智能更新行偏移缓存，适用于增量更新场景
         *
         * 不同于clearRowOffsetCache完全删除缓存和索引文件，
         * 此方法仅更新被修改行的缓存信息，并保留索引文件
         *
         * @param binFilePath 二进制文件路径
         * @param modifiedRows 被修改的行索引
         * @param preserveIndex 是否保留索引文件（默认为true）
         */
        static void updateRowOffsetCache(const QString &binFilePath,
                                         const QSet<int> &modifiedRows,
                                         bool preserveIndex = true);

        /**
         * @brief 从二进制文件中读取单行数据
         *
         * 此方法利用行偏移缓存机制，高效地读取指定行的数据，而不需要读取整个文件。
         * 是针对大型文件按需读取优化的核心方法。
         *
         * @param binFilePath 二进制文件的绝对路径
         * @param columns 列信息，用于反序列化
         * @param schemaVersion 数据库中的schema版本
         * @param rowIndex 要读取的行索引（0-based）
         * @param rowData 输出参数，存储读取的行数据
         * @param useCache 是否使用行偏移缓存（默认为true）
         * @return bool 成功返回true，失败返回false
         */
        static bool readRowFromBinary(const QString &binFilePath,
                                      const QList<Vector::ColumnInfo> &columns,
                                      int schemaVersion,
                                      int rowIndex,
                                      Vector::RowData &rowData,
                                      bool useCache = true);

        /**
         * @brief 创建一个新的空二进制文件，并写入初始头部
         *
         * @param binFilePath 要创建的二进制文件路径
         * @param columns 列信息
         * @param schemaVersion 数据库schema版本
         * @return bool 成功返回true，失败返回false
         */
        static bool createNewEmptyBinaryFile(const QString &binFilePath,
                                             const QList<Vector::ColumnInfo> &columns,
                                             int schemaVersion);

        /**
         * @brief 将单行数据追加到二进制文件末尾
         *
         * @param binFilePath 二进制文件的绝对路径
         * @param columns 列信息
         * @param rowData 要追加的单行数据
         * @return bool 成功返回true，失败返回false
         */
        static bool appendRowToBinary(const QString &binFilePath,
                                      const QList<Vector::ColumnInfo> &columns,
                                      const Vector::RowData &rowData);

    private:
        /**
         * @brief 获取列数据类型对应的固定长度
         * @param type 列数据类型
         * @return 对应数据类型的固定长度（字节数）
         */
        static int getFixedLengthForType(Vector::ColumnDataType type);

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
    };

} // namespace Persistence

#endif // BINARY_FILE_HELPER_H