#ifndef TIMESETEDGEDIALOG_H
#define TIMESETEDGEDIALOG_H

#include <QDialog>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QMap>

// 时间边沿参数编辑对话框
class TimeSetEdgeDialog : public QDialog
{
    Q_OBJECT

public:
    // 构造函数，接收初始值和波形选项
    explicit TimeSetEdgeDialog(double defaultT1R, double defaultT1F, double defaultSTBR,
                               int defaultWaveId, const QMap<int, QString> &waveOptions,
                               QWidget *parent = nullptr);

    // 获取用户设置的参数值
    double getT1R() const { return t1rSpinBox->value(); }
    double getT1F() const { return t1fSpinBox->value(); }
    double getSTBR() const { return stbrSpinBox->value(); }
    int getWaveId() const;

private slots:
    void onAccepted();
    void onRejected();

private:
    void setupUI();

    QDoubleSpinBox *t1rSpinBox;
    QDoubleSpinBox *t1fSpinBox;
    QDoubleSpinBox *stbrSpinBox;
    QComboBox *waveComboBox;
    QDialogButtonBox *buttonBox;

    // 波形选项映射表
    QMap<int, QString> m_waveOptions;
};

#endif // TIMESETEDGEDIALOG_H