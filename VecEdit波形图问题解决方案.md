# VecEdit波形图中新增管脚同步问题解决方案

## 问题描述

在VecEdit应用程序中新视图模式（使用QTableView/Model架构）下，当在"设置向量表"中新增管脚后，波形图视图的下拉框没有同步更新新增的管脚。但是在旧视图模式（QTableWidget）下该功能正常工作。

用户操作流程：

1. 在"设置向量表"中新增管脚（如"d"）
2. 管脚成功添加到向量表中
3. 在新视图模式下，波形图的下拉框中没有出现新增的管脚"d"
4. 在旧视图模式下，波形图的下拉框中会正确显示新增的管脚"d"

## 根本原因分析

通过代码分析，发现问题出现在以下几个函数中：

1. `updateWaveformView()` - 此函数负责更新波形图视图，包括管脚下拉框
2. `onWaveformDoubleClicked()` - 此函数处理波形图的双击事件，用于编辑管脚值
3. `highlightWaveformPoint()` - 此函数用于高亮波形图中的特定点

这些函数在获取管脚数据时，只考虑了从旧视图（QTableWidget）获取数据的情况，没有处理从新视图（QTableView/Model）获取数据的代码路径。具体来说：

- 在 `updateWaveformView()` 函数中，在填充管脚选择器下拉框时，只检查了 m_vectorTableWidget 是否存在，没有考虑新视图模式
- 在获取管脚列表时，同样只从 m_vectorTableWidget 获取数据，而没有从 m_vectorTableModel 获取
- 在更新表格数据时，也只考虑了旧视图的更新方式

## 解决方案

解决方案是修改相关函数，使其能够处理两种视图模式：

### 1. 修改 updateWaveformView() 函数

在此函数中添加了对当前视图模式的判断，并根据不同模式使用不同的方法获取管脚数据：

```cpp
// 判断当前使用的视图类型
bool isUsingNewView = (m_vectorStackedWidget && m_vectorStackedWidget->currentIndex() == 1);

if (m_waveformPinSelector->count() == 0)
{
    // 清空现有的选择项
    m_waveformPinSelector->clear();
    
    if (isUsingNewView && m_vectorTableModel)
    {
        // 从模型获取管脚列信息
        for (int col = 0; col < m_vectorTableModel->columnCount(); ++col)
        {
            QVariant headerData = m_vectorTableModel->headerData(col, Qt::Horizontal, Qt::DisplayRole);
            if (headerData.isValid())
            {
                QString headerText = headerData.toString();
                int currentTableId = m_vectorTableSelector->currentData().toInt();
                QList<Vector::ColumnInfo> columns = getCurrentColumnConfiguration(currentTableId);
                if (col < columns.size() && columns[col].type == Vector::ColumnDataType::PIN_STATE_ID)
                {
                    QString displayName = headerText.split('\n').first();
                    m_waveformPinSelector->addItem(displayName, headerText);
                    // 同时收集所有管脚信息
                    pinColumns.append(qMakePair(displayName, col));
                }
            }
        }
    }
    else if (m_vectorTableWidget)
    {
        // 从旧视图获取管脚列信息（原有代码）
        // ...
    }
}
```

### 2. 修改 onWaveformDoubleClicked() 函数

类似地，在此函数中也添加了对当前视图模式的判断：

```cpp
// 判断当前使用的视图类型
bool isUsingNewView = (m_vectorStackedWidget && m_vectorStackedWidget->currentIndex() == 1);
bool hasValidView = (isUsingNewView && m_vectorTableModel) || (!isUsingNewView && m_vectorTableWidget);

if (!hasValidView)
    return;

// 获取所有PIN_STATE_ID类型的列
QList<QPair<QString, int>> pinColumns;

if (isUsingNewView && m_vectorTableModel)
{
    // 从模型获取管脚列信息
    // ...
}
else if (m_vectorTableWidget)
{
    // 从旧视图获取管脚列信息
    // ...
}
```

### 3. 修改 highlightWaveformPoint() 函数

最后，在高亮显示函数中也添加了对当前视图模式的判断：

```cpp
// 判断当前使用的视图类型
bool isUsingNewView = (m_vectorStackedWidget && m_vectorStackedWidget->currentIndex() == 1);

// 从新视图或旧视图中获取管脚列信息
QList<QPair<QString, int>> allPinColumns;

if (isUsingNewView && m_vectorTableModel)
{
    // 从模型获取管脚列信息
    // ...
}
else if (m_vectorTableWidget)
{
    // 从旧视图获取管脚列信息
    // ...
}
```

## 修改的文件

1. `app/mainwindow_waveform_view_1.cpp` - 修改了 `updateWaveformView()` 函数
2. `app/mainwindow_waveform_view_2.cpp` - 修改了 `onWaveformDoubleClicked()` 和 `highlightWaveformPoint()` 函数

## 总结

问题的根本原因是代码中没有正确处理新视图模式（QTableView/Model架构）与旧视图模式（QTableWidget）的差异。在新增管脚后，由于波形图视图相关代码只从旧视图获取数据，而没有从新视图模型中获取数据，导致在新视图模式下波形图的管脚下拉框没有同步更新。

通过修改相关函数，添加对新视图模式的判断和处理逻辑，使波形图能够在两种视图模式下都正确同步显示管脚。特别是在访问和更新管脚数据时，根据当前视图模式选择正确的数据源（QTableWidget或QTableView/Model）。

这种修改保持了现有功能的完整性，同时增强了代码的健壮性，使其能够适应不同的视图模式。
