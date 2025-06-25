#include "vectortabledelegate.h"
#include "database/databasemanager.h"
#include "pin/pinvalueedit.h"
#include <QComboBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <algorithm>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QIntValidator>
#include <QDoubleValidator>
#include "common/binary_field_lengths.h"
#include <QApplication>
#include <QTableView>
#include <QHeaderView>
#include <QPainter>
#include <QTableWidget>
#include <QSqlDatabase>
#include <QCoreApplication>
#include <QMainWindow>

// 定义静态成员变量
QMap<int, QList<Vector::ColumnInfo>> VectorTableItemDelegate::s_tableColumnsCache;

VectorTableItemDelegate::VectorTableItemDelegate(QObject *parent, const QVector<int> &cellEditTypes)
    : QStyledItemDelegate(parent),
      m_tableIdCached(false),
      m_cachedTableId(-1),
      m_cellTypes(cellEditTypes)
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

    // 清空静态列配置缓存
    s_tableColumnsCache.clear();

    // 同时刷新表ID缓存
    refreshTableIdCache();

    qDebug() << "VectorTableItemDelegate::refreshCache - 缓存已清空，下次使用将重新从数据库加载";
}

// 刷新表ID缓存方法实现
void VectorTableItemDelegate::refreshTableIdCache()
{
    m_tableIdCached = false;
    m_cachedTableId = -1;
    qDebug() << "VectorTableItemDelegate::refreshTableIdCache - 表ID缓存已重置";
}

VectorTableItemDelegate::~VectorTableItemDelegate()
{
    // 析构函数
}

// 自定义绘制实现
void VectorTableItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // 获取列信息
    Vector::ColumnInfo colInfo = getColumnInfoFromModel(index.model(), index.column());

    // 绘制背景
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    // 获取数据
    QString displayText = getDisplayText(index);

    // 根据列类型绘制不同的样式
    switch (colInfo.type)
    {
    case Vector::ColumnDataType::PIN_STATE_ID:
        // 绘制管脚状态（例如不同颜色的背景）
        {
            QStyle *style = QApplication::style();
            style->drawControl(QStyle::CE_ItemViewItem, &opt, painter);

            // 居中绘制文本
            QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt);
            painter->save();
            painter->setClipRect(textRect);

            // 根据管脚状态设置颜色
            QString value = displayText.trimmed();
            QColor textColor = Qt::black;
            QColor bgColor = Qt::white;

            if (value == "1")
            {
                bgColor = QColor(200, 255, 200); // 浅绿色表示1
                textColor = Qt::darkGreen;
            }
            else if (value == "0")
            {
                bgColor = QColor(255, 230, 230); // 浅红色表示0
                textColor = Qt::darkRed;
            }
            else if (value == "X" || value == "Z" || value == "L" || value == "H")
            {
                bgColor = QColor(230, 230, 255); // 浅蓝色表示其他特殊状态
                textColor = Qt::darkBlue;
            }

            // 绘制背景和文本
            painter->fillRect(textRect, bgColor);
            painter->setPen(textColor);
            painter->drawText(textRect, Qt::AlignCenter, value);
            painter->restore();
        }
        break;

    default:
        // 默认绘制
        QStyledItemDelegate::paint(painter, option, index);
        break;
    }
}

QWidget *VectorTableItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option);

    // 获取列信息
    Vector::ColumnInfo colInfo = getColumnInfoFromModel(index.model(), index.column());

    // 根据列类型创建不同的编辑器
    switch (colInfo.type)
    {
    case Vector::ColumnDataType::TEXT:
        return new QLineEdit(parent);

    case Vector::ColumnDataType::PIN_STATE_ID:
    {
        // 创建管脚状态下拉框
        QComboBox *comboBox = new QComboBox(parent);
        comboBox->addItems(QStringList() << "0" << "1" << "X" << "Z" << "L" << "H");
        return comboBox;
    }

    case Vector::ColumnDataType::TIMESET_ID:
    {
        // 创建TimeSet下拉框
        QComboBox *comboBox = new QComboBox(parent);
        comboBox->addItems(getTimeSetOptions());
        return comboBox;
    }

    case Vector::ColumnDataType::INSTRUCTION_ID:
    {
        // 创建指令下拉框
        QComboBox *comboBox = new QComboBox(parent);
        comboBox->addItems(getInstructionOptions());
        return comboBox;
    }

    case Vector::ColumnDataType::BOOLEAN:
    {
        // 创建布尔值下拉框
        QComboBox *comboBox = new QComboBox(parent);
        comboBox->addItems(getCaptureOptions());
        return comboBox;
    }

    default:
        return new QLineEdit(parent);
    }
}

void VectorTableItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    // 获取列信息
    Vector::ColumnInfo colInfo = getColumnInfoFromModel(index.model(), index.column());

    // 获取模型数据
    QVariant value = index.data(Qt::EditRole);

    // 根据编辑器类型设置数据
    switch (colInfo.type)
    {
    case Vector::ColumnDataType::PIN_STATE_ID:
    case Vector::ColumnDataType::TIMESET_ID:
    case Vector::ColumnDataType::INSTRUCTION_ID:
    case Vector::ColumnDataType::BOOLEAN:
    {
        QComboBox *comboBox = qobject_cast<QComboBox *>(editor);
        if (!comboBox)
            return;

        QString text = index.data(Qt::DisplayRole).toString();
        int idx = comboBox->findText(text);
        if (idx >= 0)
            comboBox->setCurrentIndex(idx);
        else if (comboBox->count() > 0)
            comboBox->setCurrentIndex(0);
        break;
    }

    default:
    {
        QLineEdit *lineEdit = qobject_cast<QLineEdit *>(editor);
        if (!lineEdit)
            return;
        lineEdit->setText(value.toString());
        break;
    }
    }
}

void VectorTableItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    // 获取列信息
    Vector::ColumnInfo colInfo = getColumnInfoFromModel(model, index.column());

    // 根据编辑器类型获取数据
    switch (colInfo.type)
    {
    case Vector::ColumnDataType::PIN_STATE_ID:
    case Vector::ColumnDataType::TIMESET_ID:
    case Vector::ColumnDataType::INSTRUCTION_ID:
    case Vector::ColumnDataType::BOOLEAN:
    {
        QComboBox *comboBox = qobject_cast<QComboBox *>(editor);
        if (!comboBox)
            return;

        // 处理特殊情况：如果是TimeSet ID或指令ID，需要转换为ID
        if (colInfo.type == Vector::ColumnDataType::TIMESET_ID)
        {
            // 提取ID，假设格式为"TS123"
            QString text = comboBox->currentText();
            if (text.startsWith("TS"))
            {
                bool ok;
                int id = text.mid(2).toInt(&ok);
                if (ok)
                    model->setData(index, id, Qt::EditRole);
                else
                    model->setData(index, text, Qt::EditRole);
            }
            else
            {
                model->setData(index, text, Qt::EditRole);
            }
        }
        else if (colInfo.type == Vector::ColumnDataType::INSTRUCTION_ID)
        {
            // 提取ID，假设格式为"Instr123"
            QString text = comboBox->currentText();
            if (text.startsWith("Instr"))
            {
                bool ok;
                int id = text.mid(5).toInt(&ok);
                if (ok)
                    model->setData(index, id, Qt::EditRole);
                else
                    model->setData(index, text, Qt::EditRole);
            }
            else
            {
                model->setData(index, text, Qt::EditRole);
            }
        }
        else
        {
            // 其他类型直接设置文本值
            model->setData(index, comboBox->currentText(), Qt::EditRole);
        }
        break;
    }

    default:
    {
        QLineEdit *lineEdit = qobject_cast<QLineEdit *>(editor);
        if (!lineEdit)
            return;
        model->setData(index, lineEdit->text(), Qt::EditRole);
        break;
    }
    }
}

void VectorTableItemDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(index);
    editor->setGeometry(option.rect);
}

// 辅助函数：获取当前表的ID
int VectorTableItemDelegate::getTableIdForCurrentTable() const
{
    if (m_tableIdCached)
        return m_cachedTableId;

    QObject *selectorPtr = getVectorTableSelectorPtr();
    if (selectorPtr)
    {
        // 使用元对象系统获取当前数据
        QVariant currentData = selectorPtr->property("currentData");
        if (currentData.isValid())
        {
            m_cachedTableId = currentData.toInt();
            m_tableIdCached = true;
            return m_cachedTableId;
        }
    }

    return -1;
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
    // 查找MainWindow对象
    QList<QWidget *> topLevelWidgets = QApplication::topLevelWidgets();
    QMainWindow *mainWindow = nullptr;

    // 遍历顶层窗口查找QMainWindow实例
    foreach (QWidget *widget, topLevelWidgets)
    {
        mainWindow = qobject_cast<QMainWindow *>(widget);
        if (mainWindow)
            break;
    }

    if (!mainWindow)
        return nullptr;

    // 查找名为"m_vectorTableSelector"的QComboBox
    QComboBox *selector = mainWindow->findChild<QComboBox *>("m_vectorTableSelector");
    return selector;
}

// 获取列配置（直接从数据库获取，供缓存使用）
QList<Vector::ColumnInfo> VectorTableItemDelegate::getColumnConfiguration(int tableId) const
{
    if (tableId < 0)
        return QList<Vector::ColumnInfo>();

    // 检查缓存是否已存在
    if (s_tableColumnsCache.contains(tableId))
        return s_tableColumnsCache[tableId];

    QList<Vector::ColumnInfo> columns;
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
        return columns;

    QSqlQuery query(db);
    query.prepare("SELECT id, column_name, column_order, column_type, data_properties, IsVisible FROM VectorTableColumnConfiguration WHERE master_record_id = ? AND IsVisible = 1 ORDER BY column_order");
    query.addBindValue(tableId);

    if (query.exec())
    {
        while (query.next())
        {
            Vector::ColumnInfo col;
            col.id = query.value(0).toInt();
            col.vector_table_id = tableId;
            col.name = query.value(1).toString();
            col.order = query.value(2).toInt();
            col.original_type_str = query.value(3).toString();
            col.type = Vector::columnDataTypeFromString(col.original_type_str);
            col.is_visible = query.value(5).toBool();

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
    }

    // 添加到缓存
    s_tableColumnsCache[tableId] = columns;
    return columns;
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

Vector::ColumnInfo VectorTableItemDelegate::getColumnInfoFromModel(const QAbstractItemModel *model, int column) const
{
    // 尝试从VectorTableModel中获取列信息
    const VectorTableModel *vectorModel = qobject_cast<const VectorTableModel *>(model);
    if (vectorModel)
    {
        return vectorModel->getColumnInfo(column);
    }

    // 回退到旧方法
    int tableId = getTableIdForCurrentTable();
    return getColumnInfoByIndex(tableId, column);
}

QString VectorTableItemDelegate::getDisplayText(const QModelIndex &index) const
{
    // 获取列信息
    Vector::ColumnInfo colInfo = getColumnInfoFromModel(index.model(), index.column());

    // 获取原始数据
    QVariant data = index.data(Qt::DisplayRole);

    // 根据列类型格式化显示
    switch (colInfo.type)
    {
    case Vector::ColumnDataType::TIMESET_ID:
        // 如果是TimeSet ID，尝试查询对应的名称
        {
            int timesetId = data.toInt();
            // 暂时使用简单格式
            return QString("TS%1").arg(timesetId);
        }

    case Vector::ColumnDataType::INSTRUCTION_ID:
        // 如果是指令ID，尝试查询对应的名称
        {
            int instrId = data.toInt();
            // 暂时使用简单格式
            return QString("Instr%1").arg(instrId);
        }

    default:
        return data.toString();
    }
}

int VectorTableItemDelegate::getEditorType(Vector::ColumnDataType columnType) const
{
    switch (columnType)
    {
    case Vector::ColumnDataType::PIN_STATE_ID:
    case Vector::ColumnDataType::TIMESET_ID:
    case Vector::ColumnDataType::INSTRUCTION_ID:
    case Vector::ColumnDataType::BOOLEAN:
        return 1; // 下拉框

    default:
        return 0; // 文本框
    }
}