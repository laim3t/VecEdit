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
 * @brief VectorTableDelegate类为向量表提供自定义渲染和编辑功能
 *
 * 该类继承自QStyledItemDelegate，负责以下功能：
 * 1. 为特殊类型的单元格（如PIN_STATE_ID）提供自定义编辑器
 * 2. 在单元格未被编辑时，正确绘制单元格内容
 * 3. 在编辑器和模型之间同步数据
 */
class VectorTableDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit VectorTableDelegate(QObject *parent = nullptr);
    ~VectorTableDelegate() override;

    /**
     * @brief 为指定单元格创建适当的编辑器
     * 该函数会根据单元格数据类型，创建并返回不同的编辑控件
     */
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;

    /**
     * @brief 将模型数据设置到编辑器中
     * 该函数在编辑开始时调用，用于将单元格数据填充到编辑控件中
     */
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;

    /**
     * @brief 将编辑器中的数据更新到模型中
     * 该函数在编辑结束时调用，用于将用户输入的数据更新回模型
     */
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;

    /**
     * @brief 自定义绘制函数
     * 该函数负责根据单元格数据类型，使用不同样式绘制单元格内容
     */
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

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

    // 辅助函数：从完整列配置中提取可见列并返回指定索引的列信息
    Vector::ColumnInfo getVisibleColumn(const QList<Vector::ColumnInfo> &allColumns, int uiColumnIndex, int tableId) const;

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

    // 单元格编辑类型，用于支持不同类型的编辑器
    QVector<int> m_cellTypes;
};

#endif // VECTORTABLEDELEGATE_H