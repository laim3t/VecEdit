#include "vectorpinsettingsdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QSqlDatabase>

VectorPinSettingsDialog::VectorPinSettingsDialog(int tableId, const QString &tableName, QWidget *parent)
    : QDialog(parent), m_tableId(tableId), m_tableName(tableName)
{
    qDebug() << "VectorPinSettingsDialog::VectorPinSettingsDialog - 初始化管脚设置对话框，表ID:" << tableId << "，表名:" << tableName;
    setupUI();
    loadPinsData();
}

VectorPinSettingsDialog::~VectorPinSettingsDialog()
{
    qDebug() << "VectorPinSettingsDialog::~VectorPinSettingsDialog - 销毁管脚设置对话框";
}

void VectorPinSettingsDialog::setupUI()
{
    qDebug() << "VectorPinSettingsDialog::setupUI - 设置对话框UI";

    // 设置对话框属性
    setWindowTitle("管脚选择 - " + m_tableName);
    setMinimumSize(500, 400);

    // 创建主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // 创建标题标签
    QLabel *titleLabel = new QLabel("管脚组", this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    // 创建管脚表格
    m_pinsTable = new QTableWidget(this);
    m_pinsTable->setColumnCount(3);
    m_pinsTable->setHorizontalHeaderLabels(QStringList() << "Pins"
                                                         << "Type"
                                                         << "Data Stream");
    m_pinsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_pinsTable->setAlternatingRowColors(true);
    m_pinsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mainLayout->addWidget(m_pinsTable);

    // 创建按钮布局
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_okButton = new QPushButton("确定", this);
    m_cancelButton = new QPushButton("取消向导", this);

    buttonLayout->addStretch();
    buttonLayout->addWidget(m_okButton);
    buttonLayout->addWidget(m_cancelButton);
    mainLayout->addLayout(buttonLayout);

    // 连接信号槽
    connect(m_okButton, &QPushButton::clicked, this, &VectorPinSettingsDialog::onAccepted);
    connect(m_cancelButton, &QPushButton::clicked, this, &VectorPinSettingsDialog::onRejected);
}

void VectorPinSettingsDialog::loadPinsData()
{
    qDebug() << "VectorPinSettingsDialog::loadPinsData - 加载管脚数据";

    // 加载所有管脚
    QSqlQuery pinQuery("SELECT id, pin_name FROM pin_list ORDER BY pin_name");
    if (pinQuery.exec())
    {
        while (pinQuery.next())
        {
            int pinId = pinQuery.value(0).toInt();
            QString pinName = pinQuery.value(1).toString();
            m_allPins[pinId] = pinName;
        }
    }
    else
    {
        qWarning() << "VectorPinSettingsDialog::loadPinsData - 无法查询管脚数据:" << pinQuery.lastError().text();
    }

    // 从vector_table_pins表中加载已经关联到该向量表的管脚
    QSqlQuery existingPinsQuery;
    existingPinsQuery.prepare(
        "SELECT pin_id, pin_type FROM vector_table_pins WHERE table_id = :tableId");
    existingPinsQuery.bindValue(":tableId", m_tableId);

    if (existingPinsQuery.exec())
    {
        qDebug() << "VectorPinSettingsDialog::loadPinsData - 从vector_table_pins表加载已存在的管脚";
        while (existingPinsQuery.next())
        {
            int pinId = existingPinsQuery.value(0).toInt();
            int typeId = existingPinsQuery.value(1).toInt();

            // 将类型ID转换为字符串
            QString pinType = "In"; // 默认为In
            if (typeId == 1)
            {
                pinType = "In";
            }
            else if (typeId == 2)
            {
                pinType = "Out";
            }
            else if (typeId == 3)
            {
                pinType = "InOut";
            }

            m_selectedPins[pinId] = pinType;
            qDebug() << "  已关联管脚:" << pinId << "类型:" << m_selectedPins[pinId];
        }
    }
    else
    {
        qWarning() << "VectorPinSettingsDialog::loadPinsData - 无法查询已存在的管脚:" << existingPinsQuery.lastError().text();
    }

    // 检查每个管脚是否有数据
    for (auto it = m_selectedPins.begin(); it != m_selectedPins.end(); ++it)
    {
        if (hasPinData(it.key()))
        {
            m_pinsWithData.insert(it.key());
        }
    }

    // 填充表格
    m_pinsTable->setRowCount(m_allPins.size());
    int row = 0;

    for (auto it = m_allPins.begin(); it != m_allPins.end(); ++it, ++row)
    {
        int pinId = it.key();
        QString pinName = it.value();

        // 创建复选框
        QCheckBox *checkBox = new QCheckBox(pinName, this);
        m_checkBoxes[pinId] = checkBox;

        if (m_selectedPins.contains(pinId))
        {
            checkBox->setChecked(true);
        }

        // 如果管脚有数据，连接信号槽检查取消选中
        if (m_pinsWithData.contains(pinId))
        {
            connect(checkBox, &QCheckBox::stateChanged, this, &VectorPinSettingsDialog::onCheckBoxStateChanged);
        }

        // 添加复选框到表格
        m_pinsTable->setCellWidget(row, 0, checkBox);

        // 创建类型下拉框
        QComboBox *typeCombo = new QComboBox(this);
        typeCombo->addItems(QStringList() << "In"
                                          << "Out"
                                          << "InOut");
        m_typeComboBoxes[pinId] = typeCombo;

        if (m_selectedPins.contains(pinId))
        {
            typeCombo->setCurrentText(m_selectedPins[pinId]);
        }

        m_pinsTable->setCellWidget(row, 1, typeCombo);

        // 添加数据流信息
        QLabel *streamLabel = new QLabel("x1", this);
        streamLabel->setAlignment(Qt::AlignCenter);
        m_pinsTable->setCellWidget(row, 2, streamLabel);
    }
}

bool VectorPinSettingsDialog::hasPinData(int pinId)
{
    qDebug() << "VectorPinSettingsDialog::hasPinData - 检查管脚ID" << pinId << "是否有数据";

    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM vector_table_pin_values vtpv "
                  "JOIN vector_table_pins vtp ON vtpv.vector_pin_id = vtp.id "
                  "WHERE vtp.table_id = :tableId AND vtp.pin_id = :pinId");
    query.bindValue(":tableId", m_tableId);
    query.bindValue(":pinId", pinId);

    if (query.exec() && query.next())
    {
        int count = query.value(0).toInt();
        return count > 0;
    }

    qWarning() << "VectorPinSettingsDialog::hasPinData - 查询管脚数据失败:" << query.lastError().text();
    return false;
}

void VectorPinSettingsDialog::onCheckBoxStateChanged(int state)
{
    QCheckBox *checkBox = qobject_cast<QCheckBox *>(sender());
    if (!checkBox)
        return;

    // 查找对应的管脚ID
    int pinId = -1;
    for (auto it = m_checkBoxes.begin(); it != m_checkBoxes.end(); ++it)
    {
        if (it.value() == checkBox)
        {
            pinId = it.key();
            break;
        }
    }

    if (pinId != -1 && m_pinsWithData.contains(pinId) && state == Qt::Unchecked)
    {
        // 显示警告消息
        QMessageBox::StandardButton reply = QMessageBox::warning(
            this,
            "确认取消选择",
            QString("管脚 %1 已有数据，取消选择将删除所有相关数据，确定要继续吗？").arg(m_allPins[pinId]),
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::No)
        {
            // 恢复选中状态并阻断信号，避免递归
            checkBox->blockSignals(true);
            checkBox->setChecked(true);
            checkBox->blockSignals(false);
        }
    }
}

void VectorPinSettingsDialog::onAccepted()
{
    qDebug() << "VectorPinSettingsDialog::onAccepted - 确认管脚设置";

    // 首先收集用户选择的管脚和类型
    QMap<int, QString> newSelectedPins;
    for (auto it = m_checkBoxes.begin(); it != m_checkBoxes.end(); ++it)
    {
        int pinId = it.key();
        QCheckBox *checkBox = it.value();

        if (checkBox->isChecked())
        {
            QComboBox *typeCombo = m_typeComboBoxes.value(pinId);
            QString pinType = typeCombo ? typeCombo->currentText() : "In";
            newSelectedPins[pinId] = pinType;
        }
    }

    // 收集需要删除数据的管脚ID（取消选择的管脚）
    QList<int> pinsToDeleteData;
    for (int pinId : m_pinsWithData)
    {
        if (!newSelectedPins.contains(pinId))
        {
            pinsToDeleteData.append(pinId);
        }
    }

    // 开始事务
    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    try
    {
        // 删除取消选中的管脚数据
        if (!pinsToDeleteData.isEmpty())
        {
            for (int pinId : pinsToDeleteData)
            {
                QSqlQuery dataDeleteQuery;
                dataDeleteQuery.prepare(
                    "DELETE FROM vector_table_pin_values WHERE vector_pin_id IN "
                    "(SELECT id FROM vector_table_pins WHERE table_id = :tableId AND pin_id = :pinId)");
                dataDeleteQuery.bindValue(":tableId", m_tableId);
                dataDeleteQuery.bindValue(":pinId", pinId);

                if (!dataDeleteQuery.exec())
                {
                    qWarning() << "VectorPinSettingsDialog::onAccepted - 删除管脚数据失败:" << dataDeleteQuery.lastError().text();
                    throw std::runtime_error(dataDeleteQuery.lastError().text().toStdString());
                }

                // 同时删除vector_table_pins记录
                QSqlQuery pinDeleteQuery;
                pinDeleteQuery.prepare("DELETE FROM vector_table_pins WHERE table_id = :tableId AND pin_id = :pinId");
                pinDeleteQuery.bindValue(":tableId", m_tableId);
                pinDeleteQuery.bindValue(":pinId", pinId);

                if (!pinDeleteQuery.exec())
                {
                    qWarning() << "VectorPinSettingsDialog::onAccepted - 删除管脚设置失败:" << pinDeleteQuery.lastError().text();
                    throw std::runtime_error(pinDeleteQuery.lastError().text().toStdString());
                }
            }
        }

        // 插入或更新vector_table_pins记录
        for (auto it = newSelectedPins.begin(); it != newSelectedPins.end(); ++it)
        {
            int pinId = it.key();
            QString pinType = it.value();

            // 将字符串类型转换为数值ID
            int typeId = 1; // 默认为In (1)
            if (pinType == "Out")
            {
                typeId = 2;
            }
            else if (pinType == "InOut")
            {
                typeId = 3;
            }

            // 检查是否已存在记录
            QSqlQuery checkPinQuery;
            checkPinQuery.prepare("SELECT id FROM vector_table_pins WHERE table_id = :tableId AND pin_id = :pinId");
            checkPinQuery.bindValue(":tableId", m_tableId);
            checkPinQuery.bindValue(":pinId", pinId);

            if (checkPinQuery.exec() && checkPinQuery.next())
            {
                // 已存在，更新
                int pinRecordId = checkPinQuery.value(0).toInt();
                QSqlQuery updatePinQuery;
                updatePinQuery.prepare("UPDATE vector_table_pins SET pin_type = :typeId, pin_channel_count = 1 WHERE id = :id");
                updatePinQuery.bindValue(":typeId", typeId);
                updatePinQuery.bindValue(":id", pinRecordId);

                if (!updatePinQuery.exec())
                {
                    qWarning() << "VectorPinSettingsDialog::onAccepted - 更新管脚记录失败:" << updatePinQuery.lastError().text();
                    throw std::runtime_error(updatePinQuery.lastError().text().toStdString());
                }
            }
            else
            {
                // 不存在，插入
                QSqlQuery insertPinQuery;
                insertPinQuery.prepare("INSERT INTO vector_table_pins (table_id, pin_id, pin_channel_count, pin_type) VALUES (:tableId, :pinId, 1, :typeId)");
                insertPinQuery.bindValue(":tableId", m_tableId);
                insertPinQuery.bindValue(":pinId", pinId);
                insertPinQuery.bindValue(":typeId", typeId);

                if (!insertPinQuery.exec())
                {
                    qWarning() << "VectorPinSettingsDialog::onAccepted - 插入管脚记录失败:" << insertPinQuery.lastError().text();
                    throw std::runtime_error(insertPinQuery.lastError().text().toStdString());
                }
            }
        }

        // 提交事务
        db.commit();
        qDebug() << "VectorPinSettingsDialog::onAccepted - 成功保存管脚设置";
        accept();
    }
    catch (const std::exception &e)
    {
        // 出错时回滚事务
        db.rollback();
        qWarning() << "VectorPinSettingsDialog::onAccepted - 保存管脚设置失败:" << e.what();
        QMessageBox::critical(this, "错误", "无法更新管脚设置: " + QString(e.what()));
    }
}

void VectorPinSettingsDialog::onRejected()
{
    qDebug() << "VectorPinSettingsDialog::onRejected - 取消管脚设置";
    reject();
}

QMap<int, QString> VectorPinSettingsDialog::getSelectedPinsWithTypes() const
{
    QMap<int, QString> result;

    for (auto it = m_checkBoxes.begin(); it != m_checkBoxes.end(); ++it)
    {
        int pinId = it.key();
        QCheckBox *checkBox = it.value();

        if (checkBox->isChecked())
        {
            QComboBox *typeCombo = m_typeComboBoxes.value(pinId);
            QString pinType = typeCombo ? typeCombo->currentText() : "In";
            result[pinId] = pinType;
        }
    }

    return result;
}