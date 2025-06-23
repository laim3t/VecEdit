#ifndef VECTORTABLEMODEL_H
#define VECTORTABLEMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QMap>
#include <QString>
#include <QVariant>
#include <QVector>
#include "vector_data_types.h"

// 前置声明
class DatabaseManager;

/**
 * @brief VectorTableModel 类是基于Model/View架构的向量表模型
 * 
 * 该类继承自QAbstractTableModel，提供了一个高性能、低内存占用的向量表模型。
 * 它作为VecEdit新架构的基石，直接与DatabaseManager通信，负责向量数据的获取、
 * 缓存、修改和持久化，成为新架构的唯一数据源。
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
    ~VectorTableModel();

    // ===== QAbstractTableModel必须实现的接口 =====
    
    /**
     * @brief 获取行数
     * @param parent 父索引
     * @return 行数
     */
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    
    /**
     * @brief 获取列数
     * @param parent 父索引
     * @return 列数
     */
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    
    /**
     * @brief 获取单元格数据
     * @param index 单元格索引
     * @param role 数据角色
     * @return 单元格数据
     */
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    
    /**
     * @brief 设置单元格数据
     * @param index 单元格索引
     * @param value 新值
     * @param role 数据角色
     * @return 是否设置成功
     */
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    
    /**
     * @brief 获取表头数据
     * @param section 行/列索引
     * @param orientation 方向（水平/垂直）
     * @param role 数据角色
     * @return 表头数据
     */
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    
    /**
     * @brief 获取单元格标志
     * @param index 单元格索引
     * @return 单元格标志
     */
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    // ===== 自定义公共方法 =====
    
    /**
     * @brief 加载指定向量表的数据
     * @param tableId 向量表ID
     * @return 是否加载成功
     */
    bool loadTable(int tableId);
    
    /**
     * @brief 获取当前加载的表ID
     * @return 表ID，如果未加载则返回-1
     */
    int currentTableId() const;
    
    /**
     * @brief 保存表数据到数据库
     * @return 是否保存成功
     */
    bool saveTable();
    
    /**
     * @brief 刷新表数据
     * @return 是否刷新成功
     */
    bool refreshTable();
    
    /**
     * @brief 添加新行
     * @param position 插入位置，默认为末尾
     * @return 是否添加成功
     */
    bool addRow(int position = -1);
    
    /**
     * @brief 删除行
     * @param position 要删除的行位置
     * @return 是否删除成功
     */
    bool removeRow(int position);
    
    /**
     * @brief 批量删除行
     * @param positions 要删除的行位置列表
     * @return 是否全部删除成功
     */
    bool removeRows(const QList<int> &positions);
    
    /**
     * @brief 获取列配置
     * @return 列配置列表
     */
    QList<Vector::ColumnInfo> getColumnConfiguration() const;

private:
    // 表元数据
    int m_tableId;                             // 当前加载的表ID
    QString m_tableName;                       // 表名称
    QList<Vector::ColumnInfo> m_columns;       // 列配置
    
    // 表数据
    Vector::VectorTableData m_rows;            // 行数据 (QList<QList<QVariant>>)
    QMap<int, QMap<int, QVariant>> m_cache;    // 未保存的修改缓存 <行, <列, 值>>
    
    // 标志
    bool m_isModified;                         // 数据是否已修改但未保存
    
    // 辅助方法
    bool loadColumnConfiguration();            // 加载列配置
    bool loadRowData();                        // 加载行数据
    
    // 用于管理行数据
    struct RowMetadata {
        QString label;       // 标签列的数据
        QString comment;     // 注释列的数据
        int timeSetId;       // TimeSet列的数据
    };
    
    QMap<int, RowMetadata> m_rowMetadata;      // 行元数据映射 <行索引, 元数据>
};

#endif // VECTORTABLEMODEL_H 