#ifndef VECTORTABLEMODEL_H
#define VECTORTABLEMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QMap>
#include <QSet>
#include <QDateTime>
#include <QColor>
#include "vector/vector_data_types.h"

/**
 * @brief 向量表数据模型类，用于实现基于模型/视图架构的向量表
 *
 * 该模型负责管理向量表的数据，支持虚拟滚动和数据分段加载，
 * 极大提高大数据量表格的性能和响应速度。
 */
class VectorTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit VectorTableModel(QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~VectorTableModel() override;

    /**
     * @brief 返回行数
     * @param parent 父索引（对于表格模型通常为无效索引）
     * @return 表格总行数
     */
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    /**
     * @brief 返回列数
     * @param parent 父索引（对于表格模型通常为无效索引）
     * @return 表格列数
     */
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    /**
     * @brief 获取单元格数据
     * @param index 单元格索引
     * @param role 数据角色（显示、编辑、颜色等）
     * @return 单元格数据
     */
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    /**
     * @brief 获取表头数据
     * @param section 行或列序号
     * @param orientation 方向（水平或垂直）
     * @param role 数据角色
     * @return 表头数据
     */
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    /**
     * @brief 设置单元格数据
     * @param index 单元格索引
     * @param value 要设置的值
     * @param role 数据角色（通常为编辑角色）
     * @return 是否设置成功
     */
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    /**
     * @brief 获取单元格标志
     * @param index 单元格索引
     * @return 单元格标志（可编辑、可选择等）
     */
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    /**
     * @brief 加载指定ID的向量表
     * @param tableId 向量表ID
     * @return 是否加载成功
     */
    bool loadTable(int tableId);

    /**
     * @brief 清除数据
     */
    void clearData();

    /**
     * @brief 获取当前表ID
     * @return 向量表ID
     */
    int getTableId() const;

    /**
     * @brief 获取所有列信息
     * @return 列信息列表
     */
    QList<Vector::ColumnInfo> getColumns() const;

    /**
     * @brief 获取列信息
     * @param column 列索引
     * @return 列信息
     */
    Vector::ColumnInfo getColumnInfo(int column) const;

    /**
     * @brief 插入行
     * @param row 插入位置
     * @param count 行数
     * @param parent 父索引（对于表格模型通常为无效索引）
     * @return 是否插入成功
     */
    bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    /**
     * @brief 删除行
     * @param row 删除的起始行
     * @param count 删除的行数
     * @param parent 父索引（对于表格模型通常为无效索引）
     * @return 是否删除成功
     */
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    /**
     * @brief 保存数据到数据库和二进制文件
     * @return 是否保存成功
     */
    bool saveData();

    /**
     * @brief 获取指定行的原始数据
     * @param row 行索引
     * @return 行数据
     */
    Vector::RowData getRowData(int row) const;

    /**
     * @brief 设置行数据
     * @param row 行索引
     * @param rowData 行数据
     * @return 是否设置成功
     */
    bool setRowData(int row, const Vector::RowData &rowData);

    /**
     * @brief 获取行的背景色（用于交替行颜色等）
     * @param row 行索引
     * @return 背景色
     */
    QColor getRowBackground(int row) const;

    /**
     * @brief 检查行是否已被修改
     * @param row 行索引
     * @return 是否已修改
     */
    bool isRowModified(int row) const;

    /**
     * @brief 标记行已被修改
     * @param row 行索引
     */
    void markRowAsModified(int row);

    /**
     * @brief 清除所有修改标记
     */
    void clearModifiedRows();

signals:
    /**
     * @brief 加载进度信号
     * @param percentage 完成百分比
     */
    void loadProgress(int percentage);

    /**
     * @brief 数据加载完成信号
     */
    void loadFinished();

private:
    // 数据缓存段
    struct CacheSegment
    {
        int startRow;
        int count;
        QList<Vector::RowData> rows;

        bool contains(int row) const
        {
            return row >= startRow && row < startRow + count;
        }
    };

    // 数据成员
    int m_tableId;                           // 当前表ID
    QString m_binaryFilePath;                // 二进制文件路径
    QList<Vector::ColumnInfo> m_columns;     // 列信息
    int m_schemaVersion;                     // Schema版本
    mutable int m_totalRowCount;             // 总行数
    mutable QList<CacheSegment> m_dataCache; // 数据缓存
    QSet<int> m_modifiedRows;                // 已修改的行
    QMap<int, QVariant> m_editBuffer;        // 编辑缓冲（行列索引到值的映射）
    QColor m_alternateRowColor;              // 交替行颜色

    // 定义索引键类型
    typedef QPair<int, int> IndexKey; // (row, column)
    typedef QMap<IndexKey, QVariant> EditBufferMap;
    EditBufferMap m_pendingEdits; // 等待保存的编辑

    // 缓存相关
    static const int DEFAULT_CACHE_SEGMENT_SIZE = 500; // 默认缓存段大小
    int m_cacheSegmentSize;                            // 缓存段大小

    /**
     * @brief 加载数据段到缓存
     * @param startRow 起始行
     * @param count 行数
     * @return 是否加载成功
     */
    bool loadDataSegment(int startRow, int count) const;

    /**
     * @brief 从数据库加载行
     * @param startRow 起始行
     * @param count 行数
     * @param rows 行数据输出
     * @return 是否加载成功
     */
    bool loadRowsFromDatabase(int startRow, int count, QList<Vector::RowData> &rows) const;

    /**
     * @brief 构建缓存索引
     * @param row 需要的行
     * @return 包含该行的缓存段，或nullptr如果没有命中缓存
     */
    const CacheSegment *findCacheSegment(int row) const;

    /**
     * @brief 获取单元格的编辑缓冲值
     * @param row 行索引
     * @param column 列索引
     * @param value 值输出
     * @return 是否在编辑缓冲中
     */
    bool getEditBufferValue(int row, int column, QVariant &value) const;
};

#endif // VECTORTABLEMODEL_H