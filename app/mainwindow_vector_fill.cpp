// 显示填充向量对话框
void MainWindow::showFillVectorDialog()
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

    // 获取选中的单元格
    QModelIndexList selectedIndexes = m_vectorTableWidget->selectionModel()->selectedIndexes();
    if (selectedIndexes.isEmpty())
    {
        QMessageBox::warning(this, "操作失败", "请先选择要填充的单元格");
        return;
    }

    // 检查是否选择了同一列的单元格
    int firstColumn = selectedIndexes.first().column();
    bool sameColumn = true;
    for (const QModelIndex &index : selectedIndexes)
    {
        if (index.column() != firstColumn)
        {
            sameColumn = false;
            break;
        }
    }

    if (!sameColumn)
    {
        QMessageBox::warning(this, "操作失败", "请只选择同一列的单元格");
        return;
    }

    // 显示填充向量对话框
    FillVectorDialog dialog(this);
    // 设置向量表总行数
    dialog.setVectorRowCount(rowCount);

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

    if (dialog.exec() == QDialog::Accepted)
    {
        QString value = dialog.getSelectedValue();
        if (value.isEmpty())
        {
            return; // 无效的填充值
        }

        QList<int> rowsToUpdate;
        // 使用对话框中设置的范围
        int fromRow = dialog.getStartRow(); // 0-based
        int toRow = dialog.getEndRow();     // 0-based

        // 确保范围有效
        if (fromRow < 0 || toRow < fromRow || toRow >= rowCount)
        {
            QMessageBox::warning(this, "范围无效", "指定的行范围无效。");
            return;
        }

        for (int i = fromRow; i <= toRow; ++i)
        {
            rowsToUpdate.append(i);
        }

        // 调用执行更新的函数
        fillVectorForVectorTable(value, rowsToUpdate);
    }
}

void MainWindow::fillVectorForVectorTable(const QString &value, const QList<int> &selectedUiRows)
{
    qDebug() << "向量填充 - 开始填充过程";

    // 输出选择的行信息
    if (selectedUiRows.isEmpty())
    {
        qDebug() << "向量填充 - 用户未选择特定行，无法进行操作";
        QMessageBox::warning(this, tr("警告"), tr("请选择要填充的行"));
        return;
    }
    else
    {
        QStringList rowsList;
        for (int row : selectedUiRows)
        {
            rowsList << QString::number(row);
        }
        qDebug() << "向量填充 - 用户选择了以下行：" << rowsList.join(", ");
    }

    // 获取当前选择的向量表
    int currentIndex = m_vectorTableSelector->currentIndex();
    if (currentIndex < 0)
    {
        QMessageBox::warning(this, tr("警告"), tr("请先选择一个向量表"));
        qDebug() << "向量填充 - 未选择向量表，操作取消";
        return;
    }

    int tableId = m_vectorTableSelector->itemData(currentIndex).toInt();
    QString tableName = m_vectorTableSelector->currentText();
    qDebug() << "向量填充 - 当前向量表ID:" << tableId << ", 名称:" << tableName;

    // 获取选中的列
    QModelIndexList selectedIndexes = m_vectorTableWidget->selectionModel()->selectedIndexes();
    if (selectedIndexes.isEmpty())
    {
        QMessageBox::warning(this, tr("警告"), tr("请选择要填充的单元格"));
        qDebug() << "向量填充 - 未选择单元格，操作取消";
        return;
    }

    // 确保所有选中的单元格在同一列
    int targetColumn = selectedIndexes.first().column();
    bool sameColumn = true;
    for (const QModelIndex &index : selectedIndexes)
    {
        if (index.column() != targetColumn)
        {
            sameColumn = false;
            break;
        }
    }

    if (!sameColumn)
    {
        QMessageBox::warning(this, tr("警告"), tr("请只选择同一列的单元格"));
        qDebug() << "向量填充 - 选择了多列单元格，操作取消";
        return;
    }

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        QMessageBox::critical(this, tr("错误"), tr("数据库连接失败"));
        qDebug() << "向量填充失败 - 数据库连接失败";
        return;
    }

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
            qDebug() << "向量填充 - 查询二进制文件名失败:" << errorText;
            throw std::runtime_error(("查询二进制文件名失败: " + errorText).toStdString());
        }

        QString binFileName = fileQuery.value(0).toString();
        if (binFileName.isEmpty())
        {
            qDebug() << "向量填充 - 二进制文件名为空，无法进行填充操作";
            throw std::runtime_error("向量表未配置二进制文件存储，无法进行填充操作");
        }

        // 2. 解析二进制文件路径
        // 使用PathUtils获取项目二进制数据目录
        QString projectBinaryDataDir = Utils::PathUtils::getProjectBinaryDataDirectory(m_currentDbPath);
        if (projectBinaryDataDir.isEmpty())
        {
            QString errorMsg = QString("无法为数据库 '%1' 生成二进制数据目录路径").arg(m_currentDbPath);
            qWarning() << "向量填充 - " << errorMsg;
            throw std::runtime_error(errorMsg.toStdString());
        }

        qDebug() << "向量填充 - 项目二进制数据目录:" << projectBinaryDataDir;

        // 相对路径转绝对路径
        QString absoluteBinFilePath;
        if (QFileInfo(binFileName).isRelative())
        {
            absoluteBinFilePath = QDir(projectBinaryDataDir).absoluteFilePath(binFileName);
            qDebug() << "向量填充 - 相对路径转换为绝对路径:" << binFileName << " -> " << absoluteBinFilePath;
        }
        else
        {
            absoluteBinFilePath = binFileName;
        }

        // 3. 检查二进制文件是否存在
        if (!QFile::exists(absoluteBinFilePath))
        {
            qWarning() << "向量填充 - 二进制文件不存在:" << absoluteBinFilePath;
            throw std::runtime_error(("二进制文件不存在: " + absoluteBinFilePath).toStdString());
        }

        // 4. 查询列定义，找出对应UI列的数据库列
        QSqlQuery colQuery(db);
        colQuery.prepare("SELECT id, column_name, column_order, column_type "
                         "FROM VectorTableColumnConfiguration "
                         "WHERE master_record_id = ? ORDER BY column_order");
        colQuery.addBindValue(tableId);

        if (!colQuery.exec())
        {
            QString errorText = colQuery.lastError().text();
            qDebug() << "向量填充 - 查询列配置失败:" << errorText;
            throw std::runtime_error(("查询列配置失败: " + errorText).toStdString());
        }

        // 收集列信息
        QList<Vector::ColumnInfo> columns;
        while (colQuery.next())
        {
            Vector::ColumnInfo col;
            col.id = colQuery.value(0).toInt();
            col.name = colQuery.value(1).toString();
            col.order = colQuery.value(2).toInt();
            col.type = Vector::columnDataTypeFromString(colQuery.value(3).toString());

            columns.append(col);
        }

        if (columns.isEmpty())
        {
            qDebug() << "向量填充 - 未找到列配置";
            throw std::runtime_error("未找到列配置，无法进行填充操作");
        }

        // 5. 确定UI列对应的实际数据库列
        if (targetColumn >= columns.size())
        {
            qDebug() << "向量填充 - 选中的列索引超出范围:" << targetColumn;
            throw std::runtime_error("选中的列索引超出范围，无法进行填充操作");
        }

        Vector::ColumnInfo &targetColumnInfo = columns[targetColumn];
        qDebug() << "向量填充 - 目标列信息:" << targetColumnInfo.name << ", 类型:" << static_cast<int>(targetColumnInfo.type);

        // 6. 对所选行执行填充操作
        QFile file(absoluteBinFilePath);
        if (!file.open(QIODevice::ReadWrite))
        {
            qDebug() << "向量填充 - 无法打开二进制文件:" << absoluteBinFilePath;
            throw std::runtime_error(("无法打开二进制文件: " + absoluteBinFilePath).toStdString());
        }

        BinaryFileHeader header;
        if (!Persistence::BinaryFileHelper::readBinaryHeader(&file, header))
        {
            file.close();
            qDebug() << "向量填充 - 读取二进制文件头失败:" << absoluteBinFilePath;
            throw std::runtime_error("读取二进制文件头失败");
        }

        // 获取所有行数据
        QList<Vector::RowData> allRows;
        file.close(); // 先关闭文件

        if (!Persistence::BinaryFileHelper::readAllRowsFromBinary(absoluteBinFilePath, columns, header.data_schema_version, allRows))
        {
            qDebug() << "向量填充 - 读取二进制文件数据失败:" << absoluteBinFilePath;
            throw std::runtime_error("读取二进制文件数据失败");
        }

        // 修改指定行数据
        QMap<int, Vector::RowData> modifiedRowsMap;

        foreach (int rowIndex, selectedUiRows)
        {
            if (rowIndex >= 0 && rowIndex < allRows.size())
            {
                Vector::RowData row = allRows[rowIndex];
                if (targetColumn < row.size())
                {
                    row[targetColumn] = value;
                    modifiedRowsMap[rowIndex] = row;
                }
            }
        }

        // 保存修改的行数据
        if (!modifiedRowsMap.isEmpty())
        {
            bool success = Persistence::BinaryFileHelper::updateRowsInBinary(
                absoluteBinFilePath, columns, header.data_schema_version, modifiedRowsMap);

            if (!success)
            {
                qDebug() << "向量填充 - 更新二进制文件失败，尝试全部重写";
                // 如果增量更新失败，尝试全量重写
                for (const auto &entry : modifiedRowsMap.toStdMap())
                {
                    int idx = entry.first;
                    if (idx >= 0 && idx < allRows.size())
                    {
                        allRows[idx] = entry.second;
                    }
                }

                success = Persistence::BinaryFileHelper::writeAllRowsToBinary(
                    absoluteBinFilePath, columns, header.data_schema_version, allRows);

                if (!success)
                {
                    throw std::runtime_error("向量填充 - 写入二进制文件失败");
                }
            }
        }

        // 提交事务
        db.commit();

        // 刷新UI
        refreshVectorTableData();

        // 显示成功消息
        QMessageBox::information(this, tr("操作成功"), tr("向量填充操作已完成"));
        qDebug() << "向量填充 - 操作成功完成";
    }
    catch (const std::exception &e)
    {
        // 回滚事务
        db.rollback();

        // 显示错误消息
        QMessageBox::critical(this, tr("错误"), tr("向量填充失败: %1").arg(e.what()));
        qDebug() << "向量填充 - 操作失败:" << e.what();
    }
}