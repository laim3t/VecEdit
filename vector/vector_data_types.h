#ifndef VECTOR_DATA_TYPES_H
#define VECTOR_DATA_TYPES_H

#include <QString>
#include <QList>
#include <QVariant>
#include <QJsonObject>
#include <QDebug> // For qDebug()

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
    inline ColumnDataType columnDataTypeFromString(const QString &typeStr)
    {
        // First, try to convert from a number (to handle legacy data)
        bool isNumeric;
        int numericType = typeStr.toInt(&isNumeric);

        if (isNumeric)
        {
            switch (static_cast<ColumnDataType>(numericType))
            {
            case ColumnDataType::TEXT:
                return ColumnDataType::TEXT;
            case ColumnDataType::INTEGER:
                return ColumnDataType::INTEGER;
            case ColumnDataType::REAL:
                return ColumnDataType::REAL;
            case ColumnDataType::INSTRUCTION_ID:
                return ColumnDataType::INSTRUCTION_ID;
            case ColumnDataType::TIMESET_ID:
                return ColumnDataType::TIMESET_ID;
            case ColumnDataType::PIN_STATE_ID:
                return ColumnDataType::PIN_STATE_ID;
            case ColumnDataType::BOOLEAN:
                return ColumnDataType::BOOLEAN;
            case ColumnDataType::JSON_PROPERTIES:
                return ColumnDataType::JSON_PROPERTIES;
            }
        }

        // If not numeric or not a valid numeric enum, try string comparison
        QString upperTypeStr = typeStr.toUpper(); // 转换为大写以进行不区分大小写的比较

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