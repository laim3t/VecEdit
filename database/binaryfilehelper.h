#ifndef BINARY_FILE_HELPER_H
#define BINARY_FILE_HELPER_H

#include "common/binary_file_format.h" // For BinaryFileHeader
#include <QString>
#include <QIODevice>
#include <QByteArray>
#include <QDebug> // For qDebug()
#include "vector/vector_data_types.h"

// Forward declarations for placeholder types (replace with actual types later)
// class MemoryRowObject; // Placeholder for the actual C++ data structure for a row
// class ColumnSchema;    // Placeholder for column definition/schema information

namespace Persistence
{

    /**
     * @brief Provides utility functions for reading and writing binary data files,
     *        including headers and row-level data serialization/deserialization.
     *
     * This class is intended to be used internally by the persistence layer.
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

        // --- Placeholder methods for row data ---
        // These will need to be implemented once MemoryRowObject and ColumnSchema are defined.

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
    };

} // namespace Persistence

#endif // BINARY_FILE_HELPER_H