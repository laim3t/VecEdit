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

    // 从VectorTableModel保存数据
    bool saveDataFromModel(int tableId, const QList<Vector::RowData> &pageData,
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

    // 确保二进制文件兼容性
    bool ensureBinaryFilesCompatibility(const QString &dbPath, QString &errorMessage);

    // 获取指定表的所有列信息（包括可见和隐藏）
    QList<Vector::ColumnInfo> getAllColumnInfo(int tableId);

    // 获取指定表的schema版本
    int getSchemaVersion(int tableId);

    // 获取指定表的所有行数据
    QList<QList<QVariant>> getAllVectorRows(int tableId, bool &ok);

    /**
     * @brief Inserts multiple rows of vector data into the specified table using the new high-performance indexing mechanism.
     * @param tableId The ID of the target vector table.
     * @param logicalStartIndex The logical index where the new rows should be inserted.
     * @param rows The list of row data to insert.
     * @param errorMessage An output parameter to hold any error messages.
     * @return True if the insertion was successful, false otherwise.
     */
    bool insertVectorRows(int tableId, int logicalStartIndex, const QList<Vector::RowData> &rows, QString &errorMessage);

    // 更新单个向量行
    bool updateVectorRow(int tableId, int rowIndex, const Vector::RowData &rowData, QString &errorMessage);

    /**
     * @brief 批量更新多行数据，优化性能
     *
     * 替代逐行调用updateVectorRow的低效方式，使用批处理减少文件操作和数据库操作
     *
     * @param tableId 表ID
     * @param rows 行索引到行数据的映射，键是行索引，值是行数据
     * @param errorMessage 输出错误信息
     * @return bool 是否成功更新所有行
     */
    bool batchUpdateVectorRows(int tableId, const QMap<int, Vector::RowData> &rows, QString &errorMessage);

    /**
     * @brief 批量更新向量表中指定列的多行数据
     * @param tableId 表ID
     * @param columnIndex 要更新的列索引
     * @param rowValueMap 行索引到值的映射，键是行索引，值是新的列值
     * @param errorMessage 错误信息输出参数
     * @return 是否成功执行批量更新
     */
    bool batchUpdateVectorColumn(int tableId, int columnIndex, const QMap<int, QVariant> &rowValueMap, QString &errorMessage);

    /**
     * @brief 批量更新向量表列数据的优化版本 (批量追加-批量索引更新模式)
     *
     * 该方法使用高效的批量操作，通过以下步骤优化性能：
     * 1. 将所有新行数据一次性追加到二进制文件末尾
     * 2. 在内存中构建索引更新操作，然后批量提交到数据库
     *
     * 这种方法避免了传统的逐行读取-修改-写入循环，显著减少了I/O操作
     * 和磁盘同步次数，特别适合大批量数据更新场景。
     *
     * @param tableId 向量表ID
     * @param columnIndex 要更新的列索引
     * @param rowValueMap 行索引到新值的映射
     * @param errorMessage 错误信息输出
     * @return 操作是否成功
     */
    bool batchUpdateVectorColumnOptimized(int tableId, int columnIndex, const QMap<int, QVariant> &rowValueMap, QString &errorMessage);

    /**
     * @brief 批量填充TimeSet值到指定范围的行
     * @param tableId 表ID
     * @param rowIndexes 要更新的行索引列表
     * @param timeSetId 要设置的TimeSet ID
     * @param errorMessage 错误信息输出参数
     * @return 是否成功执行批量更新
     */
    bool batchFillTimeSet(int tableId, const QList<int> &rowIndexes, int timeSetId, QString &errorMessage);

    /**
     * @brief 批量替换TimeSet值
     * @param tableId 表ID
     * @param fromTimeSetId 要被替换的TimeSet ID
     * @param toTimeSetId 替换后的TimeSet ID
     * @param rowIndexes 要处理的行索引列表，如果为空则处理所有行
     * @param errorMessage 错误信息输出参数
     * @return 是否成功执行批量替换
     */
    bool batchReplaceTimeSet(int tableId, int fromTimeSetId, int toTimeSetId, const QList<int> &rowIndexes, QString &errorMessage);

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
    QList<QList<QVariant>> getPageData(int tableId, int pageIndex, int pageSize);

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
    RobustVectorDataHandler(QObject *parent = nullptr);
    ~RobustVectorDataHandler();

    // 禁止拷贝和赋值
    RobustVectorDataHandler(const RobustVectorDataHandler &) = delete;
    RobustVectorDataHandler &operator=(const RobustVectorDataHandler &) = delete;

    // 存储每个表的已修改行索引
    QMap<int, QSet<int>> m_modifiedRows;

    // 加载向量表元数据（列信息、行数等）
    bool loadVectorTableMeta(int tableId, QString &binFileName, QList<Vector::ColumnInfo> &columns,
                             int &schemaVersion, int &totalRowCount);

    // 从二进制文件读取页面数据
    bool readPageDataFromBinary(const QString &absoluteBinFilePath,
                                const QList<Vector::ColumnInfo> &columns,
                                int schemaVersion,
                                int startRow,
                                int numRows,
                                QList<QList<QVariant>> &pageRows);

    // 重新排列行索引，确保连续无空洞
    bool reindexLogicalOrder(int tableId, QString &errorMessage);

    // 更新二进制文件头中的行数和列数
    bool updateBinaryFileHeader(int tableId, QString &errorMessage);
};

#endif // ROBUSTVECTORDATAHANDLER_H