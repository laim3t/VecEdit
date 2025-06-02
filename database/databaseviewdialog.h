#ifndef DATABASEVIEWDIALOG_H
#define DATABASEVIEWDIALOG_H

#include <QDialog>
#include <QTreeWidget>
#include <QTableView>
#include <QSplitter>
#include <QSqlTableModel>
#include <QHeaderView>
#include <QStatusBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include "../common/tablestylemanager.h"

class DatabaseViewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DatabaseViewDialog(QWidget *parent = nullptr);
    ~DatabaseViewDialog();

    // 更新对话框内容，显示当前数据库的表
    void updateDatabaseView();

private slots:
    void onTableTreeItemClicked(QTreeWidgetItem *item, int column);
    void onRefreshButtonClicked();
    void onFilterChanged(int index);
    void executeQuery();
    void exportData();
    void printTable();

private:
    void setupUI();
    void displayTableContent(const QString &tableName);
    void clearDatabaseView();

    // UI组件
    QSplitter *splitter;
    QTreeWidget *tableTreeWidget;
    QTableView *tableView;
    QSqlTableModel *tableModel;
    QStatusBar *statusBar;
    QPushButton *refreshButton;
    QPushButton *closeButton;
    QLabel *dataViewHeader;
    QComboBox *tableFilterCombo;

    // 新增UI组件
    QLabel *dbPathInfo;
    QLabel *selectionInfo;
    QLineEdit *queryInput;
};

#endif // DATABASEVIEWDIALOG_H