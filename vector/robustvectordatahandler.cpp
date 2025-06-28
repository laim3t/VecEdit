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
    qDebug() << funcName << " - [NEW/DYNAMIC] 开始插入向量行，表ID:" << tableId
             << "，起始逻辑行号:" << startIndex << "，行数:" << rows.size();

    if (rows.isEmpty()) {
        return true;
    }

    // 1. 获取元数据和二进制文件路径
    QString binFileName;
    QList<Vector::ColumnInfo> columns;
    int schemaVersion = 0;
    int totalRowCount = 0;
    if (!loadVectorTableMeta(tableId, binFileName, columns, schemaVersion, totalRowCount)) {
        errorMessage = "无法加载表元数据";
        return false;
    }

    QString absoluteBinFilePath = resolveBinaryFilePath(tableId, errorMessage);
    if (absoluteBinFilePath.isEmpty()) {
        return false;
    }

    // 2. 打开文件准备追加
    QFile file(absoluteBinFilePath);
    if (!file.open(QIODevice::Append)) {
        errorMessage = "无法以追加模式打开二进制文件: " + file.errorString();
        return false;
    }

    QSqlDatabase db = DatabaseManager::instance()->database();
    db.transaction(); // 开始事务以确保原子性

    QSqlQuery indexQuery(db);
    indexQuery.prepare(
        "INSERT INTO VectorTableRowIndex (master_record_id, logical_row_number, physical_offset, physical_size) "
        "VALUES (?, ?, ?, ?)");

    // 3. 循环处理每一行，序列化并写入索引
    for (int i = 0; i < rows.size(); ++i) {
        const Vector::RowData &rowData = rows[i];
        
        // a. 序列化
        QByteArray rowBytes = Persistence::BinaryFileHelper::serializeRowDynamic(rowData, columns);
        if (rowBytes.isEmpty()) {
            errorMessage = QString("序列化第 %1 行数据失败").arg(i);
            db.rollback();
            return false;
        }

        // b. 获取 offset 和 size
        qint64 offset = file.size();
        qint64 size = rowBytes.size();

        // c. 写入文件
        if (file.write(rowBytes) != size) {
            errorMessage = "写入二进制文件失败: " + file.errorString();
            db.rollback();
            return false;
        }

        // d. 写入索引数据库
        indexQuery.bindValue(0, tableId);
        indexQuery.bindValue(1, startIndex + i); // 逻辑行号
        indexQuery.bindValue(2, offset);
        indexQuery.bindValue(3, size);
        if (!indexQuery.exec()) {
            errorMessage = "插入行索引失败: " + indexQuery.lastError().text();
            db.rollback();
            return false;
        }
    }

    file.close();

    // 4. 更新主记录表中的总行数
    int newTotalRowCount = totalRowCount + rows.size();
    QSqlQuery masterUpdateQuery(db);
    masterUpdateQuery.prepare("UPDATE VectorTableMasterRecord SET row_count = ? WHERE id = ?");
    masterUpdateQuery.addBindValue(newTotalRowCount);
    masterUpdateQuery.addBindValue(tableId);
    if (!masterUpdateQuery.exec()) {
        errorMessage = "更新主记录行数失败: " + masterUpdateQuery.lastError().text();
        db.rollback();
        return false;
    }

    db.commit(); // 提交事务
    qDebug() << funcName << " - [NEW/DYNAMIC] 成功插入" << rows.size() << "行，新总行数:" << newTotalRowCount;
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
    // 注意：根据您的要求，此函数现在忽略 pageIndex 和 pageSize，始终读取所有数据。
    qDebug() << funcName << " - [INDEXED READ - FULL] 开始读取所有数据，表ID:" << tableId;

    QList<Vector::RowData> allRows;
    
    // 1. 获取元数据和二进制文件路径
    QString binFileName;
    QList<Vector::ColumnInfo> columns;
    int schemaVersion = 0;
    int totalRowCount = 0;
    QString errorMessageForGetPageData; 
    if (!loadVectorTableMeta(tableId, binFileName, columns, schemaVersion, totalRowCount)) {
        qWarning() << funcName << "- 无法加载表元数据";
        return allRows;
    }

    QString absoluteBinFilePath = resolveBinaryFilePath(tableId, errorMessageForGetPageData);
    if (absoluteBinFilePath.isEmpty()) {
        qWarning() << funcName << "- 无法解析二进制文件路径:" << errorMessageForGetPageData;
        return allRows;
    }

    // 2. 查询行索引表以获取 *所有* 活动行的 offset 和 size
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery indexQuery(db);
    indexQuery.prepare(
        "SELECT physical_offset, physical_size FROM VectorTableRowIndex "
        "WHERE master_record_id = :id AND is_active = 1 "
        "ORDER BY logical_row_number"); // 移除 LIMIT 和 OFFSET
    indexQuery.bindValue(":id", tableId);

    if (!indexQuery.exec()) {
        qWarning() << funcName << "- 查询所有行索引失败:" << indexQuery.lastError().text();
        return allRows;
    }

    // 3. 打开文件，并根据索引逐行读取
    QFile file(absoluteBinFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << funcName << "- 无法以只读模式打开二进制文件:" << file.errorString();
        return allRows;
    }

    while (indexQuery.next()) {
        qint64 offset = indexQuery.value(0).toLongLong();
        qint64 size = indexQuery.value(1).toLongLong();

        if (!file.seek(offset)) {
            qWarning() << funcName << "- seek到位置" << offset << "失败";
            continue;
        }

        QByteArray rowBytes = file.read(size);
        if (rowBytes.size() != size) {
            qWarning() << funcName << "- 从位置" << offset << "读取" << size << "字节失败";
            continue;
        }

        Vector::RowData rowData;
        if (Persistence::BinaryFileHelper::deserializeRowDynamic(rowBytes, columns, rowData)) {
            allRows.append(rowData);
        } else {
            qWarning() << funcName << "- 在位置" << offset << "反序列化数据失败";
        }
    }

    qDebug() << funcName << "[INDEXED READ - FULL] 成功读取" << allRows.size() << "行数据";
    return allRows;
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
    // 根据您的新需求，新轨道不需要分页，因此我们忽略 startRow 和 numRows，始终读取所有数据。
    qDebug() << funcName << " - [Full Read Mode] Reading all data from:" << absoluteBinFilePath;

    // 清空输出列表，以防有旧数据
    pageRows.clear();

    // 直接调用 BinaryFileHelper 的静态方法来读取所有行
    // 这个方法封装了打开文件、读取头部、循环读取所有行数据的全部逻辑
    bool success = Persistence::BinaryFileHelper::readAllRowsFromBinary(
        absoluteBinFilePath,
        columns,
        schemaVersion, // 传递 schemaVersion 以进行版本兼容性检查
        pageRows       // 这是输出参数
    );

    if (!success) {
        qWarning() << funcName << " - Failed to read all rows using BinaryFileHelper from file:" << absoluteBinFilePath;
        return false;
    }

    qDebug() << funcName << " - Successfully read" << pageRows.size() << "rows.";
    return true;
}
