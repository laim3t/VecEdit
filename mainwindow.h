#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QDir>
#include <QTableWidget>
#include <QComboBox>
#include <QStyledItemDelegate>
#include <QSqlQuery>

// 自定义代理类，用于处理不同列的编辑器类型
class VectorTableItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit VectorTableItemDelegate(QObject *parent = nullptr);

    // 创建编辑器
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    // 设置编辑器数据
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;

    // 获取编辑器数据
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;

private:
    // 缓存指令选项
    mutable QStringList m_instructionOptions;

    // 缓存时间集选项
    mutable QStringList m_timesetOptions;

    // 缓存管脚选项
    mutable QStringList m_pinOptions;

    // 获取指令选项
    QStringList getInstructionOptions() const;

    // 获取时间集选项
    QStringList getTimesetOptions() const;

    // 获取管脚选项
    QStringList getPinOptions() const;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

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
    bool showTimeSetDialog();

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
    void showVectorDataDialog(int tableId, const QString &tableName);

private:
    void setupUI();
    void setupMenu();
    void setupVectorTableUI();

    // 辅助函数：添加向量行
    void addVectorRow(QTableWidget *table, const QStringList &pinOptions, int rowIdx);

    // 当前项目的数据库路径
    QString m_currentDbPath;

    // 向量表显示相关的UI组件
    QTableWidget *m_vectorTableWidget;
    QComboBox *m_vectorTableSelector;
    QWidget *m_centralWidget;
    QWidget *m_welcomeWidget;
    QWidget *m_vectorTableContainer;

    // 自定义代理
    VectorTableItemDelegate *m_itemDelegate;
};
#endif // MAINWINDOW_H
