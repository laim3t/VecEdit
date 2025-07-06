void MainWindow::setupMenu()
{
    // ... 现有代码 ...

    // 添加"工具"菜单
    QMenu *toolsMenu = menuBar()->addMenu(tr("工具"));
    
    // 添加"优化存储空间"菜单项
    QAction *optimizeStorageAction = new QAction(tr("优化存储空间"), this);
    connect(optimizeStorageAction, &QAction::triggered, this, [this]() {
        // 获取当前表ID
        int currentTableId = getCurrentVectorTableId();
        if (currentTableId <= 0) {
            QMessageBox::warning(this, tr("警告"), tr("请先打开一个向量表"));
            return;
        }
        
        // 执行垃圾回收
        QString errorMsg;
        
        // 检查无效数据比例
        double invalidDataRatio = 0.0;
        if (m_robustDataHandler->needsGarbageCollection(currentTableId, invalidDataRatio, errorMsg)) {
            // 显示确认对话框
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this, 
                                         tr("确认优化存储空间"), 
                                         tr("检测到约 %1% 的存储空间被无效数据占用。\n优化将临时占用额外存储空间，是否继续?")
                                         .arg(QString::number(invalidDataRatio * 100, 'f', 1)),
                                         QMessageBox::Yes | QMessageBox::No);
            
            if (reply == QMessageBox::Yes) {
                // 显示进度对话框
                QProgressDialog progressDialog(tr("正在优化存储空间..."), tr("取消"), 0, 100, this);
                progressDialog.setWindowModality(Qt::WindowModal);
                progressDialog.setMinimumDuration(0);
                progressDialog.show();
                
                // 连接进度更新信号
                connect(m_robustDataHandler, &RobustVectorDataHandler::progressUpdated,
                        &progressDialog, &QProgressDialog::setValue);
                        
                // 执行垃圾回收
                if (m_robustDataHandler->collectGarbage(currentTableId, true, errorMsg)) {
                    // 成功
                    QMessageBox::information(this, tr("优化完成"), 
                                           tr("存储空间优化成功，无效数据已清除，文件已压缩。"));
                } else {
                    // 失败
                    QMessageBox::critical(this, tr("优化失败"), 
                                        tr("存储空间优化失败: %1").arg(errorMsg));
                }
                
                // 断开信号连接
                disconnect(m_robustDataHandler, &RobustVectorDataHandler::progressUpdated,
                          &progressDialog, &QProgressDialog::setValue);
            }
        } else {
            QMessageBox::information(this, tr("无需优化"), 
                                   tr("当前文件的无效数据比例为 %1%，低于优化阈值，无需优化。")
                                   .arg(QString::number(invalidDataRatio * 100, 'f', 1)));
        }
    });
    toolsMenu->addAction(optimizeStorageAction);
    
    // ... 现有代码 ...
}

// 获取当前向量表ID
int MainWindow::getCurrentVectorTableId()
{
    int currentTabIndex = ui_tabWidget->currentIndex();
    if (currentTabIndex < 0 || ui_tabWidget->count() == 0)
        return -1;
    
    // 获取当前Tab的Widget
    QWidget *currentTab = ui_tabWidget->widget(currentTabIndex);
    
    // 从Tab的属性中获取表ID
    QVariant tableIdVar = currentTab->property("tableId");
    if (!tableIdVar.isValid())
        return -1;
    
    return tableIdVar.toInt();
} 