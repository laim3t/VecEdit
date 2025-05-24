#include "tablestyleperformancetester.h"
#include "tablestylemanager.h"
#include <QDateTime>
#include <QHeaderView>

TableStylePerformanceTester::TableStylePerformanceTester(QObject *parent)
    : QObject(parent)
{
}

QString TableStylePerformanceTester::runPerformanceTest(int rows, int columns, int iterations)
{
    qDebug() << "TableStylePerformanceTester::runPerformanceTest - 开始性能测试，"
             << "行数:" << rows << ", 列数:" << columns << ", 迭代次数:" << iterations;

    // 创建测试表格
    QTableWidget *testTable = createTestTable(rows, columns);

    QString result = testExistingTable(testTable, iterations);

    // 清理
    delete testTable;

    return result;
}

QString TableStylePerformanceTester::testExistingTable(QTableWidget *table, int iterations)
{
    if (!table)
    {
        qWarning() << "TableStylePerformanceTester::testExistingTable - 错误: 表格为空";
        return "错误: 表格为空";
    }

    qDebug() << "TableStylePerformanceTester::testExistingTable - 开始测试现有表格，"
             << "行数:" << table->rowCount() << ", 列数:" << table->columnCount()
             << ", 迭代次数:" << iterations;

    // 测试结果
    QVector<qint64> originalTimes;
    QVector<qint64> optimizedTimes;
    QVector<qint64> batchTimes;

    // 运行多次测试以获取平均值
    for (int i = 0; i < iterations; i++)
    {
        qDebug() << "TableStylePerformanceTester::testExistingTable - 开始迭代" << (i + 1) << "/" << iterations;

        // 清除之前的样式
        clearTableStyle(table);
        originalTimes.append(testOriginalMethod(table));

        clearTableStyle(table);
        optimizedTimes.append(testOptimizedMethod(table));

        clearTableStyle(table);
        batchTimes.append(testBatchMethod(table));
    }

    // 计算平均时间
    qint64 avgOriginal = 0;
    qint64 avgOptimized = 0;
    qint64 avgBatch = 0;

    for (int i = 0; i < iterations; i++)
    {
        avgOriginal += originalTimes[i];
        avgOptimized += optimizedTimes[i];
        avgBatch += batchTimes[i];
    }

    avgOriginal /= iterations;
    avgOptimized /= iterations;
    avgBatch /= iterations;

    // 计算性能提升
    double optimizedImprovement = 100.0 * (1.0 - (double)avgOptimized / avgOriginal);
    double batchImprovement = 100.0 * (1.0 - (double)avgBatch / avgOriginal);
    double batchVsOptimizedImprovement = 100.0 * (1.0 - (double)avgBatch / avgOptimized);

    // 生成结果报告
    QString report = QString("表格样式性能测试报告 [%1]\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    report += QString("===============================================\n");
    report += QString("测试表格大小: %1 行 x %2 列\n").arg(table->rowCount()).arg(table->columnCount());
    report += QString("测试迭代次数: %1\n\n").arg(iterations);

    report += QString("平均执行时间:\n");
    report += QString("- 原始方法 (applyTableStyle): %1 毫秒\n").arg(avgOriginal);
    report += QString("- 优化方法 (applyTableStyleOptimized): %1 毫秒\n").arg(avgOptimized);
    report += QString("- 批量方法 (applyBatchTableStyle): %1 毫秒\n\n").arg(avgBatch);

    report += QString("性能提升:\n");
    report += QString("- 优化方法较原始方法: 提升 %1%\n").arg(QString::number(optimizedImprovement, 'f', 2));
    report += QString("- 批量方法较原始方法: 提升 %1%\n").arg(QString::number(batchImprovement, 'f', 2));
    report += QString("- 批量方法较优化方法: 提升 %1%\n\n").arg(QString::number(batchVsOptimizedImprovement, 'f', 2));

    report += QString("详细数据:\n");
    report += QString("原始方法耗时 (毫秒): ");
    for (int i = 0; i < iterations; i++)
    {
        report += QString("%1").arg(originalTimes[i]);
        if (i < iterations - 1)
            report += ", ";
    }
    report += "\n";

    report += QString("优化方法耗时 (毫秒): ");
    for (int i = 0; i < iterations; i++)
    {
        report += QString("%1").arg(optimizedTimes[i]);
        if (i < iterations - 1)
            report += ", ";
    }
    report += "\n";

    report += QString("批量方法耗时 (毫秒): ");
    for (int i = 0; i < iterations; i++)
    {
        report += QString("%1").arg(batchTimes[i]);
        if (i < iterations - 1)
            report += ", ";
    }
    report += "\n";

    qDebug() << "TableStylePerformanceTester::testExistingTable - 测试完成，生成报告";
    qDebug().noquote() << report;

    return report;
}

QTableWidget *TableStylePerformanceTester::createTestTable(int rows, int columns)
{
    QTableWidget *table = new QTableWidget(rows, columns);

    // 设置表头
    for (int col = 0; col < columns; col++)
    {
        QString headerText;
        if (col == 0)
        {
            headerText = "Label";
        }
        else if (col == 1)
        {
            headerText = "Instruction";
        }
        else if (col == 2)
        {
            headerText = "TimeSet";
        }
        else if (col >= columns - 2)
        {
            // 最后两列设置为管脚列
            headerText = QString("%1\nx1\nIn").arg(QChar('A' + col - (columns - 2)));
        }
        else
        {
            headerText = QString("Column%1").arg(col + 1);
        }
        QTableWidgetItem *headerItem = new QTableWidgetItem(headerText);
        table->setHorizontalHeaderItem(col, headerItem);
    }

    // 填充数据
    for (int row = 0; row < rows; row++)
    {
        for (int col = 0; col < columns; col++)
        {
            QTableWidgetItem *item = new QTableWidgetItem();
            if (col == 0)
            {
                item->setText(QString("Label_%1").arg(row + 1));
            }
            else if (col == 1)
            {
                item->setText(row % 2 == 0 ? "INC" : "DEC");
            }
            else if (col == 2)
            {
                item->setText(QString("timeset_%1").arg(row % 5 + 1));
            }
            else if (col >= columns - 2)
            {
                item->setText(row % 3 == 0 ? "X" : (row % 3 == 1 ? "1" : "0"));
            }
            else
            {
                item->setText(QString("Cell_%1_%2").arg(row + 1).arg(col + 1));
            }
            table->setItem(row, col, item);
        }
    }

    return table;
}

qint64 TableStylePerformanceTester::testOriginalMethod(QTableWidget *table)
{
    QElapsedTimer timer;
    timer.start();

    // 使用原始方法应用样式
    TableStyleManager::applyTableStyle(table);

    qint64 elapsed = timer.elapsed();
    qDebug() << "TableStylePerformanceTester::testOriginalMethod - 原始方法耗时:" << elapsed << "毫秒";

    return elapsed;
}

qint64 TableStylePerformanceTester::testOptimizedMethod(QTableWidget *table)
{
    QElapsedTimer timer;
    timer.start();

    // 使用优化方法应用样式
    TableStyleManager::applyTableStyleOptimized(table);

    qint64 elapsed = timer.elapsed();
    qDebug() << "TableStylePerformanceTester::testOptimizedMethod - 优化方法耗时:" << elapsed << "毫秒";

    return elapsed;
}

qint64 TableStylePerformanceTester::testBatchMethod(QTableWidget *table)
{
    QElapsedTimer timer;
    timer.start();

    // 使用批量方法应用样式
    TableStyleManager::applyBatchTableStyle(table);

    qint64 elapsed = timer.elapsed();
    qDebug() << "TableStylePerformanceTester::testBatchMethod - 批量方法耗时:" << elapsed << "毫秒";

    return elapsed;
}

void TableStylePerformanceTester::clearTableStyle(QTableWidget *table)
{
    // 清除所有表格样式
    table->setStyleSheet("");

    // 重置表头
    table->horizontalHeader()->setStyleSheet("");
    table->verticalHeader()->setStyleSheet("");

    // 重置所有单元格的对齐方式和样式
    for (int row = 0; row < table->rowCount(); row++)
    {
        for (int col = 0; col < table->columnCount(); col++)
        {
            QTableWidgetItem *item = table->item(row, col);
            if (item)
            {
                item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
                item->setBackground(QBrush());
                item->setForeground(QBrush());
            }
        }
    }

    // 重置行高
    table->verticalHeader()->setDefaultSectionSize(20);

    // 重置所有列宽
    for (int col = 0; col < table->columnCount(); col++)
    {
        table->setColumnWidth(col, 100);
    }

    // 强制刷新表格
    table->viewport()->update();
}