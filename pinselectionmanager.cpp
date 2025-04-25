#include "pinselectionmanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QMessageBox>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QComboBox>

PinSelectionManager::PinSelectionManager(QListWidget *pinListWidget, TimeSetDataAccess *dataAccess)
    : m_pinListWidget(pinListWidget), m_dataAccess(dataAccess)
{
}

void PinSelectionManager::populatePinList(const QMap<int, QString> &pinList)
{
    m_pinListWidget->clear();

    for (auto it = pinList.begin(); it != pinList.end(); ++it)
    {
        QListWidgetItem *item = new QListWidgetItem(it.value());
        item->setData(Qt::UserRole, it.key());
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        m_pinListWidget->addItem(item);
    }
}

void PinSelectionManager::selectPinsForTimeSet(int timeSetId)
{
    // 获取TimeSet关联的管脚ID
    QList<int> pinIds = m_dataAccess->getPinIdsForTimeSet(timeSetId);

    // 更新列表选择状态
    for (int i = 0; i < m_pinListWidget->count(); i++)
    {
        QListWidgetItem *item = m_pinListWidget->item(i);
        int pinId = item->data(Qt::UserRole).toInt();

        if (pinIds.contains(pinId))
        {
            item->setCheckState(Qt::Checked);
        }
        else
        {
            item->setCheckState(Qt::Unchecked);
        }
    }
}

QList<int> PinSelectionManager::getSelectedPinIds() const
{
    QList<int> selectedPins;

    for (int i = 0; i < m_pinListWidget->count(); i++)
    {
        QListWidgetItem *item = m_pinListWidget->item(i);
        if (item->checkState() == Qt::Checked)
        {
            selectedPins.append(item->data(Qt::UserRole).toInt());
        }
    }

    return selectedPins;
}

void PinSelectionManager::showPinSelectionDialog(int tableId, const QString &tableName, QWidget *parent)
{
    // 创建对话框
    QDialog dialog(parent);
    dialog.setWindowTitle("选择管脚 - " + tableName);
    dialog.setMinimumSize(400, 400);

    // 创建布局
    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);

    // 创建标题标签
    QLabel *titleLabel = new QLabel("为表 " + tableName + " 选择管脚", &dialog);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    // 创建管脚列表
    QListWidget *pinsList = new QListWidget(&dialog);
    pinsList->setAlternatingRowColors(true);
    mainLayout->addWidget(pinsList);

    // 加载所有管脚
    QMap<int, QString> allPins;
    m_dataAccess->loadPins(allPins);

    // 查询已选择的管脚
    QSqlQuery query;
    query.prepare("SELECT pin_id FROM vector_pin_options WHERE table_id = ?");
    query.addBindValue(tableId);

    QSet<int> selectedPinIds;
    if (query.exec())
    {
        while (query.next())
        {
            selectedPinIds.insert(query.value(0).toInt());
        }
    }

    // 填充列表并设置选中状态
    for (auto it = allPins.begin(); it != allPins.end(); ++it)
    {
        QListWidgetItem *item = new QListWidgetItem(it.value());
        item->setData(Qt::UserRole, it.key());
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);

        if (selectedPinIds.contains(it.key()))
        {
            item->setCheckState(Qt::Checked);
        }
        else
        {
            item->setCheckState(Qt::Unchecked);
        }

        pinsList->addItem(item);
    }

    // 添加按钮
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *okButton = new QPushButton("确定", &dialog);
    QPushButton *cancelButton = new QPushButton("取消", &dialog);

    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    mainLayout->addLayout(buttonLayout);

    // 连接信号槽
    QObject::connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    QObject::connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);

    // 显示对话框
    if (dialog.exec() == QDialog::Accepted)
    {
        // 收集选中的管脚
        QList<int> newSelectedPinIds;
        for (int i = 0; i < pinsList->count(); i++)
        {
            QListWidgetItem *item = pinsList->item(i);
            if (item->checkState() == Qt::Checked)
            {
                newSelectedPinIds.append(item->data(Qt::UserRole).toInt());
            }
        }

        // 删除旧的管脚选项
        QSqlQuery deleteQuery;
        deleteQuery.prepare("DELETE FROM vector_pin_options WHERE table_id = ?");
        deleteQuery.addBindValue(tableId);

        if (!deleteQuery.exec())
        {
            qWarning() << "删除旧管脚选项失败:" << deleteQuery.lastError().text();
            return;
        }

        // 添加新的管脚选项
        for (int i = 0; i < newSelectedPinIds.size(); i++)
        {
            int pinId = newSelectedPinIds[i];
            QString pinName = allPins[pinId];

            QSqlQuery insertQuery;
            insertQuery.prepare("INSERT INTO vector_pin_options (table_id, pin_id, pin_name, pin_order) VALUES (?, ?, ?, ?)");
            insertQuery.addBindValue(tableId);
            insertQuery.addBindValue(pinId);
            insertQuery.addBindValue(pinName);
            insertQuery.addBindValue(i); // 使用循环索引作为顺序

            if (!insertQuery.exec())
            {
                qWarning() << "添加管脚选项失败:" << insertQuery.lastError().text();
                return;
            }
        }
    }
}

void PinSelectionManager::showPinSelectionDialogStandalone(int tableId, const QString &tableName, QWidget *parent)
{
    // 创建对话框
    QDialog dialog(parent);
    dialog.setWindowTitle("管脚选择和数据查看 - " + tableName);
    dialog.resize(800, 600);

    // 创建布局
    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);

    // 创建表格部分
    QTableWidget *vectorTable = new QTableWidget(&dialog);
    vectorTable->setAlternatingRowColors(true);
    vectorTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    vectorTable->setEditTriggers(QAbstractItemView::NoEditTriggers); // 设置为只读模式

    mainLayout->addWidget(vectorTable);

    // 加载数据
    m_dataAccess->loadVectorData(tableId, vectorTable);

    // 创建按钮区域
    QHBoxLayout *buttonLayout = new QHBoxLayout();

    QPushButton *selectPinsButton = new QPushButton("选择管脚", &dialog);
    QPushButton *editDataButton = new QPushButton("编辑数据", &dialog);
    QPushButton *closeButton = new QPushButton("关闭", &dialog);

    buttonLayout->addWidget(selectPinsButton);
    buttonLayout->addWidget(editDataButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);

    mainLayout->addLayout(buttonLayout);

    // 连接信号槽
    QObject::connect(selectPinsButton, &QPushButton::clicked, [this, tableId, tableName, &dialog]()
                     {
        showPinSelectionDialog(tableId, tableName, &dialog);
        // 重新加载数据
        QTableWidget* table = dialog.findChild<QTableWidget*>();
        if (table)
        {
            table->clear();
            m_dataAccess->loadVectorData(tableId, table);
        } });

    QObject::connect(editDataButton, &QPushButton::clicked, [tableId, tableName, parent]()
                     {
        // 这里应调用向量数据对话框，但目前我们没有实现，所以留空
        QMessageBox::information(parent, "功能未实现", "编辑数据功能将在VectorDataManager模块中实现。"); });

    QObject::connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);

    // 显示对话框
    dialog.exec();
}