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

    // 添加分页数据加载方法
    bool loadVectorTablePageData(int tableId, QTableWidget *tableWidget, int pageIndex, int pageSize);

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

    // 初始化缓存
    void initializeCache();

    // 加载指令缓存
    void loadInstructionCache();

    // 加载TimeSet缓存
    void loadTimesetCache();

    /**
     * @brief 解析给定表ID的二进制文件的绝对路径
     *
     * @param tableId 向量表的ID
     * @param errorMsg 如果发生错误，将填充错误消息
     * @return QString 如果成功，则为二进制文件的绝对路径；否则为空字符串
     */
    QString resolveBinaryFilePath(int tableId, QString &errorMsg);
};

#endif // VECTORDATAHANDLER_H