#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QDir>
#include <QTableWidget>
#include <QTableView>
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
#include "../vector/vectortablemodel.h"
#include "../database/binaryfilehelper.h"
#include "../common/tablestylemanager.h"
#include "qcustomplot.h"

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
    void onModelDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles);
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
    void updateVectorColumnPropertiesModel(const QModelIndex &current, const QModelIndex &previous);

    // 计算16进制值并显示在向量列属性栏中
    void calculateAndDisplayHexValue(const QList<int> &selectedRows, int column);

    // 处理16进制值编辑后的同步操作
    void onHexValueEdited();

    // 实时验证16进制输入
    void validateHexInput(const QString &text);

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
    // 项目相关
    QString m_currentDbPath;  // 当前数据库路径
    bool m_hasUnsavedChanges; // 是否有未保存的更改

    // 对话框管理器
    DialogManager *m_dialogManager; // 对话框管理器

    // UI组件
    QWidget *m_centralWidget;        // 中央部件
    QWidget *m_welcomeWidget;        // 欢迎页面
    QWidget *m_vectorTableContainer; // 向量表容器
    QLabel *m_windowSizeLabel;       // 窗口大小标签
    QLabel *m_pageInfoLabel;         // 页面信息标签
    QSpinBox *m_pageJumper;          // 页面跳转器
    QPushButton *m_jumpButton;       // 跳转按钮

    // 菜单项
    QAction *m_newProjectAction;     // 新建项目动作
    QAction *m_openProjectAction;    // 打开项目动作
    QAction *m_closeProjectAction;   // 关闭项目动作
    QMenu *m_viewMenu;               // 视图菜单
    QAction *m_toggleWaveformAction; // 切换波形视图动作

    // 侧边栏
    QTreeWidget *m_sidebarTree; // 侧边栏树形控件

    // 列属性面板
    QDockWidget *m_columnPropertiesDock;   // 列属性停靠窗口
    QWidget *m_columnPropertiesWidget;     // 列属性窗口
    QLabel *m_columnNamePinLabel;          // 列名管脚标签
    QLabel *m_pinNameLabel;                // 管脚名称标签
    QLineEdit *m_pinValueField;            // 管脚值输入框
    QLineEdit *m_errorCountField;          // 错误计数输入框
    QCheckBox *m_continuousSelectCheckBox; // 连续选择复选框

    // 波形点数据
    struct WavePoint
    {
        int rowIndex;        // 行索引
        double timePosition; // 时间位置
        char state;          // 状态值 (0/1/X/L/H/Z)
        int pinId;           // 管脚ID
        QString pinName;     // 管脚名称
    };

    // 波形图类型定义
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

    // 波形图类型定义
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

    // 波形图类型定义
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

    // 波形视图
    QWidget *m_waveformContainer;      // 波形容器
    QLineEdit *m_waveformValueEditor;  // 波形值编辑器
    int m_selectedWaveformPoint;       // 选中的波形点
    bool m_showAllPins;                // 是否显示所有管脚
    QCheckBox *m_showAllPinsCheckBox;  // 显示所有管脚复选框
    double m_currentPeriod;            // 当前周期
    double m_currentXOffset;           // 当前X偏移
    QMap<int, double> m_pinT1rRatios;  // 管脚T1R比例
    QVector<R0WavePoint> m_r0Points;   // R0波形点
    QVector<RZWavePoint> m_rzPoints;   // RZ波形点
    QVector<SbcWavePoint> m_sbcPoints; // SBC波形点
    int m_editingRow;                  // 正在编辑的行
    int m_editingPinColumn;            // 正在编辑的管脚列

    // 选择相关
    QList<int> m_currentSelectedRows; // 当前选中的行
    int m_currentHexValueColumn;      // 当前16进制值列

    // 向量表UI组件
    QComboBox *m_vectorTableSelector;        // 向量表选择器
    QTabWidget *m_vectorTabWidget;           // 向量表Tab页签
    QTableWidget *m_vectorTableWidget;       // 向量表格（旧的，将逐步替换为QTableView）
    QTableView *m_vectorTableView;           // 向量表视图（新的，基于模型/视图架构）
    VectorTableModel *m_vectorTableModel;    // 向量表数据模型
    VectorTableItemDelegate *m_itemDelegate; // 表格委托
    QMap<int, int> m_tabToTableId;           // Tab索引到表ID的映射

    // 激活标志，防止循环调用
    bool m_isUpdatingUI;

    // 分页相关
    int m_currentPage;
    int m_pageSize;
    int m_totalRows;
    int m_totalPages;
    QLabel *m_paginationInfoLabel;
    QPushButton *m_prevPageButton;
    QPushButton *m_nextPageButton;
    QSpinBox *m_pageJumpSpinBox;
    QComboBox *m_pageSizeSelector;

    // 侧边栏导航
    QTreeWidget *m_sidebarNavigator;
    QDockWidget *m_sidebarDock;

    // 向量列属性栏
    QDockWidget *m_vectorColumnPropertiesDock;
    QWidget *m_vectorColumnPropertiesPanel;
    QLabel *m_selectedCellInfoLabel;
    QLabel *m_hexValueLabel;
    QLineEdit *m_hexValueEdit;
    QPushButton *m_applyHexValueButton;
    QLabel *m_selectedRowsLabel;
    QLabel *m_selectedColumnsLabel;
    int m_lastClickedRow;
    int m_lastClickedColumn;

    // 波形图
    bool m_isWaveformVisible;
    QCustomPlot *m_waveformPlot;
    QDockWidget *m_waveformDock;
    QComboBox *m_waveformPinSelector;
    QCheckBox *m_showAllPinsCheckbox;
    QPushButton *m_updateWaveformButton;
    QList<QCPGraph *> m_waveformGraphs;
    QVector<double> m_waveformXData;            // 时间点
    QMap<int, QVector<double>> m_waveformYData; // pinId -> 波形点
    double m_waveformMaxY;
    double m_waveformMinY;

    // 波形图信息缓存
    QMap<int, QMap<int, QPair<double, double>>> m_timesetPinT1RCache; // timesetId -> pinId -> {t1r, period}
    QMap<int, QMap<int, int>> m_timesetPinWaveTypeCache;              // timesetId -> pinId -> waveTypeId

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

    // 辅助函数：检查标签是否重复
    bool isLabelDuplicate(int tableId, const QString &labelValue, int currentRow, int &duplicateRow);

    // 辅助函数：获取指定行的TimeSet ID
    int getTimeSetIdForRow(int tableId, int rowIndex);

    // 辅助函数：获取TimeSet的T1R和Period
    bool getTimeSetT1RAndPeriod(int timeSetId, int pinId, double &t1r, double &period);

    // 辅助函数：根据管脚名获取ID
    int getPinIdByName(const QString &pinName);

    // 辅助函数：根据ID获取管脚名
    QString getPinNameById(int pinId);

    // 辅助函数：构建波形图点数据
    void buildWaveformPoints(int timesetId, const QList<int> &selectedPinIds, int maxRowCount);

    // 辅助函数：创建波形图对象
    QCPGraph *createWaveformGraph(QCustomPlot *plot, const QString &name);

    // 辅助函数：获取当前向量表的ID
    int getCurrentVectorTableId() const;

    // 辅助函数：获取当前向量表的选中行
    QList<int> getSelectedRows() const;

    // 辅助函数：从模型中获取选中行
    QList<int> getSelectedRowsFromModel() const;

    // 辅助函数：获取当前向量表的配置
    QList<Vector::ColumnInfo> getCurrentColumnConfiguration(int tableId);

    // 辅助函数：比较两个列配置是否不同（简化版）
    bool areColumnConfigurationsDifferentSimplified(const QList<Vector::ColumnInfo> &config1, const QList<Vector::ColumnInfo> &config2);

    // 辅助函数：将原有行数据适配到新的列配置
    QList<Vector::RowData> adaptRowDataToNewColumns(const QList<Vector::RowData> &oldRowData,
                                                    const QList<Vector::ColumnInfo> &oldColumns,
                                                    const QList<Vector::ColumnInfo> &newColumns);

    // 辅助函数：检查是否使用QTableView还是QTableWidget
    bool isUsingTableView(int tableId) const;

    // 辅助函数：从QTableWidget转换到QTableView（过渡期使用）
    void convertToTableView(int tableId);

    // 辅助函数：设置选中的行
    void setSelectedRows(const QList<int> &rows);

    // 辅助函数：获取单元格数据
    QVariant getCellData(int row, int column) const;

    // 辅助函数：设置单元格数据
    void setCellData(int row, int column, const QVariant &value);

    // 辅助函数：获取波形类型和T1F
    bool getWaveTypeAndT1F(int timeSetId, int pinId, int &waveId, double &t1f);

    // 辅助函数：获取波形类型名称
    QString getWaveTypeName(int waveId);

    // 辅助函数：应用波形模式
    void applyWaveformPattern(int timeSetId, int pinId, const QVector<double> &xData, QVector<double> &yData, double t1rRatio, double period);

    // 辅助函数：绘制波形模式
    void drawWaveformPatterns();

    // 辅助函数：检查是否有未保存的更改
    bool hasUnsavedChanges() const;
};

#endif // MAINWINDOW_H
