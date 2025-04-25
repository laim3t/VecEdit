#include "vectortabledelegate.h"
#include "pinvalueedit.h"
#include "databasemanager.h"

#include <QComboBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

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
    m_timesetOptions.clear();
    m_pinOptions.clear();

    qDebug() << "VectorTableItemDelegate::refreshCache - 缓存已清空，下次使用将重新从数据库加载";
}

VectorTableItemDelegate::~VectorTableItemDelegate()
{
    // 析构函数
}

QWidget *VectorTableItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // 根据列索引创建不同类型的编辑器
    int column = index.column();

    // 指令列
    if (column == 1)
    {
        QComboBox *editor = new QComboBox(parent);
        editor->addItems(getInstructionOptions());
        return editor;
    }
    // 时间集列
    else if (column == 2)
    {
        QComboBox *editor = new QComboBox(parent);
        editor->addItems(getTimesetOptions());
        return editor;
    }
    // 捕获列
    else if (column == 3)
    {
        QComboBox *editor = new QComboBox(parent);
        editor->addItem("");
        editor->addItem("Y");
        return editor;
    }
    // 管脚列 - 使用PinValueLineEdit代替QComboBox
    else if (column >= 6)
    {
        PinValueLineEdit *editor = new PinValueLineEdit(parent);
        return editor;
    }

    // 其他列（标签、Ext、注释）使用默认文本编辑器
    return QStyledItemDelegate::createEditor(parent, option, index);
}

void VectorTableItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    int column = index.column();
    QString value = index.model()->data(index, Qt::EditRole).toString();

    // 如果是下拉框编辑器
    if ((column == 1) || (column == 2) || (column == 3))
    {
        QComboBox *comboBox = static_cast<QComboBox *>(editor);

        // 对于Capture列，特殊处理空值
        if (column == 3 && value.isEmpty())
        {
            comboBox->setCurrentIndex(0); // 设置为第一项（空值）
        }
        else
        {
            int comboIndex = comboBox->findText(value);
            if (comboIndex >= 0)
            {
                comboBox->setCurrentIndex(comboIndex);
            }
        }
    }
    // 如果是管脚列（使用PinValueLineEdit）
    else if (column >= 6)
    {
        PinValueLineEdit *lineEdit = static_cast<PinValueLineEdit *>(editor);
        // 如果值为空，默认设置为X
        if (value.isEmpty())
        {
            lineEdit->setText("X");
        }
        else
        {
            lineEdit->setText(value);
        }
    }
    else
    {
        // 其他使用默认设置
        QStyledItemDelegate::setEditorData(editor, index);
    }
}

void VectorTableItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    int column = index.column();

    // 如果是下拉框编辑器
    if ((column == 1) || (column == 2) || (column == 3))
    {
        QComboBox *comboBox = static_cast<QComboBox *>(editor);
        QString value = comboBox->currentText();
        model->setData(index, value, Qt::EditRole);
    }
    // 如果是管脚列（使用PinValueLineEdit）
    else if (column >= 6)
    {
        PinValueLineEdit *lineEdit = static_cast<PinValueLineEdit *>(editor);
        QString value = lineEdit->text();
        // 如果为空，默认使用X
        if (value.isEmpty())
        {
            value = "X";
        }
        model->setData(index, value, Qt::EditRole);
    }
    else
    {
        // 其他使用默认设置
        QStyledItemDelegate::setModelData(editor, model, index);
    }
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

QStringList VectorTableItemDelegate::getTimesetOptions() const
{
    // 不使用缓存，每次都从数据库获取最新的时间集选项
    m_timesetOptions.clear();

    // 从数据库获取时间集选项
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery query(db);
    query.prepare("SELECT timeset_name FROM timeset_list ORDER BY id");
    if (query.exec())
    {
        int count = 0;
        while (query.next())
        {
            QString timesetName = query.value(0).toString();
            m_timesetOptions << timesetName;
            count++;
            qDebug() << "VectorTableItemDelegate::getTimesetOptions - 加载TimeSet:" << timesetName;
        }
        qDebug() << "VectorTableItemDelegate::getTimesetOptions - 总共加载" << count << "个TimeSet选项";
    }
    else
    {
        qWarning() << "获取时间集选项失败:" << query.lastError().text();
    }

    return m_timesetOptions;
}

QStringList VectorTableItemDelegate::getPinOptions() const
{
    // 如果已缓存，则直接返回
    if (!m_pinOptions.isEmpty())
    {
        return m_pinOptions;
    }

    // 从数据库获取管脚选项
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery query(db);
    query.prepare("SELECT pin_value FROM pin_options ORDER BY id");
    if (query.exec())
    {
        while (query.next())
        {
            m_pinOptions << query.value(0).toString();
        }
    }
    else
    {
        qWarning() << "获取管脚选项失败:" << query.lastError().text();
    }

    return m_pinOptions;
}