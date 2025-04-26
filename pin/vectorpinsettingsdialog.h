#ifndef VECTORPINSETTINGSDIALOG_H
#define VECTORPINSETTINGSDIALOG_H

#include <QDialog>
#include <QMap>
#include <QSet>
#include <QTableWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <stdexcept>
#include "../common/tablestylemanager.h"

// 向量表管脚设置对话框
class VectorPinSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit VectorPinSettingsDialog(int tableId, const QString &tableName, QWidget *parent = nullptr);
    ~VectorPinSettingsDialog();

    // 获取选中的管脚ID和类型
    QMap<int, QString> getSelectedPinsWithTypes() const;

private slots:
    void onCheckBoxStateChanged(int state);
    void onAccepted();
    void onRejected();

private:
    void setupUI();
    void loadPinsData();
    bool hasPinData(int pinId); // 检查管脚是否有数据

    int m_tableId;
    QString m_tableName;

    QTableWidget *m_pinsTable;
    QPushButton *m_okButton;
    QPushButton *m_cancelButton;

    QMap<int, QString> m_allPins;            // 所有管脚，键为管脚ID，值为管脚名
    QMap<int, QString> m_selectedPins;       // 原始选中管脚，键为管脚ID，值为类型
    QMap<int, QCheckBox *> m_checkBoxes;     // 存储复选框对象
    QMap<int, QComboBox *> m_typeComboBoxes; // 存储类型下拉框对象

    QSet<int> m_pinsWithData; // 存储已有数据的管脚ID
};

#endif // VECTORPINSETTINGSDIALOG_H