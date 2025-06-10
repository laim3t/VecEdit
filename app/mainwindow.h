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
#include <QToolButton>
#include <QToolBar>
#include <QSqlDatabase>
#include <QMap>
#include <QLabel>
#include <QString>
#include <QTabWidget>
#include <QListWidget>
#include <QListWidgetItem>
#include <QResizeEvent>
#include <QStatusBar>
#include <QFile>
#include <QFileInfo>
#include <QSpinBox>
#include <QDockWidget>
#include <QTreeWidget>
#include <stdexcept>
#include "../vector/vector_data_types.h"
#include "../vector/vectordatahandler.h"
#include "../database/binaryfilehelper.h"
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

    // 向量填充
    void showFillVectorDialog();
    void fillVectorForVectorTable(const QString &value, const QList<int> &selectedUiRows);
    void fillVectorWithPattern(const QMap<int, QString> &rowValueMap);

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

    // 分页功能相关方法
    void loadCurrentPage();
    void loadNextPage();
    void loadPrevPage();
    void changePageSize(int newSize);
    void jumpToPage(int pageNum);

    // 表格数据变更监听
    void onTableCellChanged(int row, int column);
    void onTableRowModified(int row);

    // 显示管脚列的右键菜单
    void showPinColumnContextMenu(const QPoint &pos);

    // 侧边导航栏相关槽
    void refreshSidebarNavigator();
    void onSidebarItemClicked(QTreeWidgetItem *item, int column);
    void onPinItemClicked(QTreeWidgetItem *item, int column);
    void onTimeSetItemClicked(QTreeWidgetItem *item, int column);
    void onVectorTableItemClicked(QTreeWidgetItem *item, int column);
    void onLabelItemClicked(QTreeWidgetItem *item, int column);

    // 更新菜单项状态
    void updateMenuState();

private:
    void setupUI();
    void setupMenu();
    void setupVectorTableUI();
    void setupTabBar();
    void setupSidebarNavigator();
    void resetSidebarNavigator();
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

    // 更新分页信息显示
    void updatePaginationInfo();

    // 辅助方法：从数据库加载向量表元数据
    bool loadVectorTableMeta(int tableId, QString &binFileName, QList<Vector::ColumnInfo> &columns, int &schemaVersion, int &rowCount);

    // 新增：在管脚配置成功后，立即更新二进制文件头的列计数
    void updateBinaryHeaderColumnCount(int tableId);

    // 新增：重新加载并刷新指定的向量表视图
    void reloadAndRefreshVectorTable(int tableId);

    // 当前项目的数据库路径
    QString m_currentDbPath;

    // 菜单项
    QAction *m_newProjectAction;
    QAction *m_openProjectAction;
    QAction *m_closeProjectAction;

    // 向量表显示相关的UI组件
    QTableWidget *m_vectorTableWidget;
    QComboBox *m_vectorTableSelector;
    QWidget *m_centralWidget;
    QWidget *m_welcomeWidget;
    QWidget *m_vectorTableContainer;
    QAction *m_fillVectorAction;
    QAction *m_fillTimeSetAction;
    QAction *m_replaceTimeSetAction;
    QAction *m_refreshAction;
    QAction *m_timeSetSettingsAction;
    QAction *m_setupPinsAction;   // 设置向量表管脚按钮
    QAction *m_pinSettingsAction; // 管脚设置按钮
    QAction *m_addPinAction;      // 添加管脚按钮
    QAction *m_deletePinAction;   // 删除管脚按钮
    QAction *m_deleteRangeAction; // 删除指定范围内的向量行按钮
    QAction *m_gotoLineAction;    // 跳转到某行按钮
    QAction *m_addGroupAction;    // 添加管脚分组按钮

    // 分页相关UI组件
    QWidget *m_paginationWidget;   // 分页控件容器
    QPushButton *m_prevPageButton; // 上一页按钮
    QPushButton *m_nextPageButton; // 下一页按钮
    QLabel *m_pageInfoLabel;       // 页码信息标签
    QComboBox *m_pageSizeSelector; // 每页行数选择器
    QSpinBox *m_pageJumper;        // 页码跳转输入框
    QPushButton *m_jumpButton;     // 跳转按钮

    // 分页相关数据
    int m_currentPage; // 当前页码(从0开始)
    int m_pageSize;    // 每页行数
    int m_totalPages;  // 总页数
    int m_totalRows;   // 总行数

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

    // 侧边导航栏组件
    QDockWidget *m_sidebarDock;
    QTreeWidget *m_sidebarTree;

    QList<Vector::ColumnInfo> getCurrentColumnConfiguration(int tableId);
    bool areColumnConfigurationsDifferentSimplified(const QList<Vector::ColumnInfo> &config1, const QList<Vector::ColumnInfo> &config2);
    QList<Vector::RowData> adaptRowDataToNewColumns(const QList<Vector::RowData> &oldRowData,
                                                    const QList<Vector::ColumnInfo> &oldColumns,
                                                    const QList<Vector::ColumnInfo> &newColumns);
};

#endif // MAINWINDOW_H
