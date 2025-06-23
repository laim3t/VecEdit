#pragma once
#include <QAbstractTableModel>
#include "vector/vectordatahandler.h"
#include "vector/vector_data_types.h"

class VectorTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit VectorTableModel(VectorDataHandler *handler, QObject *parent = nullptr);
    void refreshModel(int tableId, int page = -1, int pageSize = -1); // -1表示加载全部
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
    VectorDataHandler *m_dataHandler;
    int m_currentTableId;
    int m_rowCountCache;
    QList<Vector::ColumnInfo> m_columnInfoCache;
    // ... 未来可能需要更复杂的数据缓存
};