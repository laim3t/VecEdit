#include "tablestylemanager.h"
#include <QColor>
#include <QScrollBar>
#include <QApplication>
#include <QStyleFactory>

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
    if (!table || table->columnCount() == 0)
    {
        qDebug() << "TableStyleManager::setColumnAlignments - 错误：表格对象为空或无列";
        return;
    }

    qDebug() << "TableStyleManager::setColumnAlignments - 开始根据内容设置列对齐方式";

    // 根据列的内容设置对齐方式
    // 假设列的标题可以指示其内容类型

    for (int col = 0; col < table->columnCount(); col++)
    {
        QTableWidgetItem *headerItem = table->horizontalHeaderItem(col);
        if (!headerItem)
            continue;

        QString headerText = headerItem->text().toLower();

        // 数字类型列（如ID、数量、金额等）右对齐
        if (headerText.contains("id") ||
            headerText.contains("数量") ||
            headerText.contains("金额") ||
            headerText.contains("大小") ||
            headerText.contains("value") ||
            headerText.contains("number") ||
            headerText.contains("size"))
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