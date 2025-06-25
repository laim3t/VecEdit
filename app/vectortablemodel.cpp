#include "vectortablemodel.h"
#include "../vector/vectordatahandler.h"
#include "../database/binaryfilehelper.h"
#include "../database/databasemanager.h"
#include <QDebug>
#include <QSqlQuery>
#include <QTableWidget>
#include <QTableWidgetItem>

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
                if (timesetId > 0)
                {
                    QSqlQuery query(DatabaseManager::instance()->database());
                    query.prepare("SELECT name FROM TimeSet WHERE id = ?");
                    query.addBindValue(timesetId);
                    if (query.exec() && query.next())
                    {
                        return query.value(0).toString();
                    }
                }
                return QString("timeset_%1").arg(timesetId); // Fallback
            }
            case Vector::ColumnDataType::INSTRUCTION_ID:
            {
                int instructionId = cellData.toInt();
                // 假设 ID=1 是 INC，其他待定
                // 这里需要一个更完善的Instruction管理类，暂时硬编码
                if (instructionId == 1)
                    return "INC";
                return QString::number(instructionId); // Fallback
            }
            case Vector::ColumnDataType::PIN_STATE_ID:
            {
                // pin state 'X', '1', '0' etc. are stored as char
                return cellData.toString();
            }
            case Vector::ColumnDataType::BOOLEAN:
            {
                // Capture 列
                return cellData.toBool() ? "Y" : "N";
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

Qt::ItemFlags VectorTableModel::flags(const QModelIndex &index) const
{
    // 检查索引是否有效
    if (!index.isValid())
        return Qt::NoItemFlags;

    // 基本标志：可选择、可启用
    Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;

    // 检查列是否存在
    if (index.column() < m_columns.size())
    {
        const Vector::ColumnInfo &colInfo = m_columns.at(index.column());

        // 根据列类型判断是否可编辑
        // 某些特殊列可能需要禁止编辑，这里可以添加相应的条件
        // 例如：if (colInfo.type != Vector::ColumnDataType::SPECIAL_READONLY_TYPE)
        flags |= Qt::ItemIsEditable;
    }

    return flags;
}

bool VectorTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    // 检查索引是否有效
    if (!index.isValid() || index.row() >= m_pageData.size() || index.column() >= m_columns.size())
        return false;

    // 仅处理编辑角色
    if (role != Qt::EditRole)
        return false;

    // 获取当前行数据的可修改副本
    Vector::RowData &rowData = m_pageData[index.row()];

    // 获取列信息
    const Vector::ColumnInfo &colInfo = m_columns.at(index.column());

    // 确保行数据长度至少为列数
    while (rowData.size() <= index.column())
        rowData.append(QVariant());

    // 根据列类型处理数据转换
    QVariant convertedValue = value;
    switch (colInfo.type)
    {
    case Vector::ColumnDataType::INTEGER:
        convertedValue = value.toInt();
        break;
    case Vector::ColumnDataType::REAL:
        convertedValue = value.toDouble();
        break;
    case Vector::ColumnDataType::BOOLEAN:
        // 处理布尔值 - 可能是字符串 "True"/"False" 或 "Y"/"N"
        if (value.type() == QVariant::String)
        {
            QString strValue = value.toString().toUpper();
            convertedValue = (strValue == "TRUE" || strValue == "Y");
        }
        else
        {
            convertedValue = value.toBool();
        }
        break;
    case Vector::ColumnDataType::INSTRUCTION_ID:
    case Vector::ColumnDataType::TIMESET_ID:
        // 这些可能需要从显示文本转换为ID
        // 暂时保持原样，后续可以添加转换逻辑
        break;
    default:
        // 其他类型保持原样
        break;
    }

    // 检查值是否有变化
    if (rowData[index.column()] == convertedValue)
        return true; // 值未变，视为成功但不触发更新

    // 更新数据
    rowData[index.column()] = convertedValue;

    // 标记行为已修改
    int globalRowIndex = m_currentPage * m_pageSize + index.row();
    VectorDataHandler::instance().markRowAsModified(m_tableId, globalRowIndex);

    // 发出数据更改信号
    emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});

    qDebug() << "VectorTableModel::setData - 数据已更新，行:" << index.row()
             << "，全局行索引:" << globalRowIndex
             << "，列:" << index.column()
             << "，列名:" << colInfo.name
             << "，值:" << convertedValue;

    return true;
}

bool VectorTableModel::saveData(QString &errorMessage)
{
    qDebug() << "VectorTableModel::saveData - 开始保存表ID:" << m_tableId << "的修改数据";

    // 检查是否有任何修改
    if (!VectorDataHandler::instance().isRowModified(m_tableId, -1))
    {
        errorMessage = "没有检测到数据变更，跳过保存";
        qDebug() << "VectorTableModel::saveData - " << errorMessage;
        return true; // 没有修改，视为成功
    }

    // 使用VectorDataHandler保存修改的数据
    // 由于我们使用了Model/View架构，不再需要传递QTableWidget
    // 而是直接使用内部的m_pageData

    // 创建一个临时的QTableWidget来兼容VectorDataHandler的接口
    // 这是一个临时解决方案，后续应该修改VectorDataHandler以直接支持QList<Vector::RowData>
    QTableWidget tempTable;

    // 设置表格的行列数
    tempTable.setRowCount(m_pageData.size());
    tempTable.setColumnCount(m_columns.size());

    // 设置水平表头，确保列名与数据库匹配
    for (int col = 0; col < m_columns.size(); ++col)
    {
        QTableWidgetItem *headerItem = new QTableWidgetItem(m_columns[col].name);
        tempTable.setHorizontalHeaderItem(col, headerItem);
    }

    // 填充临时表格数据
    for (int row = 0; row < m_pageData.size(); ++row)
    {
        const Vector::RowData &rowData = m_pageData.at(row);
        for (int col = 0; col < m_columns.size(); ++col)
        {
            const Vector::ColumnInfo &colInfo = m_columns.at(col);
            QTableWidgetItem *item = new QTableWidgetItem();

            // 获取列数据，确保索引有效
            QVariant cellData;
            if (col < rowData.size())
            {
                cellData = rowData.at(col);
            }

            // 根据列类型设置数据
            switch (colInfo.type)
            {
            case Vector::ColumnDataType::BOOLEAN:
                item->setText(cellData.toBool() ? "Y" : "N");
                break;
            case Vector::ColumnDataType::PIN_STATE_ID:
                // 确保管脚状态值不为空，默认使用X
                {
                    QString pinState = cellData.toString();
                    if (pinState.isEmpty())
                    {
                        pinState = "X";
                    }
                    item->setText(pinState);
                }
                break;
            default:
                item->setText(cellData.toString());
                break;
            }

            tempTable.setItem(row, col, item);
        }
    }

    // 调用VectorDataHandler保存数据
    bool success = VectorDataHandler::instance().saveVectorTableDataPaged(
        m_tableId, &tempTable, m_currentPage, m_pageSize, m_totalRows, errorMessage);

    if (success)
    {
        qDebug() << "VectorTableModel::saveData - 成功保存表ID:" << m_tableId << "的数据";
    }
    else
    {
        qWarning() << "VectorTableModel::saveData - 保存表ID:" << m_tableId << "的数据失败:" << errorMessage;
    }

    return success;
}