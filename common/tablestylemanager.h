#ifndef TABLESTYLEMANAGER_H
#define TABLESTYLEMANAGER_H

#include <QTableWidget>
#include <QTableView>
#include <QHeaderView>
#include <QDebug>
#include <QHash>
#include <QString>

class TableStyleManager
{
public:
    // 预定义的样式类型
    enum StyleClass
    {
        Default,      // 默认样式
        Numeric,      // 数值型单元格
        DateTime,     // 日期时间单元格
        Status,       // 状态信息单元格
        HeaderPin,    // 管脚表头
        PinCell,      // 管脚单元格
        HighlightRow, // 高亮行
        EditableCell, // 可编辑单元格
        ReadOnlyCell  // 只读单元格
    };

    // 对QTableWidget进行风格设置
    static void applyTableStyle(QTableWidget *table);

    // 对QTableWidget进行风格设置（优化版，减少重绘次数）
    static void applyTableStyleOptimized(QTableWidget *table);

    // 批量设置表格样式（高性能版）
    static void applyBatchTableStyle(QTableWidget *table);

    // 对QTableView进行风格设置
    static void applyTableStyle(QTableView *tableView);

    // 根据列内容类型设置列对齐方式
    static void setColumnAlignments(QTableWidget *table);

    // 设置表格列对齐方式
    static void setColumnAlignment(QTableWidget *table, int column, Qt::Alignment alignment);

    // 设置行高
    static void setRowHeight(QTableWidget *table, int rowHeight = 28);

    // 刷新表格显示
    static void refreshTable(QTableWidget *table);

    // 设置管脚列的列宽，使六列管脚平均分配剩余空间
    static void setPinColumnWidths(QTableWidget *table);

    // 应用预定义的样式类到单元格
    static void applyCellStyleClass(QTableWidget *table, int row, int column, StyleClass styleClass);

    // 应用预定义的样式类到整列
    static void applyColumnStyleClass(QTableWidget *table, int column, StyleClass styleClass);

private:
    // 获取预定义样式类的CSS字符串
    static QString getStyleClassCSS(StyleClass styleClass);

    // 预定义的样式类映射
    static QHash<StyleClass, QString> styleClassMap;

    // 初始化样式类映射
    static void initStyleClassMap();

    // 样式类映射是否已初始化
    static bool styleClassMapInitialized;
};

#endif // TABLESTYLEMANAGER_H