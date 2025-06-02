#ifndef DELETERANGEVECTORDIALOG_H
#define DELETERANGEVECTORDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QIntValidator>

class DeleteRangeVectorDialog : public QDialog
{
    Q_OBJECT

public:
    DeleteRangeVectorDialog(QWidget *parent = nullptr);

    // 设置最大行数
    void setMaxRow(int maxRow);

    // 设置选中的行范围
    void setSelectedRange(int from, int to);

    // 清空选中的行范围
    void clearSelectedRange();

    // 获取删除的范围
    int getFromRow() const;
    int getToRow() const;

private:
    QLabel *m_maxRowLabel;
    QLabel *m_fromLabel;
    QLabel *m_toLabel;
    QLineEdit *m_fromLineEdit;
    QLineEdit *m_toLineEdit;
    QDialogButtonBox *m_buttonBox;
    int m_maxRow;
};

#endif // DELETERANGEVECTORDIALOG_H