#include "fillvectordialog.h"
#include "../database/databasemanager.h"
#include <QMessageBox>
#include <QIntValidator>
#include <QSqlQuery>
#include <QSqlError>

FillVectorDialog::FillVectorDialog(QWidget *parent)
    : QDialog(parent), m_vectorRowCount(0)
{
    setWindowTitle(tr("填充向量行"));
    setupUI();

    // 添加填充值选项（0,1,L,H,Z,X）
    m_valueComboBox->addItem("0");
    m_valueComboBox->addItem("1");
    m_valueComboBox->addItem("L");
    m_valueComboBox->addItem("H");
    m_valueComboBox->addItem("Z");
    m_valueComboBox->addItem("X");

    // 连接信号和槽
    connect(m_startRowEdit, &QLineEdit::textChanged, this, &FillVectorDialog::validateInputs);
    connect(m_endRowEdit, &QLineEdit::textChanged, this, &FillVectorDialog::validateInputs);
}

FillVectorDialog::~FillVectorDialog()
{
}

void FillVectorDialog::setupUI()
{
    // 创建主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // 创建表单布局
    QFormLayout *formLayout = new QFormLayout();

    // 填充值下拉框
    m_valueComboBox = new QComboBox(this);
    formLayout->addRow(tr("填充值:"), m_valueComboBox);

    // 从行输入框 (从1开始)
    m_startRowEdit = new QLineEdit(this);
    m_startRowEdit->setValidator(new QIntValidator(1, 9999, this));
    formLayout->addRow(tr("从:"), m_startRowEdit);

    // 到行输入框 (从1开始)
    m_endRowEdit = new QLineEdit(this);
    m_endRowEdit->setValidator(new QIntValidator(1, 9999, this));
    formLayout->addRow(tr("到:"), m_endRowEdit);

    // 最大行数显示（只读）
    m_maxRowCountLabel = new QLineEdit(this);
    m_maxRowCountLabel->setReadOnly(true);
    m_maxRowCountLabel->setStyleSheet("background-color: #f0f0f0;");
    formLayout->addRow(tr("最大行数:"), m_maxRowCountLabel);

    // 执行行数显示（只读）
    m_executedRowCountLabel = new QLineEdit(this);
    m_executedRowCountLabel->setReadOnly(true);
    m_executedRowCountLabel->setStyleSheet("background-color: #f0f0f0;");
    formLayout->addRow(tr("执行行数:"), m_executedRowCountLabel);

    // 添加表单布局到主布局
    mainLayout->addLayout(formLayout);

    // 创建按钮
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okButton = m_buttonBox->button(QDialogButtonBox::Ok);
    m_okButton->setText(tr("确定"));
    m_cancelButton = m_buttonBox->button(QDialogButtonBox::Cancel);
    m_cancelButton->setText(tr("取消"));

    // 添加按钮到主布局
    mainLayout->addWidget(m_buttonBox);

    // 连接按钮信号
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // 设置固定大小
    setFixedSize(300, 220);
}

void FillVectorDialog::validateInputs()
{
    bool startValid = false;
    bool endValid = false;

    int start = m_startRowEdit->text().toInt(&startValid);
    int end = m_endRowEdit->text().toInt(&endValid);

    // 更新执行行数
    if (startValid && endValid && end >= start)
    {
        m_executedRowCountLabel->setText(QString::number(end - start + 1));
    }
    else
    {
        m_executedRowCountLabel->setText("0");
    }

    // 验证用户输入的值（1-based）
    bool isValid = startValid && endValid &&
                   start >= 1 && end >= start;

    // 如果行数超过向量表的行数，显示警告但不禁用确定按钮
    if (m_vectorRowCount > 0 && end > m_vectorRowCount)
    {
        m_endRowEdit->setStyleSheet("background-color: #FFEEEE;");
    }
    else
    {
        m_endRowEdit->setStyleSheet("");
    }

    m_okButton->setEnabled(isValid);
}

void FillVectorDialog::setVectorRowCount(int count)
{
    m_vectorRowCount = count;

    // 设置默认值（以用户友好的方式显示行号，从1开始）
    if (count > 0)
    {
        m_startRowEdit->setText("1");                  // 从第1行开始（而不是0）
        m_endRowEdit->setText(QString::number(count)); // 到第count行结束
        m_maxRowCountLabel->setText(QString::number(count));
    }

    validateInputs();
}

void FillVectorDialog::setSelectedRange(int startRow, int endRow)
{
    // 设置选中的范围作为起始和结束行
    // 注意：输入参数已经是1-based的行号

    // 确保不超出范围
    if (m_vectorRowCount > 0)
    {
        startRow = qMax(1, qMin(startRow, m_vectorRowCount));
        endRow = qMax(startRow, qMin(endRow, m_vectorRowCount));
    }

    m_startRowEdit->setText(QString::number(startRow));
    m_endRowEdit->setText(QString::number(endRow));

    // 行数保持显示向量表总行数，不随选中行数变化
    m_maxRowCountLabel->setText(QString::number(m_vectorRowCount));

    validateInputs();
}

QString FillVectorDialog::getSelectedValue() const
{
    return m_valueComboBox->currentText();
}

int FillVectorDialog::getStartRow() const
{
    // 将用户输入的行号（1-based）转换为数据库索引（0-based）
    return m_startRowEdit->text().toInt() - 1;
}

int FillVectorDialog::getEndRow() const
{
    // 将用户输入的行号（1-based）转换为数据库索引（0-based）
    return m_endRowEdit->text().toInt() - 1;
}