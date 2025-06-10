#ifndef FILLVECTORDIALOG_H
#define FILLVECTORDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QList>
#include <QPair>
#include <QTableWidget>
#include <QStyledItemDelegate>
#include <QDebug>

// 用于模式表格的自定义单元格委托，限制输入和自动转换大写
class PatternTableItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    PatternTableItemDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QLineEdit *lineEdit = new QLineEdit(parent);
        lineEdit->setMaxLength(1); // 限制输入一个字符
        lineEdit->setAlignment(Qt::AlignCenter);
        lineEdit->setToolTip("输入提示：0,1,L,l,H,h,X,x,S,s,V,v,M,m；默认：X");
        return lineEdit;
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override
    {
        QLineEdit *lineEdit = qobject_cast<QLineEdit *>(editor);
        if (!lineEdit)
            return;

        QString value = lineEdit->text();
        if (value.isEmpty())
        {
            value = "X"; // 默认值为X
        }

        // 验证字符是否合法
        QString validChars = "01LHXSVMlhxsvm";
        if (validChars.contains(value.at(0)))
        {
            // 将小写字母转为大写
            value = value.toUpper();
            model->setData(index, value, Qt::EditRole);
        }
    }
};

class FillVectorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FillVectorDialog(QWidget *parent = nullptr);
    ~FillVectorDialog();

    // 设置向量表的行数
    void setVectorRowCount(int count);

    // 设置选中范围（用于从选中行自动设置范围）
    void setSelectedRange(int startRow, int endRow);

    // 设置标签列表
    void setLabelList(const QList<QPair<QString, int>> &labels);

    // 设置选中的单元格数据（用于初始化模式表格）
    void setSelectedCellsData(const QList<QString> &values);

    // 获取用户选择的数据
    QList<QString> getPatternValues() const; // 获取模式值列表
    int getStartRow() const;
    int getEndRow() const;

private slots:
    void validateInputs();
    void onStartLabelSelected(int index);
    void onEndLabelSelected(int index);
    void addPatternRow();
    void deleteSelectedRows();

private:
    void setupUI();
    bool isValidFillRange() const;    // 检查填充范围是否合法
    bool isMultipleOfPattern() const; // 检查是否为模式行数的整数倍
    void initializeTable();
    int startRow() const;

    QComboBox *m_valueComboBox; // 单值填充选择（保留原功能）
    QLineEdit *m_startRowEdit;
    QLineEdit *m_endRowEdit;
    QComboBox *m_startLabelCombo;
    QComboBox *m_endLabelCombo;
    QLineEdit *m_maxRowCountLabel;
    QLineEdit *m_executedRowCountLabel;
    QPushButton *m_okButton;
    QPushButton *m_cancelButton;
    QDialogButtonBox *m_buttonBox;
    QTableWidget *m_patternTableWidget;          // 模式编辑表格
    PatternTableItemDelegate *m_patternDelegate; // 模式表格的自定义委托

    int m_vectorRowCount;
    QList<QPair<QString, int>> m_labels; // 标签名称和对应行号
};

#endif // FILLVECTORDIALOG_H