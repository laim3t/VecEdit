#include "dialogmanager.h"
#include "vector/vector_data_types.h"
#include "vector/robustvectordatahandler.h"
#include "vector/vectordatahandler.h"
#include "timeset/timesetdialog.h"
#include "database/databasemanager.h"
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QGroupBox>
#include <QFormLayout>
#include <QMessageBox>
#include <QProgressDialog>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QSet>
#include <algorithm>

bool DialogManager::showVectorDataDialog(int tableId, const QString &tableName, int startIndex)
{
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

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
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
        return false;
    }

    if (selectedPins.isEmpty())
    {
        QMessageBox::warning(m_parent, "警告", "插入向量行前请先引用管脚。");
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
    QObject::connect(saveButton, &QPushButton::clicked, [&]()
                     {
        // 获取向量行和用户设置参数
        int rowDataCount = vectorTable->rowCount();
        int totalRowCount = rowCountEdit->text().toInt();
        
        // 检查行数设置
        if (totalRowCount < rowDataCount) {
            QMessageBox::warning(&vectorDataDialog, "参数错误", "设置的总行数小于实际添加的行数据数量！");
            return;
        }
        
        if (totalRowCount % rowDataCount != 0) {
            QMessageBox::warning(&vectorDataDialog, "参数错误", "设置的总行数必须是行数据数量的整数倍！");
            return;
        }
        
        // 检查是否超出可用行数
        if (totalRowCount > remainingRows) {
            QMessageBox::warning(&vectorDataDialog, "参数错误", "添加的行数超出了可用行数！");
            return;
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
        
        // 调用VectorDataHandler插入行
        bool success = false;

        if (m_useNewDataHandler)
        {
            // 新架构的逻辑已迁移到 MainWindow::addRowToCurrentVectorTableModel 和 AddRowDialog,
            // 此处的代码已废弃，以消除对旧接口的依赖。
            errorMessage = "New data handler path is deprecated in this dialog.";
            qCritical() << "DialogManager::showVectorDataDialog - " << errorMessage;
        }
        else
        {
            success = VectorDataHandler::instance().insertVectorRows(
                tableId,
                actualStartIndex,
                totalRowCount,
                timesetCombo->currentData().toInt(),
                vectorTable,
                appendToEndCheckbox->isChecked(),
                selectedPins,
                errorMessage);
        }
        
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
        } else {
            QMessageBox::critical(&vectorDataDialog, "数据库错误", errorMessage);
        } });

    QObject::connect(cancelButton, &QPushButton::clicked, &vectorDataDialog, &QDialog::reject);

    // 显示对话框
    return (vectorDataDialog.exec() == QDialog::Accepted);
}
