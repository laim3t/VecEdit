#include "filltimesetdialog.h"
#include "database/databasemanager.h"
#include <QMessageBox>
#include <QIntValidator>

FillTimeSetDialog::FillTimeSetDialog(QWidget *parent)
    : QDialog(parent), m_vectorRowCount(0)
{
    setWindowTitle(tr("填充TimeSet"));
    setupUI();
    loadTimeSetData();

    // 连接信号和槽
    connect(m_startRowEdit, &QLineEdit::textChanged, this, &FillTimeSetDialog::validateInputs);
    connect(m_endRowEdit, &QLineEdit::textChanged, this, &FillTimeSetDialog::validateInputs);
    connect(m_stepValueEdit, &QLineEdit::textChanged, this, &FillTimeSetDialog::validateInputs);
}

FillTimeSetDialog::~FillTimeSetDialog()
{
}

void FillTimeSetDialog::setupUI()
{
    // 创建主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // 创建表单布局
    QFormLayout *formLayout = new QFormLayout();

    // TimeSet下拉框
    m_timeSetComboBox = new QComboBox(this);
    formLayout->addRow(tr("TimeSet:"), m_timeSetComboBox);

    // 从行输入框
    m_startRowEdit = new QLineEdit(this);
    m_startRowEdit->setValidator(new QIntValidator(0, 9999, this));
    formLayout->addRow(tr("从:"), m_startRowEdit);

    // 到行输入框
    m_endRowEdit = new QLineEdit(this);
    m_endRowEdit->setValidator(new QIntValidator(0, 9999, this));
    formLayout->addRow(tr("到:"), m_endRowEdit);

    // 行数显示（只读）
    m_rowCountLabel = new QLineEdit(this);
    m_rowCountLabel->setReadOnly(true);
    m_rowCountLabel->setStyleSheet("background-color: #f0f0f0;");
    formLayout->addRow(tr("行数:"), m_rowCountLabel);

    // 步长输入框
    m_stepValueEdit = new QLineEdit(this);
    m_stepValueEdit->setValidator(new QIntValidator(1, 9999, this));
    m_stepValueEdit->setText("1"); // 默认值为1，表示每行都填充
    formLayout->addRow(tr("步长:"), m_stepValueEdit);

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

void FillTimeSetDialog::loadTimeSetData()
{
    // 清空下拉框
    m_timeSetComboBox->clear();

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();

    // 加载TimeSet数据
    TimeSetDataAccess dataAccess(db);
    m_timeSetList = dataAccess.loadExistingTimeSets();

    // 添加到下拉框
    for (const TimeSetData &timeSet : m_timeSetList)
    {
        m_timeSetComboBox->addItem(timeSet.name, timeSet.dbId);
    }

    // 初始值验证
    validateInputs();
}

void FillTimeSetDialog::validateInputs()
{
    bool startValid = false;
    bool endValid = false;
    bool stepValid = false;

    int start = m_startRowEdit->text().toInt(&startValid);
    int end = m_endRowEdit->text().toInt(&endValid);
    int step = m_stepValueEdit->text().toInt(&stepValid);

    bool isValid = startValid && endValid && stepValid &&
                   start >= 0 && end >= start &&
                   step > 0 &&
                   m_timeSetComboBox->count() > 0;

    // 如果行数超过向量表的行数，显示警告但不禁用确定按钮
    if (m_vectorRowCount > 0 && end >= m_vectorRowCount)
    {
        m_endRowEdit->setStyleSheet("background-color: #FFEEEE;");
    }
    else
    {
        m_endRowEdit->setStyleSheet("");
    }

    m_okButton->setEnabled(isValid);
}

void FillTimeSetDialog::setVectorRowCount(int count)
{
    m_vectorRowCount = count;

    // 设置默认值
    if (count > 0)
    {
        m_startRowEdit->setText("0");
        m_endRowEdit->setText(QString::number(count - 1));
        m_rowCountLabel->setText(QString::number(count));
    }

    validateInputs();
}

int FillTimeSetDialog::getSelectedTimeSetId() const
{
    return m_timeSetComboBox->currentData().toInt();
}

int FillTimeSetDialog::getStartRow() const
{
    return m_startRowEdit->text().toInt();
}

int FillTimeSetDialog::getEndRow() const
{
    return m_endRowEdit->text().toInt();
}

int FillTimeSetDialog::getStepValue() const
{
    return m_stepValueEdit->text().toInt();
}