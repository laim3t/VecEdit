#ifndef VECTORTABLEDELEGATE_H
#define VECTORTABLEDELEGATE_H

#include <QStyledItemDelegate>
#include <QComboBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QMainWindow>
#include <QApplication>
#include <QPainter>
#include "database/databasemanager.h"
#include "vector/vector_data_types.h"
#include "vector/vectortablemodel.h"

/**
 * @brief 自定义代理类，用于处理向量表中不同列的编辑器类型和渲染方式
 */
class VectorTableItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit VectorTableItemDelegate(QObject *parent = nullptr, const QVector<int> &cellEditTypes = QVector<int>());
    ~VectorTableItemDelegate() override;

    /**
     * @brief 自定义绘制单元格
     * @param painter 绘图设备
     * @param option 样式选项
     * @param index 模型索引
     */
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    /**
     * @brief 创建编辑器
     * @param parent 父控件
     * @param option 样式选项
     * @param index 模型索引
     * @return 编辑器控件
     */
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    /**
     * @brief 设置编辑器数据
     * @param editor 编辑器控件
     * @param index 模型索引
     */
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;

    /**
     * @brief 获取编辑器数据
     * @param editor 编辑器控件
     * @param model 数据模型
     * @param index 模型索引
     */
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;

    /**
     * @brief 更新编辑器几何
     * @param editor 编辑器控件
     * @param option 样式选项
     * @param index 模型索引
     */
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    /**
     * @brief 刷新缓存数据
     */
    void refreshCache();

    /**
     * @brief 刷新表ID缓存
     */
    void refreshTableIdCache();

private:
    /**
     * @brief 获取指令选项
     * @return 指令选项列表
     */
    QStringList getInstructionOptions() const;

    /**
     * @brief 获取TimeSet选项
     * @return TimeSet选项列表
     */
    QStringList getTimeSetOptions() const;

    /**
     * @brief 返回Capture选项（Y/N）
     * @return Capture选项列表
     */
    QStringList getCaptureOptions() const;

    /**
     * @brief 辅助函数：获取当前表的ID
     * @return 当前表ID
     */
    int getTableIdForCurrentTable() const;

    /**
     * @brief 辅助函数：通过UI索引获取列信息
     * @param tableId 表ID
     * @param uiColumnIndex UI列索引
     * @return 列信息
     */
    Vector::ColumnInfo getColumnInfoByIndex(int tableId, int uiColumnIndex) const;

    /**
     * @brief 辅助函数：获取表的列配置
     * @param tableId 表ID
     * @return 列配置列表
     */
    QList<Vector::ColumnInfo> getColumnConfiguration(int tableId) const;

    /**
     * @brief 辅助函数：获取向量表选择器指针
     * @return 向量表选择器指针
     */
    QObject *getVectorTableSelectorPtr() const;

    /**
     * @brief 辅助函数：从模型中获取列信息
     * @param model 数据模型
     * @param column 列索引
     * @return 列信息
     */
    Vector::ColumnInfo getColumnInfoFromModel(const QAbstractItemModel *model, int column) const;

    /**
     * @brief 辅助函数：获取单元格的实际显示文本
     * @param index 模型索引
     * @return 显示文本
     */
    QString getDisplayText(const QModelIndex &index) const;

    /**
     * @brief 辅助函数：根据列类型决定编辑器类型
     * @param columnType 列数据类型
     * @return 编辑器类型（0:文本, 1:下拉框, ...）
     */
    int getEditorType(Vector::ColumnDataType columnType) const;

    // 静态缓存
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