#ifndef REPLACETIMESETDIALOG_H
#define REPLACETIMESETDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QList>
#include "timesetdataaccess.h"

class ReplaceTimeSetDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ReplaceTimeSetDialog(QWidget *parent = nullptr);
    ~ReplaceTimeSetDialog();

    // 设置向量表的行数
    void setVectorRowCount(int count);

    // 设置选中范围（用于从选中行自动设置范围）
    void setSelectedRange(int startRow, int endRow);

    // 获取用户选择的数据
    int getFromTimeSetId() const;
    int getToTimeSetId() const;
    int getStartRow() const;
    int getEndRow() const;

private slots:
    void loadTimeSetData();
    void validateInputs();

private:
    void setupUI();

    QComboBox *m_fromTimeSetComboBox;
    QComboBox *m_toTimeSetComboBox;
    QLineEdit *m_startRowEdit;
    QLineEdit *m_endRowEdit;
    QLineEdit *m_rowCountLabel;
    QPushButton *m_okButton;
    QPushButton *m_cancelButton;
    QDialogButtonBox *m_buttonBox;

    QList<TimeSetData> m_timeSetList;
    int m_vectorRowCount;
};

#endif // REPLACETIMESETDIALOG_H