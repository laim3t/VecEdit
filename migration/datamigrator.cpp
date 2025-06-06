#include "datamigrator.h"
#include "vector/vectordatahandler.h"
#include "database/binaryfilehelper.h"
#include "common/utils/pathutils.h"

#include <QMessageBox>
#include <QFile>
#include <QDebug>

namespace
{
    // 辅助函数：更新二进制文件头中的列计数字段
    bool updateBinaryHeaderColumnCount(int tableId)
    {
        const QString funcName = "updateBinaryHeaderColumnCount";
        qDebug() << funcName << "- 更新表ID的二进制文件头列计数:" << tableId;

        VectorDataHandler &dataHandler = VectorDataHandler::instance();
        QString errorMsg;
        QString binFilePath = dataHandler.resolveBinaryFilePath(tableId, errorMsg);
        if (binFilePath.isEmpty())
        {
            qWarning() << funcName << "- 无法解析二进制文件路径:" << errorMsg;
            return false;
        }

        QFile file(binFilePath);
        if (!file.exists())
        {
            qWarning() << funcName << "- 二进制文件不存在:" << binFilePath;
            return true; // 如果文件不存在，则无需更新，不算失败
        }

        if (!file.open(QIODevice::ReadWrite))
        {
            qWarning() << funcName << "- 无法打开文件进行读写:" << binFilePath;
            return false;
        }

        BinaryFileHeader header;
        if (!Persistence::BinaryFileHelper::readBinaryHeader(&file, header))
        {
            qWarning() << funcName << "- 读取文件头失败:" << binFilePath;
            file.close();
            return false;
        }

        QList<Vector::ColumnInfo> allColumns = dataHandler.getAllColumnInfo(tableId);
        int newColumnCount = allColumns.size();

        if (header.column_count_in_file == newColumnCount)
        {
            qDebug() << funcName << "- 列计数未改变，无需更新。";
            file.close();
            return true;
        }

        header.column_count_in_file = newColumnCount;

        file.seek(0); // 回到文件开头
        if (!Persistence::BinaryFileHelper::writeBinaryHeader(&file, header))
        {
            qWarning() << funcName << "- 写入更新后的文件头失败:" << binFilePath;
            file.close();
            return false;
        }

        file.close();
        qDebug() << funcName << "- 成功将文件" << binFilePath << "的列计数更新为" << newColumnCount;
        return true;
    }

    // 辅助函数：比较两个列配置列表，判断是否发生了可能影响二进制布局的实质性变化
    bool areColumnConfigurationsDifferentSimplified(const QList<Vector::ColumnInfo> &oldCols, const QList<Vector::ColumnInfo> &newCols)
    {
        if (oldCols.size() != newCols.size())
        {
            return true;
        }

        for (int i = 0; i < oldCols.size(); ++i)
        {
            if (oldCols[i].name != newCols[i].name || oldCols[i].order != newCols[i].order)
            {
                return true;
            }
        }
        return false;
    }

    // 辅助函数：将旧的行数据适配到新的列结构
    QList<Vector::RowData> adaptRowDataToNewColumns(const QList<Vector::RowData> &oldData, const QList<Vector::ColumnInfo> &oldCols, const QList<Vector::ColumnInfo> &newCols)
    {
        if (oldData.isEmpty())
        {
            return {};
        }

        QList<Vector::RowData> newDataList;
        newDataList.reserve(oldData.size());

        QMap<QString, QVariant> oldRowMap;
        for (const auto &oldRow : oldData)
        {
            oldRowMap.clear();
            for (int i = 0; i < oldCols.size(); ++i)
            {
                if (i < oldRow.size())
                {
                    oldRowMap[oldCols[i].name] = oldRow[i];
                }
            }

            Vector::RowData newRow;
            for (int i = 0; i < newCols.size(); ++i)
            {
                newRow.append(QVariant());
            }

            for (int i = 0; i < newCols.size(); ++i)
            {
                const auto &newColInfo = newCols[i];
                if (oldRowMap.contains(newColInfo.name))
                {
                    newRow[i] = oldRowMap[newColInfo.name];
                }
                else
                {
                    if (newColInfo.type == Vector::ColumnDataType::PIN_STATE_ID)
                    {
                        newRow[i] = "X";
                    }
                    else
                    {
                        newRow[i] = QVariant();
                    }
                }
            }
            newDataList.append(newRow);
        }
        return newDataList;
    }

} // anonymous namespace

bool DataMigrator::migrateDataIfNeeded(int tableId, const QList<Vector::ColumnInfo> &oldColumns, QWidget *parent)
{
    const QString funcName = "DataMigrator::migrateDataIfNeeded";
    qDebug() << funcName << "- 正在检查表ID:" << tableId << "是否需要数据迁移。";

    VectorDataHandler &dataHandler = VectorDataHandler::instance();
    QList<Vector::ColumnInfo> newColumns = dataHandler.getAllColumnInfo(tableId);

    if (areColumnConfigurationsDifferentSimplified(oldColumns, newColumns))
    {
        qDebug() << funcName << "- 检测到列配置更改，开始执行数据迁移。";

        QString errorMsg;
        QString binFilePath = dataHandler.resolveBinaryFilePath(tableId, errorMsg);
        if (binFilePath.isEmpty())
        {
            QMessageBox::critical(parent, "迁移失败", "无法解析二进制文件路径: " + errorMsg);
            return false;
        }
        int schemaVersion = dataHandler.getSchemaVersion(tableId);

        QList<Vector::RowData> oldRowDataList;
        if (QFile::exists(binFilePath))
        {
            if (!Persistence::BinaryFileHelper::readAllRowsFromBinary(binFilePath, oldColumns, schemaVersion, oldRowDataList))
            {
                QMessageBox::critical(parent, "迁移失败", "读取旧表二进制数据失败: " + binFilePath);
                return false;
            }
        }

        QList<Vector::RowData> newRowDataList = adaptRowDataToNewColumns(oldRowDataList, oldColumns, newColumns);

        if (!updateBinaryHeaderColumnCount(tableId))
        {
            QMessageBox::critical(parent, "迁移失败", "更新二进制文件头失败。");
            return false;
        }

        if (!Persistence::BinaryFileHelper::writeAllRowsToBinary(binFilePath, newColumns, schemaVersion, newRowDataList))
        {
            QMessageBox::critical(parent, "迁移失败", "将适配数据写回二进制文件失败: " + binFilePath);
            return false;
        }

        qDebug() << funcName << "- 数据迁移成功完成。";
        QMessageBox::information(parent, "成功", "数据已成功迁移以适应新的列结构。");
    }
    else
    {
        qDebug() << funcName << "- 列配置未发生实质性变化，无需数据迁移。";
        // 即使无需迁移，也更新一下文件头以确保一致性
        if (!updateBinaryHeaderColumnCount(tableId))
        {
            QMessageBox::warning(parent, "警告", "更新文件头信息失败。");
            return false;
        }
    }
    return true;
}