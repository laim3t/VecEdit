#include "vectorpinsettingsdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QSqlDatabase>
#include "tablestylemanager.h"
#include <QJsonObject>
#include <QJsonDocument>
#include "../database/databasemanager.h"
#include "../vector/vectordatahandler.h"
#include "../vector/robustvectordatahandler.h"
#include "../app/mainwindow.h"

VectorPinSettingsDialog::VectorPinSettingsDialog(int tableId, const QString &tableName, QWidget *parent)
    : QDialog(parent), m_tableId(tableId), m_tableName(tableName)
{
    qDebug() << "VectorPinSettingsDialog::VectorPinSettingsDialog - 初始化管脚设置对话框，表ID:" << tableId << "，表名:" << tableName;

    // 检查是否使用新轨道数据处理器
    MainWindow *mainWindow = qobject_cast<MainWindow *>(parent);
    if (mainWindow)
    {
        m_useNewDataHandler = mainWindow->m_useNewDataHandler;
        m_robustDataHandler = mainWindow->m_robustDataHandler;
    }
    else
    {
        m_useNewDataHandler = false;
        m_robustDataHandler = nullptr;
    }

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

    // 应用表格样式
    TableStyleManager::applyTableStyle(m_pinsTable);

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

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << "VectorPinSettingsDialog::loadPinsData - 数据库未连接";
        return;
    }

    // 加载所有管脚
    QSqlQuery pinQuery(db);
    if (pinQuery.exec("SELECT id, pin_name FROM pin_list ORDER BY pin_name"))
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

    // 获取可见列的管脚名称集合
    QSet<QString> visibleColumns;

    if (m_useNewDataHandler && m_robustDataHandler)
    {
        // 使用新轨道数据处理器获取列信息
        qDebug() << "VectorPinSettingsDialog::loadPinsData - 使用新轨道(RobustVectorDataHandler)获取管脚列信息";
        QList<Vector::ColumnInfo> columns = m_robustDataHandler->getAllColumnInfo(m_tableId);

        // 获取所有可见的管脚列
        for (const auto &col : columns)
        {
            if (col.is_visible && col.type == Vector::ColumnDataType::PIN_STATE_ID)
            {
                visibleColumns.insert(col.name);
                qDebug() << "  新轨道可见管脚列:" << col.name;
            }
        }
    }
    else
    {
        // 使用旧轨道查询数据库
        qDebug() << "VectorPinSettingsDialog::loadPinsData - 使用旧轨道从VectorTableColumnConfiguration表加载可见管脚列";
        QSqlQuery visibleColumnsQuery(db);
        visibleColumnsQuery.prepare(
            "SELECT column_name FROM VectorTableColumnConfiguration WHERE master_record_id = :tableId AND IsVisible = 1 AND column_type = 'PIN_STATE_ID'");
        visibleColumnsQuery.bindValue(":tableId", m_tableId);

        if (visibleColumnsQuery.exec())
        {
            while (visibleColumnsQuery.next())
            {
                QString columnName = visibleColumnsQuery.value(0).toString();
                visibleColumns.insert(columnName);
                qDebug() << "  可见管脚列:" << columnName;
            }
        }
        else
        {
            qWarning() << "VectorPinSettingsDialog::loadPinsData - 无法查询可见列:" << visibleColumnsQuery.lastError().text();
        }
    }

    // 从vector_table_pins表中加载已经关联到该向量表的管脚
    QSqlQuery existingPinsQuery(db);
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

            // 检查该管脚的列是否可见
            if (m_allPins.contains(pinId) && visibleColumns.contains(m_allPins[pinId]))
            {
                m_selectedPins[pinId] = pinType;
                qDebug() << "  已关联且可见的管脚:" << pinId << "名称:" << m_allPins[pinId] << "类型:" << m_selectedPins[pinId];
            }
            else if (m_allPins.contains(pinId))
            {
                qDebug() << "  管脚" << pinId << "(" << m_allPins[pinId] << ")已关联但不可见";
            }
            else
            {
                qDebug() << "  管脚ID" << pinId << "存在但未找到对应名称";
            }
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
            qDebug() << "VectorPinSettingsDialog::loadPinsData - 管脚ID" << it.key() << "(" << m_allPins[it.key()] << ")有数据";
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

        // 连接信号槽，对所有管脚添加状态改变检测
        connect(checkBox, &QCheckBox::stateChanged, this, &VectorPinSettingsDialog::onCheckBoxStateChanged);

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

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << "VectorPinSettingsDialog::hasPinData - 数据库未连接";
        return false;
    }

    QSqlQuery query(db);
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

    if (pinId != -1)
    {
        QString pinName = m_allPins[pinId];

        // 处理取消勾选
        if (state == Qt::Unchecked)
        {
            // 如果管脚有数据，需要提示用户
            if (m_pinsWithData.contains(pinId))
            {
                // 显示警告消息
                QMessageBox::StandardButton reply = QMessageBox::warning(
                    this,
                    "确认取消选择",
                    QString("管脚 %1 已有数据，取消选择将删除所有相关数据，确定要继续吗？").arg(pinName),
                    QMessageBox::Yes | QMessageBox::No);

                if (reply == QMessageBox::No)
                {
                    // 恢复选中状态并阻断信号，避免递归
                    checkBox->blockSignals(true);
                    checkBox->setChecked(true);
                    checkBox->blockSignals(false);
                    return; // 用户取消操作，直接返回
                }
            }

            // 用户确认取消勾选或管脚无数据，立即将该管脚列标记为不可见
            QString errorMsg;

            qDebug() << "VectorPinSettingsDialog::onCheckBoxStateChanged - 用户取消选择管脚:" << pinName << "，立即隐藏对应列";

            bool hideSuccess = false;

            if (m_useNewDataHandler && m_robustDataHandler)
            {
                // 使用新轨道数据处理器隐藏列
                hideSuccess = m_robustDataHandler->hideVectorTableColumn(m_tableId, pinName, errorMsg);
                qDebug() << "VectorPinSettingsDialog::onCheckBoxStateChanged - 使用新轨道处理器隐藏管脚列:" << pinName;
            }
            else
            {
                // 使用旧轨道处理器隐藏列
                hideSuccess = VectorDataHandler::instance().hideVectorTableColumn(m_tableId, pinName, errorMsg);
                qDebug() << "VectorPinSettingsDialog::onCheckBoxStateChanged - 使用旧轨道处理器隐藏管脚列:" << pinName;
            }

            if (!hideSuccess)
            {
                qWarning() << "VectorPinSettingsDialog::onCheckBoxStateChanged - 隐藏管脚列失败:" << errorMsg;
                // 显示错误消息
                QMessageBox::warning(this, "操作失败", QString("无法隐藏管脚列 %1: %2").arg(pinName).arg(errorMsg));
            }
            else
            {
                qDebug() << "VectorPinSettingsDialog::onCheckBoxStateChanged - 成功隐藏管脚列:" << pinName;
            }
        }
        else if (state == Qt::Checked)
        {
            // 用户勾选管脚，需要在onAccepted中重新显示列
            qDebug() << "VectorPinSettingsDialog::onCheckBoxStateChanged - 用户选择管脚:" << pinName << "，需要在onAccepted中重新显示";

            // 检查是否已存在列配置但处于隐藏状态
            QSqlDatabase db = DatabaseManager::instance()->database();
            if (db.isOpen())
            {
                QSqlQuery checkQuery(db);
                checkQuery.prepare("SELECT id, IsVisible FROM VectorTableColumnConfiguration WHERE master_record_id = :tableId AND column_name = :pinName");
                checkQuery.bindValue(":tableId", m_tableId);
                checkQuery.bindValue(":pinName", pinName);

                if (checkQuery.exec() && checkQuery.next())
                {
                    bool isVisible = checkQuery.value(1).toBool();
                    if (!isVisible)
                    {
                        // 列存在但处于隐藏状态，立即恢复
                        QString errorMsg;
                        bool showSuccess = false;

                        if (m_useNewDataHandler && m_robustDataHandler)
                        {
                            // 使用新轨道数据处理器显示列
                            showSuccess = m_robustDataHandler->showVectorTableColumn(m_tableId, pinName, errorMsg);
                            qDebug() << "VectorPinSettingsDialog::onCheckBoxStateChanged - 使用新轨道处理器显示管脚列:" << pinName;
                        }
                        else
                        {
                            // 使用旧轨道处理器显示列
                            showSuccess = VectorDataHandler::instance().showVectorTableColumn(m_tableId, pinName, errorMsg);
                            qDebug() << "VectorPinSettingsDialog::onCheckBoxStateChanged - 使用旧轨道处理器显示管脚列:" << pinName;
                        }

                        if (!showSuccess)
                        {
                            qWarning() << "VectorPinSettingsDialog::onCheckBoxStateChanged - 恢复列可见性失败:" << errorMsg;
                        }
                        else
                        {
                            qDebug() << "VectorPinSettingsDialog::onCheckBoxStateChanged - 成功将列 '" << pinName << "' 恢复为可见状态";
                        }
                    }
                }
            }
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
    // 找出所有之前选中但现在取消选择的管脚
    for (auto it = m_selectedPins.begin(); it != m_selectedPins.end(); ++it)
    {
        int pinId = it.key();
        if (!newSelectedPins.contains(pinId))
        {
            pinsToDeleteData.append(pinId);
            qDebug() << "VectorPinSettingsDialog::onAccepted - 管脚ID:" << pinId << "(" << m_allPins[pinId] << ")被取消选择";
        }
    }

    // 开始事务
    QSqlDatabase db = DatabaseManager::instance()->database();
    db.transaction();

    try
    {
        // 删除取消选中的管脚数据和记录
        if (!pinsToDeleteData.isEmpty())
        {
            for (int pinId : pinsToDeleteData)
            {
                QString pinName = m_allPins[pinId];

                // 1. 删除管脚数据（vector_table_pin_values表中的数据）
                QSqlQuery dataDeleteQuery(db);
                dataDeleteQuery.prepare(
                    "DELETE vtpv FROM vector_table_pin_values vtpv "
                    "JOIN vector_table_pins vtp ON vtpv.vector_pin_id = vtp.id "
                    "WHERE vtp.table_id = :tableId AND vtp.pin_id = :pinId");
                dataDeleteQuery.bindValue(":tableId", m_tableId);
                dataDeleteQuery.bindValue(":pinId", pinId);

                if (!dataDeleteQuery.exec())
                {
                    QString errorDetails = QString("Query: %1\nBound Values: tableId=%2, pinId=%3")
                                               .arg(dataDeleteQuery.lastQuery())
                                               .arg(m_tableId)
                                               .arg(pinId);
                    throw std::runtime_error(QString("删除管脚 '%1' 数据失败: %2\nDetails:\n%3")
                                                 .arg(pinName)
                                                 .arg(dataDeleteQuery.lastError().text())
                                                 .arg(errorDetails)
                                                 .toStdString());
                }

                // 2. 删除管脚记录（vector_table_pins表中的记录）
                QSqlQuery pinDeleteQuery(db);
                pinDeleteQuery.prepare(
                    "DELETE FROM vector_table_pins "
                    "WHERE table_id = :tableId AND pin_id = :pinId");
                pinDeleteQuery.bindValue(":tableId", m_tableId);
                pinDeleteQuery.bindValue(":pinId", pinId);

                if (!pinDeleteQuery.exec())
                {
                    QString errorDetails = QString("Query: %1\nBound Values: tableId=%2, pinId=%3")
                                               .arg(pinDeleteQuery.lastQuery())
                                               .arg(m_tableId)
                                               .arg(pinId);
                    throw std::runtime_error(QString("删除管脚 '%1' 记录失败: %2\nDetails:\n%3")
                                                 .arg(pinName)
                                                 .arg(pinDeleteQuery.lastError().text())
                                                 .arg(errorDetails)
                                                 .toStdString());
                }

                qDebug() << "VectorPinSettingsDialog::onAccepted - 成功删除管脚" << pinName << "(ID:" << pinId << ")的数据和记录";
            }
        }

        // 处理所有勾选的管脚
        for (auto it = newSelectedPins.begin(); it != newSelectedPins.end(); ++it)
        {
            int pinId = it.key();
            QString typeName = it.value();
            QString pinName = m_allPins[pinId];

            // 获取类型ID
            int typeId = 1; // 默认为In (1)
            if (typeName == "Out")
            {
                typeId = 2;
            }
            else if (typeName == "InOut")
            {
                typeId = 3;
            }

            // 检查此管脚是否已经存在于vector_table_pins表中
            QSqlQuery checkPinQuery(db);
            checkPinQuery.prepare("SELECT id, pin_type FROM vector_table_pins WHERE table_id = :tableId AND pin_id = :pinId");
            checkPinQuery.bindValue(":tableId", m_tableId);
            checkPinQuery.bindValue(":pinId", pinId);

            if (checkPinQuery.exec() && checkPinQuery.next())
            {
                // 管脚已存在，更新类型（如果需要）
                int existingPinId = checkPinQuery.value(0).toInt();
                int existingTypeId = checkPinQuery.value(1).toInt();

                if (existingTypeId != typeId)
                {
                    QSqlQuery updatePinQuery(db);
                    updatePinQuery.prepare("UPDATE vector_table_pins SET pin_type = :typeId WHERE id = :id");
                    updatePinQuery.bindValue(":typeId", typeId);
                    updatePinQuery.bindValue(":id", existingPinId);

                    if (!updatePinQuery.exec())
                    {
                        QString errorDetails = QString("Query: %1\nBound Values: typeId=%2, id=%3")
                                                   .arg(updatePinQuery.lastQuery())
                                                   .arg(typeId)
                                                   .arg(existingPinId);
                        throw std::runtime_error(QString("更新管脚 '%1' 类型失败: %2\nDetails:\n%3")
                                                     .arg(pinName)
                                                     .arg(updatePinQuery.lastError().text())
                                                     .arg(errorDetails)
                                                     .toStdString());
                    }

                    qDebug() << "VectorPinSettingsDialog::onAccepted - 更新管脚" << pinName << "(ID:" << pinId << ")的类型为" << typeName << "(" << typeId << ")";
                }
            }
            else
            {
                // 管脚不存在，插入新记录
                QSqlQuery insertPinQuery(db);
                insertPinQuery.prepare(
                    "INSERT INTO vector_table_pins (table_id, pin_id, pin_channel_count, pin_type) "
                    "VALUES (:tableId, :pinId, 1, :typeId)");
                insertPinQuery.bindValue(":tableId", m_tableId);
                insertPinQuery.bindValue(":pinId", pinId);
                insertPinQuery.bindValue(":typeId", typeId);

                if (!insertPinQuery.exec())
                {
                    QString errorDetails = QString("Query: %1\nBound Values: tableId=%2, pinId=%3, typeId=%4")
                                               .arg(insertPinQuery.lastQuery())
                                               .arg(m_tableId)
                                               .arg(pinId)
                                               .arg(typeId);
                    throw std::runtime_error(QString("添加管脚 '%1' 失败: %2\nDetails:\n%3")
                                                 .arg(pinName)
                                                 .arg(insertPinQuery.lastError().text())
                                                 .arg(errorDetails)
                                                 .toStdString());
                }

                qDebug() << "VectorPinSettingsDialog::onAccepted - 添加管脚" << pinName << "(ID:" << pinId << ")，类型为" << typeName << "(" << typeId << ")";
            }

            // 检查该管脚的列是否已经存在于VectorTableColumnConfiguration表中
            QSqlQuery checkColConfigQuery(db);
            checkColConfigQuery.prepare(
                "SELECT id, IsVisible FROM VectorTableColumnConfiguration "
                "WHERE master_record_id = :tableId AND column_name = :columnName");
            checkColConfigQuery.bindValue(":tableId", m_tableId);
            checkColConfigQuery.bindValue(":columnName", pinName);

            // 准备列属性（JSON格式）
            QJsonObject propObj;
            propObj["pin_id"] = pinId;
            propObj["type_id"] = typeId;
            propObj["channel_count"] = 1;
            QJsonDocument propDoc(propObj);
            QString propStr = propDoc.toJson(QJsonDocument::Compact);

            if (checkColConfigQuery.exec() && checkColConfigQuery.next())
            {
                // 列配置已存在，更新可见性和属性（如果需要）
                int colConfigId = checkColConfigQuery.value(0).toInt();
                bool isVisible = checkColConfigQuery.value(1).toBool();

                if (!isVisible)
                {
                    // 如果不可见，则更新为可见
                    QSqlQuery updateColConfigQuery(db);
                    updateColConfigQuery.prepare(
                        "UPDATE VectorTableColumnConfiguration "
                        "SET IsVisible = 1, data_properties = :props "
                        "WHERE id = :id");
                    updateColConfigQuery.bindValue(":props", propStr);
                    updateColConfigQuery.bindValue(":id", colConfigId);

                    if (!updateColConfigQuery.exec())
                    {
                        QString errorDetails = QString("Query: %1\nBound Values: props=%2, id=%3")
                                                   .arg(updateColConfigQuery.lastQuery())
                                                   .arg(propStr)
                                                   .arg(colConfigId);
                        throw std::runtime_error(QString("更新管脚列 '%1' 可见性失败: %2\nDetails:\n%3")
                                                     .arg(pinName)
                                                     .arg(updateColConfigQuery.lastError().text())
                                                     .arg(errorDetails)
                                                     .toStdString());
                    }

                    qDebug() << "VectorPinSettingsDialog::onAccepted - 将管脚列" << pinName << "设置为可见";
                }
            }
            else
            {
                // 列配置不存在，插入新记录
                // 首先获取最大的显示顺序值
                int maxDisplayOrder = 0;
                QSqlQuery maxOrderQuery(db);
                maxOrderQuery.prepare(
                    "SELECT MAX(column_order) FROM VectorTableColumnConfiguration WHERE master_record_id = :tableId");
                maxOrderQuery.bindValue(":tableId", m_tableId);

                if (maxOrderQuery.exec() && maxOrderQuery.next())
                {
                    maxDisplayOrder = maxOrderQuery.value(0).toInt();
                }

                // 然后插入新的列配置
                QSqlQuery insertColConfigQuery(db);
                insertColConfigQuery.prepare(
                    "INSERT INTO VectorTableColumnConfiguration "
                    "(master_record_id, column_name, column_type, column_order, IsVisible, data_properties) "
                    "VALUES (:tableId, :columnName, 'PIN_STATE_ID', :order, 1, :props)");
                insertColConfigQuery.bindValue(":tableId", m_tableId);
                insertColConfigQuery.bindValue(":columnName", pinName);
                insertColConfigQuery.bindValue(":order", maxDisplayOrder + 1);
                insertColConfigQuery.bindValue(":props", propStr);

                if (!insertColConfigQuery.exec())
                {
                    QString errorDetails = QString("Query: %1\nBound Values: tableId=%2, columnName=%3, order=%4, props=%5")
                                               .arg(insertColConfigQuery.lastQuery())
                                               .arg(m_tableId)
                                               .arg(pinName)
                                               .arg(maxDisplayOrder + 1)
                                               .arg(propStr);
                    throw std::runtime_error(QString("添加管脚列 '%1' 配置失败: %2\nDetails:\n%3")
                                                 .arg(pinName)
                                                 .arg(insertColConfigQuery.lastError().text())
                                                 .arg(errorDetails)
                                                 .toStdString());
                }

                qDebug() << "VectorPinSettingsDialog::onAccepted - 添加管脚列" << pinName << "配置";
            }

            // 对于新轨道，确保管脚列在数据处理器中可见
            if (m_useNewDataHandler && m_robustDataHandler)
            {
                QString errorMsg;
                if (!m_robustDataHandler->showVectorTableColumn(m_tableId, pinName, errorMsg))
                {
                    qWarning() << "VectorPinSettingsDialog::onAccepted - 使用新轨道处理器显示管脚列失败:" << errorMsg;
                }
                else
                {
                    qDebug() << "VectorPinSettingsDialog::onAccepted - 使用新轨道处理器成功显示管脚列:" << pinName;
                }
            }
        }

        // 提交事务
        db.commit();

        // 输出调试信息
        logColumnConfigInfo(m_tableId);

        // 成功处理完所有操作，关闭对话框
        accept();
    }
    catch (const std::exception &e)
    {
        // 发生错误，回滚事务
        db.rollback();

        // 显示错误消息
        QMessageBox::critical(this, "错误", QString("保存管脚设置时发生错误: %1").arg(e.what()));

        // 不关闭对话框，让用户可以继续编辑
        qWarning() << "VectorPinSettingsDialog::onAccepted - 保存管脚设置时发生错误:" << e.what();
    }
}

void VectorPinSettingsDialog::onRejected()
{
    qDebug() << "VectorPinSettingsDialog::onRejected - 取消管脚设置";
    reject();
}

void VectorPinSettingsDialog::logColumnConfigInfo(int tableId)
{
    // 获取并输出当前表的所有列配置信息，用于调试
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << "VectorPinSettingsDialog::logColumnConfigInfo - 数据库未连接";
        return;
    }

    QSqlQuery query(db);
    query.prepare("SELECT id, column_name, column_order, column_type, data_properties FROM VectorTableColumnConfiguration WHERE master_record_id = ? ORDER BY column_order");
    query.addBindValue(tableId);

    if (query.exec())
    {
        qDebug() << "VectorPinSettingsDialog::logColumnConfigInfo - 表" << tableId << "的列配置情况:";
        int count = 0;
        while (query.next())
        {
            int id = query.value(0).toInt();
            QString name = query.value(1).toString();
            int order = query.value(2).toInt();
            QString type = query.value(3).toString();
            QString props = query.value(4).toString();

            qDebug() << "  列" << count++ << ": ID=" << id << ", 名称=" << name
                     << ", 顺序=" << order << ", 类型=" << type
                     << ", 属性=" << props;
        }
        qDebug() << "  总计:" << count << "列";
    }
    else
    {
        qWarning() << "VectorPinSettingsDialog::logColumnConfigInfo - 查询列配置失败:" << query.lastError().text();
    }
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