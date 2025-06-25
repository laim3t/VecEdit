void MainWindow::showDatabaseViewDialog()
{
    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        QMessageBox::warning(this, tr("警告"), tr("请先打开或创建一个项目数据库"));
        return;
    }

    // 使用对话框管理器显示数据库视图对话框
    m_dialogManager->showDatabaseViewDialog();
}

bool MainWindow::showAddPinsDialog()
{
    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        return false;
    }

    // 使用对话框管理器显示添加管脚对话框
    bool success = m_dialogManager->showAddPinsDialog();
    if (success)
    {
        statusBar()->showMessage(tr("管脚添加成功"));
    }
    return success;
}

bool MainWindow::showTimeSetDialog(bool isNewTable)
{
    qDebug() << "MainWindow::showTimeSetDialog - 显示TimeSet设置对话框，isNewTable:" << isNewTable;

    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        QMessageBox::warning(this, "警告", "请先打开或创建一个项目数据库");
        return false;
    }

    // 创建TimeSet对话框，如果是新表则传递isInitialSetup=true参数
    TimeSetDialog dialog(this, isNewTable);

    // 显示对话框
    if (dialog.exec() == QDialog::Accepted)
    {
        qDebug() << "MainWindow::showTimeSetDialog - 用户确认TimeSet设置";

        // 强制刷新数据库连接，确保没有缓存问题
        QSqlDatabase db = DatabaseManager::instance()->database();
        if (db.isOpen())
        {
            qDebug() << "MainWindow::showTimeSetDialog - 刷新数据库缓存";
            db.transaction();
            db.commit();
        }

        // 刷新侧边导航栏
        refreshSidebarNavigator();

        // 如果波形图可见，更新波形图以反映T1R和周期的变化
        if (m_isWaveformVisible && m_waveformPlot)
        {
            qDebug() << "MainWindow::showTimeSetDialog - 更新波形图以反映TimeSet设置变更";

            // 短暂延迟以确保数据库变更已经完成
            QTimer::singleShot(100, this, [this]()
                               {
                qDebug() << "MainWindow::showTimeSetDialog - 延迟更新波形图";
                updateWaveformView(); });

            // 同时也立即更新一次
            updateWaveformView();
        }

        return true;
    }
    else
    {
        qDebug() << "MainWindow::showTimeSetDialog - 用户取消TimeSet设置";
        return false;
    }
}

void MainWindow::showPinSelectionDialog(int tableId, const QString &tableName)
{
    const QString funcName = "MainWindow::showPinSelectionDialog";
    qDebug() << funcName << " - 开始显示管脚选择对话框 for tableId:" << tableId << "Name:" << tableName;

    // 确保数据库已连接且表ID有效
    if (!DatabaseManager::instance()->isDatabaseConnected() || tableId <= 0)
    { // Corrected: ->isDatabaseConnected()
        qWarning() << funcName << " - 数据库未连接或表ID无效 (" << tableId << ")";
        return;
    }

    try
    {
        // Corrected: use m_dialogManager instance
        bool success = m_dialogManager->showPinSelectionDialog(tableId, tableName);

        if (success)
        {
            qInfo() << funcName << " - 管脚配置成功完成 for table ID:" << tableId;

            // 使用延迟更新二进制文件头的列计数，避免可能的闪退
            QTimer::singleShot(100, this, [this, tableId]()
                               {
                try {
                    qDebug() << "MainWindow::showPinSelectionDialog - 延迟更新二进制文件头";
                    updateBinaryHeaderColumnCount(tableId);
                } catch (const std::exception &e) {
                    qWarning() << "更新二进制文件头时出现异常: " << e.what();
                } catch (...) {
                    qWarning() << "更新二进制文件头时出现未知异常";
                } });

            // 使用延迟重新加载并刷新向量表视图以反映更改，避免可能的闪退
            QTimer::singleShot(200, this, [this, tableId]()
                               {
                try {
                    qDebug() << "MainWindow::showPinSelectionDialog - 延迟刷新向量表";
                    reloadAndRefreshVectorTable(tableId);
                } catch (const std::exception &e) {
                    qWarning() << "刷新向量表时出现异常: " << e.what();
                } catch (...) {
                    qWarning() << "刷新向量表时出现未知异常";
                } });
        }
        else
        {
            qWarning() << funcName << " - 管脚配置被取消或失败 for table ID:" << tableId;
            // 如果这是新表创建流程的一部分，并且管脚配置失败/取消，
            // 可能需要考虑是否回滚表的创建或进行其他清理。
            // 使用延迟重新加载以确保UI与DB（可能部分配置的）状态一致。
            QTimer::singleShot(200, this, [this, tableId]()
                               {
                try {
                    qDebug() << "MainWindow::showPinSelectionDialog - 延迟刷新向量表（取消情况）";
                    reloadAndRefreshVectorTable(tableId);
                } catch (const std::exception &e) {
                    qWarning() << "刷新向量表时出现异常: " << e.what();
                } catch (...) {
                    qWarning() << "刷新向量表时出现未知异常";
                } });
        }
    }
    catch (const std::exception &e)
    {
        qWarning() << funcName << " - 显示管脚选择对话框过程中出现异常: " << e.what();
        QMessageBox::critical(this, "错误", QString("无法显示管脚选择对话框: %1").arg(e.what()));
    }
    catch (...)
    {
        qWarning() << funcName << " - 显示管脚选择对话框过程中出现未知异常";
        QMessageBox::critical(this, "错误", "无法显示管脚选择对话框: 发生未知错误");
    }
}

void MainWindow::showVectorDataDialog(int tableId, const QString &tableName, int startIndex)
{
    const QString funcName = "MainWindow::showVectorDataDialog";
    qDebug() << funcName << " - 开始显示向量行数据录入对话框";

    if (m_dialogManager)
    {
        qDebug() << funcName << " - 调用对话框管理器显示向量行数据录入对话框";
        bool success = m_dialogManager->showVectorDataDialog(tableId, tableName, startIndex);
        qDebug() << funcName << " - 向量行数据录入对话框返回结果:" << success;

        // 如果成功添加了数据，刷新表格显示
        if (success)
        {
            qDebug() << funcName << " - 成功添加向量行数据，准备刷新表格";

            // 找到对应的表索引并刷新
            int currentIndex = m_vectorTableSelector->findData(tableId);
            if (currentIndex >= 0)
            {
                qDebug() << funcName << " - 找到表索引:" << currentIndex << "，设置为当前表";

                // 先保存当前的列状态
                m_vectorTableSelector->setCurrentIndex(currentIndex);

                // 强制调用loadVectorTableData而不是依赖信号槽，确保正确加载所有列
                if (VectorDataHandler::instance().loadVectorTableData(tableId, m_vectorTableWidget))
                {
                    qDebug() << funcName << " - 成功重新加载表格数据，列数:" << m_vectorTableWidget->columnCount();
                    // 应用表格样式（优化版本，一次性完成所有样式设置，包括列宽和对齐）
                    TableStyleManager::applyBatchTableStyle(m_vectorTableWidget);

                    // 输出每一列的标题，用于调试
                    QStringList columnHeaders;
                    for (int i = 0; i < m_vectorTableWidget->columnCount(); i++)
                    {
                        QTableWidgetItem *headerItem = m_vectorTableWidget->horizontalHeaderItem(i);
                        QString headerText = headerItem ? headerItem->text() : QString("列%1").arg(i);
                        columnHeaders << headerText;
                    }
                    qDebug() << funcName << " - 表头列表:" << columnHeaders.join(", ");
                }
                else
                {
                    qWarning() << funcName << " - 重新加载表格数据失败";
                }
            }
            else
            {
                qWarning() << funcName << " - 未找到表ID:" << tableId << "对应的索引";
            }
        }
    }
}

// 打开TimeSet设置对话框
void MainWindow::openTimeSetSettingsDialog()
{
    const QString funcName = "MainWindow::openTimeSetSettingsDialog"; // 添加 funcName 定义
    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        QMessageBox::warning(this, "警告", "请先打开或创建一个项目数据库");
        return;
    }

    // 显示TimeSet设置对话框
    if (showTimeSetDialog(false))
    {
        // 如果有修改，刷新界面
        if (m_itemDelegate)
        {
            m_itemDelegate->refreshCache();
        }

        // 重新加载当前向量表数据
        int currentIndex = m_vectorTableSelector->currentIndex();
        if (currentIndex >= 0)
        {
            // int tableId = m_vectorTableSelector->itemData(currentIndex).toInt(); // 获取 tableId 以便在日志中使用（如果需要）
            qDebug() << funcName << " - TimeSet设置已更新，将刷新当前选择的向量表，下拉框索引为: " << currentIndex; // 修正日志，移除未定义的 tableId
            m_vectorTableSelector->setCurrentIndex(currentIndex);
            // onVectorTableSelectionChanged(currentIndex); // setCurrentIndex会触发信号
        }

        statusBar()->showMessage("TimeSet设置已更新");
    }
}

// 显示管脚列的右键菜单
void MainWindow::showPinColumnContextMenu(const QPoint &pos)
{
    if (!m_vectorTableWidget)
        return;

    // 获取右键点击的单元格位置
    QTableWidgetItem *item = m_vectorTableWidget->itemAt(pos);
    if (!item)
        return;

    int row = item->row();
    int col = item->column();

    // 获取当前表ID
    int currentTableId = m_vectorTableSelector->currentData().toInt();
    QList<Vector::ColumnInfo> columns = getCurrentColumnConfiguration(currentTableId);

    // 检查是否是管脚列
    bool isPinColumn = false;
    if (col < columns.size() && columns[col].type == Vector::ColumnDataType::PIN_STATE_ID)
    {
        isPinColumn = true;
    }

    // 如果不是管脚列，不显示菜单
    if (!isPinColumn)
        return;

    QMenu contextMenu(this);

    // 添加"跳转至波形图"选项
    if (m_isWaveformVisible)
    {
        QString pinName = m_vectorTableWidget->horizontalHeaderItem(col)->text();
        QAction *jumpToWaveformAction = contextMenu.addAction(tr("跳转至波形图"));
        connect(jumpToWaveformAction, &QAction::triggered, this, [this, row, pinName]()
                { jumpToWaveformPoint(row, pinName); });

        // 添加分隔线
        contextMenu.addSeparator();
    }

    QAction *fillVectorAction = contextMenu.addAction(tr("向量填充"));
    connect(fillVectorAction, &QAction::triggered, this, [this]()
            {
        // 直接调用MainWindow的showFillVectorDialog方法
        this->showFillVectorDialog(); });

    contextMenu.exec(m_vectorTableWidget->viewport()->mapToGlobal(pos));
}