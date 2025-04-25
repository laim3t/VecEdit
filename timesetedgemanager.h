#ifndef TIMESETEDGEMANAGER_H
#define TIMESETEDGEMANAGER_H

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QMap>
#include <QBrush>
#include "timesetdataaccess.h"

class TimeSetEdgeManager
{
public:
    TimeSetEdgeManager(QTreeWidget *tree, TimeSetDataAccess *dataAccess);

    // 边缘操作方法
    void addEdgeItem(QTreeWidgetItem *parentItem, const TimeSetEdgeData &edgeData);
    bool removeEdgeItem(QTreeWidgetItem *item);
    void editEdgeItem(QTreeWidgetItem *item, int column, const QMap<int, QString> &waveOptions);
    void updateEdgeItemText(QTreeWidgetItem *edgeItem, const TimeSetEdgeData &edgeData, const QMap<int, QString> &waveOptions);
    void updateAllEdgeItemsDisplay(const QMap<int, QString> &waveOptions);

    // 从UI获取边缘数据
    QList<TimeSetEdgeData> getEdgeDataFromUI(QTreeWidgetItem *timeSetItem, int timeSetId);

    // 显示现有边缘数据
    void displayTimeSetEdges(QTreeWidgetItem *timeSetItem, const QList<TimeSetEdgeData> &edges,
                             const QMap<int, QString> &waveOptions, const QMap<int, QString> &pinList);

private:
    QTreeWidget *m_treeWidget;
    TimeSetDataAccess *m_dataAccess;
};

#endif // TIMESETEDGEMANAGER_H