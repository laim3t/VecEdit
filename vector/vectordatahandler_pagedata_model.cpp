#include "vectordatahandler.h"
#include "../app/vectortablemodel.h"
#include <QDebug>
#include <QSqlQuery>
#include <QSqlError>
#include "../database/databasemanager.h"
#include "../common/utils/pathutils.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

/**
 * @brief 为Model/View架构加载向量表分页数据
 *
 * 该方法专门用于QTableView模式下加载数据，直接将数据加载到VectorTableModel中
 *
 * @param tableId 向量表ID
 * @param model VectorTableModel模型对象指针
 * @param pageIndex 页码（从0开始）
 * @param pageSize 每页显示的行数
 * @return bool 操作是否成功
 */
bool VectorDataHandler::loadVectorTablePageDataForModel(int tableId, VectorTableModel *model, int pageIndex, int pageSize)
{
    if (!model)
    {
        qWarning() << "VectorDataHandler::loadVectorTablePageDataForModel - 模型指针为空";
        return false;
    }

    if (tableId <= 0 || pageIndex < 0 || pageSize <= 0)
    {
        qWarning() << "VectorDataHandler::loadVectorTablePageDataForModel - 无效的参数";
        return false;
    }

    qDebug() << "VectorDataHandler::loadVectorTablePageDataForModel - 开始加载表" << tableId
             << "页码" << pageIndex << "页大小" << pageSize;

    try
    {
        // 通过调用model的loadPage方法来加载数据
        // 这样保持模型接口的独立性，同时避免重复代码
        model->loadPage(tableId, pageIndex);

        qDebug() << "VectorDataHandler::loadVectorTablePageDataForModel - 加载完成";
        return true;
    }
    catch (const std::exception &e)
    {
        qWarning() << "VectorDataHandler::loadVectorTablePageDataForModel - 异常:" << e.what();
        return false;
    }
    catch (...)
    {
        qWarning() << "VectorDataHandler::loadVectorTablePageDataForModel - 未知异常";
        return false;
    }
}