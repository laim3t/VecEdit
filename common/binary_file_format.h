#ifndef BINARY_FILE_FORMAT_H
#define BINARY_FILE_FORMAT_H

#include <cstdint>
#include <QDebug> // For qDebug() if used in related cpp files

// 在 Persistence 命名空间中定义常量
namespace Persistence
{
    // 二进制文件魔数标识
    const uint32_t VEC_BINDATA_MAGIC = 0x56454342; // "VECB" in ASCII
    // 当前文件格式版本
    const uint16_t CURRENT_FILE_FORMAT_VERSION = 1;
}

// Magic number to identify our binary file type
constexpr uint32_t VBIN_MAGIC_NUMBER = 0x5642494E; // "VBIN"

/**
 * @brief Defines the header structure for .vbindata files.
 *
 * This header is written at the beginning of each binary data file and contains
 * essential metadata for interpreting the rest of the file.
 */
struct BinaryFileHeader
{
    uint32_t magic_number;         // Magic number to identify file type
    uint16_t file_format_version;  // Version of this header/file structure
    uint16_t data_schema_version;  // Version of the data schema within the file (links to DB schema_version)
    uint64_t row_count_in_file;    // Number of data rows stored in this file (实际应用限制在int范围内)
    uint32_t column_count_in_file; // Number of columns per row
    uint64_t timestamp_created;    // UTC timestamp of creation
    uint64_t timestamp_updated;    // UTC timestamp of last update
    uint8_t compression_type;      // 0: None, 1: ZLIB, etc. (future use)
    uint8_t reserved_bytes[15];    // Reserved for future expansion, initialized to 0

    BinaryFileHeader()
        : magic_number(Persistence::VEC_BINDATA_MAGIC),
          file_format_version(Persistence::CURRENT_FILE_FORMAT_VERSION),
          data_schema_version(1),
          row_count_in_file(0),
          column_count_in_file(0),
          timestamp_created(0),
          timestamp_updated(0),
          compression_type(0)
    {
        for (int i = 0; i < 15; ++i)
        {
            reserved_bytes[i] = 0;
        }
    }

    // Basic validation
    bool isValid() const
    {
        return magic_number == Persistence::VEC_BINDATA_MAGIC;
    }

    void logDetails(const QString &context) const
    {
        qDebug().nospace() << "[" << context << "] BinaryFileHeader Details:"
                           << "\n  Magic Number: 0x" << QString::number(magic_number, 16)
                           << "\n  File Format Version: " << file_format_version
                           << "\n  Data Schema Version: " << data_schema_version
                           << "\n  Row Count: " << row_count_in_file
                           << "\n  Column Count: " << column_count_in_file
                           << "\n  Created: " << timestamp_created
                           << "\n  Updated: " << timestamp_updated
                           << "\n  Compression: " << compression_type;
    }
};

#endif // BINARY_FILE_FORMAT_H