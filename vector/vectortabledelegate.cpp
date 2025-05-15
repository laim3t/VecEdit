#include "vectortabledelegate.h"
#include "pin/pinvalueedit.h"
#include "database/databasemanager.h"
#include <QComboBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <algorithm>

VectorTableItemDelegate::VectorTableItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
    // 清空缓存，确保每次创建代理时都重新从数据库获取选项
    refreshCache();
}

// 刷新缓存方法实现
void VectorTableItemDelegate::refreshCache()
{
    m_instructionOptions.clear();
    m_timeSetOptions.clear();
    m_captureOptions.clear();

    qDebug() << "VectorTableItemDelegate::refreshCache - 缓存已清空，下次使用将重新从数据库加载";
}

VectorTableItemDelegate::~VectorTableItemDelegate()
{
    // 析构函数
}

QWidget *VectorTableItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    int column = index.column();

    // 获取当前列的类型信息
    int tableId = getTableIdForCurrentTable();
    Vector::ColumnInfo colInfo = getColumnInfoByIndex(tableId, column);

    // 根据列类型创建合适的编辑器
    if (colInfo.type == Vector::ColumnDataType::INSTRUCTION_ID)
    {
        // 创建指令下拉框
        QComboBox *comboBox = new QComboBox(parent);
        comboBox->addItems(getInstructionOptions());
        comboBox->setEditable(false);
        return comboBox;
    }
    else if (colInfo.type == Vector::ColumnDataType::TIMESET_ID)
    {
        // 创建TimeSet下拉框
        QComboBox *comboBox = new QComboBox(parent);
        comboBox->addItems(getTimeSetOptions());
        comboBox->setEditable(false);
        return comboBox;
    }
    else if (colInfo.type == Vector::ColumnDataType::BOOLEAN)
    {
        // 创建Capture下拉框（Y/N）
        QComboBox *comboBox = new QComboBox(parent);
        comboBox->addItems(getCaptureOptions());
        comboBox->setEditable(false);
        return comboBox;
    }
    else if (colInfo.type == Vector::ColumnDataType::PIN_STATE_ID)
    {
        // 创建管脚输入框
        PinValueLineEdit *editor = new PinValueLineEdit(parent);
        return editor;
    }
    else
    {
        // 默认使用基类创建
        return QStyledItemDelegate::createEditor(parent, option, index);
    }
}

void VectorTableItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    int column = index.column();

    // 获取当前列的类型信息
    int tableId = getTableIdForCurrentTable();
    Vector::ColumnInfo colInfo = getColumnInfoByIndex(tableId, column);

    if (colInfo.type == Vector::ColumnDataType::INSTRUCTION_ID ||
        colInfo.type == Vector::ColumnDataType::TIMESET_ID ||
        colInfo.type == Vector::ColumnDataType::BOOLEAN)
    {
        // 设置下拉框的当前值
        QComboBox *comboBox = static_cast<QComboBox *>(editor);
        QString value = index.model()->data(index, Qt::EditRole).toString();
        int idx = comboBox->findText(value);
        if (idx >= 0)
        {
            comboBox->setCurrentIndex(idx);
        }
    }
    else if (colInfo.type == Vector::ColumnDataType::PIN_STATE_ID)
    {
        // 设置管脚输入框的值
        PinValueLineEdit *lineEdit = static_cast<PinValueLineEdit *>(editor);
        QString value = index.model()->data(index, Qt::EditRole).toString();
        lineEdit->setText(value);
    }
    else
    {
        // 默认使用基类设置
        QStyledItemDelegate::setEditorData(editor, index);
    }
}

void VectorTableItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    int column = index.column();

    // 从UI索引获取内部列类型
    int tableId = getTableIdForCurrentTable();
    Vector::ColumnInfo colInfo = getColumnInfoByIndex(tableId, column);

    // 处理不同类型的列
    if (colInfo.type == Vector::ColumnDataType::INSTRUCTION_ID)
    {
        QComboBox *comboBox = static_cast<QComboBox *>(editor);
        QString value = comboBox->currentText();
        model->setData(index, value, Qt::EditRole);
        qDebug() << "VectorTableDelegate::setModelData - 设置指令列值:" << value;
    }
    else if (colInfo.type == Vector::ColumnDataType::TIMESET_ID)
    {
        QComboBox *comboBox = static_cast<QComboBox *>(editor);
        QString value = comboBox->currentText();
        model->setData(index, value, Qt::EditRole);
        qDebug() << "VectorTableDelegate::setModelData - 设置TimeSet列值:" << value;
    }
    else if (colInfo.type == Vector::ColumnDataType::BOOLEAN)
    {
        QComboBox *comboBox = static_cast<QComboBox *>(editor);
        QString value = comboBox->currentText();
        model->setData(index, value, Qt::EditRole);
        qDebug() << "VectorTableDelegate::setModelData - 设置布尔列值:" << value;
    }
    else if (colInfo.type == Vector::ColumnDataType::PIN_STATE_ID)
    {
        PinValueLineEdit *lineEdit = static_cast<PinValueLineEdit *>(editor);
        QString value = lineEdit->text();
        // 如果为空，默认使用X
        if (value.isEmpty())
        {
            value = "X";
        }
        model->setData(index, value, Qt::EditRole);
        qDebug() << "VectorTableDelegate::setModelData - 设置管脚列值:" << value;
    }
    else
    {
        // 处理TEXT、INTEGER和其他类型
        QStyledItemDelegate::setModelData(editor, model, index);
        qDebug() << "VectorTableDelegate::setModelData - 使用默认委托设置列值";
    }
}

// 辅助函数：获取当前表的ID
int VectorTableItemDelegate::getTableIdForCurrentTable() const
{
    QComboBox *vectorTableSelector = qobject_cast<QComboBox *>(getVectorTableSelectorPtr());
    if (!vectorTableSelector)
        return -1;

    int currentIndex = vectorTableSelector->currentIndex();
    if (currentIndex < 0)
        return -1;

    return vectorTableSelector->itemData(currentIndex).toInt();
}

// 辅助函数：通过UI索引获取列信息
Vector::ColumnInfo VectorTableItemDelegate::getColumnInfoByIndex(int tableId, int uiColumnIndex) const
{
    Vector::ColumnInfo defaultInfo;
    if (tableId < 0 || uiColumnIndex < 0)
        return defaultInfo;

    // 查询列信息
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery query(db);
    query.prepare("SELECT id, column_name, column_order, column_type, data_properties "
                  "FROM VectorTableColumnConfiguration "
                  "WHERE master_record_id = ? ORDER BY column_order");
    query.addBindValue(tableId);

    if (!query.exec())
    {
        qWarning() << "VectorTableDelegate::getColumnInfoByIndex - 查询列信息失败:" << query.lastError().text();
        return defaultInfo;
    }

    QList<Vector::ColumnInfo> columns;
    while (query.next())
    {
        Vector::ColumnInfo col;
        col.id = query.value(0).toInt();
        col.vector_table_id = tableId;
        col.name = query.value(1).toString();
        col.order = query.value(2).toInt();
        col.original_type_str = query.value(3).toString();
        col.type = Vector::columnDataTypeFromString(col.original_type_str);

        QString propStr = query.value(4).toString();
        if (!propStr.isEmpty())
        {
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(propStr.toUtf8(), &err);
            if (err.error == QJsonParseError::NoError && doc.isObject())
            {
                col.data_properties = doc.object();
            }
        }

        columns.append(col);
    }

    // UI索引和列顺序不一定对应，需要根据column_order排序后找到对应的列
    std::sort(columns.begin(), columns.end(), [](const Vector::ColumnInfo &a, const Vector::ColumnInfo &b)
              { return a.order < b.order; });

    if (uiColumnIndex < columns.size())
    {
        return columns[uiColumnIndex];
    }

    return defaultInfo;
}

// 辅助函数：获取向量表选择器指针
QObject *VectorTableItemDelegate::getVectorTableSelectorPtr() const
{
    QMainWindow *mainWindow = qobject_cast<QMainWindow *>(QApplication::activeWindow());
    if (!mainWindow)
        return nullptr;

    return mainWindow->findChild<QComboBox *>("m_vectorTableSelector");
}

QStringList VectorTableItemDelegate::getInstructionOptions() const
{
    // 如果已缓存，则直接返回
    if (!m_instructionOptions.isEmpty())
    {
        return m_instructionOptions;
    }

    // 从数据库获取指令选项
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery query(db);
    query.prepare("SELECT instruction_value FROM instruction_options ORDER BY id");
    if (query.exec())
    {
        while (query.next())
        {
            m_instructionOptions << query.value(0).toString();
        }
    }
    else
    {
        qWarning() << "获取指令选项失败:" << query.lastError().text();
    }

    return m_instructionOptions;
}

QStringList VectorTableItemDelegate::getTimeSetOptions() const
{
    // 如果已缓存，则直接返回
    if (!m_timeSetOptions.isEmpty())
    {
        return m_timeSetOptions;
    }

    // 从数据库获取TimeSet选项
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery query(db);
    query.prepare("SELECT timeset_name FROM timeset_list ORDER BY id");
    if (query.exec())
    {
        while (query.next())
        {
            m_timeSetOptions << query.value(0).toString();
        }
    }
    else
    {
        qWarning() << "获取TimeSet选项失败:" << query.lastError().text();
    }

    return m_timeSetOptions;
}

QStringList VectorTableItemDelegate::getCaptureOptions() const
{
    // 如果已缓存，则直接返回
    if (!m_captureOptions.isEmpty())
    {
        return m_captureOptions;
    }

    // Capture列固定只有Y和N选项
    m_captureOptions << "Y"
                     << "N";
    return m_captureOptions;
}