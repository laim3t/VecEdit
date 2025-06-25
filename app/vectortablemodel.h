#ifndef VECTORTABLEMODEL_H
#define VECTORTABLEMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QVariant>
#include "../vector/vector_data_types.h"

/**
 * @brief VectorTableModel类实现了基于QAbstractTableModel的向量表数据模型
 *
 * 该类用于在Model/View架构中表示向量表数据，替代之前使用的QTableWidget。
 * 它负责管理数据和与VectorDataHandler的交互，而视觉表现则由QTableView负责。
 */
class VectorTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit VectorTableModel(QObject *parent = nullptr);
    ~VectorTableModel() override;

    // 必须实现的QAbstractTableModel核心虚函数
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // 重写flags方法，添加可编辑标志
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    // 重写setData方法，实现数据编辑
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    // 加载指定表格的指定页数据
    void loadPage(int tableId, int page);

private:
    // 分页状态相关成员变量
    int m_tableId;     // 当前向量表ID
    int m_currentPage; // 当前页码（从0开始）
    int m_pageSize;    // 每页显示的行数
    int m_totalRows;   // 总行数

    // 数据存储
    QList<Vector::RowData> m_pageData;   // 当前页的行数据
    QList<Vector::ColumnInfo> m_columns; // 列信息
};

#endif // VECTORTABLEMODEL_H