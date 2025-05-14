#ifndef VECTORTABLEDELEGATE_H
#define VECTORTABLEDELEGATE_H

#include <QStyledItemDelegate>
#include <QStringList>
#include <QObject> // Include QObject for qDebug

class VectorTableDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    // Enum to easily identify columns by name, improves readability
    enum ColumnIndex
    {
        LabelColumn = 0,
        InstructionColumn,
        TimeSetColumn,
        CaptureColumn,
        ExTColumn,
        CommentColumn
        // Add more columns here if needed
    };

    explicit VectorTableDelegate(QObject *parent = nullptr);

    // Set data for dropdown lists
    void setInstructionOptions(const QStringList &options);
    void setTimeSetOptions(const QStringList &options);
    void setCaptureOptions(const QStringList &options); // Added for Capture column

    // --- Reimplemented virtual functions from QStyledItemDelegate ---

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;

    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const override;

    // For custom painting, especially the ExT checkbox
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

    // To handle clicks on the ExT checkbox
    bool editorEvent(QEvent *event, QAbstractItemModel *model,
                     const QStyleOptionViewItem &option, const QModelIndex &index) override;

    // Provide hints for size, useful for checkbox column
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    QStringList m_instructionOptions;
    QStringList m_timeSetOptions;
    QStringList m_captureOptions; // Added for Capture column

    // Helper to get checkbox rect
    QRect getCheckBoxRect(const QStyleOptionViewItem &option) const;
};

#endif // VECTORTABLEDELEGATE_H