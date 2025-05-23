#ifndef TABLESTYLEMANAGER_H
#define TABLESTYLEMANAGER_H

#include <QTableWidget>
#include <QTableView>
#include <QHeaderView>
#include <QDebug>

class TableStyleManager
{
public:
    // 对QTableWidget进行风格设置
    static void applyTableStyle(QTableWidget *table);

    // 对QTableWidget进行风格设置（优化版，减少重绘次数）
    static void applyTableStyleOptimized(QTableWidget *table);

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
};

#endif // TABLESTYLEMANAGER_H