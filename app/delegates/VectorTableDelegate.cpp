#include "VectorTableDelegate.h"
#include <QLineEdit>
#include <QComboBox>
#include <QRegularExpressionValidator>
#include <QRegularExpression>
#include <QPainter>
#include <QApplication>
#include <QMouseEvent>
#include <QDebug> // Include for qDebug

VectorTableDelegate::VectorTableDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
    qDebug() << "VectorTableDelegate constructed";
    // Initialize Capture options as per observation (Y/N/Empty)
    m_captureOptions << ""
                     << "Y"
                     << "N";
}

void VectorTableDelegate::setInstructionOptions(const QStringList &options)
{
    qDebug() << "VectorTableDelegate::setInstructionOptions called with" << options.size() << "items";
    m_instructionOptions = options;
}

void VectorTableDelegate::setTimeSetOptions(const QStringList &options)
{
    qDebug() << "VectorTableDelegate::setTimeSetOptions called with" << options.size() << "items";
    m_timeSetOptions = options;
}

void VectorTableDelegate::setCaptureOptions(const QStringList &options)
{
    qDebug() << "VectorTableDelegate::setCaptureOptions called with" << options.size() << "items";
    m_captureOptions = options;
}

QWidget *VectorTableDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    qDebug() << "VectorTableDelegate::createEditor called for column:" << index.column() << "row:" << index.row();

    switch (index.column())
    {
    case LabelColumn:
    {
        QLineEdit *editor = new QLineEdit(parent);
        editor->setMaxLength(16); // Max 16 characters
        // Allow only English letters, numbers, and perhaps underscore/hyphen
        QRegularExpression rx("^[a-zA-Z0-9_-]*$");
        QValidator *validator = new QRegularExpressionValidator(rx, editor);
        editor->setValidator(validator);
        qDebug() << "  Creating QLineEdit for Label column";
        return editor;
    }
    case InstructionColumn:
    {
        QComboBox *editor = new QComboBox(parent);
        editor->addItems(m_instructionOptions);
        qDebug() << "  Creating QComboBox for Instruction column with options:" << m_instructionOptions;
        return editor;
    }
    case TimeSetColumn:
    {
        QComboBox *editor = new QComboBox(parent);
        editor->addItems(m_timeSetOptions);
        qDebug() << "  Creating QComboBox for TimeSet column with options:" << m_timeSetOptions;
        return editor;
    }
    case CaptureColumn:
    {
        QComboBox *editor = new QComboBox(parent);
        // Note: Max 30 char limit on dropdown text is unusual.
        // If custom input is needed, this editor needs modification.
        editor->addItems(m_captureOptions); // Use provided or default options
        qDebug() << "  Creating QComboBox for Capture column with options:" << m_captureOptions;
        return editor;
    }
    case ExTColumn:
        // No editor created here, handled by paint and editorEvent
        qDebug() << "  No editor needed for ExT column (handled by paint/editorEvent)";
        return nullptr;
    case CommentColumn:
    {
        QLineEdit *editor = new QLineEdit(parent);
        // No specific constraints mentioned for Comment
        qDebug() << "  Creating QLineEdit for Comment column";
        return editor;
    }
    default:
        qDebug() << "  Using default editor for column:" << index.column();
        return QStyledItemDelegate::createEditor(parent, option, index);
    }
}

void VectorTableDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    qDebug() << "VectorTableDelegate::setEditorData called for column:" << index.column() << "row:" << index.row();
    QVariant value = index.model()->data(index, Qt::EditRole);

    switch (index.column())
    {
    case LabelColumn:
    case CommentColumn:
    {
        QLineEdit *lineEdit = qobject_cast<QLineEdit *>(editor);
        if (lineEdit)
        {
            lineEdit->setText(value.toString());
            qDebug() << "  Setting QLineEdit data:" << value.toString();
        }
        break;
    }
    case InstructionColumn:
    case TimeSetColumn:
    case CaptureColumn:
    {
        QComboBox *comboBox = qobject_cast<QComboBox *>(editor);
        if (comboBox)
        {
            comboBox->setCurrentText(value.toString());
            qDebug() << "  Setting QComboBox data:" << value.toString();
        }
        break;
    }
    case ExTColumn:
        // No editor data to set
        break;
    default:
        qDebug() << "  Using default setEditorData for column:" << index.column();
        QStyledItemDelegate::setEditorData(editor, index);
        break;
    }
}

void VectorTableDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    qDebug() << "VectorTableDelegate::setModelData called for column:" << index.column() << "row:" << index.row();

    switch (index.column())
    {
    case LabelColumn:
    case CommentColumn:
    {
        QLineEdit *lineEdit = qobject_cast<QLineEdit *>(editor);
        if (lineEdit)
        {
            QString text = lineEdit->text();
            qDebug() << "  Getting QLineEdit data:" << text;
            model->setData(index, text, Qt::EditRole);
        }
        break;
    }
    case InstructionColumn:
    case TimeSetColumn:
    case CaptureColumn:
    {
        QComboBox *comboBox = qobject_cast<QComboBox *>(editor);
        if (comboBox)
        {
            QString text = comboBox->currentText();
            qDebug() << "  Getting QComboBox data:" << text;
            model->setData(index, text, Qt::EditRole);
        }
        break;
    }
    case ExTColumn:
        // No editor data to get, handled by editorEvent
        break;
    default:
        qDebug() << "  Using default setModelData for column:" << index.column();
        QStyledItemDelegate::setModelData(editor, model, index);
        break;
    }
}

void VectorTableDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    qDebug() << "VectorTableDelegate::updateEditorGeometry called for column:" << index.column() << "row:" << index.row();
    editor->setGeometry(option.rect);
}

// Helper function to calculate checkbox rectangle
QRect VectorTableDelegate::getCheckBoxRect(const QStyleOptionViewItem &option) const
{
    // Use QStyle to get the standard checkbox size
    QStyleOptionButton checkboxOpt;
    QRect checkboxRect = QApplication::style()->subElementRect(QStyle::SE_CheckBoxIndicator, &checkboxOpt, nullptr);
    // Center the checkbox in the cell
    QPoint checkboxPoint(option.rect.x() + option.rect.width() / 2 - checkboxRect.width() / 2,
                         option.rect.y() + option.rect.height() / 2 - checkboxRect.height() / 2);
    return QRect(checkboxPoint, checkboxRect.size());
}

void VectorTableDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // No qDebug here as it's called very frequently during painting/resizing
    if (index.column() == ExTColumn)
    {
        bool checked = index.model()->data(index, Qt::DisplayRole).toBool();

        QStyleOptionButton checkboxOption;
        checkboxOption.rect = getCheckBoxRect(option);
        checkboxOption.state = QStyle::State_Enabled;
        if (checked)
        {
            checkboxOption.state |= QStyle::State_On;
        }
        else
        {
            checkboxOption.state |= QStyle::State_Off;
        }

        // Draw the background if selected
        QStyledItemDelegate::paint(painter, option, index); // Draw standard background/selection

        // Draw the checkbox
        QApplication::style()->drawControl(QStyle::CE_CheckBox, &checkboxOption, painter);
    }
    else
    {
        // Default painting for other columns
        QStyledItemDelegate::paint(painter, option, index);
    }
}

bool VectorTableDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    if (index.column() == ExTColumn)
    {
        if (event->type() == QEvent::MouseButtonRelease)
        {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton)
            {
                QRect checkboxRect = getCheckBoxRect(option);
                if (checkboxRect.contains(mouseEvent->pos()))
                {
                    bool checked = index.model()->data(index, Qt::DisplayRole).toBool();
                    qDebug() << "VectorTableDelegate::editorEvent - ExT column clicked, toggling state from:" << checked << "for row:" << index.row();
                    model->setData(index, !checked, Qt::EditRole);
                    return true; // Event handled
                }
            }
        }
        // We handle the event, even if not toggling, to prevent the default editor from appearing on double-click etc.
        // return true;
        // Let's only return true if we actually handled the toggle, allow other events like double-click if needed elsewhere.
        // Fall through to default handling if not a left-click release inside the checkbox bounds.
    }

    // Default event handling for other columns or unhandled events in ExT
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

QSize VectorTableDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (index.column() == ExTColumn)
    {
        QStyleOptionButton checkboxOpt;
        QRect checkboxRect = QApplication::style()->subElementRect(QStyle::SE_CheckBoxIndicator, &checkboxOpt, nullptr);
        // Provide a hint based on checkbox size plus some padding
        return QSize(checkboxRect.width() + 10, checkboxRect.height() + 4);
    }
    return QStyledItemDelegate::sizeHint(option, index);
}