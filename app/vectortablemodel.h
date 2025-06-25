#ifndef VECTORTABLEMODEL_H
#define VECTORTABLEMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QVariant>
#include <QMap>
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

    // 表格行操作相关方法 - 重写QAbstractTableModel的标准方法
    bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    // 自定义表格行操作方法
    bool deleteSelectedRows(const QList<int> &rowIndexes, QString &errorMessage);
    bool deleteRowsInRange(int fromRow, int toRow, QString &errorMessage);
    bool addNewRow(int timesetId, const QMap<int, QString> &pinValues, QString &errorMessage);

    // 加载指定表格的指定页数据
    void loadPage(int tableId, int page);

    // 设置每页行数
    void setPageSize(int pageSize) { m_pageSize = pageSize; }

    // 保存当前修改的数据
    bool saveData(QString &errorMessage);

    // 获取当前表ID
    int tableId() const { return m_tableId; }

    // 获取当前页码
    int currentPage() const { return m_currentPage; }

    // 获取每页行数
    int pageSize() const { return m_pageSize; }

    // 获取总行数
    int totalRows() const { return m_totalRows; }

    // 获取指令ID（根据名称）
    int getInstructionId(const QString &instructionName) const;

    // 获取TimeSet ID（根据名称）
    int getTimeSetId(const QString &timeSetName) const;

    // 获取指令名称（根据ID）
    QString getInstructionName(int instructionId) const;

    // 获取TimeSet名称（根据ID）
    QString getTimeSetName(int timeSetId) const;

    // 刷新指令和TimeSet的缓存
    void refreshCaches() const;

private:
    // 分页状态相关成员变量
    int m_tableId;     // 当前向量表ID
    int m_currentPage; // 当前页码（从0开始）
    int m_pageSize;    // 每页显示的行数
    int m_totalRows;   // 总行数

    // 数据存储
    QList<Vector::RowData> m_pageData;   // 当前页的行数据
    QList<Vector::ColumnInfo> m_columns; // 列信息

    // 缓存
    mutable QMap<int, QString> m_instructionCache;         // 指令ID到名称的映射
    mutable QMap<QString, int> m_instructionNameToIdCache; // 指令名称到ID的映射
    mutable QMap<int, QString> m_timeSetCache;             // TimeSet ID到名称的映射
    mutable QMap<QString, int> m_timeSetNameToIdCache;     // TimeSet名称到ID的映射
    mutable bool m_cachesInitialized;                      // 缓存是否已初始化
};

#endif // VECTORTABLEMODEL_H