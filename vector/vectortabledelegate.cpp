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
#include <QApplication>
#include <QStyle>

VectorTableDelegate::VectorTableDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

VectorTableDelegate::~VectorTableDelegate()
{
}

void VectorTableDelegate::setColumnInfoList(const QList<Vector::ColumnInfo> &columns)
{
    m_columnInfoList = columns;
}

QWidget *VectorTableDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    int column = index.column();
    QWidget *editor = nullptr;

    // 获取当前列的类型信息
    Vector::ColumnDataType colType = getColumnType(column);

    // 根据列类型创建合适的编辑器
    if (colType == Vector::ColumnDataType::INSTRUCTION_ID)
    {
        // 创建指令下拉框
        QComboBox *comboBox = new QComboBox(parent);
        comboBox->addItems(getInstructionOptions());
        comboBox->setEditable(false);
        editor = comboBox;
    }
    else if (colType == Vector::ColumnDataType::TIMESET_ID)
    {
        // 创建TimeSet下拉框
        QComboBox *comboBox = new QComboBox(parent);
        comboBox->addItems(getTimeSetOptions());
        comboBox->setEditable(false);
        editor = comboBox;
    }
    else if (colType == Vector::ColumnDataType::BOOLEAN)
    {
        // 创建布尔值下拉框
        QComboBox *comboBox = new QComboBox(parent);
        comboBox->addItem("False");
        comboBox->addItem("True");
        editor = comboBox;
    }
    else if (colType == Vector::ColumnDataType::PIN_STATE_ID)
    {
        // 使用自定义的PinValueLineEdit作为编辑器
        PinValueLineEdit *lineEdit = new PinValueLineEdit(parent);
        lineEdit->setAlignment(Qt::AlignCenter); // 文本居中显示
        editor = lineEdit;
    }
    else if (colType == Vector::ColumnDataType::INTEGER)
    {
        QLineEdit *lineEdit = new QLineEdit(parent);
        QIntValidator *validator = new QIntValidator(lineEdit);
        lineEdit->setValidator(validator);
        lineEdit->setMaxLength(10); // INT32范围的最大位数
        editor = lineEdit;
    }
    else if (colType == Vector::ColumnDataType::REAL)
    {
        QLineEdit *lineEdit = new QLineEdit(parent);
        QDoubleValidator *validator = new QDoubleValidator(-1000000, 1000000, 6, lineEdit);
        validator->setNotation(QDoubleValidator::StandardNotation);
        lineEdit->setValidator(validator);
        lineEdit->setMaxLength(20); // 包含小数点和正负号的双精度浮点数
        editor = lineEdit;
    }
    else if (colType == Vector::ColumnDataType::TEXT)
    {
        QLineEdit *lineEdit = new QLineEdit(parent);

        // 根据列名称设置不同的长度限制
        if (column >= 0 && column < m_columnInfoList.size())
        {
            if (m_columnInfoList[column].name.compare("LABEL", Qt::CaseInsensitive) == 0)
            {
                // Label字段限制为15个字节
                lineEdit->setMaxLength(15);
            }
            else if (m_columnInfoList[column].name.compare("COMMENT", Qt::CaseInsensitive) == 0)
            {
                // Comment字段限制为30个字节
                lineEdit->setMaxLength(30);
            }
        }
        else if (column >= 0 && column < m_columnInfoList.size() && m_columnInfoList[column].name.compare("EXT", Qt::CaseInsensitive) == 0)
        {
            // EXT字段限制为5个字节
            lineEdit->setMaxLength(5);
        }
        else
        {
            // 其他文本字段默认限制为255个字符
            lineEdit->setMaxLength(255);
        }

        editor = lineEdit;
    }
    else if (colType == Vector::ColumnDataType::JSON_PROPERTIES)
    {
        QLineEdit *lineEdit = new QLineEdit(parent);
        // JSON属性限制最大长度
        lineEdit->setMaxLength(Persistence::JSON_PROPERTIES_MAX_LENGTH / 2 - 50); // 留一些空间给JSON格式化和内部存储
        editor = lineEdit;
    }
    else
    {
        // 默认使用基类创建
        editor = QStyledItemDelegate::createEditor(parent, option, index);
    }

    return editor;
}

void VectorTableDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    int column = index.column();
    Vector::ColumnDataType colType = getColumnType(column);

    if (colType == Vector::ColumnDataType::INSTRUCTION_ID ||
        colType == Vector::ColumnDataType::TIMESET_ID ||
        colType == Vector::ColumnDataType::BOOLEAN)
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
    else if (colType == Vector::ColumnDataType::PIN_STATE_ID)
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
    int column = index.column();
    Vector::ColumnDataType colType = getColumnType(column);

    if (colType == Vector::ColumnDataType::INSTRUCTION_ID ||
        colType == Vector::ColumnDataType::TIMESET_ID ||
        colType == Vector::ColumnDataType::BOOLEAN)
    {
        QComboBox *comboBox = static_cast<QComboBox *>(editor);
        QString value = comboBox->currentText();
        model->setData(index, value, Qt::EditRole);
    }
    else if (colType == Vector::ColumnDataType::PIN_STATE_ID)
    {
        // 从PinValueLineEdit获取数据
        PinValueLineEdit *lineEdit = static_cast<PinValueLineEdit *>(editor);
        QString value = lineEdit->text();
        // 如果为空，默认使用X
        if (value.isEmpty())
        {
            value = "X";
        }
        // 转换大写
        value = value.toUpper();
        
        // 发送十六进制值编辑信号
        emit hexValueEdited(index.row(), index.column(), value);
        
        model->setData(index, value, Qt::EditRole);
    }
    else
    {
        // 默认使用基类设置
        QStyledItemDelegate::setModelData(editor, model, index);
    }
}

void VectorTableDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QStyledItemDelegate::paint(painter, option, index);
    }

    Vector::ColumnDataType colType = getColumnType(index.column());

    // 保存画笔状态
    painter->save();

    // 使用标准绘制背景 (选中、悬停等效果)
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    const QWidget *widget = option.widget;
    QStyle *style = widget ? widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

    // 根据列类型绘制内容
    QRect textRect = option.rect.adjusted(3, 1, -3, -1); // 文本区域略小于单元格
    QString text = index.data().toString();
    
    // 针对不同类型的内容应用不同样式
    switch (colType) {
        case Vector::ColumnDataType::PIN_STATE_ID: {
            // 高亮显示管脚状态
            painter->setPen(Qt::blue);
            painter->drawText(textRect, Qt::AlignCenter, text);
            break;
        }
        case Vector::ColumnDataType::TIMESET_ID: {
            // 高亮显示TimeSet
            painter->setPen(Qt::darkGreen);
            painter->drawText(textRect, Qt::AlignCenter, text);
            break;
        }
        case Vector::ColumnDataType::INSTRUCTION_ID: {
            // 指令用斜体
            QFont font = painter->font();
            font.setItalic(true);
            painter->setFont(font);
            painter->setPen(Qt::black);
            painter->drawText(textRect, Qt::AlignCenter, text);
            break;
        }
        default:
            // 其他类型使用默认样式
            painter->setPen(Qt::black);
            painter->drawText(textRect, Qt::AlignCenter, text);
    }

    // 恢复画笔状态
    painter->restore();
}

QSize VectorTableDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    // 确保单元格有足够的高度
    size.setHeight(qMax(size.height(), 25));
    return size;
}

Vector::ColumnDataType VectorTableDelegate::getColumnType(int column) const
{
    if (column >= 0 && column < m_columnInfoList.size()) {
        return m_columnInfoList[column].type;
    }
    return Vector::ColumnDataType::TEXT; // 默认返回TEXT类型
}

QStringList VectorTableDelegate::getInstructionOptions() const
{
    QStringList options;
    
    // 从数据库获取指令选项
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery query(db);
    query.prepare("SELECT instruction_value FROM instruction_options ORDER BY id");
    if (query.exec())
    {
        while (query.next())
        {
            options << query.value(0).toString();
        }
    }
    else
    {
        qWarning() << "获取指令选项失败:" << query.lastError().text();
    }
    
    return options;
}

QStringList VectorTableDelegate::getTimeSetOptions() const
{
    QStringList options;
    
    // 从数据库获取TimeSet选项
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery query(db);
    query.prepare("SELECT timeset_name FROM timeset_list ORDER BY id");
    if (query.exec())
    {
        while (query.next())
        {
            options << query.value(0).toString();
        }
    }
    else
    {
        qWarning() << "获取TimeSet选项失败:" << query.lastError().text();
    }
    
    return options;
}

QStringList VectorTableDelegate::getCaptureOptions() const
{
    QStringList options;
    options << "Y" << "N";
    return options;
} 