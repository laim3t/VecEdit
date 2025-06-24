
void MainWindow::replaceTimeSetForVectorTable(int fromTimeSetId, int toTimeSetId, const QList<int> &selectedUiRows)
{
    qDebug() << "替换TimeSet - 开始替换过程";

    // 输出选择的行信息
    if (selectedUiRows.isEmpty())
    {
        qDebug() << "替换TimeSet - 用户未选择特定行，将对所有行进行操作";
    }
    else
    {
        QStringList rowsList;
        for (int row : selectedUiRows)
        {
            rowsList << QString::number(row);
        }
        qDebug() << "替换TimeSet - 用户选择了以下行：" << rowsList.join(", ");
    }

    int currentIndex = m_vectorTableSelector->currentIndex();
    if (currentIndex < 0)
    {
        QMessageBox::warning(this, tr("警告"), tr("请先选择一个向量表"));
        qDebug() << "替换TimeSet - 未选择向量表，操作取消";
        return;
    }
    int tableId = m_vectorTableSelector->itemData(currentIndex).toInt();
    QString tableName = m_vectorTableSelector->currentText();
    qDebug() << "替换TimeSet - 当前向量表ID:" << tableId << ", 名称:" << tableName;

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接失败"));
        qDebug() << "替换TimeSet失败 - 数据库连接失败";
        return;
    }

    // 获取TimeSet名称用于日志
    QSqlQuery fromNameQuery(db);
    fromNameQuery.prepare("SELECT timeset_name FROM timeset_list WHERE id = ?");
    fromNameQuery.addBindValue(fromTimeSetId);
    QString fromTimeSetName = "未知";
    if (fromNameQuery.exec() && fromNameQuery.next())
    {
        fromTimeSetName = fromNameQuery.value(0).toString();
    }

    QSqlQuery toNameQuery(db);
    toNameQuery.prepare("SELECT timeset_name FROM timeset_list WHERE id = ?");
    toNameQuery.addBindValue(toTimeSetId);
    QString toTimeSetName = "未知";
    if (toNameQuery.exec() && toNameQuery.next())
    {
        toTimeSetName = toNameQuery.value(0).toString();
    }
    qDebug() << "替换TimeSet - 查找:" << fromTimeSetName << "(" << fromTimeSetId << ") 替换:" << toTimeSetName << "(" << toTimeSetId << ")";

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
            qDebug() << "替换TimeSet - 查询二进制文件名失败:" << errorText;
            throw std::runtime_error(("查询二进制文件名失败: " + errorText).toStdString());
        }

        QString binFileName = fileQuery.value(0).toString();
        if (binFileName.isEmpty())
        {
            qDebug() << "替换TimeSet - 二进制文件名为空，无法进行替换操作";
            throw std::runtime_error("向量表未配置二进制文件存储，无法进行替换操作");
        }

        // 2. 解析二进制文件路径
        // 使用PathUtils获取项目二进制数据目录
        QString projectBinaryDataDir = Utils::PathUtils::getProjectBinaryDataDirectory(m_currentDbPath);
        if (projectBinaryDataDir.isEmpty())
        {
            QString errorMsg = QString("无法为数据库 '%1' 生成二进制数据目录路径").arg(m_currentDbPath);
            qWarning() << "替换TimeSet - " << errorMsg;
            throw std::runtime_error(errorMsg.toStdString());
        }

        qDebug() << "替换TimeSet - 项目二进制数据目录:" << projectBinaryDataDir;

        // 相对路径转绝对路径
        QString absoluteBinFilePath;
        if (QFileInfo(binFileName).isRelative())
        {
            absoluteBinFilePath = QDir(projectBinaryDataDir).absoluteFilePath(binFileName);
            qDebug() << "替换TimeSet - 相对路径转换为绝对路径:" << binFileName << " -> " << absoluteBinFilePath;
        }
        else
        {
            absoluteBinFilePath = binFileName;
        }

        // 3. 检查二进制文件是否存在
        if (!QFile::exists(absoluteBinFilePath))
        {
            qWarning() << "替换TimeSet - 二进制文件不存在:" << absoluteBinFilePath;
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
            qWarning() << "替换TimeSet - 查询列定义失败:" << errorText;
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
                timeSetColumnIndex = columns.size(); // 记录TimeSet列的索引
            }
            else if (colInfo.original_type_str == "PIN_STATE_ID")
                colInfo.type = Vector::ColumnDataType::PIN_STATE_ID;
            else
                colInfo.type = Vector::ColumnDataType::TEXT; // 默认类型

            columns.append(colInfo);
        }

        if (timeSetColumnIndex == -1)
        {
            qWarning() << "替换TimeSet - 未找到TimeSet列，尝试修复表结构";

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
                qWarning() << "替换TimeSet - 添加TimeSet列失败:" << addTimeSetColQuery.lastError().text();
                throw std::runtime_error(("未找到TimeSet类型的列，且尝试添加时失败。无法执行替换操作。"));
            }

            qDebug() << "替换TimeSet - 成功添加TimeSet列，重新获取列定义";

            // 重新查询列定义
            colQuery.clear();
            colQuery.prepare("SELECT id, column_name, column_order, column_type, data_properties, IsVisible "
                             "FROM VectorTableColumnConfiguration "
                             "WHERE master_record_id = ? ORDER BY column_order");
            colQuery.addBindValue(tableId);

            if (!colQuery.exec())
            {
                qWarning() << "替换TimeSet - 重新查询列定义失败:" << colQuery.lastError().text();
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
                    timeSetColumnIndex = columns.size(); // 记录TimeSet列的索引
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
                qWarning() << "替换TimeSet - 修复后仍未找到TimeSet列，放弃操作";
                throw std::runtime_error(("修复后仍未找到TimeSet类型的列，无法执行替换操作"));
            }

            qDebug() << "替换TimeSet - 成功修复表结构并找到TimeSet列，索引:" << timeSetColumnIndex;
        }

        // 5. 读取二进制文件数据
        QList<Vector::RowData> allRows;
        int schemaVersion = 1; // 默认值

        if (!Persistence::BinaryFileHelper::readAllRowsFromBinary(absoluteBinFilePath, columns, schemaVersion, allRows))
        {
            qWarning() << "替换TimeSet - 读取二进制文件失败:" << absoluteBinFilePath;
            QString errorMsg = QString("从二进制文件读取数据失败");
            throw std::runtime_error(errorMsg.toStdString());
        }

        qDebug() << "替换TimeSet - 从二进制文件读取到" << allRows.size() << "行数据";

        // 如果行数为0，提示用户
        if (allRows.size() == 0)
        {
            qWarning() << "替换TimeSet - 二进制文件中没有数据，无需替换";
            QMessageBox::warning(this, tr("警告"), tr("向量表中没有数据，无需替换"));
            db.rollback();
            return;
        }

        // 输出所有行的TimeSet ID和预期替换值
        qDebug() << "替换TimeSet - 预期将TimeSet ID " << fromTimeSetId << " 替换为 " << toTimeSetId;
        qDebug() << "替换TimeSet - TimeSet列索引: " << timeSetColumnIndex;
        for (int i = 0; i < allRows.size(); i++)
        {
            if (allRows[i].size() > timeSetColumnIndex)
            {
                int rowTimeSetId = allRows[i][timeSetColumnIndex].toInt();
                qDebug() << "替换TimeSet - 行 " << i << " 的TimeSet ID: " << rowTimeSetId
                         << (rowTimeSetId == fromTimeSetId ? " (匹配)" : " (不匹配)");
            }
        }

        // 创建名称到ID的映射
        QMap<QString, int> timeSetNameToIdMap;
        QSqlQuery allTimeSetQuery(db);
        if (allTimeSetQuery.exec("SELECT id, timeset_name FROM timeset_list"))
        {
            while (allTimeSetQuery.next())
            {
                int id = allTimeSetQuery.value(0).toInt();
                QString name = allTimeSetQuery.value(1).toString();
                timeSetNameToIdMap[name] = id;
                qDebug() << "替换TimeSet - TimeSet名称映射: " << name << " -> ID: " << id;
            }
        }

        // 6. 更新内存中的TimeSet数据
        int updatedRowCount = 0;
        for (int i = 0; i < allRows.size(); i++)
        {
            Vector::RowData &rowData = allRows[i];

            // 检查行数据大小是否合法
            if (rowData.size() <= timeSetColumnIndex)
            {
                qWarning() << "替换TimeSet - 行" << i << "数据列数" << rowData.size()
                           << "小于TimeSet列索引" << timeSetColumnIndex << "，跳过此行";
                continue;
            }

            // 检查当前行是否是要更新的行
            bool shouldProcessThisRow = selectedUiRows.isEmpty(); // 如果没有选择行，处理所有行

            // 如果用户选择了特定行，检查当前行是否在选择范围内
            if (!shouldProcessThisRow && i < allRows.size())
            {
                // 注意：selectedUiRows中存储的是UI中的行索引，这与二进制文件中的行索引i可能不完全一致
                // 如果用户选择了某行，我们将处理它，无论它在二进制文件中的顺序如何
                shouldProcessThisRow = selectedUiRows.contains(i);

                if (shouldProcessThisRow)
                {
                    qDebug() << "替换TimeSet - 行 " << i << " 在用户选择的行列表中";
                }
            }

            if (shouldProcessThisRow)
            {
                int currentTimeSetId = rowData[timeSetColumnIndex].toInt();
                qDebug() << "替换TimeSet - 处理行 " << i << "，当前TimeSet ID: " << currentTimeSetId
                         << "，fromTimeSetId: " << fromTimeSetId;

                // 尝试直接通过ID匹配
                if (currentTimeSetId == fromTimeSetId)
                {
                    // 更新TimeSet ID
                    rowData[timeSetColumnIndex] = toTimeSetId;
                    updatedRowCount++;
                    qDebug() << "替换TimeSet - 已更新行 " << i << " 的TimeSet ID 从 " << fromTimeSetId << " 到 " << toTimeSetId;
                }
                // 尝试通过名称匹配（获取当前ID对应的名称，检查是否与fromTimeSetName匹配）
                else
                {
                    // 获取当前ID对应的名称
                    QString currentTimeSetName = "未知";
                    QSqlQuery currentTSNameQuery(db);
                    currentTSNameQuery.prepare("SELECT timeset_name FROM timeset_list WHERE id = ?");
                    currentTSNameQuery.addBindValue(currentTimeSetId);
                    if (currentTSNameQuery.exec() && currentTSNameQuery.next())
                    {
                        currentTimeSetName = currentTSNameQuery.value(0).toString();
                    }

                    // 如果当前TimeSet名称与fromTimeSetName匹配，则更新
                    if (currentTimeSetName == fromTimeSetName)
                    {
                        // 更新TimeSet ID
                        rowData[timeSetColumnIndex] = toTimeSetId;
                        updatedRowCount++;
                        qDebug() << "替换TimeSet - 已通过名称匹配更新行 " << i
                                 << " 的TimeSet从 " << currentTimeSetName << " (ID:" << currentTimeSetId
                                 << ") 到 " << toTimeSetName << " (ID:" << toTimeSetId << ")";
                    }
                }
            }
        }

        qDebug() << "替换TimeSet - 内存中更新了" << updatedRowCount << "行数据";

        if (updatedRowCount == 0)
        {
            QString selectMessage;
            if (selectedUiRows.isEmpty())
            {
                selectMessage = tr("所有行");
            }
            else
            {
                QStringList rowStrings;
                for (int row : selectedUiRows)
                {
                    rowStrings << QString::number(row + 1); // 转换为1-based显示给用户
                }
                selectMessage = tr("选中的行 (%1)").arg(rowStrings.join(", "));
            }

            qDebug() << "替换TimeSet - 在" << selectMessage << "中没有找到使用ID " << fromTimeSetId
                     << " 或名称 " << fromTimeSetName << " 的TimeSet行";

            // 检查二进制文件中的行使用的TimeSet IDs
            QSet<int> usedTimeSetIds;
            for (int i = 0; i < allRows.size(); i++)
            {
                // 只统计选中行或全部行（如果没有选择）
                if (selectedUiRows.isEmpty() || selectedUiRows.contains(i))
                {
                    if (allRows[i].size() > timeSetColumnIndex)
                    {
                        int usedId = allRows[i][timeSetColumnIndex].toInt();
                        usedTimeSetIds.insert(usedId);
                    }
                }
            }

            qDebug() << "替换TimeSet - 在" << selectMessage << "中使用的TimeSet IDs:" << usedTimeSetIds.values();

            // 获取每个使用的ID对应的名称
            QStringList usedTimeSetNames;
            for (int usedId : usedTimeSetIds)
            {
                QSqlQuery nameQuery(db);
                nameQuery.prepare("SELECT timeset_name FROM timeset_list WHERE id = ?");
                nameQuery.addBindValue(usedId);
                QString tsName = "未知";
                if (nameQuery.exec() && nameQuery.next())
                {
                    tsName = nameQuery.value(0).toString();
                    usedTimeSetNames.append(tsName);
                }

                qDebug() << "替换TimeSet - 在" << selectMessage << "中使用的TimeSet ID: " << usedId << ", 名称: " << tsName;
            }

            // 提示用户没有找到匹配的TimeSet
            QString usedInfo;
            if (!usedTimeSetNames.isEmpty())
            {
                usedInfo = tr("表中使用的TimeSet有: %1").arg(usedTimeSetNames.join(", "));
            }
            else
            {
                usedInfo = tr("表中没有使用任何TimeSet");
            }

            QMessageBox::information(this, tr("提示"),
                                     tr("在%1中没有找到使用 %2 (ID: %3) 的数据行需要替换。\n%4")
                                         .arg(selectMessage)
                                         .arg(fromTimeSetName)
                                         .arg(fromTimeSetId)
                                         .arg(usedInfo));
            db.rollback();
            return;
        }

        // 7. 写回二进制文件
        if (!Persistence::BinaryFileHelper::writeAllRowsToBinary(absoluteBinFilePath, columns, schemaVersion, allRows))
        {
            qWarning() << "替换TimeSet - 写入二进制文件失败:" << absoluteBinFilePath;
            QString errorMsg = QString("将更新后的数据写回二进制文件失败");
            throw std::runtime_error(errorMsg.toStdString());
        }

        qDebug() << "替换TimeSet - 已成功写入二进制文件:" << absoluteBinFilePath;

        // 8. 更新数据库中的TimeSet元数据
        qDebug() << "替换TimeSet - 开始更新数据库元数据...";
        QList<int> allRowIds;
        QSqlQuery idQuery(db);
        QString idSql = QString("SELECT id FROM vector_table_data WHERE table_id = %1 ORDER BY sort_index").arg(tableId);
        if (!idQuery.exec(idSql))
        {
            throw std::runtime_error(("查询行ID失败: " + idQuery.lastError().text()).toStdString());
        }
        while (idQuery.next())
        {
            allRowIds.append(idQuery.value(0).toInt());
        }

        // 自动修复：检查并补全缺失的数据库行
        if (allRows.size() > allRowIds.size())
        {
            qWarning() << "数据不一致：二进制文件有" << allRows.size() << "行，但数据库只有" << allRowIds.size() << "行。将补全缺失的数据库记录。";
            QSqlQuery insertQuery(db);
            insertQuery.prepare("INSERT INTO vector_table_data (table_id, sort_index, timeset_id, instruction_id, comment, label) "
                                "VALUES (?, ?, ?, ?, '', '')");
            int existingRows = allRowIds.size();
            for (int i = existingRows; i < allRows.size(); ++i)
            {
                if (i < allRows.size() && timeSetColumnIndex >= 0 && timeSetColumnIndex < allRows[i].size() && instructionIdColumnIndex >= 0 && instructionIdColumnIndex < allRows[i].size())
                {
                    insertQuery.bindValue(0, tableId);
                    insertQuery.bindValue(1, i);
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

        // 更新现有行的TimeSet ID
        QSqlQuery updateQuery(db);
        updateQuery.prepare("UPDATE vector_table_data SET timeset_id = ? WHERE id = ?");
        int dbUpdatedCount = 0;
        for (int i = 0; i < allRows.size(); ++i)
        {
            if (i >= allRowIds.size() || i >= allRows.size())
                continue;

            bool wasProcessed = false;
            if (selectedUiRows.isEmpty())
            {
                wasProcessed = true; // Process all if no selection
            }
            else
            {
                wasProcessed = selectedUiRows.contains(i);
            }

            // 只更新被替换的行
            if (wasProcessed)
            {
                int currentDbId = allRowIds[i];
                int newTimeSetId = allRows[i][timeSetColumnIndex].toInt();
                updateQuery.bindValue(0, newTimeSetId);
                updateQuery.bindValue(1, currentDbId);
                if (updateQuery.exec())
                {
                    if (updateQuery.numRowsAffected() > 0)
                    {
                        dbUpdatedCount++;
                    }
                }
                else
                {
                    qWarning() << "更新数据库失败，行索引:" << i << ", DB ID:" << currentDbId << ", 错误:" << updateQuery.lastError().text();
                }
            }
        }
        qDebug() << "替换TimeSet - 数据库元数据更新完成，共更新" << dbUpdatedCount << "行。";

        // 提交事务
        if (!db.commit())
        {
            QString errorText = db.lastError().text();
            qDebug() << "替换TimeSet失败 - 提交事务失败:" << errorText;
            throw std::runtime_error(errorText.toStdString());
        }

        // 注意：不要在这里调用onVectorTableSelectionChanged，因为它会重置页码
        qDebug() << "替换TimeSet - 准备重新加载表格数据，将保留当前页码:" << m_currentPage;

        // 显示成功消息
        QMessageBox::information(this, tr("成功"), tr("已将 %1 替换为 %2，共更新了 %3 行数据").arg(fromTimeSetName).arg(toTimeSetName).arg(updatedRowCount));
        qDebug() << "替换TimeSet - 操作成功完成，共更新" << updatedRowCount << "行数据";

        // 保存当前页码并仅刷新当前页数据，不改变页码状态
        int currentIndex = m_vectorTabWidget->currentIndex();
        if (currentIndex >= 0 && m_tabToTableId.contains(currentIndex))
        {
            int tableId = m_tabToTableId[currentIndex];

            // 清除当前表的数据缓存，但不改变页码
            VectorDataHandler::instance().clearTableDataCache(tableId);

            // 直接加载当前页数据，而不是调用refreshVectorTableData
            qDebug() << "替换TimeSet - 刷新当前页数据，保持在页码:" << m_currentPage;
            VectorDataHandler::instance().loadVectorTablePageData(tableId, m_vectorTableWidget, m_currentPage, m_pageSize);

            // 更新分页信息显示
            updatePaginationInfo();
        }
        else
        {
            qWarning() << "替换TimeSet - 无法获取当前向量表ID，回退到使用refreshVectorTableData()";
            refreshVectorTableData();
        }

        // 更新波形图以反映TimeSet变更
        if (m_isWaveformVisible && m_waveformPlot)
        {
            qDebug() << "MainWindow::replaceTimeSetForVectorTable - 更新波形图以反映TimeSet变更";

            // 强制刷新数据库连接，确保没有缓存问题
            QSqlDatabase refreshDb = DatabaseManager::instance()->database();
            if (refreshDb.isOpen())
            {
                qDebug() << "MainWindow::replaceTimeSetForVectorTable - 刷新数据库缓存";
                refreshDb.transaction();
                refreshDb.commit();
            }

            // 短暂延迟以确保数据库变更已经完成
            QTimer::singleShot(100, this, [this]()
                               {
                qDebug() << "MainWindow::replaceTimeSetForVectorTable - 延迟更新波形图";
                updateWaveformView(); });

            // 同时也立即更新一次
            updateWaveformView();
        }
    }
    catch (const std::exception &e)
    {
        // 回滚事务
        db.rollback();
        qDebug() << "替换TimeSet - 操作失败，已回滚事务:" << e.what();

        // 显示错误消息
        QMessageBox::critical(this, tr("错误"), tr("替换TimeSet失败: %1").arg(e.what()));
    }
}

void MainWindow::showReplaceTimeSetDialog()
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

    // 显示替换TimeSet对话框
    ReplaceTimeSetDialog dialog(this);
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
        int fromTimeSetId = dialog.getFromTimeSetId();
        int toTimeSetId = dialog.getToTimeSetId();

        QList<int> rowsToUpdate;
        // 修复BUG：无论是否选中行，都使用对话框中设置的范围
        int fromRow = dialog.getStartRow(); // 0-based
        int toRow = dialog.getEndRow();     // 0-based

        // Ensure the range is valid
        if (fromRow < 0 || toRow < fromRow || toRow >= rowCount)
        {
            QMessageBox::warning(this, "范围无效", "指定的行范围无效。");
            return;
        }

        for (int i = fromRow; i <= toRow; ++i)
        {
            rowsToUpdate.append(i);
        }

        // Replace TimeSet
        replaceTimeSetForVectorTable(fromTimeSetId, toTimeSetId, rowsToUpdate);
    }
}
