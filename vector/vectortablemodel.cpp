#include "vectortablemodel.h"
#include "../database/databasemanager.h"
#include "../database/binaryfilehelper.h"
#include "../common/utils/pathutils.h"
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QFileInfo>
#include <QFile>
#include <QDir>

VectorTableModel::VectorTableModel(QObject *parent)
    : QAbstractTableModel(parent),
      m_tableId(-1),
      m_isModified(false)
{
    qDebug() << "VectorTableModel::VectorTableModel - 创建新的向量表模型实例";
}

VectorTableModel::~VectorTableModel()
{
    qDebug() << "VectorTableModel::~VectorTableModel - 销毁向量表模型实例";
    
    if (m_isModified) {
        qWarning() << "VectorTableModel::~VectorTableModel - 有未保存的修改";
    }
}

int VectorTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    
    return m_rows.count();
}

int VectorTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    
    return m_columns.count();
}

QVariant VectorTableModel::data(const QModelIndex &index, int role) const
{
    // 检查索引是否有效
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.count() || 
        index.column() < 0 || index.column() >= m_columns.count())
        return QVariant();
    
    // 检查是否有未保存的修改
    if (m_cache.contains(index.row()) && m_cache[index.row()].contains(index.column())) {
        if (role == Qt::DisplayRole || role == Qt::EditRole) {
            return m_cache[index.row()][index.column()];
        }
    }
    
    // 获取列信息和行数据
    const Vector::ColumnInfo &columnInfo = m_columns[index.column()];
    const Vector::RowData &rowData = m_rows[index.row()];
    
    switch (role) {
        case Qt::DisplayRole:
        case Qt::EditRole:
            // 确保行数据和列数量匹配
            if (index.column() < rowData.size()) {
                return rowData.value(index.column());
            } else {
                // 如果列索引超出行数据大小，返回空值
                return QVariant();
            }
            
        case Qt::TextAlignmentRole:
            // 根据列类型设置对齐方式
            switch (columnInfo.type) {
                case Vector::ColumnDataType::TEXT:
                    return Qt::AlignLeft + Qt::AlignVCenter;
                case Vector::ColumnDataType::INTEGER:
                case Vector::ColumnDataType::REAL:
                case Vector::ColumnDataType::TIMESET_ID:
                    return Qt::AlignCenter;
                default:
                    return Qt::AlignLeft + Qt::AlignVCenter;
            }
        default:
            return QVariant();
    }
    
    return QVariant();
}

bool VectorTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.count() || 
        index.column() < 0 || index.column() >= m_columns.count() || role != Qt::EditRole)
        return false;
    
    // 验证数据是否有效
    if (!value.isValid())
        return false;
    
    // 缓存修改
    if (!m_cache.contains(index.row())) {
        m_cache[index.row()] = QMap<int, QVariant>();
    }
    m_cache[index.row()][index.column()] = value;
    
    // 设置修改标志
    m_isModified = true;
    
    // 更新行元数据
    const Vector::ColumnInfo &columnInfo = m_columns[index.column()];
    
    // 根据列类型更新元数据
    if (columnInfo.type == Vector::ColumnDataType::TEXT) {
        // 检查列名来确定是label还是comment
        if (columnInfo.name.toLower() == "label") {
            if (!m_rowMetadata.contains(index.row())) {
                m_rowMetadata[index.row()] = RowMetadata();
            }
            m_rowMetadata[index.row()].label = value.toString();
        } else if (columnInfo.name.toLower() == "comment") {
            if (!m_rowMetadata.contains(index.row())) {
                m_rowMetadata[index.row()] = RowMetadata();
            }
            m_rowMetadata[index.row()].comment = value.toString();
        }
    } else if (columnInfo.type == Vector::ColumnDataType::TIMESET_ID) {
        if (!m_rowMetadata.contains(index.row())) {
            m_rowMetadata[index.row()] = RowMetadata();
        }
        m_rowMetadata[index.row()].timeSetId = value.toInt();
    }
    
    // 通知视图数据已更改
    emit dataChanged(index, index, {role});
    
    return true;
}

QVariant VectorTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();
    
    if (orientation == Qt::Horizontal) {
        // 水平表头（列名）
        if (section < 0 || section >= m_columns.count())
            return QVariant();
        
        return m_columns[section].name;
    } else {
        // 垂直表头（行号）
        return section + 1;
    }
}

Qt::ItemFlags VectorTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    
    // 默认所有单元格都可编辑
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

bool VectorTableModel::loadTable(int tableId)
{
    const QString funcName = "VectorTableModel::loadTable";
    qDebug() << funcName << " - 加载表，ID:" << tableId;
    
    if (tableId <= 0) {
        qWarning() << funcName << " - 无效的表ID:" << tableId;
        return false;
    }
    
    // 如果有未保存的修改，提示保存
    if (m_isModified) {
        qWarning() << funcName << " - 有未保存的修改，将被丢弃";
    }
    
    // 开始重置模型
    beginResetModel();
    
    // 重置模型状态
    m_tableId = tableId;
    m_rows.clear();
    m_cache.clear();
    m_rowMetadata.clear();
    m_isModified = false;
    
    // 检查数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen()) {
        qWarning() << funcName << " - 数据库未连接";
        m_tableId = -1;
        endResetModel();
        return false;
    }
    
    // 输出数据库连接信息，用于调试
    qDebug() << funcName << " - 数据库连接信息:";
    qDebug() << "  - 连接名称:" << db.connectionName();
    qDebug() << "  - 驱动程序:" << db.driverName();
    qDebug() << "  - 数据库路径:" << db.databaseName();
    qDebug() << "  - 连接状态:" << (db.isOpen() ? "已打开" : "未打开");
    qDebug() << "  - 最后错误:" << db.lastError().text();
    
    // 获取数据库中的表列表
    QStringList tables = db.tables();
    bool hasVectorTableMasterRecord = tables.contains("VectorTableMasterRecord", Qt::CaseInsensitive);
    bool hasVectorTables = tables.contains("vector_tables", Qt::CaseInsensitive);
    
    qDebug() << funcName << " - 数据库表检查:"
             << "VectorTableMasterRecord:" << hasVectorTableMasterRecord
             << ", vector_tables:" << hasVectorTables;
    qDebug() << funcName << " - 全部表:" << tables.join(", ");
    
    // 加载表名称
    QSqlQuery query(db); // 显式指定使用db连接
    bool tableExists = false;
    
    // 首先尝试从VectorTableMasterRecord表中获取表名
    if (hasVectorTableMasterRecord) {
        query.prepare("SELECT table_name FROM VectorTableMasterRecord WHERE id = ?");
        query.addBindValue(tableId);
        
        if (query.exec() && query.next()) {
            m_tableName = query.value(0).toString();
            tableExists = true;
            qDebug() << funcName << " - 从VectorTableMasterRecord加载到表名:" << m_tableName;
        } else {
            qWarning() << funcName << " - 从VectorTableMasterRecord获取表名失败:" << query.lastError().text();
        }
    }
    
    // 如果从VectorTableMasterRecord加载失败，尝试从vector_tables表中获取表名
    if (!tableExists && hasVectorTables) {
        query.prepare("SELECT table_name FROM vector_tables WHERE id = ?");
        query.addBindValue(tableId);
        
        if (query.exec() && query.next()) {
            m_tableName = query.value(0).toString();
            tableExists = true;
            qDebug() << funcName << " - 从vector_tables加载到表名:" << m_tableName;
        } else {
            qWarning() << funcName << " - 从vector_tables获取表名失败:" << query.lastError().text();
        }
    }
    
    // 如果两个表都没有找到，使用默认名称
    if (!tableExists) {
        qWarning() << "VectorTableModel::loadTable - 在任何表中都找不到ID为" << tableId << "的表";
        m_tableName = tr("表 %1").arg(tableId);
    }
    
    // 加载列配置
    bool success = loadColumnConfiguration();
    if (!success) {
        qWarning() << "VectorTableModel::loadTable - 加载列配置失败";
        m_tableId = -1;
        endResetModel();
        return false;
    }
    
    // 加载行数据
    success = loadRowData();
    if (!success) {
        qWarning() << "VectorTableModel::loadTable - 加载行数据失败，可能是二进制文件不存在";
        // 不返回失败，因为可能只是没有数据或者二进制文件尚未创建
        
        // 尝试清空行数据，确保视图干净
        beginResetModel();
        m_rows.clear();
        m_rowMetadata.clear();
        endResetModel();
    }
    
    // 完成重置模型
    endResetModel();
    
    qDebug() << "VectorTableModel::loadTable - 加载完成，列数:" << m_columns.count() << "，行数:" << m_rows.count();
    return true;
}

int VectorTableModel::currentTableId() const
{
    return m_tableId;
}

bool VectorTableModel::saveTable()
{
    const QString funcName = "VectorTableModel::saveTable";
    qDebug() << funcName << " - 保存表数据，ID:" << m_tableId;
    
    if (m_tableId <= 0) {
        qWarning() << funcName << " - 无效的表ID:" << m_tableId;
        return false;
    }
    
    // 即使没有修改，也要确保二进制文件存在
    // if (!m_isModified || m_cache.isEmpty()) {
    //    qDebug() << funcName << " - 没有修改，无需保存";
    //    return true;
    // }
    
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen()) {
        qWarning() << funcName << " - 数据库未连接";
        return false;
    }
    
    // 1. 获取或创建二进制文件名
    QSqlQuery masterQuery(db);
    masterQuery.prepare("SELECT binary_data_filename FROM VectorTableMasterRecord WHERE id = ?");
    masterQuery.addBindValue(m_tableId);
    
    QString binaryFilename;
    if (masterQuery.exec() && masterQuery.next()) {
        binaryFilename = masterQuery.value(0).toString();
    }
    
    // 如果二进制文件名为空，生成新文件名并更新数据库
    if (binaryFilename.isEmpty()) {
        binaryFilename = QString("table_%1_data.vbindata").arg(m_tableId);
        
        QSqlQuery updateQuery(db);
        updateQuery.prepare("UPDATE VectorTableMasterRecord SET binary_data_filename = ? WHERE id = ?");
        updateQuery.addBindValue(binaryFilename);
        updateQuery.addBindValue(m_tableId);
        
        if (!updateQuery.exec()) {
            qWarning() << funcName << " - 更新二进制文件名失败:" << updateQuery.lastError().text();
            return false;
        }
        
        qDebug() << funcName << " - 已创建新的二进制文件名:" << binaryFilename;
    }
    
    // 2. 构建文件的完整路径
    QString dbPath = db.databaseName();
    
    // 使用 PathUtils 工具类处理二进制文件路径
    QString projectBinaryDataDir = Utils::PathUtils::getProjectBinaryDataDirectory(dbPath);
    if (projectBinaryDataDir.isEmpty()) {
        qWarning() << funcName << " - 无法获取项目二进制数据目录";
        return false;
    }
    
    // 相对路径转绝对路径
    QString binaryFilePath;
    if (QFileInfo(binaryFilename).isRelative()) {
        binaryFilePath = QDir(projectBinaryDataDir).absoluteFilePath(binaryFilename);
        qDebug() << funcName << " - 相对路径转换为绝对路径:" << binaryFilename << " -> " << binaryFilePath;
    } else {
        binaryFilePath = binaryFilename;
    }
    
    qDebug() << funcName << " - 二进制文件路径:" << binaryFilePath;
    
    // 确保目录存在
    QFileInfo binFileInfo(binaryFilePath);
    QDir binDir = binFileInfo.dir();
    if (!binDir.exists()) {
        qInfo() << funcName << " - 目标二进制目录不存在，尝试创建:" << binDir.absolutePath();
        if (!binDir.mkpath(".")) {
            qWarning() << funcName << " - 无法创建二进制数据目录:" << binDir.absolutePath();
            return false;
        }
        qInfo() << funcName << " - 成功创建二进制数据目录:" << binDir.absolutePath();
    }
    
    // 3. 应用缓存中的修改到内存行数据
    if (!m_cache.isEmpty()) {
        for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
            int row = it.key();
            if (row >= 0 && row < m_rows.count()) {
                const QMap<int, QVariant> &colData = it.value();
                for (auto colIt = colData.begin(); colIt != colData.end(); ++colIt) {
                    int col = colIt.key();
                    if (col >= 0 && col < m_columns.count()) {
                        m_rows[row][col] = colIt.value();
                        
                        // 更新行元数据
                        const Vector::ColumnInfo &columnInfo = m_columns[col];
                        if (columnInfo.name.toLower() == "label") {
                            m_rowMetadata[row].label = colIt.value().toString();
                        } else if (columnInfo.name.toLower() == "comment") {
                            m_rowMetadata[row].comment = colIt.value().toString();
                        } else if (columnInfo.name.toLower() == "timeset") {
                            m_rowMetadata[row].timeSetId = colIt.value().toInt();
                        }
                    }
                }
            }
        }
        m_cache.clear();
    }
    
    // 4. 将所有行数据保存到二进制文件
    int schemaVersion = db.databaseName().contains("_v3") ? 3 : (db.databaseName().contains("_v2") ? 2 : 1);
    bool success = Persistence::BinaryFileHelper::writeAllRowsToBinary(binaryFilePath, m_columns, schemaVersion, m_rows);
    
    if (success) {
        qDebug() << funcName << " - 成功保存数据到二进制文件:" << binaryFilePath;
        m_isModified = false;
        
        // 更新数据库中的行数记录
        QSqlQuery updateRowCountQuery(db);
        updateRowCountQuery.prepare("UPDATE VectorTableMasterRecord SET row_count = ? WHERE id = ?");
        updateRowCountQuery.addBindValue(m_rows.count());
        updateRowCountQuery.addBindValue(m_tableId);
        
        if (!updateRowCountQuery.exec()) {
            qWarning() << funcName << " - 更新行数记录失败:" << updateRowCountQuery.lastError().text();
            // 这不是致命错误，可以继续
        } else {
            qDebug() << funcName << " - 已更新数据库中的行数记录:" << m_rows.count();
        }
        
        return true;
    } else {
        qWarning() << funcName << " - 保存数据到二进制文件失败";
        return false;
    }
}

bool VectorTableModel::refreshTable()
{
    qDebug() << "VectorTableModel::refreshTable - 刷新表数据，ID:" << m_tableId;
    
    if (m_tableId <= 0) {
        qWarning() << "VectorTableModel::refreshTable - 无效的表ID:" << m_tableId;
        return false;
    }
    
    // 如果有未保存的修改，提示保存
    if (m_isModified) {
        qWarning() << "VectorTableModel::refreshTable - 有未保存的修改，将被丢弃";
    }
    
    // 重新加载表数据
    return loadTable(m_tableId);
}

bool VectorTableModel::addRow(int position)
{
    qDebug() << "VectorTableModel::addRow - 添加行，位置:" << position << "，表ID:" << m_tableId;
    
    if (m_tableId <= 0) {
        qWarning() << "VectorTableModel::addRow - 无效的表ID:" << m_tableId;
        return false;
    }
    
    // 如果position为-1，则在末尾添加
    if (position < 0 || position > m_rows.count()) {
        position = m_rows.count();
    }
    
    // 开始插入行
    beginInsertRows(QModelIndex(), position, position);
    
    // 创建一个新的行数据
    Vector::RowData newRow;
    
    // 初始化行数据
    for (int i = 0; i < m_columns.count(); ++i) {
        newRow.append(QVariant()); // 添加空值
    }
    
    // 插入到内存数据中
    m_rows.insert(position, newRow);
    
    // 创建行元数据
    RowMetadata metadata;
    metadata.label = "";
    metadata.comment = "";
    metadata.timeSetId = 1; // 默认TimeSet ID
    m_rowMetadata[position] = metadata;
    
    // 结束插入行
    endInsertRows();
    
    return true;
}

bool VectorTableModel::removeRow(int position)
{
    QList<int> positions;
    positions.append(position);
    return removeRows(positions);
}

bool VectorTableModel::removeRows(const QList<int> &positions)
{
    qDebug() << "VectorTableModel::removeRows - 删除行，数量:" << positions.count() << "，表ID:" << m_tableId;
    
    if (m_tableId <= 0) {
        qWarning() << "VectorTableModel::removeRows - 无效的表ID:" << m_tableId;
        return false;
    }
    
    if (positions.isEmpty()) {
        return true; // 没有要删除的行
    }
    
    // 对位置进行排序，从大到小（这样可以避免删除时索引变化）
    QList<int> sortedPositions = positions;
    std::sort(sortedPositions.begin(), sortedPositions.end(), std::greater<int>());
    
    // 从内存中删除行
    for (int position : sortedPositions) {
        if (position < 0 || position >= m_rows.count()) {
            qWarning() << "VectorTableModel::removeRows - 无效的行索引:" << position;
            continue;
        }
        
        beginRemoveRows(QModelIndex(), position, position);
        m_rows.removeAt(position);
        m_rowMetadata.remove(position);
        endRemoveRows();
    }
    
    return true;
}

QList<Vector::ColumnInfo> VectorTableModel::getColumnConfiguration() const
{
    return m_columns;
}

bool VectorTableModel::loadColumnConfiguration()
{
    qDebug() << "VectorTableModel::loadColumnConfiguration - 加载列配置，表ID:" << m_tableId;
    
    if (m_tableId <= 0) {
        qWarning() << "VectorTableModel::loadColumnConfiguration - 无效的表ID:" << m_tableId;
        return false;
    }
    
    // 清空现有列配置
    m_columns.clear();
    
    QSqlDatabase db = DatabaseManager::instance()->database();
    
    // 检查数据库连接
    if (!db.isOpen()) {
        qWarning() << "VectorTableModel::loadColumnConfiguration - 数据库未连接";
        return false;
    }
    
    // 调试数据库状态
    qDebug() << "数据库连接名称:" << db.connectionName();
    qDebug() << "数据库类型:" << db.driverName();
    qDebug() << "数据库路径:" << db.databaseName();
    qDebug() << "数据库表:" << db.tables().join(", ");
    
    // 检查VectorTableColumnConfiguration表是否存在
    QStringList tables = db.tables();
    if (!tables.contains("VectorTableColumnConfiguration", Qt::CaseInsensitive)) {
        qWarning() << "VectorTableModel::loadColumnConfiguration - 表VectorTableColumnConfiguration不存在";
        return false;
    }
    
    // 检查表结构
    QSqlQuery structQuery(db);
    structQuery.exec("PRAGMA table_info(VectorTableColumnConfiguration)");
    QStringList columns;
    while (structQuery.next()) {
        columns << structQuery.value(1).toString();
    }
    qDebug() << "VectorTableColumnConfiguration表结构:" << columns.join(", ");
    
    // 根据表结构调整查询语句
    QSqlQuery query(db);
    
    // 构建查询语句，确保使用正确的表名和字段名
    QString queryStr;
    if (columns.contains("pin_id") && columns.contains("display_order")) {
        queryStr = "SELECT id, column_type, column_name, pin_id, display_order "
                  "FROM VectorTableColumnConfiguration WHERE master_record_id = ? "
                  "ORDER BY display_order";
    } else if (columns.contains("column_order")) {
        queryStr = "SELECT id, column_type, column_name, 0 as pin_id, column_order as display_order "
                  "FROM VectorTableColumnConfiguration WHERE master_record_id = ? "
                  "ORDER BY column_order";
    } else if (columns.contains("id") && columns.contains("column_type")) {
        // 防止列名格式略有不同
        queryStr = "SELECT id, column_type, column_name, 0 as pin_id, 0 as display_order "
                  "FROM VectorTableColumnConfiguration WHERE master_record_id = ? "
                  "ORDER BY id";
    } else {
        qWarning() << "VectorTableModel::loadColumnConfiguration - 表结构不匹配，缺少必要字段";
        return false;
    }
    
    qDebug() << "VectorTableModel::loadColumnConfiguration - 执行查询:" << queryStr;
    
    query.prepare(queryStr);
    query.addBindValue(m_tableId);
    
    if (!query.exec()) {
        qWarning() << "VectorTableModel::loadColumnConfiguration - 查询失败:" 
                   << query.lastError().text() << ", SQL:" << query.lastQuery();
        
        // 尝试直接执行不带参数的查询（用于调试）
        QString directQuery = QString("SELECT id, column_type, column_name, pin_id, display_order "
                                     "FROM VectorTableColumnConfiguration WHERE master_record_id = %1 "
                                     "ORDER BY display_order").arg(m_tableId);
        qDebug() << "尝试直接执行查询:" << directQuery;
        
        if (query.exec(directQuery)) {
            qDebug() << "直接查询成功，这表明参数绑定有问题";
        } else {
            qDebug() << "直接查询也失败:" << query.lastError().text();
        }
        
        return false;
    }
    
    // 处理查询结果
    int columnCount = 0;
    while (query.next()) {
        Vector::ColumnInfo column;
        column.id = query.value(0).toInt();
        column.vector_table_id = m_tableId;
        column.name = query.value(2).toString();
        column.order = query.value(4).toInt();
        
        // 设置列类型
        QString typeStr = query.value(1).toString();
        column.original_type_str = typeStr;
        column.type = Vector::columnDataTypeFromString(typeStr);
        
        // 设置其他属性，如果是pin列，则添加pinId
        if (column.type == Vector::ColumnDataType::PIN_STATE_ID) {
            QJsonObject properties;
            properties["pin_id"] = query.value(3).toInt();
            column.data_properties = properties;
        }
        
        m_columns.append(column);
        columnCount++;
        
        // 输出列信息
        qDebug() << "列信息 - ID:" << column.id 
                 << ", 名称:" << column.name 
                 << ", 类型:" << column.original_type_str 
                 << ", 顺序:" << column.order;
    }
    
    qDebug() << "VectorTableModel::loadColumnConfiguration - 加载了" << columnCount << "个列配置";
    
    // 如果没有列配置，尝试添加默认列
    if (columnCount == 0) {
        qWarning() << "VectorTableModel::loadColumnConfiguration - 表" << m_tableId << "没有列配置，将添加默认列";
        
        // 检查表是否存在
        QSqlQuery checkQuery(db);
        checkQuery.prepare("SELECT COUNT(*) FROM VectorTableMasterRecord WHERE id = ?");
        checkQuery.addBindValue(m_tableId);
        
        if (checkQuery.exec() && checkQuery.next()) {
            int count = checkQuery.value(0).toInt();
            qDebug() << "VectorTableMasterRecord中ID为" << m_tableId << "的记录数:" << count;
            
            if (count == 0) {
                qWarning() << "VectorTableModel::loadColumnConfiguration - 表ID在VectorTableMasterRecord中不存在";
                // 尝试检查vector_tables表
                checkQuery.prepare("SELECT COUNT(*) FROM vector_tables WHERE id = ?");
                checkQuery.addBindValue(m_tableId);
                
                if (checkQuery.exec() && checkQuery.next()) {
                    count = checkQuery.value(0).toInt();
                    qDebug() << "vector_tables中ID为" << m_tableId << "的记录数:" << count;
                    
                    if (count == 0) {
                        qWarning() << "VectorTableModel::loadColumnConfiguration - 表ID在vector_tables中也不存在";
                    }
                }
            }
        } else {
            qWarning() << "VectorTableModel::loadColumnConfiguration - 检查表是否存在失败:" 
                       << checkQuery.lastError().text() << ", SQL:" << checkQuery.lastQuery();
        }
        
        // 添加默认列：Label, Comment 和 TimeSet
        Vector::ColumnInfo labelColumn;
        labelColumn.id = -1; // 临时ID
        labelColumn.vector_table_id = m_tableId;
        labelColumn.type = Vector::ColumnDataType::TEXT;
        labelColumn.original_type_str = "TEXT";
        labelColumn.name = tr("Label");
        labelColumn.order = 0;
        m_columns.append(labelColumn);
        
        Vector::ColumnInfo commentColumn;
        commentColumn.id = -2; // 临时ID
        commentColumn.vector_table_id = m_tableId;
        commentColumn.type = Vector::ColumnDataType::TEXT;
        commentColumn.original_type_str = "TEXT";
        commentColumn.name = tr("Comment");
        commentColumn.order = 1;
        m_columns.append(commentColumn);
        
        Vector::ColumnInfo timesetColumn;
        timesetColumn.id = -3; // 临时ID
        timesetColumn.vector_table_id = m_tableId;
        timesetColumn.type = Vector::ColumnDataType::TIMESET_ID;
        timesetColumn.original_type_str = "TIMESET_ID";
        timesetColumn.name = tr("TimeSet");
        timesetColumn.order = 2;
        m_columns.append(timesetColumn);
        
        qDebug() << "VectorTableModel::loadColumnConfiguration - 添加了3个默认列";
    }
    
    return true;
}

bool VectorTableModel::loadRowData()
{
    const QString funcName = "VectorTableModel::loadRowData";
    qDebug() << funcName << " - 加载行数据，表ID:" << m_tableId;
    
    if (m_tableId <= 0) {
        qWarning() << funcName << " - 无效的表ID:" << m_tableId;
        return false;
    }
    
    // 清空现有行数据
    beginResetModel(); // 通知视图即将发生重大变化
    m_rows.clear();
    m_rowMetadata.clear();
    endResetModel();
    
    // 获取数据库连接并验证连接状态
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen()) {
        qWarning() << funcName << " - 数据库未连接";
        return false;
    }
    
    // 输出数据库连接状态，用于调试
    qDebug() << funcName << " - 数据库连接信息:";
    qDebug() << "  - 连接名称:" << db.connectionName();
    qDebug() << "  - 驱动程序:" << db.driverName();
    qDebug() << "  - 数据库路径:" << db.databaseName();
    qDebug() << "  - 连接状态:" << (db.isOpen() ? "已打开" : "未打开");
    
    // 尝试直接执行简单查询以测试连接
    QSqlQuery testQuery(db);
    if (!testQuery.exec("SELECT name FROM sqlite_master LIMIT 1")) {
        qWarning() << funcName << " - 数据库连接测试失败:" << testQuery.lastError().text();
        qWarning() << funcName << " - 可能的原因：驱动程序未正确加载或数据库连接无效";
        return false;
    } else {
        qDebug() << funcName << " - 数据库连接测试成功";
    }
    
    // 1. 从VectorTableMasterRecord表中获取二进制文件名
    QSqlQuery masterQuery(db);
    masterQuery.prepare("SELECT binary_data_filename FROM VectorTableMasterRecord WHERE id = ?");
    masterQuery.addBindValue(m_tableId);
    
    if (!masterQuery.exec() || !masterQuery.next()) {
        qWarning() << funcName << " - 无法在VectorTableMasterRecord中找到表ID:" << m_tableId << ", 错误:" << masterQuery.lastError().text();
        return false;
    }
    
    QString binaryFilename = masterQuery.value(0).toString();
    qDebug() << funcName << " - 从数据库获取到二进制文件名:" << binaryFilename;
    
    if (binaryFilename.isEmpty()) {
        qWarning() << funcName << " - 二进制文件名为空，可能是新建表或尚未保存二进制数据";
        return true; // 返回成功但无数据
    }
    
    // 2. 构建文件的完整路径
    QString dbPath = db.databaseName();
    QFileInfo dbInfo(dbPath);
    
    // 使用 PathUtils 工具类处理二进制文件路径
    // 先获取项目二进制数据目录
    QString projectBinaryDataDir = Utils::PathUtils::getProjectBinaryDataDirectory(dbPath);
    if (projectBinaryDataDir.isEmpty()) {
        qWarning() << funcName << " - 无法获取项目二进制数据目录";
        return false;
    }
    
    // 相对路径转绝对路径
    QString binaryFilePath;
    if (QFileInfo(binaryFilename).isRelative()) {
        binaryFilePath = QDir(projectBinaryDataDir).absoluteFilePath(binaryFilename);
        qDebug() << funcName << " - 相对路径转换为绝对路径:" << binaryFilename << " -> " << binaryFilePath;
    } else {
        binaryFilePath = binaryFilename;
    }
    
    qDebug() << funcName << " - 二进制文件路径:" << binaryFilePath;
    
    if (!QFile::exists(binaryFilePath)) {
        qWarning() << funcName << " - 二进制数据文件不存在:" << binaryFilePath;
        
        // 尝试创建二进制数据目录（如果不存在）
        QFileInfo binFileInfo(binaryFilePath);
        QDir binDir = binFileInfo.dir();
        if (!binDir.exists()) {
            qInfo() << funcName << " - 目标二进制目录不存在，尝试创建:" << binDir.absolutePath();
            if (!binDir.mkpath(".")) {
                qWarning() << funcName << " - 无法创建二进制数据目录:" << binDir.absolutePath();
            } else {
                qInfo() << funcName << " - 成功创建二进制数据目录:" << binDir.absolutePath();
            }
        }
        
        // 二进制文件确实不存在，返回错误
        qWarning() << funcName << " - 数据文件不存在，请先保存数据";
        return false;
    }
    
    // 3. 使用BinaryFileHelper读取二进制文件中的所有行数据
    QList<Vector::RowData> allRows;
    bool success = Persistence::BinaryFileHelper::readAllRowsFromBinary(
        binaryFilePath, 
        m_columns,
        db.databaseName().contains("_v3") ? 3 : (db.databaseName().contains("_v2") ? 2 : 1), // 从数据库文件名推断schema版本
        allRows
    );
    
    if (!success) {
        qWarning() << funcName << " - 从二进制文件读取数据失败";
        return false;
    }
    
    qDebug() << funcName << " - 从二进制文件中读取了" << allRows.count() << "行数据";
    
    if (allRows.isEmpty()) {
        qDebug() << funcName << " - 二进制文件中没有数据";
        return true; // 返回成功但无数据
    }
    
    // 4. 将读取到的数据加载到模型中，同时提取并存储行元数据
    if (allRows.isEmpty()) {
        qDebug() << funcName << " - 没有行数据需要插入";
        return true;
    }
    
    // 确保所有行都有足够的列数，避免数据不一致
    for (int i = 0; i < allRows.count(); i++) {
        Vector::RowData &rowData = allRows[i];
        // 如果行的列数少于模型的列数，补充空值
        while (rowData.count() < m_columns.count()) {
            rowData.append(QVariant());
        }
    }
    
    // 开始插入行
    beginInsertRows(QModelIndex(), 0, allRows.count() - 1);
    m_rows = allRows;
    
    // 提取行元数据
    for (int i = 0; i < m_rows.count(); i++) {
        const Vector::RowData &rowData = m_rows[i];
        RowMetadata metadata;
        metadata.label = "";
        metadata.comment = "";
        metadata.timeSetId = 1; // 默认值
        
        // 在列中查找Label、Comment和TimeSet列
        for (int j = 0; j < m_columns.count() && j < rowData.count(); j++) {
            const Vector::ColumnInfo &column = m_columns[j];
            if (column.name.toLower() == "label") {
                metadata.label = rowData[j].toString();
            } else if (column.name.toLower() == "comment") {
                metadata.comment = rowData[j].toString();
            } else if (column.name.toLower() == "timeset" || column.name.toLower() == "timeset_id") {
                metadata.timeSetId = rowData[j].toInt();
            }
        }
        
        m_rowMetadata[i] = metadata;
    }
    endInsertRows();
    
    qDebug() << funcName << " - 完成数据加载，共" << m_rows.count() << "行";
    return true;
}
