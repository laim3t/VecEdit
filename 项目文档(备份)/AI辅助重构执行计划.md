# AI 辅助重构执行计划 (版本 1.1)

**项目**: `BinaryFileHelper` 到 `RobustBinaryHelper` 的迁移
**目标**: 引导一个 AI 编程助手，逐步、安全、完整地完成本次代码重构。
**核心原则**: 硬切换 (不兼容旧格式)、小步快跑、指令明确、频繁验证。

---

## ▌准备阶段：上下文学习

**目标**: 让 AI 载入并理解所有相关的模块，为后续的代码修改做好准备。

**【任务包 P-1：学习上下文】**

*   **AI 指令**: 
    你好，AI 助手。我们将开始一次代码重构。请首先阅读并完全理解以下所有文件，不要做任何修改，只需在理解后回复"上下文学习完毕，可以开始第一阶段"。

    *   **新引擎**:
        *   `robust_binary_helper.h`
        *   `robust_binary_helper.cpp`
    *   **旧引擎 (将被移除)**:
        *   `database/binaryfilehelper.h`
        *   `database/binaryfilehelper.cpp`
    *   **核心改造目标**:
        *   `vector/vectordatahandler.h`
        *   `vector/vectordatahandler.cpp` (以及它包含的所有 `vectordatahandler_*.cpp` 文件)
    *   **项目构建文件**:
        *   `CMakeLists.txt`

---

## ▌第一阶段：核心改造 `VectorDataHandler`

**目标**: 将 `VectorDataHandler` 的实现完全从 `BinaryFileHelper` 迁移到 `RobustBinaryHelper`。

**【任务包 1.1：修改头文件 `vectordatahandler.h`】**

*   **AI 指令**:
    请对 `vector/vectordatahandler.h` 文件执行以下修改：

    1.  移除旧的头文件包含:
        ```cpp
        // 删除此行
        // #include "../database/binaryfilehelper.h" // 如果存在
        // #include "../common/binary_file_format.h" // 如果存在
        ```
    2.  添加新的头文件包含，并为 `RobustBinaryHelper` 添加一个前向声明:
        ```cpp
        // 在 #include <QObject> 之前添加
        #include <memory>

        // 在 class VectorDataHandler; 之前添加
        namespace Persistence {
            class RobustBinaryHelper;
        }
        ```
    3.  在 `private:` 部分，添加新的成员变量来管理 `RobustBinaryHelper` 实例:
        ```cpp
        // 添加此行
        std::unique_ptr<Persistence::RobustBinaryHelper> m_robustHelper;
        ```
    4.  删除以下为旧缓存机制服务的成员变量：
        ```cpp
        // QMap<int, QList<Vector::RowData>> m_tableDataCache;
        // QMap<int, QDateTime> m_tableCacheTimestamp;
        // QMap<int, QString> m_tableBinaryFilePath;
        // QMap<int, QByteArray> m_tableBinaryFileMD5;
        ```
    5.  删除以下为旧实现服务的私有函数声明：
        ```cpp
        // bool isTableDataCacheValid(int tableId, const QString &binFilePath);
        // void updateTableDataCache(int tableId, const QList<Vector::RowData> &rows, const QString &binFilePath);
        // bool loadVectorTableMeta(int tableId, QString &binFileName, QList<Vector::ColumnInfo> &columns,
        //                          int &schemaVersion, int &totalRowCount);
        // bool readAllRowsFromBinary(const QString &absoluteBinFilePath,
        //                            const QList<Vector::ColumnInfo> &columns,
        //                            int schemaVersion,
        //                            QList<Vector::RowData> &rows);
        ```
    完成后，请提交修改。

---

**【任务包 1.2：清理并重构 `vectordatahandler.cpp` 实现文件】**

*   **AI 指令**:
    现在，请对 `vector/vectordatahandler.cpp` 文件执行以下修改：

    1.  更新文件顶部的 `#include` 列表。移除所有与旧 `BinaryFileHelper` 相关的包含，并添加新 `RobustBinaryHelper` 的包含:
        ```cpp
        // 确保这些行被删除
        // #include "../database/binaryfilehelper.h"
        // #include "../common/binary_file_format.h"
        // #include <QCryptographicHash>
        
        // 确保这行被添加 (如果不存在)
        #include "robust_binary_helper.h"
        ```

    2.  在文件中找到并 **完整删除** 以下四个旧函数的全部实现代码块：
        *   `VectorDataHandler::loadVectorTableMeta`
        *   `VectorDataHandler::readAllRowsFromBinary`
        *   `VectorDataHandler::isTableDataCacheValid`
        *   `VectorDataHandler::updateTableDataCache`

    3.  重写 `getVectorTableRowCount` 函数。用以下实现替换它的整个函数体：
        ```cpp
        int VectorDataHandler::getVectorTableRowCount(int tableId)
        {
            QString errorMsg;
            QString binPath = resolveBinaryFilePath(tableId, errorMsg);
            if (binPath.isEmpty()) { return 0; }

            // 注意：RobustBinaryHelper 的接口是静态的，这在后续多表操作中可能需要重构为实例成员
            // 但根据当前 .h 设计，我们暂时遵循静态调用
            if (!Persistence::RobustBinaryHelper::openFile(binPath)) {
                return 0;
            }
            int count = Persistence::RobustBinaryHelper::getRowCount();
            Persistence::RobustBinaryHelper::closeFile();
            return count;
        }
        ```
    
    4. 重写 `createTable` (在 `vectordatahandler_core.cpp` 中，但指令针对 `Vectordatahandler` 的逻辑)。找到 `createTable` 函数，将其中创建二进制文件的逻辑替换为:
        ```cpp
        // ... (在SQLite操作成功后)
        // 使用新的RobustBinaryHelper创建文件
        if (!Persistence::RobustBinaryHelper::createFile(newBinaryFilePath)) {
            errorMessage = "Failed to create new binary file using RobustBinaryHelper.";
            qWarning() << funcName << "-" << errorMessage;
            // 回滚数据库操作...
            return false;
        }
        ```
    完成后，请提交修改。

---

**【任务包 1.3：重构数据加载和删除逻辑】**

*   **AI 指令**:
    继续修改 `vectordatahandler_*.cpp` 中的逻辑。

    1.  重写 `loadVectorTablePageData` 函数。用以下全新实现替换其整个函数体：
        ```cpp
        bool VectorDataHandler::loadVectorTablePageData(int tableId, QTableWidget *tableWidget, int pageIndex, int pageSize)
        {
            const QString funcName = "loadVectorTablePageData";
            QString errorMsg;
            QString binPath = resolveBinaryFilePath(tableId, errorMsg);
            if (binPath.isEmpty()) {
                qWarning() << funcName << "- 无法解析二进制文件路径:" << errorMsg;
                return false;
            }

            if (!Persistence::RobustBinaryHelper::openFile(binPath)) {
                qWarning() << funcName << "- 使用RobustBinaryHelper打开文件失败:" << binPath;
                return false;
            }

            QList<quint32> allRowIds = Persistence::RobustBinaryHelper::getAllRowIds();
            int totalRows = allRowIds.count();
            
            tableWidget->clearContents();

            int startIdx = pageIndex * pageSize;
            if (startIdx >= totalRows) {
                tableWidget->setRowCount(0);
                Persistence::RobustBinaryHelper::closeFile();
                return true;
            }
            int endIdx = std::min(startIdx + pageSize, totalRows);

            tableWidget->setRowCount(endIdx - startIdx);

            for (int i = startIdx; i < endIdx; ++i) {
                Persistence::RowData rowData;
                quint32 rowId = allRowIds.at(i);
                if (Persistence::RobustBinaryHelper::readRow(rowId, rowData)) {
                    int tableRow = i - startIdx;
                    // 假设 tableWidget 的列数与 rowData 的大小匹配
                    for (int j = 0; j < rowData.size(); ++j) {
                        QTableWidgetItem *item = tableWidget->item(tableRow, j);
                        if (!item) {
                            item = new QTableWidgetItem();
                            tableWidget->setItem(tableRow, j, item);
                        }
                        item->setText(rowData.at(j).toString());
                    }
                } else {
                    qWarning() << funcName << "- 读取行失败, rowId:" << rowId;
                }
            }

            Persistence::RobustBinaryHelper::closeFile();
            return true;
        }
        ```
    2.  重写 `deleteVectorRows` 函数。用以下实现替换其整个函数体：
        ```cpp
        bool VectorDataHandler::deleteVectorRows(int tableId, const QList<int> &rowIndexes, QString &errorMessage)
        {
            QString binPath = resolveBinaryFilePath(tableId, errorMessage);
            if (binPath.isEmpty()) return false;

            if (!Persistence::RobustBinaryHelper::openFile(binPath)) {
                errorMessage = "无法打开二进制文件执行删除。";
                return false;
            }

            QList<quint32> allRowIds = Persistence::RobustBinaryHelper::getAllRowIds();
            bool allSuccess = true;

            for (int rowIndex : rowIndexes) {
                if(rowIndex < 0 || rowIndex >= allRowIds.count()){
                    qWarning() << "deleteVectorRows - 行索引越界:" << rowIndex;
                    continue;
                }
                quint32 rowId = allRowIds.at(rowIndex);
                if (!Persistence::RobustBinaryHelper::deleteRow(rowId)) {
                    allSuccess = false;
                    errorMessage += QString("删除行 %1 (ID: %2) 失败。").arg(rowIndex).arg(rowId);
                }
            }

            Persistence::RobustBinaryHelper::closeFile();
            
            // 更新数据库中的总行数
            if(allSuccess) {
                int newRowCount = Persistence::RobustBinaryHelper::getRowCount();
                // ... 调用 DatabaseManager 更新 VectorTableMasterRecord ...
            }

            return allSuccess;
        }
        ```
    完成后，请提交修改。

---

**【任务包 1.4: 重构数据保存与修改逻辑】**
*   **AI 指令**:
    这是最复杂的部分。请用以下逻辑替换 `saveVectorTableDataPaged`。

    1.  重写 `saveVectorTableDataPaged` 函数:
        ```cpp
        bool VectorDataHandler::saveVectorTableDataPaged(int tableId, QTableWidget *currentPageTable,
                                                         int currentPage, int pageSize, int totalRows,
                                                         QString &errorMessage)
        {
            const QString funcName = "saveVectorTableDataPaged";
            if (!m_modifiedRows.contains(tableId) || m_modifiedRows.value(tableId).isEmpty()) {
                return true; // 没有修改，直接成功
            }

            QString binPath = resolveBinaryFilePath(tableId, errorMessage);
            if (binPath.isEmpty()) { return false; }

            if (!Persistence::RobustBinaryHelper::openFile(binPath)) {
                errorMessage = "无法打开二进制文件进行保存。";
                return false;
            }
            
            QList<quint32> allRowIds = Persistence::RobustBinaryHelper::getAllRowIds();
            bool allSuccess = true;
            
            const QSet<int>& modifiedIndices = m_modifiedRows.value(tableId);

            for (int globalRowIndex : modifiedIndices) {
                // 关键挑战：如何获取不在当前UI页面的已修改行的数据？
                // 这个问题需要更高级的数据模型来解决。
                // 当前实现仅处理在当前显示页面上的修改。
                int pageStartRow = currentPage * pageSize;
                int pageEndRow = pageStartRow + pageSize;

                if (globalRowIndex >= pageStartRow && globalRowIndex < pageEndRow) {
                    int uiRow = globalRowIndex - pageStartRow;
                    if(uiRow < currentPageTable->rowCount()){
                        Persistence::RowData rowData;
                        for(int col = 0; col < currentPageTable->columnCount(); ++col) {
                            rowData.append(currentPageTable->item(uiRow, col)->text());
                        }

                        if(globalRowIndex < allRowIds.count()){
                            quint32 rowId = allRowIds.at(globalRowIndex);
                            if(!Persistence::RobustBinaryHelper::updateRow(rowId, rowData)) {
                                allSuccess = false;
                                errorMessage += QString("更新行 %1 失败.").arg(globalRowIndex);
                            }
                        } else {
                            quint32 newRowId;
                            if(!Persistence::RobustBinaryHelper::addRow(rowData, newRowId)){
                                allSuccess = false;
                                errorMessage += QString("添加新行 %1 失败.").arg(globalRowIndex);
                            }
                        }
                    }
                } else {
                     qWarning() << funcName << "跳过对非当前页已修改行的保存，索引:" << globalRowIndex;
                }
            }

            if(allSuccess) {
                clearModifiedRows(tableId);
            }
            
            Persistence::RobustBinaryHelper::closeFile();
            return allSuccess;
        }
        ```
    完成后，请提交修改。

---

## ▌第二阶段：项目清理

**【任务包 2.1：更新 `CMakeLists.txt`】**

*   **AI 指令**:
    请打开 `CMakeLists.txt` 文件，在 `set(PROJECT_SOURCES ...)` 列表中找到并 **删除** 以下两行：
    ```cmake
    database/binaryfilehelper.h
    database/binaryfilehelper.cpp
    ```
    完成后，请提交修改。

---

**【任务包 2.2：删除废弃的源文件】**

*   **AI 指令**:
    请从文件系统中 **永久删除** 以下两个文件：
    1.  `database/binaryfilehelper.h`
    2.  `database/binaryfilehelper.cpp`
    完成后，请确认删除。

---

## ▌第三阶段：最终验证

**【任务包 3.1：全局审查】**

*   **AI 指令**:
    现在，请在整个项目中（尤其是在 `app/` 目录下）搜索对 `VectorDataHandler` 实例的调用。
    检查所有调用了 `load...` `save...` `delete...` 等方法的地方。
    由于我们改变了底层实现，这些地方的错误处理逻辑和UI反馈（如等待光标、进度条）可能需要调整。
    请列出所有这些调用点，并对每一个调用点，建议是否需要添加或修改UI反馈逻辑。
    这一个任务不需要修改代码，只需要生成一份审查报告。

---

**重构计划结束。** 