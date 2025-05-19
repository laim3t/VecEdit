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
#include <QSqlDatabase>
#include <QMap>
#include <QLabel>
#include <QString>
#include <QTabWidget>
#include <QListWidget>
#include <QListWidgetItem>
#include <QResizeEvent>
#include <QStatusBar>
#include "../common/tablestylemanager.h"

// 前置声明
class VectorTableItemDelegate;
class DialogManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // 辅助函数：添加向量行
    void addVectorRow(QTableWidget *table, const QStringList &pinOptions, int rowIdx);

protected:
    // 处理窗口大小变化事件
    void resizeEvent(QResizeEvent *event) override;

    // 处理窗口关闭事件
    void closeEvent(QCloseEvent *event) override;

signals:
    // 窗口大小变化信号
    void windowResized();

private slots:
    // 数据库操作
    void createNewProject();
    void openExistingProject();
    void closeCurrentProject();

    // 显示数据库视图对话框
    void showDatabaseViewDialog();

    // 显示添加管脚对话框
    bool showAddPinsDialog();

    // 添加单个管脚
    void addSinglePin();

    // 删除管脚
    void deletePins();

    // 向数据库添加管脚
    bool addPinsToDatabase(const QList<QString> &pinNames);

    // 显示TimeSet对话框
    bool showTimeSetDialog(bool isInitialSetup = false);

    // 显示管脚分组对话框
    void showPinGroupDialog();

    // 加载并显示向量表
    void loadVectorTable();

    // 选择向量表
    void onVectorTableSelectionChanged(int index);

    // Tab页签切换响应
    void onTabChanged(int index);

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

    // 删除指定范围内的向量行
    void deleteVectorRowsInRange();

    // 填充TimeSet
    void showFillTimeSetDialog();
    void fillTimeSetForVectorTable(int timeSetId, const QList<int> &selectedUiRows);

    // 替换TimeSet
    void showReplaceTimeSetDialog();
    void replaceTimeSetForVectorTable(int fromTimeSetId, int toTimeSetId, const QList<int> &selectedUiRows);

    // 刷新当前向量表数据
    void refreshVectorTableData();

    // 打开TimeSet设置对话框
    void openTimeSetSettingsDialog();

    // 设置当前向量表的管脚
    void setupVectorTablePins();

    // 打开管脚设置对话框
    void openPinSettingsDialog();

    // 跳转到指定行
    void gotoLine();

    void onFontZoomSliderValueChanged(int value);
    void onFontZoomReset();
    void closeTab(int index);

private:
    void setupUI();
    void setupMenu();
    void setupVectorTableUI();
    void setupTabBar();
    void addVectorTableTab(int tableId, const QString &tableName);
    void loadAllVectorTables();
    void syncTabWithComboBox(int comboBoxIndex);
    void syncComboBoxWithTab(int tabIndex);

    // 更新状态栏中的窗口大小信息
    void updateWindowSizeInfo();

    // 保存窗口状态
    void saveWindowState();

    // 恢复窗口状态
    void restoreWindowState();

    // 为新创建的向量表添加默认列配置
    bool addDefaultColumnConfigurations(int tableId);

    // 修复没有列配置的现有向量表
    bool fixExistingTableWithoutColumns(int tableId);

    // 检查和修复所有向量表的列配置
    void checkAndFixAllVectorTables();

    // 当前项目的数据库路径
    QString m_currentDbPath;

    // 向量表显示相关的UI组件
    QTableWidget *m_vectorTableWidget;
    QComboBox *m_vectorTableSelector;
    QWidget *m_centralWidget;
    QWidget *m_welcomeWidget;
    QWidget *m_vectorTableContainer;
    QPushButton *m_fillTimeSetButton;
    QPushButton *m_replaceTimeSetButton;
    QPushButton *m_refreshButton;
    QPushButton *m_timeSetSettingsButton;
    QPushButton *m_setupPinsButton;   // 设置向量表管脚按钮
    QPushButton *m_pinSettingsButton; // 管脚设置按钮
    QPushButton *m_addPinButton;      // 添加管脚按钮
    QPushButton *m_deletePinButton;   // 删除管脚按钮
    QPushButton *m_deleteRangeButton; // 删除指定范围内的向量行按钮
    QPushButton *m_gotoLineButton;    // 跳转到某行按钮
    QPushButton *m_addGroupButton;    // 添加管脚分组按钮

    // Tab页签组件
    QTabWidget *m_vectorTabWidget;
    bool m_isUpdatingUI; // 防止UI更新循环的标志

    // 自定义代理
    VectorTableItemDelegate *m_itemDelegate;

    // 数据处理和对话框管理器
    DialogManager *m_dialogManager;

    // 存储Tab页与TableId的映射关系
    QMap<int, int> m_tabToTableId;

    // 窗口大小信息标签
    QLabel *m_windowSizeLabel;
};

#endif // MAINWINDOW_H
