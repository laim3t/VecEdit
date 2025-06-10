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

    // 获取用户选择的数据
    QString getSelectedValue() const;
    int getStartRow() const;
    int getEndRow() const;

private slots:
    void validateInputs();
    void onStartLabelSelected(int index);
    void onEndLabelSelected(int index);

private:
    void setupUI();

    QComboBox *m_valueComboBox;
    QLineEdit *m_startRowEdit;
    QLineEdit *m_endRowEdit;
    QComboBox *m_startLabelCombo;
    QComboBox *m_endLabelCombo;
    QLineEdit *m_maxRowCountLabel;
    QLineEdit *m_executedRowCountLabel;
    QPushButton *m_okButton;
    QPushButton *m_cancelButton;
    QDialogButtonBox *m_buttonBox;

    int m_vectorRowCount;
    QList<QPair<QString, int>> m_labels; // 标签名称和对应行号
};

#endif // FILLVECTORDIALOG_H