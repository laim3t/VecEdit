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

bool MainWindow::showVectorDataDialog(QSqlDatabase& db, int tableId, const QString &tableName, int startIndex)
{
    qDebug() << "--- [Wizard Step] Entering 'showVectorDataDialog' for table ID:" << tableId;

    if (!m_dialogManager) {
        qDebug() << "--- [Wizard Step] ERROR: m_dialogManager is null!";
        QMessageBox::critical(this, "错误", "无法显示向量数据录入对话框：对话框管理器未初始化");
        return false;
    }

    // 传递数据库连接到对话框管理器
    bool success = m_dialogManager->showVectorDataDialog(db, tableId, tableName, startIndex);

    if (!success) {
        qDebug() << "--- [Wizard Step] DialogManager::showVectorDataDialog reported failure or cancellation.";
    }

    return success;
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
    qDebug() << "--- [Wizard Step] Entering 'showAddPinsDialog'.";
    
    // 检查数据库连接
    if (!DatabaseManager::instance()->isDatabaseConnected()) {
        QMessageBox::critical(this, "错误", "数据库未连接，无法管理管脚。");
        return false;
    }

    QSqlDatabase db = DatabaseManager::instance()->database();

    // **关键修复：由调用者开启和管理事务**
    if (!db.transaction()) {
        qCritical() << "Failed to start transaction in showAddPinsDialog: " << db.lastError().text();
        QMessageBox::critical(this, "数据库错误", "无法为管脚设置开启事务。");
        return false;
    }
    qDebug() << "Transaction started by showAddPinsDialog.";

    // 创建并显示管脚列表对话框
    PinListDialog dialog(this);
    int result = dialog.exec();

    // 检查用户是点击了"确定"还是"取消"
    if (result == QDialog::Accepted) {
        qDebug() << "--- [Wizard Step] PinListDialog accepted by user.";
        
        // 用户点击了OK，并且PinListDialog内部操作成功
        qDebug() << "PinListDialog accepted. Committing transaction.";
        if (!db.commit()) {
            qCritical() << "Failed to commit transaction in showAddPinsDialog: " << db.lastError().text();
            QMessageBox::critical(this, "数据库错误", "提交管脚设置失败。");
            db.rollback(); // 尝试回滚
            return false;
        }
        
        // 刷新侧边栏，以防管脚列表有变
        refreshSidebarNavigator();
        return true; // 用户确认了管脚设置，向导可以继续
    } else {
        qDebug() << "--- [Wizard Step] PinListDialog cancelled by user. Aborting project creation.";
        // 用户点击了Cancel，或者PinListDialog内部操作失败
        qDebug() << "PinListDialog rejected or failed. Rolling back transaction.";
        db.rollback();
        return false; // 用户取消了第一步，整个"新建项目"流程都应该终止
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
    // 这是一个核心函数，它负责在用户从下拉列表选择一个新的向量表时，更新模型。
    // 经过优化，此函数不再自动加载数据，而是将该责任交给调用者。
    qDebug() << "[UI_REFRESH_DEBUG] onVectorTableSelectionChanged called with index:" << index;
    
    if (index < 0 || index >= m_vectorTableSelector->count())
    {
        if (m_vectorTableModel) {
            m_vectorTableModel->setTable(-1); // 重置模型到无效状态
        }
        updateWindowTitle("VecEdit"); // 重置窗口标题
        return;
    }

    m_currentTableId = m_vectorTableSelector->itemData(index).toInt();
    m_currentTableName = m_vectorTableSelector->itemText(index);
    qDebug() << "[UI_REFRESH_DEBUG] Selected table ID:" << m_currentTableId << ", name:" << m_currentTableName;

    // 让新的数据模型知道要处理哪个表
    if (m_vectorTableModel)
    {
        m_vectorTableModel->setTable(m_currentTableId);
    }

    // 重置页码但不加载数据，加载的责任交给调用者
    m_currentPage = 0;
    updatePaginationInfo(); // 更新分页信息
    
    // 更新窗口标题
    updateWindowTitle(m_currentProjectName + " - " + m_currentTableName);
    
    // 刷新侧边栏和波形视图
    refreshSidebarNavigator();
    updateWaveformView();
}

void MainWindow::onTabChanged(int index)
{
    qDebug() << "STUB: onTabChanged(int) called. Re-implementation needed.";
}

bool MainWindow::showPinSelectionDialog(int tableId, const QString &tableName)
{
    qDebug() << "--- [Wizard Step] Entering 'showPinSelectionDialog' for table ID:" << tableId;

    if (!m_dialogManager) {
        qDebug() << "--- [Wizard Step] ERROR: m_dialogManager is null!";
        QMessageBox::critical(this, "错误", "无法显示管脚选择对话框：对话框管理器未初始化");
        return false;
    }

    // 直接调用，不管理事务。成功与否完全由DialogManager返回。
    bool success = m_dialogManager->showPinSelectionDialog(tableId, tableName);

    if (success) {
        qDebug() << "--- [Wizard Step] DialogManager::showPinSelectionDialog reported success. Refreshing UI.";
        // 如果操作成功且当前显示的就是这个表，则刷新UI
        if (m_vectorTableSelector->currentData().toInt() == tableId) {
            onVectorTableSelectionChanged(m_vectorTableSelector->currentIndex());
        }
    } else {
        qDebug() << "--- [Wizard Step] DialogManager::showPinSelectionDialog reported failure or cancellation.";
    }

    qDebug() << "--- [Wizard Step] Exiting 'showPinSelectionDialog'.";
    return success;
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
