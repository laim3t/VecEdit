#ifndef TIMESETDIALOG_H
#define TIMESETDIALOG_H

#include <QDialog>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QLabel>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QGroupBox>
#include <QListWidget>
#include <QSplitter>
#include <QStandardItemModel>
#include <QList>
#include <QMap>
#include <QStyledItemDelegate>
#include <QSqlDatabase>
#include <QTableWidget>
#include <QTableWidgetItem>

// 波形下拉框委托类
class WaveComboDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    WaveComboDelegate(const QMap<int, QString> &waveOptions, QObject *parent = nullptr)
        : QStyledItemDelegate(parent), m_waveOptions(waveOptions) {}

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

// TimeSet数据结构
struct TimeSetData
{
    QString name;      // TimeSet名称
    double period;     // 周期，单位ns
    QList<int> pinIds; // 关联的管脚ID列表
};

// 前向声明MainWindow类
class MainWindow;

// TimeSet边沿参数条目数据结构
struct TimeSetEdgeData
{
    int timesetId; // 关联的TimeSet ID
    int pinId;     // 关联的管脚ID
    double t1r;    // T1R值，默认250
    double t1f;    // T1F值，默认750
    double stbr;   // STBR值，默认500
    int waveId;    // 波形ID，关联wave_options表
};

class TimeSetDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TimeSetDialog(QWidget *parent = nullptr);
    ~TimeSetDialog();

private slots:
    // TimeSet操作
    void addTimeSet();
    void removeTimeSet();
    void renameTimeSet(QTreeWidgetItem *item, int column);
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

private: // 用private区域重新组织函数声明
    // UI相关函数
    void setupUI();
    void setupMainLayout();
    void setupTreeWidget();
    void setupPinSelection();
    void setupButtonBox();

    // 数据加载函数
    void loadWaveOptions();
    void loadPins();

    // 显示对话框和表单
    void showPinSelectionDialog(int tableId, const QString &tableName);
    void showPinSelectionDialogStandalone(int tableId, const QString &tableName);
    void showVectorDataDialog(int tableId, const QString &tableName);
    void addVectorRow(QTableWidget *table, const QStringList &pinOptions, int rowIdx);

    // 更新边沿项显示文本
    void updateEdgeItemText(QTreeWidgetItem *edgeItem, const TimeSetEdgeData &edgeData);

    // 保存到数据库
    bool saveToDatabase();
    bool saveTimeSetEdgesToDatabase(int timeSetId, const QList<TimeSetEdgeData> &edges);
    bool saveTimeSetToDatabase(const TimeSetData &timeSet, int &outTimeSetId);

    // MainWindow指针，用于访问共享方法
    MainWindow *m_mainWindow;

    // UI组件
    QSplitter *mainSplitter;
    QTreeWidget *timeSetTree;
    QGroupBox *pinSelectionGroup;
    QListWidget *pinListWidget;
    QPushButton *addTimeSetButton;
    QPushButton *removeTimeSetButton;
    QPushButton *addEdgeButton;
    QPushButton *removeEdgeButton;
    QDialogButtonBox *buttonBox;
    WaveComboDelegate *waveDelegate;

    // 数据
    QList<TimeSetData> timeSetDataList;
    QMap<int, QString> waveOptions; // 波形ID -> 名称映射
    QMap<int, QString> pinList;     // 管脚ID -> 名称映射
    QList<TimeSetEdgeData> edgeDataList;

    // 当前选中的TimeSet项
    QTreeWidgetItem *currentTimeSetItem;
    int currentTimeSetIndex;

    QSqlDatabase db;
};

#endif // TIMESETDIALOG_H