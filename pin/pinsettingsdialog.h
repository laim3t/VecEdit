#ifndef PINSETTINGSDIALOG_H
#define PINSETTINGSDIALOG_H

#include <QDialog>
#include <QMap>
#include <QList>
#include <QPair>
#include <QTableWidget>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollBar>

// 管脚设置对话框类，用于设置管脚的工位和通道
class PinSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PinSettingsDialog(QWidget *parent = nullptr);
    ~PinSettingsDialog();

    // 直接显示删除管脚对话框，而不显示管脚设置界面
    void showDeletePinDialog();

private slots:
    void onStationCountChanged(int value);
    void onAccepted();
    void onRejected();
    void onAddPin();    // 添加管脚功能
    void onDeletePin(); // 删除管脚功能

private:
    void setupUI();
    void loadPinsData();
    void updateTable();
    bool saveSettings();
    bool isDataModified();          // 检查数据是否有修改
    bool checkBitIndexUniqueness(); // 检查位索引值是否在每个工位上唯一

    QSpinBox *m_stationCountSpinBox;
    QTableWidget *m_pinSettingsTable;
    QPushButton *m_okButton;
    QPushButton *m_cancelButton;
    QPushButton *m_addPinButton;    // 添加管脚按钮
    QPushButton *m_deletePinButton; // 删除管脚按钮

    int m_currentStationCount; // 当前工位数

    // 保存数据库中已有的管脚设置
    // 格式: <pin_id, <station_number, station_bit_index>>
    QMap<int, QMap<int, int>> m_existingSettings;

    // 保存所有管脚信息
    // 格式: <pin_id, pin_name>
    QMap<int, QString> m_allPins;

    // 保存管脚通道数
    // 格式: <pin_id, channel_count>
    QMap<int, int> m_channelCounts;

    // 保存管脚注释
    // 格式: <pin_id, pin_note>
    QMap<int, QString> m_pinNotes;
};

#endif // PINSETTINGSDIALOG_H