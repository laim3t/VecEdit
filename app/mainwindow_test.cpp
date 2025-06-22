// ==========================================================
//  测试Model/View架构：mainwindow_test.cpp
// ==========================================================

#include "mainwindow.h"
#include "../vector/vectortablemodel.h"
#include <QTableView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QDebug>
#include <QLabel>
#include <QTimer>
#include <QSqlDatabase>
#include <QSqlQuery>

void MainWindow::testModelView()
{
    const QString funcName = "MainWindow::testModelView";
    qDebug() << funcName << " - 开始测试Model/View架构";
    
    // 创建一个对话框窗口
    QDialog* dialog = new QDialog(this);
    dialog->setWindowTitle("表格模型测试");
    dialog->resize(800, 600);
    
    // 创建布局
    QVBoxLayout* layout = new QVBoxLayout(dialog);
    
    // 创建表格视图
    m_testTableView = new QTableView(dialog);
    layout->addWidget(m_testTableView);
    
    // 创建模型并关联到视图
    m_testTableModel = new VectorTableModel(this);
    m_testTableView->setModel(m_testTableModel);
    
    // 获取当前活动的表格ID
    int currentTableId = -1;
    
    // 从当前选中的Tab获取表格ID
    int currentTabIndex = m_vectorTabWidget->currentIndex();
    if (currentTabIndex >= 0) {
        currentTableId = m_tabToTableId.value(currentTabIndex, -1);
    }
    
    // 如果没有从Tab获取到ID，尝试从下拉框获取
    if (currentTableId <= 0 && m_vectorTableSelector->count() > 0) {
        currentTableId = m_vectorTableSelector->currentData().toInt();
    }
    
    if (currentTableId > 0)
    {
        qDebug() << funcName << " - 加载表格数据，表ID:" << currentTableId;
        
        // 加载表格数据
        m_testTableModel->loadTable(currentTableId);
        
        // 创建并设置自定义代理，直接传递表格ID
        VectorTableItemDelegate* delegate = new VectorTableItemDelegate(currentTableId, m_testTableView);
        m_testTableView->setItemDelegate(delegate);
        
        // 调整列宽以适应内容
        m_testTableView->resizeColumnsToContents();
        
        // 设置表格样式
        m_testTableView->setAlternatingRowColors(true);
        m_testTableView->setSelectionBehavior(QAbstractItemView::SelectItems);
        m_testTableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
        m_testTableView->horizontalHeader()->setStretchLastSection(true);
        m_testTableView->verticalHeader()->setDefaultSectionSize(25);
        
        // 连接dataSaved信号，以便在数据保存成功后显示提示
        connect(m_testTableModel, &VectorTableModel::dataSaved, this, [dialog](int tableId, int rowIndex) {
            // 在对话框中创建一个临时标签来显示保存成功的消息
            QLabel* msgLabel = new QLabel(QString("数据已保存 - 表ID: %1, 行: %2").arg(tableId).arg(rowIndex + 1), dialog);
            msgLabel->setStyleSheet("background-color: #DFF2BF; color: #4F8A10; padding: 5px; border-radius: 3px;");
            msgLabel->setAlignment(Qt::AlignCenter);
            msgLabel->setFixedHeight(30);
            
            // 将标签添加到对话框布局的顶部
            QLayout* dialogLayout = dialog->layout();
            if (dialogLayout) {
                // 如果已经有消息标签，先移除它
                QLabel* oldLabel = dialog->findChild<QLabel*>("saveMessageLabel");
                if (oldLabel) {
                    dialogLayout->removeWidget(oldLabel);
                    delete oldLabel;
                }
                
                // 将新标签添加到布局的顶部
                QBoxLayout* boxLayout = qobject_cast<QBoxLayout*>(dialogLayout);
                if (boxLayout) {
                    boxLayout->insertWidget(0, msgLabel);
                }
            }
            
            // 设置对象名，以便后续查找
            msgLabel->setObjectName("saveMessageLabel");
            
            // 3秒后自动隐藏
            QTimer::singleShot(3000, msgLabel, &QLabel::hide);
        });
        
        // 创建按钮
        QPushButton* saveButton = new QPushButton("保存并关闭", dialog);
        QPushButton* closeButton = new QPushButton("关闭(不保存)", dialog);
        
        QHBoxLayout* buttonLayout = new QHBoxLayout();
        buttonLayout->addWidget(saveButton);
        buttonLayout->addWidget(closeButton);
        layout->addLayout(buttonLayout);
        
        // 连接按钮的点击信号
        connect(saveButton, &QPushButton::clicked, [this, dialog]() {
            // 保存数据到文件
            QString errorMessage;
            
            // 使用我们新添加的保存方法
            QMessageBox waitBox(QMessageBox::Information, "请稍候", "正在保存数据...", QMessageBox::NoButton, dialog);
            waitBox.setStandardButtons(QMessageBox::NoButton);
            QTimer::singleShot(100, &waitBox, &QMessageBox::accept);
            waitBox.exec();
            
            bool success = m_testTableModel->saveTable(errorMessage);
            
            if (success) {
                if (errorMessage.contains("没有检测到数据变更")) {
                    QMessageBox::information(dialog, "保存成功", errorMessage);
                } else {
                    QMessageBox::information(dialog, "保存成功", "数据已成功保存到文件。");
                }
                dialog->accept();
            } else {
                QMessageBox::warning(dialog, "保存失败", "保存数据时出错：" + errorMessage);
            }
        });
        
        connect(closeButton, &QPushButton::clicked, [dialog]() {
            // 询问用户是否确定不保存
            QMessageBox::StandardButton reply = QMessageBox::question(
                dialog, "确认关闭", "您确定要关闭而不保存修改吗？",
                QMessageBox::Yes | QMessageBox::No
            );
            
            if (reply == QMessageBox::Yes) {
                dialog->reject();
            }
        });
        
        // 显示对话框
        dialog->exec();
    }
    else
    {
        QMessageBox::warning(this, "警告", "请先打开或创建一个向量表！");
        dialog->deleteLater();
    }
}

void MainWindow::loadVectorTableWithModelView(int tableId)
{
    const QString funcName = "MainWindow::loadVectorTableWithModelView";
    qDebug() << funcName << " - 使用Model/View架构加载向量表，表ID:" << tableId;
    
    if (tableId <= 0) {
        qWarning() << funcName << " - 无效的表格ID:" << tableId;
        return;
    }
    
    // 获取当前Tab页签中的QWidget
    int currentTabIndex = m_vectorTabWidget->currentIndex();
    if (currentTabIndex < 0) {
        qWarning() << funcName << " - 没有活动的Tab页签";
        return; // 直接返回，不尝试创建新Tab
    }
    
    // 获取或创建当前Tab页签的内容Widget
    QWidget* tabWidget = m_vectorTabWidget->widget(currentTabIndex);
    if (!tabWidget) {
        qWarning() << funcName << " - 无法获取Tab页签的Widget";
        return;
    }
    
    // 清空现有布局
    if (tabWidget->layout()) {
        QLayoutItem* item;
        while ((item = tabWidget->layout()->takeAt(0)) != nullptr) {
            if (item->widget()) {
                item->widget()->deleteLater();
            }
            delete item;
        }
        delete tabWidget->layout();
    }
    
    // 创建新布局
    QVBoxLayout* layout = new QVBoxLayout(tabWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    
    // 创建表格视图
    QTableView* tableView = new QTableView(tabWidget);
    layout->addWidget(tableView);
    
    // 创建模型并关联到视图
    VectorTableModel* tableModel = new VectorTableModel(this);
    tableView->setModel(tableModel);
    
    // 加载表格数据
    if (tableModel->loadTable(tableId)) {
        qDebug() << funcName << " - 成功加载表格数据，表ID:" << tableId;
        
        // 创建并设置自定义代理
        VectorTableItemDelegate* delegate = new VectorTableItemDelegate(tableId, tableView);
        tableView->setItemDelegate(delegate);
        
        // 调整列宽以适应内容
        tableView->resizeColumnsToContents();
        
        // 设置表格样式
        tableView->setAlternatingRowColors(true);
        tableView->setSelectionBehavior(QAbstractItemView::SelectItems);
        tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
        tableView->horizontalHeader()->setStretchLastSection(true);
        tableView->verticalHeader()->setDefaultSectionSize(25);
        
        // 连接dataSaved信号，以便在数据保存成功后更新状态栏
        connect(tableModel, &VectorTableModel::dataSaved, this, [this](int tableId, int rowIndex) {
            QString message = QString("数据已保存 - 表ID: %1").arg(tableId);
            if (rowIndex >= 0) {
                message += QString(", 行: %1").arg(rowIndex + 1);
            }
            statusBar()->showMessage(message, 3000);
        });
        
        // 存储模型和视图的引用，以便后续使用
        tabWidget->setProperty("tableModel", QVariant::fromValue(tableModel));
        tabWidget->setProperty("tableView", QVariant::fromValue(tableView));
        
        // 更新状态栏
        statusBar()->showMessage(QString("已加载向量表 (Model/View): %1，列数: %2")
                               .arg(m_vectorTableSelector->currentText())
                               .arg(tableModel->columnCount()));
    } else {
        qWarning() << funcName << " - 加载表格数据失败，表ID:" << tableId;
        statusBar()->showMessage("加载向量表失败");
    }
} 