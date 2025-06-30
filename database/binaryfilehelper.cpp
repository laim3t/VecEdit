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
#include <QSqlQuery>
#include <QSqlError>
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
    // Static initializer for setting module log level at startup
    static struct BinaryFileHelperLoggerInitializer
    {
        BinaryFileHelperLoggerInitializer()
        {
            Logger::instance().setModuleLogLevel("BinaryFileHelper", Logger::LogLevel::Debug);
        }
    } __binaryFileHelperLoggerInitializer;

    // NOTE: All concrete implementations are now located in the
    // `binaryfilehelper_*.cpp` auxiliary files.
    // This central file is intentionally kept clean of implementations.
} // namespace Persistence
