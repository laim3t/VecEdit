#ifndef PINLISTDIALOG_H
#define PINLISTDIALOG_H

#include <QDialog>
#include <QStandardItemModel>
#include <QItemDelegate>
#include <QTableView>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QList>

// 管脚数据结构
struct PinData
{
    QString pinName;     // 管脚名称
    int pinCount;        // 管脚数目
    QString displayName; // 显示的创建名称（例如 "A0 - A1"）
};

class PinListDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PinListDialog(QWidget *parent = nullptr);
    ~PinListDialog();

    // 获取用户确认的管脚列表
    QList<QString> getPinNames() const;

private slots:
    void addNewPin();  // 添加新管脚行
    void removePins(); // 移除选中的管脚行
    void onDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    void onAccepted(); // 确认按钮点击处理

private:
    void setupUI();                  // 设置界面
    void updateDisplayName(int row); // 更新显示名称

    QTableView *pinTableView;     // 管脚表格视图
    QStandardItemModel *pinModel; // 管脚数据模型
    QPushButton *addButton;       // 添加按钮
    QPushButton *removeButton;    // 移除按钮
    QDialogButtonBox *buttonBox;  // 确认/取消按钮组

    QList<PinData> pinList;       // 管脚数据列表
    QList<QString> finalPinNames; // 最终要添加到数据库的管脚名称
};

#endif // PINLISTDIALOG_H