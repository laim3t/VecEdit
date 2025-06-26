void MainWindow::fillTimeSetForVectorTable(int timeSetId, const QList<int> &selectedUiRows)
{
    // 添加调试日志
    qDebug() << "填充TimeSet开始 - TimeSet ID:" << timeSetId;
    if (!selectedUiRows.isEmpty())
    {
        QStringList rowList;
        for (int row : selectedUiRows)
        {
            rowList << QString::number(row);
        }
        qDebug() << "填充TimeSet - 针对UI行:" << rowList.join(',');
    }
    else
    {
        qDebug() << "填充TimeSet - 未选择行，将应用于整个表";
    }

    // 检查TimeSet ID有效性
    if (timeSetId <= 0)
    {
        QMessageBox::warning(this, tr("错误"), tr("TimeSet ID无效"));
        qDebug() << "填充TimeSet参数无效 - TimeSet ID:" << timeSetId;
        return;
    }

    // 获取当前向量表ID
    int currentIndex = m_vectorTableSelector->currentIndex();
    if (currentIndex < 0)
    {
        qDebug() << "填充TimeSet失败 - 未选择向量表";
        return;
    }
    int tableId = m_vectorTableSelector->itemData(currentIndex).toInt();
    QString tableName = m_vectorTableSelector->currentText();
    qDebug() << "填充TimeSet - 当前向量表ID:" << tableId << ", 名称:" << tableName;

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接失败"));
        qDebug() << "填充TimeSet失败 - 数据库连接失败";
        return;
    }

    // 获取TimeSet名称用于日志
    QSqlQuery nameQuery(db);
    nameQuery.prepare("SELECT timeset_name FROM timeset_list WHERE id = ?");
    nameQuery.addBindValue(timeSetId);
    QString timeSetName = "未知";
    if (nameQuery.exec() && nameQuery.next())
    {
        timeSetName = nameQuery.value(0).toString();
    }
    qDebug() << "填充TimeSet - 使用TimeSet:" << timeSetName << "(" << timeSetId << ")";

    // 开始事务
    db.transaction();

    try
    {
        // 1. 查询表对应的二进制文件路径
        QSqlQuery fileQuery(db);
        fileQuery.prepare("SELECT binary_data_filename FROM VectorTableMasterRecord WHERE id = ?");
        fileQuery.addBindValue(tableId);
        if (!fileQuery.exec() || !fileQuery.next())
        {
            QString errorText = fileQuery.lastError().text();
            qDebug() << "填充TimeSet - 查询二进制文件名失败:" << errorText;
            throw std::runtime_error(("查询二进制文件名失败: " + errorText).toStdString());
        }

        QString binFileName = fileQuery.value(0).toString();
        if (binFileName.isEmpty())
        {
            qDebug() << "填充TimeSet - 二进制文件名为空，无法进行填充操作";
            throw std::runtime_error("向量表未配置二进制文件存储，无法进行填充操作");
        }

        // 2. 解析二进制文件路径
        // 使用PathUtils获取项目二进制数据目录
        QString projectBinaryDataDir = Utils::PathUtils::getProjectBinaryDataDirectory(m_currentDbPath);
        if (projectBinaryDataDir.isEmpty())
        {
            QString errorMsg = QString("无法为数据库 '%1' 生成二进制数据目录路径").arg(m_currentDbPath);
            qWarning() << "填充TimeSet - " << errorMsg;
            throw std::runtime_error(errorMsg.toStdString());
        }

        qDebug() << "填充TimeSet - 项目二进制数据目录:" << projectBinaryDataDir;

        // 相对路径转绝对路径
        QString absoluteBinFilePath;
        if (QFileInfo(binFileName).isRelative())
        {
            absoluteBinFilePath = QDir(projectBinaryDataDir).absoluteFilePath(binFileName);
            qDebug() << "填充TimeSet - 相对路径转换为绝对路径:" << binFileName << " -> " << absoluteBinFilePath;
        }
        else
        {
            absoluteBinFilePath = binFileName;
        }

        // 3. 检查二进制文件是否存在
        if (!QFile::exists(absoluteBinFilePath))
        {
            qWarning() << "填充TimeSet - 二进制文件不存在:" << absoluteBinFilePath;
            throw std::runtime_error(("二进制文件不存在: " + absoluteBinFilePath).toStdString());
        }

        // 4. 查询列定义，找出TimeSet列
        QSqlQuery colQuery(db);
        colQuery.prepare("SELECT id, column_name, column_order, column_type, data_properties, IsVisible "
                         "FROM VectorTableColumnConfiguration "
                         "WHERE master_record_id = ? ORDER BY column_order");
        colQuery.addBindValue(tableId);

        if (!colQuery.exec())
        {
            QString errorText = colQuery.lastError().text();
            qWarning() << "填充TimeSet - 查询列定义失败:" << errorText;
            throw std::runtime_error(("查询列定义失败: " + errorText).toStdString());
        }

        QList<Vector::ColumnInfo> columns;
        int timeSetColumnIndex = -1; // 用于标记TimeSet列的索引
        int instructionIdColumnIndex = -1;

        while (colQuery.next())
        {
            Vector::ColumnInfo colInfo;
            colInfo.id = colQuery.value(0).toInt();
            colInfo.name = colQuery.value(1).toString();
            colInfo.order = colQuery.value(2).toInt();
            colInfo.original_type_str = colQuery.value(3).toString();

            // 解析data_properties
            QString dataPropertiesStr = colQuery.value(4).toString();
            QJsonDocument jsonDoc = QJsonDocument::fromJson(dataPropertiesStr.toUtf8());
            if (!jsonDoc.isNull() && jsonDoc.isObject())
            {
                colInfo.data_properties = jsonDoc.object();
            }
            else
            {
                // 如果解析失败，创建一个空的QJsonObject
                colInfo.data_properties = QJsonObject();
            }

            colInfo.is_visible = colQuery.value(5).toBool();

            // 映射列类型字符串到枚举
            if (colInfo.original_type_str == "TEXT")
                colInfo.type = Vector::ColumnDataType::TEXT;
            else if (colInfo.original_type_str == "INTEGER")
                colInfo.type = Vector::ColumnDataType::INTEGER;
            else if (colInfo.original_type_str == "BOOLEAN")
                colInfo.type = Vector::ColumnDataType::BOOLEAN;
            else if (colInfo.original_type_str == "INSTRUCTION_ID")
            {
                colInfo.type = Vector::ColumnDataType::INSTRUCTION_ID;
                instructionIdColumnIndex = columns.size();
            }
            else if (colInfo.original_type_str == "TIMESET_ID")
            {
                colInfo.type = Vector::ColumnDataType::TIMESET_ID;
                timeSetColumnIndex = columns.size();
            }
            else if (colInfo.original_type_str == "PIN_STATE_ID")
                colInfo.type = Vector::ColumnDataType::PIN_STATE_ID;
            else
                colInfo.type = Vector::ColumnDataType::TEXT; // 默认类型

            columns.append(colInfo);
        }

        if (timeSetColumnIndex == -1)
        {
            qWarning() << "填充TimeSet - 未找到TimeSet列，尝试修复表结构";

            // 尝试添加缺失的TimeSet列
            QSqlQuery addTimeSetColQuery(db);
            addTimeSetColQuery.prepare("INSERT INTO VectorTableColumnConfiguration "
                                       "(master_record_id, column_name, column_order, column_type, data_properties, IsVisible) "
                                       "VALUES (?, ?, ?, ?, ?, 1)");

            // 获取当前最大列序号
            int maxOrder = -1;
            QSqlQuery maxOrderQuery(db);
            maxOrderQuery.prepare("SELECT MAX(column_order) FROM VectorTableColumnConfiguration WHERE master_record_id = ?");
            maxOrderQuery.addBindValue(tableId);
            if (maxOrderQuery.exec() && maxOrderQuery.next())
            {
                maxOrder = maxOrderQuery.value(0).toInt();
            }

            // 如果无法获取最大列序号，默认放在第2位置
            if (maxOrder < 0)
            {
                maxOrder = 1; // 添加在位置2 (索引为2，实际是第3列)
            }

            // 添加TimeSet列
            addTimeSetColQuery.addBindValue(tableId);
            addTimeSetColQuery.addBindValue("TimeSet");
            addTimeSetColQuery.addBindValue(2); // 固定在位置2 (通常TimeSet是第3列)
            addTimeSetColQuery.addBindValue("TIMESET_ID");
            addTimeSetColQuery.addBindValue("{}");

            if (!addTimeSetColQuery.exec())
            {
                qWarning() << "填充TimeSet - 添加TimeSet列失败:" << addTimeSetColQuery.lastError().text();
                throw std::runtime_error(("未找到TimeSet类型的列，且尝试添加时失败。无法执行填充操作。"));
            }

            qDebug() << "填充TimeSet - 成功添加TimeSet列，重新获取列定义";

            // 重新查询列定义
            colQuery.clear();
            colQuery.prepare("SELECT id, column_name, column_order, column_type, data_properties, IsVisible "
                             "FROM VectorTableColumnConfiguration "
                             "WHERE master_record_id = ? ORDER BY column_order");
            colQuery.addBindValue(tableId);

            if (!colQuery.exec())
            {
                qWarning() << "填充TimeSet - 重新查询列定义失败:" << colQuery.lastError().text();
                throw std::runtime_error(("查询列定义失败: " + colQuery.lastError().text()).toStdString());
            }

            // 重新解析列定义
            columns.clear();
            timeSetColumnIndex = -1;
            instructionIdColumnIndex = -1;

            while (colQuery.next())
            {
                Vector::ColumnInfo colInfo;
                colInfo.id = colQuery.value(0).toInt();
                colInfo.name = colQuery.value(1).toString();
                colInfo.order = colQuery.value(2).toInt();
                colInfo.original_type_str = colQuery.value(3).toString();

                // 解析data_properties
                QString dataPropertiesStr = colQuery.value(4).toString();
                QJsonDocument jsonDoc = QJsonDocument::fromJson(dataPropertiesStr.toUtf8());
                if (!jsonDoc.isNull() && jsonDoc.isObject())
                {
                    colInfo.data_properties = jsonDoc.object();
                }
                else
                {
                    colInfo.data_properties = QJsonObject();
                }

                colInfo.is_visible = colQuery.value(5).toBool();

                // 映射列类型字符串到枚举
                if (colInfo.original_type_str == "TEXT")
                    colInfo.type = Vector::ColumnDataType::TEXT;
                else if (colInfo.original_type_str == "INTEGER")
                    colInfo.type = Vector::ColumnDataType::INTEGER;
                else if (colInfo.original_type_str == "BOOLEAN")
                    colInfo.type = Vector::ColumnDataType::BOOLEAN;
                else if (colInfo.original_type_str == "INSTRUCTION_ID")
                {
                    colInfo.type = Vector::ColumnDataType::INSTRUCTION_ID;
                    instructionIdColumnIndex = columns.size();
                }
                else if (colInfo.original_type_str == "TIMESET_ID")
                {
                    colInfo.type = Vector::ColumnDataType::TIMESET_ID;
                    timeSetColumnIndex = columns.size();
                }
                else if (colInfo.original_type_str == "PIN_STATE_ID")
                    colInfo.type = Vector::ColumnDataType::PIN_STATE_ID;
                else
                    colInfo.type = Vector::ColumnDataType::TEXT; // 默认类型

                columns.append(colInfo);
            }

            // 再次检查是否找到TimeSet列
            if (timeSetColumnIndex == -1)
            {
                qWarning() << "填充TimeSet - 修复后仍未找到TimeSet列，放弃操作";
                throw std::runtime_error(("修复后仍未找到TimeSet类型的列，无法执行填充操作"));
            }

            qDebug() << "填充TimeSet - 成功修复表结构并找到TimeSet列，索引:" << timeSetColumnIndex;
        }

        // 5. 读取二进制文件数据
        QList<Vector::RowData> allRows;
        int schemaVersion = 1; // 默认值

        if (!Persistence::BinaryFileHelper::readAllRowsFromBinary(absoluteBinFilePath, columns, schemaVersion, allRows))
        {
            qWarning() << "填充TimeSet - 读取二进制文件失败:" << absoluteBinFilePath;
            QString errorMsg = QString("从二进制文件读取数据失败");
            throw std::runtime_error(errorMsg.toStdString());
        }

        qDebug() << "填充TimeSet - 从二进制文件读取到" << allRows.size() << "行数据";

        // 如果行数为0，提示用户
        if (allRows.size() == 0)
        {
            qWarning() << "填充TimeSet - 二进制文件中没有数据，无需填充";
            QMessageBox::warning(this, tr("警告"), tr("向量表中没有数据，无需填充"));
            db.rollback();
            return;
        }

        // 6. 更新内存中的TimeSet数据
        int updatedRowCount = 0;
        for (int i = 0; i < allRows.size(); i++)
        {
            Vector::RowData &rowData = allRows[i];

            // 检查行数据大小是否合法
            if (rowData.size() <= timeSetColumnIndex)
            {
                qWarning() << "填充TimeSet - 行" << i << "数据列数" << rowData.size()
                           << "小于TimeSet列索引" << timeSetColumnIndex << "，跳过此行";
                continue;
            }

            // 检查当前行是否是要更新的行
            bool shouldProcessThisRow = selectedUiRows.isEmpty(); // 如果没有选择行，处理所有行

            // 如果用户选择了特定行，检查当前行是否在选择范围内
            if (!shouldProcessThisRow && i < allRows.size())
            {
                // 注意：selectedUiRows中存储的是UI中的行索引，这与二进制文件中的行索引i可能不完全一致
                shouldProcessThisRow = selectedUiRows.contains(i);

                if (shouldProcessThisRow)
                {
                    qDebug() << "填充TimeSet - 行 " << i << " 在用户选择的行列表中";
                }
            }

            if (shouldProcessThisRow)
            {
                // 直接更新为新的TimeSet ID
                int oldTimeSetId = rowData[timeSetColumnIndex].toInt();
                rowData[timeSetColumnIndex] = timeSetId;
                updatedRowCount++;
                qDebug() << "填充TimeSet - 已更新行 " << i << " 的TimeSet ID 从 " << oldTimeSetId << " 到 " << timeSetId;
            }
        }

        qDebug() << "填充TimeSet - 内存中更新了" << updatedRowCount << "行数据";

        // 7. 写回二进制文件
        if (!Persistence::BinaryFileHelper::writeAllRowsToBinary(absoluteBinFilePath, columns, schemaVersion, allRows))
        {
            qWarning() << "填充TimeSet - 写入二进制文件失败:" << absoluteBinFilePath;
            QString errorMsg = QString("将更新后的数据写回二进制文件失败");
            throw std::runtime_error(errorMsg.toStdString());
        }

        qDebug() << "填充TimeSet - 已成功写入二进制文件:" << absoluteBinFilePath;

        // 更新数据库中的记录
        QList<int> idsToUpdate; // 存储要更新的数据库行的ID

        if (!selectedUiRows.isEmpty())
        {
            // 1. 获取表中所有行的ID，按sort_index排序
            QList<int> allRowIds;
            QSqlQuery idQuery(db);
            QString idSql = QString("SELECT id FROM vector_table_data WHERE table_id = %1 ORDER BY sort_index").arg(tableId);
            qDebug() << "填充TimeSet - 查询所有行ID:" << idSql;
            if (!idQuery.exec(idSql))
            {
                throw std::runtime_error(("查询行ID失败: " + idQuery.lastError().text()).toStdString());
            }
            while (idQuery.next())
            {
                allRowIds.append(idQuery.value(0).toInt());
            }
            qDebug() << "填充TimeSet - 数据库中共有" << allRowIds.size() << "行";

            if (allRows.size() > allRowIds.size())
            {
                qWarning() << "数据不一致：二进制文件有" << allRows.size() << "行，但数据库只有" << allRowIds.size() << "行。将补全缺失的数据库记录。";
                QSqlQuery insertQuery(db);
                insertQuery.prepare("INSERT INTO vector_table_data (table_id, sort_index, timeset_id, instruction_id, comment, label) "
                                    "VALUES (?, ?, ?, ?, '', '')");

                int existingRows = allRowIds.size();
                for (int i = existingRows; i < allRows.size(); ++i)
                {
                    // 确保行数据存在且TimeSet/Instruction列索引有效
                    if (i < allRows.size() && timeSetColumnIndex >= 0 && timeSetColumnIndex < allRows[i].size() && instructionIdColumnIndex >= 0 && instructionIdColumnIndex < allRows[i].size())
                    {
                        insertQuery.bindValue(0, tableId);
                        insertQuery.bindValue(1, i); // sort_index
                        insertQuery.bindValue(2, allRows[i][timeSetColumnIndex].toInt());
                        insertQuery.bindValue(3, allRows[i][instructionIdColumnIndex].toInt());

                        if (!insertQuery.exec())
                        {
                            throw std::runtime_error(("补全数据库行失败: " + insertQuery.lastError().text()).toStdString());
                        }
                        allRowIds.append(insertQuery.lastInsertId().toInt());
                    }
                    else
                    {
                        qWarning() << "跳过为行" << i << "补全数据库记录，因为数据格式无效或缺少必要的列(TimeSet/Instruction)。";
                    }
                }
                qDebug() << "成功补全" << (allRowIds.size() - existingRows) << "行数据库记录。";
            }

            // 2. 根据选中的UI行索引，找到对应的数据库ID
            foreach (int uiRow, selectedUiRows)
            {
                if (uiRow >= 0 && uiRow < allRowIds.size())
                {
                    idsToUpdate.append(allRowIds[uiRow]);
                }
                else
                {
                    qDebug() << "填充TimeSet警告 - UI行索引" << uiRow << "无效，忽略";
                }
            }
            if (idsToUpdate.isEmpty())
            {
                qDebug() << "填充TimeSet - 没有有效的数据库ID需要更新，操作取消";
                db.rollback(); // 不需要执行事务，直接回滚
                return;
            }
            qDebug() << "填充TimeSet - 准备更新的数据库ID:" << idsToUpdate;
        }

        // 准备更新SQL
        QSqlQuery query(db);
        QString updateSQL;
        if (!idsToUpdate.isEmpty())
        {
            // 如果有选定行，则基于ID更新
            QString idPlaceholders = QString("?,").repeated(idsToUpdate.size());
            idPlaceholders.chop(1); // 移除最后一个逗号
            updateSQL = QString("UPDATE vector_table_data SET timeset_id = ? WHERE id IN (%1)").arg(idPlaceholders);
            query.prepare(updateSQL);
            query.addBindValue(timeSetId);
            foreach (int id, idsToUpdate)
            {
                query.addBindValue(id);
            }
            qDebug() << "填充TimeSet - 执行SQL (按ID):" << updateSQL;
            qDebug() << "参数: timesetId=" << timeSetId << ", IDs=" << idsToUpdate;
        }
        else
        {
            // 如果没有选定行，则更新整个表
            updateSQL = "UPDATE vector_table_data SET timeset_id = :timesetId WHERE table_id = :tableId";
            query.prepare(updateSQL);
            query.bindValue(":timesetId", timeSetId);
            query.bindValue(":tableId", tableId);
            qDebug() << "填充TimeSet - 执行SQL (全表):" << updateSQL;
            qDebug() << "参数: timesetId=" << timeSetId << ", tableId=" << tableId;
        }

        if (!query.exec())
        {
            QString errorText = query.lastError().text();
            qDebug() << "填充TimeSet失败 - SQL错误:" << errorText;
            throw std::runtime_error(errorText.toStdString());
        }

        int rowsAffected = query.numRowsAffected();
        qDebug() << "填充TimeSet - 已更新" << rowsAffected << "行";

        // 提交事务
        if (!db.commit())
        {
            QString errorText = db.lastError().text();
            qDebug() << "填充TimeSet失败 - 提交事务失败:" << errorText;
            throw std::runtime_error(errorText.toStdString());
        }

        // 注意：不要在这里调用onVectorTableSelectionChanged，因为它会重置页码
        qDebug() << "填充TimeSet - 准备重新加载表格数据，将保留当前页码:" << m_currentPage;

        // 显示成功消息
        QMessageBox::information(this, tr("成功"), tr("已将 %1 填充到选中区域，共更新了 %2 行数据").arg(timeSetName).arg(updatedRowCount));
        qDebug() << "填充TimeSet - 操作成功完成";

        // 保存当前页码并仅刷新当前页数据，不改变页码状态
        int currentIndex = m_vectorTabWidget->currentIndex();
        if (currentIndex >= 0 && m_tabToTableId.contains(currentIndex))
        {
            int tableId = m_tabToTableId[currentIndex];

            // 清除当前表的数据缓存，但不改变页码
            VectorDataHandler::instance().clearTableDataCache(tableId);

            // 判断当前使用的视图类型
            bool isUsingNewView = (m_vectorStackedWidget && m_vectorStackedWidget->currentIndex() == 1);
            bool refreshSuccess = false;

            if (isUsingNewView && m_vectorTableModel)
            {
                // 新视图 (QTableView)
                qDebug() << "填充TimeSet - 使用新视图刷新当前页数据，保持在页码:" << m_currentPage;
                refreshSuccess = VectorDataHandler::instance().loadVectorTablePageDataForModel(
                    tableId, m_vectorTableModel, m_currentPage, m_pageSize);
            }
            else
            {
                // 旧视图 (QTableWidget)
                qDebug() << "填充TimeSet - 使用旧视图刷新当前页数据，保持在页码:" << m_currentPage;
                refreshSuccess = VectorDataHandler::instance().loadVectorTablePageData(
                    tableId, m_vectorTableWidget, m_currentPage, m_pageSize);
            }

            if (!refreshSuccess)
            {
                qWarning() << "填充TimeSet - 刷新表格数据失败";
            }

            // 更新分页信息显示
            updatePaginationInfo();
        }
        else
        {
            qWarning() << "填充TimeSet - 无法获取当前向量表ID，回退到使用refreshVectorTableData()";
            refreshVectorTableData();
        }

        // 更新波形图以反映TimeSet变更
        if (m_isWaveformVisible && m_waveformPlot)
        {
            qDebug() << "MainWindow::fillTimeSetForVectorTable - 更新波形图以反映TimeSet变更";

            // 强制刷新数据库连接，确保没有缓存问题
            QSqlDatabase refreshDb = DatabaseManager::instance()->database();
            if (refreshDb.isOpen())
            {
                qDebug() << "MainWindow::fillTimeSetForVectorTable - 刷新数据库缓存";
                refreshDb.transaction();
                refreshDb.commit();
            }

            // 短暂延迟以确保数据库变更已经完成
            QTimer::singleShot(100, this, [this]()
                               {
                qDebug() << "MainWindow::fillTimeSetForVectorTable - 延迟更新波形图";
                updateWaveformView(); });

            // 同时也立即更新一次
            updateWaveformView();
        }
    }
    catch (const std::exception &e)
    {
        // 回滚事务
        db.rollback();
        qDebug() << "填充TimeSet - 操作失败，已回滚事务:" << e.what();

        // 显示错误消息
        QMessageBox::critical(this, tr("错误"), tr("填充TimeSet失败: %1").arg(e.what()));
    }
}

// 显示填充TimeSet对话框
void MainWindow::showFillTimeSetDialog()
{
    // 获取当前选择的向量表
    int currentIndex = m_vectorTableSelector->currentIndex();
    if (currentIndex < 0)
    {
        QMessageBox::warning(this, "操作失败", "没有选中的向量表");
        return;
    }

    // 获取表ID
    int tableId = m_vectorTableSelector->currentData().toInt();

    // 获取向量表行数
    int rowCount = VectorDataHandler::instance().getVectorTableRowCount(tableId);
    if (rowCount <= 0)
    {
        QMessageBox::warning(this, "操作失败", "当前向量表没有数据行");
        return;
    }

    // 显示填充TimeSet对话框
    FillTimeSetDialog dialog(this);
    // 设置向量表总行数
    dialog.setVectorRowCount(rowCount);

    // 获取选中的单元格并计算行范围
    QModelIndexList selectedIndexes = m_vectorTableWidget->selectionModel()->selectedIndexes();
    if (!selectedIndexes.isEmpty())
    {
        // 找出最小和最大行号（1-based）
        int minRow = INT_MAX;
        int maxRow = 0;
        int pageOffset = m_currentPage * m_pageSize; // 分页偏移
        foreach (const QModelIndex &index, selectedIndexes)
        {
            int rowIdx = pageOffset + index.row() + 1; // 转换为1-based的绝对行号
            minRow = qMin(minRow, rowIdx);
            maxRow = qMax(maxRow, rowIdx);
        }
        if (minRow <= maxRow)
        {
            dialog.setSelectedRange(minRow, maxRow);
        }
    }

    if (dialog.exec() == QDialog::Accepted)
    {
        int timeSetId = dialog.getSelectedTimeSetId();
        if (timeSetId <= 0)
        {
            return; // No valid timeset selected
        }

        QList<int> rowsToUpdate;
        // 修复BUG：无论是否选中行，都使用对话框中设置的范围
        int fromRow = dialog.getStartRow(); // 0-based
        int toRow = dialog.getEndRow();     // 0-based

        // Ensure the range is valid.
        if (fromRow < 0 || toRow < fromRow || toRow >= rowCount)
        {
            QMessageBox::warning(this, "范围无效", "指定的行范围无效。");
            return;
        }

        for (int i = fromRow; i <= toRow; ++i)
        {
            rowsToUpdate.append(i);
        }

        // Call the function to perform the update
        fillTimeSetForVectorTable(timeSetId, rowsToUpdate);
    }
}
