#include "vectordatamanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QGroupBox>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QMessageBox>
#include <QDebug>
#include <QComboBox>
#include <QObject>

VectorDataManager::VectorDataManager(TimeSetDataAccess *dataAccess)
    : m_dataAccess(dataAccess)
{
}

void VectorDataManager::showVectorDataDialog(int tableId, const QString &tableName, QWidget *parent)
{
    // 创建对话框
    QDialog dialog(parent);
    dialog.setWindowTitle("向量数据编辑 - " + tableName);
    dialog.resize(900, 700);

    // 创建布局
    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);

    // 创建标题标签
    QLabel *titleLabel = new QLabel("编辑 " + tableName + " 的向量数据", &dialog);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    // 创建表格部分
    QTableWidget *vectorTable = new QTableWidget(&dialog);
    vectorTable->setAlternatingRowColors(true);
    vectorTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    vectorTable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    vectorTable->verticalHeader()->setDefaultSectionSize(25);

    mainLayout->addWidget(vectorTable);

    // 加载数据
    m_dataAccess->loadVectorData(tableId, vectorTable);

    // 创建数据编辑器控件
    QGroupBox *editorGroup = new QGroupBox("添加/插入数据", &dialog);
    QVBoxLayout *editorLayout = new QVBoxLayout(editorGroup);

    // 创建新数据表格
    QTableWidget *newDataTable = new QTableWidget(editorGroup);
    newDataTable->setAlternatingRowColors(true);
    newDataTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    newDataTable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    newDataTable->verticalHeader()->setDefaultSectionSize(25);

    // 设置新数据表格的列
    if (vectorTable->columnCount() > 0)
    {
        QStringList columnLabels;
        for (int i = 0; i < vectorTable->columnCount(); i++)
        {
            columnLabels << vectorTable->horizontalHeaderItem(i)->text();
        }

        newDataTable->setColumnCount(columnLabels.size());
        newDataTable->setHorizontalHeaderLabels(columnLabels);

        // 设置初始行数为5
        newDataTable->setRowCount(5);

        // 填充空单元格
        for (int row = 0; row < 5; row++)
        {
            for (int col = 0; col < columnLabels.size(); col++)
            {
                QTableWidgetItem *item = new QTableWidgetItem("");
                newDataTable->setItem(row, col, item);
            }
        }
    }

    editorLayout->addWidget(newDataTable);

    // 创建插入位置控件
    QHBoxLayout *insertLayout = new QHBoxLayout();

    QLabel *insertAtLabel = new QLabel("插入位置:", editorGroup);
    QSpinBox *insertAtEdit = new QSpinBox(editorGroup);
    insertAtEdit->setMinimum(0);
    insertAtEdit->setMaximum(vectorTable->rowCount());

    QCheckBox *appendToEndCheckbox = new QCheckBox("添加到末尾", editorGroup);
    appendToEndCheckbox->setChecked(true);

    insertLayout->addWidget(insertAtLabel);
    insertLayout->addWidget(insertAtEdit);
    insertLayout->addWidget(appendToEndCheckbox);
    insertLayout->addStretch();

    editorLayout->addLayout(insertLayout);

    // 禁用插入位置编辑框，因为默认是添加到末尾
    insertAtEdit->setEnabled(false);

    // 连接复选框状态改变信号
    QObject::connect(appendToEndCheckbox, &QCheckBox::stateChanged, [insertAtEdit](int state)
                     { insertAtEdit->setEnabled(state == Qt::Unchecked); });

    // 添加操作按钮
    QHBoxLayout *actionLayout = new QHBoxLayout();

    QPushButton *addRowButton = new QPushButton("添加行", editorGroup);
    QPushButton *removeRowButton = new QPushButton("删除行", editorGroup);
    QPushButton *saveDataButton = new QPushButton("保存数据", editorGroup);

    actionLayout->addWidget(addRowButton);
    actionLayout->addWidget(removeRowButton);
    actionLayout->addStretch();
    actionLayout->addWidget(saveDataButton);

    editorLayout->addLayout(actionLayout);

    // 添加编辑器组到主布局
    mainLayout->addWidget(editorGroup);

    // 创建对话框按钮
    QHBoxLayout *dialogButtonLayout = new QHBoxLayout();
    QPushButton *closeButton = new QPushButton("关闭", &dialog);
    dialogButtonLayout->addStretch();
    dialogButtonLayout->addWidget(closeButton);

    mainLayout->addLayout(dialogButtonLayout);

    // 连接信号槽
    QObject::connect(addRowButton, &QPushButton::clicked, [this, newDataTable]()
                     {
        QStringList columnLabels;
        for (int i = 0; i < newDataTable->columnCount(); i++)
        {
            columnLabels << newDataTable->horizontalHeaderItem(i)->text();
        }
        
        int newRow = newDataTable->rowCount();
        newDataTable->setRowCount(newRow + 1);
        
        // 添加新行
        this->addVectorRow(newDataTable, columnLabels, newRow); });

    QObject::connect(removeRowButton, &QPushButton::clicked, [newDataTable]()
                     {
        int currentRow = newDataTable->currentRow();
        if (currentRow >= 0)
        {
            newDataTable->removeRow(currentRow);
        }
        else
        {
            QMessageBox::information(nullptr, "提示", "请先选择要删除的行");
        } });

    QObject::connect(saveDataButton, &QPushButton::clicked, [this, tableId, vectorTable, newDataTable, appendToEndCheckbox, insertAtEdit, &dialog]()
                     {
        if (newDataTable->rowCount() == 0)
        {
            QMessageBox::information(&dialog, "提示", "没有数据需要保存");
            return;
        }
        
        bool appendToEnd = appendToEndCheckbox->isChecked();
        int insertPosition = appendToEnd ? -1 : insertAtEdit->value();
        
        // 保存数据
        if (m_dataAccess->saveVectorData(tableId, newDataTable, insertPosition, appendToEnd))
        {
            QMessageBox::information(&dialog, "成功", "数据保存成功");
            
            // 刷新主表格
            vectorTable->clear();
            m_dataAccess->loadVectorData(tableId, vectorTable);
            
            // 清空新数据表格
            newDataTable->clear();
            
            // 重新设置列
            QStringList columnLabels;
            for (int i = 0; i < vectorTable->columnCount(); i++)
            {
                columnLabels << vectorTable->horizontalHeaderItem(i)->text();
            }
            
            newDataTable->setColumnCount(columnLabels.size());
            newDataTable->setHorizontalHeaderLabels(columnLabels);
            
            // 设置为5行
            newDataTable->setRowCount(5);
            
            // 填充空单元格
            for (int row = 0; row < 5; row++)
            {
                for (int col = 0; col < columnLabels.size(); col++)
                {
                    QTableWidgetItem* item = new QTableWidgetItem("");
                    newDataTable->setItem(row, col, item);
                }
            }
            
            // 更新插入位置最大值
            insertAtEdit->setMaximum(vectorTable->rowCount());
        }
        else
        {
            QMessageBox::critical(&dialog, "错误", "保存数据失败");
        } });

    QObject::connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);

    // 显示对话框
    dialog.exec();
}

void VectorDataManager::addVectorRow(QTableWidget *table, const QStringList &pinOptions, int rowIdx)
{
    for (int col = 0; col < pinOptions.size(); col++)
    {
        QTableWidgetItem *item = new QTableWidgetItem("");
        table->setItem(rowIdx, col, item);
    }
}