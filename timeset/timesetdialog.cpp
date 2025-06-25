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

#include "timesetdialog_1.cpp"