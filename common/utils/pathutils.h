#ifndef PATHUTILS_H
#define PATHUTILS_H

#include <QString>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

namespace Utils
{

    class PathUtils
    {
    public:
        /**
         * @brief 生成项目特定二进制数据目录的绝对路径。
         *
         * 该目录通常以数据库文件命名，添加"_vbindata"后缀，
         * 并位于数据库文件所在的同一目录中。
         * 示例：如果dbFilePath为 "C:/Projects/MyData.db"，
         *       返回 "C:/Projects/MyData_vbindata"。
         *
         * @param currentDbFilePath 当前SQLite数据库文件的绝对路径。
         * @return QString 二进制数据目录的绝对路径。
         */
        static QString getProjectBinaryDataDirectory(const QString &currentDbFilePath)
        {
            const QString funcName = "Utils::PathUtils::getProjectBinaryDataDirectory";
            qDebug() << funcName << "- 输入数据库路径:" << currentDbFilePath;

            if (currentDbFilePath.isEmpty())
            {
                qWarning() << funcName << "- 数据库路径为空，返回空字符串";
                return QString();
            }

            QFileInfo dbFileInfo(currentDbFilePath);
            QString baseName = dbFileInfo.completeBaseName(); // 从"MyData.db"中获取"MyData"
            QString dbPath = dbFileInfo.absolutePath();       // 获取"C:/Projects"

            if (baseName.isEmpty() || dbPath.isEmpty())
            {
                qWarning() << funcName << "- 无法从以下路径获取数据库基础名称或路径：" << currentDbFilePath;
                return QString();
            }

            QString binaryDataDir = dbPath + QDir::separator() + baseName + "_vbindata";
            // 确保路径使用一致的分隔符格式
            binaryDataDir = QDir::toNativeSeparators(binaryDataDir);
            qDebug() << funcName << "- 生成的项目二进制数据目录：" << binaryDataDir;
            return binaryDataDir;
        }
    };

} // namespace Utils

#endif // PATHUTILS_H