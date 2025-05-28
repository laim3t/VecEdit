#ifndef BINARY_FIELD_LENGTHS_H
#define BINARY_FIELD_LENGTHS_H

namespace Persistence
{
    /**
     * @brief 定义二进制数据文件中各字段类型的固定长度
     *
     * 这些常量定义了各种数据类型在二进制存储中的最大长度（以字节为单位）
     */

    // TEXT字段的最大长度（默认）
    constexpr int TEXT_FIELD_MAX_LENGTH = 256;

    // 特定TEXT字段的最大长度
    constexpr int LABEL_FIELD_MAX_LENGTH = 15;   // Label字段最大长度为15字节
    constexpr int COMMENT_FIELD_MAX_LENGTH = 30; // Comment字段最大长度为30字节
    constexpr int EXT_FIELD_MAX_LENGTH = 5;      // EXT字段最大长度为5字节

    // PIN_STATE_ID字段的最大长度 (实际上只需要1个字符)
    constexpr int PIN_STATE_FIELD_MAX_LENGTH = 1;

    // INTEGER、INSTRUCTION_ID、TIMESET_ID字段的最大长度
    constexpr int INTEGER_FIELD_MAX_LENGTH = 4;

    // REAL字段的最大长度
    constexpr int REAL_FIELD_MAX_LENGTH = 8;

    // BOOLEAN字段的最大长度
    constexpr int BOOLEAN_FIELD_MAX_LENGTH = 1;

    // JSON_PROPERTIES字段的最大长度
    constexpr int JSON_PROPERTIES_MAX_LENGTH = 1024;

} // namespace Persistence

#endif // BINARY_FIELD_LENGTHS_H