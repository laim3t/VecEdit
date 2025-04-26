#include "deleterangevectordialog.h"
#include <QDebug>

DeleteRangeVectorDialog::DeleteRangeVectorDialog(QWidget *parent)
    : QDialog(parent), m_maxRow(0)
{
    setWindowTitle("删除指定范围内的向量行");
    setFixedSize(300, 150);

    // 创建布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // 创建最大行标签
    m_maxRowLabel = new QLabel(this);
    mainLayout->addWidget(m_maxRowLabel);

    // 创建输入区域
    QHBoxLayout *fromLayout = new QHBoxLayout();
    m_fromLabel = new QLabel("从：", this);
    m_fromLineEdit = new QLineEdit(this);
    m_fromLineEdit->setValidator(new QIntValidator(1, 999999, this));
    fromLayout->addWidget(m_fromLabel);
    fromLayout->addWidget(m_fromLineEdit);

    QHBoxLayout *toLayout = new QHBoxLayout();
    m_toLabel = new QLabel("到：", this);
    m_toLineEdit = new QLineEdit(this);
    m_toLineEdit->setValidator(new QIntValidator(1, 999999, this));
    toLayout->addWidget(m_toLabel);
    toLayout->addWidget(m_toLineEdit);

    mainLayout->addLayout(fromLayout);
    mainLayout->addLayout(toLayout);

    // 创建按钮
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(m_buttonBox);

    // 连接信号和槽
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // 连接输入验证
    connect(m_fromLineEdit, &QLineEdit::textChanged, this, [this]()
            {
        qDebug() << "DeleteRangeVectorDialog - 起始行改变为：" << m_fromLineEdit->text();
        bool ok;
        int from = m_fromLineEdit->text().toInt(&ok);
        if (ok && from > m_maxRow) {
            m_fromLineEdit->setText(QString::number(m_maxRow));
            qDebug() << "DeleteRangeVectorDialog - 起始行超出最大值，已调整为：" << m_maxRow;
        } });

    connect(m_toLineEdit, &QLineEdit::textChanged, this, [this]()
            {
        qDebug() << "DeleteRangeVectorDialog - 结束行改变为：" << m_toLineEdit->text();
        bool ok;
        int to = m_toLineEdit->text().toInt(&ok);
        if (ok && to > m_maxRow) {
            m_toLineEdit->setText(QString::number(m_maxRow));
            qDebug() << "DeleteRangeVectorDialog - 结束行超出最大值，已调整为：" << m_maxRow;
        } });
}

void DeleteRangeVectorDialog::setMaxRow(int maxRow)
{
    m_maxRow = maxRow;
    m_maxRowLabel->setText(QString("最大行: %1").arg(maxRow));

    // 更新验证器范围
    m_fromLineEdit->setValidator(new QIntValidator(1, maxRow, this));
    m_toLineEdit->setValidator(new QIntValidator(1, maxRow, this));

    qDebug() << "DeleteRangeVectorDialog - 设置最大行为：" << maxRow;
}

void DeleteRangeVectorDialog::setSelectedRange(int from, int to)
{
    m_fromLineEdit->setText(QString::number(from));
    m_toLineEdit->setText(QString::number(to));

    qDebug() << "DeleteRangeVectorDialog - 设置选择范围：" << from << "到" << to;
}

void DeleteRangeVectorDialog::clearSelectedRange()
{
    m_fromLineEdit->clear();
    m_toLineEdit->clear();

    qDebug() << "DeleteRangeVectorDialog - 清空选择范围";
}

int DeleteRangeVectorDialog::getFromRow() const
{
    return m_fromLineEdit->text().toInt();
}

int DeleteRangeVectorDialog::getToRow() const
{
    return m_toLineEdit->text().toInt();
}