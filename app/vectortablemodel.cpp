#include "vectortablemodel.h"
#include "../vector/vectordatahandler.h"
#include "../vector/robustvectordatahandler.h" // 添加新数据处理器头文件
#include "../database/binaryfilehelper.h"
#include "../database/databasemanager.h"
#include <QDebug>
#include <QSqlQuery>
#include <QTableWidget>
#include <QTableWidgetItem>

VectorTableModel::VectorTableModel(QObject *parent, bool useNewDataHandler, RobustVectorDataHandler *robustDataHandler)
    : QAbstractTableModel(parent),
      m_tableId(0),
      m_currentPage(0),
      m_pageSize(100), // 默认每页100行
      m_totalRows(0),
      m_cachesInitialized(false),
      m_useNewDataHandler(useNewDataHandler),
      m_robustDataHandler(robustDataHandler)
{
    // 初始化代码
}

VectorTableModel::~VectorTableModel()
{
    // 清理代码
}

void VectorTableModel::setUseNewDataHandler(bool useNew)
{
    m_useNewDataHandler = useNew;
    qDebug() << "VectorTableModel::setUseNewDataHandler - Switched to useNewDataHandler:" << useNew;
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
                return getTimeSetName(timesetId);
            }
            case Vector::ColumnDataType::INSTRUCTION_ID:
            {
                int instructionId = cellData.toInt();
                return getInstructionName(instructionId);
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

    // 刷新指令和TimeSet的缓存
    refreshCaches();

    // 告诉视图我们要开始修改数据了
    beginResetModel();

    // 获取列信息
    if (m_useNewDataHandler)
    {
        m_columns = m_robustDataHandler->getVisibleColumns(tableId);
    }
    else
    {
        m_columns = VectorDataHandler::instance().getVisibleColumns(tableId);
    }

    // 获取总行数
    if (m_useNewDataHandler)
    {
        m_totalRows = m_robustDataHandler->getVectorTableRowCount(tableId);
    }
    else
    {
        m_totalRows = VectorDataHandler::instance().getVectorTableRowCount(tableId);
    }

    // 获取当前页数据
    if (m_useNewDataHandler)
    {
        m_pageData = m_robustDataHandler->getPageData(tableId, page, m_pageSize);
    }
    else
    {
        m_pageData = VectorDataHandler::instance().getPageData(tableId, page, m_pageSize);
    }

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
        // 将指令名称转换为ID
        convertedValue = getInstructionId(value.toString());
        break;
    case Vector::ColumnDataType::TIMESET_ID:
        // 将TimeSet名称转换为ID
        convertedValue = getTimeSetId(value.toString());
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

    // 标记为已修改
    int globalRowIndex = m_currentPage * m_pageSize + index.row();
    if (m_useNewDataHandler)
    {
        m_robustDataHandler->markRowAsModified(m_tableId, globalRowIndex);
    }
    else
    {
        VectorDataHandler::instance().markRowAsModified(m_tableId, globalRowIndex);
    }

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
    bool hasModifications;
    if (m_useNewDataHandler)
    {
        hasModifications = m_robustDataHandler->isRowModified(m_tableId, -1);
    }
    else
    {
        hasModifications = VectorDataHandler::instance().isRowModified(m_tableId, -1);
    }

    if (!hasModifications)
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
            case Vector::ColumnDataType::INSTRUCTION_ID:
                // 将指令ID转换为名称
                item->setText(getInstructionName(cellData.toInt()));
                break;
            case Vector::ColumnDataType::TIMESET_ID:
                // 将TimeSet ID转换为名称
                item->setText(getTimeSetName(cellData.toInt()));
                break;
            default:
                item->setText(cellData.toString());
                break;
            }

            tempTable.setItem(row, col, item);
        }
    }

    // 调用VectorDataHandler保存数据
    bool success;
    if (m_useNewDataHandler)
    {
        success = m_robustDataHandler->saveVectorTableDataPaged(
            m_tableId, &tempTable, m_currentPage, m_pageSize, m_totalRows, errorMessage);
    }
    else
    {
        success = VectorDataHandler::instance().saveVectorTableDataPaged(
            m_tableId, &tempTable, m_currentPage, m_pageSize, m_totalRows, errorMessage);
    }

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

// 刷新指令和TimeSet的缓存
void VectorTableModel::refreshCaches() const
{
    // 清空现有缓存
    m_instructionCache.clear();
    m_instructionNameToIdCache.clear();
    m_timeSetCache.clear();
    m_timeSetNameToIdCache.clear();

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << "VectorTableModel::refreshCaches - 数据库未打开";
        return;
    }

    // 加载指令缓存
    QSqlQuery instructionQuery(db);
    instructionQuery.prepare("SELECT id, instruction_value FROM instruction_options ORDER BY id");
    if (instructionQuery.exec())
    {
        while (instructionQuery.next())
        {
            int id = instructionQuery.value(0).toInt();
            QString name = instructionQuery.value(1).toString();
            m_instructionCache[id] = name;
            m_instructionNameToIdCache[name] = id;
        }
    }
    else
    {
        qWarning() << "VectorTableModel::refreshCaches - 加载指令缓存失败:" << instructionQuery.lastError().text();
    }

    // 加载TimeSet缓存
    QSqlQuery timeSetQuery(db);
    timeSetQuery.prepare("SELECT id, timeset_name FROM timeset_list ORDER BY id");
    if (timeSetQuery.exec())
    {
        while (timeSetQuery.next())
        {
            int id = timeSetQuery.value(0).toInt();
            QString name = timeSetQuery.value(1).toString();
            m_timeSetCache[id] = name;
            m_timeSetNameToIdCache[name] = id;
        }
    }
    else
    {
        qWarning() << "VectorTableModel::refreshCaches - 加载TimeSet缓存失败:" << timeSetQuery.lastError().text();
    }

    m_cachesInitialized = true;
}

// 获取指令名称（根据ID）
QString VectorTableModel::getInstructionName(int instructionId) const
{
    // 如果缓存未初始化，则初始化缓存
    if (!m_cachesInitialized)
    {
        refreshCaches();
    }

    // 检查缓存中是否有该ID
    if (m_instructionCache.contains(instructionId))
    {
        return m_instructionCache[instructionId];
    }

    // 如果缓存中没有，尝试从数据库获取
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery query(db);
    query.prepare("SELECT instruction_value FROM instruction_options WHERE id = ?");
    query.addBindValue(instructionId);

    if (query.exec() && query.next())
    {
        QString name = query.value(0).toString();
        // 更新缓存
        m_instructionCache[instructionId] = name;
        m_instructionNameToIdCache[name] = instructionId;
        return name;
    }

    // 如果找不到对应的名称，返回ID的字符串形式
    return QString::number(instructionId);
}

// 获取TimeSet名称（根据ID）
QString VectorTableModel::getTimeSetName(int timeSetId) const
{
    // 如果缓存未初始化，则初始化缓存
    if (!m_cachesInitialized)
    {
        refreshCaches();
    }

    // 检查缓存中是否有该ID
    if (m_timeSetCache.contains(timeSetId))
    {
        return m_timeSetCache[timeSetId];
    }

    // 如果缓存中没有，尝试从数据库获取
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery query(db);
    query.prepare("SELECT timeset_name FROM timeset_list WHERE id = ?");
    query.addBindValue(timeSetId);

    if (query.exec() && query.next())
    {
        QString name = query.value(0).toString();
        // 更新缓存
        m_timeSetCache[timeSetId] = name;
        m_timeSetNameToIdCache[name] = timeSetId;
        return name;
    }

    // 如果找不到对应的名称，返回格式化的字符串
    return QString("timeset_%1").arg(timeSetId);
}

// 获取指令ID（根据名称）
int VectorTableModel::getInstructionId(const QString &instructionName) const
{
    // 如果缓存未初始化，则初始化缓存
    if (!m_cachesInitialized)
    {
        refreshCaches();
    }

    // 检查缓存中是否有该名称
    if (m_instructionNameToIdCache.contains(instructionName))
    {
        return m_instructionNameToIdCache[instructionName];
    }

    // 如果缓存中没有，尝试从数据库获取
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery query(db);
    query.prepare("SELECT id FROM instruction_options WHERE instruction_value = ?");
    query.addBindValue(instructionName);

    if (query.exec() && query.next())
    {
        int id = query.value(0).toInt();
        // 更新缓存
        m_instructionCache[id] = instructionName;
        m_instructionNameToIdCache[instructionName] = id;
        return id;
    }

    // 如果找不到对应的ID，返回-1表示无效
    return -1;
}

// 获取TimeSet ID（根据名称）
int VectorTableModel::getTimeSetId(const QString &timeSetName) const
{
    // 如果缓存未初始化，则初始化缓存
    if (!m_cachesInitialized)
    {
        refreshCaches();
    }

    // 检查缓存中是否有该名称
    if (m_timeSetNameToIdCache.contains(timeSetName))
    {
        return m_timeSetNameToIdCache[timeSetName];
    }

    // 如果缓存中没有，尝试从数据库获取
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery query(db);
    query.prepare("SELECT id FROM timeset_list WHERE timeset_name = ?");
    query.addBindValue(timeSetName);

    if (query.exec() && query.next())
    {
        int id = query.value(0).toInt();
        // 更新缓存
        m_timeSetCache[id] = timeSetName;
        m_timeSetNameToIdCache[timeSetName] = id;
        return id;
    }

    // 如果找不到对应的ID，返回-1表示无效
    return -1;
}

// 实现表格行操作方法

bool VectorTableModel::insertRows(int row, int count, const QModelIndex &parent)
{
    if (parent.isValid() || count <= 0)
        return false;

    if (!m_robustDataHandler)
    {
        qWarning() << "VectorTableModel::insertRows - RobustVectorDataHandler is null.";
        return false;
    }

    // 1. 获取当前表的列定义，以构建新行的数据结构
    QList<Vector::ColumnInfo> columns = m_robustDataHandler->getVisibleColumns(m_tableId);
    if (columns.isEmpty())
    {
        qWarning() << "VectorTableModel::insertRows - Failed to get column info for table" << m_tableId;
        return false;
    }

    // 2. 根据列定义创建'count'个带有默认值的新行
    QList<Vector::RowData> rowsToInsert;
    for (int i = 0; i < count; ++i)
    {
        Vector::RowData newRow;
        for (const auto &colInfo : columns)
        {
            // 根据列类型设置合理的默认值
            switch (colInfo.type)
            {
            case Vector::ColumnDataType::TEXT:
                // 例如，Label 和 Comment 列可以为空
                if (colInfo.name.compare("Label", Qt::CaseInsensitive) == 0)
                {
                    // 不再为Label列生成默认值，使用空字符串
                    newRow.append(QString(""));
                }
                else
                {
                    newRow.append(QString(""));
                }
                break;
            case Vector::ColumnDataType::INSTRUCTION_ID:
                newRow.append(1); // 默认指令ID
                break;
            case Vector::ColumnDataType::TIMESET_ID:
                newRow.append(1); // 默认TimeSet ID
                break;
            case Vector::ColumnDataType::BOOLEAN: // Capture
                newRow.append(false);
                break;
            case Vector::ColumnDataType::PIN_STATE_ID:
                newRow.append("X"); // 默认管脚状态
                break;
            default:
                newRow.append(QVariant()); // 其他类型使用空的QVariant
                break;
            }
        }
        rowsToInsert.append(newRow);
    }

    beginInsertRows(parent, row, row + count - 1);

    // 3. 调用新的、正确的insertVectorRows接口
    QString errorMessage;
    bool success = m_robustDataHandler->insertVectorRows(m_tableId, row, rowsToInsert, errorMessage);

    if (success)
    {
        // 4. 如果插入成功，重新加载当前页以显示新数据
        loadPage(m_tableId, m_currentPage);
    }
    else
    {
        qWarning() << "VectorTableModel::insertRows failed:" << errorMessage;
    }

    endInsertRows();
    return success;
}

bool VectorTableModel::removeRows(int row, int count, const QModelIndex &parent)
{
    // 检查参数是否有效
    if (parent.isValid() || row < 0 || count <= 0 || (row + count) > m_pageData.size() || m_tableId <= 0)
        return false;

    const QString funcName = "VectorTableModel::removeRows";
    qDebug() << funcName << "- 开始从位置" << row << "删除" << count << "行";

    // 获取要删除的行的实际索引（考虑分页）
    QList<int> rowIndexes;
    for (int i = 0; i < count; i++)
    {
        rowIndexes.append(row + i + (m_currentPage * m_pageSize));
    }

    // 调用VectorDataHandler删除行
    QString errorMessage;
    bool deleteSuccess;
    if (m_useNewDataHandler)
    {
        deleteSuccess = m_robustDataHandler->deleteVectorRows(m_tableId, rowIndexes, errorMessage);
    }
    else
    {
        deleteSuccess = VectorDataHandler::instance().deleteVectorRows(m_tableId, rowIndexes, errorMessage);
    }
    if (!deleteSuccess)
    {
        qWarning() << funcName << "- 删除行失败:" << errorMessage;
        return false;
    }

    // 开始移除行
    beginRemoveRows(parent, row, row + count - 1);

    // 从模型数据中移除行
    for (int i = 0; i < count; i++)
    {
        if (row < m_pageData.size())
        {
            m_pageData.removeAt(row);
        }
    }

    // 更新总行数
    m_totalRows -= count;

    // 结束移除行
    endRemoveRows();

    qDebug() << funcName << "- 已成功删除" << count << "行";
    return true;
}

bool VectorTableModel::deleteSelectedRows(const QList<int> &rowIndexes, QString &errorMessage)
{
    const QString funcName = "VectorTableModel::deleteSelectedRows";
    qDebug() << funcName << "- 开始删除选中的行，共" << rowIndexes.size() << "行";

    if (rowIndexes.isEmpty() || m_tableId <= 0)
    {
        errorMessage = "没有选中任何行或表ID无效";
        return false;
    }

    // 获取绝对行索引（考虑分页）
    QList<int> absoluteRowIndexes;
    for (int rowIndex : rowIndexes)
    {
        // 计算绝对行索引
        int absoluteRowIndex = rowIndex;
        absoluteRowIndexes.append(absoluteRowIndex);
    }

    // 调用VectorDataHandler删除行
    bool deleteSuccess;
    if (m_useNewDataHandler)
    {
        deleteSuccess = m_robustDataHandler->deleteVectorRows(m_tableId, absoluteRowIndexes, errorMessage);
    }
    else
    {
        deleteSuccess = VectorDataHandler::instance().deleteVectorRows(m_tableId, absoluteRowIndexes, errorMessage);
    }
    if (!deleteSuccess)
    {
        qWarning() << funcName << "- 删除行失败:" << errorMessage;
        return false;
    }

    // 重新加载当前页
    loadPage(m_tableId, m_currentPage);

    qDebug() << funcName << "- 已成功删除选中的行";
    return true;
}

bool VectorTableModel::deleteRowsInRange(int fromRow, int toRow, QString &errorMessage)
{
    const QString funcName = "VectorTableModel::deleteRowsInRange";
    qDebug() << funcName << "- 开始删除从第" << fromRow << "行到第" << toRow << "行";

    if (fromRow <= 0 || toRow <= 0 || fromRow > toRow || m_tableId <= 0)
    {
        errorMessage = "无效的行范围或表ID";
        return false;
    }

    // 调用VectorDataHandler删除行范围
    bool deleteSuccess;
    if (m_useNewDataHandler)
    {
        deleteSuccess = m_robustDataHandler->deleteVectorRowsInRange(m_tableId, fromRow, toRow, errorMessage);
    }
    else
    {
        deleteSuccess = VectorDataHandler::instance().deleteVectorRowsInRange(m_tableId, fromRow, toRow, errorMessage);
    }
    if (!deleteSuccess)
    {
        qWarning() << funcName << "- 删除行范围失败:" << errorMessage;
        return false;
    }

    // 重新加载当前页
    loadPage(m_tableId, m_currentPage);

    qDebug() << funcName << "- 已成功删除行范围";
    return true;
}

bool VectorTableModel::addNewRow(int timesetId, const QMap<int, QString> &pinValues, QString &errorMessage)
{
    // 此方法已废弃，所有行添加操作应通过insertRows进行。
    // 保留此空函数体是为了防止在完全移除前，某些旧的调用链路导致编译失败。
    qWarning() << "VectorTableModel::addNewRow is deprecated and should not be used. Use insertRows instead.";
    errorMessage = "This function is deprecated.";
    return false;
}