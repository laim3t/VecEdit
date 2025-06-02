#include "timesetedgemanager.h"
#include "timesetedgedialog.h"
#include <QApplication>
#include <QStyle>
#include <QBrush>
#include <QMessageBox>
#include <QDebug>

TimeSetEdgeManager::TimeSetEdgeManager(QTreeWidget *tree, TimeSetDataAccess *dataAccess)
    : m_treeWidget(tree), m_dataAccess(dataAccess)
{
}

void TimeSetEdgeManager::addEdgeItem(QTreeWidgetItem *parentItem, const TimeSetEdgeData &edgeData)
{
    if (!parentItem)
        return;

    // 创建新的边沿项
    QTreeWidgetItem *edgeItem = new QTreeWidgetItem(parentItem);

    // 设置边沿项数据
    edgeItem->setData(0, Qt::UserRole, edgeData.pinId);
    edgeItem->setData(1, Qt::UserRole, edgeData.t1r);
    edgeItem->setData(2, Qt::UserRole, edgeData.t1f);
    edgeItem->setData(3, Qt::UserRole, edgeData.stbr);
    edgeItem->setData(4, Qt::UserRole, edgeData.waveId);

    // 设置所有列都可编辑
    for (int col = 0; col < 5; col++)
    {
        edgeItem->setFlags(edgeItem->flags() | Qt::ItemIsEditable);
    }
}

bool TimeSetEdgeManager::removeEdgeItem(QTreeWidgetItem *item)
{
    if (!item || !item->parent())
        return false;

    QTreeWidgetItem *parentItem = item->parent();
    int timeSetId = parentItem->data(0, Qt::UserRole).toInt();
    int pinId = item->data(0, Qt::UserRole).toInt();

    // 从数据库删除边沿项
    if (m_dataAccess->deleteTimeSetEdge(timeSetId, pinId))
    {
        // 从树中删除项
        delete item;
        return true;
    }

    return false;
}

void TimeSetEdgeManager::editEdgeItem(QTreeWidgetItem *item, int column, const QMap<int, QString> &waveOptions)
{
    if (!item || !item->parent())
        return;

    // 检查是否是边沿项而不是TimeSet项
    QTreeWidgetItem *parentItem = item->parent();
    if (!parentItem)
        return;

    // 获取当前值
    double currentT1R = item->data(1, Qt::UserRole).toDouble();
    double currentT1F = item->data(2, Qt::UserRole).toDouble();
    double currentSTBR = item->data(3, Qt::UserRole).toDouble();
    int currentWaveId = item->data(4, Qt::UserRole).toInt();

    // 显示编辑对话框
    TimeSetEdgeDialog dialog(currentT1R, currentT1F, currentSTBR, currentWaveId, waveOptions, QApplication::activeWindow());

    if (dialog.exec() == QDialog::Accepted)
    {
        // 更新边沿项数据
        TimeSetEdgeData edgeData;
        edgeData.timesetId = parentItem->data(0, Qt::UserRole).toInt();
        edgeData.pinId = item->data(0, Qt::UserRole).toInt();
        edgeData.t1r = dialog.getT1R();
        edgeData.t1f = dialog.getT1F();
        edgeData.stbr = dialog.getSTBR();
        edgeData.waveId = dialog.getWaveId();

        // 更新显示
        updateEdgeItemText(item, edgeData, waveOptions);

        // 更新存储数据
        item->setData(1, Qt::UserRole, edgeData.t1r);
        item->setData(2, Qt::UserRole, edgeData.t1f);
        item->setData(3, Qt::UserRole, edgeData.stbr);
        item->setData(4, Qt::UserRole, edgeData.waveId);

        // 保存到数据库
        QList<TimeSetEdgeData> edges = getEdgeDataFromUI(parentItem, edgeData.timesetId);
        m_dataAccess->saveTimeSetEdgesToDatabase(edgeData.timesetId, edges);
    }
}

void TimeSetEdgeManager::updateEdgeItemText(QTreeWidgetItem *edgeItem, const TimeSetEdgeData &edgeData, const QMap<int, QString> &waveOptions)
{
    if (!edgeItem)
        return;

    QString waveName = waveOptions.value(edgeData.waveId, "未知");

    // 更新文本
    edgeItem->setText(1, QString::number(edgeData.t1r));
    edgeItem->setText(2, QString::number(edgeData.t1f));
    edgeItem->setText(3, QString::number(edgeData.stbr));
    edgeItem->setText(4, waveName);

    // 设置对齐方式
    for (int i = 1; i < 5; i++)
    {
        edgeItem->setTextAlignment(i, Qt::AlignCenter);
    }
}

void TimeSetEdgeManager::updateAllEdgeItemsDisplay(const QMap<int, QString> &waveOptions)
{
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); i++)
    {
        QTreeWidgetItem *timeSetItem = m_treeWidget->topLevelItem(i);

        for (int j = 0; j < timeSetItem->childCount(); j++)
        {
            QTreeWidgetItem *edgeItem = timeSetItem->child(j);

            TimeSetEdgeData edgeData;
            edgeData.timesetId = timeSetItem->data(0, Qt::UserRole).toInt();
            edgeData.pinId = edgeItem->data(0, Qt::UserRole).toInt();
            edgeData.t1r = edgeItem->data(1, Qt::UserRole).toDouble();
            edgeData.t1f = edgeItem->data(2, Qt::UserRole).toDouble();
            edgeData.stbr = edgeItem->data(3, Qt::UserRole).toDouble();
            edgeData.waveId = edgeItem->data(4, Qt::UserRole).toInt();

            updateEdgeItemText(edgeItem, edgeData, waveOptions);
        }
    }
}

QList<TimeSetEdgeData> TimeSetEdgeManager::getEdgeDataFromUI(QTreeWidgetItem *timeSetItem, int timeSetId)
{
    QList<TimeSetEdgeData> edges;

    if (!timeSetItem)
        return edges;

    for (int i = 0; i < timeSetItem->childCount(); i++)
    {
        QTreeWidgetItem *edgeItem = timeSetItem->child(i);

        TimeSetEdgeData edge;
        edge.timesetId = timeSetId;
        edge.pinId = edgeItem->data(0, Qt::UserRole).toInt();
        edge.t1r = edgeItem->data(1, Qt::UserRole).toDouble();
        edge.t1f = edgeItem->data(2, Qt::UserRole).toDouble();
        edge.stbr = edgeItem->data(3, Qt::UserRole).toDouble();
        edge.waveId = edgeItem->data(4, Qt::UserRole).toInt();

        edges.append(edge);
    }

    return edges;
}

void TimeSetEdgeManager::displayTimeSetEdges(QTreeWidgetItem *timeSetItem, const QList<TimeSetEdgeData> &edges,
                                             const QMap<int, QString> &waveOptions, const QMap<int, QString> &pinList)
{
    if (!timeSetItem)
        return;

    // 清除现有子项
    timeSetItem->takeChildren();

    // 添加所有边沿项
    for (const TimeSetEdgeData &edge : edges)
    {
        QTreeWidgetItem *edgeItem = new QTreeWidgetItem(timeSetItem);

        // 设置管脚名称
        QString pinName = pinList.value(edge.pinId, "未知管脚");
        edgeItem->setText(0, pinName);

        // 存储数据
        edgeItem->setData(0, Qt::UserRole, edge.pinId);
        edgeItem->setData(1, Qt::UserRole, edge.t1r);
        edgeItem->setData(2, Qt::UserRole, edge.t1f);
        edgeItem->setData(3, Qt::UserRole, edge.stbr);
        edgeItem->setData(4, Qt::UserRole, edge.waveId);

        // 更新显示文本
        updateEdgeItemText(edgeItem, edge, waveOptions);

        // 设置背景色
        QBrush backgroundBrush(QColor(245, 245, 245));
        for (int col = 0; col < 5; col++)
        {
            edgeItem->setBackground(col, backgroundBrush);
        }

        // 设置所有列都可编辑
        for (int col = 0; col < 5; col++)
        {
            edgeItem->setFlags(edgeItem->flags() | Qt::ItemIsEditable);
        }
    }

    // 展开项
    timeSetItem->setExpanded(true);
}