#ifndef ROBUSTVECTORDATAHANDLER_H
#define ROBUSTVECTORDATAHANDLER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QTableWidget>
#include <QAbstractItemModel>
#include <QMap>
#include <QVector>
#include "vector_data_types.h"
#include "robust_binary_helper.h"

using namespace Vector; // 添加命名空间引用

class RobustVectorDataHandler : public QObject
{
    Q_OBJECT
public:
    static RobustVectorDataHandler &instance();

    // 核心项目操作
    bool createNewProject(const QString &filePath, const QList<int> &pinList);
    bool openProject(const QString &filePath);
    void closeProject();
    bool saveProject();
    bool saveProjectAs(const QString &filePath);

    // 向量表操作
    bool deleteVectorTable(int tableId, QString &errorMessage);
    bool deleteVectorRows(int tableId, const QList<int> &rows, QString &errorMessage);
    bool deleteVectorRowsInRange(int tableId, int startRow, int endRow, QString &errorMessage);
    int getVectorTableRowCount(int tableId);
    bool loadVectorTablePageData(int tableId, QTableWidget *tableWidget, int page, int pageSize);
    void markRowAsModified(int tableId, int rowIndex);
    bool isRowModified(int tableId, int rowIndex) const;
    QList<RowData> getAllVectorRows(int tableId, bool &ok);
    void clearTableDataCache(int tableId);
    bool loadVectorTablePageDataForModel(int tableId, QAbstractItemModel *model, int page, int pageSize);
    QList<ColumnInfo> getAllColumnInfo(int tableId);
    bool loadVectorTableData(int tableId, QTableWidget *tableWidget);
    bool saveVectorTableDataPaged(int tableId, QTableWidget *tableWidget, int currentPage, int pageSize, int totalRows, QString &errorMessage);
    void clearModifiedRows(int tableId);

    // 基本数据访问
    int getRowCount() const;
    int getColumnCount() const;
    QString getData(int row, int column) const;
    bool setData(int row, int column, const QString &value);

    // 行操作
    bool insertRow(int row);
    bool removeRow(int row);
    bool insertRows(int row, int count);
    bool removeRows(int row, int count);

    // 状态查询
    bool isProjectOpen() const;
    QString getCurrentProjectPath() const;

private:
    RobustVectorDataHandler(QObject *parent = nullptr);
    ~RobustVectorDataHandler() = default;
    RobustVectorDataHandler(const RobustVectorDataHandler &) = delete;
    RobustVectorDataHandler &operator=(const RobustVectorDataHandler &) = delete;

    // 项目状态
    QString m_currentProjectPath;
    bool m_isProjectOpen;

    // 数据缓存
    struct TableCache
    {
        QMap<int, QList<QVariant>> modifiedRows; // 已修改的行
        QMap<int, QList<QVariant>> rowCache;     // 行数据缓存
        int rowCount;                            // 表格总行数
        int columnCount;                         // 表格列数
    };
    QMap<int, TableCache> m_tableCache; // tableId -> TableCache映射

    // 辅助方法
    void clearCache();
    bool ensureTableCacheExists(int tableId);
    bool loadTableMetadata(int tableId);
};

#endif // ROBUSTVECTORDATAHANDLER_H