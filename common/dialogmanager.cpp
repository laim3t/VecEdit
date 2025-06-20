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
        bool success = true;
        QSqlQuery queryHelper(db); // Use a helper query object for cleanup and column config

        // 1a. 清除该表旧的 VectorTableColumnConfiguration (确保与新选择同步)
        qDebug() << "DialogManager::showPinSelectionDialog - Preparing to delete old column config for tableId:" << tableId;
        bool prepareOk = queryHelper.prepare("DELETE FROM VectorTableColumnConfiguration WHERE master_record_id = ?");
        if (!prepareOk)
        {
            qCritical() << "DialogManager::showPinSelectionDialog - FAILED to prepare delete query for ColumnConfiguration:" << queryHelper.lastError().text();
            QMessageBox::critical(m_parent, "数据库准备错误", "准备清除旧列配置失败：" + queryHelper.lastError().text());
            return false;
        }
        queryHelper.addBindValue(tableId);
        if (!queryHelper.exec())
        {
            qCritical() << "DialogManager::showPinSelectionDialog - FAILED to execute delete query for ColumnConfiguration:" << queryHelper.lastError().text() << " (Bound values:" << queryHelper.boundValues() << ")";
            QMessageBox::critical(m_parent, "数据库错误", "清除旧列配置失败：" + queryHelper.lastError().text());
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
            return false;
        }
        queryHelper.addBindValue(tableId);
        if (!queryHelper.exec())
        {
            qCritical() << "DialogManager::showPinSelectionDialog - FAILED to execute delete query for vector_table_pins:" << queryHelper.lastError().text() << " (Bound values:" << queryHelper.boundValues() << ")";
            QMessageBox::critical(m_parent, "数据库错误", "清除旧管脚关联失败：" + queryHelper.lastError().text());
            return false;
        }
        qDebug() << "DialogManager::showPinSelectionDialog - Successfully deleted old vector_table_pins.";

        // 收集选中的管脚信息
        QList<PinDataFromDialog> selectedPins;
        for (auto it = pinCheckboxes.begin(); it != pinCheckboxes.end(); ++it)
        {
            int pinId = it.key();
            QCheckBox *checkbox = it.value();

            if (checkbox->isChecked())
        {
                PinDataFromDialog pinData;
                pinData.pinId = pinId;
                pinData.pinName = localPinList[pinId];
                pinData.typeId = pinTypeComboBoxes[pinId]->currentData().toInt();
                pinData.channelCount = 1; // 默认为1，后续可能会修改
                pinData.isChecked = true;
                
                selectedPins.append(pinData);
            }
        }
        
        if (selectedPins.isEmpty())
        {
            QMessageBox::warning(m_parent, "警告", "请至少选择一个管脚");
            return false;
        }

        int columnOrder = 0; // 用于确定列顺序

        // 添加标准列配置
        qDebug() << "DialogManager::showPinSelectionDialog - 开始添加标准列配置";
        QList<QPair<QString, QString>> standardColumnsToAdd;
        standardColumnsToAdd << qMakePair(QString("Label"), QString("TEXT"));
        standardColumnsToAdd << qMakePair(QString("Instruction"), QString("TEXT"));
        standardColumnsToAdd << qMakePair(QString("TimeSet"), QString("TIMESET"));
        standardColumnsToAdd << qMakePair(QString("Capture"), QString("TEXT"));
        standardColumnsToAdd << qMakePair(QString("Ext"), QString("TEXT"));
        standardColumnsToAdd << qMakePair(QString("Comment"), QString("TEXT"));

        // 创建空的JSON对象作为属性
        QJsonObject properties;
        QJsonDocument doc(properties);
        QString jsonString = doc.toJson(QJsonDocument::Compact);
        
        int sortIndex = 0;
        for (const auto& col : standardColumnsToAdd) {
            qDebug() << "DialogManager::showPinSelectionDialog - 添加标准列:" << col.first << "类型:" << col.second;
        queryHelper.prepare("INSERT INTO VectorTableColumnConfiguration "
                            "(master_record_id, column_name, column_order, column_type, data_properties) "
                            "VALUES (?, ?, ?, ?, ?)");
        queryHelper.addBindValue(tableId);
            queryHelper.addBindValue(col.first);
        queryHelper.addBindValue(columnOrder++);
            queryHelper.addBindValue(col.second);
            queryHelper.addBindValue(jsonString);

        if (!queryHelper.exec())
        {
                qCritical() << "DialogManager::showPinSelectionDialog - Failed to insert standard column" << col.first << ":" << queryHelper.lastError().text();
                QMessageBox::critical(m_parent, "数据库错误", "添加" + col.first + "列失败：" + queryHelper.lastError().text());
            return false;
        }
            sortIndex++;
        }

        // 3. 为每个选中的管脚添加列配置
        for (const PinDataFromDialog &pinData : selectedPins)
        {
            // 插入vector_table_pins记录
            queryHelper.prepare("INSERT INTO vector_table_pins "
                              "(table_id, pin_id, pin_type, pin_channel_count) "
                              "VALUES (?, ?, ?, ?)");
        queryHelper.addBindValue(tableId);
            queryHelper.addBindValue(pinData.pinId);
            queryHelper.addBindValue(pinData.typeId);
            queryHelper.addBindValue(pinData.channelCount);

        if (!queryHelper.exec())
        {
                qCritical() << "DialogManager::showPinSelectionDialog - Failed to insert vector_table_pins record:" << queryHelper.lastError().text();
                QMessageBox::critical(m_parent, "数据库错误", "添加管脚关联记录失败：" + queryHelper.lastError().text());
            return false;
        }

            // 添加列配置
        queryHelper.prepare("INSERT INTO VectorTableColumnConfiguration "
                            "(master_record_id, column_name, column_order, column_type, data_properties) "
                            "VALUES (?, ?, ?, ?, ?)");
        queryHelper.addBindValue(tableId);
            queryHelper.addBindValue(pinData.pinName);
        queryHelper.addBindValue(columnOrder++);
            
            // 根据管脚类型设置列类型
            QString columnType;
            switch (pinData.typeId)
        {
                case 1: // 假设1是INPUT类型
                    columnType = "INPUT";
                    break;
                case 2: // 假设2是OUTPUT类型
                    columnType = "OUTPUT";
                    break;
                case 3: // 假设3是INOUT类型
                    columnType = "INOUT";
                    break;
                default:
                    columnType = "UNKNOWN";
            }
            
            queryHelper.addBindValue(columnType);
            
            // 创建包含管脚ID的JSON对象
            QJsonObject pinProperties;
            pinProperties["pin_id"] = pinData.pinId;
            QJsonDocument pinDoc(pinProperties);
            QString pinJsonString = pinDoc.toJson(QJsonDocument::Compact);
            
            queryHelper.addBindValue(pinJsonString);
            
                if (!queryHelper.exec())
                {
                qCritical() << "DialogManager::showPinSelectionDialog - Failed to insert pin column configuration:" << queryHelper.lastError().text();
                QMessageBox::critical(m_parent, "数据库错误", "添加管脚列配置失败：" + queryHelper.lastError().text());
            return false;
        }
        }

        // 所有操作成功，提示用户并继续
            // QMessageBox::information(m_parent, "保存成功", "管脚信息已成功保存！");
        // 这里不应该调用下一个对话框，由主窗口的工作流控制
            return true;
        }
        else
        {
        // 用户取消了操作，清理任何可能的临时数据
        qDebug() << "DialogManager::showPinSelectionDialog - User cancelled pin selection.";
            return false;
        }
    }

bool DialogManager::showVectorDataDialog(QSqlDatabase& db, int tableId, const QString &tableName, int startIndex)
        {
    qDebug() << "[TXN_PROBE] DialogManager::showVectorDataDialog ENTRY. Transaction state:" << db.transaction();

    // 创建向量行数据录入对话框
    QDialog vectorDataDialog(m_parent);
    vectorDataDialog.setWindowTitle("向量行数据录入 - " + tableName);
    vectorDataDialog.setMinimumWidth(600);
    vectorDataDialog.setMinimumHeight(400);

    QVBoxLayout *mainLayout = new QVBoxLayout(&vectorDataDialog);

    // 添加标题下方的控件区域
    QHBoxLayout *headerLayout = new QHBoxLayout();

    // 添加"TimeS设置"按钮
    QPushButton *timeSetButton = new QPushButton("TimeS设置", &vectorDataDialog);
    headerLayout->addWidget(timeSetButton);

    // 在按钮右侧添加弹簧，推动按钮到左侧
    headerLayout->addStretch();

    // 将标题栏布局添加到主布局
    mainLayout->addLayout(headerLayout);

    // 添加注释标签
    QLabel *noteLabel = new QLabel("<b>注释:</b> 请确保添加的行数是下方表格中行数的倍数。", &vectorDataDialog);
    noteLabel->setStyleSheet("color: black;");
    mainLayout->addWidget(noteLabel);

    // 使用传入的数据库连接
    QSqlQuery query(db);

    // 查询当前向量表中的总行数
    VectorDataHandler &dataHandler = VectorDataHandler::instance();
    int totalRowsInFile = dataHandler.getVectorTableRowCount(tableId);

    // 计算剩余可用行数（初始总数为8388608）
    const int TOTAL_AVAILABLE_ROWS = 8388608;
    int remainingRows = TOTAL_AVAILABLE_ROWS - totalRowsInFile;
    if (remainingRows < 0)
        remainingRows = 0;

    // 从数据库获取已选择的管脚
    QList<QPair<int, QPair<QString, QPair<int, QString>>>> selectedPins; // pinId, <pinName, <channelCount, typeName>>

    query.prepare("SELECT p.id, pl.pin_name, p.pin_channel_count, t.type_name, p.pin_type "
                  "FROM vector_table_pins p "
                  "JOIN pin_list pl ON p.pin_id = pl.id "
                  "JOIN type_options t ON p.pin_type = t.id "
                  "WHERE p.table_id = ?");
    query.addBindValue(tableId);

    if (query.exec())
    {
        while (query.next())
        {
            int pinId = query.value(0).toInt();
            QString pinName = query.value(1).toString();
            int channelCount = query.value(2).toInt();
            QString typeName = query.value(3).toString();
            int typeId = query.value(4).toInt();

            selectedPins.append(qMakePair(pinId, qMakePair(pinName, qMakePair(channelCount, typeName))));
        }
    }
    else
    {
        QMessageBox::critical(m_parent, "数据库错误", "获取管脚信息失败：" + query.lastError().text());
        qDebug() << "[TXN_PROBE] DialogManager::showVectorDataDialog EXIT (query error). Transaction state:" << DatabaseManager::instance()->database().transaction();
        return false;
    }

    if (selectedPins.isEmpty())
    {
        QMessageBox::warning(m_parent, "警告", "插入向量行前请先引用管脚。");
        qDebug() << "[TXN_PROBE] DialogManager::showVectorDataDialog EXIT (no pins). Transaction state:" << DatabaseManager::instance()->database().transaction();
        return false;
    }

    // 获取pin_options选项
    QStringList pinOptions;
    query.exec("SELECT pin_value FROM pin_options ORDER BY id");
    while (query.next())
    {
        pinOptions << query.value(0).toString();
    }

    if (pinOptions.isEmpty())
    {
        pinOptions << "X"; // 默认值
    }

    // 获取timeset选项
    QMap<int, QString> timesetOptions;
    query.exec("SELECT id, timeset_name FROM timeset_list ORDER BY id");
    while (query.next())
    {
        timesetOptions[query.value(0).toInt()] = query.value(1).toString();
    }

    // 创建表格
    QTableWidget *vectorTable = new QTableWidget(&vectorDataDialog);
    vectorTable->setColumnCount(selectedPins.size()); // 设置列数为选中的管脚数
    vectorTable->setRowCount(0);                      // 初始没有行

    // 设置表头
    for (int i = 0; i < selectedPins.size(); i++)
    {
        const auto &pinInfo = selectedPins[i];
        const auto &pinName = pinInfo.second.first;
        const auto &channelCount = pinInfo.second.second.first;
        const auto &typeName = pinInfo.second.second.second;

        // 创建表头项
        QString headerText = pinName + "\nx" + QString::number(channelCount) + "\n" + typeName;
        QTableWidgetItem *headerItem = new QTableWidgetItem(headerText);
        headerItem->setTextAlignment(Qt::AlignCenter);
        QFont headerFont = headerItem->font();
        headerFont.setBold(true);
        headerItem->setFont(headerFont);

        vectorTable->setHorizontalHeaderItem(i, headerItem);
    }

    // 设置表格选择模式为整行选择
    vectorTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    vectorTable->setSelectionMode(QAbstractItemView::SingleSelection);

    // 启用单元格点击选择整行
    QObject::connect(vectorTable, &QTableWidget::cellClicked, [vectorTable](int row, int column)
                     { vectorTable->selectRow(row); });

    // 添加一行默认数据（使用批量添加方法，但只添加一行）
    VectorDataHandler::addVectorRows(vectorTable, pinOptions, 0, 1);

    // 添加表格到布局
    mainLayout->addWidget(vectorTable);

    // 创建操作按钮区域（表格和底部区域之间的右侧）
    QWidget *actionButtonsWidget = new QWidget(&vectorDataDialog);
    QHBoxLayout *actionButtonsLayout = new QHBoxLayout(actionButtonsWidget);
    actionButtonsLayout->setContentsMargins(0, 0, 0, 0);
    actionButtonsLayout->setSpacing(8); // 增加按钮之间的间距

    // 创建添加行和删除行按钮（使用SVG图标）
    QPushButton *addRowButton = new QPushButton(actionButtonsWidget);
    addRowButton->setToolTip("添加行");
    addRowButton->setFixedSize(32, 32); // 增加按钮大小
    addRowButton->setIcon(QIcon(":/resources/icons/plus-circle.svg"));
    addRowButton->setIconSize(QSize(24, 24)); // 增加图标大小
    addRowButton->setStyleSheet("QPushButton { border: none; background-color: transparent; }");

    QPushButton *deleteRowButton = new QPushButton(actionButtonsWidget);
    deleteRowButton->setToolTip("删除行");
    deleteRowButton->setFixedSize(32, 32); // 增加按钮大小
    deleteRowButton->setIcon(QIcon(":/resources/icons/x-circle.svg"));
    deleteRowButton->setIconSize(QSize(24, 24)); // 增加图标大小
    deleteRowButton->setStyleSheet("QPushButton { border: none; background-color: transparent; }");

    actionButtonsLayout->addWidget(addRowButton);
    actionButtonsLayout->addWidget(deleteRowButton);

    // 添加按钮到右侧
    QHBoxLayout *midButtonLayout = new QHBoxLayout();
    midButtonLayout->addStretch();
    midButtonLayout->addWidget(actionButtonsWidget);
    mainLayout->addLayout(midButtonLayout);

    // 创建水平布局容器，包含信息、行和设置
    QHBoxLayout *sectionsLayout = new QHBoxLayout();

    // 创建信息分组
    QGroupBox *infoGroup = new QGroupBox("信息", &vectorDataDialog);
    QFormLayout *infoLayout = new QFormLayout(infoGroup);

    // 文件中总行数显示
    QLineEdit *totalRowsEdit = new QLineEdit(infoGroup);
    totalRowsEdit->setText(QString::number(totalRowsInFile));
    totalRowsEdit->setReadOnly(true);
    totalRowsEdit->setEnabled(false);                                                         // 禁用输入框
    totalRowsEdit->setStyleSheet("QLineEdit { background-color: #F0F0F0; color: #808080; }"); // 设置灰色背景和文字颜色
    infoLayout->addRow("文件中总行数:", totalRowsEdit);

    // 剩余可用行数显示
    QLineEdit *remainingRowsEdit = new QLineEdit(infoGroup);
    remainingRowsEdit->setText(QString::number(remainingRows));
    remainingRowsEdit->setReadOnly(true);
    remainingRowsEdit->setEnabled(false);                                                         // 禁用输入框
    remainingRowsEdit->setStyleSheet("QLineEdit { background-color: #F0F0F0; color: #808080; }"); // 设置灰色背景和文字颜色
    infoLayout->addRow("剩余可用行数:", remainingRowsEdit);

    // 新增"行"参数分组
    QGroupBox *rowOptionsGroup = new QGroupBox("行", &vectorDataDialog);
    QVBoxLayout *rowOptionsLayout = new QVBoxLayout(rowOptionsGroup);

    // 添加到最后复选框
    QCheckBox *appendToEndCheckbox = new QCheckBox("添加到最后", rowOptionsGroup);
    appendToEndCheckbox->setChecked(true); // 默认勾选
    rowOptionsLayout->addWidget(appendToEndCheckbox);

    // 向前插入输入框
    QHBoxLayout *insertAtLayout = new QHBoxLayout();
    QLabel *insertAtLabel = new QLabel("向前插入：", rowOptionsGroup);
    QLineEdit *insertAtEdit = new QLineEdit(rowOptionsGroup);
    insertAtEdit->setText("1");      // 默认值为1
    insertAtEdit->setEnabled(false); // 默认不可编辑(因为默认勾选了添加到最后)
    insertAtLayout->addWidget(insertAtLabel);
    insertAtLayout->addWidget(insertAtEdit);
    rowOptionsLayout->addLayout(insertAtLayout);

    // 行数输入框
    QHBoxLayout *rowCountLayout = new QHBoxLayout();
    QLabel *rowCountLabel = new QLabel("行数：", rowOptionsGroup);
    QLineEdit *rowCountEdit = new QLineEdit(rowOptionsGroup);
    rowCountEdit->setText("1"); // 默认值为1
    rowCountLayout->addWidget(rowCountLabel);
    rowCountLayout->addWidget(rowCountEdit);
    rowOptionsLayout->addLayout(rowCountLayout);

    // 创建设置区域
    QGroupBox *settingsGroup = new QGroupBox("设置", &vectorDataDialog);
    QFormLayout *settingsLayout = new QFormLayout(settingsGroup);

    // TimeSet 选择
    QComboBox *timesetCombo = new QComboBox(settingsGroup);
    for (auto it = timesetOptions.begin(); it != timesetOptions.end(); ++it)
    {
        timesetCombo->addItem(it.value(), it.key());
    }
    settingsLayout->addRow("TimeSet:", timesetCombo);

    // 添加各个分组到水平布局
    sectionsLayout->addWidget(infoGroup);
    sectionsLayout->addWidget(rowOptionsGroup);
    sectionsLayout->addWidget(settingsGroup);

    // 将水平布局添加到主布局
    mainLayout->addLayout(sectionsLayout);

    // 连接勾选框状态改变信号
    QObject::connect(appendToEndCheckbox, &QCheckBox::stateChanged, [insertAtEdit](int state)
                     {
                         insertAtEdit->setEnabled(state != Qt::Checked); // 当不勾选时启用向前插入输入框
                         qDebug() << "向量行数据录入 - 追加模式状态改变:" << (state == Qt::Checked ? "启用" : "禁用") 
                                  << "，向前插入输入框:" << (state != Qt::Checked ? "启用" : "禁用"); });

    // 连接行数输入框变化信号，更新剩余可用行数
    QObject::connect(rowCountEdit, &QLineEdit::textChanged, [remainingRowsEdit, totalRowsInFile, vectorTable](const QString &text)
                     {
                         int requestedRows = text.toInt();
                         int dataRows = vectorTable->rowCount();
                         int totalNewRows = 0;

                         // 只有当输入的行数是有效的（大于等于数据行数且是数据行数的整数倍）时才更新
                         if (requestedRows >= dataRows && requestedRows % dataRows == 0)
                         {
                             totalNewRows = requestedRows;
                         }
                         else
                         {
                             totalNewRows = dataRows; // 默认至少使用数据行数
                         }

                         const int TOTAL_AVAILABLE_ROWS = 8388608;
                         int updatedRemaining = TOTAL_AVAILABLE_ROWS - totalRowsInFile - totalNewRows;
                         if (updatedRemaining < 0)
                             updatedRemaining = 0;
                         remainingRowsEdit->setText(QString::number(updatedRemaining)); // 即使禁用也可以更新文本
                     });

    // 按钮区域
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *saveButton = new QPushButton("确定", &vectorDataDialog);
    QPushButton *cancelButton = new QPushButton("取消", &vectorDataDialog);

    buttonLayout->addStretch();
    buttonLayout->addWidget(saveButton);
    buttonLayout->addWidget(cancelButton);

    mainLayout->addLayout(buttonLayout);

    // 连接添加行和删除行按钮信号
    QObject::connect(addRowButton, &QPushButton::clicked, [&]()
                     {
                         VectorDataHandler::addVectorRows(vectorTable, pinOptions, vectorTable->rowCount(), 1);

                         // 更新行数输入框和剩余可用行数
                         int newRowCount = vectorTable->rowCount();
                         rowCountEdit->setText(QString::number(newRowCount));

                         // 计算并更新剩余可用行数
                         int updatedRemaining = TOTAL_AVAILABLE_ROWS - totalRowsInFile - newRowCount;
                         if (updatedRemaining < 0)
                             updatedRemaining = 0;
                         remainingRowsEdit->setText(QString::number(updatedRemaining)); // 即使禁用也可以更新文本
                     });

    QObject::connect(deleteRowButton, &QPushButton::clicked, [&]()
                     {
                         // 获取当前选中的行
                         QList<int> selectedRows;
                         QModelIndexList selectedIndexes = vectorTable->selectionModel()->selectedRows();
                         if (selectedIndexes.isEmpty())
                         {
                             // 如果没有选中整行，尝试获取选中的单元格
                             QList<QTableWidgetItem *> selectedItems = vectorTable->selectedItems();
                             if (selectedItems.isEmpty())
                             {
                                 QMessageBox::warning(&vectorDataDialog, "警告", "请先选择要删除的行");
                                 return;
                             }

                             // 获取所有选中单元格所在的行
                             QSet<int> rowSet;
                             for (QTableWidgetItem *item : selectedItems)
                             {
                                 rowSet.insert(item->row());
                             }

                             // 将行号添加到列表
                             for (int row : rowSet)
                             {
                                 selectedRows.append(row);
                             }
                         }
                         else
                         {
                             // 如果选中了整行，直接获取行号
                             for (const QModelIndex &index : selectedIndexes)
                             {
                                 selectedRows.append(index.row());
                             }
                         }

                         // 从最大行号开始删除，避免索引变化
                         std::sort(selectedRows.begin(), selectedRows.end(), std::greater<int>());

                         for (int row : selectedRows)
                         {
                             vectorTable->removeRow(row);
                         }

                         // 更新行数输入框和剩余可用行数
                         int newRowCount = vectorTable->rowCount();
                         if (newRowCount == 0)
                         {
                             // 如果删除所有行，添加一个默认行
                             VectorDataHandler::addVectorRows(vectorTable, pinOptions, 0, 1);
                             newRowCount = 1;
                         }

                         rowCountEdit->setText(QString::number(newRowCount));

                         // 计算并更新剩余可用行数
                         int updatedRemaining = TOTAL_AVAILABLE_ROWS - totalRowsInFile - newRowCount;
                         if (updatedRemaining < 0)
                             updatedRemaining = 0;
                         remainingRowsEdit->setText(QString::number(updatedRemaining)); // 即使禁用也可以更新文本
                     });

    // 连接TimeS设置按钮
    QObject::connect(timeSetButton, &QPushButton::clicked, [this]()
                     {
        // 弹出TimeSet设置对话框
        TimeSetDialog dialog(m_parent);
        dialog.exec(); });

    // 连接保存和取消按钮信号
    QObject::connect(saveButton, &QPushButton::clicked, [&]() -> bool
                     {
        // 获取向量行和用户设置参数
        int rowDataCount = vectorTable->rowCount();
        int totalRowCount = rowCountEdit->text().toInt();
        
        // 检查行数设置
        if (totalRowCount < rowDataCount) {
            QMessageBox::warning(&vectorDataDialog, "参数错误", "设置的总行数小于实际添加的行数据数量！");
            return false;
        }
        
        if (totalRowCount % rowDataCount != 0) {
            QMessageBox::warning(&vectorDataDialog, "参数错误", "设置的总行数必须是行数据数量的整数倍！");
            return false;
        }
        
        // 检查是否超出可用行数
        if (totalRowCount > remainingRows) {
            QMessageBox::warning(&vectorDataDialog, "参数错误", "添加的行数超出了可用行数！");
            return false;
        }
        
        // 获取插入位置
        int actualStartIndex = startIndex;
        if (!appendToEndCheckbox->isChecked()) {
            actualStartIndex = insertAtEdit->text().toInt() - 1; // 转为0基索引
            if (actualStartIndex < 0) {
                actualStartIndex = 0;
            }
        }
        
        // 创建进度对话框
        QProgressDialog progressDialog("准备添加向量数据...", "取消", 0, 100, &vectorDataDialog);
        progressDialog.setWindowTitle(QString("正在处理 %1 行数据").arg(totalRowCount));
        progressDialog.setWindowModality(Qt::WindowModal);
        progressDialog.setMinimumDuration(0); // 立即显示
        progressDialog.resize(400, progressDialog.height()); // 加宽进度条，确保文本显示完整
        progressDialog.setValue(0);
        
        // 禁用保存和取消按钮，防止重复操作
        saveButton->setEnabled(false);
        cancelButton->setEnabled(false);
        
        // 获取VectorDataHandler单例
        VectorDataHandler& dataHandler = VectorDataHandler::instance();
        
        // 连接进度更新信号
        QObject::connect(&dataHandler, &VectorDataHandler::progressUpdated,
                         &progressDialog, &QProgressDialog::setValue);
                         
        // 连接取消按钮
        QObject::connect(&progressDialog, &QProgressDialog::canceled,
                         &dataHandler, &VectorDataHandler::cancelOperation);
        
        QString errorMessage;
        
        qDebug() << "DialogManager::showVectorDataDialog - 开始保存向量行数据，表ID:" << tableId 
                 << "，起始索引:" << actualStartIndex << "，总行数:" << totalRowCount
                 << "，追加模式:" << appendToEndCheckbox->isChecked();
        
        // 传递实际的向量表和正确的追加标志
        bool success = dataHandler.insertVectorRows(
            db, tableId, actualStartIndex, totalRowCount, timesetCombo->currentData().toInt(),
            vectorTable, appendToEndCheckbox->isChecked(), selectedPins, errorMessage
        );
        
        // 恢复按钮状态
        saveButton->setEnabled(true);
        cancelButton->setEnabled(true);
        
        // 关闭进度对话框
        progressDialog.close();
        
        // 断开信号连接，防止后续操作影响
        QObject::disconnect(&dataHandler, &VectorDataHandler::progressUpdated,
                         &progressDialog, &QProgressDialog::setValue);
        QObject::disconnect(&progressDialog, &QProgressDialog::canceled,
                         &dataHandler, &VectorDataHandler::cancelOperation);
        
        if (success) {
            // QMessageBox::information(&vectorDataDialog, "保存成功", "向量行数据已成功保存！");
            vectorDataDialog.accept();
            qDebug() << "[TXN_PROBE] DialogManager::showVectorDataDialog SAVE SUCCESS. Transaction state:" << DatabaseManager::instance()->database().transaction();
            return true;
        } else {
            QMessageBox::critical(&vectorDataDialog, "数据库错误", errorMessage);
            qDebug() << "[TXN_PROBE] DialogManager::showVectorDataDialog SAVE FAILED. Transaction state:" << DatabaseManager::instance()->database().transaction();
            return false;
        } });

    QObject::connect(cancelButton, &QPushButton::clicked, &vectorDataDialog, &QDialog::reject);

    // 显示对话框并获取结果
    if (vectorDataDialog.exec() == QDialog::Accepted) {
        // 对话框被接受，数据已经在 saveButton 点击处理中被保存
        qDebug() << "[TXN_PROBE] DialogManager::showVectorDataDialog EXIT (accepted). Transaction state:" << DatabaseManager::instance()->database().transaction();
        return true;
    }
    
    qDebug() << "[TXN_PROBE] DialogManager::showVectorDataDialog EXIT (rejected). Transaction state:" << DatabaseManager::instance()->database().transaction();
    return false;
}

bool DialogManager::showAddPinsDialog()
{
    // 创建并显示管脚添加对话框
    PinListDialog dialog(m_parent);

    if (dialog.exec() == QDialog::Accepted)
    {
        // 获取用户设置的管脚列表
        QList<QString> pinNames = dialog.getPinNames();

        // 添加到数据库
        if (!pinNames.isEmpty())
        {
            // 获取数据库连接
            QSqlDatabase db = DatabaseManager::instance()->database();

            // 开始事务
            db.transaction();

            // 循环添加每个管脚
            bool success = true;
            for (const QString &pinName : pinNames)
            {
                QSqlQuery query(db);
                query.prepare("INSERT INTO pin_list (pin_name, pin_note, pin_nav_note) VALUES (?, ?, ?)");
                query.addBindValue(pinName);
                query.addBindValue(""); // pin_note为空
                query.addBindValue(""); // pin_nav_note为空

                if (!query.exec())
                {
                    qDebug() << "添加管脚失败:" << query.lastError().text();
                    success = false;
                    break;
                }
            }

            // 提交或回滚事务
            if (success)
            {
                db.commit();
                return true;
            }
            else
            {
                db.rollback();
                return false;
            }
        }
    }

    return false;
}

bool DialogManager::showTimeSetDialog(bool isInitialSetup)
{
    qDebug() << "[TXN_PROBE] DialogManager::showTimeSetDialog ENTRY. Transaction state:" << DatabaseManager::instance()->database().transaction();
    qDebug() << "DialogManager::showTimeSetDialog - 开始显示TimeSet对话框，初始设置模式:" << isInitialSetup;

    // 在非初始设置模式下，检查向量表是否存在
    if (!isInitialSetup)
    {
        // 检查向量表是否存在
        QSqlDatabase db = DatabaseManager::instance()->database();
        QSqlQuery query(db);

        if (query.exec("SELECT COUNT(*) FROM vector_tables"))
        {
            if (query.next())
            {
                int count = query.value(0).toInt();
                qDebug() << "DialogManager::showTimeSetDialog - 检查向量表数量:" << count;

                if (count <= 0)
                {
                    qDebug() << "DialogManager::showTimeSetDialog - 非初始设置模式下未找到向量表，提前终止";
                    QMessageBox::information(m_parent, "提示", "没有找到向量表，请先创建向量表");
                    qDebug() << "[TXN_PROBE] DialogManager::showTimeSetDialog EXIT (no vector tables). Transaction state:" << DatabaseManager::instance()->database().transaction();
                    return false;
                }
            }
        }
        else
        {
            qDebug() << "DialogManager::showTimeSetDialog - 查询向量表失败:" << query.lastError().text();
        }
    }
    else
    {
        qDebug() << "DialogManager::showTimeSetDialog - 初始设置模式，跳过向量表检查";
    }

    // 创建并显示TimeSet对话框
    qDebug() << "DialogManager::showTimeSetDialog - 创建TimeSet对话框实例";
    TimeSetDialog dialog(m_parent, isInitialSetup);

    qDebug() << "DialogManager::showTimeSetDialog - 显示对话框";
    if (dialog.exec() == QDialog::Accepted)
    {
        qDebug() << "DialogManager::showTimeSetDialog - 用户接受了对话框";
        qDebug() << "[TXN_PROBE] DialogManager::showTimeSetDialog EXIT (accepted). Transaction state:" << DatabaseManager::instance()->database().transaction();
        return true;
    }

    qDebug() << "DialogManager::showTimeSetDialog - 用户取消了对话框";
    qDebug() << "[TXN_PROBE] DialogManager::showTimeSetDialog EXIT (rejected). Transaction state:" << DatabaseManager::instance()->database().transaction();
    return false;
}

void DialogManager::showDatabaseViewDialog()
{
    // 创建并显示数据库视图对话框
    DatabaseViewDialog *dialog = new DatabaseViewDialog(m_parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose); // 关闭时自动删除
    dialog->exec();
}

bool DialogManager::showPinGroupDialog()
{
    qDebug() << "DialogManager::showPinGroupDialog - 显示管脚分组对话框";

    // 检查向量表是否存在
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery query(db);

    if (query.exec("SELECT COUNT(*) FROM vector_tables"))
    {
        if (query.next())
        {
            int count = query.value(0).toInt();
            qDebug() << "DialogManager::showPinGroupDialog - 检查向量表数量:" << count;

            if (count <= 0)
            {
                qDebug() << "DialogManager::showPinGroupDialog - 未找到向量表，提前终止";
                QMessageBox::information(m_parent, "提示", "没有找到向量表，请先创建向量表");
                return false;
            }
        }
    }
    else
    {
        qDebug() << "DialogManager::showPinGroupDialog - 查询向量表失败:" << query.lastError().text();
    }

    // 创建并显示管脚分组对话框
    PinGroupDialog dialog(m_parent);

    qDebug() << "DialogManager::showPinGroupDialog - 显示对话框";
    if (dialog.exec() == QDialog::Accepted)
    {
        qDebug() << "DialogManager::showPinGroupDialog - 用户接受了对话框";
        return true;
    }

    qDebug() << "DialogManager::showPinGroupDialog - 用户取消了对话框";
    return false;
}