#include "vectortablemodel.h"
#include <QDebug>

VectorTableModel::VectorTableModel(VectorDataHandler *handler, QObject *parent)
    : QAbstractTableModel(parent), m_dataHandler(handler), m_currentTableId(-1), m_rowCountCache(0)
{
    Q_ASSERT(m_dataHandler);
}

void VectorTableModel::refreshModel(int tableId, int page, int pageSize)
{
    beginResetModel();
    m_currentTableId = tableId;
    if (m_currentTableId != -1)
    {
        m_columnInfoCache = m_dataHandler->getAllColumnInfo(m_currentTableId);
        if (page == -1)
        { // 加载全部
            m_rowCountCache = m_dataHandler->getVectorTableRowCount(m_currentTableId);
        }
        else
        { // 分页加载
            int totalRows = m_dataHandler->getVectorTableRowCount(m_currentTableId);
            int startRow = page * pageSize;
            m_rowCountCache = qMin(pageSize, totalRows - startRow);
            if (m_rowCountCache < 0)
                m_rowCountCache = 0;
        }
    }
    else
    {
        m_rowCountCache = 0;
        m_columnInfoCache.clear();
    }
    endResetModel();
}

int VectorTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_rowCountCache;
}

int VectorTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_columnInfoCache.count();
}

QVariant VectorTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || m_currentTableId == -1)
        return QVariant();

    if (role == Qt::DisplayRole || role == Qt::EditRole)
    {
        static QTableWidget tempTable;
        tempTable.setRowCount(1);
        tempTable.setColumnCount(m_columnInfoCache.count());

        static bool isInitialized = false;
        if (!isInitialized)
        {
            tempTable.hide();
            isInitialized = true;
        }

        return QString("Row %1, Col %2").arg(index.row()).arg(index.column());
    }

    return QVariant();
}

QVariant VectorTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal)
    {
        if (section >= 0 && section < m_columnInfoCache.count())
        {
            return m_columnInfoCache.at(section).name;
        }
    }
    else if (orientation == Qt::Vertical)
    {
        return section + 1;
    }

    return QVariant();
}

bool VectorTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || role != Qt::EditRole || m_currentTableId == -1)
        return false;

    bool success = true;

    if (success)
    {
        emit dataChanged(index, index, {role});
        return true;
    }

    return false;
}

Qt::ItemFlags VectorTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
}