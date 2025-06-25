
void TimeSetDialog::updatePeriod(double value)
{
    if (!m_currentTimeSetItem || m_currentTimeSetIndex < 0 || m_currentTimeSetIndex >= m_timeSetDataList.size())
        return;

    // 获取当前TimeSet数据
    TimeSetData &timeSet = m_timeSetDataList[m_currentTimeSetIndex];

    // 更新周期
    if (m_dataAccess->updateTimeSetPeriod(timeSet.dbId, value))
    {
        // 更新内存中的数据
        timeSet.period = value;

        // 更新显示
        double freq = 1000.0 / value;
        m_currentTimeSetItem->setText(0, timeSet.name + "/" + QString::number(value) + "ns=" + QString::number(freq, 'f', 3) + "MHz");
    }
    else
    {
        QMessageBox::critical(this, "错误", "更新TimeSet周期失败");
    }
}

void TimeSetDialog::timeSetSelectionChanged()
{
    QTreeWidget *timeSetTree = m_uiManager->getTimeSetTree();
    QList<QTreeWidgetItem *> selectedItems = timeSetTree->selectedItems();

    if (selectedItems.isEmpty())
    {
        // 没有选中项，禁用边沿操作按钮
        m_uiManager->getAddEdgeButton()->setEnabled(false);
        m_uiManager->getRemoveEdgeButton()->setEnabled(false);
        m_currentTimeSetItem = nullptr;
        m_currentTimeSetIndex = -1;
        return;
    }

    QTreeWidgetItem *selectedItem = selectedItems.first();

    // 检查是否是顶级项（TimeSet项）
    if (selectedItem->parent() == nullptr)
    {
        // 是TimeSet项
        m_currentTimeSetItem = selectedItem;
        m_currentTimeSetIndex = timeSetTree->indexOfTopLevelItem(selectedItem);

        // 启用边沿操作按钮
        m_uiManager->getAddEdgeButton()->setEnabled(true);
        m_uiManager->getRemoveEdgeButton()->setEnabled(false);

        // 更新管脚选择
        int timeSetId = selectedItem->data(0, Qt::UserRole).toInt();
        m_pinManager->selectPinsForTimeSet(timeSetId);
    }
    else
    {
        // 是边沿项
        m_currentTimeSetItem = selectedItem->parent();
        m_currentTimeSetIndex = timeSetTree->indexOfTopLevelItem(m_currentTimeSetItem);

        // 启用边沿操作按钮
        m_uiManager->getAddEdgeButton()->setEnabled(true);
        m_uiManager->getRemoveEdgeButton()->setEnabled(true);

        // 更新管脚选择
        int timeSetId = m_currentTimeSetItem->data(0, Qt::UserRole).toInt();
        m_pinManager->selectPinsForTimeSet(timeSetId);
    }
}

void TimeSetDialog::addEdgeItem()
{
    if (!m_currentTimeSetItem)
        return;

    // 获取当前选中的TimeSet ID
    int timeSetId = m_currentTimeSetItem->data(0, Qt::UserRole).toInt();

    // 获取选中的管脚IDs
    QList<int> selectedPinIds = m_pinManager->getSelectedPinIds();

    if (selectedPinIds.isEmpty())
    {
        QMessageBox::warning(this, "未选择管脚", "请先选择要添加的管脚。");
        return;
    }

    // 获取已存在的边沿项的管脚IDs
    QSet<int> existingPinIds;
    for (int i = 0; i < m_currentTimeSetItem->childCount(); i++)
    {
        QTreeWidgetItem *child = m_currentTimeSetItem->child(i);
        existingPinIds.insert(child->data(0, Qt::UserRole).toInt());
    }

    // 设置默认值
    double defaultT1R = 250;
    double defaultT1F = 750;
    double defaultSTBR = 500;
    int defaultWaveId = m_waveOptions.keys().first(); // 使用第一个波形ID

    // 创建边沿对话框
    TimeSetEdgeDialog dialog(defaultT1R, defaultT1F, defaultSTBR, defaultWaveId, m_waveOptions, this);

    if (dialog.exec() == QDialog::Accepted)
    {
        // 获取用户设置的值
        double t1r = dialog.getT1R();
        double t1f = dialog.getT1F();
        double stbr = dialog.getSTBR();
        int waveId = dialog.getWaveId();

        // 创建边沿数据并添加到UI
        QList<TimeSetEdgeData> newEdges;

        for (int pinId : selectedPinIds)
        {
            // 跳过已存在的管脚
            if (existingPinIds.contains(pinId))
                continue;

            TimeSetEdgeData edge;
            edge.timesetId = timeSetId;
            edge.pinId = pinId;
            edge.t1r = t1r;
            edge.t1f = t1f;
            edge.stbr = stbr;
            edge.waveId = waveId;

            // 添加到UI
            m_edgeManager->addEdgeItem(m_currentTimeSetItem, edge);

            // 添加到列表
            newEdges.append(edge);
        }

        // 获取所有边沿数据并保存到数据库
        if (!newEdges.isEmpty())
        {
            QList<TimeSetEdgeData> allEdges = m_edgeManager->getEdgeDataFromUI(m_currentTimeSetItem, timeSetId);
            m_dataAccess->saveTimeSetEdgesToDatabase(timeSetId, allEdges);

            // 重新加载边沿项以更新显示
            QList<TimeSetEdgeData> edges = m_dataAccess->loadTimeSetEdges(timeSetId);
            m_edgeManager->displayTimeSetEdges(m_currentTimeSetItem, edges, m_waveOptions, m_pinList);
        }
    }
}

void TimeSetDialog::removeEdgeItem()
{
    QTreeWidget *timeSetTree = m_uiManager->getTimeSetTree();
    QList<QTreeWidgetItem *> selectedItems = timeSetTree->selectedItems();

    if (selectedItems.isEmpty())
        return;

    QTreeWidgetItem *selectedItem = selectedItems.first();

    // 确保是边沿项
    if (selectedItem->parent() == nullptr)
        return;

    // 删除边沿项
    if (m_edgeManager->removeEdgeItem(selectedItem))
    {
        // 更新UI状态
        timeSetSelectionChanged();
    }
    else
    {
        QMessageBox::critical(this, "错误", "删除边沿参数失败");
    }
}

void TimeSetDialog::editEdgeItem(QTreeWidgetItem *item, int column)
{
    m_edgeManager->editEdgeItem(item, column, m_waveOptions);
}

void TimeSetDialog::onItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    if (!item)
        return;

    if (item->parent() == nullptr)
    {
        // TimeSet项
        editTimeSetProperties(item, column);
    }
    else
    {
        // 边沿项
        editEdgeItem(item, column);
    }
}

void TimeSetDialog::onPropertyItemChanged(QTreeWidgetItem *item, int column)
{
    // 此方法处理项目属性变更
    // 注意：由于编辑操作（如重命名、修改边沿参数）会在完成后直接更新UI项，
    // 这可能会触发itemChanged信号。为了避免在非用户直接编辑时触发不必要的逻辑（特别是保存），
    // 这个槽函数目前保持为空，或者只包含非常轻量级的UI更新逻辑。
    // 数据的保存应该在用户明确确认操作（如点击OK按钮）时进行。
    Q_UNUSED(item);
    Q_UNUSED(column);
    // 注释掉原来的保存逻辑：
    /*
    if (!item)
        return;

    // 根据项的类型处理不同的更改
    if (item->parent() == nullptr)
    {
        // TimeSet项的更改
        // 这里不处理，因为已经在editTimeSetProperties方法中处理
    }
    else
    {
        // 边沿项的更改
        // 获取父TimeSet项
        QTreeWidgetItem *parentItem = item->parent();
        int timeSetId = parentItem->data(0, Qt::UserRole).toInt();

        // 获取所有边沿数据并保存 (问题在这里！)
        QList<TimeSetEdgeData> edges = m_edgeManager->getEdgeDataFromUI(parentItem, timeSetId);
        m_dataAccess->saveTimeSetEdgesToDatabase(timeSetId, edges);
    }
    */
}

void TimeSetDialog::onPinSelectionChanged()
{
    if (!m_currentTimeSetItem || m_currentTimeSetIndex < 0 || m_currentTimeSetIndex >= m_timeSetDataList.size())
        return;

    // 获取当前TimeSet数据
    TimeSetData &timeSet = m_timeSetDataList[m_currentTimeSetIndex];

    // 获取选中的管脚IDs
    QList<int> selectedPinIds = m_pinManager->getSelectedPinIds();

    // 更新TimeSet的管脚关联
    timeSet.pinIds = selectedPinIds;

    // 保存到数据库
    m_dataAccess->savePinSelection(timeSet.dbId, selectedPinIds);
}

void TimeSetDialog::updatePinSelection(QTreeWidgetItem *item, int column)
{
    if (!item)
        return;

    // 获取当前TimeSet
    QTreeWidgetItem *timeSetItem = (item->parent() == nullptr) ? item : item->parent();
    int timeSetId = timeSetItem->data(0, Qt::UserRole).toInt();

    // 更新管脚选择
    m_pinManager->selectPinsForTimeSet(timeSetId);
}

QString TimeSetDialog::getTimeSetNameById(int id) const
{
    for (const auto &timeSet : m_timeSetDataList)
    {
        if (timeSet.dbId == id)
        {
            return timeSet.name;
        }
    }
    // 如果本地列表找不到（可能已经被标记为删除但尚未从列表移除）
    // 可以考虑再查一次数据库，但目前假设列表数据是相对完整的
    return QString(); // 返回空字符串表示未找到
}

void TimeSetDialog::onAccepted()
{
    qDebug() << "TimeSetDialog::onAccepted - 开始处理确定按钮事件";

    // --- 删除验证 ---
    QStringList conflictingNames;
    QList<int> validatedIdsToDelete;
    QList<int> conflictingIds;

    qDebug() << "TimeSetDialog::onAccepted - 检查待删除TimeSet列表:" << m_timeSetIdsToDelete.size() << "个";

    for (int idToDelete : m_timeSetIdsToDelete)
    {
        if (m_dataAccess->isTimeSetInUse(idToDelete))
        {
            QString name = getTimeSetNameById(idToDelete);
            if (!name.isEmpty())
            {
                conflictingNames.append(name);
            }
            else
            {
                conflictingNames.append(QString::number(idToDelete)); // 如果找不到名字，显示ID
            }
            conflictingIds.append(idToDelete);
            qDebug() << "TimeSetDialog::onAccepted - TimeSet ID:" << idToDelete << "(" << name << ") 正在使用中";
        }
        else
        {
            validatedIdsToDelete.append(idToDelete);
            qDebug() << "TimeSetDialog::onAccepted - TimeSet ID:" << idToDelete << "(" << getTimeSetNameById(idToDelete) << ") 未在使用，可以删除";
        }
    }

    // 如果存在冲突
    if (!conflictingNames.isEmpty())
    {
        qDebug() << "TimeSetDialog::onAccepted - 检测到删除冲突";
        QMessageBox::warning(this, "删除冲突",
                             QString("以下TimeSet正在被向量表使用，无法删除：\n - %1\n\n请先在向量表中修改或删除对这些TimeSet的引用。").arg(conflictingNames.join("\n - ")));

        // 从待删除列表中移除冲突项，保留非冲突项待下次尝试或取消
        m_timeSetIdsToDelete = validatedIdsToDelete;
        qDebug() << "TimeSetDialog::onAccepted - 冲突的TimeSet已从待删除列表移除";

        // **重要：重新加载TimeSet列表以恢复UI中被误删的冲突项**
        qDebug() << "TimeSetDialog::onAccepted - 重新加载TimeSet列表以恢复UI";
        loadExistingTimeSets();

        return; // 阻止对话框关闭
    }

    // --- 执行实际删除 ---
    bool deleteSuccess = true;
    if (!validatedIdsToDelete.isEmpty())
    {
        qDebug() << "TimeSetDialog::onAccepted - 开始执行删除操作，共" << validatedIdsToDelete.size() << "个TimeSet";
        m_db.transaction(); // 开始事务
        for (int idToDelete : validatedIdsToDelete)
        {
            if (!m_dataAccess->deleteTimeSet(idToDelete))
            {
                QString name = getTimeSetNameById(idToDelete);
                qWarning() << "TimeSetDialog::onAccepted - 删除TimeSet失败: ID=" << idToDelete << ", Name=" << name;
                QMessageBox::critical(this, "删除失败", QString("删除TimeSet '%1' (ID: %2) 时发生错误。请检查日志。").arg(name).arg(idToDelete));
                deleteSuccess = false;
                break; // 出现错误则停止删除
            }
            else
            {
                qDebug() << "TimeSetDialog::onAccepted - 成功从数据库删除TimeSet ID:" << idToDelete;
            }
        }

        if (deleteSuccess)
        {
            m_db.commit(); // 提交事务
            qDebug() << "TimeSetDialog::onAccepted - 所有待删除TimeSet已成功删除，事务已提交";
            m_timeSetIdsToDelete.clear(); // 成功删除后清空列表
            // 更新内存中的m_timeSetDataList
            m_timeSetDataList.erase(std::remove_if(m_timeSetDataList.begin(), m_timeSetDataList.end(),
                                                   [&validatedIdsToDelete](const TimeSetData &data)
                                                   {
                                                       return validatedIdsToDelete.contains(data.dbId);
                                                   }),
                                    m_timeSetDataList.end());
        }
        else
        {
            m_db.rollback(); // 回滚事务
            qDebug() << "TimeSetDialog::onAccepted - 删除过程中发生错误，事务已回滚";
            // 重新加载列表以恢复UI到删除前的状态
            loadExistingTimeSets();
            return; // 阻止关闭
        }
    }
    else
    {
        qDebug() << "TimeSetDialog::onAccepted - 没有需要删除的TimeSet";
    }

    // --- 新增：保存所有TimeSet的名称和周期更改 ---
    qDebug() << "TimeSetDialog::onAccepted - 开始保存所有TimeSet的名称和周期";
    bool savePropsSuccess = true;
    for (const TimeSetData &timeSet : m_timeSetDataList)
    {
        // 只更新数据库中已存在的记录 (dbId > 0)
        if (timeSet.dbId > 0)
        {
            qDebug() << "TimeSetDialog::onAccepted - 准备更新TimeSet ID:" << timeSet.dbId << " 名称:" << timeSet.name << " 周期:" << timeSet.period;
            // 分别更新名称和周期。可以考虑合并为一个更新函数，但目前分开处理更清晰
            if (!m_dataAccess->updateTimeSetName(timeSet.dbId, timeSet.name))
            {
                qWarning() << "TimeSetDialog::onAccepted - 更新TimeSet名称失败: ID=" << timeSet.dbId;
                QMessageBox::critical(this, "保存错误", QString("更新TimeSet '%1' (ID: %2) 的名称时发生错误。").arg(timeSet.name).arg(timeSet.dbId));
                savePropsSuccess = false;
                // break; // 可选：一个失败则全部失败
            }
            if (!m_dataAccess->updateTimeSetPeriod(timeSet.dbId, timeSet.period))
            {
                qWarning() << "TimeSetDialog::onAccepted - 更新TimeSet周期失败: ID=" << timeSet.dbId;
                QMessageBox::critical(this, "保存错误", QString("更新TimeSet '%1' (ID: %2) 的周期时发生错误。").arg(timeSet.name).arg(timeSet.dbId));
                savePropsSuccess = false;
                // break; // 可选：一个失败则全部失败
            }
        }
        else
        {
            qDebug() << "TimeSetDialog::onAccepted - 跳过新添加的TimeSet (ID <= 0):" << timeSet.name << "，将在后续步骤处理";
            // 新添加的 TimeSet 的基本信息已在 addTimeSet 时保存，这里不需要重复处理
        }
    }

    if (!savePropsSuccess)
    {
        qDebug() << "TimeSetDialog::onAccepted - 保存TimeSet属性时发生错误";
        // 如果需要，可以在这里回滚之前的删除操作，但会增加复杂性
        loadExistingTimeSets(); // 重新加载以反映部分成功或失败的状态
        return;                 // 阻止对话框关闭
    }
    qDebug() << "TimeSetDialog::onAccepted - 所有TimeSet的名称和周期已尝试保存";

    // --- 保存所有TimeSet的边沿设置 ---
    qDebug() << "TimeSetDialog::onAccepted - 开始保存所有TimeSet的边沿设置";
    bool saveSuccess = true;
    QTreeWidget *tree = m_uiManager->getTimeSetTree();
    for (int i = 0; i < tree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *timeSetItem = tree->topLevelItem(i);
        if (!timeSetItem)
            continue;
        int timeSetId = timeSetItem->data(0, Qt::UserRole).toInt();
        if (timeSetId <= 0)
            continue; // 跳过无效ID

        qDebug() << "TimeSetDialog::onAccepted - 准备保存 TimeSet ID:" << timeSetId << "的边沿";
        QList<TimeSetEdgeData> edges = m_edgeManager->getEdgeDataFromUI(timeSetItem, timeSetId);
        if (!m_dataAccess->saveTimeSetEdgesToDatabase(timeSetId, edges))
        {
            QString timeSetName = getTimeSetNameById(timeSetId);
            qWarning() << "TimeSetDialog::onAccepted - 保存TimeSet ID:" << timeSetId << "(" << timeSetName << ") 的边沿参数失败";
            QMessageBox::critical(this, "保存错误", QString("保存TimeSet '%1' (ID: %2) 的边沿参数时发生错误。请检查日志。").arg(timeSetName).arg(timeSetId));
            saveSuccess = false;
            // 这里可以选择 break 或 continue，取决于是否希望一个失败阻止所有保存
            // 为了数据一致性，可能需要回滚所有更改，但这会增加复杂性。
            // 目前选择继续尝试保存其他的，但标记为失败。
        }
        else
        {
            qDebug() << "TimeSetDialog::onAccepted - 成功保存 TimeSet ID:" << timeSetId << "的边沿";
        }
    }

    if (!saveSuccess)
    {
        qDebug() << "TimeSetDialog::onAccepted - 保存边沿参数时至少发生一个错误";
        // 可以选择在这里回滚之前的删除操作，或者只是阻止对话框关闭并提示用户
        // 重新加载数据以确保UI一致性可能也是必要的
        loadExistingTimeSets(); // 重新加载以反映部分成功或失败的状态
        return;                 // 阻止对话框关闭
    }
    qDebug() << "TimeSetDialog::onAccepted - 所有TimeSet的边沿设置已尝试保存";

    // --- 后续处理（向量表检查等）---
    // 在初始设置模式下，跳过向量表检查
    if (!m_isInitialSetup)
    {
        // 检查是否有向量表
        QSqlQuery query(m_db);
        if (query.exec("SELECT COUNT(*) FROM vector_tables"))
        {
            if (query.next())
            {
                int count = query.value(0).toInt();
                qDebug() << "TimeSetDialog::onAccepted - 检查向量表数量：" << count;

                if (count <= 0)
                {
                    qDebug() << "TimeSetDialog::onAccepted - 未找到向量表，显示错误消息";
                    QMessageBox::information(this, "提示", "没有找到向量表，请先创建向量表");
                    return;
                }
            }
        }
        else
        {
            qDebug() << "TimeSetDialog::onAccepted - 查询向量表失败：" << query.lastError().text();
        }
    }
    else
    {
        qDebug() << "TimeSetDialog::onAccepted - 初始设置模式，跳过向量表检查";
    }

    // 检查TimeSet数量
    qDebug() << "TimeSetDialog::onAccepted - 检查已配置的TimeSet数量：" << m_timeSetDataList.size();

    // 接受对话框
    qDebug() << "TimeSetDialog::onAccepted - 关闭对话框并接受更改";
    accept();
}

void TimeSetDialog::onRejected()
{
    qDebug() << "TimeSetDialog::onRejected - 用户取消操作";
    // 如果是初始设置，确认用户取消
    if (m_isInitialSetup)
    {
        if (QMessageBox::question(this, "确认取消",
                                  "您正在进行初始设置，取消将不会保存任何更改。确定要取消吗？",
                                  QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
        {
            qDebug() << "TimeSetDialog::onRejected - 用户选择不取消初始设置";
            return;
        }
    }

    // 清空待删除列表
    m_timeSetIdsToDelete.clear();
    qDebug() << "TimeSetDialog::onRejected - 待删除TimeSet列表已清空";

    // 重新加载TimeSet列表以恢复UI
    qDebug() << "TimeSetDialog::onRejected - 重新加载TimeSet列表以恢复UI";
    loadExistingTimeSets();

    // 拒绝对话框
    qDebug() << "TimeSetDialog::onRejected - 关闭对话框并拒绝更改";
    reject();
}
