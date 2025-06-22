#ifndef VECTORTABLEMODEL_H
#define VECTORTABLEMODEL_H

#include <QAbstractTableModel>
#include <QSet>
#include "vector/vector_data_types.h" // 包含依赖

// 前置声明
class VectorDataHandler;

class VectorTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit VectorTableModel(QObject *parent = nullptr);
    ~VectorTableModel();

    // QAbstractTableModel覆盖方法
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    
    // 表格加载与清理
    void loadTable(int tableId, VectorDataHandler* dataHandler);
    void clear();
    
    // 列信息和数据管理
    void setColumnInfo(const QList<Vector::ColumnInfo>& columnInfo);
    void setTotalRowCount(int totalRows);
    void loadTable(const QList<QList<QVariant>>& data, int startRow);
    
    // 辅助方法
    int currentTableId() const { return m_tableId; }
    void markRowAsModified(int row);
    
signals:
    // 行被修改的信号
    void rowModified(int row);

private:
    VectorDataHandler* m_dataHandler = nullptr;
    QList<Vector::ColumnInfo> m_columnInfoList;
    int m_rowCount = 0;
    int m_tableId = -1;
    
    // 页面数据
    QList<QList<QVariant>> m_pageData;
    int m_startRow = 0;
    
    // 已修改的行集合
    QSet<int> m_modifiedRows;
};

#endif // VECTORTABLEMODEL_H 