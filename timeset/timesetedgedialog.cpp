#include "timesetedgedialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QMessageBox>

TimeSetEdgeDialog::TimeSetEdgeDialog(double defaultT1R, double defaultT1F, double defaultSTBR,
                                     int defaultWaveId, const QMap<int, QString> &waveOptions,
                                     QWidget *parent)
    : QDialog(parent),
      m_waveOptions(waveOptions)
{
    setWindowTitle("编辑时间边沿参数");
    setMinimumWidth(300);

    setupUI();

    // 设置默认值
    t1rSpinBox->setValue(defaultT1R);
    t1fSpinBox->setValue(defaultT1F);
    stbrSpinBox->setValue(defaultSTBR);

    // 设置波形选项
    int currentIndex = 0;
    int index = 0;
    for (auto it = m_waveOptions.begin(); it != m_waveOptions.end(); ++it)
    {
        waveComboBox->addItem(it.value(), it.key());
        if (it.key() == defaultWaveId)
        {
            currentIndex = index;
        }
        index++;
    }

    if (!m_waveOptions.isEmpty())
    {
        waveComboBox->setCurrentIndex(currentIndex);
    }
}

void TimeSetEdgeDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // 创建表单布局
    QFormLayout *formLayout = new QFormLayout();

    // T1R参数
    t1rSpinBox = new QDoubleSpinBox(this);
    t1rSpinBox->setRange(0.0, 10000.0);
    t1rSpinBox->setSingleStep(1.0);
    t1rSpinBox->setDecimals(1);
    t1rSpinBox->setValue(250.0);
    formLayout->addRow("T1R:", t1rSpinBox);

    // T1F参数
    t1fSpinBox = new QDoubleSpinBox(this);
    t1fSpinBox->setRange(0.0, 10000.0);
    t1fSpinBox->setSingleStep(1.0);
    t1fSpinBox->setDecimals(1);
    t1fSpinBox->setValue(750.0);
    formLayout->addRow("T1F:", t1fSpinBox);

    // STBR参数
    stbrSpinBox = new QDoubleSpinBox(this);
    stbrSpinBox->setRange(0.0, 10000.0);
    stbrSpinBox->setSingleStep(1.0);
    stbrSpinBox->setDecimals(1);
    stbrSpinBox->setValue(500.0);
    formLayout->addRow("STBR:", stbrSpinBox);

    // WAVE参数（下拉框）
    waveComboBox = new QComboBox(this);
    formLayout->addRow("波形类型:", waveComboBox);

    mainLayout->addLayout(formLayout);

    // 对话框按钮
    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &TimeSetEdgeDialog::onAccepted);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &TimeSetEdgeDialog::onRejected);

    mainLayout->addWidget(buttonBox);
}

int TimeSetEdgeDialog::getWaveId() const
{
    return waveComboBox->currentData().toInt();
}

void TimeSetEdgeDialog::onAccepted()
{
    // 验证输入
    if (t1rSpinBox->value() < 0 || t1fSpinBox->value() < 0 || stbrSpinBox->value() < 0)
    {
        QMessageBox::warning(this, "输入错误", "所有参数值必须大于或等于0");
        return;
    }

    if (t1fSpinBox->value() <= t1rSpinBox->value())
    {
        QMessageBox::warning(this, "输入错误", "T1F值必须大于T1R值");
        return;
    }

    if (waveComboBox->count() == 0)
    {
        QMessageBox::warning(this, "输入错误", "请选择一个波形类型");
        return;
    }

    accept();
}

void TimeSetEdgeDialog::onRejected()
{
    reject();
}