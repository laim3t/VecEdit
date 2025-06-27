// 添加单个管脚
void MainWindow::addSinglePin()
{
    qDebug() << "MainWindow::addSinglePin - 开始添加单个管脚";

    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        QMessageBox::warning(this, tr("警告"), tr("请先打开或创建一个项目数据库"));
        return;
    }

    // 弹出输入对话框
    bool ok;
    QString pinName = QInputDialog::getText(this, tr("添加管脚"),
                                            tr("请输入管脚名称:"), QLineEdit::Normal,
                                            "", &ok);
    if (ok && !pinName.isEmpty())
    {
        // 添加到数据库
        QList<QString> pins;
        pins << pinName;
        if (addPinsToDatabase(pins))
        {
            statusBar()->showMessage(tr("管脚 \"%1\" 添加成功").arg(pinName));

            // 刷新侧边导航栏
            refreshSidebarNavigator();
        }
    }
}

// 删除管脚
void MainWindow::deletePins()
{
    qDebug() << "MainWindow::deletePins - 开始删除管脚";

    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        QMessageBox::warning(this, tr("警告"), tr("请先打开或创建一个项目数据库"));
        return;
    }

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();

    // 查询现有管脚
    QSqlQuery query(db);
    query.exec("SELECT id, pin_name FROM pin_list ORDER BY pin_name");

    // 构建管脚列表
    QMap<int, QString> pinMap;
    while (query.next())
    {
        int id = query.value(0).toInt();
        QString name = query.value(1).toString();
        pinMap.insert(id, name);
    }

    if (pinMap.isEmpty())
    {
        QMessageBox::information(this, tr("提示"), tr("当前没有可删除的管脚"));
        return;
    }

    // 创建选择对话框
    QDialog dialog(this);
    dialog.setWindowTitle(tr("选择要删除的管脚"));
    dialog.setMinimumWidth(350);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QLabel *label = new QLabel(tr("请选择要删除的管脚:"), &dialog);
    layout->addWidget(label);

    // 使用带复选框的滚动区域
    QScrollArea *scrollArea = new QScrollArea(&dialog);
    scrollArea->setWidgetResizable(true);
    QWidget *scrollContent = new QWidget(scrollArea);
    QVBoxLayout *checkBoxLayout = new QVBoxLayout(scrollContent);

    QMap<int, QCheckBox *> checkBoxes;
    for (auto it = pinMap.begin(); it != pinMap.end(); ++it)
    {
        QCheckBox *checkBox = new QCheckBox(it.value(), scrollContent);
        checkBoxes[it.key()] = checkBox;
        checkBoxLayout->addWidget(checkBox);
    }
    scrollContent->setLayout(checkBoxLayout);
    scrollArea->setWidget(scrollContent);
    layout->addWidget(scrollArea);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);

    // 显示对话框并等待用户操作
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    // 收集选中的管脚
    QList<int> pinIdsToDelete;
    QStringList pinNamesToDelete;
    for (auto it = pinMap.begin(); it != pinMap.end(); ++it)
    {
        if (checkBoxes.value(it.key())->isChecked())
        {
            pinIdsToDelete.append(it.key());
            pinNamesToDelete.append(it.value());
        }
    }

    if (pinIdsToDelete.isEmpty())
    {
        QMessageBox::information(this, tr("提示"), tr("未选择任何管脚"));
        return;
    }

    // 检查管脚使用情况
    QMap<QString, QStringList> pinUsageMap; // pinName -> [table1, table2]
    QSqlQuery findUsageQuery(db);

    for (int pinId : pinIdsToDelete)
    {
        // 根据新的 schema, 检查 VectorTableColumnConfiguration, 仅当管脚可见时才视为"正在使用"
        // 修复：同时检查 'pin_id' 和 'pin_list_id'，以兼容新旧两种数据格式
        findUsageQuery.prepare(
            "SELECT m.table_name "
            "FROM VectorTableColumnConfiguration c "
            "JOIN VectorTableMasterRecord m ON c.master_record_id = m.id "
            "WHERE c.column_type = 'PIN_STATE_ID' AND c.IsVisible = 1 AND "
            "(json_extract(c.data_properties, '$.pin_list_id') = ? OR json_extract(c.data_properties, '$.pin_id') = ?)");
        findUsageQuery.addBindValue(pinId);
        findUsageQuery.addBindValue(pinId);

        if (findUsageQuery.exec())
        {
            while (findUsageQuery.next())
            {
                QString pinName = pinMap.value(pinId);
                QString tableName = findUsageQuery.value(0).toString();
                pinUsageMap[pinName].append(tableName);
            }
        }
        else
        {
            qWarning() << "检查管脚使用情况失败:" << findUsageQuery.lastError().text();
        }
    }

    // 根据使用情况弹出不同确认对话框
    if (!pinUsageMap.isEmpty())
    {
        // 如果有管脚被使用，显示详细警告，并阻止删除
        QString warningText = tr("警告：以下管脚正在被一个或多个向量表使用，无法删除：\n");
        for (auto it = pinUsageMap.begin(); it != pinUsageMap.end(); ++it)
        {
            warningText.append(tr("\n管脚 \"%1\" 被用于: %2").arg(it.key()).arg(it.value().join(", ")));
        }
        warningText.append(tr("\n\n请先从相应的向量表设置中移除这些管脚，然后再尝试删除。"));
        QMessageBox::warning(this, tr("无法删除管脚"), warningText);
        return;
    }

    // 如果没有管脚被使用，弹出标准删除确认
    QString confirmQuestion = tr("您确定要删除以下 %1 个管脚吗？\n\n%2\n\n此操作不可恢复，并将删除所有相关的配置（如Pin Settings, Group Settings等）。")
                                  .arg(pinNamesToDelete.size())
                                  .arg(pinNamesToDelete.join("\n"));
    if (QMessageBox::question(this, tr("确认删除"), confirmQuestion, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
    {
        return;
    }

    // 执行删除操作 (借鉴 PinSettingsDialog 的逻辑)
    db.transaction();
    bool success = true;
    QString errorMsg;

    try
    {
        QSqlQuery updateQuery(db);
        QSqlQuery deleteQuery(db);

        for (int pinId : pinIdsToDelete)
        {
            // 在删除管脚之前，更新所有使用该管脚的列，将它们设置为不可见
            updateQuery.prepare("UPDATE VectorTableColumnConfiguration SET IsVisible = 0 WHERE column_type = 'PIN_STATE_ID' AND json_extract(data_properties, '$.pin_list_id') = ?");
            updateQuery.addBindValue(pinId);
            if (!updateQuery.exec())
            {
                throw QString("无法将管脚列标记为不可见: " + updateQuery.lastError().text());
            }
            qDebug() << "Pin Deletion - Marked columns for pinId" << pinId << "as not visible.";

            // 注意: 这里的删除逻辑需要非常完整，确保所有关联数据都被清除
            // 1. 从 timeset_settings 删除
            deleteQuery.prepare("DELETE FROM timeset_settings WHERE pin_id = ?");
            deleteQuery.addBindValue(pinId);
            if (!deleteQuery.exec())
                throw QString(deleteQuery.lastError().text());

            // 2. 从 pin_group_members 删除
            deleteQuery.prepare("DELETE FROM pin_group_members WHERE pin_id = ?");
            deleteQuery.addBindValue(pinId);
            if (!deleteQuery.exec())
                throw QString(deleteQuery.lastError().text());

            // 3. 从 pin_settings 删除
            deleteQuery.prepare("DELETE FROM pin_settings WHERE pin_id = ?");
            deleteQuery.addBindValue(pinId);
            if (!deleteQuery.exec())
                throw QString(deleteQuery.lastError().text());

            // 4. 从 pin_list 删除 (主表)
            deleteQuery.prepare("DELETE FROM pin_list WHERE id = ?");
            deleteQuery.addBindValue(pinId);
            if (!deleteQuery.exec())
                throw QString(deleteQuery.lastError().text());
        }
    }
    catch (const QString &e)
    {
        success = false;
        errorMsg = e;
        db.rollback();
    }

    if (success)
    {
        db.commit();
        QMessageBox::information(this, tr("成功"), tr("已成功删除选中的管脚。"));
        // 刷新侧边导航栏和可能打开的视图
        refreshSidebarNavigator();
        // 如果有其他需要同步的视图，也在这里调用
    }
    else
    {
        QMessageBox::critical(this, tr("数据库错误"), tr("删除管脚时发生错误: %1").arg(errorMsg));
    }
}

bool MainWindow::addPinsToDatabase(const QList<QString> &pinNames)
{
    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        return false;
    }

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

void MainWindow::setupVectorTablePins()
{
    qDebug() << "MainWindow::setupVectorTablePins - 开始设置向量表管脚";

    // 获取当前选择的向量表
    if (m_vectorTableSelector->count() == 0 || m_vectorTableSelector->currentIndex() < 0)
    {
        QMessageBox::warning(this, "错误", "请先选择一个向量表");
        return;
    }
    int tableId = m_vectorTableSelector->currentData().toInt();
    QString tableName = m_vectorTableSelector->currentText();

    qDebug() << "MainWindow::setupVectorTablePins - 打开管脚设置对话框，表ID:" << tableId << "，表名:" << tableName;

    // 1. 获取当前列配置
    QList<Vector::ColumnInfo> oldColumns;
    if (m_useNewDataHandler)
    {
        oldColumns = m_robustDataHandler->getAllColumnInfo(tableId);
    }
    else
    {
        oldColumns = VectorDataHandler::instance().getAllColumnInfo(tableId);
    }

    // 2. 创建并显示管脚设置对话框
    VectorPinSettingsDialog dialog(tableId, tableName, this);

    if (dialog.exec() == QDialog::Accepted)
    {
        qDebug() << "MainWindow::setupVectorTablePins - 用户确认了管脚设置，调用数据迁移器。";
        // 3. 调用迁移器处理后续所有逻辑（比较、迁移、提示）
        DataMigrator::migrateDataIfNeeded(tableId, oldColumns, this);
    }
    else
    {
        qDebug() << "MainWindow::setupVectorTablePins - 用户取消了管脚设置。";
    }

    // 4. 无论成功、失败还是取消，都刷新UI以保证与数据库状态一致
    reloadAndRefreshVectorTable(tableId);
}

// 打开管脚设置对话框
void MainWindow::openPinSettingsDialog()
{
    qDebug() << "MainWindow::openPinSettingsDialog - 打开管脚设置对话框";

    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        QMessageBox::warning(this, "警告", "请先打开或创建一个项目数据库");
        return;
    }

    // 创建并显示管脚设置对话框
    PinSettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted)
    {
        qDebug() << "MainWindow::openPinSettingsDialog - 管脚设置已更新";
        // 刷新当前向量表（如果有）
        if (m_vectorTableSelector->count() > 0 && m_vectorTableSelector->currentIndex() >= 0)
        {
            onVectorTableSelectionChanged(m_vectorTableSelector->currentIndex());
        }
        // 不再显示第二个提示对话框，避免重复提示
    }
    else
    {
        qDebug() << "MainWindow::openPinSettingsDialog - 用户取消了管脚设置";
    }
}

void MainWindow::showPinGroupDialog()
{
    qDebug() << "MainWindow::showPinGroupDialog - 显示管脚分组对话框";

    // 检查是否有打开的数据库
    if (m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        qDebug() << "MainWindow::showPinGroupDialog - 没有打开的数据库";
        QMessageBox::warning(this, tr("警告"), tr("请先打开或创建项目"));
        return;
    }

    // 使用对话框管理器显示分组对话框
    bool success = m_dialogManager->showPinGroupDialog();

    if (success)
    {
        qDebug() << "MainWindow::showPinGroupDialog - 分组创建成功";
        statusBar()->showMessage(tr("管脚分组已成功添加"));
    }
    else
    {
        qDebug() << "MainWindow::showPinGroupDialog - 分组创建取消或失败";
    }
}

// Add the new private helper method
void MainWindow::updateBinaryHeaderColumnCount(int tableId)
{
    const QString funcName = "MainWindow::updateBinaryHeaderColumnCount";
    qDebug() << funcName << "- Attempting to update binary header column count for table ID:" << tableId;

    QString errorMessage;
    QList<Vector::ColumnInfo> columns;
    int dbSchemaVersion = -1;
    QString binaryFileNameBase; // Base name like "table_1_data.vbindata"

    DatabaseManager *dbManager = DatabaseManager::instance(); // Corrected: Pointer type
    if (!dbManager->isDatabaseConnected())
    { // Corrected: ->isDatabaseConnected()
        qWarning() << funcName << "- Database not open.";
        return;
    }
    QSqlDatabase db = dbManager->database();

    // 1. Get master record info (schema version, binary file name)
    QSqlQuery masterQuery(db);
    // Corrected: VectorTableMasterRecord table name, and use 'id' for tableId
    masterQuery.prepare("SELECT data_schema_version, binary_data_filename FROM VectorTableMasterRecord WHERE id = :tableId");
    masterQuery.bindValue(":tableId", tableId);

    if (!masterQuery.exec())
    {
        qWarning() << funcName << "- Failed to execute query for VectorTableMasterRecord:" << masterQuery.lastError().text();
        return;
    }

    if (masterQuery.next())
    {
        dbSchemaVersion = masterQuery.value("data_schema_version").toInt();        // Corrected column name
        binaryFileNameBase = masterQuery.value("binary_data_filename").toString(); // Corrected column name
    }
    else
    {
        qWarning() << funcName << "- No VectorTableMasterRecord found for table ID:" << tableId;
        return;
    }

    if (binaryFileNameBase.isEmpty())
    {
        qWarning() << funcName << "- Binary file name is empty in master record for table ID:" << tableId;
        return;
    }

    // 2. Get column configurations from DB to count actual columns
    QSqlQuery columnQuery(db);
    // Query to get the actual number of columns configured for this table in the database
    // This includes standard columns and any pin-specific columns
    // Corrected: VectorTableColumnConfiguration table name, master_record_id for tableId relation
    QString columnSql = QString(
        "SELECT COUNT(*) FROM VectorTableColumnConfiguration WHERE master_record_id = ?"); // 使用位置占位符

    if (!columnQuery.prepare(columnSql))
    {
        qWarning() << funcName << "- CRITICAL_WARNING: Failed to PREPARE query for actual column count. SQL:" << columnSql
                   << ". Error:" << columnQuery.lastError().text();
        return;
    }
    columnQuery.addBindValue(tableId); // 使用 addBindValue

    int actualColumnCount = 0;
    if (columnQuery.exec())
    {
        if (columnQuery.next())
        {
            actualColumnCount = columnQuery.value(0).toInt();
        }
        // No 'else' here, if query returns no rows, actualColumnCount remains 0, which is handled below.
    }
    else
    {
        qWarning() << funcName << "- CRITICAL_WARNING: Failed to EXECUTE query for actual column count. TableId:" << tableId
                   << ". SQL:" << columnSql << ". Error:" << columnQuery.lastError().text()
                   << "(Reason: DB query for actual column count failed after successful prepare)";
        return;
    }

    qDebug() << funcName << "- Actual column count from DB for tableId" << tableId << "is" << actualColumnCount;

    if (actualColumnCount == 0 && tableId > 0)
    {
        qWarning() << funcName << "- No columns found in DB configuration for table ID:" << tableId << ". Header update may not be meaningful (or it's a new table). Continuing.";
    }

    // 3. Construct full binary file path
    // QString projectDbPath = dbManager->getCurrentDatabasePath(); // Incorrect method
    QString projectDbPath = m_currentDbPath; // Use MainWindow's member
    QString projBinDataDir = Utils::PathUtils::getProjectBinaryDataDirectory(projectDbPath);
    QString binFilePath = projBinDataDir + QDir::separator() + binaryFileNameBase;

    QFile file(binFilePath);
    if (!file.exists())
    {
        qWarning() << funcName << "- Binary file does not exist, cannot update header:" << binFilePath;
        return;
    }

    if (!file.open(QIODevice::ReadWrite))
    {
        qWarning() << funcName << "- Failed to open binary file for ReadWrite:" << binFilePath << file.errorString();
        return;
    }

    BinaryFileHeader header;
    bool existingHeaderRead = Persistence::BinaryFileHelper::readBinaryHeader(&file, header);

    if (existingHeaderRead)
    {
        // Corrected member access to use column_count_in_file
        if (header.column_count_in_file == actualColumnCount && header.data_schema_version == dbSchemaVersion)
        {
            qDebug() << funcName << "- Header column count (" << header.column_count_in_file
                     << ") and schema version (" << header.data_schema_version
                     << ") already match DB. No update needed for table" << tableId;
            file.close();
            return;
        }
        // Preserve creation time and row count from existing header
        header.column_count_in_file = actualColumnCount; // Corrected to column_count_in_file
        header.data_schema_version = dbSchemaVersion;    // Update schema version from DB
        header.timestamp_updated = QDateTime::currentSecsSinceEpoch();
    }
    else
    {
        qWarning() << funcName << "- Failed to read existing header from" << binFilePath << ". This is unexpected if addNewVectorTable created it. Re-initializing header for update.";
        header.magic_number = Persistence::VEC_BINDATA_MAGIC;
        header.file_format_version = Persistence::CURRENT_FILE_FORMAT_VERSION;
        header.data_schema_version = dbSchemaVersion;
        header.row_count_in_file = 0;
        header.column_count_in_file = actualColumnCount; // Corrected to column_count_in_file
        header.timestamp_created = QDateTime::currentSecsSinceEpoch();
        header.timestamp_updated = QDateTime::currentSecsSinceEpoch();
        header.compression_type = 0;
        // Removed memset calls for header.reserved and header.future_use as they are not members
    }

    file.seek(0);
    if (Persistence::BinaryFileHelper::writeBinaryHeader(&file, header))
    {
        qInfo() << funcName << "- Successfully updated binary file header for table" << tableId
                << ". Path:" << binFilePath
                << ". New ColumnCount:" << actualColumnCount
                << ", SchemaVersion:" << dbSchemaVersion;
    }
    else
    {
        qWarning() << funcName << "- Failed to write updated binary file header for table" << tableId
                   << ". Path:" << binFilePath;
    }
    file.close();

    // Corrected cache invalidation method name
    if (m_useNewDataHandler)
    {
        qWarning() << "RobustVectorDataHandler::clearTableDataCache is not implemented yet.";
    }
    else
    {
        VectorDataHandler::instance().clearTableDataCache(tableId);
    }
    // Clearing data cache is often sufficient. If specific metadata cache for columns/schema
    // exists and needs explicit invalidation, that would require a specific method in VectorDataHandler.
    // For now, assuming clearTableDataCache() and subsequent reloads handle it.
}