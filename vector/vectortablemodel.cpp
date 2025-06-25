#include "vectortablemodel.h"
#include "database/databasemanager.h"
#include "database/binaryfilehelper.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QColor>
#include <QBrush>
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QApplication>
#include <QPalette>

VectorTableModel::VectorTableModel(QObject *parent)
    : QAbstractTableModel(parent),
      m_tableId(-1),
      m_schemaVersion(0),
      m_totalRowCount(0),
      m_cacheSegmentSize(DEFAULT_CACHE_SEGMENT_SIZE)
{
    // 获取系统交替行颜色
    QPalette palette = QApplication::palette();
    m_alternateRowColor = palette.alternateBase().color();
}

VectorTableModel::~VectorTableModel()
{
    clearData();
}

int VectorTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return m_totalRowCount;
}

int VectorTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return m_columns.size();
}

QVariant VectorTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_totalRowCount || index.row() < 0 || index.column() >= m_columns.size() || index.column() < 0)
        return QVariant();

    // 首先检查编辑缓冲区
    QVariant editValue;
    if (getEditBufferValue(index.row(), index.column(), editValue))
        return editValue;

    // 查找缓存段
    const CacheSegment *segment = findCacheSegment(index.row());

    // 如果未找到缓存，尝试加载数据
    if (!segment)
    {
        int startRow = (index.row() / m_cacheSegmentSize) * m_cacheSegmentSize;
        if (!loadDataSegment(startRow, m_cacheSegmentSize))
        {
            qWarning() << "VectorTableModel::data - 加载数据段失败，行:" << index.row();
            return QVariant();
        }
        segment = findCacheSegment(index.row());
        if (!segment)
        {
            qWarning() << "VectorTableModel::data - 缓存段加载后仍未找到，行:" << index.row();
            return QVariant();
        }
    }

    // 计算缓存内偏移
    int rowOffset = index.row() - segment->startRow;
    if (rowOffset >= segment->rows.size())
    {
        qWarning() << "VectorTableModel::data - 缓存段边界错误，行:" << index.row()
                   << "，段起始行:" << segment->startRow
                   << "，段大小:" << segment->count;
        return QVariant();
    }

    // 获取指定行数据
    const Vector::RowData &rowData = segment->rows[rowOffset];

    // 检查列索引是否有效
    if (index.column() >= rowData.size())
    {
        qWarning() << "VectorTableModel::data - 列索引越界，列:" << index.column()
                   << "，行数据大小:" << rowData.size();
        return QVariant();
    }

    const Vector::ColumnInfo &colInfo = m_columns[index.column()];

    // 根据不同角色返回不同数据
    if (role == Qt::DisplayRole || role == Qt::EditRole)
    {
        QVariant value = rowData.value(index.column());

        // 根据列类型处理特殊显示
        switch (colInfo.type)
        {
        case Vector::ColumnDataType::TIMESET_ID:
            // 如果是TimeSet ID，尝试查询对应的名称
            if (role == Qt::DisplayRole)
            {
                int timesetId = value.toInt();
                // TODO: 从缓存中获取TimeSet名称
                // 暂时直接返回ID
                return QString("TS%1").arg(timesetId);
            }
            return value;

        case Vector::ColumnDataType::INSTRUCTION_ID:
            // 如果是指令ID，尝试查询对应的名称
            if (role == Qt::DisplayRole)
            {
                int instrId = value.toInt();
                // TODO: 从缓存中获取指令名称
                // 暂时直接返回ID
                return QString("Instr%1").arg(instrId);
            }
            return value;

        default:
            return value;
        }
    }
    else if (role == Qt::BackgroundRole)
    {
        // 设置交替行背景色
        if (index.row() % 2)
            return m_alternateRowColor;

        // 如果行被修改，设置特殊背景色
        if (m_modifiedRows.contains(index.row()))
            return QColor(255, 255, 200); // 浅黄色背景表示已修改
    }

    return QVariant();
}

QVariant VectorTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal)
    {
        if (section >= 0 && section < m_columns.size())
            return m_columns[section].name;
        return QVariant();
    }

    // 垂直表头显示行号（从1开始）
    return section + 1;
}

bool VectorTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() >= m_totalRowCount || index.column() >= m_columns.size() || role != Qt::EditRole)
        return false;

    // 保存编辑值到缓冲区
    IndexKey key = qMakePair(index.row(), index.column());
    m_pendingEdits[key] = value;

    // 标记行为已修改
    markRowAsModified(index.row());

    // 通知视图数据已更改
    emit dataChanged(index, index);
    return true;
}

Qt::ItemFlags VectorTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    // 默认标志：可选择、已启用
    Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;

    // 根据列类型决定是否可编辑
    const Vector::ColumnInfo &colInfo = m_columns[index.column()];

    // 大多数类型都可编辑
    flags |= Qt::ItemIsEditable;

    return flags;
}

bool VectorTableModel::loadTable(int tableId)
{
    // 清除旧数据
    beginResetModel();
    clearData();

    m_tableId = tableId;

    // 加载表元数据
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << "VectorTableModel::loadTable - 数据库未打开";
        endResetModel();
        return false;
    }

    // 查询主记录表
    QSqlQuery metaQuery(db);
    metaQuery.prepare("SELECT binary_data_filename, data_schema_version, row_count FROM VectorTableMasterRecord WHERE id = ?");
    metaQuery.addBindValue(tableId);
    if (!metaQuery.exec() || !metaQuery.next())
    {
        qWarning() << "VectorTableModel::loadTable - 查询主记录失败, 表ID:" << tableId << ", 错误:" << metaQuery.lastError().text();
        endResetModel();
        return false;
    }

    m_binaryFilePath = metaQuery.value(0).toString();
    m_schemaVersion = metaQuery.value(1).toInt();
    m_totalRowCount = metaQuery.value(2).toInt();

    // 获取数据库路径，用于解析相对路径
    QString dbFilePath = db.databaseName();
    QFileInfo dbFileInfo(dbFilePath);
    QString dbDir = dbFileInfo.absolutePath();

    // 确保使用正确的绝对路径
    QFileInfo binFileInfo(m_binaryFilePath);
    if (binFileInfo.isRelative())
    {
        m_binaryFilePath = dbDir + QDir::separator() + m_binaryFilePath;
        m_binaryFilePath = QDir::toNativeSeparators(m_binaryFilePath);
    }

    // 查询列结构 - 只加载IsVisible=1的列
    QSqlQuery colQuery(db);
    colQuery.prepare("SELECT id, column_name, column_order, column_type, data_properties, IsVisible FROM VectorTableColumnConfiguration WHERE master_record_id = ? AND IsVisible = 1 ORDER BY column_order");
    colQuery.addBindValue(tableId);
    if (!colQuery.exec())
    {
        qWarning() << "VectorTableModel::loadTable - 查询列结构失败, 错误:" << colQuery.lastError().text();
        endResetModel();
        return false;
    }

    while (colQuery.next())
    {
        Vector::ColumnInfo col;
        col.id = colQuery.value(0).toInt();
        col.vector_table_id = tableId;
        col.name = colQuery.value(1).toString();
        col.order = colQuery.value(2).toInt();
        col.original_type_str = colQuery.value(3).toString();
        col.type = Vector::columnDataTypeFromString(col.original_type_str);
        col.is_visible = colQuery.value(5).toBool();

        QString propStr = colQuery.value(4).toString();
        if (!propStr.isEmpty())
        {
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(propStr.toUtf8(), &err);
            if (err.error == QJsonParseError::NoError && doc.isObject())
            {
                col.data_properties = doc.object();
            }
        }
        m_columns.append(col);
    }

    // 模型重置完成
    endResetModel();

    // 加载第一段数据
    if (m_totalRowCount > 0)
    {
        int initialLoadSize = qMin(m_cacheSegmentSize, m_totalRowCount);
        loadDataSegment(0, initialLoadSize);
    }

    return true;
}

void VectorTableModel::clearData()
{
    // 清除所有数据和缓存
    m_tableId = -1;
    m_binaryFilePath.clear();
    m_columns.clear();
    m_schemaVersion = 0;
    m_totalRowCount = 0;
    m_dataCache.clear();
    m_modifiedRows.clear();
    m_pendingEdits.clear();
}

int VectorTableModel::getTableId() const
{
    return m_tableId;
}

QList<Vector::ColumnInfo> VectorTableModel::getColumns() const
{
    return m_columns;
}

Vector::ColumnInfo VectorTableModel::getColumnInfo(int column) const
{
    if (column >= 0 && column < m_columns.size())
        return m_columns[column];

    return Vector::ColumnInfo();
}

bool VectorTableModel::insertRows(int row, int count, const QModelIndex &parent)
{
    if (row < 0 || count <= 0 || parent.isValid())
        return false;

    // 确保行号有效
    row = qMin(row, m_totalRowCount);

    beginInsertRows(QModelIndex(), row, row + count - 1);

    // 增加总行数
    m_totalRowCount += count;

    // 在数据库中插入新行
    // TODO: 实现数据库行插入逻辑

    // 清除缓存，以便下次访问时重新加载
    m_dataCache.clear();

    endInsertRows();
    return true;
}

bool VectorTableModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if (row < 0 || count <= 0 || row + count > m_totalRowCount || parent.isValid())
        return false;

    beginRemoveRows(QModelIndex(), row, row + count - 1);

    // 从数据库中删除行
    // TODO: 实现数据库行删除逻辑

    // 减少总行数
    m_totalRowCount -= count;

    // 清除缓存，以便下次访问时重新加载
    m_dataCache.clear();

    endRemoveRows();
    return true;
}

bool VectorTableModel::saveData()
{
    if (m_tableId < 0 || m_pendingEdits.isEmpty())
        return true; // 没有可保存的内容

    // TODO: 实现保存逻辑，将编辑缓冲区的数据保存到数据库和二进制文件

    // 标记所有修改已保存
    m_pendingEdits.clear();
    m_modifiedRows.clear();

    return true;
}

Vector::RowData VectorTableModel::getRowData(int row) const
{
    if (row < 0 || row >= m_totalRowCount)
        return Vector::RowData();

    // 查找缓存段
    const CacheSegment *segment = findCacheSegment(row);

    // 如果未找到缓存，尝试加载数据
    if (!segment)
    {
        int startRow = (row / m_cacheSegmentSize) * m_cacheSegmentSize;
        if (!loadDataSegment(startRow, m_cacheSegmentSize))
            return Vector::RowData();

        segment = findCacheSegment(row);
        if (!segment)
            return Vector::RowData();
    }

    // 计算缓存内偏移
    int rowOffset = row - segment->startRow;
    if (rowOffset >= segment->rows.size())
        return Vector::RowData();

    // 获取行数据基础副本
    Vector::RowData rowData = segment->rows[rowOffset];

    // 应用所有待定的编辑
    for (int col = 0; col < qMin(rowData.size(), m_columns.size()); ++col)
    {
        IndexKey key = qMakePair(row, col);
        if (m_pendingEdits.contains(key))
            rowData[col] = m_pendingEdits[key];
    }

    return rowData;
}

bool VectorTableModel::setRowData(int row, const Vector::RowData &rowData)
{
    if (row < 0 || row >= m_totalRowCount)
        return false;

    // 将整行数据添加到编辑缓冲区
    for (int col = 0; col < qMin(rowData.size(), m_columns.size()); ++col)
    {
        IndexKey key = qMakePair(row, col);
        m_pendingEdits[key] = rowData[col];
    }

    // 标记行为已修改
    markRowAsModified(row);

    // 通知视图数据已更改
    emit dataChanged(index(row, 0), index(row, m_columns.size() - 1));
    return true;
}

QColor VectorTableModel::getRowBackground(int row) const
{
    if (row % 2)
        return m_alternateRowColor;

    return QColor(Qt::white);
}

bool VectorTableModel::isRowModified(int row) const
{
    return m_modifiedRows.contains(row);
}

void VectorTableModel::markRowAsModified(int row)
{
    if (row >= 0 && row < m_totalRowCount)
        m_modifiedRows.insert(row);
}

void VectorTableModel::clearModifiedRows()
{
    m_modifiedRows.clear();
}

bool VectorTableModel::loadDataSegment(int startRow, int count) const
{
    if (startRow < 0 || count <= 0)
        return false;

    // 确保不超出总行数
    if (startRow >= m_totalRowCount)
        return false;

    // 调整count确保不超出总行数
    count = qMin(count, m_totalRowCount - startRow);

    // 先检查现有缓存段是否已包含请求的数据范围
    for (const CacheSegment &segment : m_dataCache)
    {
        if (segment.contains(startRow) && segment.contains(startRow + count - 1))
            return true; // 已经缓存
    }

    // 从数据库加载行
    QList<Vector::RowData> rows;
    if (!loadRowsFromDatabase(startRow, count, rows))
        return false;

    // 添加到缓存
    CacheSegment newSegment;
    newSegment.startRow = startRow;
    newSegment.count = rows.size();
    newSegment.rows = rows;

    // 管理缓存大小（如果需要可以实现LRU策略）
    const int MAX_CACHE_SEGMENTS = 5; // 最多缓存5个段
    while (m_dataCache.size() >= MAX_CACHE_SEGMENTS)
    {
        m_dataCache.removeFirst();
    }

    m_dataCache.append(newSegment);
    return true;
}

bool VectorTableModel::loadRowsFromDatabase(int startRow, int count, QList<Vector::RowData> &rows) const
{
    if (m_tableId < 0 || startRow < 0 || count <= 0)
        return false;

    // 打开二进制文件
    QFile file(m_binaryFilePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "VectorTableModel::loadRowsFromDatabase - 无法打开文件:" << m_binaryFilePath;
        return false;
    }

    // 读取文件头
    BinaryFileHeader header;
    if (!Persistence::BinaryFileHelper::readBinaryHeader(&file, header))
    {
        qWarning() << "VectorTableModel::loadRowsFromDatabase - 文件头读取失败";
        file.close();
        return false;
    }

    // 检查行范围
    if (startRow >= header.row_count_in_file)
    {
        qWarning() << "VectorTableModel::loadRowsFromDatabase - 起始行超出文件总行数";
        file.close();
        return false;
    }

    // 限制读取的实际行数
    int actualCount = qMin(count, static_cast<int>(header.row_count_in_file - startRow));

    // 跳过文件头和前面的行
    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);

    // 找到起始行的位置
    bool success = Persistence::BinaryFileHelper::seekToRow(&file, startRow);
    if (!success)
    {
        qWarning() << "VectorTableModel::loadRowsFromDatabase - 无法定位到起始行:" << startRow;
        file.close();
        return false;
    }

    // 读取指定数量的行
    rows.clear();
    for (int i = 0; i < actualCount; ++i)
    {
        QByteArray rowBytes;
        quint32 rowLen = 0;
        in >> rowLen;

        if (in.status() != QDataStream::Ok || rowLen == 0)
        {
            qWarning() << "VectorTableModel::loadRowsFromDatabase - 行长度读取失败, 行:" << (startRow + i);
            file.close();
            return false;
        }

        // 添加对行大小的安全检查
        const quint32 MAX_REASONABLE_ROW_SIZE = 1 * 1024 * 1024; // 1MB 是合理的单行最大值
        if (rowLen > MAX_REASONABLE_ROW_SIZE)
        {
            qCritical() << "VectorTableModel::loadRowsFromDatabase - 检测到异常大的行大小:" << rowLen
                        << "字节，超过合理限制" << MAX_REASONABLE_ROW_SIZE << "字节，行:" << (startRow + i);
            file.close();
            return false;
        }

        rowBytes.resize(rowLen);
        if (file.read(rowBytes.data(), rowLen) != rowLen)
        {
            qWarning() << "VectorTableModel::loadRowsFromDatabase - 行数据读取失败, 行:" << (startRow + i);
            file.close();
            return false;
        }

        Vector::RowData rowData;
        if (!Persistence::BinaryFileHelper::deserializeRow(rowBytes, m_columns, m_schemaVersion, rowData))
        {
            qWarning() << "VectorTableModel::loadRowsFromDatabase - 行反序列化失败, 行:" << (startRow + i);
            file.close();
            return false;
        }

        rows.append(rowData);
    }

    file.close();
    return true;
}

const VectorTableModel::CacheSegment *VectorTableModel::findCacheSegment(int row) const
{
    for (const CacheSegment &segment : m_dataCache)
    {
        if (segment.contains(row))
            return &segment;
    }
    return nullptr;
}

bool VectorTableModel::getEditBufferValue(int row, int column, QVariant &value) const
{
    IndexKey key = qMakePair(row, column);
    auto it = m_pendingEdits.find(key);
    if (it != m_pendingEdits.end())
    {
        value = it.value();
        return true;
    }
    return false;
}