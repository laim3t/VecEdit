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
#include <QPainter>
#include <QTimer>

// 定义静态成员变量
QMap<int, QList<Vector::ColumnInfo>> VectorTableDelegate::s_tableColumnsCache;

VectorTableDelegate::VectorTableDelegate(QObject *parent)
    : QStyledItemDelegate(parent),
      m_tableIdCached(false),
      m_cachedTableId(-1),
      m_cellTypes()
{
    // 清空缓存，确保每次创建代理时都重新从数据库获取选项
    refreshCache();
}

// 刷新缓存方法实现
void VectorTableDelegate::refreshCache()
{
    m_instructionOptions.clear();
    m_timeSetOptions.clear();
    m_captureOptions.clear();

    // 清空静态列配置缓存
    s_tableColumnsCache.clear();

    // 同时刷新表ID缓存
    refreshTableIdCache();

    qDebug() << "VectorTableDelegate::refreshCache - 缓存已清空，下次使用将重新从数据库加载";
}

// 刷新表ID缓存方法实现
void VectorTableDelegate::refreshTableIdCache()
{
    m_tableIdCached = false;
    m_cachedTableId = -1;
    qDebug() << "VectorTableDelegate::refreshTableIdCache - 表ID缓存已重置";
}

VectorTableDelegate::~VectorTableDelegate()
{
    // 析构函数
}

QWidget *VectorTableDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    qDebug() << "VectorTableDelegate::createEditor - 为单元格创建编辑器，行:" << index.row() << "列:" << index.column();

    int column = index.column();
    QWidget *editor = nullptr;

    // 获取当前列的类型信息
    int tableId = getTableIdForCurrentTable();

    if (tableId < 0)
    {
        qWarning() << "VectorTableDelegate::createEditor - 无效的表ID:" << tableId << "，使用默认编辑器";
        return QStyledItemDelegate::createEditor(parent, option, index);
    }

    Vector::ColumnInfo colInfo = getColumnInfoByIndex(tableId, column);
    qDebug() << "VectorTableDelegate::createEditor - 列信息 - 名称:" << colInfo.name
             << "类型:" << static_cast<int>(colInfo.type)
             << "原始类型字符串:" << colInfo.original_type_str;

    // 根据列类型创建合适的编辑器
    if (colInfo.type == Vector::ColumnDataType::INSTRUCTION_ID)
    {
        // 创建指令下拉框
        QComboBox *comboBox = new QComboBox(parent);
        comboBox->addItems(getInstructionOptions());
        comboBox->setEditable(false);
        editor = comboBox;
        qDebug() << "VectorTableDelegate::createEditor - 创建指令下拉框编辑器";
    }
    else if (colInfo.type == Vector::ColumnDataType::TIMESET_ID)
    {
        // 创建TimeSet下拉框
        QComboBox *comboBox = new QComboBox(parent);
        comboBox->addItems(getTimeSetOptions());
        comboBox->setEditable(false);
        editor = comboBox;
        qDebug() << "VectorTableDelegate::createEditor - 创建TimeSet下拉框编辑器";
    }
    else if (colInfo.type == Vector::ColumnDataType::BOOLEAN)
    {
        // 创建布尔值下拉框(如Capture列)
        QComboBox *comboBox = new QComboBox(parent);
        comboBox->addItems(getCaptureOptions());
        comboBox->setEditable(false);
        editor = comboBox;
        qDebug() << "VectorTableDelegate::createEditor - 创建布尔值下拉框编辑器";
    }
    else if (colInfo.type == Vector::ColumnDataType::PIN_STATE_ID)
    {
        // 创建管脚状态编辑框
        PinValueLineEdit *pinEdit = new PinValueLineEdit(parent);
        editor = pinEdit;
        qDebug() << "VectorTableDelegate::createEditor - 创建管脚状态编辑器";
    }
    else if (colInfo.type == Vector::ColumnDataType::INTEGER)
    {
        // 创建整数编辑框
        QLineEdit *lineEdit = new QLineEdit(parent);
        lineEdit->setValidator(new QIntValidator(lineEdit));
        editor = lineEdit;
        qDebug() << "VectorTableDelegate::createEditor - 创建整数编辑器";
    }
    else if (colInfo.type == Vector::ColumnDataType::REAL)
    {
        // 创建浮点数编辑框
        QLineEdit *lineEdit = new QLineEdit(parent);
        lineEdit->setValidator(new QDoubleValidator(lineEdit));
        editor = lineEdit;
        qDebug() << "VectorTableDelegate::createEditor - 创建浮点数编辑器";
    }
    else if (colInfo.type == Vector::ColumnDataType::TEXT)
    {
        // 创建文本编辑框
        QLineEdit *lineEdit = new QLineEdit(parent);
        editor = lineEdit;
        qDebug() << "VectorTableDelegate::createEditor - 创建文本编辑器";
    }
    else if (colInfo.type == Vector::ColumnDataType::JSON_PROPERTIES)
    {
        QLineEdit *lineEdit = new QLineEdit(parent);
        // JSON属性限制最大长度
        lineEdit->setMaxLength(Persistence::JSON_PROPERTIES_MAX_LENGTH / 2 - 50); // 留一些空间给JSON格式化和内部存储
        editor = lineEdit;
        qDebug() << "VectorTableDelegate::createEditor - 创建JSON属性编辑器, 最大长度:" << lineEdit->maxLength();
    }
    else
    {
        qDebug() << "VectorTableDelegate::createEditor - 未知列类型" << static_cast<int>(colInfo.type)
                 << "，使用默认编辑器";
        // 默认使用基类创建
        editor = QStyledItemDelegate::createEditor(parent, option, index);
    }

    return editor;
}

void VectorTableDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    int column = index.column();

    // 获取当前列的类型信息
    int tableId = getTableIdForCurrentTable();
    Vector::ColumnInfo colInfo = getColumnInfoByIndex(tableId, column);

    // 获取模型中的数据
    QVariant value = index.model()->data(index, Qt::EditRole);

    if (colInfo.type == Vector::ColumnDataType::INSTRUCTION_ID ||
        colInfo.type == Vector::ColumnDataType::TIMESET_ID ||
        colInfo.type == Vector::ColumnDataType::BOOLEAN)
    {
        // 设置下拉框的当前值
        QComboBox *comboBox = qobject_cast<QComboBox *>(editor);
        if (comboBox)
        {
            QString strValue = value.toString();
            int idx = comboBox->findText(strValue);
            if (idx >= 0)
            {
                comboBox->setCurrentIndex(idx);
            }
        }
    }
    else if (colInfo.type == Vector::ColumnDataType::PIN_STATE_ID)
    {
        // 设置管脚状态编辑器的值
        PinValueLineEdit *lineEdit = qobject_cast<PinValueLineEdit *>(editor);
        if (lineEdit)
        {
            QString strValue = value.toString();
            if (strValue.isEmpty())
            {
                strValue = "X"; // 默认值
            }
            lineEdit->setText(strValue);
        }
    }
    else if (colInfo.type == Vector::ColumnDataType::INTEGER ||
             colInfo.type == Vector::ColumnDataType::REAL ||
             colInfo.type == Vector::ColumnDataType::TEXT ||
             colInfo.type == Vector::ColumnDataType::JSON_PROPERTIES)
    {
        // 设置文本编辑框的值
        QLineEdit *lineEdit = qobject_cast<QLineEdit *>(editor);
        if (lineEdit)
        {
            lineEdit->setText(value.toString());
        }
    }
    else
    {
        // 默认使用基类设置
        QStyledItemDelegate::setEditorData(editor, index);
    }
}

void VectorTableDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    int column = index.column();

    // 获取当前列的类型信息
    int tableId = getTableIdForCurrentTable();
    Vector::ColumnInfo colInfo = getColumnInfoByIndex(tableId, column);

    if (colInfo.type == Vector::ColumnDataType::INSTRUCTION_ID)
    {
        QComboBox *comboBox = qobject_cast<QComboBox *>(editor);
        if (comboBox)
        {
            QString value = comboBox->currentText();
            model->setData(index, value, Qt::EditRole);
        }
    }
    else if (colInfo.type == Vector::ColumnDataType::TIMESET_ID)
    {
        QComboBox *comboBox = qobject_cast<QComboBox *>(editor);
        if (comboBox)
        {
            QString value = comboBox->currentText();
            model->setData(index, value, Qt::EditRole);
        }
    }
    else if (colInfo.type == Vector::ColumnDataType::BOOLEAN)
    {
        QComboBox *comboBox = qobject_cast<QComboBox *>(editor);
        if (comboBox)
        {
            QString value = comboBox->currentText();
            model->setData(index, value, Qt::EditRole);
        }
    }
    else if (colInfo.type == Vector::ColumnDataType::PIN_STATE_ID)
    {
        PinValueLineEdit *lineEdit = qobject_cast<PinValueLineEdit *>(editor);
        if (lineEdit)
        {
            QString value = lineEdit->text();
            // 如果为空，默认使用X
            if (value.isEmpty())
            {
                value = "X";
            }
            // 简单转换大写，但不进行严格验证
            value = value.toUpper();
            model->setData(index, value, Qt::EditRole);
        }
    }
    else if (colInfo.type == Vector::ColumnDataType::INTEGER ||
             colInfo.type == Vector::ColumnDataType::REAL ||
             colInfo.type == Vector::ColumnDataType::TEXT ||
             colInfo.type == Vector::ColumnDataType::JSON_PROPERTIES)
    {
        QLineEdit *lineEdit = qobject_cast<QLineEdit *>(editor);
        if (lineEdit)
        {
            model->setData(index, lineEdit->text(), Qt::EditRole);
        }
    }
    else
    {
        // 默认使用基类方法
        QStyledItemDelegate::setModelData(editor, model, index);
    }
}

// 辅助函数：获取当前表的ID
int VectorTableDelegate::getTableIdForCurrentTable() const
{
    qDebug() << "[Debug] VectorTableItemDelegate::getTableIdForCurrentTable - Function entry.";

    // 如果已经缓存了表ID，直接返回
    if (m_tableIdCached)
    {
        qDebug() << "[Debug] VectorTableItemDelegate::getTableIdForCurrentTable - 使用缓存的表ID:" << m_cachedTableId;
        return m_cachedTableId;
    }

    // 否则执行正常的查找流程
    QComboBox *vectorTableSelector = qobject_cast<QComboBox *>(getVectorTableSelectorPtr());
    if (!vectorTableSelector)
    {
        qWarning() << "[Debug] VectorTableItemDelegate::getTableIdForCurrentTable - Failed to get QComboBox* from getVectorTableSelectorPtr().";
        return -1;
    }
    qDebug() << "[Debug] VectorTableItemDelegate::getTableIdForCurrentTable - Successfully got vectorTableSelector. ObjectName:" << vectorTableSelector->objectName();

    int currentIndex = vectorTableSelector->currentIndex();
    qDebug() << "[Debug] VectorTableItemDelegate::getTableIdForCurrentTable - Current index of vectorTableSelector:" << currentIndex;
    if (currentIndex < 0)
    {
        qWarning() << "[Debug] VectorTableItemDelegate::getTableIdForCurrentTable - vectorTableSelector current index is invalid (" << currentIndex << ").";
        return -1;
    }

    QVariant itemData = vectorTableSelector->itemData(currentIndex);
    qDebug() << "[Debug] VectorTableItemDelegate::getTableIdForCurrentTable - Item data for current index:" << itemData;
    bool conversionOk = false;
    int tableId = itemData.toInt(&conversionOk);
    if (!conversionOk)
    {
        qWarning() << "[Debug] VectorTableItemDelegate::getTableIdForCurrentTable - Failed to convert itemData to int. Original data:" << itemData;
        return -1;
    }

    // 缓存结果
    m_tableIdCached = true;
    m_cachedTableId = tableId;

    qDebug() << "[Debug] VectorTableItemDelegate::getTableIdForCurrentTable - 缓存表ID:" << tableId;
    qDebug() << "[Debug] VectorTableItemDelegate::getTableIdForCurrentTable - Returning tableId:" << tableId;
    return tableId;
}

// 辅助函数：通过UI索引获取列信息
Vector::ColumnInfo VectorTableDelegate::getColumnInfoByIndex(int tableId, int uiColumnIndex) const
{
    Vector::ColumnInfo defaultInfo;
    if (tableId < 0 || uiColumnIndex < 0)
    {
        qWarning() << "VectorTableDelegate::getColumnInfoByIndex - 收到无效参数，tableId:" << tableId << "uiColumnIndex:" << uiColumnIndex;
        return defaultInfo;
    }

    // 1. 检查缓存
    if (!s_tableColumnsCache.contains(tableId))
    {
        // 缓存未命中，从数据库加载并存入缓存
        qDebug() << "VectorTableDelegate::getColumnInfoByIndex - 缓存未命中，为表ID加载列配置:" << tableId;
        s_tableColumnsCache[tableId] = getColumnConfiguration(tableId);
    }

    const QList<Vector::ColumnInfo> &allColumns = s_tableColumnsCache.value(tableId);

    // 再次检查，如果缓存中的列表仍然为空（说明加载失败），则发出警告
    if (allColumns.isEmpty())
    {
        qWarning() << "VectorTableDelegate::getColumnInfoByIndex - 缓存中表ID" << tableId << "的列配置为空或加载失败。";
        // 尝试重新加载一次，以应对首次加载时数据库未准备好的情况
        QList<Vector::ColumnInfo> refreshedColumns = getColumnConfiguration(tableId);
        if (refreshedColumns.isEmpty())
        {
            qWarning() << "VectorTableDelegate::getColumnInfoByIndex - 重新加载后，列配置仍然为空。";
            return defaultInfo;
        }
        else
        {
            qDebug() << "VectorTableDelegate::getColumnInfoByIndex - 重新加载成功，获取到" << refreshedColumns.size() << "个列。";
            s_tableColumnsCache[tableId] = refreshedColumns;
            // 重新获取对缓存中数据的引用
            const QList<Vector::ColumnInfo> &reloadedColumns = s_tableColumnsCache.value(tableId);
            return getVisibleColumn(reloadedColumns, uiColumnIndex, tableId);
        }
    }

    return getVisibleColumn(allColumns, uiColumnIndex, tableId);
}

// 新增辅助函数，用于从列配置列表中提取可见列
Vector::ColumnInfo VectorTableDelegate::getVisibleColumn(const QList<Vector::ColumnInfo> &allColumns, int uiColumnIndex, int tableId) const
{
    Vector::ColumnInfo defaultInfo;

    // 筛选出可见的列
    QList<Vector::ColumnInfo> visibleColumns;
    for (const auto &col : allColumns)
    {
        if (col.is_visible)
        {
            visibleColumns.append(col);
        }
    }

    // 从可见列列表中通过UI索引获取正确的列信息
    if (uiColumnIndex < visibleColumns.size())
    {
        return visibleColumns[uiColumnIndex];
    }
    else
    {
        qWarning() << "VectorTableDelegate::getVisibleColumn - UI列索引" << uiColumnIndex
                   << "超出了可见列的数量" << visibleColumns.size() << "对于表ID" << tableId;
        return defaultInfo;
    }
}

// 辅助函数：获取向量表选择器指针
QObject *VectorTableDelegate::getVectorTableSelectorPtr() const
{
    qDebug() << "[Debug] VectorTableItemDelegate::getVectorTableSelectorPtr - Function entry.";
    QMainWindow *mainWindow = qobject_cast<QMainWindow *>(QApplication::activeWindow());
    if (!mainWindow)
    {
        qWarning() << "[Debug] VectorTableItemDelegate::getVectorTableSelectorPtr - QApplication::activeWindow() is not a QMainWindow or is null.";
        return nullptr;
    }
    qDebug() << "[Debug] VectorTableItemDelegate::getVectorTableSelectorPtr - Successfully got QMainWindow. Attempting to find child QComboBox 'm_vectorTableSelector'.";

    QComboBox *selector = mainWindow->findChild<QComboBox *>("m_vectorTableSelector");
    if (!selector)
    {
        qWarning() << "[Debug] VectorTableItemDelegate::getVectorTableSelectorPtr - Did not find QComboBox with object name 'm_vectorTableSelector' in mainWindow.";
        // Let's list all QComboBox children of mainWindow to help debug
        QList<QComboBox *> comboBoxes = mainWindow->findChildren<QComboBox *>();
        qDebug() << "[Debug] VectorTableItemDelegate::getVectorTableSelectorPtr - Found QComboBoxes in mainWindow:";
        for (QComboBox *cb : comboBoxes)
        {
            qDebug() << "[Debug]   - ComboBox Name:" << cb->objectName() << "Visible:" << cb->isVisible();
        }
    }
    else
    {
        qDebug() << "[Debug] VectorTableItemDelegate::getVectorTableSelectorPtr - Found 'm_vectorTableSelector'. ObjectName:" << selector->objectName();
    }
    return selector;
}

// 获取列配置（直接从数据库获取，供缓存使用）
QList<Vector::ColumnInfo> VectorTableDelegate::getColumnConfiguration(int tableId) const
{
    QList<Vector::ColumnInfo> columns;

    // 参数检查
    if (tableId <= 0)
    {
        qWarning() << "VectorTableDelegate::getColumnConfiguration - 无效的表ID:" << tableId;
        return columns;
    }

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << "VectorTableDelegate::getColumnConfiguration - 数据库未打开";
        return columns;
    }

    // --- NEW DIAGNOSTIC LOGGING ---
    qDebug() << "[DB_DIAGNOSTIC] In VectorTableDelegate::getColumnConfiguration:";
    qDebug() << "[DB_DIAGNOSTIC]   Connection Name:" << db.connectionName();
    qDebug() << "[DB_DIAGNOSTIC]   Database Name:" << db.databaseName();
    qDebug() << "[DB_DIAGNOSTIC]   Is Valid:" << db.isValid();
    qDebug() << "[DB_DIAGNOSTIC]   Driver Name:" << db.driverName();
    // --- END DIAGNOSTIC LOGGING ---

    qDebug() << "VectorTableDelegate::getColumnConfiguration - 开始从数据库加载表ID" << tableId << "的列配置";

    QSqlQuery query(db);
    query.prepare("SELECT * FROM VectorTableColumnConfiguration WHERE master_record_id = ? ORDER BY column_order");
    query.addBindValue(tableId);

    if (!query.exec())
    {
        qWarning() << "VectorTableDelegate::getColumnConfiguration - 执行查询失败:"
                   << query.lastError().text() << "，SQL:" << query.lastQuery();
        return columns;
    }

    // 检查查询是否返回任何行
    if (query.size() == 0)
    {
        qWarning() << "VectorTableDelegate::getColumnConfiguration - 表ID" << tableId
                   << "在VectorTableColumnConfiguration中没有任何列配置记录";
    }

    while (query.next())
    {
        Vector::ColumnInfo col;
        // 按索引取值以匹配 SELECT *
        col.id = query.value(0).toInt();
        // master_record_id 是第1列，与tableId相同
        col.vector_table_id = tableId;
        col.name = query.value(2).toString();
        col.order = query.value(3).toInt();
        col.original_type_str = query.value(4).toString();

        // 添加详细诊断日志
        qDebug() << "VectorTableDelegate::getColumnConfiguration - 列 " << col.name
                 << " 的原始类型字符串：'" << col.original_type_str << "'";

        // 传递列名到columnDataTypeFromString函数，实现基于列名的类型映射
        col.type = Vector::columnDataTypeFromString(col.original_type_str, col.name);

        // 添加类型转换结果日志
        qDebug() << "VectorTableDelegate::getColumnConfiguration - 列 " << col.name
                 << " 类型转换结果：" << static_cast<int>(col.type);

        // default_value 是第5列
        // is_visible 是第6列
        col.is_visible = query.value(6).toBool();
        // data_properties 是第7列
        QString propStr = query.value(7).toString();

        if (!propStr.isEmpty())
        {
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(propStr.toUtf8(), &err);
            if (err.error == QJsonParseError::NoError && doc.isObject())
                col.data_properties = doc.object();
        }

        columns.append(col);
        qDebug() << "VectorTableDelegate::getColumnConfiguration - 加载列:" << col.name
                 << "类型:" << col.original_type_str << "是否可见:" << col.is_visible;
    }

    qDebug() << "VectorTableDelegate::getColumnConfiguration - 成功加载了" << columns.size()
             << "个列配置，表ID:" << tableId;

    return columns;
}

QStringList VectorTableDelegate::getInstructionOptions() const
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

QStringList VectorTableDelegate::getTimeSetOptions() const
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

QStringList VectorTableDelegate::getCaptureOptions() const
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

void VectorTableDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // 使用默认绘制函数，不对PIN_STATE_ID列进行特殊颜色处理
    QStyledItemDelegate::paint(painter, option, index);
}