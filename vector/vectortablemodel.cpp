#include "vectortablemodel.h"
#include "database/databasemanager.h"
#include "database/binaryfilehelper.h"
#include <QDebug>
#include <QTableWidget> // Added for QTableWidget
#include <QSet> // Added for QSet
#include <QSqlQuery>
#include <QSqlError>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QApplication>
#include <QColor>
#include <QBrush>
#include <QDateTime>

VectorTableModel::VectorTableModel(QObject *parent)
    : QAbstractTableModel(parent),
      m_tableId(-1),
      m_schemaVersion(0)
{
    qDebug() << "VectorTableModel::VectorTableModel - 创建向量表模型";
}

VectorTableModel::~VectorTableModel()
{
    qDebug() << "VectorTableModel::~VectorTableModel - 销毁向量表模型";
}

int VectorTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    
    return m_rows.size();
}

int VectorTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    
    return m_columns.size();
}

QVariant VectorTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size() || index.column() >= m_columns.size())
        return QVariant();

    const Vector::RowData &rowData = m_rows.at(index.row());
    const Vector::ColumnInfo &colInfo = m_columns.at(index.column());
    
    // 确保行数据与列配置匹配
    if (index.column() >= rowData.size())
        return QVariant();

    // 根据不同的角色返回不同的数据
    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
        return formatDisplayValue(rowData.at(index.column()), colInfo.type);
        
    case Qt::TextAlignmentRole:
        // 文本居中对齐
        return Qt::AlignCenter;
        
    case Qt::BackgroundRole:
        // 如果行被修改，显示浅黄色背景
        if (isRowModified(index.row()))
            return QBrush(QColor(255, 255, 200)); // 浅黄色
        return QVariant();
        
    default:
        return QVariant();
    }
}

QVariant VectorTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();
    
    if (orientation == Qt::Horizontal) {
        // 返回列标题
        if (section >= 0 && section < m_columns.size())
            return m_columns.at(section).name;
        return QVariant();
    } else {
        // 返回行号（从1开始）
        return section + 1;
    }
}

bool VectorTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() >= m_rows.size() || index.column() >= m_columns.size() || role != Qt::EditRole)
        return false;
    
    // 获取当前行数据和列信息
    Vector::RowData &rowData = m_rows[index.row()];
    const Vector::ColumnInfo &colInfo = m_columns.at(index.column());
    
    // 确保行数据与列配置匹配
    if (index.column() >= rowData.size())
        return false;
    
    // 解析编辑值
    QVariant parsedValue = parseEditValue(value, colInfo.type);
    
    // 如果值没有变化，不做任何操作
    if (rowData.at(index.column()) == parsedValue)
        return false;
    
    // 更新数据
    rowData[index.column()] = parsedValue;
    
    // 标记行已修改
    markRowAsModified(index.row());
    
    // 发送数据变更信号
    emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
    
    return true;
}

Qt::ItemFlags VectorTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

bool VectorTableModel::loadTable(int tableId)
{
    qDebug() << "VectorTableModel::loadTable - 加载表格数据，表ID:" << tableId;
    
    if (tableId <= 0) {
        qWarning() << "VectorTableModel::loadTable - 无效的表格ID:" << tableId;
        return false;
    }
    
    // 清空现有数据
    beginResetModel();
    m_tableId = tableId;
    m_columns.clear();
    m_rows.clear();
    m_modifiedRows.clear();
    
    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen()) {
        qWarning() << "VectorTableModel::loadTable - 数据库未打开";
        endResetModel();
        return false;
    }
    
    // 获取表格元数据
    QString binFileName;
    int rowCount = 0;
    
    QSqlQuery query(db);
    query.prepare("SELECT BinaryFileName, SchemaVersion, RowCount FROM VectorTableMasterRecord WHERE ID = ?");
    query.addBindValue(tableId);
    
    if (!query.exec() || !query.next()) {
        qWarning() << "VectorTableModel::loadTable - 获取表格元数据失败:" << query.lastError().text();
        endResetModel();
        return false;
    }
    
    binFileName = query.value("BinaryFileName").toString();
    m_schemaVersion = query.value("SchemaVersion").toInt();
    rowCount = query.value("RowCount").toInt();
    
    // 获取列配置
    query.prepare("SELECT ID, VectorTableID, Name, ColumnOrder, DataType, DataProperties, IsVisible "
                 "FROM VectorTableColumnConfiguration "
                 "WHERE VectorTableID = ? AND IsVisible = 1 "
                 "ORDER BY ColumnOrder");
    query.addBindValue(tableId);
    
    if (!query.exec()) {
        qWarning() << "VectorTableModel::loadTable - 获取列配置失败:" << query.lastError().text();
        endResetModel();
        return false;
    }
    
    while (query.next()) {
        Vector::ColumnInfo colInfo;
        colInfo.id = query.value("ID").toInt();
        colInfo.vector_table_id = query.value("VectorTableID").toInt();
        colInfo.name = query.value("Name").toString();
        colInfo.order = query.value("ColumnOrder").toInt();
        colInfo.original_type_str = query.value("DataType").toString();
        colInfo.type = Vector::columnDataTypeFromString(colInfo.original_type_str);
        colInfo.is_visible = query.value("IsVisible").toBool();
        
        // 解析JSON属性
        QString dataPropertiesStr = query.value("DataProperties").toString();
        if (!dataPropertiesStr.isEmpty()) {
            QJsonDocument jsonDoc = QJsonDocument::fromJson(dataPropertiesStr.toUtf8());
            if (!jsonDoc.isNull() && jsonDoc.isObject()) {
                colInfo.data_properties = jsonDoc.object();
            }
        }
        
        m_columns.append(colInfo);
    }
    
    if (m_columns.isEmpty()) {
        qWarning() << "VectorTableModel::loadTable - 表格没有可见列";
        endResetModel();
        return false;
    }
    
    // 解析二进制文件路径
    QString errorMsg;
    m_binaryFilePath = resolveBinaryFilePath(tableId, errorMsg);
    
    if (m_binaryFilePath.isEmpty()) {
        qWarning() << "VectorTableModel::loadTable - 解析二进制文件路径失败:" << errorMsg;
        endResetModel();
        return false;
    }
    
    // 加载二进制文件
    bool loadSuccess = loadBinaryFile(m_binaryFilePath);
    
    endResetModel();
    
    return loadSuccess;
}

bool VectorTableModel::saveTable(QString &errorMessage)
{
    qDebug() << "VectorTableModel::saveTable - 保存表格数据，表ID:" << m_tableId;
    
    if (m_tableId <= 0) {
        errorMessage = "无效的表格ID";
        return false;
    }
    
    if (m_binaryFilePath.isEmpty()) {
        errorMessage = "二进制文件路径为空";
        return false;
    }
    
    if (m_modifiedRows.isEmpty()) {
        errorMessage = "没有检测到数据变更，无需保存";
        return true;
    }
    
    // 保存二进制文件
    bool saveSuccess = saveBinaryFile(m_binaryFilePath, errorMessage);
    
    if (saveSuccess) {
        // 清除修改标记
        clearModifiedRows();
        
        // 更新数据库中的行数记录
        QSqlDatabase db = DatabaseManager::instance()->database();
        if (db.isOpen()) {
            QSqlQuery query(db);
            query.prepare("UPDATE VectorTableMasterRecord SET RowCount = ? WHERE ID = ?");
            query.addBindValue(m_rows.size());
            query.addBindValue(m_tableId);
            
            if (!query.exec()) {
                qWarning() << "VectorTableModel::saveTable - 更新行数记录失败:" << query.lastError().text();
            }
        }
        
        // 发送保存成功信号（使用-1表示保存了整个表）
        emit dataSaved(m_tableId, -1);
    }
    
    return saveSuccess;
}

int VectorTableModel::getCurrentTableId() const
{
    return m_tableId;
}

QList<Vector::ColumnInfo> VectorTableModel::getColumnConfiguration() const
{
    return m_columns;
}

Vector::RowData VectorTableModel::getRowData(int row) const
{
    if (row >= 0 && row < m_rows.size())
        return m_rows.at(row);
    
    return Vector::RowData();
}

bool VectorTableModel::setRowData(int row, const Vector::RowData &data)
{
    if (row < 0 || row >= m_rows.size() || data.size() != m_columns.size())
        return false;
    
    m_rows[row] = data;
    markRowAsModified(row);
    
    emit dataChanged(index(row, 0), index(row, columnCount() - 1));
    
    return true;
}

bool VectorTableModel::insertRow(int row, const Vector::RowData &data)
{
    return insertRows(row, 1);
}

bool VectorTableModel::insertRows(int row, int count, const QModelIndex &parent)
{
    if (row < 0 || count <= 0 || parent.isValid())
        return false;
    
    // 确保行索引不超过现有行数
    int insertPosition = qMin(row, m_rows.size());
    
    beginInsertRows(QModelIndex(), insertPosition, insertPosition + count - 1);
    
    // 创建默认行数据
    for (int i = 0; i < count; ++i) {
        Vector::RowData newRow;
        
        // 为每列创建默认值
        for (const Vector::ColumnInfo &colInfo : m_columns) {
            switch (colInfo.type) {
            case Vector::ColumnDataType::INTEGER:
            case Vector::ColumnDataType::INSTRUCTION_ID:
            case Vector::ColumnDataType::TIMESET_ID:
            case Vector::ColumnDataType::PIN_STATE_ID:
                newRow.append(0);
                break;
            case Vector::ColumnDataType::REAL:
                newRow.append(0.0);
                break;
            case Vector::ColumnDataType::BOOLEAN:
                newRow.append(false);
                break;
            case Vector::ColumnDataType::TEXT:
            case Vector::ColumnDataType::JSON_PROPERTIES:
            default:
                newRow.append("");
                break;
            }
        }
        
        m_rows.insert(insertPosition + i, newRow);
        markRowAsModified(insertPosition + i);
    }
    
    endInsertRows();
    
    return true;
}

bool VectorTableModel::removeRow(int row, const QModelIndex &parent)
{
    return removeRows(row, 1, parent);
}

bool VectorTableModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if (row < 0 || count <= 0 || row + count > m_rows.size() || parent.isValid())
        return false;
    
    beginRemoveRows(QModelIndex(), row, row + count - 1);
    
    for (int i = 0; i < count; ++i) {
        m_rows.removeAt(row);
    }
    
    // 更新修改标记
    QSet<int> newModifiedRows;
    for (int modifiedRow : m_modifiedRows) {
        if (modifiedRow < row) {
            newModifiedRows.insert(modifiedRow);
        } else if (modifiedRow >= row + count) {
            newModifiedRows.insert(modifiedRow - count);
        }
    }
    m_modifiedRows = newModifiedRows;
    
    endRemoveRows();
    
    return true;
}

void VectorTableModel::refresh()
{
    if (m_tableId > 0) {
        loadTable(m_tableId);
    }
}

QString VectorTableModel::getColumnName(int column) const
{
    if (column >= 0 && column < m_columns.size())
        return m_columns.at(column).name;
    
    return QString();
}

Vector::ColumnDataType VectorTableModel::getColumnType(int column) const
{
    if (column >= 0 && column < m_columns.size())
        return m_columns.at(column).type;
    
    return Vector::ColumnDataType::TEXT;
}

void VectorTableModel::markRowAsModified(int row)
{
    if (row >= 0 && row < m_rows.size()) {
        m_modifiedRows.insert(row);
    }
}

bool VectorTableModel::isRowModified(int row) const
{
    return m_modifiedRows.contains(row);
}

void VectorTableModel::clearModifiedRows()
{
    m_modifiedRows.clear();
}

bool VectorTableModel::loadBinaryFile(const QString &filePath)
{
    qDebug() << "VectorTableModel::loadBinaryFile - 加载二进制文件:" << filePath;
    
    QFile file(filePath);
    if (!file.exists()) {
        qWarning() << "VectorTableModel::loadBinaryFile - 文件不存在:" << filePath;
        return false;
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "VectorTableModel::loadBinaryFile - 无法打开文件:" << file.errorString();
        return false;
    }
    
    // 获取文件信息
    QFileInfo fileInfo(file);
    m_lastModified = fileInfo.lastModified();
    
    // 使用BinaryFileHelper加载数据
    Persistence::BinaryFileHelper binaryHelper;
    QList<Vector::RowData> rows;
    bool success = Persistence::BinaryFileHelper::readAllRowsFromBinary(filePath, m_columns, m_schemaVersion, rows);
    
    file.close();
    
    if (success) {
        m_rows = rows;
        qDebug() << "VectorTableModel::loadBinaryFile - 成功加载" << m_rows.size() << "行数据";
    } else {
        qWarning() << "VectorTableModel::loadBinaryFile - 加载数据失败";
    }
    
    return success;
}

bool VectorTableModel::saveBinaryFile(const QString &filePath, QString &errorMessage)
{
    qDebug() << "VectorTableModel::saveBinaryFile - 保存二进制文件:" << filePath;
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        errorMessage = "无法打开文件进行写入: " + file.errorString();
        return false;
    }
    
    // 使用BinaryFileHelper保存数据
    bool success = Persistence::BinaryFileHelper::writeAllRowsToBinary(filePath, m_columns, m_schemaVersion, m_rows);
    
    file.close();
    
    if (!success) {
        errorMessage = "保存数据失败";
    }
    
    return success;
}

QVariant VectorTableModel::formatDisplayValue(const QVariant &value, Vector::ColumnDataType type) const
{
    switch (type) {
    case Vector::ColumnDataType::BOOLEAN:
        return value.toBool() ? "True" : "False";
        
    case Vector::ColumnDataType::PIN_STATE_ID:
        {
            int stateId = value.toInt();
            switch (stateId) {
            case 0: return "0";
            case 1: return "1";
            case 2: return "L";
            case 3: return "H";
            case 4: return "Z";
            case 5: return "X";
            default: return value;
            }
        }
        
    case Vector::ColumnDataType::INSTRUCTION_ID:
        {
            // 从数据库获取指令名称
            int instructionId = value.toInt();
            QSqlDatabase db = DatabaseManager::instance()->database();
            if (db.isOpen()) {
                QSqlQuery query(db);
                query.prepare("SELECT Name FROM Instructions WHERE ID = ?");
                query.addBindValue(instructionId);
                
                if (query.exec() && query.next()) {
                    return query.value("Name").toString();
                }
            }
            return value;
        }
        
    case Vector::ColumnDataType::TIMESET_ID:
        {
            // 从数据库获取TimeSet名称
            int timeSetId = value.toInt();
            QSqlDatabase db = DatabaseManager::instance()->database();
            if (db.isOpen()) {
                QSqlQuery query(db);
                query.prepare("SELECT Name FROM TimeSets WHERE ID = ?");
                query.addBindValue(timeSetId);
                
                if (query.exec() && query.next()) {
                    return query.value("Name").toString();
                }
            }
            return value;
        }
        
    default:
        return value;
    }
}

QVariant VectorTableModel::parseEditValue(const QVariant &value, Vector::ColumnDataType type) const
{
    switch (type) {
    case Vector::ColumnDataType::BOOLEAN:
        {
            QString strValue = value.toString().toLower();
            return (strValue == "true" || strValue == "1" || strValue == "yes" || strValue == "y");
        }
        
    case Vector::ColumnDataType::PIN_STATE_ID:
        {
            QString strValue = value.toString().toUpper();
            if (strValue == "0") return 0;
            if (strValue == "1") return 1;
            if (strValue == "L") return 2;
            if (strValue == "H") return 3;
            if (strValue == "Z") return 4;
            if (strValue == "X") return 5;
            return 0; // 默认为0
        }
        
    case Vector::ColumnDataType::INSTRUCTION_ID:
        {
            // 如果是数字，直接返回
            bool ok;
            int id = value.toInt(&ok);
            if (ok) return id;
            
            // 如果是指令名称，查找对应的ID
            QString strValue = value.toString();
            QSqlDatabase db = DatabaseManager::instance()->database();
            if (db.isOpen()) {
                QSqlQuery query(db);
                query.prepare("SELECT ID FROM Instructions WHERE Name = ?");
                query.addBindValue(strValue);
                
                if (query.exec() && query.next()) {
                    return query.value("ID").toInt();
                }
            }
            return 0; // 默认为0
        }
        
    case Vector::ColumnDataType::TIMESET_ID:
        {
            // 如果是数字，直接返回
            bool ok;
            int id = value.toInt(&ok);
            if (ok) return id;
            
            // 如果是TimeSet名称，查找对应的ID
            QString strValue = value.toString();
            QSqlDatabase db = DatabaseManager::instance()->database();
            if (db.isOpen()) {
                QSqlQuery query(db);
                query.prepare("SELECT ID FROM TimeSets WHERE Name = ?");
                query.addBindValue(strValue);
                
                if (query.exec() && query.next()) {
                    return query.value("ID").toInt();
                }
            }
            return 0; // 默认为0
        }
        
    case Vector::ColumnDataType::INTEGER:
        return value.toInt();
        
    case Vector::ColumnDataType::REAL:
        return value.toDouble();
        
    default:
        return value;
    }
}

// 辅助函数：解析二进制文件路径
QString VectorTableModel::resolveBinaryFilePath(int tableId, QString &errorMsg)
{
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen()) {
        errorMsg = "数据库未打开";
        return QString();
    }
    
    QSqlQuery query(db);
    query.prepare("SELECT BinaryFileName FROM VectorTableMasterRecord WHERE ID = ?");
    query.addBindValue(tableId);
    
    if (!query.exec() || !query.next()) {
        errorMsg = "获取二进制文件名失败: " + query.lastError().text();
        return QString();
    }
    
    QString binFileName = query.value("BinaryFileName").toString();
    if (binFileName.isEmpty()) {
        errorMsg = "二进制文件名为空";
        return QString();
    }
    
    // 获取数据库文件路径
    QString dbFilePath = db.databaseName();
    QFileInfo dbFileInfo(dbFilePath);
    QString dbDir = dbFileInfo.absolutePath();
    
    // 构建二进制文件的绝对路径
    QString binFilePath = dbDir + QDir::separator() + binFileName;
    
    return binFilePath;
}

bool VectorTableModel::hasModifiedRows() const
{
    return !m_modifiedRows.isEmpty();
} 