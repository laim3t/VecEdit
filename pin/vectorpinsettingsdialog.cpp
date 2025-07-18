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
    // 新逻辑下，此函数内容被清空。所有数据库操作推迟到 onAccepted 函数中处理。
    // 我们保留这个函数以防未来需要添加实时UI反馈，但它不再直接修改数据库。
    Q_UNUSED(state);
}

void VectorPinSettingsDialog::onAccepted()
{
    qDebug() << "VectorPinSettingsDialog::onAccepted - 用户点击确定，开始更新管脚可见性";
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << "VectorPinSettingsDialog::onAccepted - 数据库未连接";
        QMessageBox::critical(this, "错误", "数据库未连接，无法保存更改。");
        return;
    }

    db.transaction();

    bool success = true;
    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE VectorTableColumnConfiguration SET is_visible = ? WHERE master_record_id = ? AND column_name = ?");

    for (auto it = m_checkBoxes.begin(); it != m_checkBoxes.end(); ++it)
    {
        int pinId = it.key();
        QCheckBox *checkBox = it.value();

        // 从 m_allPins 中获取管脚名称
        QString pinName = m_allPins.value(pinId);
        if (pinName.isEmpty())
        {
            qWarning() << "VectorPinSettingsDialog::onAccepted - 无法找到Pin ID:" << pinId << "对应的名称，跳过。";
            continue;
        }

        bool isVisible = checkBox->isChecked();

        updateQuery.bindValue(0, isVisible ? 1 : 0);
        updateQuery.bindValue(1, m_tableId);
        updateQuery.bindValue(2, pinName);

        if (!updateQuery.exec())
        {
            qWarning() << "VectorPinSettingsDialog::onAccepted - 更新管脚可见性失败:" << pinName << "错误:" << updateQuery.lastError().text();
            success = false;
            break;
        }
    }

    if (success)
    {
        if (!db.commit())
        {
            qWarning() << "VectorPinSettingsDialog::onAccepted - 提交事务失败:" << db.lastError().text();
            db.rollback();
            QMessageBox::critical(this, "数据库错误", "无法提交更改到数据库。");
        }
        else
        {
            qInfo() << "VectorPinSettingsDialog::onAccepted - 成功更新管脚可见性。";
            accept();
        }
    }
    else
    {
        db.rollback();
        QMessageBox::critical(this, "数据库错误", "更新一个或多个管脚的可见性时出错，所有更改已回滚。");
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