#ifndef VECTORTABLEDELEGATE_H
#define VECTORTABLEDELEGATE_H

#include <QStyledItemDelegate>
#include <QComboBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QMainWindow>
#include <QApplication>
#include "database/databasemanager.h"
#include "vector/vector_data_types.h"

/**
 * @brief 自定义代理类，用于处理向量表中不同列的编辑器类型
 */
class VectorTableItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    // 原有构造函数
    explicit VectorTableItemDelegate(QObject *parent = nullptr, const QVector<int> &cellEditTypes = QVector<int>());
    
    // 新增构造函数，允许直接指定表格ID
    explicit VectorTableItemDelegate(int tableId, QObject *parent = nullptr, const QVector<int> &cellEditTypes = QVector<int>());
    
    ~VectorTableItemDelegate() override;

    // 创建编辑器
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    // 设置编辑器数据
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;

    // 获取编辑器数据
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;

    // 刷新缓存数据
    void refreshCache();

    // 刷新表ID缓存
    void refreshTableIdCache();
    
    // 设置表格ID
    void setTableId(int tableId);

private:
    // 获取指令选项
    QStringList getInstructionOptions() const;

    // 获取TimeSet选项
    QStringList getTimeSetOptions() const;

    // 返回Capture选项（Y/N）
    QStringList getCaptureOptions() const;

    // 辅助函数：获取当前表的ID
    int getTableIdForCurrentTable() const;

    // 辅助函数：通过UI索引获取列信息
    Vector::ColumnInfo getColumnInfoByIndex(int tableId, int uiColumnIndex) const;
    
    // 辅助函数：获取表的列配置（新增）
    QList<Vector::ColumnInfo> getColumnConfiguration(int tableId) const;

    // 辅助函数：获取向量表选择器指针
    QObject *getVectorTableSelectorPtr() const;

    // 静态缓存（新增）
    static QMap<int, QList<Vector::ColumnInfo>> s_tableColumnsCache;
    
    // 缓存
    mutable QStringList m_instructionOptions;
    mutable QStringList m_timeSetOptions;
    mutable QStringList m_captureOptions;

    // 表ID缓存
    mutable bool m_tableIdCached;
    mutable int m_cachedTableId;
    
    // 直接指定的表格ID，优先级高于缓存的ID
    int m_specifiedTableId;

    // 单元格编辑类型，用于支持不同类型的编辑器
    QVector<int> m_cellTypes;
};

#endif // VECTORTABLEDELEGATE_H