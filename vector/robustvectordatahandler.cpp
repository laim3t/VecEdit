#include "robustvectordatahandler.h"
#include <QDebug>

RobustVectorDataHandler &RobustVectorDataHandler::instance()
{
    static RobustVectorDataHandler inst;
    return inst;
}

RobustVectorDataHandler::RobustVectorDataHandler(QObject *parent) : QObject(parent) {}

bool RobustVectorDataHandler::createNewProject(const QString &filePath, const QList<int> &pinList)
{
    qWarning() << "RobustVectorDataHandler::createNewProject is not implemented yet.";
    return false;
}

bool RobustVectorDataHandler::openProject(const QString &filePath)
{
    qWarning() << "RobustVectorDataHandler::openProject is not implemented yet.";
    return false;
}

void RobustVectorDataHandler::closeProject()
{
    qWarning() << "RobustVectorDataHandler::closeProject is not implemented yet.";
}

bool RobustVectorDataHandler::saveProject()
{
    qWarning() << "RobustVectorDataHandler::saveProject is not implemented yet.";
    return false;
}

bool RobustVectorDataHandler::saveProjectAs(const QString &filePath)
{
    qWarning() << "RobustVectorDataHandler::saveProjectAs is not implemented yet.";
    return false;
}

int RobustVectorDataHandler::getRowCount() const
{
    qWarning() << "RobustVectorDataHandler::getRowCount is not implemented yet.";
    return 0;
}

int RobustVectorDataHandler::getColumnCount() const
{
    qWarning() << "RobustVectorDataHandler::getColumnCount is not implemented yet.";
    return 0;
}

QString RobustVectorDataHandler::getData(int row, int column) const
{
    qWarning() << "RobustVectorDataHandler::getData is not implemented yet.";
    return QString();
}

bool RobustVectorDataHandler::setData(int row, int column, const QString &value)
{
    qWarning() << "RobustVectorDataHandler::setData is not implemented yet.";
    return false;
}

bool RobustVectorDataHandler::insertRow(int row)
{
    qWarning() << "RobustVectorDataHandler::insertRow is not implemented yet.";
    return false;
}

bool RobustVectorDataHandler::removeRow(int row)
{
    qWarning() << "RobustVectorDataHandler::removeRow is not implemented yet.";
    return false;
}

bool RobustVectorDataHandler::insertRows(int row, int count)
{
    qWarning() << "RobustVectorDataHandler::insertRows is not implemented yet.";
    return false;
}

bool RobustVectorDataHandler::removeRows(int row, int count)
{
    qWarning() << "RobustVectorDataHandler::removeRows is not implemented yet.";
    return false;
}

bool RobustVectorDataHandler::isProjectOpen() const
{
    qWarning() << "RobustVectorDataHandler::isProjectOpen is not implemented yet.";
    return false;
}

QString RobustVectorDataHandler::getCurrentProjectPath() const
{
    qWarning() << "RobustVectorDataHandler::getCurrentProjectPath is not implemented yet.";
    return QString();
}

bool RobustVectorDataHandler::deleteVectorTable(int tableId, QString &errorMessage)
{
    qWarning() << "RobustVectorDataHandler::deleteVectorTable is not implemented yet.";
    return false;
}

int RobustVectorDataHandler::getVectorTableRowCount(int tableId)
{
    qWarning() << "RobustVectorDataHandler::getVectorTableRowCount is not implemented yet.";
    return 0;
}

bool RobustVectorDataHandler::loadVectorTablePageData(int tableId, QTableWidget *tableWidget, int page, int pageSize)
{
    qWarning() << "RobustVectorDataHandler::loadVectorTablePageData is not implemented yet.";
    return false;
}

void RobustVectorDataHandler::markRowAsModified(int tableId, int rowIndex)
{
    qWarning() << "RobustVectorDataHandler::markRowAsModified is not implemented yet.";
}

QList<Vector::RowData> RobustVectorDataHandler::getAllVectorRows(int tableId, bool &ok)
{
    qWarning() << "RobustVectorDataHandler::getAllVectorRows is not implemented yet.";
    ok = false;
    return QList<Vector::RowData>();
}

void RobustVectorDataHandler::clearTableDataCache(int tableId)
{
    qWarning() << "RobustVectorDataHandler::clearTableDataCache is not implemented yet.";
}

bool RobustVectorDataHandler::loadVectorTablePageDataForModel(int tableId, QAbstractItemModel *model, int page, int pageSize)
{
    qWarning() << "RobustVectorDataHandler::loadVectorTablePageDataForModel is not implemented yet.";
    return false;
}

QList<Vector::ColumnInfo> RobustVectorDataHandler::getAllColumnInfo(int tableId)
{
    qWarning() << "RobustVectorDataHandler::getAllColumnInfo is not implemented yet.";
    return QList<Vector::ColumnInfo>();
}

bool RobustVectorDataHandler::loadVectorTableData(int tableId, QTableWidget *tableWidget)
{
    qWarning() << "RobustVectorDataHandler::loadVectorTableData is not implemented yet.";
    return false;
}

bool RobustVectorDataHandler::saveVectorTableDataPaged(int tableId, QTableWidget *tableWidget, int currentPage, int pageSize, int totalRows, QString &errorMessage)
{
    qWarning() << "RobustVectorDataHandler::saveVectorTableDataPaged is not implemented yet.";
    errorMessage = "RobustVectorDataHandler::saveVectorTableDataPaged is not implemented yet.";
    return false;
}

void RobustVectorDataHandler::clearModifiedRows(int tableId)
{
    qWarning() << "RobustVectorDataHandler::clearModifiedRows is not implemented yet.";
}