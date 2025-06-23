#ifndef ADVANCEDVECTORDIALOG_H
#define ADVANCEDVECTORDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QStatusBar>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include "vectortableview.h"
#include "vectortablemodel.h"
#include "../database/databasemanager.h"

/**
 * @brief AdvancedVectorDialog 类是基于Model/View架构的向量表对话框
 * 
 * 该类提供了一个高级的向量表编辑界面，使用Qt的Model/View架构，
 * 相比传统的QTableWidget实现，具有更好的性能和内存占用。
 */
class AdvancedVectorDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口
     */
    explicit AdvancedVectorDialog(QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    virtual ~AdvancedVectorDialog();

private slots:
    /**
     * @brief 刷新向量表列表
     */
    void refreshVectorTablesList();
    
    /**
     * @brief 加载选中的向量表
     */
    void loadSelectedVectorTable();

private:
    /**
     * @brief 设置界面
     */
    void setupUI();
    
    /**
     * @brief 设置信号和槽连接
     */
    void setupConnections();
    
    /**
     * @brief 更新状态栏消息
     * @param message 要显示的消息
     */
    void updateStatusMessage(const QString &message);

private:
    VectorTableView *m_vectorTableView;       // 向量表视图
    VectorTableModel *m_vectorTableModel;     // 向量表模型
    QComboBox *m_vectorTableSelector;         // 向量表选择器
    QPushButton *m_refreshButton;            // 刷新按钮
    QPushButton *m_saveButton;               // 保存按钮
    QStatusBar *m_statusBar;                 // 状态栏
};

#endif // ADVANCEDVECTORDIALOG_H 