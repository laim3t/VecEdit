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
 * @brief 自定义代理类，用于处理向量表中不同列的编辑器类型和渲染方式
 */
class VectorTableDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit VectorTableDelegate(QObject *parent = nullptr);
    ~VectorTableDelegate() override;
    
    // 设置列信息
    void setColumnInfoList(const QList<Vector::ColumnInfo> &columns);

    // 创建编辑器
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    // 设置编辑器数据
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;

    // 获取编辑器数据
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;

    // 重写绘制函数，根据列类型设置不同的渲染样式
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
              const QModelIndex &index) const override;
              
    // 委托需要的大小提示
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

signals:
    // 十六进制值编辑完成信号
    void hexValueEdited(int row, int column, const QString &value) const;

private:
    QList<Vector::ColumnInfo> m_columnInfoList;

    // 获取指定索引的列类型
    Vector::ColumnDataType getColumnType(int column) const;
    
    // 获取指令选项
    QStringList getInstructionOptions() const;

    // 获取TimeSet选项
    QStringList getTimeSetOptions() const;

    // 返回Capture选项（Y/N）
    QStringList getCaptureOptions() const;
};

#endif // VECTORTABLEDELEGATE_H