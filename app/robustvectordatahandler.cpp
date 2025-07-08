bool RobustVectorDataHandler::deleteVectorRows(int tableId, const QList<int> &rowIndices, QString &errorMsg)
{
    const QString funcName = "RobustVectorDataHandler::deleteVectorRows";
    qDebug() << funcName << " - 开始删除指定行，表ID:" << tableId << "行数量:" << rowIndices.size();

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        errorMsg = "数据库未打开";
        qWarning() << funcName << " - " << errorMsg;
        return false;
    }

    // 开启事务，确保数据一致性
    db.transaction();

    try
    {
        // 1. 查询当前表的主记录
        QSqlQuery queryMaster(db);
        queryMaster.prepare("SELECT row_count FROM VectorTableMasterRecord WHERE id = ?");
        queryMaster.addBindValue(tableId);

        if (!queryMaster.exec() || !queryMaster.next())
        {
            errorMsg = "无法获取表 " + QString::number(tableId) + " 的主记录";
            qWarning() << funcName << " - " << errorMsg;
            db.rollback();
            return false;
        }

        int currentRowCount = queryMaster.value(0).toInt();

        // 2. 将选定的行标记为无效（将is_active设为0）
        QSqlQuery updateQuery(db);
        updateQuery.prepare("UPDATE VectorTableRowIndex SET is_active = 0 "
                            "WHERE master_record_id = ? AND logical_row_order = ?");

        int updatedRows = 0;
        for (int rowIndex : rowIndices)
        {
            // 确保行索引有效
            if (rowIndex < 0 || rowIndex >= currentRowCount)
            {
                qWarning() << funcName << " - 忽略无效行索引: " << rowIndex;
                continue;
            }

            // 更新行索引记录
            updateQuery.bindValue(0, tableId);
            updateQuery.bindValue(1, rowIndex);

            if (!updateQuery.exec())
            {
                qWarning() << funcName << " - 无法更新行 " << rowIndex << ": " << updateQuery.lastError().text();
                continue;
            }

            updatedRows++;
        }

        // 3. 更新主记录表中的行数（减去成功删除的行数）
        if (updatedRows > 0)
        {
            QSqlQuery updateMaster(db);
            updateMaster.prepare("UPDATE VectorTableMasterRecord SET row_count = row_count - ? WHERE id = ?");
            updateMaster.bindValue(0, updatedRows);
            updateMaster.bindValue(1, tableId);

            if (!updateMaster.exec())
            {
                errorMsg = "无法更新表 " + QString::number(tableId) + " 的行数: " + updateMaster.lastError().text();
                qWarning() << funcName << " - " << errorMsg;
                db.rollback();
                return false;
            }
        }

        // 4. 提交事务
        db.commit();
        qDebug() << funcName << " - 成功删除 " << updatedRows << " 行";
        return true;
    }
    catch (const std::exception &e)
    {
        errorMsg = "删除行时发生异常: " + QString(e.what());
        qWarning() << funcName << " - " << errorMsg;
        db.rollback();
        return false;
    }
}

bool RobustVectorDataHandler::deleteVectorRowsInRange(int tableId, int startRow, int endRow, QString &errorMsg)
{
    const QString funcName = "RobustVectorDataHandler::deleteVectorRowsInRange";
    qDebug() << funcName << " - 开始删除行范围，表ID:" << tableId << "起始行:" << startRow << "结束行:" << endRow;

    // 确保起始行不大于结束行
    if (startRow > endRow)
    {
        int temp = startRow;
        startRow = endRow;
        endRow = temp;
    }

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        errorMsg = "数据库未打开";
        qWarning() << funcName << " - " << errorMsg;
        return false;
    }

    // 开启事务，确保数据一致性
    db.transaction();

    try
    {
        // 1. 查询当前表的主记录，获取总行数
        QSqlQuery queryMaster(db);
        queryMaster.prepare("SELECT row_count FROM VectorTableMasterRecord WHERE id = ?");
        queryMaster.addBindValue(tableId);

        if (!queryMaster.exec() || !queryMaster.next())
        {
            errorMsg = "无法获取表 " + QString::number(tableId) + " 的主记录";
            qWarning() << funcName << " - " << errorMsg;
            db.rollback();
            return false;
        }

        int currentRowCount = queryMaster.value(0).toInt();

        // 调整范围，确保不超出有效行范围
        startRow = qMax(0, startRow);
        endRow = qMin(currentRowCount - 1, endRow);

        // 计算要删除的行数
        int rowsToDelete = endRow - startRow + 1;
        if (rowsToDelete <= 0)
        {
            errorMsg = "无效的行范围";
            qWarning() << funcName << " - " << errorMsg;
            db.rollback();
            return false;
        }

        // 2. 批量将选定范围内的行标记为无效（将is_active设为0）
        QSqlQuery updateQuery(db);
        updateQuery.prepare("UPDATE VectorTableRowIndex SET is_active = 0 "
                            "WHERE master_record_id = ? AND logical_row_order BETWEEN ? AND ?");
        updateQuery.bindValue(0, tableId);
        updateQuery.bindValue(1, startRow);
        updateQuery.bindValue(2, endRow);

        if (!updateQuery.exec())
        {
            errorMsg = "无法批量更新行索引: " + updateQuery.lastError().text();
            qWarning() << funcName << " - " << errorMsg;
            db.rollback();
            return false;
        }

        int affectedRows = updateQuery.numRowsAffected();
        if (affectedRows <= 0)
        {
            errorMsg = "没有行被更新，可能是指定范围内没有有效行";
            qWarning() << funcName << " - " << errorMsg;
            db.rollback();
            return false;
        }

        // 3. 更新主记录表中的行数（减去成功删除的行数）
        QSqlQuery updateMaster(db);
        updateMaster.prepare("UPDATE VectorTableMasterRecord SET row_count = row_count - ? WHERE id = ?");
        updateMaster.bindValue(0, affectedRows);
        updateMaster.bindValue(1, tableId);

        if (!updateMaster.exec())
        {
            errorMsg = "无法更新表 " + QString::number(tableId) + " 的行数: " + updateMaster.lastError().text();
            qWarning() << funcName << " - " << errorMsg;
            db.rollback();
            return false;
        }

        // 4. 提交事务
        db.commit();
        qDebug() << funcName << " - 成功删除 " << affectedRows << " 行";
        return true;
    }
    catch (const std::exception &e)
    {
        errorMsg = "删除行范围时发生异常: " + QString(e.what());
        qWarning() << funcName << " - " << errorMsg;
        db.rollback();
        return false;
    }
}