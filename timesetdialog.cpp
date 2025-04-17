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
}

TimeSetDialog::~TimeSetDialog()
{
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

    // 周期设置
    QFormLayout *periodLayout = new QFormLayout();
    periodSpinBox = new QDoubleSpinBox(leftWidget);
    periodSpinBox->setRange(1.0, 10000.0);
    periodSpinBox->setSingleStep(1.0);
    periodSpinBox->setDecimals(1);
    periodSpinBox->setValue(1000.0); // 默认1000ns
    periodSpinBox->setSuffix(" ns");
    periodLayout->addRow("周期:", periodSpinBox);
    leftLayout->addLayout(periodLayout);

    // 边沿操作按钮
    QHBoxLayout *edgeButtonLayout = new QHBoxLayout();
    addEdgeButton = new QPushButton("添加边沿参数", leftWidget);
    removeEdgeButton = new QPushButton("删除边沿参数", leftWidget);
    edgeButtonLayout->addWidget(addEdgeButton);
    edgeButtonLayout->addWidget(removeEdgeButton);
    leftLayout->addLayout(edgeButtonLayout);

    // 右侧为管脚选择
    pinSelectionGroup = new QGroupBox("管脚选择", mainSplitter);

    mainSplitter->addWidget(leftWidget);
    mainSplitter->addWidget(pinSelectionGroup);
    mainSplitter->setStretchFactor(0, 2);
    mainSplitter->setStretchFactor(1, 1);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttonBox);

    // 连接信号槽
    connect(addTimeSetButton, &QPushButton::clicked, this, &TimeSetDialog::addTimeSet);
    connect(removeTimeSetButton, &QPushButton::clicked, this, &TimeSetDialog::removeTimeSet);
    connect(timeSetTree, &QTreeWidget::itemChanged, this, &TimeSetDialog::renameTimeSet);
    connect(timeSetTree, &QTreeWidget::itemSelectionChanged, this, &TimeSetDialog::timeSetSelectionChanged);
    connect(periodSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &TimeSetDialog::updatePeriod);

    connect(addEdgeButton, &QPushButton::clicked, this, &TimeSetDialog::addEdgeItem);
    connect(removeEdgeButton, &QPushButton::clicked, this, &TimeSetDialog::removeEdgeItem);
    connect(timeSetTree, &QTreeWidget::itemDoubleClicked, this, &TimeSetDialog::editEdgeItem);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &TimeSetDialog::onAccepted);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &TimeSetDialog::onRejected);
}

void TimeSetDialog::setupTreeWidget()
{
    timeSetTree->setHeaderLabels(QStringList() << "TimeSet/边沿参数"
                                               << "属性");
    timeSetTree->setColumnCount(2);
    timeSetTree->setAlternatingRowColors(true);
    timeSetTree->setSelectionMode(QAbstractItemView::SingleSelection);
    timeSetTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    timeSetTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
}

void TimeSetDialog::setupPinSelection()
{
    QVBoxLayout *pinLayout = new QVBoxLayout(pinSelectionGroup);

    pinListWidget = new QListWidget(pinSelectionGroup);
    pinListWidget->setSelectionMode(QAbstractItemView::MultiSelection);

    pinLayout->addWidget(pinListWidget);

    // 添加管脚到列表
    for (auto it = pinList.begin(); it != pinList.end(); ++it)
    {
        QListWidgetItem *item = new QListWidgetItem(it.value());
        item->setData(Qt::UserRole, it.key()); // 存储管脚ID
        pinListWidget->addItem(item);
    }

    connect(pinListWidget, &QListWidget::itemSelectionChanged, this, &TimeSetDialog::onPinSelectionChanged);
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
        newTimeSet.period = periodSpinBox->value();

        // 添加到列表
        timeSetDataList.append(newTimeSet);

        // 创建树项
        QTreeWidgetItem *item = new QTreeWidgetItem(timeSetTree);
        item->setText(0, name);
        item->setText(1, QString::number(newTimeSet.period) + " ns");
        item->setFlags(item->flags() | Qt::ItemIsEditable);

        // 选择新项
        timeSetTree->setCurrentItem(item);
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
        }
    }
}

void TimeSetDialog::renameTimeSet(QTreeWidgetItem *item, int column)
{
    if (column == 0 && item && !item->parent())
    {
        QString newName = item->text(0);

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
                    item->setText(0, timeSetDataList[index].name); // 恢复原名称
                    return;
                }
            }

            // 更新数据
            timeSetDataList[index].name = newName;
        }
    }
}

void TimeSetDialog::updatePeriod(double value)
{
    if (currentTimeSetItem && currentTimeSetIndex >= 0)
    {
        timeSetDataList[currentTimeSetIndex].period = value;
        currentTimeSetItem->setText(1, QString::number(value) + " ns");
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

            // 更新UI
            periodSpinBox->setValue(timeSetDataList[currentTimeSetIndex].period);

            // 更新管脚选择
            QList<int> pinIds = timeSetDataList[currentTimeSetIndex].pinIds;
            for (int i = 0; i < pinListWidget->count(); i++)
            {
                QListWidgetItem *pinItem = pinListWidget->item(i);
                int pinId = pinItem->data(Qt::UserRole).toInt();
                pinItem->setSelected(pinIds.contains(pinId));
            }

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
        // 为每个选中的管脚创建边沿参数项
        for (QListWidgetItem *pinItem : selectedPins)
        {
            int pinId = pinItem->data(Qt::UserRole).toInt();
            QString pinName = pinItem->text();

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
                edgeData.t1r = dialog.getT1R();
                edgeData.t1f = dialog.getT1F();
                edgeData.stbr = dialog.getSTBR();
                edgeData.waveId = dialog.getWaveId();

                // 添加到列表
                edgeDataList.append(edgeData);

                // 创建树项
                QTreeWidgetItem *edgeItem = new QTreeWidgetItem(currentTimeSetItem);
                updateEdgeItemText(edgeItem, edgeData);
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
        int childIndex = item->parent()->indexOfChild(item);

        // 找到对应的边沿数据
        for (int i = 0; i < edgeDataList.size(); i++)
        {
            TimeSetEdgeData &edge = edgeDataList[i];
            if (edge.timesetId == parentIndex)
            {
                // 找到对应的管脚
                QString pinName = item->text(0).split(" [")[0]; // 获取管脚名称
                for (auto it = pinList.begin(); it != pinList.end(); ++it)
                {
                    if (it.value() == pinName && edge.pinId == it.key())
                    {
                        // 删除数据
                        edgeDataList.removeAt(i);
                        // 删除树项
                        delete item;
                        return;
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

        // 找到对应的边沿数据
        for (int i = 0; i < edgeDataList.size(); i++)
        {
            TimeSetEdgeData &edge = edgeDataList[i];
            if (edge.timesetId == parentIndex)
            {
                // 找到对应的管脚
                QString pinName = item->text(0).split(" [")[0]; // 获取管脚名称
                for (auto it = pinList.begin(); it != pinList.end(); ++it)
                {
                    if (it.value() == pinName && edge.pinId == it.key())
                    {
                        // 创建并显示边沿参数对话框
                        TimeSetEdgeDialog dialog(edge.t1r, edge.t1f, edge.stbr, edge.waveId, waveOptions, this);

                        if (dialog.exec() == QDialog::Accepted)
                        {
                            // 更新数据
                            edge.t1r = dialog.getT1R();
                            edge.t1f = dialog.getT1F();
                            edge.stbr = dialog.getSTBR();
                            edge.waveId = dialog.getWaveId();

                            // 更新显示
                            updateEdgeItemText(item, edge);
                        }
                        return;
                    }
                }
            }
        }
    }
}

void TimeSetDialog::onPinSelectionChanged()
{
    if (currentTimeSetIndex >= 0)
    {
        // 更新当前TimeSet的管脚列表
        QList<int> pinIds;
        QList<QListWidgetItem *> selectedItems = pinListWidget->selectedItems();

        for (QListWidgetItem *item : selectedItems)
        {
            pinIds.append(item->data(Qt::UserRole).toInt());
        }

        timeSetDataList[currentTimeSetIndex].pinIds = pinIds;
    }
}

void TimeSetDialog::updateEdgeItemText(QTreeWidgetItem *edgeItem, const TimeSetEdgeData &edgeData)
{
    QString pinName = pinList.value(edgeData.pinId);
    QString waveName = waveOptions.value(edgeData.waveId);

    edgeItem->setText(0, pinName + " [" + waveName + "]");
    edgeItem->setText(1, QString("T1R=%1, T1F=%2, STBR=%3")
                             .arg(edgeData.t1r)
                             .arg(edgeData.t1f)
                             .arg(edgeData.stbr));
}

void TimeSetDialog::onAccepted()
{
    // 保存数据到数据库
    if (saveToDatabase())
    {
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

void TimeSetDialog::updatePinSelection(QTreeWidgetItem *item, int column)
{
    if (currentTimeSetIndex >= 0 && item && !item->parent())
    {
        // 获取当前选中的管脚
        QList<QListWidgetItem *> selectedItems = pinListWidget->selectedItems();
        QList<int> pinIds;

        for (QListWidgetItem *pinItem : selectedItems)
        {
            pinIds.append(pinItem->data(Qt::UserRole).toInt());
        }

        // 更新当前TimeSet的管脚列表
        timeSetDataList[currentTimeSetIndex].pinIds = pinIds;
    }
}