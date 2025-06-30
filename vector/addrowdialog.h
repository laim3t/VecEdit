#ifndef ADDROWDIALOG_H
#define ADDROWDIALOG_H

#include <QDialog>

// 前向声明
class QLabel;
class QSpinBox;
class QDialogButtonBox;

/**
 * @brief AddRowDialog 类为一个简单的对话框，用于获取用户想要添加的行数。
 *
 * 该对话框为新架构 (Model/View) 设计，取代了旧的、复杂的向量行数据录入对话框。
 * 它只负责收集用户输入，不包含任何业务逻辑。
 */
class AddRowDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddRowDialog(QWidget *parent = nullptr);
    ~AddRowDialog() override;

    /**
     * @brief 获取用户在SpinBox中输入的行数。
     * @return 用户要添加的行数。
     */
    int getRowCount() const;

private:
    void setupUi();

    QLabel *m_infoLabel;
    QSpinBox *m_rowCountSpinBox;
    QDialogButtonBox *m_buttonBox;
};

#endif // ADDROWDIALOG_H