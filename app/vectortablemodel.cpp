#include "vectortablemodel.h"

VectorTableModel::VectorTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    // 初始化代码将在后续步骤中添加
}

VectorTableModel::~VectorTableModel()
{
    // 清理代码将在后续步骤中添加
}

int VectorTableModel::rowCount(const QModelIndex &parent) const
{
    // 父索引有效时意味着是树形结构中的子项，表格模型中不应该有子项
    if (parent.isValid())
        return 0;

    // 此处返回0，表示暂无数据行
    return 0;
}

int VectorTableModel::columnCount(const QModelIndex &parent) const
{
    // 父索引有效时意味着是树形结构中的子项，表格模型中不应该有子项
    if (parent.isValid())
        return 0;

    // 此处返回0，表示暂无数据列
    return 0;
}

QVariant VectorTableModel::data(const QModelIndex &index, int role) const
{
    // 索引无效或没有数据时直接返回空值
    if (!index.isValid())
        return QVariant();

    // 此处返回空值，表示暂无数据
    return QVariant();
}

QVariant VectorTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    // 只处理DisplayRole，其他角色使用默认行为
    if (role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);

    // 返回空字符串作为临时的表头
    return QString();
}