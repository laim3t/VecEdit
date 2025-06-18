#include "vectormodeltest.h"
#include "../database/databasemanager.h"
#include "../common/logger.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDataStream>
#include <QFile>
#include <QFileInfo>

namespace Vector
{

    VectorModelTest::VectorModelTest(QObject *parent)
        : QObject(parent)
    {
    }

    bool VectorModelTest::initializeTestData(const QString &dbPath)
    {
        // 打开测试数据库
        if (!DatabaseManager::instance()->openExistingDatabase(dbPath))
        {
            qWarning() << "无法打开数据库:" << dbPath << " - " << DatabaseManager::instance()->lastError();
            return false;
        }

        QSqlDatabase db = DatabaseManager::instance()->database();
        QSqlQuery query(db);

        // 检查vector_tables表是否存在测试数据
        query.prepare("SELECT COUNT(*) FROM vector_tables");
        if (!query.exec() || !query.next() || query.value(0).toInt() == 0)
        {
            qWarning() << "数据库中没有向量表数据，测试无法继续";
            return false;
        }

        return true;
    }

    QList<int> VectorModelTest::getAvailableTables()
    {
        QList<int> tableIds;

        QSqlDatabase db = DatabaseManager::instance()->database();
        QSqlQuery query(db);

        query.prepare("SELECT id FROM vector_tables ORDER BY id");
        if (!query.exec())
        {
            qWarning() << "获取表列表失败:" << query.lastError().text();
            return tableIds;
        }

        while (query.next())
        {
            tableIds.append(query.value(0).toInt());
        }

        return tableIds;
    }

    QString VectorModelTest::getTableName(int tableId)
    {
        QSqlDatabase db = DatabaseManager::instance()->database();
        QSqlQuery query(db);

        // 获取表结构
        QStringList columnNames;
        QSqlQuery columnsQuery(db);
        if (columnsQuery.exec("PRAGMA table_info(vector_tables)"))
        {
            while (columnsQuery.next())
            {
                columnNames << columnsQuery.value(1).toString();
            }
        }

        // 根据表结构选择正确的列名
        QString nameColumn = "tablename"; // 默认尝试
        if (columnNames.contains("name"))
        {
            nameColumn = "name";
        }
        else if (columnNames.contains("table_name"))
        {
            nameColumn = "table_name";
        }

        // 构建查询
        QString sql = QString("SELECT %1 FROM vector_tables WHERE id = ?").arg(nameColumn);
        query.prepare(sql);
        query.addBindValue(tableId);

        if (query.exec() && query.next())
        {
            QString name = query.value(0).toString();
            if (!name.isEmpty())
            {
                return name;
            }
        }

        return QString("表 %1").arg(tableId);
    }

} // namespace Vector