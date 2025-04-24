#ifndef DIALOGMANAGER_H
#define DIALOGMANAGER_H

#include <QString>
#include <QList>
#include <QWidget>
#include <QDialog>
#include <QTableWidget>

class DialogManager
{
public:
    DialogManager(QWidget *parent = nullptr);

    // 显示管脚选择对话框
    bool showPinSelectionDialog(int tableId, const QString &tableName);

    // 显示向量行数据录入对话框
    bool showVectorDataDialog(int tableId, const QString &tableName, int startIndex = 0);

    // 显示添加管脚对话框
    bool showAddPinsDialog();

    // 显示TimeSet对话框
    bool showTimeSetDialog(bool isInitialSetup = false);

    // 显示数据库视图对话框
    void showDatabaseViewDialog();

private:
    QWidget *m_parent;
};

#endif // DIALOGMANAGER_H