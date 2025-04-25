#ifndef TIMESETDIALOG_H
#define TIMESETDIALOG_H

#include <QDialog>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QSqlDatabase>
#include <QMap>
#include <QList>
#include <QStyledItemDelegate>

#include "timesetdataaccess.h"
#include "timesetui.h"
#include "timesetedgemanager.h"
#include "pinselectionmanager.h"
#include "vectordatamanager.h"

class MainWindow;

// 波形下拉框委托类
class WaveComboDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    WaveComboDelegate(const QMap<int, QString> &waveOptions, QObject *parent = nullptr);

    // 创建编辑器
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;

    // 设置编辑器数据
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;

    // 获取编辑器数据
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;

    // 更新编辑器几何形状
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const override;

private:
    QMap<int, QString> m_waveOptions;
};

// TimeSet数据结构和边沿参数条目数据结构已在timesetdataaccess.h中定义

class TimeSetDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TimeSetDialog(QWidget *parent = nullptr, bool isInitialSetup = false);
    ~TimeSetDialog();

private slots:
    // TimeSet操作
    void addTimeSet();
    void removeTimeSet();
    void updatePeriod(double value);
    void timeSetSelectionChanged();
    void editTimeSetProperties(QTreeWidgetItem *item, int column);

    // 边沿参数条目操作
    void addEdgeItem();
    void removeEdgeItem();
    void editEdgeItem(QTreeWidgetItem *item, int column);
    void onItemDoubleClicked(QTreeWidgetItem *item, int column);
    void onPropertyItemChanged(QTreeWidgetItem *item, int column);

    // 管脚选择
    void onPinSelectionChanged();
    void updatePinSelection(QTreeWidgetItem *item, int column);

    // 对话框按钮
    void onAccepted();
    void onRejected();

private:
    // 初始化方法
    void initialize();
    void setupConnections();

    // 数据加载方法
    bool loadExistingTimeSets();

    // Helper to get TimeSet name by ID
    QString getTimeSetNameById(int id) const;

    // MainWindow指针，用于访问共享方法
    MainWindow *m_mainWindow;

    // 模块管理器
    TimeSetUIManager *m_uiManager;
    TimeSetDataAccess *m_dataAccess;
    TimeSetEdgeManager *m_edgeManager;
    PinSelectionManager *m_pinManager;
    VectorDataManager *m_vectorManager;

    // 数据
    QList<TimeSetData> m_timeSetDataList;
    QMap<int, QString> m_waveOptions;
    QMap<int, QString> m_pinList;
    QList<int> m_timeSetIdsToDelete; // Track TimeSet IDs marked for deletion

    // 当前选中的TimeSet项
    QTreeWidgetItem *m_currentTimeSetItem;
    int m_currentTimeSetIndex;

    // 标识是否是初始设置流程
    bool m_isInitialSetup;

    // 数据库连接
    QSqlDatabase m_db;

    // 委托
    WaveComboDelegate *m_waveDelegate;
};

#endif // TIMESETDIALOG_H