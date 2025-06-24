// 侧边栏项目点击事件处理
void MainWindow::onSidebarItemClicked(QTreeWidgetItem *item, int column)
{
    if (!item)
        return;

    if (item->parent() == nullptr) // 根节点点击
    {
        QString rootType = item->data(0, Qt::UserRole).toString();

        if (rootType == "pins")
        {
            // 不打开设置对话框，只展开或收起节点
            item->setExpanded(!item->isExpanded());
            // 原来打开设置对话框的代码注释掉
            // openPinSettingsDialog();
        }
        else if (rootType == "timesets")
        {
            // 不打开设置对话框，只展开或收起节点
            item->setExpanded(!item->isExpanded());
            // 原来打开设置对话框的代码注释掉
            // openTimeSetSettingsDialog();
        }
        // 向量表和标签根节点处理
        else
        {
            item->setExpanded(!item->isExpanded());
        }
        return;
    }

    // 子节点点击处理
    QTreeWidgetItem *parentItem = item->parent();
    QString itemType = parentItem->data(0, Qt::UserRole).toString();
    QString itemValue = item->data(0, Qt::UserRole).toString();

    if (itemType == "pins")
    {
        onPinItemClicked(item, column);
    }
    else if (itemType == "timesets")
    {
        onTimeSetItemClicked(item, column);
    }
    else if (itemType == "vectortables")
    {
        onVectorTableItemClicked(item, column);
    }
    else if (itemType == "labels")
    {
        onLabelItemClicked(item, column);
    }
}

// 管脚项目点击事件
void MainWindow::onPinItemClicked(QTreeWidgetItem *item, int column)
{
    if (!item)
        return;

    int pinId = item->data(0, Qt::UserRole).toString().toInt();

    // 查找当前表中与此管脚相关的所有列
    if (m_vectorTableWidget && m_vectorTableWidget->columnCount() > 0)
    {
        bool found = false;
        QString pinName = item->text(0);

        // 遍历所有列，寻找与此管脚名匹配的列
        for (int col = 0; col < m_vectorTableWidget->columnCount(); col++)
        {
            QTableWidgetItem *headerItem = m_vectorTableWidget->horizontalHeaderItem(col);
            // 修改匹配逻辑：检查列头是否以管脚名开头，而不是精确匹配
            if (headerItem && headerItem->text().startsWith(pinName))
            {
                // 找到了匹配的列，高亮显示该列
                m_vectorTableWidget->clearSelection();
                m_vectorTableWidget->selectColumn(col);

                // 滚动到该列
                m_vectorTableWidget->scrollTo(m_vectorTableWidget->model()->index(0, col));

                found = true;
                break;
            }
        }

        if (!found)
        {
            QMessageBox::information(this, "提示", QString("当前表中没有找到管脚 '%1' 对应的列").arg(pinName));
        }
    }
    else
    {
        QMessageBox::information(this, "提示", "没有打开的向量表，无法定位管脚");
    }
}

// TimeSet项目点击事件
void MainWindow::onTimeSetItemClicked(QTreeWidgetItem *item, int column)
{
    if (!item)
        return;

    int timeSetId = item->data(0, Qt::UserRole).toString().toInt();
    QString timeSetName = item->text(0);

    // 在当前表中查找并高亮使用此TimeSet的所有行
    if (m_vectorTableWidget && m_vectorTableWidget->rowCount() > 0)
    {
        bool found = false;
        m_vectorTableWidget->setSelectionMode(QAbstractItemView::MultiSelection);

        // 假设TimeSet列是第三列（索引2），您可能需要根据实际情况进行调整
        int timeSetColumn = -1;
        for (int col = 0; col < m_vectorTableWidget->columnCount(); col++)
        {
            QTableWidgetItem *headerItem = m_vectorTableWidget->horizontalHeaderItem(col);
            if (headerItem && headerItem->text().toLower() == "timeset")
            {
                timeSetColumn = col;
                break;
            }
        }

        if (timeSetColumn >= 0)
        {
            // 遍历所有行，查找使用此TimeSet的行
            for (int row = 0; row < m_vectorTableWidget->rowCount(); row++)
            {
                QTableWidgetItem *item = m_vectorTableWidget->item(row, timeSetColumn);
                if (item && item->text() == timeSetName)
                {
                    m_vectorTableWidget->selectRow(row);
                    if (!found)
                    {
                        // 滚动到第一个找到的行
                        m_vectorTableWidget->scrollTo(m_vectorTableWidget->model()->index(row, 0));
                        found = true;
                    }
                }
            }

            if (!found)
            {
                QMessageBox::information(this, "提示", QString("当前表中没有使用TimeSet '%1' 的行").arg(timeSetName));
            }
        }
        else
        {
            QMessageBox::information(this, "提示", "当前表中没有找到TimeSet列");
        }

        // 恢复为ExtendedSelection选择模式
        m_vectorTableWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    }
    else
    {
        QMessageBox::information(this, "提示", "没有打开的向量表，无法定位TimeSet");
    }
}

// 向量表项目点击事件
void MainWindow::onVectorTableItemClicked(QTreeWidgetItem *item, int column)
{
    if (!item)
        return;

    int tableId = item->data(0, Qt::UserRole).toString().toInt();

    // 在combobox中选中对应的向量表
    for (int i = 0; i < m_vectorTableSelector->count(); i++)
    {
        if (m_vectorTableSelector->itemData(i).toInt() == tableId)
        {
            m_vectorTableSelector->setCurrentIndex(i);
            break;
        }
    }
}

// 标签项目点击事件
void MainWindow::onLabelItemClicked(QTreeWidgetItem *item, int column)
{
    if (!item)
        return;

    const QString funcName = "MainWindow::onLabelItemClicked";
    QString labelText = item->data(0, Qt::UserRole).toString();
    
    // 获取标签所属的表ID和行索引（如果存在）
    int labelTableId = item->data(0, Qt::UserRole + 1).toInt();
    int labelRowIndex = item->data(0, Qt::UserRole + 2).toInt();
    
    qDebug() << funcName << " - 点击标签:" << labelText << "，所属表ID:" << labelTableId << "，行索引:" << labelRowIndex;

    // 获取当前表ID
    int tabIndex = m_vectorTabWidget->currentIndex();
    if (tabIndex < 0 || !m_tabToTableId.contains(tabIndex))
    {
        qWarning() << funcName << " - 没有选中有效的向量表";
        return;
    }

    int currentTableId = m_tabToTableId[tabIndex];
    qDebug() << funcName << " - 当前向量表ID:" << currentTableId;

    // 如果标签来自其他表，需要先切换到对应的表
    if (labelTableId > 0 && labelTableId != currentTableId)
    {
        qDebug() << funcName << " - 标签来自不同的表，需要先切换表";
        
        // 查找表ID对应的索引
        int targetTabIndex = -1;
        for (auto it = m_tabToTableId.begin(); it != m_tabToTableId.end(); ++it) {
            if (it.value() == labelTableId) {
                targetTabIndex = it.key();
                break;
            }
        }
        
        if (targetTabIndex >= 0 && targetTabIndex < m_vectorTabWidget->count())
        {
            // 切换到目标表
            m_vectorTabWidget->setCurrentIndex(targetTabIndex);
            // 更新当前表ID
            currentTableId = labelTableId;
            qDebug() << funcName << " - 已切换到表ID:" << currentTableId << "，Tab索引:" << targetTabIndex;
        }
        else
        {
            // 如果表未打开，则打开它
            QSqlQuery query(DatabaseManager::instance()->database());
            query.prepare("SELECT table_name FROM vector_tables WHERE id = ?");
            query.addBindValue(labelTableId);
            
            if (query.exec() && query.next())
            {
                QString tableName = query.value(0).toString();
                qDebug() << funcName << " - 表未打开，正在打开表:" << tableName << "，ID:" << labelTableId;
                
                // 打开表
                openVectorTable(labelTableId, tableName);
                
                // 更新当前表ID
                tabIndex = m_vectorTabWidget->currentIndex();
                if (tabIndex >= 0 && m_tabToTableId.contains(tabIndex))
                {
                    currentTableId = m_tabToTableId[tabIndex];
                }
            }
            else
            {
                qWarning() << funcName << " - 无法找到表ID:" << labelTableId;
                return;
            }
        }
    }

    // 如果我们有精确的行索引，直接跳转
    if (labelRowIndex >= 0)
    {
        qDebug() << funcName << " - 使用已知行索引直接跳转:" << labelRowIndex;
        
        // 计算目标行所在的页码
        int targetPage = labelRowIndex / m_pageSize;
        int rowInPage = labelRowIndex % m_pageSize;

        qDebug() << funcName << " - 标签所在页码:" << targetPage << "，页内行号:" << rowInPage;

        // 如果不是当前页，需要切换页面
        if (targetPage != m_currentPage)
        {
            qDebug() << funcName << " - 跳转到页码:" << targetPage;
            m_currentPage = targetPage;
            loadCurrentPage();
        }

        // 在页面中选中行
        if (rowInPage >= 0 && rowInPage < m_vectorTableWidget->rowCount())
        {
            m_vectorTableWidget->selectRow(rowInPage);
            m_vectorTableWidget->scrollToItem(m_vectorTableWidget->item(rowInPage, 0));
            qDebug() << funcName << " - 已选中页内行:" << rowInPage;
            return;
        }
    }

    // 如果没有精确的行索引或者跳转失败，则在当前表中查找标签
    // 1. 首先在当前页中查找
    bool foundInCurrentPage = false;
    if (m_vectorTableWidget && m_vectorTableWidget->rowCount() > 0)
    {
        // 获取当前表的列配置
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

        // 如果找到了Label列，在当前页中查找
        if (labelColumnIndex >= 0)
        {
            for (int row = 0; row < m_vectorTableWidget->rowCount(); row++)
            {
                QTableWidgetItem *item = m_vectorTableWidget->item(row, labelColumnIndex);
                if (item && item->text() == labelText)
                {
                    m_vectorTableWidget->selectRow(row);
                    m_vectorTableWidget->scrollToItem(item);
                    qDebug() << funcName << " - 在当前页找到标签，行:" << row;
                    foundInCurrentPage = true;
                    break;
                }
            }
        }
    }

    // 2. 如果当前页未找到，则在所有数据中查找
    if (!foundInCurrentPage)
    {
        qDebug() << funcName << " - 当前页未找到标签，开始在所有数据中查找";

        // 从二进制文件中读取所有行数据
        bool ok = false;
        QList<Vector::RowData> allRows = VectorDataHandler::instance().getAllVectorRows(currentTableId, ok);

        if (ok && !allRows.isEmpty())
        {
            // 查找Label列的索引
            QList<Vector::ColumnInfo> columns = getCurrentColumnConfiguration(currentTableId);
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
                // 在所有行中查找匹配的标签
                int matchingRowIndex = -1;
                for (int i = 0; i < allRows.size(); i++)
                {
                    if (labelColumnIndex < allRows[i].size() &&
                        allRows[i][labelColumnIndex].toString() == labelText)
                    {
                        matchingRowIndex = i;
                        qDebug() << funcName << " - 在所有数据中找到标签，全局行索引:" << matchingRowIndex;
                        break;
                    }
                }

                if (matchingRowIndex >= 0)
                {
                    // 计算目标行所在的页码
                    int targetPage = matchingRowIndex / m_pageSize;
                    int rowInPage = matchingRowIndex % m_pageSize;

                    qDebug() << funcName << " - 标签所在页码:" << targetPage << "，页内行号:" << rowInPage;

                    // 如果不是当前页，需要切换页面
                    if (targetPage != m_currentPage)
                    {
                        qDebug() << funcName << " - 跳转到页码:" << targetPage;
                        m_currentPage = targetPage;
                        loadCurrentPage();
                    }

                    // 在页面中选中行
                    if (rowInPage >= 0 && rowInPage < m_vectorTableWidget->rowCount())
                    {
                        m_vectorTableWidget->selectRow(rowInPage);
                        m_vectorTableWidget->scrollToItem(m_vectorTableWidget->item(rowInPage, 0));
                        qDebug() << funcName << " - 已选中页内行:" << rowInPage;
                    }
                }
                else
                {
                    qWarning() << funcName << " - 在所有数据中未找到标签:" << labelText;
                }
            }
        }
        else
        {
            qWarning() << funcName << " - 无法获取所有行数据";
        }
    }
}

void MainWindow::onProjectStructureItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    // 双击时展开或折叠项目
    if (item)
    {
        item->setExpanded(!item->isExpanded());
    }
}
