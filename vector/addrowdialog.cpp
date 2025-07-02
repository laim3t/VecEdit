#include "addrowdialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QDialogButtonBox>

AddRowDialog::AddRowDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &AddRowDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &AddRowDialog::reject);
}

AddRowDialog::~AddRowDialog()
{
}

void AddRowDialog::setupUi()
{
    setWindowTitle(tr("添加向量行"));
    setMinimumWidth(300);
    setModal(true); // 设置为模态对话框

    // 创建布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QFormLayout *formLayout = new QFormLayout();

    // 创建控件
    m_infoLabel = new QLabel(tr("请输入要添加的新行数："), this);
    m_rowCountSpinBox = new QSpinBox(this);
    m_rowCountSpinBox->setRange(1, 9999999); // 将最大值提升到 9,999,999
    m_rowCountSpinBox->setValue(1);          // 将默认值设置为 1

    // 将控件添加到布局
    formLayout->addRow(m_infoLabel, m_rowCountSpinBox);
    mainLayout->addLayout(formLayout);

    // 创建标准按钮
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(m_buttonBox);

    setLayout(mainLayout);
}

int AddRowDialog::getRowCount() const
{
    return m_rowCountSpinBox->value();
}