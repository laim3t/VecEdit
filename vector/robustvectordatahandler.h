#ifndef ROBUSTVECTORDATAHANDLER_H
#define ROBUSTVECTORDATAHANDLER_H

#include <QString>
#include <QList>
#include <QMap>
#include <QTableWidget>
#include <QWidget>
#include <QObject>
#include <QAtomicInt>
#include <QSet>
#include <QDateTime>
#include "vector/vector_data_types.h"

class VectorTableModel;

class RobustVectorDataHandler : public QObject
{
    Q_OBJECT

public:
    static RobustVectorDataHandler &instance();

    // 加载向量表数据到表格控件
    bool loadVectorTableData(int tableId, QTableWidget *tableWidget);

    // 保存表格控件数据到数据库
    bool saveVectorTableData(int tableId, QTableWidget *tableWidget, QString &errorMessage);

    // 保存分页模式下的表格数据 - 优化版本，避免创建临时表格
    bool saveVectorTableDataPaged(int tableId, QTableWidget *currentPageTable,
                                  int currentPage, int pageSize, int totalRows,
                                  QString &errorMessage);

    // 添加向量行
    static void addVectorRow(QTableWidget *table, const QStringList &pinOptions, int rowIdx);

    // 添加批量向量行
    static void addVectorRows(QTableWidget *table, const QStringList &pinOptions, int startRowIdx, int count);

    // 删除向量表
    bool deleteVectorTable(int tableId, QString &errorMessage);

    // 删除向量行
    bool deleteVectorRows(int tableId, const QList<int> &rowIndexes, QString &errorMessage);

    // 删除指定范围内的向量行
    bool deleteVectorRowsInRange(int tableId, int fromRow, int toRow, QString &errorMessage);

    // 获取向量表总行数
    int getVectorTableRowCount(int tableId);

    // 获取指定表的所有列信息（包括可见和隐藏）
    QList<Vector::ColumnInfo> getAllColumnInfo(int tableId);

    // 获取指定表的schema版本
    int getSchemaVersion(int tableId);

    // 获取指定表的所有行数据
    QList<Vector::RowData> getAllVectorRows(int tableId, bool &ok);

    // 插入向量行数据
    bool insertVectorRows(int tableId, int startIndex, int rowCount, int timesetId,
                          QTableWidget *dataTable, bool appendToEnd,
                          const QList<QPair<int, QPair<QString, QPair<int, QString>>>> &selectedPins,
                          QString &errorMessage);

    // 跳转到指定行
    bool gotoLine(int tableId, int lineNumber);

    // 逻辑删除列（设置IsVisible=0）
    bool hideVectorTableColumn(int tableId, const QString &columnName, QString &errorMessage);

    // 逻辑恢复列（设置IsVisible=1）
    bool showVectorTableColumn(int tableId, const QString &columnName, QString &errorMessage);

    // 取消当前操作
    void cancelOperation();

    // 清除缓存数据
    void clearCache();

    // 清除特定表的数据缓存
    void clearTableDataCache(int tableId);

    // 清除所有表的数据缓存
    void clearAllTableDataCache();

    // 添加分页数据加载方法
    bool loadVectorTablePageData(int tableId, QTableWidget *tableWidget, int pageIndex, int pageSize);

    // 为Model/View架构添加分页数据加载方法
    bool loadVectorTablePageDataForModel(int tableId, VectorTableModel *model, int pageIndex, int pageSize);

    // 获取向量表指定页数据 - 返回数据而不是填充表格
    QList<Vector::RowData> getPageData(int tableId, int pageIndex, int pageSize);

    // 获取向量表的可见列信息
    QList<Vector::ColumnInfo> getVisibleColumns(int tableId);

    // 标记行已被修改
    void markRowAsModified(int tableId, int rowIndex);

    // 清除指定表的所有修改标记
    void clearModifiedRows(int tableId);

    // 检查行是否被修改过
    bool isRowModified(int tableId, int rowIndex);

    // 解析给定表ID的二进制文件的绝对路径
    QString resolveBinaryFilePath(int tableId, QString &errorMsg);

signals:
    // 进度更新信号
    void progressUpdated(int percentage);

private:
    RobustVectorDataHandler(QObject* parent = nullptr);
    ~RobustVectorDataHandler();

    // 禁止拷贝和赋值
    RobustVectorDataHandler(const RobustVectorDataHandler &) = delete;
    RobustVectorDataHandler &operator=(const RobustVectorDataHandler &) = delete;
};

#endif // ROBUSTVECTORDATAHANDLER_H 