#include "timesetdialog.h"
#include "timesetedgedialog.h"
#include "databasemanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QInputDialog>
#include <QHeaderView>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

// 实现WaveComboDelegate的方法
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

TimeSetDialog::TimeSetDialog(QWidget *parent)
    : QDialog(parent),
      currentTimeSetItem(nullptr),
      currentTimeSetIndex(-1)
{
    setWindowTitle("TimeSet 设置");
    setMinimumSize(800, 600);

    // 加载波形选项
    loadWaveOptions();

    // 加载管脚列表
    loadPins();

    // 设置UI
    setupUI();

    // 创建波形委托并设置到树控件
    waveDelegate = new WaveComboDelegate(waveOptions, this);
    timeSetTree->setItemDelegateForColumn(4, waveDelegate);
}

TimeSetDialog::~TimeSetDialog()
{
    // 析构时释放动态创建的对象
    if (waveDelegate)
    {
        delete waveDelegate;
        waveDelegate = nullptr;
    }
}

void TimeSetDialog::setupUI()
{
    setupMainLayout();
    setupTreeWidget();
    setupPinSelection();
    setupButtonBox();
}

void TimeSetDialog::setupMainLayout()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainLayout->addWidget(mainSplitter);

    // 左侧为TimeSet树
    QWidget *leftWidget = new QWidget(mainSplitter);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    // TimeSet树
    timeSetTree = new QTreeWidget(leftWidget);
    leftLayout->addWidget(timeSetTree);

    // TimeSet操作按钮
    QHBoxLayout *timeSetButtonLayout = new QHBoxLayout();
    addTimeSetButton = new QPushButton("添加TimeSet", leftWidget);
    removeTimeSetButton = new QPushButton("删除TimeSet", leftWidget);
    timeSetButtonLayout->addWidget(addTimeSetButton);
    timeSetButtonLayout->addWidget(removeTimeSetButton);
    leftLayout->addLayout(timeSetButtonLayout);

    // 边沿操作按钮
    QHBoxLayout *edgeButtonLayout = new QHBoxLayout();
    addEdgeButton = new QPushButton("添加边沿参数", leftWidget);
    removeEdgeButton = new QPushButton("删除边沿参数", leftWidget);
    edgeButtonLayout->addWidget(addEdgeButton);
    edgeButtonLayout->addWidget(removeEdgeButton);
    leftLayout->addLayout(edgeButtonLayout);

    // 右侧显示所有可用管脚
    QWidget *rightWidget = new QWidget(mainSplitter);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(10, 10, 10, 10);

    // 创建标题
    QLabel *availablePinsLabel = new QLabel("可用管脚列表", rightWidget);
    QFont titleFont = availablePinsLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    availablePinsLabel->setFont(titleFont);
    availablePinsLabel->setAlignment(Qt::AlignCenter);
    rightLayout->addWidget(availablePinsLabel);
    rightLayout->addSpacing(10);

    // 创建管脚列表显示
    QListWidget *availablePinsList = new QListWidget(rightWidget);
    availablePinsList->setAlternatingRowColors(true);
    rightLayout->addWidget(availablePinsList);

    // 添加所有管脚到列表中
    for (auto it = pinList.begin(); it != pinList.end(); ++it)
    {
        QListWidgetItem *item = new QListWidgetItem(it.value());
        availablePinsList->addItem(item);
    }

    // 添加说明文本
    QLabel *hintLabel = new QLabel("双击TimeSet名称可修改名称和周期", rightWidget);
    hintLabel->setAlignment(Qt::AlignCenter);
    rightLayout->addWidget(hintLabel);

    // 添加左右分区到分割器
    mainSplitter->addWidget(leftWidget);
    mainSplitter->addWidget(rightWidget);
    mainSplitter->setStretchFactor(0, 2);
    mainSplitter->setStretchFactor(1, 1);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttonBox);

    // 连接信号槽
    connect(addTimeSetButton, &QPushButton::clicked, this, &TimeSetDialog::addTimeSet);
    connect(removeTimeSetButton, &QPushButton::clicked, this, &TimeSetDialog::removeTimeSet);
    connect(timeSetTree, &QTreeWidget::itemChanged, this, &TimeSetDialog::onPropertyItemChanged);
    connect(timeSetTree, &QTreeWidget::itemSelectionChanged, this, &TimeSetDialog::timeSetSelectionChanged);
    connect(timeSetTree, &QTreeWidget::itemDoubleClicked, this, &TimeSetDialog::onItemDoubleClicked);
    connect(addEdgeButton, &QPushButton::clicked, this, &TimeSetDialog::addEdgeItem);
    connect(removeEdgeButton, &QPushButton::clicked, this, &TimeSetDialog::removeEdgeItem);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &TimeSetDialog::onAccepted);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &TimeSetDialog::onRejected);
}

void TimeSetDialog::setupTreeWidget()
{
    timeSetTree->setHeaderLabels(QStringList() << "名称/周期(单位)"
                                               << "T1R"
                                               << "T1F"
                                               << "STBR"
                                               << "WAVE");
    timeSetTree->setColumnCount(5);
    timeSetTree->setAlternatingRowColors(true);
    timeSetTree->setSelectionMode(QAbstractItemView::SingleSelection);
    timeSetTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int i = 1; i < 5; i++)
    {
        timeSetTree->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    }

    // 启用编辑功能
    timeSetTree->setEditTriggers(QAbstractItemView::DoubleClicked |
                                 QAbstractItemView::EditKeyPressed);

    // 设置所有列可编辑
    timeSetTree->setItemsExpandable(true);
}

void TimeSetDialog::setupPinSelection()
{
    // 这个函数不再需要，我们已经在setupMainLayout中创建了新的界面布局
    // 保留这个函数只是为了避免改变太多现有代码
    pinSelectionGroup = nullptr;
    pinListWidget = nullptr;
}

void TimeSetDialog::setupButtonBox()
{
    // 初始禁用按钮，直到有TimeSet被选中
    addEdgeButton->setEnabled(false);
    removeEdgeButton->setEnabled(false);
}

void TimeSetDialog::loadWaveOptions()
{
    // 从数据库加载波形选项
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery query(db);

    if (query.exec("SELECT id, wave_type FROM wave_options"))
    {
        while (query.next())
        {
            int id = query.value(0).toInt();
            QString waveType = query.value(1).toString();
            waveOptions[id] = waveType;
        }
    }
    else
    {
        qWarning() << "加载波形选项失败:" << query.lastError().text();
    }
}

void TimeSetDialog::loadPins()
{
    // 从数据库加载管脚列表
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery query(db);

    if (query.exec("SELECT id, pin_name FROM pin_list"))
    {
        while (query.next())
        {
            int id = query.value(0).toInt();
            QString pinName = query.value(1).toString();
            pinList[id] = pinName;
        }
    }
    else
    {
        qWarning() << "加载管脚列表失败:" << query.lastError().text();
    }
}

void TimeSetDialog::addTimeSet()
{
    bool ok;
    QString name = QInputDialog::getText(this, "添加TimeSet",
                                         "TimeSet名称:", QLineEdit::Normal,
                                         "timeset_" + QString::number(timeSetDataList.size() + 1), &ok);
    if (ok && !name.isEmpty())
    {
        // 检查名称是否已存在
        for (const TimeSetData &timeSet : timeSetDataList)
        {
            if (timeSet.name == name)
            {
                QMessageBox::warning(this, "名称重复", "TimeSet名称已存在，请使用其他名称。");
                return;
            }
        }

        // 创建新的TimeSet数据
        TimeSetData newTimeSet;
        newTimeSet.name = name;
        newTimeSet.period = 1000.0; // 默认1000ns

        // 计算频率，单位MHz
        double freq = 1000.0 / newTimeSet.period;

        // 添加到列表
        timeSetDataList.append(newTimeSet);

        // 创建树项
        QTreeWidgetItem *item = new QTreeWidgetItem(timeSetTree);
        item->setText(0, name + " / " + QString::number(newTimeSet.period) + "(ns) ⇔ " + QString::number(freq, 'f', 6) + "MHz");
        // 不再设置任何默认属性值，保持属性列为空
        item->setText(1, "");
        item->setText(2, "");
        item->setText(3, "");
        item->setText(4, "");

        item->setFlags(item->flags() | Qt::ItemIsEditable);

        // 选择新项
        timeSetTree->setCurrentItem(item);

        // 调用选择变更函数更新右侧显示
        timeSetSelectionChanged();
    }
}

void TimeSetDialog::removeTimeSet()
{
    if (currentTimeSetItem && currentTimeSetIndex >= 0)
    {
        // 确认删除
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "确认删除",
                                      "确定要删除TimeSet \"" + timeSetDataList[currentTimeSetIndex].name + "\"吗？",
                                      QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes)
        {
            // 删除所有关联的边沿数据
            for (int i = edgeDataList.size() - 1; i >= 0; i--)
            {
                if (i < edgeDataList.size() && edgeDataList[i].timesetId == currentTimeSetIndex)
                {
                    edgeDataList.removeAt(i);
                }
            }

            // 从树中删除项
            delete currentTimeSetItem;
            currentTimeSetItem = nullptr;

            // 从数据列表中删除
            timeSetDataList.removeAt(currentTimeSetIndex);
            currentTimeSetIndex = -1;

            // 更新UI状态
            addEdgeButton->setEnabled(false);
            removeEdgeButton->setEnabled(false);

            // 如果还有TimeSet，选择第一个
            if (timeSetTree->topLevelItemCount() > 0)
            {
                timeSetTree->setCurrentItem(timeSetTree->topLevelItem(0));
                // 触发选择变更
                timeSetSelectionChanged();
            }
        }
    }
}

void TimeSetDialog::renameTimeSet(QTreeWidgetItem *item, int column)
{
    if (column == 0 && item && !item->parent())
    {
        QString newText = item->text(0);

        // 如果用户手动编辑，可能会导致格式不一致，因此提取名称部分
        QString newName = newText;
        if (newText.contains("/"))
        {
            newName = newText.split("/").first().trimmed();
        }

        // 找到对应的TimeSet索引
        int index = timeSetTree->indexOfTopLevelItem(item);
        if (index >= 0 && index < timeSetDataList.size())
        {
            // 检查名称是否已存在
            for (int i = 0; i < timeSetDataList.size(); i++)
            {
                if (i != index && timeSetDataList[i].name == newName)
                {
                    QMessageBox::warning(this, "名称重复", "TimeSet名称已存在，请使用其他名称。");

                    // 恢复原名称和格式
                    double period = timeSetDataList[index].period;
                    double freq = 1000.0 / period;
                    item->setText(0, timeSetDataList[index].name + " / " + QString::number(period) + "(ns) ⇔ " + QString::number(freq, 'f', 6) + "MHz");

                    return;
                }
            }

            // 更新数据
            timeSetDataList[index].name = newName;

            // 重新设置正确格式的文本
            double period = timeSetDataList[index].period;
            double freq = 1000.0 / period;
            item->setText(0, newName + " / " + QString::number(period) + "(ns) ⇔ " + QString::number(freq, 'f', 6) + "MHz");
        }
    }
}

void TimeSetDialog::updatePeriod(double value)
{
    if (currentTimeSetItem && currentTimeSetIndex >= 0)
    {
        timeSetDataList[currentTimeSetIndex].period = value;

        // 计算频率
        double freq = 1000.0 / value;

        // 更新显示
        QString name = timeSetDataList[currentTimeSetIndex].name;
        currentTimeSetItem->setText(0, name + " / " + QString::number(value) + "(ns) ⇔ " + QString::number(freq, 'f', 6) + "MHz");
    }
}

void TimeSetDialog::timeSetSelectionChanged()
{
    QList<QTreeWidgetItem *> selectedItems = timeSetTree->selectedItems();

    // 清除上一个选择
    currentTimeSetItem = nullptr;
    currentTimeSetIndex = -1;

    if (!selectedItems.isEmpty())
    {
        QTreeWidgetItem *item = selectedItems.first();

        // 如果是顶级项（TimeSet），记录它
        if (!item->parent())
        {
            currentTimeSetItem = item;
            currentTimeSetIndex = timeSetTree->indexOfTopLevelItem(item);

            // 启用边沿按钮
            addEdgeButton->setEnabled(true);
            removeEdgeButton->setEnabled(true);
        }
    }
    else
    {
        // 禁用边沿按钮
        addEdgeButton->setEnabled(false);
        removeEdgeButton->setEnabled(false);
    }
}

void TimeSetDialog::addEdgeItem()
{
    if (currentTimeSetIndex < 0)
    {
        return;
    }

    // 获取当前选中的管脚
    // 因为我们移除了pinListWidget，这里需要修改为显示可选管脚的对话框
    QStringList availablePins;
    for (auto it = pinList.begin(); it != pinList.end(); ++it)
    {
        availablePins << it.value();
    }

    // 创建管脚选择对话框
    QDialog pinDialog(this);
    pinDialog.setWindowTitle("选择管脚");
    QVBoxLayout *pinDialogLayout = new QVBoxLayout(&pinDialog);

    QListWidget *pinListWidget = new QListWidget(&pinDialog);
    pinListWidget->setSelectionMode(QAbstractItemView::MultiSelection);
    pinDialogLayout->addWidget(pinListWidget);

    // 添加管脚到列表
    for (const QString &pinName : availablePins)
    {
        // 检查此管脚是否已有边沿参数
        bool alreadyExists = false;
        for (const TimeSetEdgeData &edge : edgeDataList)
        {
            if (edge.timesetId == currentTimeSetIndex)
            {
                int pinId = -1;
                for (auto it = pinList.begin(); it != pinList.end(); ++it)
                {
                    if (it.value() == pinName)
                    {
                        pinId = it.key();
                        break;
                    }
                }

                if (edge.pinId == pinId)
                {
                    alreadyExists = true;
                    break;
                }
            }
        }

        if (!alreadyExists)
        {
            QListWidgetItem *item = new QListWidgetItem(pinName);
            pinListWidget->addItem(item);
        }
    }

    // 对话框按钮
    QDialogButtonBox *pinDialogButtonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &pinDialog);
    connect(pinDialogButtonBox, &QDialogButtonBox::accepted, &pinDialog, &QDialog::accept);
    connect(pinDialogButtonBox, &QDialogButtonBox::rejected, &pinDialog, &QDialog::reject);
    pinDialogLayout->addWidget(pinDialogButtonBox);

    // 显示管脚选择对话框
    if (pinDialog.exec() != QDialog::Accepted || pinListWidget->selectedItems().isEmpty())
    {
        return; // 用户取消或未选择管脚
    }

    // 获取选中的管脚
    QList<QListWidgetItem *> selectedPins = pinListWidget->selectedItems();
    if (selectedPins.isEmpty())
    {
        QMessageBox::warning(this, "未选择管脚", "请先选择至少一个管脚。");
        return;
    }

    // 默认值
    double defaultT1R = 250.0;
    double defaultT1F = 750.0;
    double defaultSTBR = 500.0;
    int defaultWaveId = waveOptions.isEmpty() ? 0 : waveOptions.keys().first();

    // 创建并显示边沿参数对话框
    TimeSetEdgeDialog dialog(defaultT1R, defaultT1F, defaultSTBR, defaultWaveId, waveOptions, this);

    if (dialog.exec() == QDialog::Accepted)
    {
        // 获取设置的参数值
        double t1r = dialog.getT1R();
        double t1f = dialog.getT1F();
        double stbr = dialog.getSTBR();
        int waveId = dialog.getWaveId();
        QString waveName = waveOptions.value(waveId);

        // 查找是否已有相同属性的边沿参数项
        QTreeWidgetItem *existingItem = nullptr;
        for (int i = 0; i < currentTimeSetItem->childCount(); i++)
        {
            QTreeWidgetItem *childItem = currentTimeSetItem->child(i);
            if (childItem->text(1) == QString::number(t1r) &&
                childItem->text(2) == QString::number(t1f) &&
                childItem->text(3) == QString::number(stbr) &&
                childItem->text(4) == waveName)
            {
                existingItem = childItem;
                break;
            }
        }

        // 保存所有选中管脚的ID列表，以便更新
        QList<int> pinIds;
        for (QListWidgetItem *pinItem : selectedPins)
        {
            QString pinName = pinItem->text();
            int pinId = -1;

            // 查找管脚ID
            for (auto it = pinList.begin(); it != pinList.end(); ++it)
            {
                if (it.value() == pinName)
                {
                    pinId = it.key();
                    break;
                }
            }

            if (pinId != -1)
            {
                pinIds.append(pinId);
            }
        }

        if (existingItem)
        {
            // 如果已有相同属性的项，将管脚名称添加到该项
            QString currentPins = existingItem->text(0);
            QStringList pinNames;

            // 提取现有的管脚名称
            if (currentPins.contains(","))
            {
                pinNames = currentPins.split(",");
                for (int i = 0; i < pinNames.size(); i++)
                {
                    pinNames[i] = pinNames[i].trimmed();
                }
            }
            else
            {
                pinNames.append(currentPins.trimmed());
            }

            // 添加新选择的管脚
            for (QListWidgetItem *pinItem : selectedPins)
            {
                QString pinName = pinItem->text();
                if (!pinNames.contains(pinName))
                {
                    pinNames.append(pinName);
                }
            }

            // 更新显示
            existingItem->setText(0, pinNames.join(", "));

            // 为每个管脚创建边沿参数数据
            for (int pinId : pinIds)
            {
                // 检查是否已经有此管脚的边沿参数
                bool exists = false;
                for (const TimeSetEdgeData &edge : edgeDataList)
                {
                    if (edge.timesetId == currentTimeSetIndex && edge.pinId == pinId)
                    {
                        exists = true;
                        break;
                    }
                }

                if (!exists)
                {
                    // 创建新的边沿参数数据
                    TimeSetEdgeData edgeData;
                    edgeData.timesetId = currentTimeSetIndex;
                    edgeData.pinId = pinId;
                    edgeData.t1r = t1r;
                    edgeData.t1f = t1f;
                    edgeData.stbr = stbr;
                    edgeData.waveId = waveId;

                    // 添加到列表
                    edgeDataList.append(edgeData);
                }
            }
        }
        else
        {
            // 如果没有相同属性的项，创建新项
            QStringList pinNames;
            for (QListWidgetItem *pinItem : selectedPins)
            {
                pinNames.append(pinItem->text());
            }

            // 创建树项
            QTreeWidgetItem *edgeItem = new QTreeWidgetItem(currentTimeSetItem);
            edgeItem->setText(0, pinNames.join(", "));
            edgeItem->setText(1, QString::number(t1r));
            edgeItem->setText(2, QString::number(t1f));
            edgeItem->setText(3, QString::number(stbr));
            edgeItem->setText(4, waveName);

            // 设置所有列都可编辑
            for (int i = 0; i < 5; i++)
            {
                edgeItem->setFlags(edgeItem->flags() | Qt::ItemIsEditable);
            }

            // 为每个管脚创建边沿参数数据
            for (int pinId : pinIds)
            {
                TimeSetEdgeData edgeData;
                edgeData.timesetId = currentTimeSetIndex;
                edgeData.pinId = pinId;
                edgeData.t1r = t1r;
                edgeData.t1f = t1f;
                edgeData.stbr = stbr;
                edgeData.waveId = waveId;

                // 添加到列表
                edgeDataList.append(edgeData);
            }
        }
    }
}

void TimeSetDialog::removeEdgeItem()
{
    QList<QTreeWidgetItem *> selectedItems = timeSetTree->selectedItems();
    if (selectedItems.isEmpty())
    {
        return;
    }

    QTreeWidgetItem *item = selectedItems.first();

    // 只处理边沿参数项（子项）
    if (item->parent())
    {
        // 找到对应的数据
        int parentIndex = timeSetTree->indexOfTopLevelItem(item->parent());

        // 获取管脚名称列表
        QString pinText = item->text(0);
        QStringList pinNames = pinText.split(",");
        for (int i = 0; i < pinNames.size(); i++)
        {
            pinNames[i] = pinNames[i].trimmed();
        }

        // 创建一个可供选择的对话框，让用户选择要删除的管脚
        QDialog pinDialog(this);
        pinDialog.setWindowTitle("选择要删除的管脚");
        QVBoxLayout *pinDialogLayout = new QVBoxLayout(&pinDialog);

        QLabel *label = new QLabel("请选择要删除的管脚:", &pinDialog);
        pinDialogLayout->addWidget(label);

        QListWidget *pinListWidget = new QListWidget(&pinDialog);
        pinListWidget->setSelectionMode(QAbstractItemView::MultiSelection);
        pinDialogLayout->addWidget(pinListWidget);

        // 添加管脚到列表
        for (const QString &pinName : pinNames)
        {
            QListWidgetItem *listItem = new QListWidgetItem(pinName);
            pinListWidget->addItem(listItem);
            listItem->setSelected(true); // 默认全选
        }

        // 对话框按钮
        QDialogButtonBox *pinDialogButtonBox = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &pinDialog);
        connect(pinDialogButtonBox, &QDialogButtonBox::accepted, &pinDialog, &QDialog::accept);
        connect(pinDialogButtonBox, &QDialogButtonBox::rejected, &pinDialog, &QDialog::reject);
        pinDialogLayout->addWidget(pinDialogButtonBox);

        // 如果只有一个管脚，直接删除整行
        if (pinNames.size() == 1)
        {
            QString pinName = pinNames.first();

            // 查找管脚ID
            int pinId = -1;
            for (auto it = pinList.begin(); it != pinList.end(); ++it)
            {
                if (it.value() == pinName)
                {
                    pinId = it.key();
                    break;
                }
            }

            if (pinId != -1)
            {
                // 删除该管脚的边沿数据
                for (int i = 0; i < edgeDataList.size(); i++)
                {
                    if (edgeDataList[i].timesetId == parentIndex && edgeDataList[i].pinId == pinId)
                    {
                        edgeDataList.removeAt(i);
                        break;
                    }
                }

                // 删除树项
                delete item;
            }
        }
        // 如果有多个管脚，显示选择对话框
        else if (pinDialog.exec() == QDialog::Accepted)
        {
            QList<QListWidgetItem *> selectedPins = pinListWidget->selectedItems();
            if (selectedPins.isEmpty())
            {
                return; // 用户没有选择任何管脚
            }

            // 收集要删除的管脚名称
            QStringList pinsToDelete;
            for (QListWidgetItem *selectedPin : selectedPins)
            {
                pinsToDelete << selectedPin->text();
            }

            // 如果用户选择删除所有管脚，删除整行
            if (pinsToDelete.size() == pinNames.size())
            {
                // 删除所有这些管脚的边沿数据
                for (const QString &pinName : pinNames)
                {
                    // 查找管脚ID
                    int pinId = -1;
                    for (auto it = pinList.begin(); it != pinList.end(); ++it)
                    {
                        if (it.value() == pinName)
                        {
                            pinId = it.key();
                            break;
                        }
                    }

                    if (pinId != -1)
                    {
                        // 删除该管脚的边沿数据
                        for (int i = 0; i < edgeDataList.size(); i++)
                        {
                            if (edgeDataList[i].timesetId == parentIndex && edgeDataList[i].pinId == pinId)
                            {
                                edgeDataList.removeAt(i);
                                break;
                            }
                        }
                    }
                }

                // 删除树项
                delete item;
            }
            // 否则只删除选中的管脚，并更新显示
            else
            {
                // 找出要保留的管脚
                QStringList remainingPins;
                for (const QString &pinName : pinNames)
                {
                    if (!pinsToDelete.contains(pinName))
                    {
                        remainingPins << pinName;
                    }
                }

                // 更新显示
                item->setText(0, remainingPins.join(", "));

                // 删除选中的管脚的边沿数据
                for (const QString &pinName : pinsToDelete)
                {
                    // 查找管脚ID
                    int pinId = -1;
                    for (auto it = pinList.begin(); it != pinList.end(); ++it)
                    {
                        if (it.value() == pinName)
                        {
                            pinId = it.key();
                            break;
                        }
                    }

                    if (pinId != -1)
                    {
                        // 删除该管脚的边沿数据
                        for (int i = 0; i < edgeDataList.size(); i++)
                        {
                            if (edgeDataList[i].timesetId == parentIndex && edgeDataList[i].pinId == pinId)
                            {
                                edgeDataList.removeAt(i);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
}

void TimeSetDialog::editEdgeItem(QTreeWidgetItem *item, int column)
{
    // 只处理边沿参数项（子项）
    if (item && item->parent())
    {
        // 找到对应的数据
        int parentIndex = timeSetTree->indexOfTopLevelItem(item->parent());

        // 如果双击的是第一列（管脚名称），显示完整的边沿参数对话框
        if (column == 0)
        {
            // 从这一行获取当前属性值
            double currentT1R = item->text(1).toDouble();
            double currentT1F = item->text(2).toDouble();
            double currentSTBR = item->text(3).toDouble();

            // 查找波形ID
            int currentWaveId = -1;
            QString waveName = item->text(4);
            for (auto it = waveOptions.begin(); it != waveOptions.end(); ++it)
            {
                if (it.value() == waveName)
                {
                    currentWaveId = it.key();
                    break;
                }
            }

            if (currentWaveId == -1)
            {
                currentWaveId = waveOptions.isEmpty() ? 0 : waveOptions.keys().first();
            }

            // 创建并显示边沿参数对话框
            TimeSetEdgeDialog dialog(currentT1R, currentT1F, currentSTBR, currentWaveId, waveOptions, this);

            if (dialog.exec() == QDialog::Accepted)
            {
                // 获取新属性值
                double newT1R = dialog.getT1R();
                double newT1F = dialog.getT1F();
                double newSTBR = dialog.getSTBR();
                int newWaveId = dialog.getWaveId();
                QString newWaveName = waveOptions.value(newWaveId);

                // 更新显示
                item->setText(1, QString::number(newT1R));
                item->setText(2, QString::number(newT1F));
                item->setText(3, QString::number(newSTBR));
                item->setText(4, newWaveName);

                // 提取该行中的所有管脚
                QString pinText = item->text(0);
                QStringList pinNames = pinText.split(",");
                for (int i = 0; i < pinNames.size(); i++)
                {
                    pinNames[i] = pinNames[i].trimmed();
                }

                // 更新所有这些管脚的边沿数据
                for (const QString &pinName : pinNames)
                {
                    // 查找管脚ID
                    int pinId = -1;
                    for (auto it = pinList.begin(); it != pinList.end(); ++it)
                    {
                        if (it.value() == pinName)
                        {
                            pinId = it.key();
                            break;
                        }
                    }

                    if (pinId != -1)
                    {
                        // 更新这个管脚的边沿参数
                        for (int i = 0; i < edgeDataList.size(); i++)
                        {
                            TimeSetEdgeData &edge = edgeDataList[i];
                            if (edge.timesetId == parentIndex && edge.pinId == pinId)
                            {
                                edge.t1r = newT1R;
                                edge.t1f = newT1F;
                                edge.stbr = newSTBR;
                                edge.waveId = newWaveId;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
}

void TimeSetDialog::onPinSelectionChanged()
{
    // 这个函数不再需要，保留它只是为了避免更改太多代码
}

void TimeSetDialog::updatePinSelection(QTreeWidgetItem *item, int column)
{
    // 这个函数不再需要，保留它只是为了避免更改太多代码
}

void TimeSetDialog::updateEdgeItemText(QTreeWidgetItem *edgeItem, const TimeSetEdgeData &edgeData)
{
    QString pinName = pinList.value(edgeData.pinId);
    QString waveName = waveOptions.value(edgeData.waveId);

    // 第1列显示管脚名称
    edgeItem->setText(0, pinName);

    // 第2-5列分别显示T1R、T1F、STBR和波形类型值
    edgeItem->setText(1, QString::number(edgeData.t1r));
    edgeItem->setText(2, QString::number(edgeData.t1f));
    edgeItem->setText(3, QString::number(edgeData.stbr));
    edgeItem->setText(4, waveName);
}

void TimeSetDialog::onAccepted()
{
    // 保存数据到数据库
    if (saveToDatabase())
    {
        // 先保存tableId以便后续使用
        int tableId = -1;
        QString tableName;

        // 先隐藏TimeSet对话框，避免它一直显示在背景中
        this->hide();

        // 弹出向量表命名对话框
        QDialog vectorNameDialog(nullptr); // 使用nullptr而不是this作为父窗口
        vectorNameDialog.setWindowTitle("创建向量表向导");
        vectorNameDialog.setFixedSize(320, 120);

        QVBoxLayout *layout = new QVBoxLayout(&vectorNameDialog);

        // 名称标签和输入框
        QHBoxLayout *nameLayout = new QHBoxLayout();
        QLabel *nameLabel = new QLabel("名称:", &vectorNameDialog);
        QLineEdit *nameEdit = new QLineEdit(&vectorNameDialog);
        nameEdit->setMinimumWidth(200);
        nameLayout->addWidget(nameLabel);
        nameLayout->addWidget(nameEdit);
        layout->addLayout(nameLayout);

        // 按钮布局
        QHBoxLayout *buttonLayout = new QHBoxLayout();
        buttonLayout->addStretch();
        QPushButton *okButton = new QPushButton("确定", &vectorNameDialog);
        QPushButton *cancelButton = new QPushButton("取消向导", &vectorNameDialog);
        buttonLayout->addWidget(okButton);
        buttonLayout->addWidget(cancelButton);
        layout->addLayout(buttonLayout);

        // 连接信号槽
        connect(okButton, &QPushButton::clicked, &vectorNameDialog, &QDialog::accept);
        connect(cancelButton, &QPushButton::clicked, &vectorNameDialog, &QDialog::reject);

        // 显示对话框
        if (vectorNameDialog.exec() == QDialog::Accepted)
        {
            tableName = nameEdit->text().trimmed();
            if (!tableName.isEmpty())
            {
                // 保存到数据库
                QSqlDatabase db = DatabaseManager::instance()->database();
                QSqlQuery query(db);

                // 检查vector_tables表是否存在，如不存在则创建
                bool tableExists = false;
                QStringList tables = db.tables();
                if (tables.contains("vector_tables"))
                {
                    tableExists = true;
                }
                else
                {
                    // 创建表
                    QString createTableSql = "CREATE TABLE vector_tables ("
                                             "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                             "table_name TEXT NOT NULL UNIQUE,"
                                             "table_nav_note TEXT"
                                             ")";
                    if (query.exec(createTableSql))
                    {
                        tableExists = true;
                    }
                    else
                    {
                        QMessageBox::critical(nullptr, "数据库错误", "创建vector_tables表失败：" + query.lastError().text());
                    }
                }

                // 如果表存在则插入数据
                if (tableExists)
                {
                    // 检查是否已存在同名向量表
                    query.prepare("SELECT id FROM vector_tables WHERE table_name = ?");
                    query.addBindValue(tableName);

                    if (query.exec() && query.next())
                    {
                        QMessageBox::warning(nullptr, "名称重复", "已存在同名的向量表，请使用其他名称。");
                    }
                    else
                    {
                        query.prepare("INSERT INTO vector_tables (table_name, table_nav_note) VALUES (?, '')");
                        query.addBindValue(tableName);

                        if (!query.exec())
                        {
                            QMessageBox::critical(nullptr, "数据库错误", "创建向量表记录失败：" + query.lastError().text());
                        }
                        else
                        {
                            // 获取新插入记录的ID
                            tableId = query.lastInsertId().toInt();

                            QMessageBox::information(nullptr, "创建成功", "向量表 '" + tableName + "' 已成功创建！");

                            // 检查vector_table_pins表是否存在，如不存在则创建
                            bool pinsTableExists = false;
                            if (tables.contains("vector_table_pins"))
                            {
                                pinsTableExists = true;
                            }
                            else
                            {
                                // 创建表
                                QString createPinsTableSql = "CREATE TABLE vector_table_pins ("
                                                             "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                                             "table_id INTEGER NOT NULL,"
                                                             "pin_id INTEGER NOT NULL,"
                                                             "pin_type INTEGER NOT NULL,"
                                                             "pin_channel_count INTEGER DEFAULT 1,"
                                                             "FOREIGN KEY(table_id) REFERENCES vector_tables(id),"
                                                             "FOREIGN KEY(pin_id) REFERENCES pin_list(id)"
                                                             ")";
                                if (query.exec(createPinsTableSql))
                                {
                                    pinsTableExists = true;
                                }
                                else
                                {
                                    QMessageBox::critical(nullptr, "数据库错误", "创建vector_table_pins表失败：" + query.lastError().text());
                                }
                            }

                            // 如果pins表创建成功，使用独立对话框函数显示管脚选择对话框
                            if (pinsTableExists && tableId > 0)
                            {
                                showPinSelectionDialogStandalone(tableId, tableName);
                            }
                        }
                    }
                }
            }
        }

        // 在所有操作完成后接受对话框（这实际上只是为了完成整个流程，因为对话框已经隐藏）
        accept();
    }
}

void TimeSetDialog::onRejected()
{
    reject();
}

bool TimeSetDialog::saveToDatabase()
{
    QSqlDatabase db = DatabaseManager::instance()->database();

    // 开始事务
    if (!db.transaction())
    {
        QMessageBox::critical(this, "数据库错误", "无法开始数据库事务：" + db.lastError().text());
        return false;
    }

    bool success = true;

    // 保存所有TimeSet
    for (int i = 0; i < timeSetDataList.size(); i++)
    {
        int timeSetId = -1;
        if (!saveTimeSetToDatabase(timeSetDataList[i], timeSetId))
        {
            success = false;
            break;
        }

        // 找到所有与此TimeSet关联的边沿参数
        QList<TimeSetEdgeData> edges;
        for (const TimeSetEdgeData &edge : edgeDataList)
        {
            if (edge.timesetId == i)
            {
                TimeSetEdgeData newEdge = edge;
                newEdge.timesetId = timeSetId; // 使用数据库中的实际ID
                edges.append(newEdge);
            }
        }

        // 保存边沿参数
        if (!edges.isEmpty() && !saveTimeSetEdgesToDatabase(timeSetId, edges))
        {
            success = false;
            break;
        }
    }

    // 提交或回滚事务
    if (success)
    {
        if (!db.commit())
        {
            QMessageBox::critical(this, "数据库错误", "无法提交数据库事务：" + db.lastError().text());
            db.rollback();
            return false;
        }
    }
    else
    {
        db.rollback();
        return false;
    }

    return true;
}

bool TimeSetDialog::saveTimeSetToDatabase(const TimeSetData &timeSet, int &outTimeSetId)
{
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery query(db);

    // 检查是否已存在
    query.prepare("SELECT id FROM timeset_list WHERE timeset_name = ?");
    query.addBindValue(timeSet.name);

    if (!query.exec())
    {
        QMessageBox::critical(this, "数据库错误", "查询TimeSet失败：" + query.lastError().text());
        return false;
    }

    if (query.next())
    {
        // 更新现有记录
        int id = query.value(0).toInt();
        query.prepare("UPDATE timeset_list SET period = ? WHERE id = ?");
        query.addBindValue(timeSet.period);
        query.addBindValue(id);

        if (!query.exec())
        {
            QMessageBox::critical(this, "数据库错误", "更新TimeSet失败：" + query.lastError().text());
            return false;
        }

        outTimeSetId = id;
    }
    else
    {
        // 插入新记录
        query.prepare("INSERT INTO timeset_list (timeset_name, period) VALUES (?, ?)");
        query.addBindValue(timeSet.name);
        query.addBindValue(timeSet.period);

        if (!query.exec())
        {
            QMessageBox::critical(this, "数据库错误", "添加TimeSet失败：" + query.lastError().text());
            return false;
        }

        outTimeSetId = query.lastInsertId().toInt();
    }

    return true;
}

bool TimeSetDialog::saveTimeSetEdgesToDatabase(int timeSetId, const QList<TimeSetEdgeData> &edges)
{
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery query(db);

    // 首先删除此timeSetId的所有现有边沿参数
    query.prepare("DELETE FROM timeset_settings WHERE timeset_id = ?");
    query.addBindValue(timeSetId);

    if (!query.exec())
    {
        QMessageBox::critical(this, "数据库错误", "删除现有边沿参数失败：" + query.lastError().text());
        return false;
    }

    // 添加新的边沿参数
    for (const TimeSetEdgeData &edge : edges)
    {
        query.prepare("INSERT INTO timeset_settings (timeset_id, pin_id, T1R, T1F, STBR, wave_id) "
                      "VALUES (?, ?, ?, ?, ?, ?)");
        query.addBindValue(timeSetId);
        query.addBindValue(edge.pinId);
        query.addBindValue(edge.t1r);
        query.addBindValue(edge.t1f);
        query.addBindValue(edge.stbr);
        query.addBindValue(edge.waveId);

        if (!query.exec())
        {
            QMessageBox::critical(this, "数据库错误", "添加边沿参数失败：" + query.lastError().text());
            return false;
        }
    }

    return true;
}

void TimeSetDialog::editTimeSetProperties(QTreeWidgetItem *item, int column)
{
    // 只处理顶级项（TimeSet）
    if (item && !item->parent())
    {
        int index = timeSetTree->indexOfTopLevelItem(item);
        if (index >= 0 && index < timeSetDataList.size())
        {
            // 创建对话框
            QDialog dialog(this);
            dialog.setWindowTitle("编辑TimeSet属性");
            dialog.setMinimumWidth(300);

            QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);

            // 名称和周期编辑框
            QFormLayout *formLayout = new QFormLayout();

            QLineEdit *nameEdit = new QLineEdit(&dialog);
            nameEdit->setText(timeSetDataList[index].name);
            formLayout->addRow("名称:", nameEdit);

            QDoubleSpinBox *periodSpinBox = new QDoubleSpinBox(&dialog);
            periodSpinBox->setRange(1.0, 10000.0);
            periodSpinBox->setSingleStep(1.0);
            periodSpinBox->setDecimals(1);
            periodSpinBox->setValue(timeSetDataList[index].period);
            periodSpinBox->setSuffix(" ns");
            formLayout->addRow("周期:", periodSpinBox);

            mainLayout->addLayout(formLayout);

            // 显示自动计算的频率和分辨率
            QLabel *freqLabel = new QLabel(&dialog);
            QLabel *resLabel = new QLabel(&dialog);

            auto updateLabels = [&]()
            {
                double period = periodSpinBox->value();
                double freq = 1000.0 / period;      // MHz
                double resolution = period / 100.0; // 假设分辨率为周期的1/100

                freqLabel->setText(QString("%1 MHz").arg(freq, 0, 'f', 6));
                resLabel->setText(QString("%1 ns").arg(resolution, 0, 'f', 2));
            };

            updateLabels(); // 初始化标签

            formLayout->addRow("频率:", freqLabel);
            formLayout->addRow("分辨率:", resLabel);

            // 当周期改变时更新频率和分辨率显示
            connect(periodSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    [&](double)
                    { updateLabels(); });

            // 对话框按钮
            QDialogButtonBox *buttonBox = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
            connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
            connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

            mainLayout->addWidget(buttonBox);

            // 显示对话框
            if (dialog.exec() == QDialog::Accepted)
            {
                QString newName = nameEdit->text();
                double newPeriod = periodSpinBox->value();
                double newFreq = 1000.0 / newPeriod; // 计算频率，单位MHz

                // 检查名称是否已存在
                for (int i = 0; i < timeSetDataList.size(); i++)
                {
                    if (i != index && timeSetDataList[i].name == newName)
                    {
                        QMessageBox::warning(this, "名称重复", "TimeSet名称已存在，请使用其他名称。");
                        return;
                    }
                }

                // 更新数据
                timeSetDataList[index].name = newName;
                timeSetDataList[index].period = newPeriod;

                // 更新显示
                item->setText(0, newName + " / " + QString::number(newPeriod) + "(ns) ⇔ " + QString::number(newFreq, 'f', 6) + "MHz");
            }
        }
    }
    else if (item && item->parent())
    {
        // 如果是子项（边沿项），调用原有的编辑边沿参数函数
        editEdgeItem(item, column);
    }
}

void TimeSetDialog::onItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    // 如果是顶级项（TimeSet）且双击第一列，则编辑TimeSet属性
    if (item && !item->parent() && column == 0)
    {
        editTimeSetProperties(item, column);
    }
    // 如果是子项但双击的是第一列（管脚名称），则显示完整的边沿参数对话框
    else if (item && item->parent() && column == 0)
    {
        editEdgeItem(item, column);
    }
    // 其他情况，例如属性列，会自动处理为直接编辑
}

void TimeSetDialog::onPropertyItemChanged(QTreeWidgetItem *item, int column)
{
    // 如果是顶级项（TimeSet）且编辑的是第0列（名称）
    if (item && !item->parent() && column == 0)
    {
        renameTimeSet(item, column);
        return;
    }

    // 如果不是子项或不是属性列，则忽略
    if (!item || !item->parent() || column < 1 || column > 4)
        return;

    // 获取修改后的值
    QString newValue = item->text(column);

    // 检查数值列是否为有效数字
    if (column >= 1 && column <= 3)
    {
        bool ok;
        double value = newValue.toDouble(&ok);
        if (!ok || value < 0)
        {
            // 无效输入，恢复之前的值
            QMessageBox::warning(this, "无效输入", "请输入有效的数值");

            // 重新获取这个管脚的值
            int parentIndex = timeSetTree->indexOfTopLevelItem(item->parent());
            QString pinText = item->text(0);
            QStringList pinNames = pinText.split(",");
            if (!pinNames.isEmpty())
            {
                QString pinName = pinNames.first().trimmed();
                int pinId = -1;
                for (auto it = pinList.begin(); it != pinList.end(); ++it)
                {
                    if (it.value() == pinName)
                    {
                        pinId = it.key();
                        break;
                    }
                }

                if (pinId != -1)
                {
                    for (const TimeSetEdgeData &edge : edgeDataList)
                    {
                        if (edge.timesetId == parentIndex && edge.pinId == pinId)
                        {
                            // 恢复原来的值
                            switch (column)
                            {
                            case 1:
                                item->setText(column, QString::number(edge.t1r));
                                break;
                            case 2:
                                item->setText(column, QString::number(edge.t1f));
                                break;
                            case 3:
                                item->setText(column, QString::number(edge.stbr));
                                break;
                            }
                            break;
                        }
                    }
                }
            }
            return;
        }
    }

    // 获取父项索引
    int parentIndex = timeSetTree->indexOfTopLevelItem(item->parent());

    // 获取管脚名称列表
    QString pinText = item->text(0);
    QStringList pinNames = pinText.split(",");
    for (int i = 0; i < pinNames.size(); i++)
    {
        pinNames[i] = pinNames[i].trimmed();
    }

    // 更新所有管脚的对应属性
    for (const QString &pinName : pinNames)
    {
        // 查找管脚ID
        int pinId = -1;
        for (auto it = pinList.begin(); it != pinList.end(); ++it)
        {
            if (it.value() == pinName)
            {
                pinId = it.key();
                break;
            }
        }

        if (pinId != -1)
        {
            // 更新这个管脚的属性
            for (int i = 0; i < edgeDataList.size(); i++)
            {
                TimeSetEdgeData &edge = edgeDataList[i];
                if (edge.timesetId == parentIndex && edge.pinId == pinId)
                {
                    // 根据列更新不同的属性
                    switch (column)
                    {
                    case 1: // T1R
                        edge.t1r = newValue.toDouble();
                        break;
                    case 2: // T1F
                        edge.t1f = newValue.toDouble();
                        break;
                    case 3: // STBR
                        edge.stbr = newValue.toDouble();
                        break;
                    case 4: // WAVE
                        // 查找波形ID
                        for (auto it = waveOptions.begin(); it != waveOptions.end(); ++it)
                        {
                            if (it.value() == newValue)
                            {
                                edge.waveId = it.key();
                                break;
                            }
                        }
                        break;
                    }
                    break;
                }
            }
        }
    }
}

// 添加一个独立的函数来显示管脚选择对话框，不依赖于TimeSetDialog实例
void TimeSetDialog::showPinSelectionDialogStandalone(int tableId, const QString &tableName)
{
    // 创建管脚选择对话框
    QDialog pinDialog(nullptr); // 使用nullptr而不是this作为父窗口
    pinDialog.setWindowTitle(tableName);
    pinDialog.setMinimumWidth(400);

    QVBoxLayout *mainLayout = new QVBoxLayout(&pinDialog);

    // 添加一个标签用于显示"管脚组"
    QLabel *groupLabel = new QLabel("管脚组", &pinDialog);
    QFont boldFont = groupLabel->font();
    boldFont.setBold(true);
    groupLabel->setFont(boldFont);
    mainLayout->addWidget(groupLabel);

    // 创建一个白色背景的widget来包含表格
    QWidget *tableWidget = new QWidget(&pinDialog);
    tableWidget->setStyleSheet("background-color: white;");
    QVBoxLayout *tableLayout = new QVBoxLayout(tableWidget);
    tableLayout->setContentsMargins(10, 10, 10, 10);

    // 添加表格布局用于显示管脚、类型和数据流
    QGridLayout *gridLayout = new QGridLayout();
    gridLayout->setSpacing(15);

    // 添加表头
    QLabel *pinHeader = new QLabel("Pins", tableWidget);
    QLabel *typeHeader = new QLabel("Type", tableWidget);
    QLabel *dataStreamHeader = new QLabel("Data Stream", tableWidget);

    // 设置表头样式
    QFont headerFont = pinHeader->font();
    headerFont.setBold(true);
    pinHeader->setFont(headerFont);
    typeHeader->setFont(headerFont);
    dataStreamHeader->setFont(headerFont);

    pinHeader->setAlignment(Qt::AlignLeft);
    typeHeader->setAlignment(Qt::AlignLeft);
    dataStreamHeader->setAlignment(Qt::AlignLeft);

    gridLayout->addWidget(pinHeader, 0, 0);
    gridLayout->addWidget(typeHeader, 0, 1);
    gridLayout->addWidget(dataStreamHeader, 0, 2);

    // 添加表头分隔线
    QFrame *headerLine = new QFrame(tableWidget);
    headerLine->setFrameShape(QFrame::HLine);
    headerLine->setFrameShadow(QFrame::Sunken);
    gridLayout->addWidget(headerLine, 1, 0, 1, 3);

    // 从数据库加载类型选项
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery query(db);

    // 首先确保type_options表存在
    if (!db.tables().contains("type_options"))
    {
        query.exec("CREATE TABLE type_options ("
                   "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                   "type_name TEXT NOT NULL UNIQUE"
                   ")");

        // 添加默认类型
        query.exec("INSERT INTO type_options (type_name) VALUES ('InOut')");
        query.exec("INSERT INTO type_options (type_name) VALUES ('Input')");
        query.exec("INSERT INTO type_options (type_name) VALUES ('Output')");
    }

    // 确保vector_table_pins表存在
    if (!db.tables().contains("vector_table_pins"))
    {
        // 创建表
        QString createTableSql = "CREATE TABLE vector_table_pins ("
                                 "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                 "table_id INTEGER NOT NULL REFERENCES vector_tables(id),"
                                 "pin_id INTEGER NOT NULL REFERENCES pin_list(id),"
                                 "pin_channel_count INT NOT NULL DEFAULT 1,"
                                 "pin_type INTEGER NOT NULL DEFAULT 3 REFERENCES type_options(id)"
                                 ")";
        if (!query.exec(createTableSql))
        {
            QMessageBox::critical(nullptr, "数据库错误", "创建vector_table_pins表失败：" + query.lastError().text());
        }

        // 创建唯一索引
        QString createIndexSql = "CREATE UNIQUE INDEX idx_table_pin_unique "
                                 "ON vector_table_pins(table_id, pin_id)";
        if (!query.exec(createIndexSql))
        {
            QMessageBox::critical(nullptr, "数据库错误", "创建vector_table_pins索引失败：" + query.lastError().text());
        }
    }

    // 获取所有类型选项
    QMap<int, QString> typeOptions;
    if (query.exec("SELECT id, type_name FROM type_options"))
    {
        while (query.next())
        {
            int id = query.value(0).toInt();
            QString typeName = query.value(1).toString();
            typeOptions[id] = typeName;
        }
    }

    // 如果类型选项为空，添加一个默认选项
    if (typeOptions.isEmpty())
    {
        typeOptions[1] = "InOut";
    }

    // 获取所有管脚列表
    QMap<int, QString> localPinList;
    if (query.exec("SELECT id, pin_name FROM pin_list"))
    {
        while (query.next())
        {
            int id = query.value(0).toInt();
            QString pinName = query.value(1).toString();
            localPinList[id] = pinName;
        }
    }

    // 创建一个映射来存储每个管脚的复选框、类型选择框和数据流标签
    QMap<int, QCheckBox *> pinCheckboxes;
    QMap<int, QComboBox *> pinTypeComboBoxes;
    QMap<int, QLabel *> pinDataStreamLabels;

    // 添加管脚和对应的控件
    int row = 2; // 从第2行开始（0是表头，1是分隔线）
    for (auto it = localPinList.begin(); it != localPinList.end(); ++it)
    {
        int pinId = it.key();
        QString pinName = it.value();

        // 创建复选框
        QCheckBox *checkbox = new QCheckBox(pinName, tableWidget);
        pinCheckboxes[pinId] = checkbox;

        // 创建类型下拉框
        QComboBox *typeCombo = new QComboBox(tableWidget);
        for (auto typeIt = typeOptions.begin(); typeIt != typeOptions.end(); ++typeIt)
        {
            typeCombo->addItem(typeIt.value(), typeIt.key());
        }
        pinTypeComboBoxes[pinId] = typeCombo;

        // 创建数据流标签
        QLabel *dataStreamLabel = new QLabel("x1", tableWidget);
        dataStreamLabel->setAlignment(Qt::AlignCenter);
        pinDataStreamLabels[pinId] = dataStreamLabel;

        // 添加到表格
        gridLayout->addWidget(checkbox, row, 0);
        gridLayout->addWidget(typeCombo, row, 1);
        gridLayout->addWidget(dataStreamLabel, row, 2);

        row++;
    }

    tableLayout->addLayout(gridLayout);
    mainLayout->addWidget(tableWidget);

    // 添加底部按钮区域
    QWidget *buttonContainer = new QWidget(&pinDialog);
    buttonContainer->setStyleSheet("background-color: #f0f0f0;");
    QHBoxLayout *buttonLayout = new QHBoxLayout(buttonContainer);
    buttonLayout->setContentsMargins(10, 5, 10, 5);
    buttonLayout->addStretch();

    QPushButton *okButton = new QPushButton("确定", buttonContainer);
    QPushButton *cancelButton = new QPushButton("取消向导", buttonContainer);

    okButton->setMinimumWidth(100);
    cancelButton->setMinimumWidth(100);

    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);

    mainLayout->addWidget(buttonContainer);

    // 连接信号槽
    connect(okButton, &QPushButton::clicked, &pinDialog, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, &pinDialog, &QDialog::reject);

    // 显示对话框
    if (pinDialog.exec() == QDialog::Accepted)
    {
        // 获取选中的管脚并保存到数据库
        QSqlDatabase db = DatabaseManager::instance()->database();
        QSqlQuery query(db);

        // 开始事务
        db.transaction();

        bool success = true;

        for (auto it = pinCheckboxes.begin(); it != pinCheckboxes.end(); ++it)
        {
            int pinId = it.key();
            QCheckBox *checkbox = it.value();

            // 如果勾选了该管脚
            if (checkbox->isChecked())
            {
                // 获取选择的类型
                QComboBox *typeCombo = pinTypeComboBoxes[pinId];
                int typeId = typeCombo->currentData().toInt();

                // 固定使用1作为channel_count
                int channelCount = 1;

                // 保存到数据库
                query.prepare("INSERT INTO vector_table_pins (table_id, pin_id, pin_type, pin_channel_count) "
                              "VALUES (?, ?, ?, ?)");
                query.addBindValue(tableId);
                query.addBindValue(pinId);
                query.addBindValue(typeId);
                query.addBindValue(channelCount);

                if (!query.exec())
                {
                    QMessageBox::critical(nullptr, "数据库错误", "保存管脚信息失败：" + query.lastError().text());
                    success = false;
                    break;
                }
            }
        }

        // 提交或回滚事务
        if (success)
        {
            db.commit();
            QMessageBox::information(nullptr, "保存成功", "管脚信息已成功保存！");
        }
        else
        {
            db.rollback();
        }
    }
}

// 添加一个新函数用于显示管脚选择对话框
void TimeSetDialog::showPinSelectionDialog(int tableId, const QString &tableName)
{
    // 创建管脚选择对话框
    QDialog pinDialog(this);
    pinDialog.setWindowTitle(tableName);
    pinDialog.setMinimumWidth(400);

    QVBoxLayout *mainLayout = new QVBoxLayout(&pinDialog);

    // 添加一个标签用于显示"管脚组"
    QLabel *groupLabel = new QLabel("管脚组", &pinDialog);
    QFont boldFont = groupLabel->font();
    boldFont.setBold(true);
    groupLabel->setFont(boldFont);
    mainLayout->addWidget(groupLabel);

    // 创建一个白色背景的widget来包含表格
    QWidget *tableWidget = new QWidget(&pinDialog);
    tableWidget->setStyleSheet("background-color: white;");
    QVBoxLayout *tableLayout = new QVBoxLayout(tableWidget);
    tableLayout->setContentsMargins(10, 10, 10, 10);

    // 添加表格布局用于显示管脚、类型和数据流
    QGridLayout *gridLayout = new QGridLayout();
    gridLayout->setSpacing(15);

    // 添加表头
    QLabel *pinHeader = new QLabel("Pins", tableWidget);
    QLabel *typeHeader = new QLabel("Type", tableWidget);
    QLabel *dataStreamHeader = new QLabel("Data Stream", tableWidget);

    // 设置表头样式
    QFont headerFont = pinHeader->font();
    headerFont.setBold(true);
    pinHeader->setFont(headerFont);
    typeHeader->setFont(headerFont);
    dataStreamHeader->setFont(headerFont);

    pinHeader->setAlignment(Qt::AlignLeft);
    typeHeader->setAlignment(Qt::AlignLeft);
    dataStreamHeader->setAlignment(Qt::AlignLeft);

    gridLayout->addWidget(pinHeader, 0, 0);
    gridLayout->addWidget(typeHeader, 0, 1);
    gridLayout->addWidget(dataStreamHeader, 0, 2);

    // 添加表头分隔线
    QFrame *headerLine = new QFrame(tableWidget);
    headerLine->setFrameShape(QFrame::HLine);
    headerLine->setFrameShadow(QFrame::Sunken);
    gridLayout->addWidget(headerLine, 1, 0, 1, 3);

    // 从数据库加载类型选项
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery query(db);

    // 首先确保type_options表存在
    if (!db.tables().contains("type_options"))
    {
        query.exec("CREATE TABLE type_options ("
                   "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                   "type_name TEXT NOT NULL UNIQUE"
                   ")");

        // 添加默认类型
        query.exec("INSERT INTO type_options (type_name) VALUES ('InOut')");
        query.exec("INSERT INTO type_options (type_name) VALUES ('Input')");
        query.exec("INSERT INTO type_options (type_name) VALUES ('Output')");
    }

    // 确保vector_table_pins表存在
    if (!db.tables().contains("vector_table_pins"))
    {
        // 创建表
        QString createTableSql = "CREATE TABLE vector_table_pins ("
                                 "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                 "table_id INTEGER NOT NULL REFERENCES vector_tables(id),"
                                 "pin_id INTEGER NOT NULL REFERENCES pin_list(id),"
                                 "pin_channel_count INT NOT NULL DEFAULT 1,"
                                 "pin_type INTEGER NOT NULL DEFAULT 3 REFERENCES type_options(id)"
                                 ")";
        if (!query.exec(createTableSql))
        {
            QMessageBox::critical(this, "数据库错误", "创建vector_table_pins表失败：" + query.lastError().text());
        }

        // 创建唯一索引
        QString createIndexSql = "CREATE UNIQUE INDEX idx_table_pin_unique "
                                 "ON vector_table_pins(table_id, pin_id)";
        if (!query.exec(createIndexSql))
        {
            QMessageBox::critical(this, "数据库错误", "创建vector_table_pins索引失败：" + query.lastError().text());
        }
    }

    // 获取所有类型选项
    QMap<int, QString> typeOptions;
    if (query.exec("SELECT id, type_name FROM type_options"))
    {
        while (query.next())
        {
            int id = query.value(0).toInt();
            QString typeName = query.value(1).toString();
            typeOptions[id] = typeName;
        }
    }

    // 如果类型选项为空，添加一个默认选项
    if (typeOptions.isEmpty())
    {
        typeOptions[1] = "InOut";
    }

    // 创建一个映射来存储每个管脚的复选框、类型选择框和数据流标签
    QMap<int, QCheckBox *> pinCheckboxes;
    QMap<int, QComboBox *> pinTypeComboBoxes;
    QMap<int, QLabel *> pinDataStreamLabels;

    // 添加管脚和对应的控件
    int row = 2; // 从第2行开始（0是表头，1是分隔线）
    for (auto it = pinList.begin(); it != pinList.end(); ++it)
    {
        int pinId = it.key();
        QString pinName = it.value();

        // 创建复选框
        QCheckBox *checkbox = new QCheckBox(pinName, tableWidget);
        pinCheckboxes[pinId] = checkbox;

        // 创建类型下拉框
        QComboBox *typeCombo = new QComboBox(tableWidget);
        for (auto typeIt = typeOptions.begin(); typeIt != typeOptions.end(); ++typeIt)
        {
            typeCombo->addItem(typeIt.value(), typeIt.key());
        }
        pinTypeComboBoxes[pinId] = typeCombo;

        // 创建数据流标签
        QLabel *dataStreamLabel = new QLabel("x1", tableWidget);
        dataStreamLabel->setAlignment(Qt::AlignCenter);
        pinDataStreamLabels[pinId] = dataStreamLabel;

        // 添加到表格
        gridLayout->addWidget(checkbox, row, 0);
        gridLayout->addWidget(typeCombo, row, 1);
        gridLayout->addWidget(dataStreamLabel, row, 2);

        row++;
    }

    tableLayout->addLayout(gridLayout);
    mainLayout->addWidget(tableWidget);

    // 添加底部按钮区域
    QWidget *buttonContainer = new QWidget(&pinDialog);
    buttonContainer->setStyleSheet("background-color: #f0f0f0;");
    QHBoxLayout *buttonLayout = new QHBoxLayout(buttonContainer);
    buttonLayout->setContentsMargins(10, 5, 10, 5);
    buttonLayout->addStretch();

    QPushButton *okButton = new QPushButton("确定", buttonContainer);
    QPushButton *cancelButton = new QPushButton("取消向导", buttonContainer);

    okButton->setMinimumWidth(100);
    cancelButton->setMinimumWidth(100);

    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);

    mainLayout->addWidget(buttonContainer);

    // 连接信号槽
    connect(okButton, &QPushButton::clicked, &pinDialog, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, &pinDialog, &QDialog::reject);

    // 显示对话框
    if (pinDialog.exec() == QDialog::Accepted)
    {
        // 获取选中的管脚并保存到数据库
        QSqlDatabase db = DatabaseManager::instance()->database();
        QSqlQuery query(db);

        // 开始事务
        db.transaction();

        bool success = true;

        for (auto it = pinCheckboxes.begin(); it != pinCheckboxes.end(); ++it)
        {
            int pinId = it.key();
            QCheckBox *checkbox = it.value();

            // 如果勾选了该管脚
            if (checkbox->isChecked())
            {
                // 获取选择的类型
                QComboBox *typeCombo = pinTypeComboBoxes[pinId];
                int typeId = typeCombo->currentData().toInt();

                // 固定使用1作为channel_count
                int channelCount = 1;

                // 保存到数据库
                query.prepare("INSERT INTO vector_table_pins (table_id, pin_id, pin_type, pin_channel_count) "
                              "VALUES (?, ?, ?, ?)");
                query.addBindValue(tableId);
                query.addBindValue(pinId);
                query.addBindValue(typeId);
                query.addBindValue(channelCount);

                if (!query.exec())
                {
                    QMessageBox::critical(this, "数据库错误", "保存管脚信息失败：" + query.lastError().text());
                    success = false;
                    break;
                }
            }
        }

        // 提交或回滚事务
        if (success)
        {
            db.commit();
            QMessageBox::information(this, "保存成功", "管脚信息已成功保存！");
        }
        else
        {
            db.rollback();
        }
    }
}