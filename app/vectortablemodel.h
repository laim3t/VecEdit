#ifndef VECTORTABLEMODEL_H
#define VECTORTABLEMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QVariant>
#include <QMap>
#include "../vector/vector_data_types.h"

// 前向声明
class RobustVectorDataHandler;

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
    VectorTableModel(QObject *parent = nullptr, bool useNewDataHandler = false, RobustVectorDataHandler *robustDataHandler = nullptr);
    ~VectorTableModel() override;

    // 设置数据处理器架构
    void setUseNewDataHandler(bool useNew);

    // 获取行数和列数
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // 重写flags方法，添加可编辑标志
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    // 重写setData方法，实现数据编辑
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    // 表格行操作相关方法 - 重写QAbstractTableModel的标准方法
    bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    // 自定义表格行操作方法
    bool deleteSelectedRows(const QList<int> &rowIndexes, QString &errorMessage);
    bool deleteRowsInRange(int fromRow, int toRow, QString &errorMessage);
    bool addNewRow(int timesetId, const QMap<int, QString> &pinValues, QString &errorMessage); // 实现在vectortablemodel_1.cpp中

    // 分页相关方法
    void loadPage(int tableId, int page);
    int getPageSize() const { return m_pageSize; }
    void setPageSize(int pageSize) { m_pageSize = pageSize; }
    int getCurrentPage() const { return m_currentPage; }
    int getTotalRows() const { return m_totalRows; }
    int getTotalPages() const { return (m_totalRows + m_pageSize - 1) / m_pageSize; }
    int getTableId() const { return m_tableId; }

    // 兼容旧代码的方法
    void loadAllData(int tableId);                         // 完整实现在vectortablemodel.cpp中
    int currentPage() const { return getCurrentPage(); }   // 兼容旧代码
    int pageSize() const { return getPageSize(); }         // 兼容旧代码
    int getCurrentTableId() const { return getTableId(); } // 兼容旧代码
    void refreshColumns(int tableId);                      // 完整实现在vectortablemodel.cpp中

    // 保存数据
    bool saveData(QString &errorMessage);

    // 重置模型，清空所有数据
    void resetModel();

signals:
    // 数据修改信号，当模型数据被修改时发出
    void dataModified(int row);

private:
    // 获取指令名称（根据ID）和ID（根据名称）
    QString getInstructionName(int instructionId) const;
    int getInstructionId(const QString &instructionName) const;

    // 获取TimeSet名称（根据ID）和ID（根据名称）
    QString getTimeSetName(int timeSetId) const;
    int getTimeSetId(const QString &timeSetName) const;

    // 刷新指令和TimeSet的缓存
    void refreshCaches() const;

    // 表ID
    int m_tableId;
    int m_lastTableId; // 用于跟踪表ID变化

    // 页面数据
    int m_currentPage;
    int m_pageSize;
    int m_totalRows;

    // 数据存储
    QList<Vector::RowData> m_pageData;   // 当前页的所有行数据
    QList<Vector::ColumnInfo> m_columns; // 所有列的信息

    // 缓存
    mutable bool m_cachesInitialized;
    mutable QMap<int, QString> m_instructionCache;         // 指令ID -> 名称缓存
    mutable QMap<QString, int> m_instructionNameToIdCache; // 名称 -> 指令ID缓存
    mutable QMap<int, QString> m_timeSetCache;             // TimeSet ID -> 名称缓存
    mutable QMap<QString, int> m_timeSetNameToIdCache;     // 名称 -> TimeSet ID缓存

    // 数据处理器相关
    bool m_useNewDataHandler;
    RobustVectorDataHandler *m_robustDataHandler;
};

#endif // VECTORTABLEMODEL_H