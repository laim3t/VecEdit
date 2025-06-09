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

    // 获取用户选择的数据
    QString getSelectedValue() const;
    int getStartRow() const;
    int getEndRow() const;

private slots:
    void validateInputs();

private:
    void setupUI();

    QComboBox *m_valueComboBox;
    QLineEdit *m_startRowEdit;
    QLineEdit *m_endRowEdit;
    QLineEdit *m_maxRowCountLabel;
    QLineEdit *m_executedRowCountLabel;
    QPushButton *m_okButton;
    QPushButton *m_cancelButton;
    QDialogButtonBox *m_buttonBox;

    int m_vectorRowCount;
};

#endif // FILLVECTORDIALOG_H