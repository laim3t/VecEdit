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

    qDebug() << "VectorTableItemDelegate::refreshCache - 缓存已清空，下次使用将重新从数据库加载";
}

// 刷新表ID缓存方法实现
void VectorTableDelegate::refreshTableIdCache()
{
    m_tableIdCached = false;
    m_cachedTableId = -1;
    qDebug() << "VectorTableItemDelegate::refreshTableIdCache - 表ID缓存已重置";
}

VectorTableDelegate::~VectorTableDelegate()
{
    // 析构函数
}

QWidget *VectorTableDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    qDebug() << "[Debug] VectorTableItemDelegate::createEditor - Function entry. Attempting to create editor for cell at row:" << index.row() << "column:" << index.column();

    int column = index.column();
    QWidget *editor = nullptr;

    // 获取当前列的类型信息
    qDebug() << "[Debug] VectorTableItemDelegate::createEditor - Calling getTableIdForCurrentTable() for column:" << column;
    int tableId = getTableIdForCurrentTable();
    qDebug() << "[Debug] VectorTableItemDelegate::createEditor - Received tableId:" << tableId;

    if (tableId < 0) // Assuming -1 or other negative values indicate an error or invalid table
    {
        qWarning() << "[Debug] VectorTableItemDelegate::createEditor - Invalid tableId (" << tableId << ") received. Falling back to default editor.";
        return QStyledItemDelegate::createEditor(parent, option, index);
    }

    qDebug() << "[Debug] VectorTableItemDelegate::createEditor - Calling getColumnInfoByIndex() for tableId:" << tableId << "and uiColumnIndex:" << column;
    Vector::ColumnInfo colInfo = getColumnInfoByIndex(tableId, column);
    qDebug() << "[Debug] VectorTableItemDelegate::createEditor - Received ColumnInfo - Name:" << colInfo.name << "Type String:" << colInfo.original_type_str << "Resolved Type Enum:" << static_cast<int>(colInfo.type) << "Order:" << colInfo.order;

    // 根据列类型创建合适的编辑器
    if (colInfo.type == Vector::ColumnDataType::INSTRUCTION_ID)
    {
        qDebug() << "[Debug] VectorTableItemDelegate::createEditor - Detected INSTRUCTION_ID type for column:" << column << ". Creating QComboBox.";
        // 创建指令下拉框
        QComboBox *comboBox = new QComboBox(parent);
        comboBox->addItems(getInstructionOptions());
        comboBox->setEditable(false);
        editor = comboBox;
        qDebug() << "VectorTableDelegate::createEditor - 创建指令下拉框编辑器";
    }
    else if (colInfo.type == Vector::ColumnDataType::TIMESET_ID)
    {
        qDebug() << "[Debug] VectorTableItemDelegate::createEditor - Detected TIMESET_ID type for column:" << column << ". Creating QComboBox.";
        // 创建TimeSet下拉框
        QComboBox *comboBox = new QComboBox(parent);
        comboBox->addItems(getTimeSetOptions());
        comboBox->setEditable(false);
        editor = comboBox;
        qDebug() << "VectorTableDelegate::createEditor - 创建TimeSet下拉框编辑器";
    }
    else if (colInfo.type == Vector::ColumnDataType::BOOLEAN)
    {
        qDebug() << "[Debug] VectorTableItemDelegate::createEditor - Detected BOOLEAN type for column:" << column << ". Creating QComboBox.";
        // 创建Capture下拉框（Y/N）
        QComboBox *comboBox = new QComboBox(parent);
        comboBox->addItem("False");
        comboBox->addItem("True");
        editor = comboBox;
        qDebug() << "VectorTableDelegate::createEditor - 创建布尔值下拉框编辑器";
    }
    else if (colInfo.type == Vector::ColumnDataType::PIN_STATE_ID)
    {
        qDebug() << "[Debug] VectorTableItemDelegate::createEditor - Detected PIN_STATE_ID type for column:" << column << ". Creating PinValueLineEdit for pin state.";
        // 使用自定义的PinValueLineEdit作为编辑器
        PinValueLineEdit *lineEdit = new PinValueLineEdit(parent);
        lineEdit->setAlignment(Qt::AlignCenter); // 文本居中显示
        editor = lineEdit;
        qDebug() << "VectorTableDelegate::createEditor - 创建管脚状态编辑器";
    }
    else if (colInfo.type == Vector::ColumnDataType::INTEGER)
    {
        QLineEdit *lineEdit = new QLineEdit(parent);
        QIntValidator *validator = new QIntValidator(lineEdit);
        lineEdit->setValidator(validator);
        lineEdit->setMaxLength(10); // INT32范围的最大位数
        editor = lineEdit;
        qDebug() << "VectorTableDelegate::createEditor - 创建整数编辑器";
    }
    else if (colInfo.type == Vector::ColumnDataType::REAL)
    {
        QLineEdit *lineEdit = new QLineEdit(parent);
        QDoubleValidator *validator = new QDoubleValidator(-1000000, 1000000, 6, lineEdit);
        validator->setNotation(QDoubleValidator::StandardNotation);
        lineEdit->setValidator(validator);
        lineEdit->setMaxLength(20); // 包含小数点和正负号的双精度浮点数
        editor = lineEdit;
        qDebug() << "VectorTableDelegate::createEditor - 创建浮点数编辑器";
    }
    else if (colInfo.type == Vector::ColumnDataType::TEXT)
    {
        QLineEdit *lineEdit = new QLineEdit(parent);

        // 根据字段名称设置不同的长度限制
        if (colInfo.name.compare("Label", Qt::CaseInsensitive) == 0)
        {
            // Label字段限制为15个字节
            lineEdit->setMaxLength(15);
            qDebug() << "创建Label字段编辑器，限制长度为15字节";
        }
        else if (colInfo.name.compare("Comment", Qt::CaseInsensitive) == 0)
        {
            // Comment字段限制为30个字节
            lineEdit->setMaxLength(30);
            qDebug() << "创建Comment字段编辑器，限制长度为30字节";
        }
        else if (colInfo.name.compare("EXT", Qt::CaseInsensitive) == 0)
        {
            // EXT字段限制为5个字节
            lineEdit->setMaxLength(5);
            qDebug() << "创建EXT字段编辑器，限制长度为5字节";
        }
        else
        {
            // 其他文本字段默认限制为255个字符
            lineEdit->setMaxLength(255);
            qDebug() << "创建普通文本字段编辑器，限制长度为255字符";
        }

        editor = lineEdit;
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
        qDebug() << "[Debug] VectorTableItemDelegate::createEditor - Column type is" << static_cast<int>(colInfo.type) << "or not specifically handled. Falling back to default editor for column:" << column;
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
        // 设置编辑器数据时，目标是PinValueLineEdit
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

void VectorTableDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    if (!index.isValid() || !model || !editor)
    {
        QStyledItemDelegate::setModelData(editor, model, index);
        return;
    }

    // 获取单元格所在的列索引
    int tableId = getTableIdForCurrentTable();
    if (tableId < 0)
    {
        // 如果无法获取表ID，回退到默认委托
        QStyledItemDelegate::setModelData(editor, model, index);
        return;
    }

    // 使用静态缓存减少数据库查询
    QList<Vector::ColumnInfo> columns;
    if (s_tableColumnsCache.contains(tableId))
    {
        columns = s_tableColumnsCache[tableId];
    }
    else
    {
        // 缓存未命中，查询数据库
        columns = getColumnConfiguration(tableId);
        // 更新缓存
        s_tableColumnsCache[tableId] = columns;
    }

    // 如果无法获取列信息或列索引越界，回退到默认委托
    int columnIndex = index.column();
    if (columns.isEmpty() || columnIndex >= columns.size())
    {
        QStyledItemDelegate::setModelData(editor, model, index);
        return;
    }

    const Vector::ColumnInfo &colInfo = columns[columnIndex];

    if (colInfo.type == Vector::ColumnDataType::INSTRUCTION_ID)
    {
        QComboBox *comboBox = static_cast<QComboBox *>(editor);
        QString value = comboBox->currentText();
        model->setData(index, value, Qt::EditRole);
    }
    else if (colInfo.type == Vector::ColumnDataType::TIMESET_ID)
    {
        QComboBox *comboBox = static_cast<QComboBox *>(editor);
        QString value = comboBox->currentText();
        model->setData(index, value, Qt::EditRole);
    }
    else if (colInfo.type == Vector::ColumnDataType::BOOLEAN)
    {
        QComboBox *comboBox = static_cast<QComboBox *>(editor);
        QString value = comboBox->currentText();
        model->setData(index, value, Qt::EditRole);
    }
    else if (colInfo.type == Vector::ColumnDataType::PIN_STATE_ID)
    {
        // 从PinValueLineEdit获取数据
        PinValueLineEdit *lineEdit = static_cast<PinValueLineEdit *>(editor);
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
    else
    {
        // 处理TEXT、INTEGER和其他类型 - 优化：直接设置数据，避免冗余处理
        QLineEdit *lineEdit = qobject_cast<QLineEdit *>(editor);
        if (lineEdit)
        {
            // 直接设置值，跳过不必要的处理
            model->setData(index, lineEdit->text(), Qt::EditRole);
        }
        else
        {
            // 对于不是QLineEdit的控件，回退到默认委托
            QStyledItemDelegate::setModelData(editor, model, index);
        }
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
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
        return columns;

    QSqlQuery query(db);
    query.prepare("SELECT id, column_name, column_order, column_type, data_properties, IsVisible "
                  "FROM VectorTableColumnConfiguration "
                  "WHERE master_record_id = ? ORDER BY column_order");
    query.addBindValue(tableId);

    if (!query.exec())
        return columns;

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
                col.data_properties = doc.object();
        }

        columns.append(col);
    }

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
    // 暂时使用默认绘制函数
    QStyledItemDelegate::paint(painter, option, index);
}