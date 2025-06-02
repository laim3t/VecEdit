#include "tablestylemanager.h"
#include <QColor>
#include <QScrollBar>
#include <QApplication>
#include <QStyleFactory>
#include <QMetaEnum>
#include <QElapsedTimer>

// 初始化静态成员变量
QHash<TableStyleManager::StyleClass, QString> TableStyleManager::styleClassMap;
bool TableStyleManager::styleClassMapInitialized = false;

void TableStyleManager::applyTableStyle(QTableWidget *table)
{
    const QString funcName = "TableStyleManager::applyTableStyle(QTableWidget*)";
    qDebug() << funcName << " - Entry";

    if (!table)
    {
        qWarning() << funcName << " - 错误：表格对象为空";
        return;
    }

    qDebug() << funcName << " - 开始设置表格样式";

    // 设置表格边框和网格线
    table->setShowGrid(true);
    table->setStyleSheet(
        "QTableWidget {"
        "   border: 1px solid #c0c0c0;"
        "   gridline-color: #d0d0d0;"
        "   selection-background-color: #e0e7ff;"
        "}"
        "QTableWidget::item {"
        "   border: 0px;"
        "   padding-left: 3px;"
        "}"
        "QTableWidget::item:selected {"
        "   background-color: #e0e7ff;"
        "   color: black;"
        "}"
        "QHeaderView::section {"
        "   background-color: #f0f0f0;"
        "   border: 1px solid #c0c0c0;"
        "   font-weight: bold;"
        "   padding: 4px;"
        "   text-align: center;"
        "}"
        "QHeaderView::section:checked {"
        "   background-color: #e0e0e0;"
        "}");

    // 设置行高
    setRowHeight(table);

    // 设置列对齐方式
    setColumnAlignments(table);

    // 设置管脚列的列宽
    setPinColumnWidths(table);

    // 刷新显示
    refreshTable(table);

    qDebug() << funcName << " - 表格样式设置完成";
}

void TableStyleManager::applyTableStyleOptimized(QTableWidget *table)
{
    const QString funcName = "TableStyleManager::applyTableStyleOptimized";
    qDebug() << funcName << " - Entry";

    if (!table)
    {
        qWarning() << funcName << " - 错误：表格对象为空";
        return;
    }

    qDebug() << funcName << " - 开始设置表格样式（优化版）";

    // 完全禁用表格更新，减少重绘次数
    table->setUpdatesEnabled(false);

    // 暂时禁用所有子组件的重绘和布局更新，进一步减少UI更新
    table->viewport()->setUpdatesEnabled(false);
    table->horizontalHeader()->setUpdatesEnabled(false);
    table->verticalHeader()->setUpdatesEnabled(false);

    // 暂时禁用滚动条更新并记录当前位置
    QScrollBar *hScrollBar = table->horizontalScrollBar();
    QScrollBar *vScrollBar = table->verticalScrollBar();
    int hScrollValue = hScrollBar ? hScrollBar->value() : 0;
    int vScrollValue = vScrollBar ? vScrollBar->value() : 0;

    if (hScrollBar)
        hScrollBar->setUpdatesEnabled(false);
    if (vScrollBar)
        vScrollBar->setUpdatesEnabled(false);

    // 开始批量处理，避免每次小改动都触发信号
    table->blockSignals(true);

    // 设置表格边框和网格线
    table->setShowGrid(true);
    table->setStyleSheet(
        "QTableWidget {"
        "   border: 1px solid #c0c0c0;"
        "   gridline-color: #d0d0d0;"
        "   selection-background-color: #e0e7ff;"
        "}"
        "QTableWidget::item {"
        "   border: 0px;"
        "   padding-left: 3px;"
        "}"
        "QTableWidget::item:selected {"
        "   background-color: #e0e7ff;"
        "   color: black;"
        "}"
        "QHeaderView::section {"
        "   background-color: #f0f0f0;"
        "   border: 1px solid #c0c0c0;"
        "   font-weight: bold;"
        "   padding: 4px;"
        "   text-align: center;"
        "}"
        "QHeaderView::section:checked {"
        "   background-color: #e0e0e0;"
        "}");

    // 一次性设置所有行的行高
    int rowHeight = 28; // 默认行高
    table->verticalHeader()->setDefaultSectionSize(rowHeight);

    // 批量设置列对齐方式
    int columnCount = table->columnCount();
    for (int col = 0; col < columnCount; col++)
    {
        QTableWidgetItem *headerItem = table->horizontalHeaderItem(col);
        if (!headerItem)
            continue;

        QString headerText = headerItem->text();
        Qt::Alignment alignment;

        // 检查是否包含管脚信息（格式为"名称\nx数量\n类型"）
        if (headerText.contains("\n"))
        {
            // 管脚列，居中对齐
            alignment = Qt::AlignCenter;
        }
        // 数值类型列靠右对齐
        else if (headerText.contains("数量") || headerText.contains("数值") ||
                 headerText.contains("count") || headerText.contains("value") ||
                 headerText.contains("amount") || headerText.contains("id") ||
                 headerText.contains("ID"))
        {
            alignment = Qt::AlignRight | Qt::AlignVCenter;
        }
        // 日期时间类型居中对齐
        else if (headerText.contains("日期") || headerText.contains("时间") ||
                 headerText.contains("date") || headerText.contains("time") ||
                 headerText.contains("timeset"))
        {
            alignment = Qt::AlignCenter;
        }
        // 状态、类型等短文本居中对齐
        else if (headerText.contains("状态") || headerText.contains("类型") ||
                 headerText.contains("status") || headerText.contains("type") ||
                 headerText.contains("capture") || headerText.contains("ext"))
        {
            alignment = Qt::AlignCenter;
        }
        // 默认为靠左对齐
        else
        {
            alignment = Qt::AlignLeft | Qt::AlignVCenter;
        }

        // 设置表头对齐方式
        headerItem->setTextAlignment(Qt::AlignCenter); // 表头总是居中对齐

        // 同时设置所有单元格的对齐方式，避免后续逐个设置
        for (int row = 0; row < table->rowCount(); row++)
        {
            QTableWidgetItem *item = table->item(row, col);
            if (item)
            {
                item->setTextAlignment(alignment);
            }
        }
    }

    // 设置管脚列的列宽（只调用一次）
    setPinColumnWidths(table);

    // 恢复滚动条位置
    if (hScrollBar)
    {
        hScrollBar->setUpdatesEnabled(true);
        hScrollBar->setValue(hScrollValue);
    }
    if (vScrollBar)
    {
        vScrollBar->setUpdatesEnabled(true);
        vScrollBar->setValue(vScrollValue);
    }

    // 恢复信号处理
    table->blockSignals(false);

    // 恢复表格更新（按顺序启用各部分的更新）
    table->horizontalHeader()->setUpdatesEnabled(true);
    table->verticalHeader()->setUpdatesEnabled(true);
    table->viewport()->setUpdatesEnabled(true);
    table->setUpdatesEnabled(true);

    // 最后一次性刷新显示
    table->viewport()->update();

    qDebug() << funcName << " - 表格样式设置完成（优化版）";
}

void TableStyleManager::applyTableStyle(QTableView *tableView)
{
    const QString funcName = "TableStyleManager::applyTableStyle(QTableView*)";
    qDebug() << funcName << " - Entry";

    if (!tableView)
    {
        qWarning() << funcName << " - 错误：表格视图对象为空";
        return;
    }

    qDebug() << funcName << " - 开始设置表格视图样式";

    // 设置表格边框和网格线
    tableView->setShowGrid(true);
    tableView->setStyleSheet(
        "QTableView {"
        "   border: 1px solid #c0c0c0;"
        "   gridline-color: #d0d0d0;"
        "   selection-background-color: #e0e7ff;"
        "}"
        "QTableView::item {"
        "   border: 0px;"
        "   padding-left: 3px;"
        "}"
        "QTableView::item:selected {"
        "   background-color: #e0e7ff;"
        "   color: black;"
        "}"
        "QHeaderView::section {"
        "   background-color: #f0f0f0;"
        "   border: 1px solid #c0c0c0;"
        "   font-weight: bold;"
        "   padding: 4px;"
        "   text-align: center;"
        "}"
        "QHeaderView::section:checked {"
        "   background-color: #e0e0e0;"
        "}");

    // 设置交替行颜色
    tableView->setAlternatingRowColors(true);

    // 设置默认行高
    tableView->verticalHeader()->setDefaultSectionSize(28);

    qDebug() << funcName << " - 表格视图样式设置完成";
}

void TableStyleManager::setColumnAlignments(QTableWidget *table)
{
    if (!table || table->columnCount() <= 0)
    {
        qDebug() << "TableStyleManager::setColumnAlignments - 错误：表格对象为空或没有列";
        return;
    }

    qDebug() << "TableStyleManager::setColumnAlignments - 开始设置表格列对齐方式";

    // 遍历所有列
    for (int col = 0; col < table->columnCount(); col++)
    {
        QTableWidgetItem *headerItem = table->horizontalHeaderItem(col);
        if (!headerItem)
        {
            qDebug() << "TableStyleManager::setColumnAlignments - 错误：列" << col << "没有表头项";
            continue;
        }

        QString headerText = headerItem->text();

        // 检查是否包含管脚信息（格式为"名称\nx数量\n类型"）
        if (headerText.contains("\n"))
        {
            // 管脚列，居中对齐
            setColumnAlignment(table, col, Qt::AlignCenter);
            qDebug() << "TableStyleManager::setColumnAlignments - 设置管脚列" << col << "居中对齐";
            continue;
        }

        // 数值类型列靠右对齐
        if (headerText.contains("数量") ||
            headerText.contains("数值") ||
            headerText.contains("count") ||
            headerText.contains("value") ||
            headerText.contains("amount") ||
            headerText.contains("id") ||
            headerText.contains("ID"))
        {
            setColumnAlignment(table, col, Qt::AlignRight | Qt::AlignVCenter);
        }
        // 日期时间类型居中对齐
        else if (headerText.contains("日期") ||
                 headerText.contains("时间") ||
                 headerText.contains("date") ||
                 headerText.contains("time") ||
                 headerText.contains("timeset"))
        {
            setColumnAlignment(table, col, Qt::AlignCenter);
        }
        // 状态、类型等短文本居中对齐
        else if (headerText.contains("状态") ||
                 headerText.contains("类型") ||
                 headerText.contains("status") ||
                 headerText.contains("type") ||
                 headerText.contains("capture") ||
                 headerText.contains("ext"))
        {
            setColumnAlignment(table, col, Qt::AlignCenter);
        }
        // 默认为靠左对齐
        else
        {
            setColumnAlignment(table, col, Qt::AlignLeft | Qt::AlignVCenter);
        }
    }

    qDebug() << "TableStyleManager::setColumnAlignments - 列对齐方式设置完成";
}

void TableStyleManager::setColumnAlignment(QTableWidget *table, int column, Qt::Alignment alignment)
{
    if (!table || column < 0 || column >= table->columnCount())
    {
        qDebug() << "TableStyleManager::setColumnAlignment - 错误：表格对象为空或列索引无效";
        return;
    }

    qDebug() << "TableStyleManager::setColumnAlignment - 设置列" << column << "的对齐方式";

    // 设置表头对齐方式
    QTableWidgetItem *headerItem = table->horizontalHeaderItem(column);
    if (headerItem)
    {
        headerItem->setTextAlignment(alignment);
    }

    // 设置所有单元格的对齐方式
    for (int row = 0; row < table->rowCount(); row++)
    {
        QTableWidgetItem *item = table->item(row, column);
        if (item)
        {
            item->setTextAlignment(alignment);
        }
    }
}

void TableStyleManager::setRowHeight(QTableWidget *table, int rowHeight)
{
    if (!table)
    {
        qDebug() << "TableStyleManager::setRowHeight - 错误：表格对象为空";
        return;
    }

    qDebug() << "TableStyleManager::setRowHeight - 设置表格行高为" << rowHeight << "像素";

    // 设置默认行高
    table->verticalHeader()->setDefaultSectionSize(rowHeight);

    // 强制所有行都使用此行高
    for (int row = 0; row < table->rowCount(); row++)
    {
        table->setRowHeight(row, rowHeight);
    }
}

void TableStyleManager::refreshTable(QTableWidget *table)
{
    if (!table)
    {
        qDebug() << "TableStyleManager::refreshTable - 错误：表格对象为空";
        return;
    }

    qDebug() << "TableStyleManager::refreshTable - 刷新表格显示";

    // 更新表格显示
    table->horizontalHeader()->viewport()->update();
    table->verticalHeader()->viewport()->update();
    table->viewport()->update();

    // 调整滚动条
    if (table->horizontalScrollBar())
    {
        table->horizontalScrollBar()->setValue(table->horizontalScrollBar()->value());
    }
    if (table->verticalScrollBar())
    {
        table->verticalScrollBar()->setValue(table->verticalScrollBar()->value());
    }
}

void TableStyleManager::setPinColumnWidths(QTableWidget *table)
{
    const QString funcName = "TableStyleManager::setPinColumnWidths";
    qDebug() << funcName << " - 开始设置管脚列的列宽";

    if (!table)
    {
        qWarning() << funcName << " - 错误：表格对象为空";
        return;
    }

    int columnCount = table->columnCount();
    if (columnCount <= 0)
    {
        qWarning() << funcName << " - 错误：表格没有列";
        return;
    }

    // 基础列（非管脚列）索引和列宽设置
    const int ADDRESS_COL = 0; // 地址列索引
    const int LABEL_COL = 1;   // 标签列索引
    const int TIMESET_COL = 2; // TimeSet列索引
    const int I_DUT_COL = 3;   // I_DUT列索引
    const int O_DUT_COL = 4;   // O_DUT列索引
    const int IO_DUT_COL = 5;  // IO_DUT列索引

    const int ADDRESS_WIDTH = 80;  // 地址列宽度
    const int LABEL_WIDTH = 120;   // 标签列宽度
    const int TIMESET_WIDTH = 100; // TimeSet列宽度
    const int I_DUT_WIDTH = 80;    // I_DUT列宽度
    const int O_DUT_WIDTH = 80;    // O_DUT列宽度
    const int IO_DUT_WIDTH = 80;   // IO_DUT列宽度

    int totalFixedWidth = ADDRESS_WIDTH + LABEL_WIDTH + TIMESET_WIDTH + I_DUT_WIDTH + O_DUT_WIDTH + IO_DUT_WIDTH;

    // 检查列数是否足够
    if (columnCount <= 6)
    {
        qDebug() << funcName << " - 无需调整管脚列宽度，列数不足：" << columnCount;
        return;
    }

    // 设置基础列的宽度
    if (ADDRESS_COL < columnCount)
    {
        table->setColumnWidth(ADDRESS_COL, ADDRESS_WIDTH);
        table->horizontalHeader()->setSectionResizeMode(ADDRESS_COL, QHeaderView::Interactive);
    }
    if (LABEL_COL < columnCount)
    {
        table->setColumnWidth(LABEL_COL, LABEL_WIDTH);
        table->horizontalHeader()->setSectionResizeMode(LABEL_COL, QHeaderView::Interactive);
    }
    if (TIMESET_COL < columnCount)
    {
        table->setColumnWidth(TIMESET_COL, TIMESET_WIDTH);
        table->horizontalHeader()->setSectionResizeMode(TIMESET_COL, QHeaderView::Interactive);
    }
    if (I_DUT_COL < columnCount)
    {
        table->setColumnWidth(I_DUT_COL, I_DUT_WIDTH);
        table->horizontalHeader()->setSectionResizeMode(I_DUT_COL, QHeaderView::Interactive);
    }
    if (O_DUT_COL < columnCount)
    {
        table->setColumnWidth(O_DUT_COL, O_DUT_WIDTH);
        table->horizontalHeader()->setSectionResizeMode(O_DUT_COL, QHeaderView::Interactive);
    }
    if (IO_DUT_COL < columnCount)
    {
        table->setColumnWidth(IO_DUT_COL, IO_DUT_WIDTH);
        table->horizontalHeader()->setSectionResizeMode(IO_DUT_COL, QHeaderView::Interactive);
    }

    // 识别管脚列（通过列标题中包含的换行符）
    QList<int> pinColumns;
    for (int col = 6; col < columnCount; col++)
    {
        QTableWidgetItem *headerItem = table->horizontalHeaderItem(col);
        if (headerItem && headerItem->text().contains("\n"))
        {
            pinColumns.append(col);
            qDebug() << funcName << " - 检测到管脚列:" << col << headerItem->text();
        }
    }

    // 如果没有找到管脚列，直接返回
    if (pinColumns.isEmpty())
    {
        qDebug() << funcName << " - 未检测到管脚列";
        return;
    }

    // 计算除固定宽度外的可用空间
    int availableWidth = table->width() - totalFixedWidth;
    if (availableWidth <= 0)
    {
        qDebug() << funcName << " - 没有足够的空间分配给管脚列";
        return;
    }

    // 计算每列应分配的宽度
    int widthPerPinColumn = availableWidth / pinColumns.size();
    qDebug() << funcName << " - 每个管脚列分配宽度: " << widthPerPinColumn
             << ", 管脚列数: " << pinColumns.size()
             << ", 可用总宽度: " << availableWidth;

    // 设置所有管脚列的宽度
    for (int pinCol : pinColumns)
    {
        table->setColumnWidth(pinCol, widthPerPinColumn);
        table->horizontalHeader()->setSectionResizeMode(pinCol, QHeaderView::Interactive);
        qDebug() << funcName << " - 设置管脚列 " << pinCol << " 宽度为 " << widthPerPinColumn;
    }

    qDebug() << funcName << " - 管脚列宽度设置完成";
}

void TableStyleManager::applyBatchTableStyle(QTableWidget *table)
{
    const QString funcName = "TableStyleManager::applyBatchTableStyle";
    qDebug() << funcName << " - Entry";
    QElapsedTimer timer;
    timer.start();

    if (!table)
    {
        qWarning() << funcName << " - 错误：表格对象为空";
        return;
    }

    qDebug() << funcName << " - 开始批量设置表格样式";

    // 完全禁用表格更新，减少重绘次数
    table->setUpdatesEnabled(false);

    // 禁用所有子组件的重绘和布局更新
    table->viewport()->setUpdatesEnabled(false);
    table->horizontalHeader()->setUpdatesEnabled(false);
    table->verticalHeader()->setUpdatesEnabled(false);

    // 暂时禁用滚动条更新并记录当前位置
    QScrollBar *hScrollBar = table->horizontalScrollBar();
    QScrollBar *vScrollBar = table->verticalScrollBar();
    int hScrollValue = hScrollBar ? hScrollBar->value() : 0;
    int vScrollValue = vScrollBar ? vScrollBar->value() : 0;

    if (hScrollBar)
        hScrollBar->setUpdatesEnabled(false);
    if (vScrollBar)
        vScrollBar->setUpdatesEnabled(false);

    // 开始批量处理，避免每次小改动都触发信号
    table->blockSignals(true);

    // 设置全局表格样式
    table->setShowGrid(true);

    // 使用统一的基础样式字符串
    QString baseStyleSheet =
        "QTableWidget {"
        "   border: 1px solid #c0c0c0;"
        "   gridline-color: #d0d0d0;"
        "   selection-background-color: #e0e7ff;"
        "}"
        "QTableWidget::item {"
        "   border: 0px;"
        "   padding-left: 3px;"
        "}"
        "QTableWidget::item:selected {"
        "   background-color: #e0e7ff;"
        "   color: black;"
        "}"
        "QHeaderView::section {"
        "   background-color: #f0f0f0;"
        "   border: 1px solid #c0c0c0;"
        "   font-weight: bold;"
        "   padding: 4px;"
        "   text-align: center;"
        "}"
        "QHeaderView::section:checked {"
        "   background-color: #e0e0e0;"
        "}";

    table->setStyleSheet(baseStyleSheet);

    // 一次性设置所有行的行高
    int rowHeight = 28; // 默认行高
    int rowCount = table->rowCount();

    // 设置默认行高而不是逐行设置
    table->verticalHeader()->setDefaultSectionSize(rowHeight);

    // 类型映射 - 用于批量处理列样式
    QHash<int, StyleClass> columnStyleMap;
    QHash<int, Qt::Alignment> columnAlignmentMap;

    // 确保样式类已初始化
    if (!styleClassMapInitialized)
    {
        initStyleClassMap();
    }

    // 分析列类型并映射到预定义样式
    int columnCount = table->columnCount();
    for (int col = 0; col < columnCount; col++)
    {
        QTableWidgetItem *headerItem = table->horizontalHeaderItem(col);
        if (!headerItem)
            continue;

        QString headerText = headerItem->text();
        StyleClass styleClass = Default;
        Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter;

        // 检查是否包含管脚信息（格式为"名称\nx数量\n类型"）
        if (headerText.contains("\n"))
        {
            styleClass = HeaderPin;
            alignment = Qt::AlignCenter;
        }
        // 数值类型列靠右对齐
        else if (headerText.contains("数量") || headerText.contains("数值") ||
                 headerText.contains("count") || headerText.contains("value") ||
                 headerText.contains("amount") || headerText.contains("id") ||
                 headerText.contains("ID"))
        {
            styleClass = Numeric;
            alignment = Qt::AlignRight | Qt::AlignVCenter;
        }
        // 日期时间类型居中对齐
        else if (headerText.contains("日期") || headerText.contains("时间") ||
                 headerText.contains("date") || headerText.contains("time") ||
                 headerText.contains("timeset"))
        {
            styleClass = DateTime;
            alignment = Qt::AlignCenter;
        }
        // 状态、类型等短文本居中对齐
        else if (headerText.contains("状态") || headerText.contains("类型") ||
                 headerText.contains("status") || headerText.contains("type") ||
                 headerText.contains("capture") || headerText.contains("ext"))
        {
            styleClass = Status;
            alignment = Qt::AlignCenter;
        }

        // 存储分类信息
        columnStyleMap[col] = styleClass;
        columnAlignmentMap[col] = alignment;

        // 设置表头对齐方式
        headerItem->setTextAlignment(Qt::AlignCenter);
    }

    // 批量设置单元格样式 - 通过预先构建数据再一次性赋值以提高性能
    QVector<QVector<QTableWidgetItem *>> tableItems(rowCount, QVector<QTableWidgetItem *>(columnCount, nullptr));

    // 创建并初始化所有单元格项目
    for (int row = 0; row < rowCount; row++)
    {
        for (int col = 0; col < columnCount; col++)
        {
            QTableWidgetItem *item = table->item(row, col);

            // 如果单元格为空，创建新的单元格
            if (!item)
            {
                item = new QTableWidgetItem();
                tableItems[row][col] = item;
            }
            else
            {
                // 保留原有内容，只更新样式
                QString text = item->text();
                Qt::Alignment align = columnAlignmentMap.value(col, Qt::AlignLeft | Qt::AlignVCenter);

                // 创建新的Item并设置内容和对齐方式
                item = new QTableWidgetItem(text);
                item->setTextAlignment(align);

                // 根据列类型应用适当的样式
                StyleClass styleClass = columnStyleMap.value(col, Default);
                if (styleClass == HeaderPin)
                {
                    styleClass = PinCell; // 表头是管脚列，单元格使用PinCell样式
                }

                tableItems[row][col] = item;
            }
        }
    }

    // 一次性批量设置所有表格项
    for (int row = 0; row < rowCount; row++)
    {
        for (int col = 0; col < columnCount; col++)
        {
            QTableWidgetItem *item = tableItems[row][col];
            if (item)
            {
                // 设置对齐方式
                item->setTextAlignment(columnAlignmentMap.value(col, Qt::AlignLeft | Qt::AlignVCenter));

                // 应用样式类
                StyleClass styleClass = columnStyleMap.value(col, Default);
                if (styleClass == HeaderPin)
                {
                    styleClass = PinCell;
                }

                table->setItem(row, col, item);
            }
        }
    }

    // 设置管脚列的列宽（一次性批量设置）
    setPinColumnWidths(table);

    // 恢复滚动条位置
    if (hScrollBar)
    {
        hScrollBar->setUpdatesEnabled(true);
        hScrollBar->setValue(hScrollValue);
    }
    if (vScrollBar)
    {
        vScrollBar->setUpdatesEnabled(true);
        vScrollBar->setValue(vScrollValue);
    }

    // 恢复信号处理
    table->blockSignals(false);

    // 恢复表格更新（按顺序启用各部分的更新）
    table->horizontalHeader()->setUpdatesEnabled(true);
    table->verticalHeader()->setUpdatesEnabled(true);
    table->viewport()->setUpdatesEnabled(true);
    table->setUpdatesEnabled(true);

    // 最后一次性刷新显示
    table->viewport()->update();

    qDebug() << funcName << " - 表格样式批量设置完成，耗时: " << timer.elapsed() << "毫秒";
}

// 初始化样式类映射
void TableStyleManager::initStyleClassMap()
{
    if (styleClassMapInitialized)
        return;

    qDebug() << "TableStyleManager::initStyleClassMap - 初始化样式类映射";

    // 为每种样式类型定义CSS样式
    styleClassMap[Default] = "color: black; background-color: white;";

    styleClassMap[Numeric] = "color: #000080; background-color: #f8f8ff;"
                             "font-family: 'Consolas', 'Courier New', monospace;";

    styleClassMap[DateTime] = "color: #006400; background-color: #f0fff0;";

    styleClassMap[Status] = "color: #800000; background-color: #fff0f0;"
                            "font-weight: bold;";

    styleClassMap[HeaderPin] = "color: #000000; background-color: #e0e0ff;"
                               "font-weight: bold; border: 1px solid #9090ff;";

    styleClassMap[PinCell] = "color: #000000; background-color: #f0f0ff;"
                             "font-family: 'Consolas', 'Courier New', monospace;";

    styleClassMap[HighlightRow] = "color: #000000; background-color: #ffffd0;";

    styleClassMap[EditableCell] = "color: #000000; background-color: #e0ffe0;"
                                  "border: 1px dotted #80c080;";

    styleClassMap[ReadOnlyCell] = "color: #808080; background-color: #f0f0f0;"
                                  "border: none;";

    styleClassMapInitialized = true;
    qDebug() << "TableStyleManager::initStyleClassMap - 样式类映射初始化完成";
}

// 获取样式类的CSS字符串
QString TableStyleManager::getStyleClassCSS(StyleClass styleClass)
{
    if (!styleClassMapInitialized)
    {
        initStyleClassMap();
    }

    return styleClassMap.value(styleClass, styleClassMap[Default]);
}

// 应用样式类到单元格
void TableStyleManager::applyCellStyleClass(QTableWidget *table, int row, int column, StyleClass styleClass)
{
    const QString funcName = "TableStyleManager::applyCellStyleClass";

    if (!table || row < 0 || column < 0 || row >= table->rowCount() || column >= table->columnCount())
    {
        qWarning() << funcName << " - 错误：表格对象为空或单元格索引无效";
        return;
    }

    QTableWidgetItem *item = table->item(row, column);
    if (!item)
    {
        qWarning() << funcName << " - 错误：单元格项目不存在";
        return;
    }

    // 获取样式类的CSS
    QString css = getStyleClassCSS(styleClass);
    item->setData(Qt::UserRole, static_cast<int>(styleClass)); // 存储样式类型

    // 应用样式
    table->setStyleSheet(table->styleSheet() +
                         QString("QTableWidget::item[row=\"%1\"][col=\"%2\"] { %3 }")
                             .arg(row)
                             .arg(column)
                             .arg(css));

    qDebug() << funcName << " - 应用样式类" << static_cast<int>(styleClass) << "到单元格 [" << row << "," << column << "]";
}

// 应用样式类到整列
void TableStyleManager::applyColumnStyleClass(QTableWidget *table, int column, StyleClass styleClass)
{
    const QString funcName = "TableStyleManager::applyColumnStyleClass";

    if (!table || column < 0 || column >= table->columnCount())
    {
        qWarning() << funcName << " - 错误：表格对象为空或列索引无效";
        return;
    }

    // 获取样式类的CSS
    QString css = getStyleClassCSS(styleClass);

    // 应用样式到整列
    table->setStyleSheet(table->styleSheet() +
                         QString("QTableWidget::item[col=\"%1\"] { %2 }")
                             .arg(column)
                             .arg(css));

    qDebug() << funcName << " - 应用样式类" << static_cast<int>(styleClass) << "到整列 " << column;
}