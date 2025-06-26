// =====================================================================
// 模型/视图架构下的向量列属性栏实现
// 新视图(QTableView)支持与旧视图(QTableWidget)相同的向量列属性栏功能
//
// 实现功能:
// 1. 显示当前选中管脚列信息
// 2. 十六进制值的计算和显示
// 3. 十六进制值编辑和表格更新
// 4. 连续编辑模式
// 5. 错误计数显示
// =====================================================================

// 向量列属性栏在新视图(Model/View架构)下的实现
// 遵循"实现-包含"模式，此文件被mainwindow.cpp包含

// 处理QTableView选择变化，更新向量列属性栏
void MainWindow::onNewViewSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    // 获取当前表格所有选中的单元格，而不仅仅是最后一次选择变化的部分
    QModelIndexList allSelectedIndexes = m_vectorTableView->selectionModel()->selectedIndexes();

    if (allSelectedIndexes.isEmpty())
    {
        qDebug() << "onNewViewSelectionChanged - 没有选中项，不处理";
        if (m_pinValueField)
        {
            m_pinValueField->clear();
        }
        return;
    }

    qDebug() << "onNewViewSelectionChanged - 选中的总索引数量:" << allSelectedIndexes.size();

    // 确定选中单元格是否都在同一列
    QMap<int, QList<int>> columnToRows; // 列号->该列中选中的行集合

    // 收集每一列中选中的行
    for (const QModelIndex &index : allSelectedIndexes)
    {
        int col = index.column();
        int row = index.row();

        if (!columnToRows.contains(col))
        {
            columnToRows[col] = QList<int>();
        }

        if (!columnToRows[col].contains(row))
        {
            columnToRows[col].append(row);
        }
    }

    // 如果多于一列被选中，找出包含最多行的那一列
    int maxColumn = -1;
    int maxRowCount = 0;

    QMapIterator<int, QList<int>> it(columnToRows);
    while (it.hasNext())
    {
        it.next();
        qDebug() << "列" << it.key() << "有" << it.value().size() << "行被选中";

        if (it.value().size() > maxRowCount)
        {
            maxRowCount = it.value().size();
            maxColumn = it.key();
        }
    }

    if (maxColumn == -1)
    {
        qDebug() << "onNewViewSelectionChanged - 未找到有效列";
        return;
    }

    qDebug() << "onNewViewSelectionChanged - 选择处理列:" << maxColumn << "共" << columnToRows[maxColumn].size() << "行";

    // 更新向量列属性栏，使用确定的列和该列中的所有选中行
    updateVectorColumnPropertiesForModel(columnToRows[maxColumn], maxColumn);
}

// 更新向量列属性栏（基于模型的实现）
void MainWindow::updateVectorColumnPropertiesForModel(const QList<int> &selectedRows, int column)
{
    qDebug() << "updateVectorColumnPropertiesForModel - 开始更新，选中行数:" << selectedRows.size() << "列:" << column;

    // 检查是否有有效的模型
    if (!m_vectorTableModel || column < 0)
    {
        qDebug() << "updateVectorColumnPropertiesForModel - 无效的模型或列索引，退出函数";
        return;
    }

    // 检查是否有Tab被打开
    if (m_vectorTabWidget->count() == 0 || m_vectorTabWidget->currentIndex() < 0)
    {
        qDebug() << "updateVectorColumnPropertiesForModel - 没有打开的Tab页，退出函数";
        return;
    }

    // 重置16进制输入框的验证状态
    if (m_pinValueField)
    {
        m_pinValueField->setStyleSheet("");
        m_pinValueField->setToolTip("");
        m_pinValueField->setProperty("invalid", false);
    }

    // 获取当前表ID
    int currentTableId = m_tabToTableId[m_vectorTabWidget->currentIndex()];
    qDebug() << "updateVectorColumnPropertiesForModel - 当前表ID:" << currentTableId;

    // 获取列配置信息
    QList<Vector::ColumnInfo> columns = getCurrentColumnConfiguration(currentTableId);
    qDebug() << "updateVectorColumnPropertiesForModel - 获取到" << columns.size() << "列配置";

    // 检查列索引是否有效
    if (column < 0 || column >= columns.size())
    {
        qDebug() << "updateVectorColumnPropertiesForModel - 列索引超出范围，退出函数";
        return;
    }

    // 获取列类型
    Vector::ColumnDataType colType = columns[column].type;
    qDebug() << "updateVectorColumnPropertiesForModel - 列类型:" << (int)colType
             << "列名称:" << columns[column].name;

    // 处理管脚列
    if (colType == Vector::ColumnDataType::PIN_STATE_ID)
    {
        qDebug() << "updateVectorColumnPropertiesForModel - 处理管脚列";

        // 获取列标题（从模型获取）
        QString headerText = m_vectorTableModel->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString();
        qDebug() << "updateVectorColumnPropertiesForModel - 列标题:" << headerText;

        // 按换行符分割文本，取第一行（管脚名称）
        QString pinName = headerText.split("\n").at(0);
        qDebug() << "updateVectorColumnPropertiesForModel - 提取的管脚名称:" << pinName;

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

        // 保存当前选中的行和列
        m_currentSelectedRows = selectedRows;
        m_currentHexValueColumn = column;

        qDebug() << "updateVectorColumnPropertiesForModel - 保存当前选择信息 - 行数:"
                 << m_currentSelectedRows.size() << "列:" << m_currentHexValueColumn;

        // 计算并显示16进制值
        calculateAndDisplayHexValueForModel(selectedRows, column);

        // 计算并显示错误个数
        updateErrorCountForModel(selectedRows, column);

        // 启用连续勾选框
        if (m_continuousSelectCheckBox)
        {
            m_continuousSelectCheckBox->setEnabled(true);
        }
    }
    else
    {
        qDebug() << "updateVectorColumnPropertiesForModel - 处理非管脚列，清空相关显示";

        // 处理非管脚列（清空所有显示）
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

// 计算并更新错误个数显示（基于模型的实现）
void MainWindow::updateErrorCountForModel(const QList<int> &selectedRows, int column)
{
    if (selectedRows.isEmpty() || column < 0 || !m_errorCountField || !m_vectorTableModel)
        return;

    // 获取当前表的timeSet列
    int timeSetColumn = -1;
    QAbstractItemModel *model = m_vectorTableModel;

    // 查找TimeSet列（假设列标题为"TimeSet"）
    for (int i = 0; i < model->columnCount(); ++i)
    {
        QString headerText = model->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString();
        if (headerText.contains("TimeSet", Qt::CaseInsensitive))
        {
            timeSetColumn = i;
            break;
        }
    }

    // 如果没有找到TimeSet列，则无法计算错误
    if (timeSetColumn < 0)
    {
        m_errorCountField->setText("0");
        return;
    }

    // 收集选中行的TimeSet值
    QSet<QString> timeSets;
    for (int row : selectedRows)
    {
        QModelIndex timeSetIdx = model->index(row, timeSetColumn);
        QString timeSetValue = model->data(timeSetIdx, Qt::DisplayRole).toString();
        timeSets.insert(timeSetValue);
    }

    // 如果选中的行使用了多个不同的TimeSet，则可能有潜在错误
    int errorCount = timeSets.size() > 1 ? 1 : 0;

    // 更新错误个数字段
    m_errorCountField->setText(QString::number(errorCount));
}

// 计算16进制值并显示在向量列属性栏中（基于模型的实现）
void MainWindow::calculateAndDisplayHexValueForModel(const QList<int> &selectedRows, int column)
{
    // 如果没有选择行或列无效，则直接返回
    if (selectedRows.isEmpty() || column < 0 || !m_vectorTableModel)
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

    // 获取当前表ID和列配置
    int currentTableId = m_tabToTableId[m_vectorTabWidget->currentIndex()];
    QList<Vector::ColumnInfo> columns = getCurrentColumnConfiguration(currentTableId);

    // 确保列索引有效
    if (column >= columns.size())
    {
        qDebug() << "calculateAndDisplayHexValueForModel - 列索引超出范围:" << column;
        return;
    }

    // 获取列类型
    Vector::ColumnDataType colType = columns[column].type;

    // 收集选中单元格的内容（从模型获取）
    QStringList cellValues;
    bool only01 = true; // 是否只包含0和1
    bool onlyHL = true; // 是否只包含H和L

    qDebug() << "calculateAndDisplayHexValueForModel - 开始处理 " << processRows.size() << " 行的数据：";

    for (int row : processRows)
    {
        // 使用模型获取数据 - 对于管脚列，我们使用DisplayRole，因为我们关心实际显示的值
        QModelIndex index = m_vectorTableModel->index(row, column);
        if (!index.isValid())
            continue;

        // 根据列类型获取适当的数据
        QString cellValue;
        if (colType == Vector::ColumnDataType::PIN_STATE_ID)
        {
            // 对于管脚状态列，我们需要原始的字符值（0、1、H、L、X等）
            cellValue = m_vectorTableModel->data(index, Qt::DisplayRole).toString().trimmed();
        }
        else
        {
            // 对于其他类型列，我们可能需要EditRole以获取未格式化的原始数据
            cellValue = m_vectorTableModel->data(index, Qt::EditRole).toString().trimmed();
        }

        cellValues.append(cellValue);

        qDebug() << "Row:" << row << "Column:" << column << "Value:" << cellValue;

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

    // 调试输出收集的所有单元格值
    qDebug() << "收集到的单元格值:" << cellValues.join(",")
             << "only01:" << only01
             << "onlyHL:" << onlyHL;

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
    // 情况C：混合或特殊字符 - 完全匹配老视图的逻辑
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

    // 显示最终结果
    qDebug() << "最终16进制结果:" << hexResult;

    // 显示结果
    if (m_pinValueField)
    {
        m_pinValueField->setText(hexResult);
    }
}

// 处理16进制值编辑后的同步操作（基于模型的实现）
void MainWindow::onHexValueEditedForModel()
{
    // 获取输入的16进制值
    QString hexValue = m_pinValueField->text().trimmed();
    if (hexValue.isEmpty())
        return;

    // 如果输入无效，则不执行任何操作
    if (m_pinValueField->property("invalid").toBool())
    {
        return;
    }

    // 使用已保存的列和行信息
    QList<int> selectedRows = m_currentSelectedRows;
    int selectedColumn = m_currentHexValueColumn;

    // 如果没有有效的选择信息，则不执行操作
    if (selectedColumn < 0 || selectedRows.isEmpty() || !m_vectorTableModel)
    {
        return;
    }

    // 确保行按从上到下排序
    std::sort(selectedRows.begin(), selectedRows.end());

    // 获取当前表的ID和列配置信息
    if (m_vectorTabWidget->currentIndex() < 0)
        return;

    int currentTableId = m_tabToTableId[m_vectorTabWidget->currentIndex()];
    QList<Vector::ColumnInfo> columns = getCurrentColumnConfiguration(currentTableId);

    // 检查列索引是否有效且是否为PIN_STATE_ID列
    if (selectedColumn < 0 || selectedColumn >= columns.size())
        return;

    Vector::ColumnDataType colType = columns[selectedColumn].type;
    if (colType != Vector::ColumnDataType::PIN_STATE_ID)
        return;

    // 判断格式类型和提取16进制值
    bool useHLFormat = false;
    QString hexDigits;
    bool validFormat = false;

    // 统一将输入转为小写以便处理
    QString lowerHexValue = hexValue.toLower();

    if (lowerHexValue.startsWith("+0x"))
    {
        useHLFormat = true;
        hexDigits = lowerHexValue.mid(3);
        validFormat = true;
    }
    else if (lowerHexValue.startsWith("0x"))
    {
        useHLFormat = false;
        hexDigits = lowerHexValue.mid(2);
        validFormat = true;
    }
    else
    {
        return;
    }

    if (!validFormat)
    {
        return;
    }

    QRegExp hexRegex("^[0-9a-fA-F]{1,2}$");
    if (!hexRegex.exactMatch(hexDigits))
    {
        return;
    }

    bool ok;
    int decimalValue = hexDigits.toInt(&ok, 16);
    if (!ok)
    {
        return;
    }

    // 1. 转换为8位二进制字符串
    QString binaryStr = QString::number(decimalValue, 2).rightJustified(8, '0');

    // 2. 确定要操作的行数 (最多8行)
    int rowsToChange = qMin(selectedRows.size(), 8);

    // 3. 从8位字符串中截取右边的部分
    QString finalBinaryStr = binaryStr.right(rowsToChange);

    // 4. 将最终的二进制字符串覆写到选中的单元格（通过模型）
    for (int i = 0; i < finalBinaryStr.length(); ++i)
    {
        int row = selectedRows[i];
        QChar bit = finalBinaryStr[i];
        QString newValue;
        if (useHLFormat)
        {
            newValue = (bit == '1' ? "H" : "L");
        }
        else
        {
            newValue = QString(bit);
        }

        // 通过模型设置数据
        QModelIndex index = m_vectorTableModel->index(row, selectedColumn);
        if (index.isValid())
        {
            m_vectorTableModel->setData(index, newValue, Qt::EditRole);
        }
    }

    // 更新错误计数，数据可能发生变化
    updateErrorCountForModel(selectedRows, selectedColumn);

    // --- 新增：处理回车后的跳转和选择逻辑 ---

    // 1. 确定最后一个被影响的行和总行数
    int lastAffectedRow = -1;
    if (selectedRows.size() <= 8)
    {
        lastAffectedRow = selectedRows.last();
    }
    else
    {
        lastAffectedRow = selectedRows[7]; // n > 8 时，只影响前8行
    }

    int totalRowCount = m_vectorTableModel->rowCount();
    int nextRow = lastAffectedRow + 1;

    // 如果下一行超出表格范围，则不执行任何操作
    if (nextRow >= totalRowCount)
    {
        return;
    }

    // 2. 清除当前选择（使用选择模型）
    m_vectorTableView->selectionModel()->clearSelection();

    // 3. 根据"连续"模式设置新选择
    if (m_continuousSelectCheckBox && m_continuousSelectCheckBox->isChecked())
    {
        // 连续模式开启
        int selectionStartRow = nextRow;
        int selectionEndRow = qMin(selectionStartRow + 7, totalRowCount - 1);

        // 创建选择范围
        QItemSelection selection;
        QModelIndex topLeft = m_vectorTableModel->index(selectionStartRow, selectedColumn);
        QModelIndex bottomRight = m_vectorTableModel->index(selectionEndRow, selectedColumn);
        selection.append(QItemSelectionRange(topLeft, bottomRight));

        // 设置当前项并选中范围
        m_vectorTableView->setCurrentIndex(topLeft);
        m_vectorTableView->selectionModel()->select(selection, QItemSelectionModel::Select);

        // 确保新选区可见
        m_vectorTableView->scrollTo(topLeft, QAbstractItemView::PositionAtTop);

        // 将焦点设置回输入框
        m_pinValueField->setFocus();
        m_pinValueField->selectAll();
    }
    else
    {
        // 连续模式关闭
        QModelIndex nextIndex = m_vectorTableModel->index(nextRow, selectedColumn);
        m_vectorTableView->setCurrentIndex(nextIndex);
    }
}

// 实时验证16进制输入（这个函数与老视图相同，无需修改）
// void MainWindow::validateHexInput(const QString &text) 保持原有实现，不变
// void MainWindow::validateHexInput(const QString &text) 保持原有实现，不变