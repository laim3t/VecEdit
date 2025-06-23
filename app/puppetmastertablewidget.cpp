#include "puppetmastertablewidget.h"
#include <QVBoxLayout>
#include <QHeaderView>

PuppetMasterTableWidget::PuppetMasterTableWidget(VectorDataHandler *dataHandler, QWidget *parent)
    : QTableWidget(parent), m_isSyncing(false)
{
    // 1. 创建内部核心组件
    m_model = new VectorTableModel(dataHandler, this);
    m_view = new QTableView(this);
    m_view->setModel(m_model);

    // 2. 将真实视图放入布局，让它填满整个控件
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);
    this->setLayout(layout);

    // 3. 建立双向同步连接
    // View -> Puppet
    connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, this, &PuppetMasterTableWidget::syncSelectionFromViewToPuppet);
    // Model -> Puppet
    connect(m_model, &VectorTableModel::dataChanged, this, &PuppetMasterTableWidget::syncDataChangeFromModelToPuppet);
    connect(m_model, &QAbstractItemModel::modelReset, this, &PuppetMasterTableWidget::syncResetFromModelToPuppet);
    // Puppet -> Model
    connect(this, &QTableWidget::itemChanged, this, &PuppetMasterTableWidget::syncItemChangeFromPuppetToModel);
    connect(this->selectionModel(), &QItemSelectionModel::selectionChanged, this, &PuppetMasterTableWidget::syncSelectionFromPuppetToView);
}

QTableWidgetItem *PuppetMasterTableWidget::item(int row, int column) const
{
    // 这是关键的API重写，它会在每次旧代码尝试访问项目时被调用
    // 我们确保该单元格在传统表格中存在，这样旧代码就可以继续工作
    QTableWidgetItem *existingItem = QTableWidget::item(row, column);
    if (!existingItem)
    {
        // 如果项目不存在，则即时创建
        const_cast<PuppetMasterTableWidget *>(this)->setItem(row, column, new QTableWidgetItem());
        existingItem = QTableWidget::item(row, column);
    }
    return existingItem;
}

void PuppetMasterTableWidget::syncSelectionFromViewToPuppet()
{
    if (m_isSyncing)
        return;
    m_isSyncing = true;

    // 清除旧选择
    this->clearSelection();

    // 获取视图的选择模型
    QItemSelectionModel *viewSelection = m_view->selectionModel();
    if (viewSelection->hasSelection())
    {
        // 遍历视图的所有选择范围
        foreach (const QItemSelectionRange &range, viewSelection->selection())
        {
            // 将每个范围转换为旧表格的选择范围
            for (int row = range.top(); row <= range.bottom(); ++row)
            {
                for (int col = range.left(); col <= range.right(); ++col)
                {
                    // 确保单元格存在，然后选中它
                    item(row, col);
                    this->setCurrentCell(row, col, QItemSelectionModel::Select);
                }
            }
        }
    }

    m_isSyncing = false;
}

void PuppetMasterTableWidget::syncSelectionFromPuppetToView()
{
    if (m_isSyncing)
        return;
    m_isSyncing = true;

    // 清除视图的旧选择
    m_view->clearSelection();

    // 获取旧表格的选择模型
    QItemSelectionModel *puppetSelection = this->selectionModel();
    if (puppetSelection->hasSelection())
    {
        // 获取所有选中的单元格
        QList<QTableWidgetSelectionRange> ranges = this->selectedRanges();

        // 为每个范围创建新的选择模型范围
        QItemSelection newSelection;
        foreach (const QTableWidgetSelectionRange &range, ranges)
        {
            QModelIndex topLeft = m_model->index(range.topRow(), range.leftColumn());
            QModelIndex bottomRight = m_model->index(range.bottomRow(), range.rightColumn());
            newSelection.append(QItemSelectionRange(topLeft, bottomRight));
        }

        // 将新范围应用到视图的选择模型
        m_view->selectionModel()->select(newSelection, QItemSelectionModel::ClearAndSelect);
    }

    m_isSyncing = false;
}

void PuppetMasterTableWidget::syncDataChangeFromModelToPuppet(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    if (m_isSyncing)
        return;
    m_isSyncing = true;

    // 将模型的数据变更同步到旧表格
    for (int row = topLeft.row(); row <= bottomRight.row(); ++row)
    {
        for (int col = topLeft.column(); col <= bottomRight.column(); ++col)
        {
            QModelIndex idx = m_model->index(row, col);
            QVariant data = m_model->data(idx, Qt::DisplayRole);

            // 确保项目存在
            QTableWidgetItem *tableItem = item(row, col);
            tableItem->setData(Qt::DisplayRole, data);
        }
    }

    m_isSyncing = false;
}

void PuppetMasterTableWidget::syncResetFromModelToPuppet()
{
    if (m_isSyncing)
        return;
    m_isSyncing = true;

    // 模型重置时，清空并重建旧表格
    this->clearContents();
    this->setRowCount(m_model->rowCount());
    this->setColumnCount(m_model->columnCount());

    // 设置表头
    for (int col = 0; col < m_model->columnCount(); ++col)
    {
        QVariant headerText = m_model->headerData(col, Qt::Horizontal, Qt::DisplayRole);
        this->setHorizontalHeaderItem(col, new QTableWidgetItem(headerText.toString()));
    }

    m_isSyncing = false;
}

void PuppetMasterTableWidget::syncItemChangeFromPuppetToModel(QTableWidgetItem *item)
{
    if (m_isSyncing || !item)
        return;
    m_isSyncing = true;

    // 获取项目的位置和数据
    int row = item->row();
    int col = item->column();
    QVariant data = item->data(Qt::DisplayRole);

    // 更新模型数据
    QModelIndex idx = m_model->index(row, col);
    m_model->setData(idx, data, Qt::EditRole);

    m_isSyncing = false;
}