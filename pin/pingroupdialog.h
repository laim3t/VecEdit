#ifndef PINGROUPDIALOG_H
#define PINGROUPDIALOG_H

#include <QDialog>
#include <QMap>
#include <QSet>
#include <QList>
#include <QString>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>

class PinGroupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PinGroupDialog(QWidget *parent = nullptr);
    ~PinGroupDialog();

private slots:
    void onVectorTableSelectionChanged(int index);
    void onPinTypeFilterChanged(const QString &type);
    void onPinSelectionChanged(bool checked);
    void onAccepted();
    void onRejected();

private:
    void setupUI();
    void loadAllVectorTables();
    void loadVectorTablePins(int tableId);
    void filterPinsByType(const QString &type);
    void clearPinSelection();
    bool saveGroupData();

    QComboBox *m_vectorTableComboBox;
    QTableWidget *m_leftTable;    // 左侧向量表列表（带勾选框）
    QTableWidget *m_rightTable;   // 右侧管脚列表（带勾选框）
    QComboBox *m_pinTypeComboBox; // 管脚类型筛选
    QLineEdit *m_groupNameEdit;   // 分组名称输入框
    QPushButton *m_okButton;
    QPushButton *m_cancelButton;

    int m_currentTableId;                     // 当前选中的向量表ID
    QMap<QString, QList<int>> m_pinNameToIds; // 管脚名称到ID的映射
    QMap<int, QString> m_pinIdToType;         // 管脚ID到类型的映射
    QSet<int> m_selectedPinIds;               // 选中的管脚ID
    bool m_isLoading;                         // 防止递归触发信号
};

#endif // PINGROUPDIALOG_H