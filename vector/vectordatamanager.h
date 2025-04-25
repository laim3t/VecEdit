#ifndef VECTORDATAMANAGER_H
#define VECTORDATAMANAGER_H

#include <QDialog>
#include <QTableWidget>
#include "timeset/timesetdataaccess.h"

class VectorDataManager
{
public:
    VectorDataManager(TimeSetDataAccess *dataAccess);

    // 向量数据方法
    void showVectorDataDialog(int tableId, const QString &tableName, QWidget *parent);
    void addVectorRow(QTableWidget *table, const QStringList &pinOptions, int rowIdx);

private:
    TimeSetDataAccess *m_dataAccess;
};

#endif // VECTORDATAMANAGER_H