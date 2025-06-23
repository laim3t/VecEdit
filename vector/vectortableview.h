#ifndef VECTORTABLEVIEW_H
#define VECTORTABLEVIEW_H

#include <QTableView>
#include <QMenu>
#include <QAction>

class VectorTableModel;

/**
 * @brief VectorTableView 类是基于Model/View架构的向量表视图
 * 
 * 该类继承自QTableView，提供了一个高性能、低内存占用的向量表视图。
 * 它与VectorTableModel配合使用，构成VecEdit新架构的视图层。
 */
class VectorTableView : public QTableView
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit VectorTableView(QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~VectorTableView();
    
    /**
     * @brief 设置数据模型
     * @param model 向量表模型
     */
    void setVectorTableModel(VectorTableModel *model);
    
    /**
     * @brief 获取当前数据模型
     * @return 向量表模型
     */
    VectorTableModel* vectorTableModel() const;

signals:
    /**
     * @brief 请求添加行信号
     */
    void requestAddRow();
    
    /**
     * @brief 请求删除选中行信号
     * @param rows 选中的行索引列表
     */
    void requestRemoveRows(const QList<int> &rows);
    
    /**
     * @brief 请求填充向量信号
     * @param rows 选中的行索引列表
     */
    void requestFillVector(const QList<int> &rows);
    
    /**
     * @brief 请求填充TimeSet信号
     * @param rows 选中的行索引列表
     */
    void requestFillTimeSet(const QList<int> &rows);
    
    /**
     * @brief 请求替换TimeSet信号
     * @param rows 选中的行索引列表
     */
    void requestReplaceTimeSet(const QList<int> &rows);

protected:
    /**
     * @brief 上下文菜单事件处理
     * @param event 上下文菜单事件
     */
    void contextMenuEvent(QContextMenuEvent *event) override;
    
    /**
     * @brief 键盘按键事件处理
     * @param event 键盘事件
     */
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    /**
     * @brief 处理添加行动作
     */
    void onAddRowTriggered();
    
    /**
     * @brief 处理删除行动作
     */
    void onRemoveRowsTriggered();
    
    /**
     * @brief 处理填充向量动作
     */
    void onFillVectorTriggered();
    
    /**
     * @brief 处理填充TimeSet动作
     */
    void onFillTimeSetTriggered();
    
    /**
     * @brief 处理替换TimeSet动作
     */
    void onReplaceTimeSetTriggered();

private:
    /**
     * @brief 创建上下文菜单
     * @return 上下文菜单
     */
    QMenu* createContextMenu();
    
    /**
     * @brief 获取当前选中的行索引列表
     * @return 行索引列表
     */
    QList<int> getSelectedRows() const;

    // 菜单相关
    QMenu *m_contextMenu;       // 上下文菜单
    QAction *m_addRowAction;    // 添加行动作
    QAction *m_removeRowAction; // 删除行动作
    QAction *m_fillVectorAction;// 填充向量动作
    QAction *m_fillTimeSetAction;// 填充TimeSet动作
    QAction *m_replaceTimeSetAction;// 替换TimeSet动作
};

#endif // VECTORTABLEVIEW_H 