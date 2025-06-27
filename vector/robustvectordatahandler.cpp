#include "robustvectordatahandler.h"
#include "database/databasemanager.h"
#include "pin/pinvalueedit.h"
#include "vector/vectordatahandler.h"
#include <QDebug>
#include <QFile>
#include <QScrollBar>
#include <QTableWidgetItem>
#include <QSqlQuery>
#include <QSqlError>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <stdexcept>

RobustVectorDataHandler& RobustVectorDataHandler::instance() {
    static RobustVectorDataHandler inst;
    return inst;
}

RobustVectorDataHandler::RobustVectorDataHandler(QObject* parent) : QObject(parent) {}

RobustVectorDataHandler::~RobustVectorDataHandler() = default;

bool RobustVectorDataHandler::loadVectorTableData(int tableId, QTableWidget* tableWidget) {
    qWarning() << "RobustVectorDataHandler::loadVectorTableData is not implemented yet.";
    return false;
}

bool RobustVectorDataHandler::saveVectorTableData(int tableId, QTableWidget* tableWidget, QString& errorMessage) {
    qWarning() << "RobustVectorDataHandler::saveVectorTableData is not implemented yet.";
    return false;
}

bool RobustVectorDataHandler::saveVectorTableDataPaged(int tableId, QTableWidget* currentPageTable, int currentPage, int pageSize, int totalRows, QString& errorMessage) {
    qWarning() << "RobustVectorDataHandler::saveVectorTableDataPaged is not implemented yet.";
    return false;
}

void RobustVectorDataHandler::addVectorRow(QTableWidget* table, const QStringList& pinOptions, int rowIdx) {
    qWarning() << "RobustVectorDataHandler::addVectorRow is not implemented yet.";
}

void RobustVectorDataHandler::addVectorRows(QTableWidget* table, const QStringList& pinOptions, int startRowIdx, int count) {
    qWarning() << "RobustVectorDataHandler::addVectorRows is not implemented yet.";
}

bool RobustVectorDataHandler::deleteVectorTable(int tableId, QString& errorMessage) {
    qWarning() << "RobustVectorDataHandler::deleteVectorTable is not implemented yet.";
    return false;
}

bool RobustVectorDataHandler::deleteVectorRows(int tableId, const QList<int>& rowIndexes, QString& errorMessage) {
    qWarning() << "RobustVectorDataHandler::deleteVectorRows is not implemented yet.";
    return false;
}

bool RobustVectorDataHandler::deleteVectorRowsInRange(int tableId, int fromRow, int toRow, QString& errorMessage) {
    qWarning() << "RobustVectorDataHandler::deleteVectorRowsInRange is not implemented yet.";
    return false;
}

int RobustVectorDataHandler::getVectorTableRowCount(int tableId) {
    qWarning() << "RobustVectorDataHandler::getVectorTableRowCount is not implemented yet.";
    return 0;
}

QList<Vector::ColumnInfo> RobustVectorDataHandler::getAllColumnInfo(int tableId) {
    qWarning() << "RobustVectorDataHandler::getAllColumnInfo is not implemented yet.";
    return {};
}

int RobustVectorDataHandler::getSchemaVersion(int tableId) {
    qWarning() << "RobustVectorDataHandler::getSchemaVersion is not implemented yet.";
    return 0;
}

QList<Vector::RowData> RobustVectorDataHandler::getAllVectorRows(int tableId, bool& ok) {
    qWarning() << "RobustVectorDataHandler::getAllVectorRows is not implemented yet.";
    ok = false;
    return {};
}

bool RobustVectorDataHandler::insertVectorRows(int tableId, int startIndex, int rowCount, int timesetId, QTableWidget* dataTable, bool appendToEnd, const QList<QPair<int, QPair<QString, QPair<int, QString>>>>& selectedPins, QString& errorMessage) {
    qWarning() << "RobustVectorDataHandler::insertVectorRows is not implemented yet.";
    return false;
}

bool RobustVectorDataHandler::gotoLine(int tableId, int lineNumber) {
    qWarning() << "RobustVectorDataHandler::gotoLine is not implemented yet.";
    return false;
}

bool RobustVectorDataHandler::hideVectorTableColumn(int tableId, const QString& columnName, QString& errorMessage) {
    qWarning() << "RobustVectorDataHandler::hideVectorTableColumn is not implemented yet.";
    return false;
}

bool RobustVectorDataHandler::showVectorTableColumn(int tableId, const QString& columnName, QString& errorMessage) {
    qWarning() << "RobustVectorDataHandler::showVectorTableColumn is not implemented yet.";
    return false;
}

void RobustVectorDataHandler::cancelOperation() {
    qWarning() << "RobustVectorDataHandler::cancelOperation is not implemented yet.";
}

void RobustVectorDataHandler::clearCache() {
    qWarning() << "RobustVectorDataHandler::clearCache is not implemented yet.";
}

void RobustVectorDataHandler::clearTableDataCache(int tableId) {
    qWarning() << "RobustVectorDataHandler::clearTableDataCache is not implemented yet.";
}

void RobustVectorDataHandler::clearAllTableDataCache() {
    qWarning() << "RobustVectorDataHandler::clearAllTableDataCache is not implemented yet.";
}

bool RobustVectorDataHandler::loadVectorTablePageData(int tableId, QTableWidget* tableWidget, int pageIndex, int pageSize) {
    const QString funcName = "RobustVectorDataHandler::loadVectorTablePageData";
    qDebug() << funcName << " - 开始加载分页数据, 表ID:" << tableId
             << ", 页码:" << pageIndex << ", 每页行数:" << pageSize;

    if (!tableWidget) {
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

    try {
        // 1. 使用公共方法获取列信息和总行数
        QList<Vector::ColumnInfo> columns = VectorDataHandler::instance().getVisibleColumns(tableId);
        int totalRowCount = VectorDataHandler::instance().getVectorTableRowCount(tableId);
        int schemaVersion = VectorDataHandler::instance().getSchemaVersion(tableId);
        
        qDebug() << funcName << " - 元数据加载成功, 列数:" << columns.size() << ", 总行数:" << totalRowCount;

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
        if (rowsToLoad <= 0) {
            qDebug() << funcName << " - 当前页没有数据, 直接返回";

            // 恢复更新和信号
            tableWidget->blockSignals(false);
            tableWidget->verticalHeader()->setUpdatesEnabled(true);
            tableWidget->horizontalHeader()->setUpdatesEnabled(true);
            tableWidget->setUpdatesEnabled(true);
            return true;
        }

        // 如果列数为0，也无法继续
        if (columns.isEmpty()) {
            qWarning() << funcName << " - 表 " << tableId << " 没有列配置。";
            tableWidget->setRowCount(0);

            // 恢复更新和信号
            tableWidget->blockSignals(false);
            tableWidget->verticalHeader()->setUpdatesEnabled(true);
            tableWidget->horizontalHeader()->setUpdatesEnabled(true);
            tableWidget->setUpdatesEnabled(true);
            return true; // 返回true，因为元数据已加载，只是表为空
        }

        // 2. 设置表格列（如果需要）
        if (tableWidget->columnCount() != columns.size()) {
            tableWidget->setColumnCount(columns.size());

            // 设置表头
            QStringList headers;
            for (const auto &col : columns) {
                // 根据列类型设置表头
                if (col.type == Vector::ColumnDataType::PIN_STATE_ID && !col.data_properties.isEmpty()) {
                    // 获取管脚属性
                    int channelCount = col.data_properties["channel_count"].toInt(1);
                    int typeId = col.data_properties["type_id"].toInt(1);

                    // 获取类型名称
                    QString typeName = "In"; // 默认为输入类型
                    QSqlQuery typeQuery(DatabaseManager::instance()->database());
                    typeQuery.prepare("SELECT type_name FROM type_options WHERE id = ?");
                    typeQuery.addBindValue(typeId);
                    if (typeQuery.exec() && typeQuery.next()) {
                        typeName = typeQuery.value(0).toString();
                    }

                    // 创建带有管脚信息的表头
                    QString headerText = col.name + "\nx" + QString::number(channelCount) + "\n" + typeName;
                    headers << headerText;
                } else {
                    // 标准列，直接使用列名
                    headers << col.name;
                }
            }

            tableWidget->setHorizontalHeaderLabels(headers);

            // 设置表头居中对齐
            for (int i = 0; i < tableWidget->columnCount(); ++i) {
                QTableWidgetItem *headerItem = tableWidget->horizontalHeaderItem(i);
                if (headerItem) {
                    headerItem->setTextAlignment(Qt::AlignCenter);
                }
            }
        }

        // 3. 获取页面数据
        QList<Vector::RowData> pageRows = VectorDataHandler::instance().getPageData(tableId, pageIndex, pageSize);
        
        if (pageRows.isEmpty() && totalRowCount > 0) {
            qWarning() << funcName << " - 无法获取页面数据";
            tableWidget->setRowCount(0);
            
            // 恢复更新和信号
            tableWidget->blockSignals(false);
            tableWidget->verticalHeader()->setUpdatesEnabled(true);
            tableWidget->horizontalHeader()->setUpdatesEnabled(true);
            tableWidget->setUpdatesEnabled(true);
            return false;
        }
        
        // 4. 填充表格数据
        QSqlDatabase db = DatabaseManager::instance()->database();
        for (int rowIdx = 0; rowIdx < pageRows.size(); ++rowIdx) {
            const Vector::RowData &rowData = pageRows[rowIdx];
            
            for (int colIdx = 0; colIdx < columns.size(); ++colIdx) {
                const Vector::ColumnInfo &col = columns[colIdx];
                QVariant cellValue = rowData.value(colIdx);
                
                // 根据列类型创建适当的单元格部件
                if (col.type == Vector::ColumnDataType::PIN_STATE_ID) {
                    // 管脚状态列使用特殊的编辑器
                    PinValueLineEdit *pinEdit = new PinValueLineEdit(tableWidget);
                    pinEdit->setText(cellValue.toString());
                    tableWidget->setCellWidget(rowIdx, colIdx, pinEdit);
                } else if (col.type == Vector::ColumnDataType::INSTRUCTION_ID) {
                    // 指令ID列需要查询指令名称
                    int instructionId = cellValue.toInt();
                    QString instructionName;
                    
                    QSqlQuery instructionQuery(db);
                    instructionQuery.prepare("SELECT instruction_name FROM instructions WHERE id = ?");
                    instructionQuery.addBindValue(instructionId);
                    if (instructionQuery.exec() && instructionQuery.next()) {
                        instructionName = instructionQuery.value(0).toString();
                    } else {
                        instructionName = QString::number(instructionId);
                    }
                    
                    QTableWidgetItem *item = new QTableWidgetItem(instructionName);
                    item->setData(Qt::UserRole, instructionId); // 保存原始ID
                    tableWidget->setItem(rowIdx, colIdx, item);
                } else if (col.type == Vector::ColumnDataType::TIMESET_ID) {
                    // TimeSet ID列需要查询TimeSet名称
                    int timesetId = cellValue.toInt();
                    QString timesetName;
                    
                    QSqlQuery timesetQuery(db);
                    timesetQuery.prepare("SELECT timeset_name FROM timesets WHERE id = ?");
                    timesetQuery.addBindValue(timesetId);
                    if (timesetQuery.exec() && timesetQuery.next()) {
                        timesetName = timesetQuery.value(0).toString();
                    } else {
                        timesetName = QString::number(timesetId);
                    }
                    
                    QTableWidgetItem *item = new QTableWidgetItem(timesetName);
                    item->setData(Qt::UserRole, timesetId); // 保存原始ID
                    tableWidget->setItem(rowIdx, colIdx, item);
                } else {
                    // 其他类型列使用标准表格项
                    QTableWidgetItem *item = new QTableWidgetItem(cellValue.toString());
                    tableWidget->setItem(rowIdx, colIdx, item);
                }
            }
            
            // 每处理50行，让出一些CPU时间，避免UI完全冻结
            if (rowIdx % 50 == 0 && rowIdx > 0) {
                QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            }
        }
        
        // 恢复滚动条位置
        if (vScrollBar) vScrollBar->setValue(vScrollValue);
        if (hScrollBar) hScrollBar->setValue(hScrollValue);
        
        // 恢复更新和信号
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);
        
        qDebug() << funcName << " - 页面数据加载完成，行数:" << pageRows.size();
        return true;
    } catch (const std::exception &e) {
        qWarning() << funcName << " - 异常:" << e.what();
        
        // 恢复更新和信号
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);
        return false;
    } catch (...) {
        qWarning() << funcName << " - 未知异常";
        
        // 恢复更新和信号
        tableWidget->blockSignals(false);
        tableWidget->verticalHeader()->setUpdatesEnabled(true);
        tableWidget->horizontalHeader()->setUpdatesEnabled(true);
        tableWidget->setUpdatesEnabled(true);
        return false;
    }
}

bool RobustVectorDataHandler::loadVectorTablePageDataForModel(int tableId, VectorTableModel* model, int pageIndex, int pageSize) {
    qWarning() << "RobustVectorDataHandler::loadVectorTablePageDataForModel is not implemented yet.";
    return false;
}

QList<Vector::RowData> RobustVectorDataHandler::getPageData(int tableId, int pageIndex, int pageSize) {
    qWarning() << "RobustVectorDataHandler::getPageData is not implemented yet.";
    return {};
}

QList<Vector::ColumnInfo> RobustVectorDataHandler::getVisibleColumns(int tableId) {
    qWarning() << "RobustVectorDataHandler::getVisibleColumns is not implemented yet.";
    return {};
}

void RobustVectorDataHandler::markRowAsModified(int tableId, int rowIndex) {
    qWarning() << "RobustVectorDataHandler::markRowAsModified is not implemented yet.";
}

void RobustVectorDataHandler::clearModifiedRows(int tableId) {
    qWarning() << "RobustVectorDataHandler::clearModifiedRows is not implemented yet.";
}

bool RobustVectorDataHandler::isRowModified(int tableId, int rowIndex) {
    qWarning() << "RobustVectorDataHandler::isRowModified is not implemented yet.";
    return false;
}

QString RobustVectorDataHandler::resolveBinaryFilePath(int tableId, QString& errorMsg) {
    qWarning() << "RobustVectorDataHandler::resolveBinaryFilePath is not implemented yet.";
    return {};
} 