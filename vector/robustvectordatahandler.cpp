#include "robustvectordatahandler.h"
#include "database/databasemanager.h"
#include "pin/pinvalueedit.h"
#include "vector/vectordatahandler.h"
#include "common/utils/pathutils.h"
#include <QDebug>
#include <QFile>
#include <QScrollBar>
#include <QTableWidgetItem>
#include <QSqlQuery>
#include <QSqlError>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <stdexcept>
#include "database/binaryfilehelper.h"
#include "common/binary_file_format.h"
#include <QCryptographicHash>
#include <limits>
#include <algorithm>
#include <QDataStream>
#include <QSqlDatabase>

RobustVectorDataHandler &RobustVectorDataHandler::instance()
{
    static RobustVectorDataHandler inst;
    return inst;
}

RobustVectorDataHandler::RobustVectorDataHandler(QObject *parent) : QObject(parent) {}

RobustVectorDataHandler::~RobustVectorDataHandler() = default;

bool RobustVectorDataHandler::loadVectorTableData(int tableId, QTableWidget *tableWidget)
{
    qWarning() << "RobustVectorDataHandler::loadVectorTableData is not implemented yet.";
    return false;
}

bool RobustVectorDataHandler::saveVectorTableData(int tableId, QTableWidget *tableWidget, QString &errorMessage)
{
    qWarning() << "RobustVectorDataHandler::saveVectorTableData is not implemented yet.";
    return false;
}

bool RobustVectorDataHandler::saveVectorTableDataPaged(int tableId, QTableWidget *currentPageTable, int currentPage, int pageSize, int totalRows, QString &errorMessage)
{
    qWarning() << "RobustVectorDataHandler::saveVectorTableDataPaged is not implemented yet.";
    return false;
}

void RobustVectorDataHandler::addVectorRow(QTableWidget *table, const QStringList &pinOptions, int rowIdx)
{
    qWarning() << "RobustVectorDataHandler::addVectorRow is not implemented yet.";
}

void RobustVectorDataHandler::addVectorRows(QTableWidget *table, const QStringList &pinOptions, int startRowIdx, int count)
{
    qWarning() << "RobustVectorDataHandler::addVectorRows is not implemented yet.";
}

bool RobustVectorDataHandler::deleteVectorTable(int tableId, QString &errorMessage)
{
    qWarning() << "RobustVectorDataHandler::deleteVectorTable is not implemented yet.";
    return false;
}

bool RobustVectorDataHandler::deleteVectorRows(int tableId, const QList<int> &rowIndexes, QString &errorMessage)
{
    qWarning() << "RobustVectorDataHandler::deleteVectorRows is not implemented yet.";
    return false;
}

bool RobustVectorDataHandler::deleteVectorRowsInRange(int tableId, int fromRow, int toRow, QString &errorMessage)
{
    qWarning() << "RobustVectorDataHandler::deleteVectorRowsInRange is not implemented yet.";
    return false;
}

int RobustVectorDataHandler::getVectorTableRowCount(int tableId)
{
    QString binFileName;
    QList<Vector::ColumnInfo> columns;
    int schemaVersion = 0;
    int totalRowCount = 0;

    if (loadVectorTableMeta(tableId, binFileName, columns, schemaVersion, totalRowCount))
    {
        return totalRowCount;
    }

    qWarning() << "Failed to get row count for table" << tableId;
    return 0;
}

QList<Vector::ColumnInfo> RobustVectorDataHandler::getAllColumnInfo(int tableId)
{
    qWarning() << "RobustVectorDataHandler::getAllColumnInfo is not implemented yet.";
    return {};
}

int RobustVectorDataHandler::getSchemaVersion(int tableId)
{
    qWarning() << "RobustVectorDataHandler::getSchemaVersion is not implemented yet.";
    return 0;
}

QList<Vector::RowData> RobustVectorDataHandler::getAllVectorRows(int tableId, bool &ok)
{
    qWarning() << "RobustVectorDataHandler::getAllVectorRows is not implemented yet.";
    ok = false;
    return {};
}

bool RobustVectorDataHandler::insertVectorRows(int tableId, int startIndex, const QList<Vector::RowData> &rows, int timesetId, const QList<QPair<int, QPair<QString, QPair<int, QString>>>> &selectedPins, QString &errorMessage)
{
    const QString funcName = "RobustVectorDataHandler::insertVectorRows";
    qDebug() << funcName << " - [NEW] 开始插入向量行，表ID:" << tableId
             << "，起始索引:" << startIndex << "，行数:" << rows.size();

    if (rows.isEmpty())
    {
        qDebug() << funcName << " - [NEW] 插入行数为空，操作提前结束。";
        return true;
    }

    // 1. 加载表元数据
    QString binFileName;
    QList<Vector::ColumnInfo> columns;
    int schemaVersion = 0;
    int totalRowCount = 0;

    if (!loadVectorTableMeta(tableId, binFileName, columns, schemaVersion, totalRowCount))
    {
        errorMessage = "无法加载表元数据";
        qWarning() << funcName << " - [NEW] " << errorMessage;
        return false;
    }

    // 2. 获取二进制文件路径
    QString absoluteBinFilePath = resolveBinaryFilePath(tableId, errorMessage);
    if (absoluteBinFilePath.isEmpty())
    {
        qWarning() << funcName << " - [NEW] " << errorMessage;
        return false;
    }

    // 3. 调用BinaryFileHelper执行插入操作
    using Persistence::BinaryFileHelper;
    if (!BinaryFileHelper::insertRowsInBinary(absoluteBinFilePath, columns, schemaVersion, startIndex, rows, errorMessage))
    {
        qWarning() << funcName << " - [NEW] BinaryFileHelper插入行失败: " << errorMessage;
        return false;
    }

    // 4. 更新数据库中的元数据行数
    int newTotalRowCount = totalRowCount + rows.size();
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery query(db);
    query.prepare("UPDATE VectorTableMasterRecord SET row_count = ? WHERE id = ?");
    query.addBindValue(newTotalRowCount);
    query.addBindValue(tableId);

    if (!query.exec())
    {
        errorMessage = "更新数据库行数失败: " + query.lastError().text();
        qWarning() << funcName << " - [NEW] " << errorMessage;
        // 注意：即使这里失败，二进制文件也已经修改，可能需要回滚或标记为不一致状态
        return false;
    }

    qDebug() << funcName << " - [NEW] 向量行数据操作成功完成，新总行数:" << newTotalRowCount;
    return true;
}

bool RobustVectorDataHandler::updateVectorRow(int tableId, int rowIndex, const Vector::RowData &rowData, QString &errorMessage)
{
    qWarning() << "RobustVectorDataHandler::updateVectorRow is not implemented yet.";
    errorMessage = "Function not implemented.";
    return false;
}

bool RobustVectorDataHandler::gotoLine(int tableId, int lineNumber)
{
    qWarning() << "RobustVectorDataHandler::gotoLine is not implemented yet.";
    return false;
}

bool RobustVectorDataHandler::hideVectorTableColumn(int tableId, const QString &columnName, QString &errorMessage)
{
    qWarning() << "RobustVectorDataHandler::hideVectorTableColumn is not implemented yet.";
    return false;
}

bool RobustVectorDataHandler::showVectorTableColumn(int tableId, const QString &columnName, QString &errorMessage)
{
    qWarning() << "RobustVectorDataHandler::showVectorTableColumn is not implemented yet.";
    return false;
}

void RobustVectorDataHandler::cancelOperation()
{
    qWarning() << "RobustVectorDataHandler::cancelOperation is not implemented yet.";
}

void RobustVectorDataHandler::clearCache()
{
    qWarning() << "RobustVectorDataHandler::clearCache is not implemented yet.";
}

void RobustVectorDataHandler::clearTableDataCache(int tableId)
{
    qWarning() << "RobustVectorDataHandler::clearTableDataCache is not implemented yet.";
}

void RobustVectorDataHandler::clearAllTableDataCache()
{
    qWarning() << "RobustVectorDataHandler::clearAllTableDataCache is not implemented yet.";
}

bool RobustVectorDataHandler::loadVectorTablePageData(int tableId, QTableWidget *tableWidget, int pageIndex, int pageSize)
{
    const QString funcName = "RobustVectorDataHandler::loadVectorTablePageData";
    qDebug() << funcName << " - 开始加载分页数据, 表ID:" << tableId
             << ", 页码:" << pageIndex << ", 每页行数:" << pageSize;

    if (!tableWidget)
    {
        qWarning() << funcName << " - tableWidget 为空";
        return false;
    }

    // 禁用表格更新，减少UI重绘，提升性能
    tableWidget->setUpdatesEnabled(false);
    tableWidget->horizontalHeader()->setUpdatesEnabled(false);
    tableWidget->verticalHeader()->setUpdatesEnabled(false);

    // 保存当前滚动条位置
    QScrollBar *vScrollBar = tableWidget->verticalScrollBar();
    QScrollBar *hScrollBar = tableWidget->horizontalScrollBar();
    int vScrollValue = vScrollBar ? vScrollBar->value() : 0;
    int hScrollValue = hScrollBar ? hScrollBar->value() : 0;

    // 阻止信号以避免不必要的更新
    tableWidget->blockSignals(true);

    // 清理现有内容，但保留表头
    tableWidget->clearContents();

    try
    {
        // 1. 使用新的独立方法获取列信息和总行数
        QString binFileName;
        QList<Vector::ColumnInfo> columns;
        int schemaVersion = 0;
        int totalRowCount = 0;

        if (!loadVectorTableMeta(tableId, binFileName, columns, schemaVersion, totalRowCount))
        {
            qWarning() << funcName << " - 无法加载表元数据";

            // 恢复更新和信号
            tableWidget->blockSignals(false);
            tableWidget->verticalHeader()->setUpdatesEnabled(true);
            tableWidget->horizontalHeader()->setUpdatesEnabled(true);
            tableWidget->setUpdatesEnabled(true);
            return false;
        }

        qDebug() << funcName << " - 元数据加载成功, 列数:" << columns.size() << ", 总行数:" << totalRowCount;

        // 过滤仅保留可见的列
        QList<Vector::ColumnInfo> visibleColumns;
        for (const auto &col : columns)
        {
            if (col.is_visible)
            {
                visibleColumns.append(col);
            }
        }

        if (visibleColumns.isEmpty())
        {
            qWarning() << funcName << " - 表 " << tableId << " 没有可见的列";
            tableWidget->setRowCount(0);
            tableWidget->setColumnCount(0);

            // 恢复更新和信号
            tableWidget->blockSignals(false);
            tableWidget->verticalHeader()->setUpdatesEnabled(true);
            tableWidget->horizontalHeader()->setUpdatesEnabled(true);
            tableWidget->setUpdatesEnabled(true);
            return true; // 返回true，因为元数据已加载成功，只是没有可见列
        }

        // 计算分页参数
        int totalPages = (totalRowCount + pageSize - 1) / pageSize; // 向上取整
        if (pageIndex < 0)
            pageIndex = 0;
        if (pageIndex >= totalPages && totalPages > 0)
            pageIndex = totalPages - 1;

        int startRow = pageIndex * pageSize;
        int rowsToLoad = qMin(pageSize, totalRowCount - startRow);

        qDebug() << funcName << " - 分页参数: 总页数=" << totalPages
                 << ", 当前页=" << pageIndex << ", 起始行=" << startRow
                 << ", 加载行数=" << rowsToLoad;

        // 设置表格行数
        tableWidget->setRowCount(rowsToLoad);

        // 如果没有数据，直接返回成功
        if (rowsToLoad <= 0)
        {
            qDebug() << funcName << " - 当前页没有数据, 直接返回";

            // 恢复更新和信号
            tableWidget->blockSignals(false);
            tableWidget->verticalHeader()->setUpdatesEnabled(true);
            tableWidget->horizontalHeader()->setUpdatesEnabled(true);
            tableWidget->setUpdatesEnabled(true);
            return true;
        }

        // 2. 设置表格列
        tableWidget->setColumnCount(visibleColumns.size());

        // 设置表头
        QStringList headers;
        for (const auto &col : visibleColumns)
        {
            // 根据列类型设置表头
            if (col.type == Vector::ColumnDataType::PIN_STATE_ID && !col.data_properties.isEmpty())
            {
                // 获取管脚属性
                int channelCount = col.data_properties["channel_count"].toInt(1);
                int typeId = col.data_properties["type_id"].toInt(1);

                // 获取类型名称
                QString typeName = "In"; // 默认为输入类型
                QSqlQuery typeQuery(DatabaseManager::instance()->database());
                typeQuery.prepare("SELECT type_name FROM type_options WHERE id = ?");
                typeQuery.addBindValue(typeId);
                if (typeQuery.exec() && typeQuery.next())
                {
                    typeName = typeQuery.value(0).toString();
                }

                // 创建带有管脚信息的表头
                QString headerText = col.name + "\nx" + QString::number(channelCount) + "\n" + typeName;
                headers << headerText;
            }
            else
            {
                // 标准列，直接使用列名
                headers << col.name;
            }
        }

        tableWidget->setHorizontalHeaderLabels(headers);

        // 设置表头居中对齐
        for (int i = 0; i < tableWidget->columnCount(); ++i)
        {
            QTableWidgetItem *headerItem = tableWidget->horizontalHeaderItem(i);
            if (headerItem)
            {
                headerItem->setTextAlignment(Qt::AlignCenter);
            }
        }

        // 3. 解析二进制文件路径
        QString errorMsg;
        QString absoluteBinFilePath = resolveBinaryFilePath(tableId, errorMsg);
        if (absoluteBinFilePath.isEmpty())
        {
            qWarning() << funcName << " - 无法解析二进制文件路径: " << errorMsg;

            // 恢复更新和信号
            tableWidget->blockSignals(false);
            tableWidget->verticalHeader()->setUpdatesEnabled(true);
            tableWidget->horizontalHeader()->setUpdatesEnabled(true);
            tableWidget->setUpdatesEnabled(true);
            return false;
        }

        // 4. 读取页面数据
        QList<Vector::RowData> pageRows;
        if (!readPageDataFromBinary(absoluteBinFilePath, columns, schemaVersion, startRow, rowsToLoad, pageRows))
        {
            qWarning() << funcName << " - 无法读取二进制文件数据";

            // 恢复更新和信号
            tableWidget->blockSignals(false);
            tableWidget->verticalHeader()->setUpdatesEnabled(true);
            tableWidget->horizontalHeader()->setUpdatesEnabled(true);
            tableWidget->setUpdatesEnabled(true);
            return false;
        }

        if (pageRows.isEmpty() && totalRowCount > 0)
        {
            qWarning() << funcName << " - 无法获取页面数据";
            tableWidget->setRowCount(0);

            // 恢复更新和信号
            tableWidget->blockSignals(false);
            tableWidget->verticalHeader()->setUpdatesEnabled(true);
            tableWidget->horizontalHeader()->setUpdatesEnabled(true);
            tableWidget->setUpdatesEnabled(true);
            return false;
        }

        // 5. 填充表格数据
        QSqlDatabase db = DatabaseManager::instance()->database();
        for (int row = 0; row < pageRows.size(); ++row)
        {
            const auto &rowData = pageRows[row];
            int visibleColIdx = 0;

            for (int colIndex = 0; colIndex < columns.size(); ++colIndex)
            {
                const auto &column = columns[colIndex];

                // 跳过不可见的列
                if (!column.is_visible)
                {
                    continue;
                }

                if (rowData.size() <= colIndex)
                {
                    qWarning() << funcName << " - 行数据不足，跳过剩余列, 行:" << row;
                    break;
                }

                const QVariant &cellValue = rowData[colIndex];

                // 根据列类型创建表格项
                QTableWidgetItem *item = nullptr;

                switch (column.type)
                {
                case Vector::ColumnDataType::INSTRUCTION_ID:
                {
                    int instructionId = cellValue.toInt();
                    QString instructionText = "Unknown";

                    // 获取指令文本
                    QSqlQuery instructionQuery(db);
                    instructionQuery.prepare("SELECT instruction_text FROM instructions WHERE id = ?");
                    instructionQuery.addBindValue(instructionId);
                    if (instructionQuery.exec() && instructionQuery.next())
                    {
                        instructionText = instructionQuery.value(0).toString();
                    }

                    item = new QTableWidgetItem(instructionText);
                    item->setData(Qt::UserRole, instructionId); // 保存原始ID
                    break;
                }
                case Vector::ColumnDataType::TIMESET_ID:
                {
                    int timesetId = cellValue.toInt();
                    QString timesetName = "Unknown";

                    // 获取TimeSet名称
                    QSqlQuery timesetQuery(db);
                    timesetQuery.prepare("SELECT timeset_name FROM timeset WHERE id = ?");
                    timesetQuery.addBindValue(timesetId);
                    if (timesetQuery.exec() && timesetQuery.next())
                    {
                        timesetName = timesetQuery.value(0).toString();
                    }

                    item = new QTableWidgetItem(timesetName);
                    item->setData(Qt::UserRole, timesetId); // 保存原始ID
                    break;
                }
                case Vector::ColumnDataType::PIN_STATE_ID:
                {
                    // 创建自定义的管脚状态编辑器
                    PinValueLineEdit *pinEdit = new PinValueLineEdit(tableWidget);
                    pinEdit->setAlignment(Qt::AlignCenter);
                    pinEdit->setText(cellValue.toString());
                    tableWidget->setCellWidget(row, visibleColIdx, pinEdit);

                    // 创建一个隐藏项来保存值，以便于后续访问
                    item = new QTableWidgetItem(cellValue.toString());
                    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                    break;
                }
                case Vector::ColumnDataType::BOOLEAN:
                {
                    bool isChecked = cellValue.toBool();
                    item = new QTableWidgetItem();
                    item->setCheckState(isChecked ? Qt::Checked : Qt::Unchecked);
                    item->setTextAlignment(Qt::AlignCenter);
                    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
                    break;
                }
                default:
                    // 文本或其他类型
                    item = new QTableWidgetItem(cellValue.toString());
                    break;
                }

                if (item)
                {
                    tableWidget->setItem(row, visibleColIdx, item);

                    // 设置文本居中对齐
                    if (column.type != Vector::ColumnDataType::PIN_STATE_ID)
                    {
                        item->setTextAlignment(Qt::AlignCenter);
                    }
                }

                visibleColIdx++;
            }
        }

        // 6. 设置行标题为行号
        QStringList rowLabels;
        for (int i = 0; i < rowsToLoad; ++i)
        {
            rowLabels << QString::number(startRow + i + 1); // 显示1-based的行号
        }
        tableWidget->setVerticalHeaderLabels(rowLabels);

        // 恢复滚动条位置
        if (vScrollBar)
            vScrollBar->setValue(vScrollValue);
        if (hScrollBar)
            hScrollBar->setValue(hScrollValue);

        // 恢复信号和更新
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);

        return true;
    }
    catch (const std::exception &e)
    {
        qCritical() << funcName << " - 加载失败，异常:" << e.what();

        // 恢复信号和更新
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);

        return false;
    }
    catch (...)
    {
        qCritical() << funcName << " - 加载失败，未知异常";

        // 恢复信号和更新
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);

        return false;
    }
}

bool RobustVectorDataHandler::loadVectorTablePageDataForModel(int tableId, VectorTableModel *model, int pageIndex, int pageSize)
{
    qWarning() << "RobustVectorDataHandler::loadVectorTablePageDataForModel is not implemented yet.";
    return false;
}

QList<Vector::RowData> RobustVectorDataHandler::getPageData(int tableId, int pageIndex, int pageSize)
{
    const QString funcName = "RobustVectorDataHandler::getPageData";
    QString errorMessage;

    // 1. 加载元数据
    QString binFileName;
    QList<Vector::ColumnInfo> columns;
    int schemaVersion = 0;
    int totalRowCount = 0;
    if (!loadVectorTableMeta(tableId, binFileName, columns, schemaVersion, totalRowCount))
    {
        qWarning() << funcName << " - Failed to load meta for table" << tableId;
        return {};
    }

    // 2. 计算分页参数
    int startRow = pageIndex * pageSize;
    if (startRow >= totalRowCount)
    {
        return {}; // 页码超出范围，返回空列表
    }
    int numRows = std::min(pageSize, totalRowCount - startRow);

    // 3. 获取二进制文件路径
    QString absoluteBinFilePath = resolveBinaryFilePath(tableId, errorMessage);
    if (absoluteBinFilePath.isEmpty())
    {
        qWarning() << funcName << " - " << errorMessage;
        return {};
    }

    // 4. 从二进制文件读取数据
    QList<Vector::RowData> pageRows;
    if (!readPageDataFromBinary(absoluteBinFilePath, columns, schemaVersion, startRow, numRows, pageRows))
    {
        qWarning() << funcName << " - Failed to read page data from binary file for table" << tableId;
        return {};
    }

    qDebug() << funcName << " - Successfully read" << pageRows.size() << "rows for table" << tableId << "page" << pageIndex;

    return pageRows;
}

QList<Vector::ColumnInfo> RobustVectorDataHandler::getVisibleColumns(int tableId)
{
    const QString funcName = "RobustVectorDataHandler::getVisibleColumns";
    QList<Vector::ColumnInfo> result;

    // 获取所有列信息
    QString binFileName;
    QList<Vector::ColumnInfo> allColumns;
    int schemaVersion = 0;
    int totalRowCount = 0;

    if (!loadVectorTableMeta(tableId, binFileName, allColumns, schemaVersion, totalRowCount))
    {
        qWarning() << funcName << " - 无法加载表元数据";
        return result;
    }

    // 过滤出可见的列
    for (const auto &column : allColumns)
    {
        if (column.is_visible)
        {
            result.append(column);
        }
    }

    return result;
}

void RobustVectorDataHandler::markRowAsModified(int tableId, int rowIndex)
{
    qWarning() << "RobustVectorDataHandler::markRowAsModified is not implemented yet.";
}

void RobustVectorDataHandler::clearModifiedRows(int tableId)
{
    qWarning() << "RobustVectorDataHandler::clearModifiedRows is not implemented yet.";
}

bool RobustVectorDataHandler::isRowModified(int tableId, int rowIndex)
{
    qWarning() << "RobustVectorDataHandler::isRowModified is not implemented yet.";
    return false;
}

QString RobustVectorDataHandler::resolveBinaryFilePath(int tableId, QString &errorMsg)
{
    const QString funcName = "RobustVectorDataHandler::resolveBinaryFilePath";
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        errorMsg = "数据库未打开";
        qWarning() << funcName << " - " << errorMsg;
        return QString();
    }

    // 1. 查询二进制文件名
    QSqlQuery query(db);
    query.prepare("SELECT binary_data_filename FROM VectorTableMasterRecord WHERE id = ?");
    query.addBindValue(tableId);
    if (!query.exec() || !query.next())
    {
        errorMsg = "无法获取表 " + QString::number(tableId) + " 的记录: " + query.lastError().text();
        qWarning() << funcName << " - " << errorMsg;
        return QString();
    }
    QString binFileName = query.value(0).toString();
    if (binFileName.isEmpty())
    {
        errorMsg = "表 " + QString::number(tableId) + " 没有关联的二进制文件";
        qWarning() << funcName << " - " << errorMsg;
        return QString();
    }

    // 2. 获取数据库文件路径
    QString dbPath = db.databaseName();
    if (dbPath.isEmpty() || dbPath == ":memory:")
    {
        errorMsg = "无法确定数据库文件路径";
        qWarning() << funcName << " - " << errorMsg;
        return QString();
    }

    // 3. 使用PathUtils获取正确的二进制数据目录
    QString binaryDataDir = Utils::PathUtils::getProjectBinaryDataDirectory(dbPath);
    if (binaryDataDir.isEmpty())
    {
        errorMsg = "无法生成项目二进制数据目录";
        qWarning() << funcName << " - " << errorMsg;
        return QString();
    }

    // 4. 构建并返回最终的绝对路径
    QDir dir(binaryDataDir);
    QString fullPath = dir.absoluteFilePath(binFileName);

    // 文件存在性检查是可选的，因为在创建新表时文件可能尚不存在
    // QFileInfo fileInfo(fullPath);
    // if (!fileInfo.exists())
    // {
    //     qWarning() << funcName << " - 警告: 二进制文件不存在: " << fullPath;
    // }

    qDebug() << funcName << " - 解析后的二进制文件路径: " << fullPath;
    return fullPath;
}

bool RobustVectorDataHandler::loadVectorTableMeta(int tableId, QString &binFileName, QList<Vector::ColumnInfo> &columns,
                                                  int &schemaVersion, int &totalRowCount)
{
    const QString funcName = "RobustVectorDataHandler::loadVectorTableMeta";
    qDebug() << funcName << " - 查询表ID:" << tableId;

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << funcName << " - 数据库未打开";
        return false;
    }

    // 1. 查询主记录表
    QSqlQuery metaQuery(db);
    metaQuery.prepare("SELECT binary_data_filename, data_schema_version, row_count FROM VectorTableMasterRecord WHERE id = ?");
    metaQuery.addBindValue(tableId);
    if (!metaQuery.exec() || !metaQuery.next())
    {
        qWarning() << funcName << " - 查询主记录失败, 表ID:" << tableId << ", 错误:" << metaQuery.lastError().text();
        return false;
    }

    binFileName = metaQuery.value(0).toString();
    schemaVersion = metaQuery.value(1).toInt();
    totalRowCount = metaQuery.value(2).toInt();
    qDebug() << funcName << " - 文件名:" << binFileName << ", schemaVersion:" << schemaVersion << ", rowCount:" << totalRowCount;

    // 2. 查询列结构 - 只加载IsVisible=1的列
    QSqlQuery colQuery(db);
    colQuery.prepare("SELECT id, column_name, column_order, column_type, data_properties, IsVisible FROM VectorTableColumnConfiguration WHERE master_record_id = ? ORDER BY column_order");
    colQuery.addBindValue(tableId);
    if (!colQuery.exec())
    {
        qWarning() << funcName << " - 查询列结构失败, 错误:" << colQuery.lastError().text();
        return false;
    }

    columns.clear();
    while (colQuery.next())
    {
        Vector::ColumnInfo col;
        col.id = colQuery.value(0).toInt();
        col.vector_table_id = tableId;
        col.name = colQuery.value(1).toString();
        col.order = colQuery.value(2).toInt();
        col.original_type_str = colQuery.value(3).toString();
        col.type = Vector::columnDataTypeFromString(col.original_type_str);
        col.is_visible = colQuery.value(5).toBool();

        QString propStr = colQuery.value(4).toString();
        if (!propStr.isEmpty())
        {
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(propStr.toUtf8(), &err);
            qDebug().nospace() << funcName << " - JSON Parsing Details for Column: '" << col.name
                               << "', Input: '" << propStr
                               << "', ErrorCode: " << err.error
                               << " (ErrorStr: \"" << err.errorString()
                               << "\"), IsObject: " << doc.isObject();

            if (err.error == QJsonParseError::NoError && doc.isObject())
            {
                col.data_properties = doc.object();
            }
            else
            {
                qWarning().nospace() << funcName << " - 列属性JSON解析失败, 列: '" << col.name
                                     << "', Input: '" << propStr
                                     << "', ErrorCode: " << err.error
                                     << " (ErrorStr: \"" << err.errorString()
                                     << "\"), IsObject: " << doc.isObject();
            }
        }

        col.logDetails(funcName);
        columns.append(col);
    }

    return true;
}

bool RobustVectorDataHandler::readPageDataFromBinary(const QString &absoluteBinFilePath,
                                                     const QList<Vector::ColumnInfo> &columns,
                                                     int schemaVersion,
                                                     int startRow,
                                                     int numRows,
                                                     QList<Vector::RowData> &pageRows)
{
    const QString funcName = "RobustVectorDataHandler::readPageDataFromBinary";
    qDebug() << funcName << " - 开始读取二进制数据, 文件:" << absoluteBinFilePath
             << ", 起始行:" << startRow << ", 行数:" << numRows;

    QFile file(absoluteBinFilePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << funcName << " - 无法打开文件:" << absoluteBinFilePath << ", 错误:" << file.errorString();
        return false;
    }

    BinaryFileHeader header;
    if (!Persistence::BinaryFileHelper::readBinaryHeader(&file, header))
    {
        qWarning() << funcName << " - 文件头读取失败";
        file.close();
        return false;
    }

    // 版本兼容性检查
    if (header.data_schema_version != schemaVersion)
    {
        qWarning() << funcName << " - 文件schema版本与数据库不一致! 文件:" << header.data_schema_version << ", DB:" << schemaVersion;

        if (header.data_schema_version > schemaVersion)
        {
            qCritical() << funcName << " - 文件版本高于数据库版本，无法加载!";
            file.close();
            return false;
        }
    }

    // 数据范围检查
    quint64 rowCount = header.row_count_in_file;
    if (rowCount == 0)
    {
        qDebug() << funcName << " - 文件中没有数据行";
        file.close();
        return true; // 没有数据，返回true并清空结果集
    }

    // 安全检查：防止整数溢出
    if (rowCount > static_cast<quint64>(std::numeric_limits<int>::max()))
    {
        qCritical() << funcName << " - 文件中的行数超过int类型最大值, 无法处理";
        file.close();
        return false;
    }

    // 请求的数据范围检查
    int totalRows = static_cast<int>(rowCount);
    if (startRow < 0 || startRow >= totalRows)
    {
        qWarning() << funcName << " - 请求的起始行超出范围: " << startRow << ", 总行数: " << totalRows;
        file.close();
        return false;
    }

    // 调整请求的行数
    numRows = std::min(numRows, totalRows - startRow);
    if (numRows <= 0)
    {
        qDebug() << funcName << " - 没有可读取的行";
        file.close();
        return true; // 清空结果集并返回成功
    }

    // 清空结果集
    pageRows.clear();

    // 尝试预分配内存
    try
    {
        pageRows.reserve(numRows);
    }
    catch (const std::bad_alloc &)
    {
        qCritical() << funcName << " - 内存分配失败，无法为" << numRows << "行数据预分配内存";
        file.close();
        return false;
    }

    // 手动计算每一行的位置，而不是使用readRowPositions
    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);

    // 跳过头部
    file.seek(sizeof(BinaryFileHeader));

    // 跳过前面的行
    for (int i = 0; i < startRow; ++i)
    {
        quint32 rowLen = 0;
        in >> rowLen;
        if (in.status() != QDataStream::Ok)
        {
            qWarning() << funcName << " - 行长度读取失败, 行:" << i;
            file.close();
            return false;
        }

        // 跳过这一行的数据
        file.seek(file.pos() + rowLen);
    }

    // 读取请求的行
    for (int i = 0; i < numRows; ++i)
    {
        // 读取行数据长度
        quint32 rowLen = 0;
        in >> rowLen;
        if (in.status() != QDataStream::Ok || rowLen == 0)
        {
            qWarning() << funcName << " - 行长度读取失败, 行索引:" << (startRow + i);
            file.close();
            return false;
        }

        // 检查行长度是否异常
        const quint32 MAX_REASONABLE_ROW_SIZE = 1 * 1024 * 1024; // 1MB 是合理的单行最大值
        if (rowLen > MAX_REASONABLE_ROW_SIZE)
        {
            qCritical() << funcName << " - 检测到异常大的行大小:" << rowLen
                        << "字节，超过合理限制" << MAX_REASONABLE_ROW_SIZE << "字节，行:" << (startRow + i);
            file.close();
            return false;
        }

        // 读取行数据
        QByteArray rowBytes;
        try
        {
            rowBytes.resize(rowLen);
        }
        catch (const std::bad_alloc &)
        {
            qCritical() << funcName << " - 内存分配失败，无法为行" << (startRow + i) << "分配" << rowLen << "字节的内存";
            file.close();
            return false;
        }

        if (file.read(rowBytes.data(), rowLen) != rowLen)
        {
            qWarning() << funcName << " - 行数据读取失败, 行索引:" << (startRow + i);
            file.close();
            return false;
        }

        // 反序列化行数据
        Vector::RowData rowData;
        if (!Persistence::BinaryFileHelper::deserializeRow(rowBytes, columns, header.data_schema_version, rowData))
        {
            qWarning() << funcName << " - 行反序列化失败, 行索引:" << (startRow + i);
            file.close();
            return false;
        }

        pageRows.append(rowData);
    }

    qDebug() << funcName << " - 成功读取" << pageRows.size() << "行数据";
    file.close();
    return true;
}
