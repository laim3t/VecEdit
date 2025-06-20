#include "pinlistdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QLabel>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include "../database/databasemanager.h"

PinListDialog::PinListDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("创建管脚向量");
    setMinimumSize(500, 300);

    setupUI();

    // 添加一个空行作为初始状态
    addNewPin();
}

PinListDialog::~PinListDialog()
{
}

void PinListDialog::setupUI()
{
    // 创建主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // 创建表格模型和视图
    pinModel = new QStandardItemModel(0, 3, this);
    pinModel->setHorizontalHeaderLabels(QStringList() << "名称"
                                                      << "数目"
                                                      << "创建名称");

    pinTableView = new QTableView(this);
    pinTableView->setModel(pinModel);
    pinTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    pinTableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    pinTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    pinTableView->verticalHeader()->setVisible(true);

    // 应用表格样式
    TableStyleManager::applyTableStyle(pinTableView);

    // 连接数据变化信号
    connect(pinModel, &QStandardItemModel::dataChanged, this, &PinListDialog::onDataChanged);

    mainLayout->addWidget(pinTableView);

    // 创建按钮布局
    QHBoxLayout *buttonLayout = new QHBoxLayout();

    // 添加和移除按钮
    addButton = new QPushButton("+", this);
    addButton->setToolTip("添加新管脚");
    connect(addButton, &QPushButton::clicked, this, &PinListDialog::addNewPin);

    removeButton = new QPushButton("-", this);
    removeButton->setToolTip("移除选中的管脚");
    connect(removeButton, &QPushButton::clicked, this, &PinListDialog::removePins);

    buttonLayout->addWidget(addButton);
    buttonLayout->addWidget(removeButton);
    buttonLayout->addStretch();

    // 对话框按钮
    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &PinListDialog::onAccepted);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    buttonLayout->addWidget(buttonBox);

    mainLayout->addLayout(buttonLayout);
}

void PinListDialog::addNewPin()
{
    int row = pinModel->rowCount();
    pinModel->insertRow(row);

    // 设置新行的单元格
    pinModel->setItem(row, 0, new QStandardItem("")); // 名称

    QStandardItem *countItem = new QStandardItem("1"); // 默认数目为1
    countItem->setTextAlignment(Qt::AlignCenter);
    pinModel->setItem(row, 1, countItem);

    pinModel->setItem(row, 2, new QStandardItem("")); // 创建名称

    // 创建一个新的PinData对象
    PinData newPin;
    newPin.pinName = "";
    newPin.pinCount = 1;
    newPin.displayName = "";

    // 添加到列表
    pinList.append(newPin);

    // 选中新行
    pinTableView->selectRow(row);

    // 设置"创建名称"列为只读
    QModelIndex index = pinModel->index(row, 2);
    QStandardItem *item = pinModel->itemFromIndex(index);
    if (item)
    {
        item->setEditable(false);
    }
}

void PinListDialog::removePins()
{
    QModelIndexList selected = pinTableView->selectionModel()->selectedRows();

    // 确保至少留一行
    if (pinModel->rowCount() - selected.count() < 1)
    {
        QMessageBox::warning(this, "警告", "至少需要保留一个管脚！");
        return;
    }

    // 从后往前删除，避免索引变化问题
    std::sort(selected.begin(), selected.end(), [](const QModelIndex &a, const QModelIndex &b)
              { return a.row() > b.row(); });

    for (const QModelIndex &index : selected)
    {
        int row = index.row();
        pinModel->removeRow(row);
        pinList.removeAt(row);
    }
}

void PinListDialog::onDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    // 只处理名称和数目列的变化
    for (int row = topLeft.row(); row <= bottomRight.row(); ++row)
    {
        int column = topLeft.column();

        if (column == 0 || column == 1)
        { // 名称或数目列
            QString name = pinModel->data(pinModel->index(row, 0)).toString().trimmed();
            QString countStr = pinModel->data(pinModel->index(row, 1)).toString().trimmed();

            // 更新PinData
            if (row < pinList.size())
            {
                pinList[row].pinName = name;

                bool ok;
                int count = countStr.toInt(&ok);
                if (ok && count > 0)
                {
                    pinList[row].pinCount = count;
                }
                else
                {
                    // 如果输入无效，重置为1并更新UI
                    pinList[row].pinCount = 1;
                    pinModel->setData(pinModel->index(row, 1), "1");
                }

                // 更新创建名称
                updateDisplayName(row);
            }
        }
    }
}

void PinListDialog::updateDisplayName(int row)
{
    if (row >= 0 && row < pinList.size())
    {
        QString name = pinList[row].pinName;
        int count = pinList[row].pinCount;

        QString displayName;

        if (name.isEmpty())
        {
            displayName = "";
        }
        else if (count <= 1)
        {
            displayName = name;
        }
        else
        {
            displayName = QString("%1%2 - %1%3").arg(name).arg(0).arg(count - 1);
        }

        pinList[row].displayName = displayName;
        pinModel->setData(pinModel->index(row, 2), displayName);
    }
}

void PinListDialog::onAccepted()
{
    qDebug() << "PinListDialog: OK button clicked. Attempting to save changes.";

    // 检查是否所有管脚都有名称
    bool allValid = true;
    for (int i = 0; i < pinList.size(); ++i)
    {
        if (pinList[i].pinName.isEmpty())
        {
            allValid = false;
            break;
        }
    }

    if (!allValid)
    {
        QMessageBox::warning(this, "无效输入", "所有管脚必须有名称！");
        return;
    }

    // 生成最终的管脚名称列表
    finalPinNames.clear();

    for (const PinData &pin : pinList)
    {
        if (pin.pinCount <= 1)
        {
            // 如果数目为1或无效，直接使用名称
            finalPinNames.append(pin.pinName);
        }
        else
        {
            // 否则生成序列化名称
            for (int i = 0; i < pin.pinCount; ++i)
            {
                finalPinNames.append(pin.pinName + QString::number(i));
            }
        }
    }

    // 保存到数据库
    QSqlDatabase db = DatabaseManager::instance()->database();
    bool success = true;
    
    // 清空现有的pin_list表，然后重新插入所有管脚
    QSqlQuery clearQuery(db);
    if (!clearQuery.exec("DELETE FROM pin_list")) {
        qWarning() << "Failed to clear pin_list table:" << clearQuery.lastError().text();
        QMessageBox::critical(this, "数据库错误", "清空管脚列表失败: " + clearQuery.lastError().text());
        success = false;
    }

    if (success) {
        qDebug() << "Successfully cleared pin_list table. Inserting" << finalPinNames.size() << "pins.";

        // 插入所有管脚
        QSqlQuery insertQuery(db);
        insertQuery.prepare("INSERT INTO pin_list (pin_name, pin_note, pin_nav_note) VALUES (?, ?, ?)");
        
        for (const QString &pinName : finalPinNames) {
            insertQuery.bindValue(0, pinName);
            insertQuery.bindValue(1, ""); // pin_note为空
            insertQuery.bindValue(2, ""); // pin_nav_note为空
            
            if (!insertQuery.exec()) {
                qWarning() << "Failed to insert pin" << pinName << ":" << insertQuery.lastError().text();
                QMessageBox::critical(this, "数据库错误", "插入管脚失败: " + insertQuery.lastError().text());
                success = false;
                break;
            }
        }
    }

    // 3. 根据结果关闭或保留对话框
    if (success) {
        qDebug() << "Pin changes successfully staged for commit by the caller.";
        accept(); // 操作成功，关闭对话框，让调用者去commit
    } else {
        qDebug() << "Pin changes failed. The transaction should be rolled back by the caller.";
        // 不要关闭对话框，让用户看到错误信息
    }
}

QList<QString> PinListDialog::getPinNames() const
{
    return finalPinNames;
}