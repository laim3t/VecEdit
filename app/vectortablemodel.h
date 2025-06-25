#ifndef VECTORTABLEMODEL_H
#define VECTORTABLEMODEL_H

#include <QAbstractTableModel>

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
};

#endif // VECTORTABLEMODEL_H