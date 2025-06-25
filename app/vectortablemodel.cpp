#include "vectortablemodel.h"
#include "../vector/vectordatahandler.h"
#include "../database/binaryfilehelper.h"
#include <QDebug>

VectorTableModel::VectorTableModel(QObject *parent)
    : QAbstractTableModel(parent),
      m_tableId(0),
      m_currentPage(0),
      m_pageSize(100), // 默认每页100行
      m_totalRows(0)
{
    // 初始化代码
}

VectorTableModel::~VectorTableModel()
{
    // 清理代码
}

int VectorTableModel::rowCount(const QModelIndex &parent) const
{
    // 父索引有效时意味着是树形结构中的子项，表格模型中不应该有子项
    if (parent.isValid())
        return 0;

    // 返回当前页数据的行数
    return m_pageData.size();
}

int VectorTableModel::columnCount(const QModelIndex &parent) const
{
    // 父索引有效时意味着是树形结构中的子项，表格模型中不应该有子项
    if (parent.isValid())
        return 0;

    // 返回列数
    return m_columns.size();
}

QVariant VectorTableModel::data(const QModelIndex &index, int role) const
{
    // 检查索引是否有效
    if (!index.isValid() || index.row() >= m_pageData.size() || index.column() >= m_columns.size())
        return QVariant();

    // 获取对应的行数据和列信息
    const Vector::RowData &rowData = m_pageData.at(index.row());
    const Vector::ColumnInfo &colInfo = m_columns.at(index.column());

    switch (role)
    {
    case Qt::DisplayRole: // 显示角色，返回要显示的文本
    case Qt::EditRole:    // 编辑角色，返回用于编辑的值
    {
        if (index.column() < rowData.size())
        {
            const QVariant &cellData = rowData.at(index.column());

            // 根据列类型格式化数据
            switch (colInfo.type)
            {
            case Vector::ColumnDataType::TIMESET_ID:
            {
                int timesetId = cellData.toInt();
                // 可以在这里返回TimeSet的名称，但目前先返回ID值
                return timesetId;
            }
            case Vector::ColumnDataType::INSTRUCTION_ID:
            {
                int instructionId = cellData.toInt();
                // 可以在这里返回指令的文本，但目前先返回ID值
                return instructionId;
            }
            default:
                // 其他类型直接返回存储的值
                return cellData;
            }
        }
        return QVariant();
    }

    case Qt::TextAlignmentRole:
    {
        // 根据列类型设置对齐方式
        switch (colInfo.type)
        {
        case Vector::ColumnDataType::TEXT:
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        case Vector::ColumnDataType::PIN_STATE_ID:
            return int(Qt::AlignCenter);
        default:
            return int(Qt::AlignRight | Qt::AlignVCenter);
        }
    }

        // 其他角色可以在这里处理，例如背景色、前景色等

    default:
        return QVariant();
    }
}

QVariant VectorTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    // 仅处理水平表头
    if (orientation == Qt::Horizontal)
    {
        // 显示角色，返回列标题
        if (role == Qt::DisplayRole && section >= 0 && section < m_columns.size())
        {
            const Vector::ColumnInfo &colInfo = m_columns.at(section);

            // 根据列类型设置表头
            if (colInfo.type == Vector::ColumnDataType::PIN_STATE_ID && !colInfo.data_properties.isEmpty())
            {
                // 获取管脚属性
                int channelCount = colInfo.data_properties["channel_count"].toInt(1);

                // 创建带有管脚信息的表头
                return QString("%1\nx%2").arg(colInfo.name).arg(channelCount);
            }
            else
            {
                // 标准列，直接使用列名
                return colInfo.name;
            }
        }
        else if (role == Qt::TextAlignmentRole)
        {
            // 表头居中对齐
            return int(Qt::AlignCenter);
        }
    }
    // 垂直表头：行号 (从1开始)
    else if (orientation == Qt::Vertical && role == Qt::DisplayRole)
    {
        return section + 1 + (m_currentPage * m_pageSize);
    }

    return QAbstractTableModel::headerData(section, orientation, role);
}

void VectorTableModel::loadPage(int tableId, int page)
{
    qDebug() << "VectorTableModel::loadPage - 加载表ID:" << tableId << "页码:" << page;

    // 保存新的表ID和页码
    m_tableId = tableId;
    m_currentPage = page;

    // 告诉视图我们要开始修改数据了
    beginResetModel();

    // 获取列信息
    m_columns = VectorDataHandler::instance().getVisibleColumns(tableId);

    // 获取总行数
    m_totalRows = VectorDataHandler::instance().getVectorTableRowCount(tableId);

    // 获取当前页数据
    m_pageData = VectorDataHandler::instance().getPageData(tableId, page, m_pageSize);

    // 告诉视图我们已经修改完数据
    endResetModel();

    qDebug() << "VectorTableModel::loadPage - 加载完成，列数:" << m_columns.size()
             << "，当前页行数:" << m_pageData.size() << "，总行数:" << m_totalRows;
}