// 检查行是否被修改过
bool VectorDataHandler::isRowModified(int tableId, int rowIndex)
{
    // 当rowIndex为-1时，检查整个表是否有任何修改
    if (rowIndex == -1)
    {
        return m_modifiedRows.contains(tableId) && !m_modifiedRows[tableId].isEmpty();
    }

    // 检查特定行是否被修改
    return m_modifiedRows.contains(tableId) && m_modifiedRows[tableId].contains(rowIndex);
}

bool VectorDataHandler::gotoLine(int tableId, int lineNumber)
{
    qDebug() << "VectorDataHandler::gotoLine - 准备跳转到向量表" << tableId << "的第" << lineNumber << "行";

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qDebug() << "VectorDataHandler::gotoLine - 错误：数据库未打开";
        return false;
    }

    // 首先检查表格是否存在
    QSqlQuery tableCheckQuery(db);
    tableCheckQuery.prepare("SELECT table_name FROM vector_tables WHERE id = ?");
    tableCheckQuery.addBindValue(tableId);

    if (!tableCheckQuery.exec() || !tableCheckQuery.next())
    {
        qDebug() << "VectorDataHandler::gotoLine - 错误：找不到指定的向量表 ID:" << tableId;
        return false;
    }

    QString tableName = tableCheckQuery.value(0).toString();
    qDebug() << "VectorDataHandler::gotoLine - 向量表名称:" << tableName;

    // 检查行号是否有效
    int totalRows = getVectorTableRowCount(tableId);
    if (lineNumber < 1 || lineNumber > totalRows)
    {
        qDebug() << "VectorDataHandler::gotoLine - 错误：行号" << lineNumber << "超出范围（1-" << totalRows << "）";
        return false;
    }

    // 找到对应的向量数据ID
    QSqlQuery dataQuery(db);
    dataQuery.prepare("SELECT id FROM vector_table_data WHERE table_id = ? ORDER BY sort_index LIMIT 1 OFFSET ?");
    dataQuery.addBindValue(tableId);
    dataQuery.addBindValue(lineNumber - 1); // 数据库OFFSET是0-based，而行号是1-based

    if (!dataQuery.exec() || !dataQuery.next())
    {
        qDebug() << "VectorDataHandler::gotoLine - 错误：无法获取第" << lineNumber << "行的数据 ID";
        qDebug() << "SQL错误：" << dataQuery.lastError().text();
        return false;
    }

    int dataId = dataQuery.value(0).toInt();
    qDebug() << "VectorDataHandler::gotoLine - 找到第" << lineNumber << "行的数据 ID:" << dataId;

    // 如果需要滚动到指定行，可以在这里记录dataId，然后通过UI组件使用这个ID来定位和滚动
    // 但在大多数情况下，我们只需要知道行数，因为我们会在UI层面使用selectRow方法来选中行

    qDebug() << "VectorDataHandler::gotoLine - 跳转成功：表" << tableName << "的第" << lineNumber << "行";
    return true;
}

bool VectorDataHandler::hideVectorTableColumn(int tableId, const QString &columnName, QString &errorMessage)
{
    const QString funcName = "VectorDataHandler::hideVectorTableColumn";
    qDebug() << funcName << " - 开始逻辑删除列, 表ID:" << tableId << ", 列名:" << columnName;

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        errorMessage = "数据库未打开";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 检查列是否存在
    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT id FROM VectorTableColumnConfiguration WHERE master_record_id = ? AND column_name = ?");
    checkQuery.addBindValue(tableId);
    checkQuery.addBindValue(columnName);

    if (!checkQuery.exec())
    {
        errorMessage = "查询列信息失败: " + checkQuery.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    if (!checkQuery.next())
    {
        errorMessage = QString("找不到表 %1 中的列 '%2'").arg(tableId).arg(columnName);
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    int columnId = checkQuery.value(0).toInt();
    qDebug() << funcName << " - 找到列ID:" << columnId;

    // 更新IsVisible字段为0（逻辑删除）
    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE VectorTableColumnConfiguration SET IsVisible = 0 WHERE id = ?");
    updateQuery.addBindValue(columnId);

    if (!updateQuery.exec())
    {
        errorMessage = "更新列可见性失败: " + updateQuery.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    qDebug() << funcName << " - 成功将列 '" << columnName << "' 标记为不可见 (IsVisible=0)";

    // 清除该表的缓存，确保下次加载时获取最新数据
    clearTableDataCache(tableId);
    qDebug() << funcName << " - 已清除表 " << tableId << " 的缓存数据";

    return true;
}

bool VectorDataHandler::showVectorTableColumn(int tableId, const QString &columnName, QString &errorMessage)
{
    const QString funcName = "VectorDataHandler::showVectorTableColumn";
    qDebug() << funcName << " - 开始恢复列显示, 表ID:" << tableId << ", 列名:" << columnName;

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        errorMessage = "数据库未打开";
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    // 检查列是否存在
    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT id FROM VectorTableColumnConfiguration WHERE master_record_id = ? AND column_name = ?");
    checkQuery.addBindValue(tableId);
    checkQuery.addBindValue(columnName);

    if (!checkQuery.exec())
    {
        errorMessage = "查询列信息失败: " + checkQuery.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    if (!checkQuery.next())
    {
        errorMessage = QString("找不到表 %1 中的列 '%2'").arg(tableId).arg(columnName);
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    int columnId = checkQuery.value(0).toInt();
    qDebug() << funcName << " - 找到列ID:" << columnId;

    // 更新IsVisible字段为1（恢复显示）
    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE VectorTableColumnConfiguration SET IsVisible = 1 WHERE id = ?");
    updateQuery.addBindValue(columnId);

    if (!updateQuery.exec())
    {
        errorMessage = "更新列可见性失败: " + updateQuery.lastError().text();
        qWarning() << funcName << " - " << errorMessage;
        return false;
    }

    qDebug() << funcName << " - 成功将列 '" << columnName << "' 标记为可见 (IsVisible=1)";
    return true;
}

void VectorDataHandler::addVectorRow(QTableWidget *table, const QStringList &pinOptions, int rowIdx)
{
    table->setRowCount(rowIdx + 1);

    // 添加每个管脚的文本输入框
    for (int col = 0; col < table->columnCount(); col++)
    {
        PinValueLineEdit *pinEdit = new PinValueLineEdit(table);

        // 默认设置为"X"
        pinEdit->setText("X");

        table->setCellWidget(rowIdx, col, pinEdit);
    }
}

void VectorDataHandler::addVectorRows(QTableWidget *table, const QStringList &pinOptions, int startRowIdx, int count)
{
    const QString funcName = "VectorDataHandler::addVectorRows";
    qDebug() << funcName << "- 开始批量添加" << count << "行，从索引" << startRowIdx << "开始";

    if (count <= 0)
    {
        qDebug() << funcName << "- 要添加的行数必须大于0，当前值:" << count;
        return;
    }

    // 预先设置行数，避免多次调整
    int newRowCount = startRowIdx + count;
    table->setRowCount(newRowCount);

    // 禁用表格更新，提高性能
    table->setUpdatesEnabled(false);

    // 批量添加行
    for (int row = startRowIdx; row < newRowCount; row++)
    {
        // 添加每个管脚的文本输入框
        for (int col = 0; col < table->columnCount(); col++)
        {
            PinValueLineEdit *pinEdit = new PinValueLineEdit(table);
            pinEdit->setText("X");
            table->setCellWidget(row, col, pinEdit);
        }

        // 每处理100行，让出一些CPU时间，避免UI完全冻结
        if ((row - startRowIdx) % 100 == 0 && row > startRowIdx)
        {
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        }
    }

    // 恢复表格更新
    table->setUpdatesEnabled(true);

    qDebug() << funcName << "- 批量添加完成，总行数:" << table->rowCount();
}

/**
 * @brief 私有辅助函数：解析给定表ID的二进制文件的绝对路径。
 *
 * @param tableId 向量表的ID。
 * @param[out] errorMsg 如果发生错误，将填充错误消息。
 * @return QString 如果成功，则为二进制文件的绝对路径；否则为空字符串。
 */
QString VectorDataHandler::resolveBinaryFilePath(int tableId, QString &errorMsg)
{
    const QString funcName = "VectorDataHandler::resolveBinaryFilePath";
    qDebug() << funcName << " - 开始解析表ID的二进制文件路径:" << tableId;

    // 1. 获取该表存储的纯二进制文件名
    QString justTheFileName;
    QList<Vector::ColumnInfo> columns; // We don't need columns here, but loadVectorTableMeta requires it
    int schemaVersion = 0;
    int rowCount = 0; // Not needed here either

    if (!loadVectorTableMeta(tableId, justTheFileName, columns, schemaVersion, rowCount))
    {
        errorMsg = QString("无法加载表 %1 的元数据以获取二进制文件名").arg(tableId);
        qWarning() << funcName << " - " << errorMsg;
        return QString();
    }

    // 检查获取到的文件名是否为空
    if (justTheFileName.isEmpty())
    {
        errorMsg = QString("表 %1 的元数据中缺少二进制文件名").arg(tableId);
        qWarning() << funcName << " - " << errorMsg;
        // 这可能表示一个新创建但尚未完全初始化的表，或者一个损坏的记录。
        // 根据业务逻辑，可能需要返回错误或允许创建/处理。
        // 对于加载和保存，缺少文件名通常是一个错误。
        return QString();
    }

    // 检查文件名是否包含路径分隔符 (指示旧格式或错误数据)
    if (justTheFileName.contains('\\') || justTheFileName.contains('/'))
    {
        qWarning() << funcName << " - 表 " << tableId << " 的二进制文件名 '" << justTheFileName
                   << "' 包含路径分隔符，这可能是旧格式或错误数据。尝试提取文件名部分。";
        // 尝试仅提取文件名部分作为临时解决方案
        QFileInfo fileInfo(justTheFileName);
        justTheFileName = fileInfo.fileName();
        if (justTheFileName.isEmpty())
        {
            errorMsg = QString("无法从表 %1 的记录 '%2' 中提取有效的文件名").arg(tableId).arg(fileInfo.filePath());
            qWarning() << funcName << " - " << errorMsg;
            return QString();
        }
        qWarning() << funcName << " - 提取的文件名: " << justTheFileName << ". 建议运行数据迁移来修复数据库记录。";
        // 注意：这里没有实际移动文件，只是在内存中使用了提取的文件名。
        // 一个完整的迁移工具应该处理文件移动。
    }

    // 2. 获取当前数据库路径
    QSqlDatabase db = DatabaseManager::instance()->database();
    QString currentDbPath = db.databaseName();
    if (currentDbPath.isEmpty())
    {
        errorMsg = "无法获取当前数据库路径";
        qWarning() << funcName << " - " << errorMsg;
        return QString();
    }

    // 3. 使用 PathUtils 获取项目二进制数据目录
    QString projectBinaryDataDir = Utils::PathUtils::getProjectBinaryDataDirectory(currentDbPath);
    if (projectBinaryDataDir.isEmpty())
    {
        errorMsg = QString("无法为数据库 '%1' 生成项目二进制数据目录").arg(currentDbPath);
        qWarning() << funcName << " - " << errorMsg;
        return QString();
    }

    // 4. 拼接得到绝对路径
    // 标准化路径格式并确保使用正确的分隔符
    QString absoluteBinFilePath = QDir::cleanPath(projectBinaryDataDir + QDir::separator() + justTheFileName);
    absoluteBinFilePath = QDir::toNativeSeparators(absoluteBinFilePath);

    qDebug() << funcName << " - 解析得到的绝对路径: " << absoluteBinFilePath
             << " (DB: " << currentDbPath << ", File: " << justTheFileName << ")";

    errorMsg.clear(); // Clear error message on success
    return absoluteBinFilePath;
}

