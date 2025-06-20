#ifndef VECTORDATAHANDLER_H
#define VECTORDATAHANDLER_H

#include <QString>
#include <QList>
#include <QMap>
#include <QTableWidget>
#include <QWidget>
#include <QObject>
#include <QAtomicInt>
#include <QSet>
#include <QDateTime>
#include <QSqlDatabase>
#include "vector/vector_data_types.h"
#include "common/binary_file_format.h"

class VectorDataHandler : public QObject
{
    Q_OBJECT

public:
    static VectorDataHandler &instance();

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
    bool insertVectorRows(QSqlDatabase& db, int tableId, int startIndex, int rowCount, int timesetId,
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

    // 标记行已被修改
    void markRowAsModified(int tableId, int rowIndex);

    // 清除指定表的所有修改标记
    void clearModifiedRows(int tableId);

    // 检查行是否被修改过
    bool isRowModified(int tableId, int rowIndex);

    /**
     * @brief 更新单元格数据
     *
     * 更新指定表格中指定单元格的数据，并持久化到二进制文件中。
     * 这个方法是专为VectorTableModel设计的，支持高效的单点更新操作。
     *
     * @param tableId 向量表ID
     * @param rowIndex 行索引（从0开始）
     * @param columnIndex 列索引（从0开始）
     * @param value 新的数据值
     * @return bool 成功返回true，失败返回false
     */
    bool updateCellData(int tableId, int rowIndex, int columnIndex, const QVariant &value);

    /**
     * @brief 按需获取单行数据
     *
     * 从二进制文件中读取指定行的数据，而不需要加载整个文件。
     * 这是重构后的核心方法，支持大型文件的高效访问。
     *
     * @param tableId 向量表的ID
     * @param rowIndex 行索引（从0开始）
     * @param rowData 输出参数，存储获取到的行数据
     * @return bool 成功返回true，失败返回false
     */
    bool fetchRowData(int tableId, int rowIndex, Vector::RowData &rowData);

    /**
     * @brief 解析给定表ID的二进制文件的绝对路径
     *
     * @param tableId 向量表的ID
     * @param errorMsg 如果发生错误，将填充错误消息
     * @return QString 如果成功，则为二进制文件的绝对路径；否则为空字符串
     */
    QString resolveBinaryFilePath(int tableId, QString &errorMsg);

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

    // 缓存数据
    QMap<int, QString> m_instructionCache; // 指令ID到指令文本的缓存
    QMap<int, QString> m_timesetCache;     // TimeSet ID到TimeSet名称的缓存
    bool m_cacheInitialized;               // 缓存是否已初始化

    // 表数据缓存 - 性能优化
    QMap<int, QList<Vector::RowData>> m_tableDataCache; // 表ID -> 行数据的缓存
    QMap<int, QDateTime> m_tableCacheTimestamp;         // 表ID -> 缓存时间戳
    QMap<int, QString> m_tableBinaryFilePath;           // 表ID -> 二进制文件路径
    QMap<int, QByteArray> m_tableBinaryFileMD5;         // 表ID -> 二进制文件MD5校验和

    // 修改跟踪数据
    QMap<int, QSet<int>> m_modifiedRows; // 表ID -> 修改行索引集合的映射

    // 初始化缓存
    void initializeCache();

    // 加载指令缓存
    void loadInstructionCache();

    // 加载TimeSet缓存
    void loadTimesetCache();

    // 检查表数据缓存是否有效
    bool isTableDataCacheValid(int tableId, const QString &binFilePath);

    // 更新表数据缓存
    void updateTableDataCache(int tableId, const QList<Vector::RowData> &rows, const QString &binFilePath);
};

#endif // VECTORDATAHANDLER_H