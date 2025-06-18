#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QCommandLineParser>
#include <QCommandLineOption>

#include "database/databasemanager.h"
#include "vector/vectordatahandler.h"
#include "vector/vector_data_types.h"
#include "common/logger.h"

/**
 * @brief 简单的测试工具，用于验证fetchRowData的性能和正确性
 *
 * 编译和使用:
 * 1. 将此文件添加到CMakeLists.txt中的单独测试目标
 * 2. 构建并运行 test_fetch_row 可执行文件
 *
 * 命令行参数：
 *   -t, --table-id      要测试的表ID (默认: 1)
 *   -r, --row-index     要读取的行索引 (默认: 0)
 *   -c, --count         要读取的行数 (默认: 1)
 *   -b, --benchmark     启用性能基准测试 (读取多行并测量性能)
 *
 * 示例：
 *   test_fetch_row --table-id 2 --row-index 1000 --count 5
 *   test_fetch_row -t 2 -r 1000 -c 5 -b
 */
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // 设置命令行解析
    QCommandLineParser parser;
    parser.setApplicationDescription("测试 VectorDataHandler::fetchRowData 性能和正确性");
    parser.addHelpOption();

    // 定义命令行选项
    QCommandLineOption tableIdOption(QStringList() << "t" << "table-id", "要测试的表ID", "table-id", "1");
    QCommandLineOption rowIndexOption(QStringList() << "r" << "row-index", "要读取的行索引", "row-index", "0");
    QCommandLineOption countOption(QStringList() << "c" << "count", "要读取的行数", "count", "1");
    QCommandLineOption benchmarkOption(QStringList() << "b" << "benchmark", "启用性能基准测试");

    // 添加选项到解析器
    parser.addOption(tableIdOption);
    parser.addOption(rowIndexOption);
    parser.addOption(countOption);
    parser.addOption(benchmarkOption);

    // 解析命令行
    parser.process(app);

    // 获取选项值
    int tableId = parser.value(tableIdOption).toInt();
    int rowIndex = parser.value(rowIndexOption).toInt();
    int count = parser.value(countOption).toInt();
    bool benchmark = parser.isSet(benchmarkOption);

    // 设置日志级别
    Logger::instance().setLogLevel(Logger::LogLevel::Debug);

    qDebug() << "测试参数：表ID=" << tableId << ", 行索引=" << rowIndex << ", 行数=" << count;

    // 初始化数据库连接
    QString dbPath = "tests/database/TEST3211.db"; // 使用提供的测试数据库
    if (!DatabaseManager::instance()->openExistingDatabase(dbPath))
    {
        qCritical() << "无法打开数据库！测试失败。错误:" << DatabaseManager::instance()->lastError();
        return 1;
    }

    // 获取表的实际行数，用于验证
    int actualRowCount = VectorDataHandler::instance().getVectorTableRowCount(tableId);
    qDebug() << "表" << tableId << "实际行数:" << actualRowCount;

    if (rowIndex >= actualRowCount)
    {
        qWarning() << "指定的行索引" << rowIndex << "超出表范围 (0 - " << (actualRowCount - 1) << ")";
        return 1;
    }

    // 测试单行读取性能和正确性
    qDebug() << "\n=== 单行读取测试 ===";

    QElapsedTimer timer;
    timer.start();

    Vector::RowData rowData;
    bool success = VectorDataHandler::instance().fetchRowData(tableId, rowIndex, rowData);

    qint64 elapsed = timer.elapsed();

    if (success)
    {
        qDebug() << "成功读取行" << rowIndex << ", 耗时:" << elapsed << "毫秒";
        qDebug() << "行数据包含" << rowData.size() << "个字段:";

        // 输出行数据的内容
        for (int i = 0; i < rowData.size() && i < 10; ++i) // 最多显示10个字段，避免输出过多
        {
            qDebug() << "  字段" << i << ":" << rowData[i].toString().left(50); // 限制长度，避免输出过长
        }

        if (rowData.size() > 10)
        {
            qDebug() << "  ... (还有" << (rowData.size() - 10) << "个字段未显示)";
        }
    }
    else
    {
        qCritical() << "读取行" << rowIndex << "失败!";
        return 1;
    }

    // 如果指定了多行或基准测试，则测试多行读取性能
    if (count > 1 || benchmark)
    {
        int testCount = benchmark ? 1000 : count;               // 基准测试模式下使用1000行
        testCount = qMin(testCount, actualRowCount - rowIndex); // 确保不超出表范围

        qDebug() << "\n=== 多行读取性能测试 (" << testCount << "行) ===";

        timer.restart();
        int successCount = 0;

        for (int i = 0; i < testCount; ++i)
        {
            Vector::RowData data;
            if (VectorDataHandler::instance().fetchRowData(tableId, rowIndex + i, data))
            {
                successCount++;
            }
        }

        qint64 totalElapsed = timer.elapsed();
        double avgTimePerRow = testCount > 0 ? (double)totalElapsed / testCount : 0;

        qDebug() << "成功读取" << successCount << "/" << testCount << "行";
        qDebug() << "总耗时:" << totalElapsed << "毫秒";
        qDebug() << "平均每行耗时:" << avgTimePerRow << "毫秒";
        qDebug() << "读取速度:" << (testCount * 1000.0 / totalElapsed) << "行/秒";
    }

    // 完成退出
    DatabaseManager::instance()->closeDatabase();
    qDebug() << "\n测试完成!";

    return 0;
}