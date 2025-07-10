#ifndef ADDROWDIALOG_H
#define ADDROWDIALOG_H

#include <QDialog>
#include <QList>
#include <QMap>
#include <QVector>
#include <QModelIndex>

// 前向声明
class QLabel;
class QSpinBox;
class QDialogButtonBox;
class QTableWidget;
class QCheckBox;
class QPushButton;
class QComboBox;
class QGroupBox;

/**
 * @brief AddRowDialog 类为添加向量行对话框。
 *
 * 该对话框为新架构 (Model/View) 设计，实现与旧的向量行数据录入对话框相同的功能。
 * 它负责收集用户输入，以及提供数据预览功能。
 */
class AddRowDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddRowDialog(int tableId, const QMap<int, QString> &pinInfo, QWidget *parent = nullptr);
    ~AddRowDialog() override;

    /**
     * @brief 获取用户在SpinBox中输入的行数。
     * @return 用户要添加的行数。
     */
    int getRowCount() const;

    /**
     * @brief 获取向量插入的起始位置
     * @return 插入的起始行号(1-based)
     */
    int getStartRow() const;

    /**
     * @brief 获取是否添加到最后
     * @return 如果勾选了添加到最后，返回true
     */
    bool isAppendToEnd() const;

    /**
     * @brief 获取选中的TimeSet ID
     * @return 选中的TimeSet ID
     */
    int getSelectedTimeSetId() const;

    /**
     * @brief 获取表格中的数据
     * @return 表格数据列表
     */
    QList<QStringList> getTableData() const;

    /**
     * @brief 获取管脚列信息
     * @return 管脚列的映射 (列索引 -> 名称)
     */
    const QMap<int, QString> &getPinOptions() const;

private slots:
    void onAppendToEndStateChanged(int state);
    void onAddRowButtonClicked();
    void onRemoveRowButtonClicked();
    void onTimeSetButtonClicked();
    void validateRowCount(int value); // 新增：验证行数是否是表格行数的整数倍

    // 重写accept方法实现提交前的验证和进度显示
    void accept() override;

private:
    void setupUi();
    void setupTable();
    void setupConnections();
    void loadTimeSets();
    void updateRemainingRowsDisplay();
    void ensureRowSelectionBehavior();                 // 新增：确保点击单元格会选中整行
    void initializeRowWithDefaultValues(int rowIndex); // 新增：初始化行的默认值为"X"

    QLabel *m_infoLabel;
    QSpinBox *m_rowCountSpinBox;
    QDialogButtonBox *m_buttonBox;

    // 表格区域
    QTableWidget *m_previewTable;
    QPushButton *m_timeSetButton;

    // 操作按钮区域
    QPushButton *m_addRowButton;
    QPushButton *m_removeRowButton;

    // 信息显示区域
    QGroupBox *m_infoGroup;
    QLabel *m_totalRowsLabel;
    QLabel *m_remainingRowsLabel;

    // 行操作区域
    QGroupBox *m_rowGroup;
    QCheckBox *m_appendToEndCheckBox;
    QLabel *m_startRowLabel;
    QSpinBox *m_startRowSpinBox;

    // 设置区域
    QGroupBox *m_settingsGroup;
    QComboBox *m_timeSetComboBox;

    // 数据缓存
    QMap<int, QString> m_pinInfo; // 用于存储管脚信息
    QMap<int, QString> m_timeSetOptions;
    QMap<int, QString> m_pinOptions;
    int m_totalRows;
    int m_remainingRows;
    int m_tableId;

    // 警告标签 - 用于显示行数不是整数倍时的警告
    QLabel *m_warningLabel;
};

#endif // ADDROWDIALOG_H