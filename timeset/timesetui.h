#ifndef TIMESETUI_H
#define TIMESETUI_H

#include <QDialog>
#include <QTreeWidget>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QSplitter>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>

class TimeSetUIManager
{
public:
    TimeSetUIManager(QDialog *dialog);

    // UI初始化方法
    void setupMainLayout();
    void setupTreeWidget();
    void setupPinSelection();
    void setupButtonBox();

    // 获取UI组件
    QTreeWidget *getTimeSetTree() const { return timeSetTree; }
    QListWidget *getPinListWidget() const { return availablePinsList; }
    QPushButton *getAddTimeSetButton() const { return addTimeSetButton; }
    QPushButton *getRemoveTimeSetButton() const { return removeTimeSetButton; }
    QPushButton *getAddEdgeButton() const { return addEdgeButton; }
    QPushButton *getRemoveEdgeButton() const { return removeEdgeButton; }
    QDialogButtonBox *getButtonBox() const { return buttonBox; }
    QSplitter *getMainSplitter() const { return mainSplitter; }

private:
    // 对话框引用
    QDialog *m_dialog;

    // UI组件
    QSplitter *mainSplitter;
    QTreeWidget *timeSetTree;
    QListWidget *availablePinsList;
    QPushButton *addTimeSetButton;
    QPushButton *removeTimeSetButton;
    QPushButton *addEdgeButton;
    QPushButton *removeEdgeButton;
    QDialogButtonBox *buttonBox;
};

#endif // TIMESETUI_H