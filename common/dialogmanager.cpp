#include "dialogmanager.h"
#include "database/databasemanager.h"
#include "database/databaseviewdialog.h"
#include "pin/pinlistdialog.h"
#include "timeset/timesetdialog.h"
#include "pin/pinvalueedit.h"
#include "vector/vectordatahandler.h"
#include "pin/pingroupdialog.h"
#include <QObject>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QFrame>
#include <QFont>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QProgressDialog>
#include <QJsonObject>
#include <QJsonDocument>

struct PinDataFromDialog
{
    int pinId;
    QString pinName;
    int typeId;
    int channelCount;
    bool isChecked;
};

DialogManager::DialogManager(QWidget *parent)
    : m_parent(parent)
{
}

bool DialogManager::showPinSelectionDialog(int tableId, const QString &tableName)
{
    // 创建管脚选择对话框
    QDialog pinDialog(m_parent);
    pinDialog.setWindowTitle("管脚选择 - " + tableName);
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

    // 从数据库加载类型选项和管脚列表
    QMap<int, QString> typeOptions;
    QMap<int, QString> localPinList;
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery query(db);

    // 加载类型选项
    if (query.exec("SELECT id, type_name FROM type_options ORDER BY id"))
    {
        while (query.next())
        {
            int id = query.value(0).toInt();
            QString typeName = query.value(1).toString();
            typeOptions[id] = typeName;
        }
    }

    // 加载管脚列表
    if (query.exec("SELECT id, pin_name FROM pin_list ORDER BY pin_name"))
    {
        while (query.next())
        {
            int id = query.value(0).toInt();
            QString pinName = query.value(1).toString();
            localPinList[id] = pinName;
        }
    }

    if (localPinList.isEmpty())
    {
        QMessageBox::warning(m_parent, "警告", "没有找到管脚，请先添加管脚");
        return false;
    }

    // 添加管脚、类型和数据流控件
    int row = 2; // 从第2行开始（表头和分隔线各占一行）
    QMap<int, QCheckBox *> pinCheckboxes;
    QMap<int, QComboBox *> pinTypeComboBoxes;
    QMap<int, QLabel *> pinDataStreamLabels;

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
    QObject::connect(okButton, &QPushButton::clicked, &pinDialog, &QDialog::accept);
    QObject::connect(cancelButton, &QPushButton::clicked, &pinDialog, &QDialog::reject);

    // 显示对话框
    if (pinDialog.exec() == QDialog::Accepted)
    {
        db.transaction();
        bool success = true;
        QSqlQuery queryHelper(db); // Use a helper query object for cleanup and column config

        // 1a. 清除该表旧的 VectorTableColumnConfiguration (确保与新选择同步)
        qDebug() << "DialogManager::showPinSelectionDialog - Preparing to delete old column config for tableId:" << tableId;
        bool prepareOk = queryHelper.prepare("DELETE FROM VectorTableColumnConfiguration WHERE master_record_id = ?");
        if (!prepareOk)
        {
            qCritical() << "DialogManager::showPinSelectionDialog - FAILED to prepare delete query for ColumnConfiguration:" << queryHelper.lastError().text();
            QMessageBox::critical(m_parent, "数据库准备错误", "准备清除旧列配置失败：" + queryHelper.lastError().text());
            db.rollback();
            return false;
        }
        queryHelper.addBindValue(tableId);
        if (!queryHelper.exec())
        {
            qCritical() << "DialogManager::showPinSelectionDialog - FAILED to execute delete query for ColumnConfiguration:" << queryHelper.lastError().text() << " (Bound values:" << queryHelper.boundValues() << ")";
            QMessageBox::critical(m_parent, "数据库错误", "清除旧列配置失败：" + queryHelper.lastError().text());
            db.rollback(); // 回滚事务
            return false;
        }
        qDebug() << "DialogManager::showPinSelectionDialog - Successfully deleted old column config.";

        // 1b. 清除该表旧的 vector_table_pins 记录 (NEW AND IMPORTANT)
        qDebug() << "DialogManager::showPinSelectionDialog - Preparing to delete old vector_table_pins for tableId:" << tableId;
        prepareOk = queryHelper.prepare("DELETE FROM vector_table_pins WHERE table_id = ?");
        if (!prepareOk)
        {
            qCritical() << "DialogManager::showPinSelectionDialog - FAILED to prepare delete query for vector_table_pins:" << queryHelper.lastError().text();
            QMessageBox::critical(m_parent, "数据库准备错误", "准备清除旧管脚关联失败：" + queryHelper.lastError().text());
            db.rollback();
            return false;
        }
        queryHelper.addBindValue(tableId);
        if (!queryHelper.exec())
        {
            qCritical() << "DialogManager::showPinSelectionDialog - FAILED to execute delete query for vector_table_pins:" << queryHelper.lastError().text() << " (Bound values:" << queryHelper.boundValues() << ")";
            QMessageBox::critical(m_parent, "数据库错误", "清除旧管脚关联失败：" + queryHelper.lastError().text());
            db.rollback();
            return false;
        }
        qDebug() << "DialogManager::showPinSelectionDialog - Successfully deleted old vector_table_pins.";

        int columnOrder = 0; // 用于确定列顺序

        // 添加标准列配置
        qDebug() << "DialogManager::showPinSelectionDialog - 开始添加标准列配置";
        // 1. 添加Label列
        queryHelper.prepare("INSERT INTO VectorTableColumnConfiguration "
                            "(master_record_id, column_name, column_order, column_type, data_properties) "
                            "VALUES (?, ?, ?, ?, ?)");
        queryHelper.addBindValue(tableId);
        queryHelper.addBindValue("Label");
        queryHelper.addBindValue(columnOrder++);
        queryHelper.addBindValue("TEXT");
        queryHelper.addBindValue("{}");

        if (!queryHelper.exec())
        {
            QMessageBox::critical(m_parent, "数据库错误", "添加Label列失败：" + queryHelper.lastError().text());
            success = false;
            db.rollback();
            return false;
        }

        // 2. 添加Instruction列
        queryHelper.prepare("INSERT INTO VectorTableColumnConfiguration "
                            "(master_record_id, column_name, column_order, column_type, data_properties) "
                            "VALUES (?, ?, ?, ?, ?)");
        queryHelper.addBindValue(tableId);
        queryHelper.addBindValue("Instruction");
        queryHelper.addBindValue(columnOrder++);
        queryHelper.addBindValue("INSTRUCTION_ID");
        queryHelper.addBindValue("{}");

        if (!queryHelper.exec())
        {
            QMessageBox::critical(m_parent, "数据库错误", "添加Instruction列失败：" + queryHelper.lastError().text());
            success = false;
            db.rollback();
            return false;
        }

        // 3. 添加TimeSet列
        queryHelper.prepare("INSERT INTO VectorTableColumnConfiguration "
                            "(master_record_id, column_name, column_order, column_type, data_properties) "
                            "VALUES (?, ?, ?, ?, ?)");
        queryHelper.addBindValue(tableId);
        queryHelper.addBindValue("TimeSet");
        queryHelper.addBindValue(columnOrder++);
        queryHelper.addBindValue("TIMESET_ID");
        queryHelper.addBindValue("{}");

        if (!queryHelper.exec())
        {
            QMessageBox::critical(m_parent, "数据库错误", "添加TimeSet列失败：" + queryHelper.lastError().text());
            success = false;
            db.rollback();
            return false;
        }

        // 4. 添加Capture列
        queryHelper.prepare("INSERT INTO VectorTableColumnConfiguration "
                            "(master_record_id, column_name, column_order, column_type, data_properties) "
                            "VALUES (?, ?, ?, ?, ?)");
        queryHelper.addBindValue(tableId);
        queryHelper.addBindValue("Capture");
        queryHelper.addBindValue(columnOrder++);
        queryHelper.addBindValue("TEXT");
        queryHelper.addBindValue("{}");

        if (!queryHelper.exec())
        {
            QMessageBox::critical(m_parent, "数据库错误", "添加Capture列失败：" + queryHelper.lastError().text());
            success = false;
            db.rollback();
            return false;
        }

        // 5. 添加Ext列
        queryHelper.prepare("INSERT INTO VectorTableColumnConfiguration "
                            "(master_record_id, column_name, column_order, column_type, data_properties) "
                            "VALUES (?, ?, ?, ?, ?)");
        queryHelper.addBindValue(tableId);
        queryHelper.addBindValue("Ext");
        queryHelper.addBindValue(columnOrder++);
        queryHelper.addBindValue("TEXT");
        queryHelper.addBindValue("{}");

        if (!queryHelper.exec())
        {
            QMessageBox::critical(m_parent, "数据库错误", "添加Ext列失败：" + queryHelper.lastError().text());
            success = false;
            db.rollback();
            return false;
        }

        // 6. 添加Comment列
        queryHelper.prepare("INSERT INTO VectorTableColumnConfiguration "
                            "(master_record_id, column_name, column_order, column_type, data_properties) "
                            "VALUES (?, ?, ?, ?, ?)");
        queryHelper.addBindValue(tableId);
        queryHelper.addBindValue("Comment");
        queryHelper.addBindValue(columnOrder++);
        queryHelper.addBindValue("TEXT");
        queryHelper.addBindValue("{}");

        if (!queryHelper.exec())
        {
            QMessageBox::critical(m_parent, "数据库错误", "添加Comment列失败：" + queryHelper.lastError().text());
            success = false;
            db.rollback();
            return false;
        }

        qDebug() << "DialogManager::showPinSelectionDialog - 已成功添加" << columnOrder << "个标准列配置";

        // 收集所有管脚的配置信息
        QList<PinDataFromDialog> allPinDataFromDialog;
        for (auto it_lp = localPinList.begin(); it_lp != localPinList.end(); ++it_lp)
        {
            int pinId = it_lp.key();
            QString pinName = it_lp.value();
            QCheckBox *cb = pinCheckboxes.value(pinId);
            QComboBox *combo = pinTypeComboBoxes.value(pinId);

            PinDataFromDialog pData;
            pData.pinId = pinId;
            pData.pinName = pinName;
            pData.typeId = combo ? combo->currentData().toInt() : 1; // Default to type_id 1 if combo is null
            pData.channelCount = 1;                                  // From "x1"
            pData.isChecked = cb ? cb->isChecked() : false;
            allPinDataFromDialog.append(pData);
        }

        // 按照管脚名称排序（如果需要确保列顺序基于管脚名称）
        // std::sort(allPinDataFromDialog.begin(), allPinDataFromDialog.end(), [](const PinDataFromDialog& a, const PinDataFromDialog& b){ return a.pinName < b.pinName; });

        for (const auto &pData : allPinDataFromDialog)
        {
            qDebug() << "DialogManager::showPinSelectionDialog - 正在处理管脚: " << pData.pinName
                     << ", ID:" << pData.pinId
                     << ", 勾选状态:" << (pData.isChecked ? "已选择" : "未选择");

            if (!success)
                break;

            // 只处理被勾选的管脚
            if (pData.isChecked)
            {
                qDebug() << "DialogManager::showPinSelectionDialog - 为选中的管脚添加配置: " << pData.pinName;

                // 2. 插入到 vector_table_pins
                queryHelper.prepare("INSERT INTO vector_table_pins (table_id, pin_id, pin_channel_count, pin_type) "
                                    "VALUES (?, ?, ?, ?)");
                queryHelper.addBindValue(tableId);
                queryHelper.addBindValue(pData.pinId);
                queryHelper.addBindValue(pData.channelCount);
                queryHelper.addBindValue(pData.typeId);
                if (!queryHelper.exec())
                {
                    QMessageBox::critical(m_parent, "数据库错误", QString("向 vector_table_pins 添加管脚 %1 失败：").arg(pData.pinName) + queryHelper.lastError().text());
                    success = false;
                    continue;
                }
                qDebug() << "DialogManager::showPinSelectionDialog - 成功添加管脚到 vector_table_pins: " << pData.pinName;

                // 3. 只为勾选的管脚插入到 VectorTableColumnConfiguration
                QJsonObject properties;
                properties["pin_list_id"] = pData.pinId;
                properties["channel_count"] = pData.channelCount;
                properties["type_id"] = pData.typeId;
                QJsonDocument doc(properties);
                QString propertiesJson = doc.toJson(QJsonDocument::Compact);

                queryHelper.prepare("INSERT INTO VectorTableColumnConfiguration "
                                    "(master_record_id, column_name, column_order, column_type, data_properties) "
                                    "VALUES (?, ?, ?, ?, ?)");
                queryHelper.addBindValue(tableId);
                queryHelper.addBindValue(pData.pinName);
                queryHelper.addBindValue(columnOrder++);
                queryHelper.addBindValue("PIN_STATE_ID");
                queryHelper.addBindValue(propertiesJson);

                if (!queryHelper.exec())
                {
                    QMessageBox::critical(m_parent, "数据库错误", QString("为管脚 %1 保存列配置失败： ").arg(pData.pinName) + queryHelper.lastError().text());
                    success = false;
                    // continue; // Allow other column configs to be attempted, but transaction will fail
                }
                else
                {
                    qDebug() << "DialogManager::showPinSelectionDialog - 成功为管脚添加列配置:" << pData.pinName << "Props:" << propertiesJson;
                }
            }
            else
            {
                qDebug() << "DialogManager::showPinSelectionDialog - 跳过未选中的管脚: " << pData.pinName;
            }
        }

        if (!success)
        {
            db.rollback();
            return false;
        }

        // 4. 更新主记录中的 column_count
        queryHelper.prepare("UPDATE VectorTableMasterRecord SET column_count = ? WHERE id = ?");
        queryHelper.addBindValue(columnOrder);
        queryHelper.addBindValue(tableId);
        if (!queryHelper.exec())
        {
            qWarning() << "DialogManager::showPinSelectionDialog - 更新主记录 column_count 失败: " << queryHelper.lastError().text();
            // This might not be a critical failure to warrant a full rollback if pins/cols were set,
            // but for consistency it's better to ensure it succeeds.
            // For now, let's consider it non-critical if previous steps succeeded.
            // success = false; // Uncomment if this should cause a rollback
        }

        if (success)
        {
            db.commit();
            // QMessageBox::information(m_parent, "保存成功", "管脚信息已成功保存！");
            showVectorDataDialog(tableId, tableName, 0);
            return true;
        }
        else
        {
            db.rollback();
            return false;
        }
    }
    else
    {
        // 用户取消，删除已创建的向量表及相关记录
        qDebug() << "DialogManager::showPinSelectionDialog - User cancelled. Cleaning up tableId:" << tableId;
        QSqlDatabase db = DatabaseManager::instance()->database(); // Get a fresh instance or ensure it's the same
        db.transaction();
        bool cleanupSuccess = true;

        QSqlQuery deleteQuery(db);

        deleteQuery.prepare("DELETE FROM VectorTableColumnConfiguration WHERE master_record_id = ?");
        deleteQuery.addBindValue(tableId);
        if (!deleteQuery.exec())
        {
            qWarning() << "DialogManager::showPinSelectionDialog (Cancel) - Failed to delete VectorTableColumnConfiguration:" << deleteQuery.lastError().text();
            cleanupSuccess = false;
        }

        deleteQuery.prepare("DELETE FROM vector_table_pins WHERE table_id = ?");
        deleteQuery.addBindValue(tableId);
        if (!deleteQuery.exec())
        {
            qWarning() << "DialogManager::showPinSelectionDialog (Cancel) - Failed to delete vector_table_pins:" << deleteQuery.lastError().text();
            cleanupSuccess = false;
        }

        // Assuming original_vector_table_id in VectorTableMasterRecord is the same as tableId (newTableId from MainWindow::addNewVectorTable)
        deleteQuery.prepare("DELETE FROM VectorTableMasterRecord WHERE original_vector_table_id = ?");
        deleteQuery.addBindValue(tableId);
        if (!deleteQuery.exec())
        {
            qWarning() << "DialogManager::showPinSelectionDialog (Cancel) - Failed to delete VectorTableMasterRecord:" << deleteQuery.lastError().text();
            cleanupSuccess = false;
        }

        deleteQuery.prepare("DELETE FROM vector_tables WHERE id = ?");
        deleteQuery.addBindValue(tableId);
        if (!deleteQuery.exec())
        {
            qWarning() << "DialogManager::showPinSelectionDialog (Cancel) - Failed to delete vector_tables record:" << tableId << deleteQuery.lastError().text();
            cleanupSuccess = false;
        }

        if (cleanupSuccess)
        {
            db.commit();
            qDebug() << "DialogManager::showPinSelectionDialog (Cancel) - Cleanup successful for tableId:" << tableId;
        }
        else
        {
            db.rollback();
            qWarning() << "DialogManager::showPinSelectionDialog (Cancel) - Cleanup failed for tableId:" << tableId << ", transaction rolled back.";
        }
        return false;
    }
}

#include "dialogmanager_1.cpp"

#include "dialogmanager_DataDialog.cpp"
