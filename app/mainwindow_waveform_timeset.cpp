// ==========================================================
//  波形图与TimeSet相关实现：mainwindow_waveform_timeset.cpp
// ==========================================================

#include "app/mainwindow.h"
#include "vector/vectordatahandler.h"
#include "common/qcustomplot.h"
#include "database/databasemanager.h"
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

// 获取向量表当前行的TimeSet ID
int MainWindow::getTimeSetIdForRow(int tableId, int rowIndex)
{
    // 定义返回的默认值
    int defaultTimeSetId = -1;
    
    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << "getTimeSetIdForRow - 数据库连接失败";
        return defaultTimeSetId;
    }
    
    // 首先获取当前表的列信息
    QSqlQuery columnsQuery(db);
    columnsQuery.prepare("SELECT id, name, column_order FROM VectorTableColumnConfiguration "
                         "WHERE master_record_id = ? AND column_type = 'TIMESET_ID' LIMIT 1");
    columnsQuery.addBindValue(tableId);
    
    if (!columnsQuery.exec())
    {
        qWarning() << "getTimeSetIdForRow - 查询列信息失败:" << columnsQuery.lastError().text();
        return defaultTimeSetId;
    }
    
    if (!columnsQuery.next())
    {
        qWarning() << "getTimeSetIdForRow - 未找到TimeSet列";
        return defaultTimeSetId;
    }
    
    // 获取TimeSet列的位置和名称
    QString timesetColumnName = columnsQuery.value(1).toString();
    int timesetColumnOrder = columnsQuery.value(2).toInt();
    
    // 如果使用新表格模型
    if (m_isUsingNewTableModel && m_vectorTableModel)
    {
        // 使用模型方式获取数据
        QModelIndex index = m_vectorTableModel->index(rowIndex, timesetColumnOrder);
        if (index.isValid())
        {
            QVariant value = m_vectorTableModel->data(index);
            if (value.isValid() && !value.isNull())
            {
                bool ok = false;
                int timesetId = value.toInt(&ok);
                if (ok && timesetId > 0)
                {
                    qDebug() << "getTimeSetIdForRow - 从模型获取TimeSet ID成功:" << timesetId;
                    return timesetId;
                }
            }
        }
        else
        {
            qWarning() << "getTimeSetIdForRow - 模型索引无效";
        }
        
        // 获取失败时尝试从数据库获取
    }
    
    // 使用传统方式或模型方式失败后的备选方案：从数据库直接查询
    
    // 获取指定行的数据
    bool ok = false;
    QList<Vector::RowData> allRows = VectorDataHandler::instance().getAllVectorRows(tableId, ok);
    
    if (!ok || allRows.isEmpty() || rowIndex >= allRows.size())
    {
        qWarning() << "getTimeSetIdForRow - 获取行数据失败";
        return defaultTimeSetId;
    }
    
    // 获取指定行的TimeSet ID
    if (timesetColumnOrder < allRows[rowIndex].size())
    {
        QVariant value = allRows[rowIndex][timesetColumnOrder];
        if (value.isValid() && !value.isNull())
        {
            bool ok = false;
            int timesetId = value.toInt(&ok);
            if (ok && timesetId > 0)
            {
                return timesetId;
            }
        }
    }
    
    // 如果上述方法都失败，尝试从数据库直接查询该行数据
    QSqlQuery rowQuery(db);
    rowQuery.prepare("SELECT data_json FROM vector_data WHERE table_id = ? AND row_index = ?");
    rowQuery.addBindValue(tableId);
    rowQuery.addBindValue(rowIndex);
    
    if (rowQuery.exec() && rowQuery.next())
    {
        QString dataJson = rowQuery.value(0).toString();
        QJsonDocument doc = QJsonDocument::fromJson(dataJson.toUtf8());
        
        if (doc.isArray())
        {
            QJsonArray array = doc.array();
            if (timesetColumnOrder < array.size())
            {
                int timesetId = array.at(timesetColumnOrder).toInt();
                if (timesetId > 0)
                {
                    return timesetId;
                }
            }
        }
    }
    
    return defaultTimeSetId;
}

// 获取TimeSet的T1R值和周期
bool MainWindow::getTimeSetT1RAndPeriod(int timeSetId, int pinId, double &t1r, double &period)
{
    // 初始化返回值
    t1r = 250.0;    // 默认T1R值
    period = 1000.0; // 默认周期1000ns
    
    // 检查参数有效性
    if (timeSetId <= 0)
    {
        qWarning() << "getTimeSetT1RAndPeriod - 无效的TimeSet ID:" << timeSetId;
        return false;
    }
    
    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << "getTimeSetT1RAndPeriod - 数据库连接失败";
        return false;
    }
    
    // 查询周期值
    QSqlQuery periodQuery(db);
    periodQuery.prepare("SELECT period FROM timeset_list WHERE id = ?");
    periodQuery.addBindValue(timeSetId);
    
    if (!periodQuery.exec() || !periodQuery.next())
    {
        qWarning() << "getTimeSetT1RAndPeriod - 获取TimeSet周期失败: " 
                   << (periodQuery.lastError().text().isEmpty() ? "未找到记录" : periodQuery.lastError().text());
        return false;
    }
    
    period = periodQuery.value(0).toDouble();
    qDebug() << "getTimeSetT1RAndPeriod - 读取到的周期值:" << period << "ns (TimeSet ID:" << timeSetId << ")";
    
    // 查询T1R值 - 如果指定了管脚
    if (pinId > 0)
    {
        QSqlQuery t1rQuery(db);
        t1rQuery.prepare("SELECT T1R FROM timeset_settings WHERE timeset_id = ? AND pin_id = ?");
        t1rQuery.addBindValue(timeSetId);
        t1rQuery.addBindValue(pinId);
        
        if (t1rQuery.exec() && t1rQuery.next())
        {
            t1r = t1rQuery.value(0).toDouble();
            qDebug() << "getTimeSetT1RAndPeriod - 读取到的T1R值:" << t1r << "ns (TimeSet ID:" << timeSetId << ", Pin ID:" << pinId << ")";
        }
        else
        {
            qWarning() << "getTimeSetT1RAndPeriod - 获取管脚特定的T1R值失败, 使用默认值。错误: " 
                       << (t1rQuery.lastError().text().isEmpty() ? "未找到记录" : t1rQuery.lastError().text())
                       << " (TimeSet ID:" << timeSetId << ", Pin ID:" << pinId << ")";
        }
    }
    else
    {
        // 如果没有指定管脚，尝试获取任何该TimeSet的T1R值
        QSqlQuery anyT1rQuery(db);
        anyT1rQuery.prepare("SELECT T1R FROM timeset_settings WHERE timeset_id = ? LIMIT 1");
        anyT1rQuery.addBindValue(timeSetId);
        
        if (anyT1rQuery.exec() && anyT1rQuery.next())
        {
            t1r = anyT1rQuery.value(0).toDouble();
            qDebug() << "getTimeSetT1RAndPeriod - 读取到的任意管脚T1R值:" << t1r << "ns (TimeSet ID:" << timeSetId << ")";
        }
        else
        {
            qWarning() << "getTimeSetT1RAndPeriod - 获取任何T1R值失败, 使用默认值";
        }
    }
    
    qDebug() << "getTimeSetT1RAndPeriod - 最终使用的T1R=" << t1r << "ns, 周期=" << period << "ns, 比例=" << (t1r/period);
    return true;
}

// 获取管脚ID
int MainWindow::getPinIdByName(const QString &pinName)
{
    // 初始化返回值
    int pinId = -1;
    
    if (pinName.isEmpty())
    {
        qWarning() << "getPinIdByName - 管脚名称为空";
        return pinId;
    }
    
    qDebug() << "getPinIdByName - 尝试获取管脚名称为" << pinName << "的ID";
    
    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << "getPinIdByName - 数据库连接失败";
        return pinId;
    }
    
    // 查询管脚ID
    QSqlQuery pinQuery(db);
    pinQuery.prepare("SELECT id FROM pin_list WHERE pin_name = ?");
    pinQuery.addBindValue(pinName);
    
    if (pinQuery.exec()) {
        if (pinQuery.next()) {
            pinId = pinQuery.value(0).toInt();
            qDebug() << "getPinIdByName - 成功获取管脚" << pinName << "的ID:" << pinId;
        } else {
            qWarning() << "getPinIdByName - 未找到名为" << pinName << "的管脚记录";
            
            // 尝试列出所有可用的管脚
            QSqlQuery listQuery(db);
            if (listQuery.exec("SELECT id, pin_name FROM pin_list LIMIT 10")) {
                qDebug() << "getPinIdByName - 数据库中的管脚列表:";
                while (listQuery.next()) {
                    qDebug() << "  ID:" << listQuery.value(0).toInt() << ", 名称:" << listQuery.value(1).toString();
                }
            }
        }
    } else {
        qWarning() << "getPinIdByName - 查询失败: " << pinQuery.lastError().text();
    }
    
    return pinId;
} 