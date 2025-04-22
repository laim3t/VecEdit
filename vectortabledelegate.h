#ifndef VECTORTABLEDELEGATE_H
#define VECTORTABLEDELEGATE_H

#include <QStyledItemDelegate>
#include <QStringList>

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

private:
    // 缓存指令选项
    mutable QStringList m_instructionOptions;

    // 缓存时间集选项
    mutable QStringList m_timesetOptions;

    // 缓存管脚选项
    mutable QStringList m_pinOptions;

    // 获取指令选项
    QStringList getInstructionOptions() const;

    // 获取时间集选项
    QStringList getTimesetOptions() const;

    // 获取管脚选项
    QStringList getPinOptions() const;
};

#endif // VECTORTABLEDELEGATE_H