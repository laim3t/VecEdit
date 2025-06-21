#ifndef VECTORTABLEMODEL_H
#define VECTORTABLEMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QMap>
#include <QSet>
#include <QDateTime>
#include "vector_data_types.h"

/**
 * @brief 向量表数据模型类，用于实现Model/View架构下的数据处理
 * 
 * 该类继承自QAbstractTableModel，提供了向量表数据的展示、编辑和持久化功能
 */
class VectorTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit VectorTableModel(QObject *parent = nullptr);
    ~VectorTableModel() override;

    // QAbstractTableModel必须实现的方法
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    // 加载表格数据
    bool loadTable(int tableId);

    // 保存表格数据
    bool saveTable(QString &errorMessage);

    // 获取当前表格ID
    int getCurrentTableId() const;

    // 获取列配置信息
    QList<Vector::ColumnInfo> getColumnConfiguration() const;

    // 获取行数据
    Vector::RowData getRowData(int row) const;

    // 设置行数据
    bool setRowData(int row, const Vector::RowData &data);

    // 添加行
    bool insertRow(int row, const Vector::RowData &data = Vector::RowData());
    bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    // 删除行
    bool removeRow(int row, const QModelIndex &parent = QModelIndex());
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    // 刷新数据
    void refresh();
    
    // 检查是否有修改的行
    bool hasModifiedRows() const;

signals:
    // 数据保存成功信号
    void dataSaved(int tableId, int rowIndex);

private:
    // 当前表格ID
    int m_tableId;

    // 列配置信息
    QList<Vector::ColumnInfo> m_columns;

    // 行数据
    QList<Vector::RowData> m_rows;

    // 修改的行集合
    QSet<int> m_modifiedRows;

    // 表格元数据
    QString m_binaryFilePath;
    int m_schemaVersion;
    QDateTime m_lastModified;

    // 辅助方法：获取列名
    QString getColumnName(int column) const;

    // 辅助方法：获取列类型
    Vector::ColumnDataType getColumnType(int column) const;

    // 辅助方法：标记行已修改
    void markRowAsModified(int row);

    // 辅助方法：检查行是否已修改
    bool isRowModified(int row) const;

    // 辅助方法：清除修改标记
    void clearModifiedRows();

    // 辅助方法：加载二进制文件
    bool loadBinaryFile(const QString &filePath);

    // 辅助方法：保存二进制文件
    bool saveBinaryFile(const QString &filePath, QString &errorMessage);

    // 辅助方法：格式化显示值
    QVariant formatDisplayValue(const QVariant &value, Vector::ColumnDataType type) const;

    // 辅助方法：解析编辑值
    QVariant parseEditValue(const QVariant &value, Vector::ColumnDataType type) const;
    
    // 辅助方法：解析二进制文件路径
    QString resolveBinaryFilePath(int tableId, QString &errorMsg);
};

#endif // VECTORTABLEMODEL_H 