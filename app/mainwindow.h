#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QDir>
#include <QTableWidget>
#include <QTableView>
#include <QStackedWidget>
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
#include "../vector/robustvectordatahandler.h"
#include "qcustomplot.h"
#include "vectortablemodel.h"
#include "vector/vectordatahandler.h"
#include "vector/vectortabledelegate.h"

// 前置声明
class VectorTableDelegate;
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
    void createNewProjectWithNewArch(); // 使用新架构创建项目
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

    // 打开指定ID的向量表
    void openVectorTable(int tableId, const QString &tableName);

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

    // 为当前选中的向量表添加行(Model/View架构)
    void addRowToCurrentVectorTableModel();

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

    // 向量列属性栏相关槽
    void updateVectorColumnProperties(int row, int column);

    // 计算16进制值并显示在向量列属性栏中
    void calculateAndDisplayHexValue(const QList<int> &selectedRows, int column);

    // 处理16进制值编辑后的同步操作
    void onHexValueEdited();

    // 实时验证16进制输入
    void validateHexInput(const QString &text);

    // === 新视图中向量列属性栏相关槽 ===
    // 处理新视图选择变化
    void onNewViewSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);

    // 新视图中更新向量列属性栏
    void updateVectorColumnPropertiesForModel(const QList<int> &selectedRows, int column);

    // 新视图中计算16进制值并显示在向量列属性栏中
    void calculateAndDisplayHexValueForModel(const QList<int> &selectedRows, int column);

    // 新视图中计算并更新错误个数显示
    void updateErrorCountForModel(const QList<int> &selectedRows, int column);

    // 新视图中处理16进制值编辑后的同步操作
    void onHexValueEditedForModel();

    // 波形图相关槽函数
    void setupWaveformView();
    void updateWaveformView();
    void toggleWaveformView(bool show);
    void onWaveformPinSelectionChanged(int index);
    void onShowAllPinsChanged(int state);
    void onWaveformContextMenuRequested(const QPoint &pos);
    void setupWaveformClickHandling();
    void highlightWaveformPoint(int rowIndex, int pinIndex = -1);
    void jumpToWaveformPoint(int rowIndex, const QString &pinName);
    void saveCurrentTableData(); // 保存当前页表格数据，用于页面切换时
    void onWaveformDoubleClicked(QMouseEvent *event);
    void onWaveformValueEdited();

    void on_action_triggered(bool checked);
    void onProjectStructureItemDoubleClicked(QTreeWidgetItem *item, int column);
    void updateWindowTitle(const QString &dbPath = QString());

private:
    void setupUI();
    void setupMenu();
    void setupVectorTableUI();
    void setupTabBar();
    void setupSidebarNavigator();
    void setupVectorColumnPropertiesBar();
    QFrame *createHorizontalSeparator(); // 创建水平分隔线的辅助函数
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

    // 辅助函数：添加默认列配置
    bool addDefaultColumnConfigurations(int tableId);

    // 辅助函数：修复没有列配置的表
    bool fixExistingTableWithoutColumns(int tableId);

    // 辅助函数：检查并修复所有向量表
    void checkAndFixAllVectorTables();

    // 辅助函数：更新分页信息
    void updatePaginationInfo();

    // 辅助函数：加载向量表元数据
    bool loadVectorTableMeta(int tableId, QString &binFileName, QList<Vector::ColumnInfo> &columns, int &schemaVersion, int &rowCount);

    // 辅助函数：更新二进制文件头中的列数
    void updateBinaryHeaderColumnCount(int tableId);

    // 辅助函数：重新加载并刷新向量表
    void reloadAndRefreshVectorTable(int tableId);

    // 辅助函数：检查Label值是否重复
    bool isLabelDuplicate(int tableId, const QString &labelValue, int currentRow, int &duplicateRow);

    // 辅助函数：获取特定行的TimeSet ID
    int getTimeSetIdForRow(int tableId, int rowIndex);
    bool getTimeSetT1RAndPeriod(int timeSetId, int pinId, double &t1r, double &period);
    int getPinIdByName(const QString &pinName);
    QString getPinNameById(int pinId);

    // 当前项目的数据库路径
    QString m_currentDbPath;

    // 菜单项
    QMenu *m_viewMenu; // "视图"菜单
    QAction *m_newProjectAction;
    QAction *m_openProjectAction;
    QAction *m_closeProjectAction;

    // 向量表显示相关的UI组件
    QStackedWidget *m_vectorStackedWidget; // 用于切换旧表格和新表格的堆栈窗口
    QTableWidget *m_vectorTableWidget;     // 旧的表格组件
    QTableView *m_vectorTableView;         // 新的表格视图组件
    VectorTableModel *m_vectorTableModel;  // 新的表格数据模型
    QComboBox *m_vectorTableSelector;
    QWidget *m_centralWidget;
    QWidget *m_welcomeWidget;
    QWidget *m_vectorTableContainer;
    QAction *m_fillVectorAction;
    QAction *m_fillTimeSetAction;
    QAction *m_replaceTimeSetAction;
    QAction *m_refreshAction;
    QAction *m_timeSetSettingsAction;
    QAction *m_setupPinsAction;       // 设置向量表管脚按钮
    QAction *m_pinSettingsAction;     // 管脚设置按钮
    QAction *m_addPinAction;          // 添加管脚按钮
    QAction *m_deletePinAction;       // 删除管脚按钮
    QAction *m_deleteRangeAction;     // 删除指定范围内的向量行按钮
    QAction *m_gotoLineAction;        // 跳转到某行按钮
    QAction *m_addGroupAction;        // 添加管脚分组按钮
    QAction *m_toggleTableViewAction; // 切换新旧表格视图按钮

    // 分页相关UI组件
    QWidget *m_paginationWidget;   // 分页控件容器
    QPushButton *m_prevPageButton; // 上一页按钮
    QPushButton *m_nextPageButton; // 下一页按钮
    QLabel *m_pageInfoLabel;       // 页码信息标签
    QComboBox *m_pageSizeSelector; // 每页行数选择器
    QSpinBox *m_pageJumper;        // 页码跳转输入框
    QPushButton *m_jumpButton;     // 跳转按钮

    // 分页相关数据
    int m_currentPage;    // 当前页码（从0开始）
    int m_pageSize;       // 每页显示的行数
    int m_totalPages;     // 总页数
    int m_totalRows;      // 总行数
    bool m_pagingEnabled; // 是否启用分页

    // 数据修改状态
    bool m_hasUnsavedChanges; // 跟踪是否有未保存的内容

    // 判断是否有未保存的内容
    bool hasUnsavedChanges() const;

    // Tab页签组件
    QTabWidget *m_vectorTabWidget;
    bool m_isUpdatingUI; // 防止UI更新循环的标志

    // 自定义代理
    VectorTableDelegate *m_itemDelegate;

    // 数据处理和对话框管理器
    DialogManager *m_dialogManager;

    // 存储Tab页与TableId的映射关系
    QMap<int, int> m_tabToTableId;

    // 窗口大小信息标签
    QLabel *m_windowSizeLabel;

    // 侧边导航栏组件
    QDockWidget *m_sidebarDock;
    QTreeWidget *m_sidebarTree;

    // 向量列属性栏组件
    QDockWidget *m_columnPropertiesDock;   // 向量列属性栏容器
    QWidget *m_columnPropertiesWidget;     // 向量列属性内容区域
    QLabel *m_columnNamePinLabel;          // 列名称旁的管脚标签
    QLabel *m_pinNameLabel;                // 管脚名称标签
    QLineEdit *m_pinValueField;            // 16进制值字段
    QLineEdit *m_errorCountField;          // 错误个数字段
    QCheckBox *m_continuousSelectCheckBox; // 连续选择勾选框

    // 当前选中的用于16进制值显示和编辑的信息
    int m_currentHexValueColumn;      // 当前16进制值对应的列
    QList<int> m_currentSelectedRows; // 当前选中的行

    QList<Vector::ColumnInfo> getCurrentColumnConfiguration(int tableId);
    bool areColumnConfigurationsDifferentSimplified(const QList<Vector::ColumnInfo> &config1, const QList<Vector::ColumnInfo> &config2);
    QList<Vector::RowData> adaptRowDataToNewColumns(const QList<Vector::RowData> &oldRowData,
                                                    const QList<Vector::ColumnInfo> &oldColumns,
                                                    const QList<Vector::ColumnInfo> &newColumns);

    // 波形图视图相关UI组件
    QDockWidget *m_waveformDock = nullptr;
    QWidget *m_waveformContainer = nullptr;
    QCustomPlot *m_waveformPlot = nullptr;
    QComboBox *m_waveformPinSelector = nullptr;
    QCheckBox *m_showAllPinsCheckBox = nullptr;
    bool m_showAllPins = false; // 是否显示所有管脚
    QAction *m_toggleWaveformAction;
    bool m_isWaveformVisible = false;
    int m_selectedWaveformPoint = -1;
    double m_currentXOffset = 0.0; // [DEPRECATED] To be replaced by pin-specific ratios
    double m_currentPeriod;
    QMap<int, double> m_pinT1rRatios;
    int m_editingRow = -1;       // 波形图编辑行
    int m_editingPinColumn = -1; // 波形图编辑管脚列

    // R0波形点结构 (需要保存原始电平信息)
    struct R0WavePoint
    {
        int cycleIndex;  // 区间索引
        double t1fXPos;  // T1F点位置
        bool isOne;      // 原始数据是否为'1'
        double highY;    // 高电平Y坐标
        double lowY;     // 低电平Y坐标
        int pinId;       // 管脚ID
        QString pinName; // 管脚名称
        double t1rRatio; // T1R比例
    };

    // RZ波形点结构 (需要保存原始电平信息)
    struct RZWavePoint
    {
        int cycleIndex;  // 区间索引
        double t1fXPos;  // T1F点位置
        bool isOne;      // 原始数据是否为'1'
        double highY;    // 高电平Y坐标
        double lowY;     // 低电平Y坐标
        int pinId;       // 管脚ID
        QString pinName; // 管脚名称
        double t1rRatio; // T1R比例
    };

    // SBC波形点结构 (需要保存原始电平信息)
    struct SbcWavePoint
    {
        int cycleIndex;  // 区间索引
        double t1fXPos;  // T1F点位置
        bool isOne;      // 原始数据是否为'1'
        double highY;    // 高电平Y坐标
        double lowY;     // 低电平Y坐标
        int pinId;       // 管脚ID
        QString pinName; // 管脚名称
        double t1rRatio; // T1R比例
    };

    // 波形图类型相关
    bool getWaveTypeAndT1F(int timeSetId, int pinId, int &waveId, double &t1f);
    QString getWaveTypeName(int waveId);
    void applyWaveformPattern(int timeSetId, int pinId, const QVector<double> &xData, QVector<double> &yData, double t1rRatio, double period);
    void drawWaveformPatterns();
    QList<R0WavePoint> m_r0Points;   // 存储R0点信息
    QList<RZWavePoint> m_rzPoints;   // 存储RZ点信息
    QList<SbcWavePoint> m_sbcPoints; // 存储SBC点信息

    // 波形图在线编辑
    QLineEdit *m_waveformValueEditor = nullptr;
    
    // 数据处理器切换控制
    bool m_useNewDataHandler = false;
    RobustVectorDataHandler* m_robustDataHandler = nullptr;
};

#endif // MAINWINDOW_H
