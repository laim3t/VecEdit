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
    explicit AddRowDialog(QWidget *parent = nullptr);
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

private slots:
    void onAppendToEndStateChanged(int state);
    void onAddRowButtonClicked();
    void onRemoveRowButtonClicked();
    void onTimeSetButtonClicked();

private:
    void setupUi();
    void setupTable();
    void setupConnections();
    void loadTimeSets();
    void updateRemainingRowsDisplay();

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
    QMap<int, QString> m_timeSetOptions;
    QMap<int, QString> m_pinOptions;
    int m_totalRows;
    int m_remainingRows;
    int m_tableId;
};

#endif // ADDROWDIALOG_H