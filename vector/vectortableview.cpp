#include "vectortableview.h"
#include "vectortablemodel.h"
#include <QDebug>
#include <QHeaderView>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>

VectorTableView::VectorTableView(QWidget *parent)
    : QTableView(parent),
      m_contextMenu(nullptr),
      m_addRowAction(nullptr),
      m_removeRowAction(nullptr),
      m_fillVectorAction(nullptr),
      m_fillTimeSetAction(nullptr),
      m_replaceTimeSetAction(nullptr)
{
    qDebug() << "VectorTableView::VectorTableView - 创建新的向量表视图实例";
    
    // 设置视图属性
    setAlternatingRowColors(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    setContextMenuPolicy(Qt::CustomContextMenu);
    
    // 设置表头属性
    horizontalHeader()->setStretchLastSection(true);
    verticalHeader()->setDefaultSectionSize(25);
    
    // 创建上下文菜单
    m_contextMenu = createContextMenu();
    
    // 连接自定义上下文菜单请求信号
    connect(this, &QTableView::customContextMenuRequested,
            [this](const QPoint &pos) {
                if (m_contextMenu)
                    m_contextMenu->exec(viewport()->mapToGlobal(pos));
            });
}

VectorTableView::~VectorTableView()
{
    qDebug() << "VectorTableView::~VectorTableView - 销毁向量表视图实例";
    
    // QObject会自动清理子对象（包括m_contextMenu及其动作）
}

void VectorTableView::setVectorTableModel(VectorTableModel *model)
{
    qDebug() << "VectorTableView::setVectorTableModel - 设置向量表模型";
    setModel(model);
}

VectorTableModel* VectorTableView::vectorTableModel() const
{
    return qobject_cast<VectorTableModel*>(model());
}

void VectorTableView::contextMenuEvent(QContextMenuEvent *event)
{
    if (m_contextMenu) {
        // 更新菜单项状态
        bool hasSelection = !selectionModel()->selectedRows().isEmpty();
        if (m_removeRowAction)
            m_removeRowAction->setEnabled(hasSelection);
        if (m_fillVectorAction)
            m_fillVectorAction->setEnabled(hasSelection);
        if (m_fillTimeSetAction)
            m_fillTimeSetAction->setEnabled(hasSelection);
        if (m_replaceTimeSetAction)
            m_replaceTimeSetAction->setEnabled(hasSelection);
        
        // 显示菜单
        m_contextMenu->exec(event->globalPos());
    }
}

void VectorTableView::keyPressEvent(QKeyEvent *event)
{
    // 处理键盘事件（例如Delete键删除行）
    if (event->key() == Qt::Key_Delete) {
        onRemoveRowsTriggered();
        return;
    }
    
    // 其他键交给基类处理
    QTableView::keyPressEvent(event);
}

void VectorTableView::onAddRowTriggered()
{
    qDebug() << "VectorTableView::onAddRowTriggered - 请求添加行";
    emit requestAddRow();
}

void VectorTableView::onRemoveRowsTriggered()
{
    QList<int> selectedRows = getSelectedRows();
    if (selectedRows.isEmpty())
        return;
    
    qDebug() << "VectorTableView::onRemoveRowsTriggered - 请求删除" << selectedRows.count() << "行";
    emit requestRemoveRows(selectedRows);
}

void VectorTableView::onFillVectorTriggered()
{
    QList<int> selectedRows = getSelectedRows();
    if (selectedRows.isEmpty())
        return;
    
    qDebug() << "VectorTableView::onFillVectorTriggered - 请求填充向量，选中" << selectedRows.count() << "行";
    emit requestFillVector(selectedRows);
}

void VectorTableView::onFillTimeSetTriggered()
{
    QList<int> selectedRows = getSelectedRows();
    if (selectedRows.isEmpty())
        return;
    
    qDebug() << "VectorTableView::onFillTimeSetTriggered - 请求填充TimeSet，选中" << selectedRows.count() << "行";
    emit requestFillTimeSet(selectedRows);
}

void VectorTableView::onReplaceTimeSetTriggered()
{
    QList<int> selectedRows = getSelectedRows();
    if (selectedRows.isEmpty())
        return;
    
    qDebug() << "VectorTableView::onReplaceTimeSetTriggered - 请求替换TimeSet，选中" << selectedRows.count() << "行";
    emit requestReplaceTimeSet(selectedRows);
}

QMenu* VectorTableView::createContextMenu()
{
    QMenu *menu = new QMenu(this);
    
    // 添加行动作
    m_addRowAction = menu->addAction(tr("添加行"));
    connect(m_addRowAction, &QAction::triggered, this, &VectorTableView::onAddRowTriggered);
    
    // 删除行动作
    m_removeRowAction = menu->addAction(tr("删除行"));
    connect(m_removeRowAction, &QAction::triggered, this, &VectorTableView::onRemoveRowsTriggered);
    
    menu->addSeparator();
    
    // 填充向量动作
    m_fillVectorAction = menu->addAction(tr("填充向量..."));
    connect(m_fillVectorAction, &QAction::triggered, this, &VectorTableView::onFillVectorTriggered);
    
    // 填充TimeSet动作
    m_fillTimeSetAction = menu->addAction(tr("填充TimeSet..."));
    connect(m_fillTimeSetAction, &QAction::triggered, this, &VectorTableView::onFillTimeSetTriggered);
    
    // 替换TimeSet动作
    m_replaceTimeSetAction = menu->addAction(tr("替换TimeSet..."));
    connect(m_replaceTimeSetAction, &QAction::triggered, this, &VectorTableView::onReplaceTimeSetTriggered);
    
    return menu;
}

QList<int> VectorTableView::getSelectedRows() const
{
    QList<int> rows;
    QModelIndexList selectedIndexes = selectionModel()->selectedRows();
    
    for (const QModelIndex &index : selectedIndexes) {
        if (!rows.contains(index.row())) {
            rows.append(index.row());
        }
    }
    
    // 按行号排序
    std::sort(rows.begin(), rows.end());
    
    return rows;
} 