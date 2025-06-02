#include "pingroupdialog.h"
#include "database/databasemanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QFrame>
#include <QFont>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

PinGroupDialog::PinGroupDialog(QWidget *parent)
    : QDialog(parent), m_currentTableId(-1), m_isLoading(false)
{
    setWindowTitle("添加管脚分组");
    setMinimumSize(600, 400);

    setupUI();
    loadAllVectorTables();
}

PinGroupDialog::~PinGroupDialog()
{
}

void PinGroupDialog::setupUI()
{
    qDebug() << "PinGroupDialog::setupUI - 设置界面UI";

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // 顶部区域 - 向量表选择下拉框
    QFormLayout *topLayout = new QFormLayout();
    m_vectorTableComboBox = new QComboBox(this);
    topLayout->addRow("选择向量表:", m_vectorTableComboBox);
    mainLayout->addLayout(topLayout);

    // 中间区域 - 两个表格并排
    QHBoxLayout *tableLayout = new QHBoxLayout();

    // 左侧表格 - 向量表
    QVBoxLayout *leftLayout = new QVBoxLayout();
    QLabel *leftLabel = new QLabel("向量表:", this);
    QFont boldFont = leftLabel->font();
    boldFont.setBold(true);
    leftLabel->setFont(boldFont);
    leftLayout->addWidget(leftLabel);

    m_leftTable = new QTableWidget(this);
    m_leftTable->setColumnCount(2);
    m_leftTable->setHorizontalHeaderLabels(QStringList() << "选择"
                                                         << "向量表");
    m_leftTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_leftTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_leftTable->verticalHeader()->setVisible(false);
    m_leftTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_leftTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    leftLayout->addWidget(m_leftTable);

    tableLayout->addLayout(leftLayout);

    // 右侧表格 - 管脚
    QVBoxLayout *rightLayout = new QVBoxLayout();
    QLabel *rightLabel = new QLabel("管脚:", this);
    rightLabel->setFont(boldFont);
    rightLayout->addWidget(rightLabel);

    // 添加管脚类型筛选下拉框
    QHBoxLayout *filterLayout = new QHBoxLayout();
    QLabel *filterLabel = new QLabel("管脚类型:", this);
    m_pinTypeComboBox = new QComboBox(this);
    m_pinTypeComboBox->addItem("全部");
    m_pinTypeComboBox->addItem("In");
    m_pinTypeComboBox->addItem("Out");
    m_pinTypeComboBox->addItem("InOut");
    filterLayout->addWidget(filterLabel);
    filterLayout->addWidget(m_pinTypeComboBox);
    filterLayout->addStretch();
    rightLayout->addLayout(filterLayout);

    m_rightTable = new QTableWidget(this);
    m_rightTable->setColumnCount(3);
    m_rightTable->setHorizontalHeaderLabels(QStringList() << "选择"
                                                          << "管脚"
                                                          << "类型");
    m_rightTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_rightTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_rightTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_rightTable->verticalHeader()->setVisible(false);
    m_rightTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_rightTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    rightLayout->addWidget(m_rightTable);

    tableLayout->addLayout(rightLayout);
    mainLayout->addLayout(tableLayout);

    // 底部区域 - 分组名称输入和按钮
    QGridLayout *bottomLayout = new QGridLayout();
    QLabel *nameLabel = new QLabel("分组名称:", this);
    m_groupNameEdit = new QLineEdit(this);

    bottomLayout->addWidget(nameLabel, 0, 0);
    bottomLayout->addWidget(m_groupNameEdit, 0, 1);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_okButton = new QPushButton("确定", this);
    m_cancelButton = new QPushButton("取消", this);

    buttonLayout->addStretch();
    buttonLayout->addWidget(m_okButton);
    buttonLayout->addWidget(m_cancelButton);

    bottomLayout->addLayout(buttonLayout, 1, 0, 1, 2);
    mainLayout->addLayout(bottomLayout);

    // 连接信号和槽
    connect(m_vectorTableComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PinGroupDialog::onVectorTableSelectionChanged);
    connect(m_pinTypeComboBox, &QComboBox::currentTextChanged,
            this, &PinGroupDialog::onPinTypeFilterChanged);
    connect(m_okButton, &QPushButton::clicked, this, &PinGroupDialog::onAccepted);
    connect(m_cancelButton, &QPushButton::clicked, this, &PinGroupDialog::onRejected);
}

void PinGroupDialog::loadAllVectorTables()
{
    qDebug() << "PinGroupDialog::loadAllVectorTables - 加载所有向量表";

    m_isLoading = true;

    // 清空当前的向量表
    m_vectorTableComboBox->clear();
    m_leftTable->setRowCount(0);

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qDebug() << "PinGroupDialog::loadAllVectorTables - 数据库未打开";
        return;
    }

    // 查询所有向量表
    QSqlQuery query(db);
    if (query.exec("SELECT id, table_name FROM vector_tables ORDER BY table_name"))
    {
        while (query.next())
        {
            int tableId = query.value(0).toInt();
            QString tableName = query.value(1).toString();

            // 添加到下拉选择框
            m_vectorTableComboBox->addItem(tableName, tableId);

            // 添加到左侧表格
            int row = m_leftTable->rowCount();
            m_leftTable->insertRow(row);

            // 创建复选框
            QWidget *checkBoxWidget = new QWidget(m_leftTable);
            QHBoxLayout *checkBoxLayout = new QHBoxLayout(checkBoxWidget);
            checkBoxLayout->setAlignment(Qt::AlignCenter);
            checkBoxLayout->setContentsMargins(0, 0, 0, 0);

            QCheckBox *checkBox = new QCheckBox(checkBoxWidget);
            checkBoxLayout->addWidget(checkBox);

            // 连接信号槽以确保只能选择一个向量表
            connect(checkBox, &QCheckBox::toggled, [this, row, checkBox, tableId](bool checked)
                    {
                if (checked) {
                    // 取消其他复选框的选中状态
                    for (int i = 0; i < m_leftTable->rowCount(); ++i) {
                        if (i != row) {
                            QWidget *widget = m_leftTable->cellWidget(i, 0);
                            if (widget) {
                                QCheckBox *otherCheckBox = widget->findChild<QCheckBox*>();
                                if (otherCheckBox && otherCheckBox->isChecked()) {
                                    otherCheckBox->setChecked(false);
                                }
                            }
                        }
                    }
                    
                    // 设置当前选中的向量表ID
                    m_currentTableId = tableId;
                    
                    // 加载该向量表的管脚数据
                    loadVectorTablePins(tableId);
                    
                    // 将下拉框同步到当前选择
                    int index = m_vectorTableComboBox->findData(tableId);
                    if (index >= 0 && m_vectorTableComboBox->currentIndex() != index) {
                        m_vectorTableComboBox->setCurrentIndex(index);
                    }
                } else {
                    // 如果取消选中，则清空右侧管脚列表
                    if (m_currentTableId == tableId) {
                        m_currentTableId = -1;
                        m_rightTable->setRowCount(0);
                        m_pinNameToIds.clear();
                        m_pinIdToType.clear();
                        m_selectedPinIds.clear();
                    }
                } });

            m_leftTable->setCellWidget(row, 0, checkBoxWidget);
            m_leftTable->setItem(row, 1, new QTableWidgetItem(tableName));
        }
    }
    else
    {
        qDebug() << "PinGroupDialog::loadAllVectorTables - 查询向量表失败:" << query.lastError().text();
    }

    m_isLoading = false;

    // 如果有向量表，默认选择第一个
    if (m_vectorTableComboBox->count() > 0)
    {
        m_vectorTableComboBox->setCurrentIndex(0);
    }
}

void PinGroupDialog::loadVectorTablePins(int tableId)
{
    qDebug() << "PinGroupDialog::loadVectorTablePins - 加载向量表管脚, tableId:" << tableId;

    // 清空右侧表格和缓存
    m_rightTable->setRowCount(0);
    m_pinNameToIds.clear();
    m_pinIdToType.clear();
    m_selectedPinIds.clear();

    if (tableId <= 0)
    {
        return;
    }

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qDebug() << "PinGroupDialog::loadVectorTablePins - 数据库未打开";
        return;
    }

    // 查询该向量表的所有管脚
    QSqlQuery query(db);
    query.prepare("SELECT vtp.id, pl.id as pin_id, pl.pin_name, t.type_name, vtp.pin_type "
                  "FROM vector_table_pins vtp "
                  "JOIN pin_list pl ON vtp.pin_id = pl.id "
                  "JOIN type_options t ON vtp.pin_type = t.id "
                  "WHERE vtp.table_id = ? "
                  "ORDER BY pl.pin_name");
    query.addBindValue(tableId);

    if (query.exec())
    {
        while (query.next())
        {
            int vectorPinId = query.value(0).toInt();
            int pinId = query.value(1).toInt();
            QString pinName = query.value(2).toString();
            QString typeName = query.value(3).toString();
            int typeId = query.value(4).toInt();

            // 保存管脚信息到映射
            if (!m_pinNameToIds.contains(pinName))
            {
                m_pinNameToIds[pinName] = QList<int>();
            }
            m_pinNameToIds[pinName].append(pinId);
            m_pinIdToType[pinId] = typeName;

            // 添加到右侧表格
            int row = m_rightTable->rowCount();
            m_rightTable->insertRow(row);

            // 创建复选框
            QWidget *checkBoxWidget = new QWidget(m_rightTable);
            QHBoxLayout *checkBoxLayout = new QHBoxLayout(checkBoxWidget);
            checkBoxLayout->setAlignment(Qt::AlignCenter);
            checkBoxLayout->setContentsMargins(0, 0, 0, 0);

            QCheckBox *checkBox = new QCheckBox(checkBoxWidget);
            checkBoxLayout->addWidget(checkBox);

            // 连接信号槽以记录选中的管脚
            connect(checkBox, &QCheckBox::toggled, [this, pinId](bool checked)
                    {
                if (checked) {
                    m_selectedPinIds.insert(pinId);
                } else {
                    m_selectedPinIds.remove(pinId);
                }
                qDebug() << "PinGroupDialog - 管脚选择状态变更, pinId:" << pinId 
                         << "已选管脚数:" << m_selectedPinIds.size(); });

            m_rightTable->setCellWidget(row, 0, checkBoxWidget);
            m_rightTable->setItem(row, 1, new QTableWidgetItem(pinName));
            m_rightTable->setItem(row, 2, new QTableWidgetItem(typeName));

            // 存储管脚ID及其所在行，用于筛选
            m_rightTable->item(row, 1)->setData(Qt::UserRole, pinId);
            m_rightTable->item(row, 2)->setData(Qt::UserRole, typeName);
        }
    }
    else
    {
        qDebug() << "PinGroupDialog::loadVectorTablePins - 查询管脚失败:" << query.lastError().text();
    }
}

void PinGroupDialog::onVectorTableSelectionChanged(int index)
{
    if (m_isLoading)
        return;

    qDebug() << "PinGroupDialog::onVectorTableSelectionChanged - 向量表选择变更, index:" << index;

    // 清除左侧表格所有复选框选中状态
    for (int i = 0; i < m_leftTable->rowCount(); ++i)
    {
        QWidget *widget = m_leftTable->cellWidget(i, 0);
        if (widget)
        {
            QCheckBox *checkBox = widget->findChild<QCheckBox *>();
            if (checkBox)
            {
                checkBox->setChecked(false);
            }
        }
    }

    if (index >= 0)
    {
        int tableId = m_vectorTableComboBox->itemData(index).toInt();

        // 勾选对应的左侧表格项
        for (int i = 0; i < m_leftTable->rowCount(); ++i)
        {
            if (m_leftTable->item(i, 1)->text() == m_vectorTableComboBox->currentText())
            {
                QWidget *widget = m_leftTable->cellWidget(i, 0);
                if (widget)
                {
                    QCheckBox *checkBox = widget->findChild<QCheckBox *>();
                    if (checkBox)
                    {
                        checkBox->setChecked(true);
                        break;
                    }
                }
            }
        }
    }
}

void PinGroupDialog::onPinTypeFilterChanged(const QString &type)
{
    qDebug() << "PinGroupDialog::onPinTypeFilterChanged - 类型筛选变更:" << type;

    filterPinsByType(type);
}

void PinGroupDialog::filterPinsByType(const QString &type)
{
    // 如果选择"全部"，则显示所有管脚
    if (type == "全部")
    {
        for (int i = 0; i < m_rightTable->rowCount(); ++i)
        {
            m_rightTable->setRowHidden(i, false);
        }
        return;
    }

    // 否则，只显示指定类型的管脚
    for (int i = 0; i < m_rightTable->rowCount(); ++i)
    {
        QTableWidgetItem *typeItem = m_rightTable->item(i, 2);
        QString pinType = typeItem->text();
        m_rightTable->setRowHidden(i, pinType != type);
    }
}

void PinGroupDialog::clearPinSelection()
{
    for (int i = 0; i < m_rightTable->rowCount(); ++i)
    {
        QWidget *widget = m_rightTable->cellWidget(i, 0);
        if (widget)
        {
            QCheckBox *checkBox = widget->findChild<QCheckBox *>();
            if (checkBox)
            {
                checkBox->setChecked(false);
            }
        }
    }
    m_selectedPinIds.clear();
}

void PinGroupDialog::onAccepted()
{
    qDebug() << "PinGroupDialog::onAccepted - 用户点击确定";

    // 检查是否选择了向量表
    if (m_currentTableId <= 0)
    {
        QMessageBox::warning(this, "警告", "请选择一个向量表！");
        return;
    }

    // 检查是否选择了管脚
    if (m_selectedPinIds.isEmpty())
    {
        QMessageBox::warning(this, "警告", "请至少选择一个管脚！");
        return;
    }

    // 检查分组名称是否为空
    QString groupName = m_groupNameEdit->text().trimmed();
    if (groupName.isEmpty())
    {
        QMessageBox::warning(this, "警告", "请输入分组名称！");
        m_groupNameEdit->setFocus();
        return;
    }

    // 保存分组数据
    if (saveGroupData())
    {
        accept();
    }
}

void PinGroupDialog::onRejected()
{
    qDebug() << "PinGroupDialog::onRejected - 用户点击取消";
    reject();
}

bool PinGroupDialog::saveGroupData()
{
    qDebug() << "PinGroupDialog::saveGroupData - 保存分组数据";

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qDebug() << "PinGroupDialog::saveGroupData - 数据库未打开";
        QMessageBox::critical(this, "错误", "数据库连接失败！");
        return false;
    }

    QString groupName = m_groupNameEdit->text().trimmed();
    QString pinType = m_pinTypeComboBox->currentText();

    // 获取类型ID
    int typeId = 3; // 默认为InOut (3)
    if (pinType == "In")
    {
        typeId = 1;
    }
    else if (pinType == "Out")
    {
        typeId = 2;
    }
    else if (pinType == "InOut")
    {
        typeId = 3;
    }

    // 验证分组名称唯一性
    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT COUNT(*) FROM pin_groups WHERE group_name = ?");
    checkQuery.addBindValue(groupName);

    if (checkQuery.exec() && checkQuery.next())
    {
        int count = checkQuery.value(0).toInt();
        if (count > 0)
        {
            QMessageBox::warning(this, "警告", "分组名称已存在，请使用不同的名称！");
            m_groupNameEdit->setFocus();
            return false;
        }
    }

    // 开始事务
    db.transaction();

    try
    {
        // 插入分组记录
        QSqlQuery insertGroupQuery(db);
        insertGroupQuery.prepare("INSERT INTO pin_groups (table_id, group_name, group_channel_count, group_type) "
                                 "VALUES (?, ?, 1, ?)");
        insertGroupQuery.addBindValue(m_currentTableId);
        insertGroupQuery.addBindValue(groupName);
        insertGroupQuery.addBindValue(typeId);

        if (!insertGroupQuery.exec())
        {
            throw QString("创建分组失败: %1").arg(insertGroupQuery.lastError().text());
        }

        // 获取新插入的分组ID
        int groupId = insertGroupQuery.lastInsertId().toInt();

        // 插入分组成员记录
        int sortIndex = 0;
        QSqlQuery insertMemberQuery(db);
        insertMemberQuery.prepare("INSERT INTO pin_group_members (group_id, pin_id, sort_index) "
                                  "VALUES (?, ?, ?)");

        foreach (int pinId, m_selectedPinIds)
        {
            insertMemberQuery.bindValue(0, groupId);
            insertMemberQuery.bindValue(1, pinId);
            insertMemberQuery.bindValue(2, sortIndex++);

            if (!insertMemberQuery.exec())
            {
                throw QString("添加分组成员失败: %1").arg(insertMemberQuery.lastError().text());
            }
        }

        // 提交事务
        db.commit();
        qDebug() << "PinGroupDialog::saveGroupData - 成功保存分组, groupId:" << groupId
                 << "成员数:" << m_selectedPinIds.size();

        QMessageBox::information(this, "成功", "分组创建成功！");
        return true;
    }
    catch (const QString &error)
    {
        // 回滚事务
        db.rollback();
        QMessageBox::critical(this, "错误", error);
        qDebug() << "PinGroupDialog::saveGroupData - 保存失败:" << error;
        return false;
    }
}

void PinGroupDialog::onPinSelectionChanged(bool checked)
{
    qDebug() << "PinGroupDialog::onPinSelectionChanged - 管脚选择状态变更:" << checked;
    // 这个函数在头文件中声明但在cpp中未实现，导致链接错误
    // 实际上这个功能已经在右侧表格的复选框勾选回调中实现了
    // 所以这个函数可以留空，只是为了修复编译错误
}