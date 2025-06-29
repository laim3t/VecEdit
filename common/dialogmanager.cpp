#include "dialogmanager.h"
#include "database/databasemanager.h"
#include "database/databaseviewdialog.h"
#include "pin/pinlistdialog.h"
#include "timeset/timesetdialog.h"
#include "pin/pinvalueedit.h"
#include "vector/vectordatahandler.h"
#include "vector/robustvectordatahandler.h"
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

DialogManager::DialogManager(QWidget *parent, bool useNewDataHandler, RobustVectorDataHandler *robustDataHandler)
    : m_parent(parent),
      m_useNewDataHandler(useNewDataHandler),
      m_robustDataHandler(robustDataHandler)
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

        // Step 1: Collect selected pin data from the dialog
        QList<PinDataFromDialog> selectedPins;
        for (auto it = pinCheckboxes.begin(); it != pinCheckboxes.end(); ++it)
        {
            if (it.value()->isChecked())
            {
                int pinId = it.key();
                PinDataFromDialog data;
                data.pinId = pinId;
                data.pinName = it.value()->text();
                data.typeId = pinTypeComboBoxes[pinId]->currentData().toInt();
                data.channelCount = pinDataStreamLabels[pinId]->text().remove('x').toInt();
                data.isChecked = true;
                selectedPins.append(data);
            }
        }

        // Step 2: Clear old pin associations for this table
        QSqlQuery queryHelper(db);
        qDebug() << "DialogManager::showPinSelectionDialog - Deleting old pin associations (vector_table_pins) for tableId:" << tableId;
        queryHelper.prepare("DELETE FROM vector_table_pins WHERE table_id = ?");
        queryHelper.addBindValue(tableId);
        if (!queryHelper.exec()) {
            qCritical() << "DialogManager::showPinSelectionDialog - FAILED to delete from vector_table_pins:" << queryHelper.lastError().text();
            db.rollback();
            return false;
        }

        // Step 3: Insert new pin associations based on dialog selection
        queryHelper.prepare("INSERT INTO vector_table_pins (table_id, pin_id, pin_type, pin_channel_count) VALUES (?, ?, ?, ?)");
        for (const auto& pin : selectedPins)
        {
            queryHelper.bindValue(0, tableId);
            queryHelper.bindValue(1, pin.pinId);
            queryHelper.bindValue(2, pin.typeId);
            queryHelper.bindValue(3, pin.channelCount);
            if (!queryHelper.exec()) {
                qCritical() << "DialogManager::showPinSelectionDialog - FAILED to insert into vector_table_pins:" << queryHelper.lastError().text();
                success = false;
                break;
            }
        }
        
        if (success) {
            db.commit();
            qDebug() << "DialogManager::showPinSelectionDialog - Successfully updated pin associations for table" << tableId;
            return true; // Success
        } else {
            db.rollback();
            qWarning() << "DialogManager::showPinSelectionDialog - Pin association update failed, transaction rolled back.";
            return false;
        }
    }

    return false; // User cancelled the dialog
}

#include "dialogmanager_1.cpp"

#include "dialogmanager_DataDialog.cpp"
