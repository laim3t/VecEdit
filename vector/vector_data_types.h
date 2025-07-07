#ifndef VECTOR_DATA_TYPES_H
#define VECTOR_DATA_TYPES_H

#include <QString>
#include <QList>
#include <QVariant>
#include <QJsonObject>
#include <QDebug> // For qDebug()
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSet>

namespace Vector
{

    /**
     * @brief Represents the type of data stored in a vector table column.
     *
     * These types guide serialization, deserialization, and potentially UI representation.
     */
    enum class ColumnDataType
    {
        TEXT,
        INTEGER,        // General integer
        REAL,           // General real number
        INSTRUCTION_ID, // Foreign key to an instructions table
        TIMESET_ID,     // Foreign key to a timesets table
        PIN_STATE_ID,   // Foreign key to pin_options or similar for pin states
        BOOLEAN,        // Boolean type (e.g., for Capture Y/N)
        JSON_PROPERTIES // For complex properties stored as JSON text
        // Add more types as needed
    };

    /**
     * @brief Stores metadata for a single column in a vector table.
     *
     * This information is typically loaded from the VectorTableColumnConfiguration table.
     */
    struct ColumnInfo
    {
        int id;                      // Primary key from VectorTableColumnConfiguration
        int vector_table_id;         // Foreign key to VectorTableMasterRecord
        QString name;                // Column name (e.g., "Label", "Instruction", "Pin1")
        int order;                   // Display and storage order of the column
        ColumnDataType type;         // The data type of the column (guides serialization)
        QString original_type_str;   // The original string for type from DB (e.g., "TEXT", "PIN_STATE_ID")
        QJsonObject data_properties; // Additional properties, e.g., for pin columns (pin_list_id, channel_count)
        bool is_visible;             // 表示列是否可见（对应数据库中的IsVisible字段）

        ColumnInfo()
            : id(-1),
              vector_table_id(-1),
              order(-1),
              type(ColumnDataType::TEXT),
              is_visible(true) {}

        void logDetails(const QString &context) const
        {
            qDebug().nospace() << "[" << context << "] ColumnInfo Details:"
                               << "\n  ID: " << id
                               << "\n  VectorTableID: " << vector_table_id
                               << "\n  Name: " << name
                               << "\n  Order: " << order
                               << "\n  Type: " << static_cast<int>(type) << " (Original: " << original_type_str << ")"
                               << "\n  IsVisible: " << (is_visible ? "true" : "false")
                               << "\n  Properties: " << data_properties;
        }
    };

    /**
     * @brief Represents a single row of data in a vector table.
     *
     * It's a list of QVariants, where each variant corresponds to a column
     * as defined by a list of ColumnInfo objects.
     */
    using RowData = QList<QVariant>;

    /**
     * @brief Represents a collection of rows, essentially the in-memory version of a vector table's data.
     */
    using VectorTableData = QList<RowData>;

    // Helper to convert string type from DB to enum
    inline ColumnDataType columnDataTypeFromString(const QString &typeStr, const QString &columnName = QString())
    {
        // 特殊情况处理: 根据列名做硬编码映射，覆盖数据库中不正确的类型值
        // 这是一个紧急修复，后续应该通过修复数据库中的值来解决根本问题
        if (!columnName.isEmpty())
        {
            // 检查是否是标准列名
            if (columnName == "Label")
                return ColumnDataType::TEXT;
            else if (columnName == "Instruction")
                return ColumnDataType::INSTRUCTION_ID;
            else if (columnName == "TimeSet")
                return ColumnDataType::TIMESET_ID;
            else if (columnName == "Capture")
                return ColumnDataType::BOOLEAN; // 这是关键修复：Capture列应该使用布尔类型编辑器
            else if (columnName == "EXT" || columnName == "Comment")
                return ColumnDataType::TEXT;
            
            // 除了上述固定列名外，所有其他列名都视为管脚列
            // 这是最简单有效的方式，因为根据项目规则，除了固定的六列外，其他列都是管脚列
            qDebug() << "columnDataTypeFromString - 非标准列名:" << columnName << "识别为管脚列(PIN_STATE_ID)";
            return ColumnDataType::PIN_STATE_ID;
        }

        // 检查是否是数字（在数据库中，类型存储为数字形式 "0", "1", "2" 等）
        bool isInt;
        int typeInt = typeStr.toInt(&isInt);
        if (isInt)
        {
            // 验证整数值是否在有效范围内
            if (typeInt >= 0 && typeInt <= 7) // 假设枚举最大值为7 (JSON_PROPERTIES)
            {
                return static_cast<ColumnDataType>(typeInt);
            }
        }

        // 如果不是数字或数字超出范围，则尝试按文本匹配（向后兼容）
        QString upperTypeStr = typeStr.toUpper();

        if (upperTypeStr == "TEXT")
            return ColumnDataType::TEXT;
        if (upperTypeStr == "INTEGER")
            return ColumnDataType::INTEGER;
        if (upperTypeStr == "REAL")
            return ColumnDataType::REAL;
        if (upperTypeStr == "INSTRUCTION_ID")
            return ColumnDataType::INSTRUCTION_ID;
        if (upperTypeStr == "TIMESET_ID")
            return ColumnDataType::TIMESET_ID;
        if (upperTypeStr == "PIN_STATE_ID" || upperTypeStr == "PIN")
            return ColumnDataType::PIN_STATE_ID;
        if (upperTypeStr == "BOOLEAN")
            return ColumnDataType::BOOLEAN;
        if (upperTypeStr == "JSON_PROPERTIES")
            return ColumnDataType::JSON_PROPERTIES;

        qWarning() << "Unrecognized column type string:" << typeStr << ", defaulting to TEXT";
        return ColumnDataType::TEXT; // Default to TEXT for unrecognized types
    }

} // namespace Vector

#endif // VECTOR_DATA_TYPES_H