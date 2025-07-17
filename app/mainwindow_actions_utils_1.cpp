// 刷新侧边导航栏数据
void MainWindow::refreshSidebarNavigator()
{
    if (!m_sidebarTree || m_currentDbPath.isEmpty() || !DatabaseManager::instance()->isDatabaseConnected())
    {
        return;
    }

    // 临时保存选中状态
    QMap<QString, QString> selectedItems;
    for (int i = 0; i < m_sidebarTree->topLevelItemCount(); i++)
    {
        QTreeWidgetItem *root = m_sidebarTree->topLevelItem(i);
        QString rootType = root->data(0, Qt::UserRole).toString();

        for (int j = 0; j < root->childCount(); j++)
        {
            QTreeWidgetItem *child = root->child(j);
            if (child->isSelected())
            {
                selectedItems[rootType] = child->data(0, Qt::UserRole).toString();
            }
        }

        // 清空子节点，准备重新加载
        while (root->childCount() > 0)
        {
            delete root->takeChild(0);
        }
    }

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();

    // 获取管脚列表
    QTreeWidgetItem *pinRoot = nullptr;
    for (int i = 0; i < m_sidebarTree->topLevelItemCount(); i++)
    {
        if (m_sidebarTree->topLevelItem(i)->data(0, Qt::UserRole).toString() == "pins")
        {
            pinRoot = m_sidebarTree->topLevelItem(i);
            break;
        }
    }

    if (pinRoot)
    {
        QSqlQuery pinQuery(db);
        // 修改查询语句，获取所有管脚，不限于被使用的管脚
        pinQuery.exec("SELECT id, pin_name FROM pin_list WHERE is_placeholder = 0 ORDER BY pin_name");
        while (pinQuery.next())
        {
            int pinId = pinQuery.value(0).toInt();
            QString pinName = pinQuery.value(1).toString();

            QTreeWidgetItem *pinItem = new QTreeWidgetItem(pinRoot);
            pinItem->setText(0, pinName);
            pinItem->setData(0, Qt::UserRole, QString::number(pinId));

            // 恢复选中状态
            if (selectedItems.contains("pins") && selectedItems["pins"] == QString::number(pinId))
            {
                pinItem->setSelected(true);
            }
        }
    }

    // 获取TimeSet列表
    QTreeWidgetItem *timeSetRoot = nullptr;
    for (int i = 0; i < m_sidebarTree->topLevelItemCount(); i++)
    {
        if (m_sidebarTree->topLevelItem(i)->data(0, Qt::UserRole).toString() == "timesets")
        {
            timeSetRoot = m_sidebarTree->topLevelItem(i);
            break;
        }
    }

    if (timeSetRoot)
    {
        QSqlQuery timeSetQuery(db);
        // 修改查询语句，获取所有实际使用的TimeSet
        timeSetQuery.exec("SELECT id, timeset_name FROM timeset_list ORDER BY timeset_name");
        while (timeSetQuery.next())
        {
            int timeSetId = timeSetQuery.value(0).toInt();
            QString timeSetName = timeSetQuery.value(1).toString();

            QTreeWidgetItem *timeSetItem = new QTreeWidgetItem(timeSetRoot);
            timeSetItem->setText(0, timeSetName);
            timeSetItem->setData(0, Qt::UserRole, QString::number(timeSetId));

            // 恢复选中状态
            if (selectedItems.contains("timesets") && selectedItems["timesets"] == QString::number(timeSetId))
            {
                timeSetItem->setSelected(true);
            }
        }
    }

    // 获取向量表列表
    QTreeWidgetItem *vectorTableRoot = nullptr;
    for (int i = 0; i < m_sidebarTree->topLevelItemCount(); i++)
    {
        if (m_sidebarTree->topLevelItem(i)->data(0, Qt::UserRole).toString() == "vectortables")
        {
            vectorTableRoot = m_sidebarTree->topLevelItem(i);
            break;
        }
    }

    if (vectorTableRoot)
    {
        QSqlQuery tableQuery(db);
        tableQuery.exec("SELECT id, table_name FROM vector_tables ORDER BY table_name");
        while (tableQuery.next())
        {
            int tableId = tableQuery.value(0).toInt();
            QString tableName = tableQuery.value(1).toString();

            QTreeWidgetItem *tableItem = new QTreeWidgetItem(vectorTableRoot);
            tableItem->setText(0, tableName);
            tableItem->setData(0, Qt::UserRole, QString::number(tableId));

            // 恢复选中状态
            if (selectedItems.contains("vectortables") && selectedItems["vectortables"] == QString::number(tableId))
            {
                tableItem->setSelected(true);
            }
        }
    }

    // 获取标签列表（从所有向量表中获取唯一的Label值）
    QTreeWidgetItem *labelRoot = nullptr;
    for (int i = 0; i < m_sidebarTree->topLevelItemCount(); i++)
    {
        if (m_sidebarTree->topLevelItem(i)->data(0, Qt::UserRole).toString() == "labels")
        {
            labelRoot = m_sidebarTree->topLevelItem(i);
            break;
        }
    }

    if (labelRoot)
    {
        // 创建一个结构来存储标签及其在向量表中的位置信息
        struct LabelInfo
        {
            QString name; // 标签名称
            int tableId;  // 所属表ID
            int rowIndex; // 在表中的行索引
        };

        // 用于存储所有标签信息的列表
        QList<LabelInfo> labelInfoList;

        // 1. 优先从当前打开的表格中收集未保存的标签数据
        if (m_vectorTableWidget && m_vectorTableWidget->isVisible() && m_vectorTableWidget->columnCount() > 0)
        {
            // 获取当前表的信息
            int tabIndex = m_vectorTabWidget->currentIndex();
            if (tabIndex >= 0 && m_tabToTableId.contains(tabIndex))
            {
                int currentTableId = m_tabToTableId[tabIndex];
                QList<Vector::ColumnInfo> columns = getCurrentColumnConfiguration(currentTableId);

                // 查找Label列的索引
                int labelColumnIndex = -1;
                for (int i = 0; i < columns.size() && i < m_vectorTableWidget->columnCount(); i++)
                {
                    if (columns[i].name.toLower() == "label" && columns[i].is_visible)
                    {
                        // 找到表格中对应的列索引
                        QTableWidgetItem *headerItem = m_vectorTableWidget->horizontalHeaderItem(i);
                        if (headerItem && headerItem->text().toLower() == "label")
                        {
                            labelColumnIndex = i;
                            break;
                        }
                    }
                }

                // 如果找到了Label列，收集当前表格中的标签（包括未保存的修改）
                if (labelColumnIndex >= 0)
                {
                    qDebug() << "MainWindow::refreshSidebarNavigator - 从当前表格UI收集未保存的标签数据，列索引:" << labelColumnIndex;

                    // 用于记录当前表格中已经收集的标签
                    QSet<QString> currentTableLabels;

                    // 从UI表格中收集标签（当前页的数据，包括未保存的修改）
                    for (int row = 0; row < m_vectorTableWidget->rowCount(); row++)
                    {
                        QTableWidgetItem *item = m_vectorTableWidget->item(row, labelColumnIndex);
                        if (item && !item->text().isEmpty())
                        {
                            // 计算全局行索引
                            int globalRowIndex = m_currentPage * m_pageSize + row;

                            LabelInfo info;
                            info.name = item->text();
                            info.tableId = currentTableId;
                            info.rowIndex = globalRowIndex;

                            labelInfoList.append(info);
                            currentTableLabels.insert(item->text());

                            qDebug() << "  - 从UI表格收集标签:" << item->text() << "，行索引:" << globalRowIndex;
                        }
                    }

                    // 获取当前表的所有行数据（包括其他页的数据）
                    bool ok = false;
                    QList<Vector::RowData> allRows;
                    if (m_useNewDataHandler)
                    {
                        allRows = m_robustDataHandler->getAllVectorRows(currentTableId, ok);
                    }
                    else
                    {
                        allRows = VectorDataHandler::instance().getAllVectorRows(currentTableId, ok);
                    }

                    if (ok && !allRows.isEmpty())
                    {
                        qDebug() << "MainWindow::refreshSidebarNavigator - 从二进制文件补充当前表的其他页标签数据";

                        // 从其他页的数据中收集标签（避免与当前页的标签重复）
                        for (int rowIdx = 0; rowIdx < allRows.size(); rowIdx++)
                        {
                            // 跳过当前页中的行
                            int pageStart = m_currentPage * m_pageSize;
                            int pageEnd = pageStart + m_pageSize - 1;
                            if (rowIdx >= pageStart && rowIdx <= pageEnd)
                                continue;

                            if (labelColumnIndex < allRows[rowIdx].size())
                            {
                                QString label = allRows[rowIdx][labelColumnIndex].toString();
                                if (!label.isEmpty() && !currentTableLabels.contains(label))
                                {
                                    LabelInfo info;
                                    info.name = label;
                                    info.tableId = currentTableId;
                                    info.rowIndex = rowIdx;

                                    labelInfoList.append(info);
                                    currentTableLabels.insert(label);

                                    qDebug() << "  - 从二进制文件补充当前表的标签:" << label << "，行索引:" << rowIdx;
                                }
                            }
                        }
                    }
                }
            }
        }

        // 2. 从其他表中收集标签信息
        QSqlQuery tablesQuery(db);
        tablesQuery.exec("SELECT id FROM vector_tables");

        // 获取当前表ID（用于跳过当前表，避免重复收集）
        int currentTableId = -1;
        int tabIndex = m_vectorTabWidget->currentIndex();
        if (tabIndex >= 0 && m_tabToTableId.contains(tabIndex))
        {
            currentTableId = m_tabToTableId[tabIndex];
        }

        while (tablesQuery.next())
        {
            int tableId = tablesQuery.value(0).toInt();

            // 跳过当前打开的表（已经在上面收集过了）
            if (tableId == currentTableId)
            {
                qDebug() << "MainWindow::refreshSidebarNavigator - 跳过当前表ID:" << tableId << "，避免重复收集";
                continue;
            }

            // 从每个表获取Label列信息和二进制文件
            QString binFileName;
            QList<Vector::ColumnInfo> columns;
            int schemaVersion = 0;
            int rowCount = 0;

            if (loadVectorTableMeta(tableId, binFileName, columns, schemaVersion, rowCount))
            {
                // 查找Label列的索引
                int labelColumnIndex = -1;
                for (int i = 0; i < columns.size(); i++)
                {
                    if (columns[i].name.toLower() == "label")
                    {
                        labelColumnIndex = i;
                        break;
                    }
                }

                if (labelColumnIndex >= 0)
                {
                    // 使用VectorDataHandler::getAllVectorRows读取所有行数据
                    bool ok = false;
                    QList<Vector::RowData> allRows;
                    if (m_useNewDataHandler)
                    {
                        allRows = m_robustDataHandler->getAllVectorRows(tableId, ok);
                    }
                    else
                    {
                        allRows = VectorDataHandler::instance().getAllVectorRows(tableId, ok);
                    }

                    if (ok && !allRows.isEmpty())
                    {
                        qDebug() << "MainWindow::refreshSidebarNavigator - 使用getAllVectorRows从表ID:" << tableId
                                 << "成功读取" << allRows.size() << "行数据";

                        // 从所有行数据中收集标签
                        for (int rowIdx = 0; rowIdx < allRows.size(); rowIdx++)
                        {
                            if (labelColumnIndex < allRows[rowIdx].size())
                            {
                                QString label = allRows[rowIdx][labelColumnIndex].toString();
                                if (!label.isEmpty())
                                {
                                    // 收集标签名称及其位置信息
                                    LabelInfo info;
                                    info.name = label;
                                    info.tableId = tableId;
                                    info.rowIndex = rowIdx;

                                    labelInfoList.append(info);
                                    qDebug() << "  - 从表" << tableId << "行" << rowIdx << "收集到标签:" << label;
                                }
                            }
                        }
                    }
                }
            }
        }

        // 按照表ID和行索引排序标签
        std::sort(labelInfoList.begin(), labelInfoList.end(),
                  [](const LabelInfo &a, const LabelInfo &b)
                  {
                      if (a.tableId != b.tableId)
                          return a.tableId < b.tableId;
                      return a.rowIndex < b.rowIndex;
                  });

        qDebug() << "MainWindow::refreshSidebarNavigator - 总共收集到" << labelInfoList.size() << "个标签（按行顺序排序）";

        // 添加排序后的标签到侧边栏
        QSet<QString> addedLabels; // 用于跟踪已添加的标签（避免重复）
        for (const LabelInfo &info : labelInfoList)
        {
            // 只添加尚未添加的标签（保持唯一性）
            if (!addedLabels.contains(info.name))
            {
                QTreeWidgetItem *labelItem = new QTreeWidgetItem(labelRoot);
                labelItem->setText(0, info.name);
                labelItem->setData(0, Qt::UserRole, info.name);
                // 存储标签所属的表ID和行索引
                labelItem->setData(0, Qt::UserRole + 1, info.tableId);
                labelItem->setData(0, Qt::UserRole + 2, info.rowIndex);

                // 恢复选中状态
                if (selectedItems.contains("labels") && selectedItems["labels"] == info.name)
                {
                    labelItem->setSelected(true);
                }

                // 标记为已添加
                addedLabels.insert(info.name);
            }
        }
    }

    // 展开所有根节点
    for (int i = 0; i < m_sidebarTree->topLevelItemCount(); i++)
    {
        m_sidebarTree->topLevelItem(i)->setExpanded(true);
    }
}

// 更新向量列属性栏
void MainWindow::updateVectorColumnProperties(int row, int column)
{
    // 检查是否有向量表被打开
    if (!m_vectorTableWidget || m_vectorTableWidget->columnCount() == 0)
    {
        return;
    }

    // 检查是否有Tab被打开
    if (m_vectorTabWidget->count() == 0 || m_vectorTabWidget->currentIndex() < 0)
    {
        return;
    }

    // 重置16进制输入框的验证状态
    if (m_pinValueField)
    {
        m_pinValueField->setStyleSheet("");
        m_pinValueField->setToolTip("");
        m_pinValueField->setProperty("invalid", false);
    }

    // 获取当前表的列配置信息
    int currentTableId = m_tabToTableId[m_vectorTabWidget->currentIndex()];
    QList<Vector::ColumnInfo> columns = getCurrentColumnConfiguration(currentTableId);

    // 检查列索引是否有效
    if (column < 0 || column >= columns.size())
    {
        return;
    }

    // 获取列类型
    Vector::ColumnDataType colType = columns[column].type;

    // 处理管脚列
    if (colType == Vector::ColumnDataType::PIN_STATE_ID)
    {
        // 获取管脚名称（从列标题获取而不是从单元格获取）
        QTableWidgetItem *headerItem = m_vectorTableWidget->horizontalHeaderItem(column);
        if (headerItem)
        {
            // 获取表头文本并只提取第一行（管脚名称）
            QString headerText = headerItem->text();
            // 按换行符分割文本，取第一行
            QString pinName = headerText.split("\n").at(0);

            // 更新管脚名称标签
            if (m_pinNameLabel)
            {
                m_pinNameLabel->setText(pinName);
            }

            // 更新列名称旁的管脚标签
            if (m_columnNamePinLabel)
            {
                m_columnNamePinLabel->setText(pinName);
            }

            // 获取当前选中的行
            m_currentSelectedRows.clear();
            QList<QTableWidgetItem *> selectedItems = m_vectorTableWidget->selectedItems();
            foreach (QTableWidgetItem *item, selectedItems)
            {
                if (item->column() == column && !m_currentSelectedRows.contains(item->row()))
                {
                    m_currentSelectedRows.append(item->row());
                }
            }

            // 保存当前选中的列
            m_currentHexValueColumn = column;

            // 计算并显示16进制值
            calculateAndDisplayHexValue(m_currentSelectedRows, column);

            // 设置默认错误个数为0
            if (m_errorCountField)
            {
                m_errorCountField->setText("0");
            }

            // 启用连续勾选框
            if (m_continuousSelectCheckBox)
            {
                m_continuousSelectCheckBox->setEnabled(true);
            }
        }
    }
    // 处理非管脚列（清空所有显示）
    else
    {
        // 清空管脚名称标签
        if (m_pinNameLabel)
        {
            m_pinNameLabel->setText("");
        }

        // 清空列名称旁的管脚标签
        if (m_columnNamePinLabel)
        {
            m_columnNamePinLabel->setText("");
        }

        // 清空16进制值输入框
        if (m_pinValueField)
        {
            m_pinValueField->clear();
        }

        // 清空错误个数字段
        if (m_errorCountField)
        {
            m_errorCountField->setText("");
        }

        // 禁用连续勾选框
        if (m_continuousSelectCheckBox)
        {
            m_continuousSelectCheckBox->setEnabled(false);
            m_continuousSelectCheckBox->setChecked(false);
        }

        // 重置当前选中列和行
        m_currentHexValueColumn = -1;
        m_currentSelectedRows.clear();
    }
}

// 计算16进制值并显示在向量列属性栏中
void MainWindow::calculateAndDisplayHexValue(const QList<int> &selectedRows, int column)
{
    // 如果没有选择行或列无效，则直接返回
    if (selectedRows.isEmpty() || column < 0 || !m_vectorTableWidget)
    {
        if (m_pinValueField)
        {
            m_pinValueField->clear();
        }
        return;
    }

    // 只处理前8行数据
    QList<int> processRows = selectedRows;
    if (processRows.size() > 8)
    {
        // 按行号排序，确保从上到下处理
        std::sort(processRows.begin(), processRows.end());
        // 只保留前8行
        processRows = processRows.mid(0, 8);
    }

    // 收集选中单元格的内容
    QStringList cellValues;
    bool only01 = true; // 是否只包含0和1
    bool onlyHL = true; // 是否只包含H和L

    for (int row : processRows)
    {
        QTableWidgetItem *item = m_vectorTableWidget->item(row, column);
        if (!item)
            continue;

        QString cellValue = item->text().trimmed();
        cellValues.append(cellValue);

        // 检查是否只包含0和1
        if (!cellValue.isEmpty() && cellValue != "0" && cellValue != "1")
        {
            only01 = false;
        }

        // 检查是否只包含H和L
        if (!cellValue.isEmpty() && cellValue != "H" && cellValue != "L")
        {
            onlyHL = false;
        }
    }

    QString hexResult;

    // 情况A：纯0和1
    if (only01 && !cellValues.isEmpty())
    {
        QString binaryStr;
        for (const QString &value : cellValues)
        {
            binaryStr += value;
        }

        bool ok;
        int decimal = binaryStr.toInt(&ok, 2);
        if (ok)
        {
            // 根据行数决定16进制格式
            if (processRows.size() <= 4)
            {
                // 少于等于4行，不补零
                hexResult = QString("0x%1").arg(decimal, 0, 16).toUpper().replace("0X", "0x");
            }
            else
            {
                // 超过4行，格式化为两位16进制，不足补0
                hexResult = QString("0x%1").arg(decimal, 2, 16, QChar('0')).toUpper().replace("0X", "0x");
            }
        }
    }
    // 情况B：纯H和L
    else if (onlyHL && !cellValues.isEmpty())
    {
        QString binaryStr;
        for (const QString &value : cellValues)
        {
            if (value == "H")
                binaryStr += "1";
            else if (value == "L")
                binaryStr += "0";
        }

        bool ok;
        int decimal = binaryStr.toInt(&ok, 2);
        if (ok)
        {
            // 根据行数决定16进制格式
            if (processRows.size() <= 4)
            {
                // 少于等于4行，不补零
                hexResult = QString("+0x%1").arg(decimal, 0, 16).toUpper().replace("+0X", "+0x");
            }
            else
            {
                // 超过4行，格式化为两位16进制，不足补0
                hexResult = QString("+0x%1").arg(decimal, 2, 16, QChar('0')).toUpper().replace("+0X", "+0x");
            }
        }
    }
    // 情况C：混合或特殊字符
    else
    {
        // 将所有单元格值连接起来，不截断长度
        hexResult = cellValues.join("");

        // 如果结果太长，可以考虑显示提示信息，表明这是多个单元格的混合值
        if (hexResult.length() > 15)
        {
            // 超过15个字符时截断并添加省略号
            hexResult = hexResult.left(12) + "...";
        }
    }

    // 显示结果
    if (m_pinValueField)
    {
        m_pinValueField->setText(hexResult);
    }
}

void MainWindow::updateWindowTitle(const QString &dbPath)
{
    QString prefix = m_hasUnsavedChanges ? "*" : "";

    if (dbPath.isEmpty())
    {
        setWindowTitle(prefix + tr("向量编辑器"));
    }
    else
    {
        QFileInfo fileInfo(dbPath);
        setWindowTitle(prefix + tr("向量编辑器 - %1").arg(fileInfo.fileName()));
    }
}
