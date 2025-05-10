#ifndef VECTORDATAHANDLER_H
#define VECTORDATAHANDLER_H

#include <QString>
#include <QList>
#include <QMap>
#include <QTableWidget>
#include <QWidget>
#include <QObject>
#include <QAtomicInt>

class VectorDataHandler : public QObject
{
    Q_OBJECT

public:
    static VectorDataHandler &instance();

    // 加载向量表数据到表格控件
    bool loadVectorTableData(int tableId, QTableWidget *tableWidget);

    // 保存表格控件数据到数据库
    bool saveVectorTableData(int tableId, QTableWidget *tableWidget, QString &errorMessage);

    // 添加向量行
    static void addVectorRow(QTableWidget *table, const QStringList &pinOptions, int rowIdx);

    // 删除向量表
    bool deleteVectorTable(int tableId, QString &errorMessage);

    // 删除向量行
    bool deleteVectorRows(int tableId, const QList<int> &rowIndexes, QString &errorMessage);

    // 删除指定范围内的向量行
    bool deleteVectorRowsInRange(int tableId, int fromRow, int toRow, QString &errorMessage);

    // 获取向量表总行数
    int getVectorTableRowCount(int tableId);

    // 插入向量行数据
    bool insertVectorRows(int tableId, int startIndex, int rowCount, int timesetId,
                          QTableWidget *dataTable, bool appendToEnd,
                          const QList<QPair<int, QPair<QString, QPair<int, QString>>>> &selectedPins,
                          QString &errorMessage);

    // 跳转到指定行
    bool gotoLine(int tableId, int lineNumber);

    // 取消当前操作
    void cancelOperation();

signals:
    // 进度更新信号
    void progressUpdated(int percentage);

private:
    VectorDataHandler();
    ~VectorDataHandler();

    // 禁止拷贝和赋值
    VectorDataHandler(const VectorDataHandler &) = delete;
    VectorDataHandler &operator=(const VectorDataHandler &) = delete;

    // 取消操作标志
    QAtomicInt m_cancelRequested;
};

#endif // VECTORDATAHANDLER_H