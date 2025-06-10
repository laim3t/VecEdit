#include "fillvectordialog.h"
#include "../database/databasemanager.h"
#include <QMessageBox>
#include <QIntValidator>
#include <QSqlQuery>
#include <QSqlError>
#include <QHeaderView>

FillVectorDialog::FillVectorDialog(QWidget *parent)
    : QDialog(parent), m_vectorRowCount(0)
{
    setWindowTitle(tr("填充向量行"));
    setupUI();

    // 添加填充值选项（0,1,L,H,X,S,V,M）
    m_valueComboBox->addItem("0");
    m_valueComboBox->addItem("1");
    m_valueComboBox->addItem("L");
    m_valueComboBox->addItem("H");
    m_valueComboBox->addItem("X");
    m_valueComboBox->addItem("S");
    m_valueComboBox->addItem("V");
    m_valueComboBox->addItem("M");

    // 连接信号和槽
    connect(m_startRowEdit, &QLineEdit::textChanged, this, &FillVectorDialog::validateInputs);
    connect(m_endRowEdit, &QLineEdit::textChanged, this, &FillVectorDialog::validateInputs);
    connect(m_startLabelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FillVectorDialog::onStartLabelSelected);
    connect(m_endLabelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FillVectorDialog::onEndLabelSelected);
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
    m_valueComboBox->setVisible(false); // 隐藏单值填充控件，改为使用模式表格
    formLayout->addRow(tr("填充值:"), m_valueComboBox);

    // 从行输入框组合布局 (从1开始)
    QHBoxLayout *startRowLayout = new QHBoxLayout();
    m_startRowEdit = new QLineEdit(this);
    m_startRowEdit->setValidator(new QIntValidator(1, 9999, this));
    m_startLabelCombo = new QComboBox(this);
    m_startLabelCombo->addItem(tr("选择标签"));
    startRowLayout->addWidget(m_startRowEdit);
    startRowLayout->addWidget(m_startLabelCombo);
    formLayout->addRow(tr("从:"), startRowLayout);

    // 到行输入框组合布局 (从1开始)
    QHBoxLayout *endRowLayout = new QHBoxLayout();
    m_endRowEdit = new QLineEdit(this);
    m_endRowEdit->setValidator(new QIntValidator(1, 9999, this));
    m_endLabelCombo = new QComboBox(this);
    m_endLabelCombo->addItem(tr("选择标签"));
    endRowLayout->addWidget(m_endRowEdit);
    endRowLayout->addWidget(m_endLabelCombo);
    formLayout->addRow(tr("到:"), endRowLayout);

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

    // 添加模式编辑表格
    QLabel *patternLabel = new QLabel(tr("填充模式:"), this);
    mainLayout->addWidget(patternLabel);

    m_patternTableWidget = new QTableWidget(0, 1, this);
    m_patternTableWidget->setHorizontalHeaderLabels(QStringList() << tr("值"));
    m_patternTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_patternTableWidget->setMinimumHeight(150);
    mainLayout->addWidget(m_patternTableWidget);

    // 创建按钮
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okButton = m_buttonBox->button(QDialogButtonBox::Ok);
    m_okButton->setText(tr("确定"));
    m_cancelButton = m_buttonBox->button(QDialogButtonBox::Cancel);
    m_cancelButton->setText(tr("取消"));

    // 添加按钮到主布局
    mainLayout->addWidget(m_buttonBox);

    // 连接按钮信号
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, [this]()
            {
        if (isMultipleOfPattern()) {
            accept();
        } else {
            QMessageBox::warning(this, tr("向量填充编辑器"), 
                tr("添加的行数应当是填充模式中行数的整数倍。"));
        } });
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // 设置固定大小
    setFixedSize(400, 450);
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

bool FillVectorDialog::isValidFillRange() const
{
    bool startValid = false;
    bool endValid = false;

    int start = m_startRowEdit->text().toInt(&startValid);
    int end = m_endRowEdit->text().toInt(&endValid);

    return startValid && endValid && start >= 1 && end >= start;
}

bool FillVectorDialog::isMultipleOfPattern() const
{
    // 如果没有模式行，任何填充都是有效的
    int patternRowCount = m_patternTableWidget->rowCount();
    if (patternRowCount <= 0)
    {
        return true;
    }

    // 获取填充范围行数
    bool startValid = false;
    bool endValid = false;
    int start = m_startRowEdit->text().toInt(&startValid);
    int end = m_endRowEdit->text().toInt(&endValid);

    if (!startValid || !endValid || start < 1 || end < start)
    {
        return false;
    }

    int fillRowCount = end - start + 1;

    // 检查是否为模式行数的整数倍
    return (fillRowCount % patternRowCount) == 0;
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

void FillVectorDialog::setLabelList(const QList<QPair<QString, int>> &labels)
{
    m_labels = labels;

    // 先清除旧数据
    m_startLabelCombo->clear();
    m_endLabelCombo->clear();

    // 添加初始选项
    m_startLabelCombo->addItem(tr("选择标签"));
    m_endLabelCombo->addItem(tr("选择标签"));

    // 填充标签下拉框
    for (const auto &label : m_labels)
    {
        QString displayText = QString("%1").arg(label.first);
        m_startLabelCombo->addItem(displayText, label.second);
        m_endLabelCombo->addItem(displayText, label.second);
    }
}

void FillVectorDialog::setSelectedCellsData(const QList<QString> &values)
{
    // 清空表格
    m_patternTableWidget->setRowCount(0);

    // 添加每个值作为模式的一行
    for (int i = 0; i < values.size(); ++i)
    {
        m_patternTableWidget->insertRow(i);
        QTableWidgetItem *item = new QTableWidgetItem(values[i]);
        m_patternTableWidget->setItem(i, 0, item);
    }
}

void FillVectorDialog::onStartLabelSelected(int index)
{
    if (index <= 0)
        return; // 忽略第一个"选择标签"项或无效项

    // 获取行号并填入输入框
    int row = m_startLabelCombo->itemData(index).toInt();
    m_startRowEdit->setText(QString::number(row));

    validateInputs();
}

void FillVectorDialog::onEndLabelSelected(int index)
{
    if (index <= 0)
        return; // 忽略第一个"选择标签"项或无效项

    // 获取行号并填入输入框
    int row = m_endLabelCombo->itemData(index).toInt();
    m_endRowEdit->setText(QString::number(row));

    validateInputs();
}

QList<QString> FillVectorDialog::getPatternValues() const
{
    QList<QString> values;
    for (int i = 0; i < m_patternTableWidget->rowCount(); ++i)
    {
        QTableWidgetItem *item = m_patternTableWidget->item(i, 0);
        if (item)
        {
            values.append(item->text());
        }
        else
        {
            values.append(""); // 空白单元格
        }
    }
    return values;
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