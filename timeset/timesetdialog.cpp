#include "timesetdialog.h"
#include "timesetedgedialog.h"
#include "database/databasemanager.h"
#include "app/mainwindow.h"
#include <QApplication>

#include <QMessageBox>
#include <QInputDialog>
#include <QDebug>
#include <QComboBox>
#include <QBrush>
#include <QColor>
#include <QFont>
#include <QSqlQuery>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>

// WaveComboDelegate 实现
WaveComboDelegate::WaveComboDelegate(const QMap<int, QString> &waveOptions, QObject *parent)
    : QStyledItemDelegate(parent), m_waveOptions(waveOptions)
{
}

QWidget *WaveComboDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                                         const QModelIndex &index) const
{
    // 创建下拉框
    QComboBox *editor = new QComboBox(parent);
    editor->setFrame(false);

    // 添加所有波形选项
    for (auto it = m_waveOptions.begin(); it != m_waveOptions.end(); ++it)
    {
        editor->addItem(it.value(), it.key());
    }

    return editor;
}

void WaveComboDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    // 获取当前值
    QString value = index.model()->data(index, Qt::EditRole).toString();

    // 设置下拉框当前选项
    QComboBox *comboBox = static_cast<QComboBox *>(editor);
    int idx = comboBox->findText(value);
    if (idx >= 0)
    {
        comboBox->setCurrentIndex(idx);
    }
}

void WaveComboDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                     const QModelIndex &index) const
{
    // 获取下拉框选择的值
    QComboBox *comboBox = static_cast<QComboBox *>(editor);
    QString value = comboBox->currentText();

    // 更新模型数据
    model->setData(index, value, Qt::EditRole);
}

void WaveComboDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                                             const QModelIndex &index) const
{
    // 设置编辑器的几何位置
    editor->setGeometry(option.rect);
}

// TimeSetDialog 实现
TimeSetDialog::TimeSetDialog(QWidget *parent, bool isInitialSetup)
    : QDialog(parent),
      m_mainWindow(qobject_cast<MainWindow *>(parent)),
      m_currentTimeSetItem(nullptr),
      m_currentTimeSetIndex(-1),
      m_isInitialSetup(isInitialSetup)
{
    // 设置窗口属性
    setWindowTitle("TimeSet 设置");
    setMinimumSize(800, 600);

    // 设置全局样式
    QString styleSheet = "QTreeWidget {"
                         "    border: 1px solid #C0C0C0;"
                         "    background-color: white;"
                         "    font-size: 9pt;"
                         "}"
                         "QTreeWidget::item {"
                         "    border-bottom: 1px solid #F0F0F0;"
                         "    height: 25px;"
                         "}"
                         "QTreeWidget::item:selected {"
                         "    background-color: #E0E8F0;"
                         "    color: black;"
                         "}"
                         "QTreeWidget::branch:has-children:!has-siblings:closed,"
                         "QTreeWidget::branch:closed:has-children:has-siblings {"
                         "    border-image: none;"
                         "    image: url(:/icons/branch-closed.png);"
                         "}"
                         "QTreeWidget::branch:open:has-children:!has-siblings,"
                         "QTreeWidget::branch:open:has-children:has-siblings {"
                         "    border-image: none;"
                         "    image: url(:/icons/branch-open.png);"
                         "}"
                         "QHeaderView::section {"
                         "    background-color: #F0F0F0;"
                         "    padding: 5px;"
                         "    border: 1px solid #D0D0D0;"
                         "    font-weight: bold;"
                         "}";
    setStyleSheet(styleSheet);

    // 初始化
    initialize();

    // 设置UI连接
    setupConnections();
}

TimeSetDialog::~TimeSetDialog()
{
    // 释放内存
    delete m_uiManager;
    delete m_dataAccess;
    delete m_edgeManager;
    delete m_pinManager;
    delete m_vectorManager;
    delete m_waveDelegate;
}

void TimeSetDialog::initialize()
{
    // 初始化成员变量
    m_timeSetIdsToDelete.clear();

    // 初始化数据库连接
    m_db = DatabaseManager::instance()->database();

    // 创建数据访问层
    m_dataAccess = new TimeSetDataAccess(m_db);

    // 加载基础数据
    m_dataAccess->loadWaveOptions(m_waveOptions);
    m_dataAccess->loadPins(m_pinList);

    // 创建UI管理器
    m_uiManager = new TimeSetUIManager(this);

    // 创建边缘管理器
    m_edgeManager = new TimeSetEdgeManager(m_uiManager->getTimeSetTree(), m_dataAccess);

    // 创建管脚选择管理器
    m_pinManager = new PinSelectionManager(m_uiManager->getPinListWidget(), m_dataAccess);
    m_pinManager->populatePinList(m_pinList);

    // 创建向量数据管理器
    m_vectorManager = new VectorDataManager(m_dataAccess);

    // 创建波形委托
    m_waveDelegate = new WaveComboDelegate(m_waveOptions, this);
    m_uiManager->getTimeSetTree()->setItemDelegateForColumn(4, m_waveDelegate);

    // 加载现有TimeSet设置
    if (!loadExistingTimeSets())
    {
        // 如果加载失败，显示错误信息
        QMessageBox::warning(this, "加载错误", "TimeSet数据加载失败，请检查数据库连接。");
    }

    // 更新所有边沿项的显示格式
    m_edgeManager->updateAllEdgeItemsDisplay(m_waveOptions);
}

bool TimeSetDialog::loadExistingTimeSets()
{
    QTreeWidget *timeSetTree = m_uiManager->getTimeSetTree();
    timeSetTree->clear();

    qDebug() << "开始加载TimeSet列表到UI";

    // 从数据库加载TimeSet列表
    if (m_dataAccess->loadExistingTimeSets(m_timeSetDataList))
    {
        qDebug() << "成功从数据库加载，TimeSet数量：" << m_timeSetDataList.size();

        // 添加每个TimeSet到树中
        for (const TimeSetData &timeSet : m_timeSetDataList)
        {
            // 创建TimeSet项
            QTreeWidgetItem *timeSetItem = new QTreeWidgetItem(timeSetTree);

            // 设置数据
            double period = timeSet.period;
            double freq = 1000.0 / period;
            timeSetItem->setText(0, timeSet.name + "/" + QString::number(period) + "ns=" + QString::number(freq, 'f', 3) + "MHz");
            timeSetItem->setData(0, Qt::UserRole, timeSet.dbId);

            // 设置字体和背景样式
            QFont boldFont = timeSetItem->font(0);
            boldFont.setBold(true);
            boldFont.setPointSize(boldFont.pointSize() + 1);
            timeSetItem->setFont(0, boldFont);

            // 设置背景色
            QBrush brush(QColor(230, 240, 250));
            for (int col = 0; col < 5; col++)
            {
                timeSetItem->setBackground(col, brush);
            }

            // 加载并显示边沿参数
            QList<TimeSetEdgeData> edges = m_dataAccess->loadTimeSetEdges(timeSet.dbId);
            m_edgeManager->displayTimeSetEdges(timeSetItem, edges, m_waveOptions, m_pinList);

            // 展开
            timeSetItem->setExpanded(true);

            qDebug() << "已添加TimeSet到UI: id=" << timeSet.dbId << ", name=" << timeSet.name;
        }

        return true;
    }
    else
    {
        qWarning() << "加载TimeSet设置失败";
        return false;
    }
}

void TimeSetDialog::setupConnections()
{
    // 连接按钮信号
    connect(m_uiManager->getAddTimeSetButton(), &QPushButton::clicked, this, &TimeSetDialog::addTimeSet);
    connect(m_uiManager->getRemoveTimeSetButton(), &QPushButton::clicked, this, &TimeSetDialog::removeTimeSet);
    connect(m_uiManager->getAddEdgeButton(), &QPushButton::clicked, this, &TimeSetDialog::addEdgeItem);
    connect(m_uiManager->getRemoveEdgeButton(), &QPushButton::clicked, this, &TimeSetDialog::removeEdgeItem);

    // 连接TreeWidget信号
    connect(m_uiManager->getTimeSetTree(), &QTreeWidget::itemSelectionChanged, this, &TimeSetDialog::timeSetSelectionChanged);
    connect(m_uiManager->getTimeSetTree(), &QTreeWidget::itemChanged, this, &TimeSetDialog::onPropertyItemChanged);
    connect(m_uiManager->getTimeSetTree(), &QTreeWidget::itemDoubleClicked, this, &TimeSetDialog::onItemDoubleClicked);

    // 连接对话框按钮
    connect(m_uiManager->getButtonBox(), &QDialogButtonBox::accepted, this, &TimeSetDialog::onAccepted);
    connect(m_uiManager->getButtonBox(), &QDialogButtonBox::rejected, this, &TimeSetDialog::onRejected);
}

void TimeSetDialog::addTimeSet()
{
    bool ok;
    QString defaultName = "timeset_" + QString::number(m_timeSetDataList.size() + 1);
    QString name = QInputDialog::getText(this, "添加TimeSet",
                                         "TimeSet名称:", QLineEdit::Normal,
                                         defaultName, &ok);
    if (ok && !name.isEmpty())
    {
        qDebug() << "添加新TimeSet: 用户输入名称 =" << name;

        // 检查名称是否已存在
        if (m_dataAccess->isTimeSetNameExists(name))
        {
            qDebug() << "名称已存在，拒绝创建新TimeSet";
            QMessageBox::warning(this, "名称重复", "TimeSet名称已存在，请使用其他名称。");
            return;
        }

        // 检查数据库连接
        if (!DatabaseManager::instance()->isDatabaseConnected())
        {
            QMessageBox::critical(this, "数据库错误", "数据库未连接，请重新启动应用程序。");
            return;
        }

        // 创建新的TimeSet数据
        TimeSetData newTimeSet;
        newTimeSet.dbId = 0; // 新项的ID为0
        newTimeSet.name = name;
        newTimeSet.period = 1000; // 默认1000ns
        newTimeSet.pinIds.clear();

        qDebug() << "准备保存新TimeSet到数据库: name=" << newTimeSet.name << ", period=" << newTimeSet.period;

        // 保存到数据库
        int newTimeSetId = 0;
        if (m_dataAccess->saveTimeSetToDatabase(newTimeSet, newTimeSetId))
        {
            qDebug() << "新TimeSet保存成功: id=" << newTimeSetId;

            // 更新ID
            newTimeSet.dbId = newTimeSetId;
            m_timeSetDataList.append(newTimeSet);

            // 创建TreeWidget项
            QTreeWidget *timeSetTree = m_uiManager->getTimeSetTree();
            QTreeWidgetItem *newItem = new QTreeWidgetItem(timeSetTree);

            // 设置数据
            double freq = 1000.0 / newTimeSet.period;
            newItem->setText(0, newTimeSet.name + "/" + QString::number(newTimeSet.period) + "ns=" + QString::number(freq, 'f', 3) + "MHz");
            newItem->setData(0, Qt::UserRole, newTimeSet.dbId);

            // 设置字体和背景样式
            QFont boldFont = newItem->font(0);
            boldFont.setBold(true);
            boldFont.setPointSize(boldFont.pointSize() + 1);
            newItem->setFont(0, boldFont);

            // 设置背景色
            QBrush brush(QColor(230, 240, 250));
            for (int col = 0; col < 5; col++)
            {
                newItem->setBackground(col, brush);
            }

            // 选中新项
            timeSetTree->setCurrentItem(newItem);
            timeSetSelectionChanged();

            // 在操作失败后刷新TimeSet列表，确保UI和数据库一致
            loadExistingTimeSets();
        }
        else
        {
            qDebug() << "创建TimeSet失败";
            QMessageBox::critical(this, "错误", "创建TimeSet失败，请检查数据库日志以获取详细信息。\n可能是数据库连接问题或权限问题。");

            // 在操作失败后刷新TimeSet列表，确保UI和数据库一致
            loadExistingTimeSets();
        }
    }
}

void TimeSetDialog::removeTimeSet()
{
    if (!m_currentTimeSetItem)
        return;

    int timeSetId = m_currentTimeSetItem->data(0, Qt::UserRole).toInt();
    QString timeSetName = getTimeSetNameById(timeSetId); // 使用辅助函数获取名称

    if (timeSetName.isEmpty())
    { // 如果找不到名称，则不进行操作
        qWarning() << "无法找到TimeSet ID:" << timeSetId << " 的名称";
        return;
    }

    // 确认删除
    if (QMessageBox::question(this, "确认删除",
                              "确定要删除TimeSet '" + timeSetName + "' 吗？",
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
    {
        // 将TimeSet ID添加到待删除列表
        if (!m_timeSetIdsToDelete.contains(timeSetId))
        {
            m_timeSetIdsToDelete.append(timeSetId);
            qDebug() << "TimeSetDialog::removeTimeSet - 将TimeSet ID添加到待删除列表:" << timeSetId;
        }
        else
        {
            qDebug() << "TimeSetDialog::removeTimeSet - TimeSet ID已在待删除列表中:" << timeSetId;
        }

        // 从UI中移除该项 (稍后如果验证失败会恢复)
        qDebug() << "TimeSetDialog::removeTimeSet - 从UI移除TimeSet项:" << timeSetId;
        int indexToRemove = m_uiManager->getTimeSetTree()->indexOfTopLevelItem(m_currentTimeSetItem);
        if (indexToRemove != -1)
        {
            // 正确删除子项：手动遍历列表并删除每个子项指针
            QList<QTreeWidgetItem *> children = m_currentTimeSetItem->takeChildren(); // 获取并移除子项列表
            for (QTreeWidgetItem *child : children)
            {
                delete child; // 逐个删除子项
            }
            // children 列表本身是栈变量，会自动销毁，无需手动 clear()

            // 删除父项本身
            delete m_uiManager->getTimeSetTree()->takeTopLevelItem(indexToRemove);
        }

        // 更新当前选择
        m_currentTimeSetItem = nullptr;
        m_currentTimeSetIndex = -1;
        timeSetSelectionChanged();
    }
}

void TimeSetDialog::editTimeSetProperties(QTreeWidgetItem *item, int column)
{
    if (!item || column != 0 || item->parent() != nullptr)
        return;

    int timeSetId = item->data(0, Qt::UserRole).toInt();
    QString currentName = "";
    double currentPeriod = 0.0;

    // 从 m_timeSetDataList 获取当前值
    int dataIndex = -1;
    for (int i = 0; i < m_timeSetDataList.size(); ++i)
    {
        if (m_timeSetDataList[i].dbId == timeSetId)
        {
            currentName = m_timeSetDataList[i].name;
            currentPeriod = m_timeSetDataList[i].period;
            dataIndex = i;
            break;
        }
    }

    if (dataIndex == -1)
    {
        qWarning() << "editTimeSetProperties: 未能在 m_timeSetDataList 中找到 TimeSet ID:" << timeSetId;
        return;
    }

    // 创建自定义对话框
    QDialog dialog(this);
    dialog.setWindowTitle("编辑 TimeSet 属性");

    QFormLayout *formLayout = new QFormLayout(&dialog);

    QLineEdit *nameEdit = new QLineEdit(currentName, &dialog);
    QDoubleSpinBox *periodSpinBox = new QDoubleSpinBox(&dialog);
    periodSpinBox->setSuffix(" ns");
    periodSpinBox->setDecimals(3);           // 根据需要调整精度
    periodSpinBox->setMinimum(0.001);        // 最小周期，例如 1ps
    periodSpinBox->setMaximum(1000000000.0); // 最大周期，例如 1s (1e9 ns)
    periodSpinBox->setValue(currentPeriod);

    formLayout->addRow("新名称:", nameEdit);
    formLayout->addRow("周期:", periodSpinBox);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    formLayout->addWidget(buttonBox);

    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted)
    {
        QString newName = nameEdit->text().trimmed();
        double newPeriod = periodSpinBox->value();

        // 验证
        if (newName.isEmpty())
        {
            QMessageBox::warning(this, "无效输入", "TimeSet 名称不能为空。");
            return;
        }
        if (newPeriod <= 0)
        {
            QMessageBox::warning(this, "无效输入", "周期必须为正数。");
            return;
        }

        bool nameChanged = (newName != currentName);
        bool periodChanged = (newPeriod != currentPeriod);

        // 检查名称冲突（如果名称改变了）
        if (nameChanged && m_dataAccess->isTimeSetNameExists(newName))
        {
            // 需要排除自身ID进行检查，但isTimeSetNameExists目前不接受排除ID
            // 暂时简单处理：如果新名称存在，则拒绝更改。
            // TODO: 改进 isTimeSetNameExists 或添加新方法以支持排除ID检查
            QMessageBox::warning(this, "名称冲突", QString("TimeSet 名称 '%1' 已存在。").arg(newName));
            return;
        }

        if (nameChanged || periodChanged)
        {
            qDebug() << "TimeSet ID:" << timeSetId << "属性已更改。新名称:" << newName << ", 新周期:" << newPeriod;

            // 1. 更新内存中的数据
            m_timeSetDataList[dataIndex].name = newName;
            m_timeSetDataList[dataIndex].period = newPeriod;

            // 2. 更新UI TreeWidget Item 的显示文本 (直接格式化)
            double freq = (newPeriod > 0) ? (1000.0 / newPeriod) : 0.0;
            QString formattedText = QString("%1/%2ns=%3MHz")
                                        .arg(newName)
                                        .arg(QString::number(newPeriod)) // 保持原始精度
                                        .arg(QString::number(freq, 'f', 3));
            item->setText(0, formattedText);

            // 数据库的更新将在点击对话框的 OK 时在 onAccepted() 中处理
        }
        else
        {
            qDebug() << "TimeSet ID:" << timeSetId << "属性未更改。";
        }
    }
}

void TimeSetDialog::updatePeriod(double value)
{
    if (!m_currentTimeSetItem || m_currentTimeSetIndex < 0 || m_currentTimeSetIndex >= m_timeSetDataList.size())
        return;

    // 获取当前TimeSet数据
    TimeSetData &timeSet = m_timeSetDataList[m_currentTimeSetIndex];

    // 更新周期
    if (m_dataAccess->updateTimeSetPeriod(timeSet.dbId, value))
    {
        // 更新内存中的数据
        timeSet.period = value;

        // 更新显示
        double freq = 1000.0 / value;
        m_currentTimeSetItem->setText(0, timeSet.name + "/" + QString::number(value) + "ns=" + QString::number(freq, 'f', 3) + "MHz");
    }
    else
    {
        QMessageBox::critical(this, "错误", "更新TimeSet周期失败");
    }
}

void TimeSetDialog::timeSetSelectionChanged()
{
    QTreeWidget *timeSetTree = m_uiManager->getTimeSetTree();
    QList<QTreeWidgetItem *> selectedItems = timeSetTree->selectedItems();

    if (selectedItems.isEmpty())
    {
        // 没有选中项，禁用边沿操作按钮
        m_uiManager->getAddEdgeButton()->setEnabled(false);
        m_uiManager->getRemoveEdgeButton()->setEnabled(false);
        m_currentTimeSetItem = nullptr;
        m_currentTimeSetIndex = -1;
        return;
    }

    QTreeWidgetItem *selectedItem = selectedItems.first();

    // 检查是否是顶级项（TimeSet项）
    if (selectedItem->parent() == nullptr)
    {
        // 是TimeSet项
        m_currentTimeSetItem = selectedItem;
        m_currentTimeSetIndex = timeSetTree->indexOfTopLevelItem(selectedItem);

        // 启用边沿操作按钮
        m_uiManager->getAddEdgeButton()->setEnabled(true);
        m_uiManager->getRemoveEdgeButton()->setEnabled(false);

        // 更新管脚选择
        int timeSetId = selectedItem->data(0, Qt::UserRole).toInt();
        m_pinManager->selectPinsForTimeSet(timeSetId);
    }
    else
    {
        // 是边沿项
        m_currentTimeSetItem = selectedItem->parent();
        m_currentTimeSetIndex = timeSetTree->indexOfTopLevelItem(m_currentTimeSetItem);

        // 启用边沿操作按钮
        m_uiManager->getAddEdgeButton()->setEnabled(true);
        m_uiManager->getRemoveEdgeButton()->setEnabled(true);

        // 更新管脚选择
        int timeSetId = m_currentTimeSetItem->data(0, Qt::UserRole).toInt();
        m_pinManager->selectPinsForTimeSet(timeSetId);
    }
}

void TimeSetDialog::addEdgeItem()
{
    if (!m_currentTimeSetItem)
        return;

    // 获取当前选中的TimeSet ID
    int timeSetId = m_currentTimeSetItem->data(0, Qt::UserRole).toInt();

    // 获取选中的管脚IDs
    QList<int> selectedPinIds = m_pinManager->getSelectedPinIds();

    if (selectedPinIds.isEmpty())
    {
        QMessageBox::warning(this, "未选择管脚", "请先选择要添加的管脚。");
        return;
    }

    // 获取已存在的边沿项的管脚IDs
    QSet<int> existingPinIds;
    for (int i = 0; i < m_currentTimeSetItem->childCount(); i++)
    {
        QTreeWidgetItem *child = m_currentTimeSetItem->child(i);
        existingPinIds.insert(child->data(0, Qt::UserRole).toInt());
    }

    // 设置默认值
    double defaultT1R = 250;
    double defaultT1F = 750;
    double defaultSTBR = 500;
    int defaultWaveId = m_waveOptions.keys().first(); // 使用第一个波形ID

    // 创建边沿对话框
    TimeSetEdgeDialog dialog(defaultT1R, defaultT1F, defaultSTBR, defaultWaveId, m_waveOptions, this);

    if (dialog.exec() == QDialog::Accepted)
    {
        // 获取用户设置的值
        double t1r = dialog.getT1R();
        double t1f = dialog.getT1F();
        double stbr = dialog.getSTBR();
        int waveId = dialog.getWaveId();

        // 创建边沿数据并添加到UI
        QList<TimeSetEdgeData> newEdges;

        for (int pinId : selectedPinIds)
        {
            // 跳过已存在的管脚
            if (existingPinIds.contains(pinId))
                continue;

            TimeSetEdgeData edge;
            edge.timesetId = timeSetId;
            edge.pinId = pinId;
            edge.t1r = t1r;
            edge.t1f = t1f;
            edge.stbr = stbr;
            edge.waveId = waveId;

            // 添加到UI
            m_edgeManager->addEdgeItem(m_currentTimeSetItem, edge);

            // 添加到列表
            newEdges.append(edge);
        }

        // 获取所有边沿数据并保存到数据库
        if (!newEdges.isEmpty())
        {
            QList<TimeSetEdgeData> allEdges = m_edgeManager->getEdgeDataFromUI(m_currentTimeSetItem, timeSetId);
            m_dataAccess->saveTimeSetEdgesToDatabase(timeSetId, allEdges);

            // 重新加载边沿项以更新显示
            QList<TimeSetEdgeData> edges = m_dataAccess->loadTimeSetEdges(timeSetId);
            m_edgeManager->displayTimeSetEdges(m_currentTimeSetItem, edges, m_waveOptions, m_pinList);
        }
    }
}

void TimeSetDialog::removeEdgeItem()
{
    QTreeWidget *timeSetTree = m_uiManager->getTimeSetTree();
    QList<QTreeWidgetItem *> selectedItems = timeSetTree->selectedItems();

    if (selectedItems.isEmpty())
        return;

    QTreeWidgetItem *selectedItem = selectedItems.first();

    // 确保是边沿项
    if (selectedItem->parent() == nullptr)
        return;

    // 删除边沿项
    if (m_edgeManager->removeEdgeItem(selectedItem))
    {
        // 更新UI状态
        timeSetSelectionChanged();
    }
    else
    {
        QMessageBox::critical(this, "错误", "删除边沿参数失败");
    }
}

void TimeSetDialog::editEdgeItem(QTreeWidgetItem *item, int column)
{
    m_edgeManager->editEdgeItem(item, column, m_waveOptions);
}

void TimeSetDialog::onItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    if (!item)
        return;

    if (item->parent() == nullptr)
    {
        // TimeSet项
        editTimeSetProperties(item, column);
    }
    else
    {
        // 边沿项
        editEdgeItem(item, column);
    }
}

void TimeSetDialog::onPropertyItemChanged(QTreeWidgetItem *item, int column)
{
    // 此方法处理项目属性变更
    // 注意：由于编辑操作（如重命名、修改边沿参数）会在完成后直接更新UI项，
    // 这可能会触发itemChanged信号。为了避免在非用户直接编辑时触发不必要的逻辑（特别是保存），
    // 这个槽函数目前保持为空，或者只包含非常轻量级的UI更新逻辑。
    // 数据的保存应该在用户明确确认操作（如点击OK按钮）时进行。
    Q_UNUSED(item);
    Q_UNUSED(column);
    // 注释掉原来的保存逻辑：
    /*
    if (!item)
        return;

    // 根据项的类型处理不同的更改
    if (item->parent() == nullptr)
    {
        // TimeSet项的更改
        // 这里不处理，因为已经在editTimeSetProperties方法中处理
    }
    else
    {
        // 边沿项的更改
        // 获取父TimeSet项
        QTreeWidgetItem *parentItem = item->parent();
        int timeSetId = parentItem->data(0, Qt::UserRole).toInt();

        // 获取所有边沿数据并保存 (问题在这里！)
        QList<TimeSetEdgeData> edges = m_edgeManager->getEdgeDataFromUI(parentItem, timeSetId);
        m_dataAccess->saveTimeSetEdgesToDatabase(timeSetId, edges);
    }
    */
}

void TimeSetDialog::onPinSelectionChanged()
{
    if (!m_currentTimeSetItem || m_currentTimeSetIndex < 0 || m_currentTimeSetIndex >= m_timeSetDataList.size())
        return;

    // 获取当前TimeSet数据
    TimeSetData &timeSet = m_timeSetDataList[m_currentTimeSetIndex];

    // 获取选中的管脚IDs
    QList<int> selectedPinIds = m_pinManager->getSelectedPinIds();

    // 更新TimeSet的管脚关联
    timeSet.pinIds = selectedPinIds;

    // 保存到数据库
    m_dataAccess->savePinSelection(timeSet.dbId, selectedPinIds);
}

void TimeSetDialog::updatePinSelection(QTreeWidgetItem *item, int column)
{
    if (!item)
        return;

    // 获取当前TimeSet
    QTreeWidgetItem *timeSetItem = (item->parent() == nullptr) ? item : item->parent();
    int timeSetId = timeSetItem->data(0, Qt::UserRole).toInt();

    // 更新管脚选择
    m_pinManager->selectPinsForTimeSet(timeSetId);
}

QString TimeSetDialog::getTimeSetNameById(int id) const
{
    for (const auto &timeSet : m_timeSetDataList)
    {
        if (timeSet.dbId == id)
        {
            return timeSet.name;
        }
    }
    // 如果本地列表找不到（可能已经被标记为删除但尚未从列表移除）
    // 可以考虑再查一次数据库，但目前假设列表数据是相对完整的
    return QString(); // 返回空字符串表示未找到
}

void TimeSetDialog::onAccepted()
{
    qDebug() << "TimeSetDialog::onAccepted - 开始处理确定按钮事件";

    // --- 删除验证 ---
    QStringList conflictingNames;
    QList<int> validatedIdsToDelete;
    QList<int> conflictingIds;

    qDebug() << "TimeSetDialog::onAccepted - 检查待删除TimeSet列表:" << m_timeSetIdsToDelete.size() << "个";

    for (int idToDelete : m_timeSetIdsToDelete)
    {
        if (m_dataAccess->isTimeSetInUse(idToDelete))
        {
            QString name = getTimeSetNameById(idToDelete);
            if (!name.isEmpty())
            {
                conflictingNames.append(name);
            }
            else
            {
                conflictingNames.append(QString::number(idToDelete)); // 如果找不到名字，显示ID
            }
            conflictingIds.append(idToDelete);
            qDebug() << "TimeSetDialog::onAccepted - TimeSet ID:" << idToDelete << "(" << name << ") 正在使用中";
        }
        else
        {
            validatedIdsToDelete.append(idToDelete);
            qDebug() << "TimeSetDialog::onAccepted - TimeSet ID:" << idToDelete << "(" << getTimeSetNameById(idToDelete) << ") 未在使用，可以删除";
        }
    }

    // 如果存在冲突
    if (!conflictingNames.isEmpty())
    {
        qDebug() << "TimeSetDialog::onAccepted - 检测到删除冲突";
        QMessageBox::warning(this, "删除冲突",
                             QString("以下TimeSet正在被向量表使用，无法删除：\n - %1\n\n请先在向量表中修改或删除对这些TimeSet的引用。").arg(conflictingNames.join("\n - ")));

        // 从待删除列表中移除冲突项，保留非冲突项待下次尝试或取消
        m_timeSetIdsToDelete = validatedIdsToDelete;
        qDebug() << "TimeSetDialog::onAccepted - 冲突的TimeSet已从待删除列表移除";

        // **重要：重新加载TimeSet列表以恢复UI中被误删的冲突项**
        qDebug() << "TimeSetDialog::onAccepted - 重新加载TimeSet列表以恢复UI";
        loadExistingTimeSets();

        return; // 阻止对话框关闭
    }

    // --- 执行实际删除 ---
    bool deleteSuccess = true;
    if (!validatedIdsToDelete.isEmpty())
    {
        qDebug() << "TimeSetDialog::onAccepted - 开始执行删除操作，共" << validatedIdsToDelete.size() << "个TimeSet";
        m_db.transaction(); // 开始事务
        for (int idToDelete : validatedIdsToDelete)
        {
            if (!m_dataAccess->deleteTimeSet(idToDelete))
            {
                QString name = getTimeSetNameById(idToDelete);
                qWarning() << "TimeSetDialog::onAccepted - 删除TimeSet失败: ID=" << idToDelete << ", Name=" << name;
                QMessageBox::critical(this, "删除失败", QString("删除TimeSet '%1' (ID: %2) 时发生错误。请检查日志。").arg(name).arg(idToDelete));
                deleteSuccess = false;
                break; // 出现错误则停止删除
            }
            else
            {
                qDebug() << "TimeSetDialog::onAccepted - 成功从数据库删除TimeSet ID:" << idToDelete;
            }
        }

        if (deleteSuccess)
        {
            m_db.commit(); // 提交事务
            qDebug() << "TimeSetDialog::onAccepted - 所有待删除TimeSet已成功删除，事务已提交";
            m_timeSetIdsToDelete.clear(); // 成功删除后清空列表
            // 更新内存中的m_timeSetDataList
            m_timeSetDataList.erase(std::remove_if(m_timeSetDataList.begin(), m_timeSetDataList.end(),
                                                   [&validatedIdsToDelete](const TimeSetData &data)
                                                   {
                                                       return validatedIdsToDelete.contains(data.dbId);
                                                   }),
                                    m_timeSetDataList.end());
        }
        else
        {
            m_db.rollback(); // 回滚事务
            qDebug() << "TimeSetDialog::onAccepted - 删除过程中发生错误，事务已回滚";
            // 重新加载列表以恢复UI到删除前的状态
            loadExistingTimeSets();
            return; // 阻止关闭
        }
    }
    else
    {
        qDebug() << "TimeSetDialog::onAccepted - 没有需要删除的TimeSet";
    }

    // --- 新增：保存所有TimeSet的名称和周期更改 ---
    qDebug() << "TimeSetDialog::onAccepted - 开始保存所有TimeSet的名称和周期";
    bool savePropsSuccess = true;
    for (const TimeSetData &timeSet : m_timeSetDataList)
    {
        // 只更新数据库中已存在的记录 (dbId > 0)
        if (timeSet.dbId > 0)
        {
            qDebug() << "TimeSetDialog::onAccepted - 准备更新TimeSet ID:" << timeSet.dbId << " 名称:" << timeSet.name << " 周期:" << timeSet.period;
            // 分别更新名称和周期。可以考虑合并为一个更新函数，但目前分开处理更清晰
            if (!m_dataAccess->updateTimeSetName(timeSet.dbId, timeSet.name))
            {
                qWarning() << "TimeSetDialog::onAccepted - 更新TimeSet名称失败: ID=" << timeSet.dbId;
                QMessageBox::critical(this, "保存错误", QString("更新TimeSet '%1' (ID: %2) 的名称时发生错误。").arg(timeSet.name).arg(timeSet.dbId));
                savePropsSuccess = false;
                // break; // 可选：一个失败则全部失败
            }
            if (!m_dataAccess->updateTimeSetPeriod(timeSet.dbId, timeSet.period))
            {
                qWarning() << "TimeSetDialog::onAccepted - 更新TimeSet周期失败: ID=" << timeSet.dbId;
                QMessageBox::critical(this, "保存错误", QString("更新TimeSet '%1' (ID: %2) 的周期时发生错误。").arg(timeSet.name).arg(timeSet.dbId));
                savePropsSuccess = false;
                // break; // 可选：一个失败则全部失败
            }
        }
        else
        {
            qDebug() << "TimeSetDialog::onAccepted - 跳过新添加的TimeSet (ID <= 0):" << timeSet.name << "，将在后续步骤处理";
            // 新添加的 TimeSet 的基本信息已在 addTimeSet 时保存，这里不需要重复处理
        }
    }

    if (!savePropsSuccess)
    {
        qDebug() << "TimeSetDialog::onAccepted - 保存TimeSet属性时发生错误";
        // 如果需要，可以在这里回滚之前的删除操作，但会增加复杂性
        loadExistingTimeSets(); // 重新加载以反映部分成功或失败的状态
        return;                 // 阻止对话框关闭
    }
    qDebug() << "TimeSetDialog::onAccepted - 所有TimeSet的名称和周期已尝试保存";

    // --- 保存所有TimeSet的边沿设置 ---
    qDebug() << "TimeSetDialog::onAccepted - 开始保存所有TimeSet的边沿设置";
    bool saveSuccess = true;
    QTreeWidget *tree = m_uiManager->getTimeSetTree();
    for (int i = 0; i < tree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *timeSetItem = tree->topLevelItem(i);
        if (!timeSetItem)
            continue;
        int timeSetId = timeSetItem->data(0, Qt::UserRole).toInt();
        if (timeSetId <= 0)
            continue; // 跳过无效ID

        qDebug() << "TimeSetDialog::onAccepted - 准备保存 TimeSet ID:" << timeSetId << "的边沿";
        QList<TimeSetEdgeData> edges = m_edgeManager->getEdgeDataFromUI(timeSetItem, timeSetId);
        if (!m_dataAccess->saveTimeSetEdgesToDatabase(timeSetId, edges))
        {
            QString timeSetName = getTimeSetNameById(timeSetId);
            qWarning() << "TimeSetDialog::onAccepted - 保存TimeSet ID:" << timeSetId << "(" << timeSetName << ") 的边沿参数失败";
            QMessageBox::critical(this, "保存错误", QString("保存TimeSet '%1' (ID: %2) 的边沿参数时发生错误。请检查日志。").arg(timeSetName).arg(timeSetId));
            saveSuccess = false;
            // 这里可以选择 break 或 continue，取决于是否希望一个失败阻止所有保存
            // 为了数据一致性，可能需要回滚所有更改，但这会增加复杂性。
            // 目前选择继续尝试保存其他的，但标记为失败。
        }
        else
        {
            qDebug() << "TimeSetDialog::onAccepted - 成功保存 TimeSet ID:" << timeSetId << "的边沿";
        }
    }

    if (!saveSuccess)
    {
        qDebug() << "TimeSetDialog::onAccepted - 保存边沿参数时至少发生一个错误";
        // 可以选择在这里回滚之前的删除操作，或者只是阻止对话框关闭并提示用户
        // 重新加载数据以确保UI一致性可能也是必要的
        loadExistingTimeSets(); // 重新加载以反映部分成功或失败的状态
        return;                 // 阻止对话框关闭
    }
    qDebug() << "TimeSetDialog::onAccepted - 所有TimeSet的边沿设置已尝试保存";

    // --- 后续处理（向量表检查等）---
    // 在初始设置模式下，跳过向量表检查
    if (!m_isInitialSetup)
    {
        // 检查是否有向量表
        QSqlQuery query(m_db);
        if (query.exec("SELECT COUNT(*) FROM vector_tables"))
        {
            if (query.next())
            {
                int count = query.value(0).toInt();
                qDebug() << "TimeSetDialog::onAccepted - 检查向量表数量：" << count;

                if (count <= 0)
                {
                    qDebug() << "TimeSetDialog::onAccepted - 未找到向量表，显示错误消息";
                    QMessageBox::information(this, "提示", "没有找到向量表，请先创建向量表");
                    return;
                }
            }
        }
        else
        {
            qDebug() << "TimeSetDialog::onAccepted - 查询向量表失败：" << query.lastError().text();
        }
    }
    else
    {
        qDebug() << "TimeSetDialog::onAccepted - 初始设置模式，跳过向量表检查";
    }

    // 检查TimeSet数量
    qDebug() << "TimeSetDialog::onAccepted - 检查已配置的TimeSet数量：" << m_timeSetDataList.size();

    // 接受对话框
    qDebug() << "TimeSetDialog::onAccepted - 关闭对话框并接受更改";
    accept();
}

void TimeSetDialog::onRejected()
{
    qDebug() << "TimeSetDialog::onRejected - 用户取消操作";
    // 如果是初始设置，确认用户取消
    if (m_isInitialSetup)
    {
        if (QMessageBox::question(this, "确认取消",
                                  "您正在进行初始设置，取消将不会保存任何更改。确定要取消吗？",
                                  QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
        {
            qDebug() << "TimeSetDialog::onRejected - 用户选择不取消初始设置";
            return;
        }
    }

    // 清空待删除列表
    m_timeSetIdsToDelete.clear();
    qDebug() << "TimeSetDialog::onRejected - 待删除TimeSet列表已清空";

    // 重新加载TimeSet列表以恢复UI
    qDebug() << "TimeSetDialog::onRejected - 重新加载TimeSet列表以恢复UI";
    loadExistingTimeSets();

    // 拒绝对话框
    qDebug() << "TimeSetDialog::onRejected - 关闭对话框并拒绝更改";
    reject();
}