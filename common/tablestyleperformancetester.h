#ifndef TABLESTYLEPERFORMANCETESTER_H
#define TABLESTYLEPERFORMANCETESTER_H

#include <QObject>
#include <QTableWidget>
#include <QElapsedTimer>
#include <QDebug>

/**
 * @brief 表格样式性能测试类
 *
 * 此类用于测试不同的表格样式应用方法的性能，
 * 包括原始方法、优化方法和批量样式设置方法，
 * 并提供详细的性能对比结果。
 */
class TableStylePerformanceTester : public QObject
{
    Q_OBJECT

public:
    explicit TableStylePerformanceTester(QObject *parent = nullptr);

    /**
     * @brief 运行性能测试
     * @param rows 测试表格的行数
     * @param columns 测试表格的列数
     * @param iterations 测试重复次数
     * @return 返回包含测试结果的字符串
     */
    QString runPerformanceTest(int rows = 100, int columns = 10, int iterations = 5);

    /**
     * @brief 测试特定表格
     * @param table 要测试的表格部件
     * @param iterations 测试重复次数
     * @return 返回包含测试结果的字符串
     */
    QString testExistingTable(QTableWidget *table, int iterations = 5);

private:
    // 创建测试表格
    QTableWidget *createTestTable(int rows, int columns);

    // 测试原始方法
    qint64 testOriginalMethod(QTableWidget *table);

    // 测试优化方法
    qint64 testOptimizedMethod(QTableWidget *table);

    // 测试批量样式设置方法
    qint64 testBatchMethod(QTableWidget *table);

    // 清除表格样式
    void clearTableStyle(QTableWidget *table);
};

#endif // TABLESTYLEPERFORMANCETESTER_H