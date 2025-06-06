#ifndef DATAMIGRATOR_H
#define DATAMIGRATOR_H

#include <QList>
#include <QWidget>
#include "vector/vector_data_types.h"

class DataMigrator
{
public:
    /**
     * @brief 检查列配置是否有变，并在需要时执行数据迁移
     * @param tableId 要检查和迁移的向量表的ID
     * @param oldColumns 迁移前的列配置
     * @param parent 用于显示消息框的父窗口
     * @return bool 如果不需要迁移或迁移成功，返回true；否则返回false
     */
    static bool migrateDataIfNeeded(int tableId, const QList<Vector::ColumnInfo> &oldColumns, QWidget *parent);
};

#endif // DATAMIGRATOR_H