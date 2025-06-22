#include "vectortablemodel.h"
#include "vector/vectordatahandler.h" // 包含依赖
#include <QColor>  // 添加颜色支持

VectorTableModel::VectorTableModel(QObject *parent) : QAbstractTableModel(parent) {}
VectorTableModel::~VectorTableModel() {}

void VectorTableModel::loadTable(int tableId, VectorDataHandler* dataHandler) {
    beginResetModel();
    m_dataHandler = dataHandler;
    m_tableId = tableId;
    if (m_dataHandler) {
        m_rowCount = m_dataHandler->getVectorTableRowCount(m_tableId);
        m_columnInfoList = m_dataHandler->getAllColumnInfo(m_tableId);
        // 清空页面数据，因为将会按需加载
        m_pageData.clear();
        m_startRow = 0;
    } else {
        m_rowCount = 0;
        m_columnInfoList.clear();
        m_pageData.clear();
        m_startRow = 0;
    }
    // 清空修改记录
    m_modifiedRows.clear();
    endResetModel();
}

void VectorTableModel::clear() {
    beginResetModel();
    m_dataHandler = nullptr;
    m_tableId = -1;
    m_rowCount = 0;
    m_columnInfoList.clear();
    m_pageData.clear();
    m_startRow = 0;
    m_modifiedRows.clear();
    endResetModel();
}

void VectorTableModel::setColumnInfo(const QList<Vector::ColumnInfo>& columnInfo) {
    beginResetModel();
    m_columnInfoList = columnInfo;
    endResetModel();
}

void VectorTableModel::setTotalRowCount(int totalRows) {
    m_rowCount = totalRows;
}

void VectorTableModel::loadTable(const QList<QList<QVariant>>& data, int startRow) {
    beginResetModel();
    m_pageData = data;
    m_startRow = startRow;
    m_modifiedRows.clear();
    endResetModel();
}

int VectorTableModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    
    // 如果有页面数据，返回页面数据的行数，否则返回总行数
    if (!m_pageData.isEmpty()) {
        return m_pageData.size();
    }
    return m_rowCount;
}

int VectorTableModel::columnCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_columnInfoList.size();
}

QVariant VectorTableModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid()) {
        return QVariant();
    }
    
    // 显示和编辑角色
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        // 如果有页面数据，从页面数据获取
        if (!m_pageData.isEmpty()) {
            if (index.row() >= 0 && index.row() < m_pageData.size() &&
                index.column() >= 0 && index.column() < m_pageData[index.row()].size()) {
                return m_pageData[index.row()][index.column()];
            }
            return QVariant();
        }
        
        // 否则使用按需加载
        if (!m_dataHandler) {
            return QVariant();
        }
        
        // 使用新函数 getRowForModel
        QList<QVariant> rowData = m_dataHandler->getRowForModel(m_tableId, index.row());
        if (index.column() < rowData.size()) {
            return rowData.at(index.column());
        }
        return QVariant();
    }
    // 设置文本对齐方式
    else if (role == Qt::TextAlignmentRole) {
        return Qt::AlignCenter;
    }
    // 根据列类型设置背景色
    else if (role == Qt::BackgroundRole) {
        if (index.column() < m_columnInfoList.size()) {
            Vector::ColumnDataType colType = m_columnInfoList[index.column()].type;
            if (colType == Vector::ColumnDataType::TIMESET_ID) {
                return QColor(230, 230, 255); // 浅蓝色背景 for TimeSet
            } else if (colType == Vector::ColumnDataType::TEXT) {
                // 根据列名判断是LABEL还是COMMENT
                QString colName = m_columnInfoList[index.column()].name;
                if (colName.compare("LABEL", Qt::CaseInsensitive) == 0) {
                    return QColor(230, 255, 230); // 浅绿色背景 for Label
                } else if (colName.compare("COMMENT", Qt::CaseInsensitive) == 0) {
                    return QColor(255, 255, 230); // 浅黄色背景 for Comment
                }
            }
        }
    }
    
    return QVariant();
}

QVariant VectorTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Horizontal) {
            if (section >= 0 && section < m_columnInfoList.size()) {
                return m_columnInfoList.at(section).name;
            }
        } 
        else if (orientation == Qt::Vertical) {
            // 显示全局行号（实际行号 = 当前页起始行号 + 页内行索引）
            return m_startRow + section;
        }
    }
    return QVariant();
}

// 新增：数据编辑支持
bool VectorTableModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if (!index.isValid() || role != Qt::EditRole) {
        return false;
    }
    
    if (!m_pageData.isEmpty()) {
        if (index.row() >= 0 && index.row() < m_pageData.size() &&
            index.column() >= 0 && index.column() < m_columnInfoList.size()) {
            
            // 确保列索引在行数据范围内
            if (index.column() >= m_pageData[index.row()].size()) {
                // 如果需要，扩展行数据以适应列索引
                while (m_pageData[index.row()].size() <= index.column()) {
                    m_pageData[index.row()].append(QVariant());
                }
            }
            
            // 检查值是否实际发生变化
            if (m_pageData[index.row()][index.column()] == value) {
                return false;
            }
            
            // 更新数据
            m_pageData[index.row()][index.column()] = value;
            
            // 标记行为已修改
            markRowAsModified(index.row());
            
            // 通知视图数据已变更
            emit dataChanged(index, index, QVector<int>() << role);
            
            return true;
        }
    }
    return false;
}

Qt::ItemFlags VectorTableModel::flags(const QModelIndex &index) const {
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

// 标记行为已修改
void VectorTableModel::markRowAsModified(int row) {
    if (row >= 0 && row < m_pageData.size()) {
        m_modifiedRows.insert(row);
        emit rowModified(row);  // 发送行修改信号
    }
}