#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QDir>
#include <QTableWidget>
#include <QComboBox>
#include <QPushButton>

// 前置声明
class VectorTableItemDelegate;
class VectorDataHandler;
class DialogManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // 辅助函数：添加向量行
    void addVectorRow(QTableWidget *table, const QStringList &pinOptions, int rowIdx);

private slots:
    // 数据库操作
    void createNewProject();
    void openExistingProject();
    void closeCurrentProject();

    // 显示数据库视图对话框
    void showDatabaseViewDialog();

    // 显示添加管脚对话框
    bool showAddPinsDialog();

    // 向数据库添加管脚
    bool addPinsToDatabase(const QList<QString> &pinNames);

    // 显示TimeSet对话框
    bool showTimeSetDialog(bool isInitialSetup = false);

    // 加载并显示向量表
    void loadVectorTable();

    // 选择向量表
    void onVectorTableSelectionChanged(int index);

    // 保存向量表数据
    void saveVectorTableData();

    // 添加新向量表
    void addNewVectorTable();

    // 显示管脚选择对话框
    void showPinSelectionDialog(int tableId, const QString &tableName);

    // 显示向量行数据录入对话框
    void showVectorDataDialog(int tableId, const QString &tableName, int startIndex = 0);

    // 为当前选中的向量表添加行
    void addRowToCurrentVectorTable();

    // 删除当前选中的向量表
    void deleteCurrentVectorTable();

    // 删除选中的向量行
    void deleteSelectedVectorRows();

    // 填充TimeSet
    void showFillTimeSetDialog();
    void fillTimeSetForVectorTable(int timeSetId, const QList<int> &selectedUiRows);

    // 刷新当前向量表数据
    void refreshVectorTableData();

    // 打开TimeSet设置对话框
    void openTimeSetSettingsDialog();

private:
    void setupUI();
    void setupMenu();
    void setupVectorTableUI();

    // 当前项目的数据库路径
    QString m_currentDbPath;

    // 向量表显示相关的UI组件
    QTableWidget *m_vectorTableWidget;
    QComboBox *m_vectorTableSelector;
    QWidget *m_centralWidget;
    QWidget *m_welcomeWidget;
    QWidget *m_vectorTableContainer;
    QPushButton *m_fillTimeSetButton;
    QPushButton *m_refreshButton;
    QPushButton *m_timeSetSettingsButton;

    // 自定义代理
    VectorTableItemDelegate *m_itemDelegate;

    // 数据处理和对话框管理器
    VectorDataHandler *m_dataHandler;
    DialogManager *m_dialogManager;
};
#endif // MAINWINDOW_H
