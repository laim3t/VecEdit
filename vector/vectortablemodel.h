#ifndef VECTORTABLEMODEL_H
#define VECTORTABLEMODEL_H

#include <QAbstractTableModel>
#include <QFile>
#include <QList>
#include <QColor>
#include <QSqlDatabase>

#include "vector_data_types.h"
#include "../database/binaryfilehelper.h"

namespace Vector
{

    /**
     * @class VectorTableModel
     * @brief 针对向量数据的优化表格模型，支持大型数据集的高效访问
     *
     * 本模型实现了Qt的Model/View架构，摆脱了全量加载数据的限制，
     * 采用按需加载策略，使得应用可以处理百万级行数据而不会导致内存溢出。
     *
     * 设计特点：
     * 1. 状态化设计 - 维护数据库连接和元数据缓存，避免重复加载
     * 2. 懒加载策略 - 仅在需要展示时才加载具体行的数据
     * 3. 高效索引 - 主动维护行偏移索引，确保高效的随机访问
     */
    class VectorTableModel : public QAbstractTableModel
    {
        Q_OBJECT

    public:
        /**
         * @brief 构造函数
         * @param parent 父对象
         */
        explicit VectorTableModel(QObject *parent = nullptr);

        /**
         * @brief 析构函数 - 负责清理资源
         */
        virtual ~VectorTableModel();

        // ---- 基本信息接口 ----

        /**
         * @brief 获取行数 - QAbstractTableModel必须实现的方法
         * @param parent 父索引，表格模型通常忽略此参数
         * @return 表格的总行数
         */
        int rowCount(const QModelIndex &parent = QModelIndex()) const override;

        /**
         * @brief 获取列数 - QAbstractTableModel必须实现的方法
         * @param parent 父索引，表格模型通常忽略此参数
         * @return 表格的总列数
         */
        int columnCount(const QModelIndex &parent = QModelIndex()) const override;

        /**
         * @brief 获取数据 - QAbstractTableModel必须实现的方法
         *
         * 这是本模型的核心方法，仅在Qt需要显示特定单元格时才被调用，
         * 从而实现了按需加载的效果，避免一次性加载所有数据。
         *
         * @param index 要获取数据的单元格索引
         * @param role 数据角色，如显示值、颜色等
         * @return 单元格数据
         */
        QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

        /**
         * @brief 获取表头数据
         * @param section 行或列的索引
         * @param orientation 水平或垂直方向
         * @param role 数据角色
         * @return 表头数据
         */
        QVariant headerData(int section, Qt::Orientation orientation,
                            int role = Qt::DisplayRole) const override;

        /**
         * @brief 设置单元格数据 - 实现单元格编辑功能
         * 
         * 本方法处理用户对单元格的编辑操作，确保数据有效性，
         * 并将修改持久化到后端存储。
         *
         * @param index 要修改的单元格索引
         * @param value 新的单元格数据
         * @param role 数据角色
         * @return 成功修改返回true，否则返回false
         */
        bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
        
        /**
         * @brief 获取单元格标志 - 决定单元格的交互属性
         * 
         * 根据列的类型和属性，决定单元格是否可编辑、可选择等。
         *
         * @param index 单元格索引
         * @return 单元格标志组合
         */
        Qt::ItemFlags flags(const QModelIndex &index) const override;

        // ---- 数据源设置接口 ----

        /**
         * @brief 设置数据源表ID
         *
         * 当调用此方法时，模型会重新连接到指定的表，并重新加载元数据。
         * 这会导致模型发出信号通知视图进行更新。
         *
         * @param tableId 向量表的ID
         * @return 加载成功返回true，失败返回false
         */
        bool setTable(int tableId);

        /**
         * @brief 获取当前表的ID
         * @return 当前加载的表ID，如果未加载则返回-1
         */
        int tableId() const { return m_tableId; }

        /**
         * @brief 获取总行数
         * @return 表格的总行数
         */
        int totalRows() const { return m_rowCount; }

        /**
         * @brief 获取列信息
         * @return 当前表的列信息列表
         */
        const QList<ColumnInfo> &columns() const { return m_columns; }

    private:
        // ---- 状态管理成员 ----
        int m_tableId;               // 当前加载的表ID
        int m_rowCount;              // 表格总行数
        int m_columnCount;           // 表格总列数
        int m_schemaVersion;         // 数据库schema版本
        QString m_binaryFilePath;    // 二进制文件路径
        QList<ColumnInfo> m_columns; // 列信息缓存

        // 数据库连接管理
        QString m_dbConnectionName;      // 专用数据库连接名
        bool ensureDatabaseConnection(); // 确保数据库连接有效

        // 二进制文件处理
        mutable QFile m_binaryFile;         // 二进制文件句柄(mutable允许在const方法中使用)
        bool ensureBinaryFileOpen() const;  // 确保二进制文件打开
        bool ensureIndexFileExists() const; // 确保索引文件存在

        // 缓存管理
        mutable QMap<int, RowData> m_rowCache; // 行数据缓存
        mutable int m_cacheHits;               // 缓存命中计数(用于统计)
        mutable int m_cacheMisses;             // 缓存未命中计数
        const int MAX_CACHE_SIZE = 1000;       // 最大缓存行数

        /**
         * @brief 验证单元格数据是否有效
         * 
         * 根据列的类型和属性，验证用户输入的数据是否合法。
         *
         * @param column 列索引
         * @param value 输入的数据值
         * @return 有效返回true，无效返回false
         */
        bool validateCellData(int column, const QVariant &value) const;
        
        /**
         * @brief 更新缓存中的行数据
         * 
         * 当单元格数据发生变化时，更新内存缓存。
         *
         * @param rowIndex 行索引
         * @param colIndex 列索引
         * @param value 新的数据值
         */
        void updateRowCache(int rowIndex, int colIndex, const QVariant &value);

        /**
         * @brief 加载表的元数据
         * @return 加载成功返回true，失败返回false
         */
        bool loadTableMetadata();

        /**
         * @brief 获取指定行的数据
         *
         * 这是内部方法，负责高效地获取行数据，优先从缓存获取，
         * 缓存未命中时才从文件中读取。
         *
         * @param rowIndex 行索引
         * @return 行数据
         */
        RowData fetchRowData(int rowIndex) const;

        /**
         * @brief 清理资源
         * 关闭文件句柄和数据库连接，清除缓存
         */
        void cleanup();
    };

} // namespace Vector

#endif // VECTORTABLEMODEL_H