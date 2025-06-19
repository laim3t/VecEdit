void MainWindow::onFontZoomSliderValueChanged(int value)
{
    if (m_vectorTableView)
    {
        // 实现字体缩放逻辑
    }
}

void MainWindow::onFontZoomReset()
{
    if (m_vectorTableView)
    {
        // 实现字体缩放重置逻辑
    }
}

void MainWindow::syncComboBoxWithTab(int index)
{
    // 新模型下，此函数逻辑已简化或合并到 onVectorTableSelectionChanged
}

void MainWindow::gotoLine()
{
    // 实现跳转到行逻辑
}

int MainWindow::showVectorDataDialog(int tableId, const QString &tableName, int startIndex)
{
    qDebug() << "--- [Wizard Step] Entering 'showVectorDataDialog' for table ID:" << tableId << ". Replacing STUB with real implementation.";
    
    // 使用DialogManager来处理向量数据录入
    if (m_dialogManager) {
        int rowCount = m_dialogManager->showVectorDataDialog(tableId, tableName, startIndex);
        qDebug() << "--- [Wizard Step] Completed DialogManager::showVectorDataDialog call. Returned row count:" << rowCount;
        return rowCount;
    } else {
        qDebug() << "--- [Wizard Step] ERROR: m_dialogManager is null!";
        QMessageBox::critical(this, "错误", "无法显示向量数据录入对话框：对话框管理器未初始化");
        return -1;
    }
}

void MainWindow::refreshSidebarNavigator()
{
    // 实现侧边栏刷新逻辑
}

void MainWindow::onPinItemClicked(QTreeWidgetItem *item, int column) {}
void MainWindow::onTimeSetItemClicked(QTreeWidgetItem *item, int column) {}
void MainWindow::onLabelItemClicked(QTreeWidgetItem *item, int column) {}
void MainWindow::showPinColumnContextMenu(const QPoint &pos) {}
void MainWindow::jumpToWaveformPoint(int vectorIndex, const QString &pinName) {}
void MainWindow::updateVectorColumnProperties(int column, int type) {}
void MainWindow::calculateAndDisplayHexValue(const QList<int> &selectedRows, int column) {}
void MainWindow::validateHexInput(const QString &text) {}

// =================================================================
// 补全之前被错误删除的函数定义，以解决链接器错误
// =================================================================

void MainWindow::showDatabaseViewDialog() 
{
    qDebug() << "STUB: showDatabaseViewDialog() called. Re-implementation needed.";
}

bool MainWindow::showAddPinsDialog()
{
    qDebug() << "--- [Wizard Step] Entering 'showAddPinsDialog'. Replacing STUB with real implementation.";
    
    // 检查数据库连接
    if (!DatabaseManager::instance()->isDatabaseConnected()) {
        QMessageBox::critical(this, "错误", "数据库未连接，无法管理管脚。");
        return false;
    }

    // 创建并显示管脚列表对话框
    PinListDialog dialog(this);
    int result = dialog.exec();

    // 检查用户是点击了"确定"还是"取消"
    if (result == QDialog::Accepted) {
        qDebug() << "--- [Wizard Step] PinListDialog accepted by user.";
        // 刷新侧边栏，以防管脚列表有变
        refreshSidebarNavigator();
        return true; // 用户确认了管脚设置，向导可以继续
    } else {
        qDebug() << "--- [Wizard Step] PinListDialog cancelled by user. Aborting project creation.";
        // 用户取消了第一步，整个"新建项目"流程都应该终止
        return false;
    }
}

bool MainWindow::showTimeSetDialog(bool create_new)
{
    qDebug() << "--- [Wizard Step] Entering 'showTimeSetDialog'. Replacing STUB with real implementation.";
    
    // The 'create_new' parameter is used to indicate if this is part of the initial project setup
    TimeSetDialog dialog(this, create_new);
    if (dialog.exec() == QDialog::Accepted) {
        qDebug() << "--- [Wizard Step] TimeSetDialog accepted by user.";
        refreshSidebarNavigator(); // Refresh sidebar in case TimeSets changed
        return true; // User confirmed the settings, the wizard can continue.
    } else {
        qDebug() << "--- [Wizard Step] TimeSetDialog cancelled by user. Aborting project creation.";
        return false; // User cancelled, the entire wizard should stop.
    }
}

void MainWindow::onVectorTableSelectionChanged(int index)
{
    // 这是一个核心函数，在之前的重构中被意外删除。
    // 它负责在用户从下拉列表选择一个新的向量表时，更新所有相关的UI和数据。
    if (index < 0 || index >= m_vectorTableSelector->count())
    {
        return;
    }

    m_currentTableId = m_vectorTableSelector->itemData(index).toInt();
    m_currentTableName = m_vectorTableSelector->itemText(index);
    qDebug() << "MainWindow::onVectorTableSelectionChanged - a vector table was selected:" << m_currentTableName << "with tableId" << m_currentTableId;

    // 让新的数据模型知道要处理哪个表
    if (m_vectorTableModel)
    {
        m_vectorTableModel->setTable(m_currentTableId);
    }

    // 加载第一页数据
    m_currentPage = 0;
    loadCurrentPage(); // 这个函数现在应该可以正确工作了
    
    // 更新其他UI组件
    updateWindowTitle(m_currentProjectName + " - " + m_currentTableName);
    refreshSidebarNavigator();
    updateWaveformView();
}

void MainWindow::onTabChanged(int index)
{
    qDebug() << "STUB: onTabChanged(int) called. Re-implementation needed.";
}

void MainWindow::showPinSelectionDialog(int tableId, const QString &tableName)
{
    qDebug() << "--- [Wizard Step] Entering 'showPinSelectionDialog' for table ID:" << tableId << ". Replacing STUB with real implementation.";

    // 使用DialogManager来处理管脚选择
    if (m_dialogManager) {
        m_dialogManager->showPinSelectionDialog(tableId, tableName);
        qDebug() << "--- [Wizard Step] Completed DialogManager::showPinSelectionDialog call.";
    } else {
        qDebug() << "--- [Wizard Step] ERROR: m_dialogManager is null!";
        QMessageBox::critical(this, "错误", "无法显示管脚选择对话框：对话框管理器未初始化");
    }

    qDebug() << "--- [Wizard Step] Exiting 'showPinSelectionDialog'.";

    // After pins are potentially selected, we should refresh the UI
    // in case this table is the currently active one.
    if (m_vectorTableSelector->currentData().toInt() == tableId) {
        onVectorTableSelectionChanged(m_vectorTableSelector->currentIndex());
    }
}

void MainWindow::openTimeSetSettingsDialog()
{
    qDebug() << "STUB: openTimeSetSettingsDialog() called. Re-implementation needed.";
}

void MainWindow::closeTab(int index)
{
    qDebug() << "STUB: closeTab(int) called. Re-implementation needed.";
}

void MainWindow::jumpToPage(int page)
{
    if (page >= 0 && page < m_totalPages)
    {
        m_currentPage = page;
        loadCurrentPage();
    }
}

void MainWindow::onSidebarItemClicked(QTreeWidgetItem *item, int column)
{
    qDebug() << "STUB: onSidebarItemClicked(QTreeWidgetItem*, int) called. Re-implementation needed.";
}

void MainWindow::onVectorTableItemClicked(QTreeWidgetItem *item, int column)
{
    qDebug() << "STUB: onVectorTableItemClicked(QTreeWidgetItem*, int) called. This is likely obsolete.";
}

void MainWindow::on_action_triggered(bool checked)
{
    qDebug() << "STUB: on_action_triggered(bool) called. Re-implementation needed.";
}

void MainWindow::onProjectStructureItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    qDebug() << "STUB: onProjectStructureItemDoubleClicked(QTreeWidgetItem*, int) called. Re-implementation needed.";
}

void MainWindow::updateWindowTitle(const QString &title)
{
    this->setWindowTitle(title);
}

void MainWindow::on_actionNewProject_triggered()
{
    qDebug() << "on_actionNewProject_triggered - 调用createNewProject()开始新项目流程";
    
    // 简化实现：直接调用核心方法创建新项目
    // 由于原有代码中的成员变量/方法不存在，暂时使用一个简单的实现
    createNewProject(); // 调用MainWindow类中已存在的方法
    
    qDebug() << "on_actionNewProject_triggered - 完成";
}