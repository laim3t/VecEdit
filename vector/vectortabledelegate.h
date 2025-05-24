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
    explicit VectorTableItemDelegate(QObject *parent = nullptr);
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

    // 辅助函数：获取向量表选择器指针
    QObject *getVectorTableSelectorPtr() const;

    // 缓存
    mutable QStringList m_instructionOptions;
    mutable QStringList m_timeSetOptions;
    mutable QStringList m_captureOptions;

    // 表ID缓存
    mutable bool m_tableIdCached;
    mutable int m_cachedTableId;
};

#endif // VECTORTABLEDELEGATE_H