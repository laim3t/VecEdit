#include "databasemanager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>        // Needed for unique connection name, already used in openExistingDatabase
#include <QCoreApplication> // For applicationDirPath as a fallback for script path

// 静态实例初始化为nullptr
DatabaseManager *DatabaseManager::m_instance = nullptr;

// 定义静态常量
const QString DatabaseManager::VERSION_TABLE = "db_version";

DatabaseManager *DatabaseManager::instance()
{
    if (!m_instance)
    {
        m_instance = new DatabaseManager();
    }
    return m_instance;
}

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent), m_currentDbFileVersion(0)
{
    // 确保我们可以在应用程序中使用SQLite
    if (!QSqlDatabase::isDriverAvailable("QSQLITE"))
    {
        m_lastError = "SQLite驱动不可用";
        qCritical() << m_lastError;
    }
}

DatabaseManager::~DatabaseManager()
{
    closeDatabase();
}

bool DatabaseManager::initializeNewDatabase(const QString &dbFilePath, const QString &schemaFilePath)
{
    // 首先检查文件是否已存在
    QFile dbFile(dbFilePath);
    if (dbFile.exists())
    {
        m_lastError = QString("数据库文件已存在: %1").arg(dbFilePath);
        qWarning() << m_lastError;
        return false;
    }

    // 读取schema文件
    QString schemaContent = readSqlScriptFromFile(schemaFilePath);
    if (schemaContent.isEmpty())
    {
        return false; // 错误信息已在readSqlScriptFromFile中设置
    }

    // 确保数据库文件所在的目录存在
    QDir dir = QFileInfo(dbFilePath).dir();
    if (!dir.exists())
    {
        if (!dir.mkpath("."))
        {
            m_lastError = QString("无法创建目录: %1").arg(dir.path());
            qCritical() << m_lastError;
            return false;
        }
    }

    // 创建数据库连接
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(dbFilePath);

    // 打开数据库
    if (!m_db.open())
    {
        m_lastError = QString("无法打开数据库: %1").arg(m_db.lastError().text());
        qCritical() << m_lastError;
        return false;
    }

    // 执行schema脚本
    if (!executeSqlScript(schemaContent))
    {
        // 出错了，关闭数据库，删除文件
        m_db.close();
        dbFile.remove();
        return false;
    }

    // 创建版本表并设置应用程序支持的最新版本
    if (!createVersionTableIfNotExists())
    {
        m_db.close();
        dbFile.remove();
        return false;
    }

    // 设置数据库文件版本为应用程序支持的最新版本
    if (!setCurrentDbFileVersionInTable(LATEST_DB_SCHEMA_VERSION))
    {
        m_lastError = QString("无法设置初始数据库版本为 %1: %2").arg(LATEST_DB_SCHEMA_VERSION).arg(m_db.lastError().text());
        qCritical() << m_lastError;
        m_db.close();
        dbFile.remove();
        return false;
    }

    // 初始化默认固定数据
    if (!initializeDefaultData())
    {
        m_lastError = QString("初始化默认数据失败");
        qCritical() << m_lastError;
        m_db.close();
        dbFile.remove();
        return false;
    }

    m_currentDbFileVersion = LATEST_DB_SCHEMA_VERSION;
    qInfo() << "数据库已成功初始化: " << dbFilePath << "， 版本: " << m_currentDbFileVersion;
    return true;
}

bool DatabaseManager::openExistingDatabase(const QString &dbFilePath)
{
    // 检查文件是否存在
    QFile dbFile(dbFilePath);
    if (!dbFile.exists())
    {
        m_lastError = QString("数据库文件不存在: %1").arg(dbFilePath);
        qWarning() << m_lastError;
        return false;
    }

    // 如果已有连接，先关闭
    if (m_db.isOpen())
    {
        closeDatabase();
    }

    // 创建数据库连接
    // 使用唯一的连接名，以防多个DatabaseManager实例（尽管是单例，但以防万一）或测试中出现问题
    QString connectionName = QString("db_conn_%1").arg(QDateTime::currentMSecsSinceEpoch());
    m_db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    m_db.setDatabaseName(dbFilePath);

    // 打开数据库
    if (!m_db.open())
    {
        m_lastError = QString("无法打开数据库: %1 (%2)").arg(m_db.lastError().text()).arg(dbFilePath);
        qCritical() << m_lastError;
        QSqlDatabase::removeDatabase(connectionName); // 清理连接
        return false;
    }

    // 保存数据库文件路径，用于后续的二进制文件路径解析
    m_dbFilePath = dbFilePath;

    // 获取当前数据库文件中的版本号
    m_currentDbFileVersion = ensureVersionTableAndGetCurrentVersion();

    if (m_currentDbFileVersion < 0)
    { // Error occurred while getting version
        // m_lastError is already set by ensureVersionTableAndGetCurrentVersion
        m_db.close();
        QSqlDatabase::removeDatabase(connectionName);
        return false;
    }

    qInfo() << "数据库已成功打开: " << dbFilePath << "，文件报告版本: " << m_currentDbFileVersion;

    // 检查是否需要升级
    if (m_currentDbFileVersion < LATEST_DB_SCHEMA_VERSION)
    {
        qInfo() << "数据库需要从版本 " << m_currentDbFileVersion << " 升级到 " << LATEST_DB_SCHEMA_VERSION;

        // 开始事务执行所有升级步骤
        if (!m_db.transaction())
        {
            m_lastError = "无法开始数据库升级事务: " + m_db.lastError().text();
            qCritical() << m_lastError;
            m_db.close();
            QSqlDatabase::removeDatabase(connectionName);
            return false;
        }

        bool upgradeSuccess = true;

        // --- 升级到版本 2 ---
        if (m_currentDbFileVersion < 2)
        {
            qInfo() << "正在升级到版本 2...";
            if (!performSchemaUpgradeToV2())
            {                           // This function will also update m_currentDbFileVersion internally on success
                upgradeSuccess = false; // m_lastError will be set by performSchemaUpgradeToV2
            }
            else
            {
                qInfo() << "成功升级到版本 2.";
            }
        }

        // --- 升级到版本 3 ---
        if (upgradeSuccess && m_currentDbFileVersion < 3)
        {
            qInfo() << "正在升级到版本 3...";
            if (!performSchemaUpgradeToV3())
            {
                upgradeSuccess = false;
            }
            else
            {
                qInfo() << "成功升级到版本 3.";
            }
        }

        if (upgradeSuccess)
        {
            if (!m_db.commit())
            {
                m_lastError = "提交数据库升级事务失败: " + m_db.lastError().text();
                qCritical() << m_lastError;
                // 尝试回滚，尽管可能为时已晚
                m_db.rollback();
                m_db.close();
                QSqlDatabase::removeDatabase(connectionName);
                return false;
            }
            qInfo() << "数据库升级成功完成，当前版本: " << m_currentDbFileVersion;
        }
        else
        {
            qCritical() << "数据库升级失败: " << m_lastError << ". 正在回滚...";
            m_db.rollback();
            m_db.close();
            QSqlDatabase::removeDatabase(connectionName);
            return false;
        }
    }
    else if (m_currentDbFileVersion > LATEST_DB_SCHEMA_VERSION)
    {
        qWarning() << "数据库版本 " << m_currentDbFileVersion
                   << " 比应用程序支持的最新版本 " << LATEST_DB_SCHEMA_VERSION << " 更高."
                   << "请更新应用程序.";
        // 根据策略决定是否允许打开，或者仅以只读模式打开，或者报错并关闭
        // 为安全起见，这里可以选择关闭
        // m_lastError = "数据库版本过新";
        // m_db.close();
        // QSqlDatabase::removeDatabase(connectionName);
        // return false;
    }

    qInfo() << "数据库准备就绪，最终版本: " << m_currentDbFileVersion;
    return true;
}

void DatabaseManager::closeDatabase()
{
    QString connectionName;
    if (m_db.isOpen())
    {
        connectionName = m_db.connectionName();
        m_db.close();
    }

    // 如果有连接名，移除连接
    if (!connectionName.isEmpty())
    {
        QSqlDatabase::removeDatabase(connectionName);
    }

    qInfo() << "数据库连接已关闭";
}

bool DatabaseManager::isDatabaseConnected() const
{
    return m_db.isOpen();
}

QString DatabaseManager::lastError() const
{
    return m_lastError;
}

int DatabaseManager::getCurrentDatabaseVersion() const
{
    return m_currentDbFileVersion;
}

// 确保版本表存在，并获取当前数据库文件中的版本号
// 返回: 数据库版本号，如果出错或表不存在且无法创建则返回0或-1
int DatabaseManager::ensureVersionTableAndGetCurrentVersion()
{
    if (!isDatabaseConnected())
    {
        m_lastError = "数据库未连接，无法获取版本信息";
        qWarning() << m_lastError;
        return -1; // Indicate error
    }

    QSqlQuery query(m_db);
    // 检查版本表是否存在
    if (!query.exec(QString("SELECT name FROM sqlite_master WHERE type='table' AND name='%1'").arg(VERSION_TABLE)))
    {
        m_lastError = QString("无法检查版本表: %1").arg(query.lastError().text());
        qCritical() << m_lastError;
        return -1; // Indicate error
    }

    if (!query.next())
    {
        // 版本表不存在，尝试创建它
        qInfo() << "版本表 '" << VERSION_TABLE << "' 不存在，正在尝试创建...";
        if (!createVersionTableIfNotExists())
        { // createVersionTableIfNotExists should handle its own errors
            // m_lastError is set by createVersionTableIfNotExists
            return -1; // Indicate error
        }
        // 版本表刚被创建，里面还没有版本记录，我们将其视为版本0，需要升级。
        // 或者，如果这是第一次，且schema.sql已经包含了V2内容，那创建新库时会直接写入V2。
        // 对于一个"没有版本信息"的旧库，我们通常认为它是需要升级到最新版的基础版本（可能是0或1）。
        // 考虑到 initializeNewDatabase 会直接写入 LATEST_DB_SCHEMA_VERSION，
        // 这里如果表是新创建的（意味着是一个非常旧的库或损坏的库），返回0是安全的，促使其升级。
        qInfo() << "版本表已创建，但无版本记录。将当前版本视为 0，需要升级。";
        return 0;
    }

    // 版本表存在，获取 MAX 版本号
    if (!query.exec(QString("SELECT MAX(version) FROM %1").arg(VERSION_TABLE)))
    {
        m_lastError = QString("无法从版本表 '%1' 获取数据库版本: %2").arg(VERSION_TABLE).arg(query.lastError().text());
        qCritical() << m_lastError;
        return -1; // Indicate error
    }

    if (query.next())
    {
        bool ok;
        int version = query.value(0).toInt(&ok);
        if (ok)
        {
            return version;
        }
        m_lastError = QString("版本表中版本号无效: %1").arg(query.value(0).toString());
        qCritical() << m_lastError;
        return 0; // Treat as unversioned or base version if value is invalid
    }
    // 版本表存在但为空
    m_lastError = QString("版本表 '%1' 存在但为空.").arg(VERSION_TABLE);
    qWarning() << m_lastError;
    return 0; // Treat as unversioned or base version, needs upgrade
}

// 内部函数，用于将指定版本号写入数据库的 db_version 表
bool DatabaseManager::setCurrentDbFileVersionInTable(int version)
{
    if (!isDatabaseConnected())
    {
        m_lastError = "数据库未连接";
        qWarning() << m_lastError;
        return false;
    }
    // 确保版本表存在
    if (!createVersionTableIfNotExists())
    { // This also sets m_lastError on failure
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(QString("INSERT INTO %1 (version) VALUES (?)").arg(VERSION_TABLE));
    query.addBindValue(version);

    if (!query.exec())
    {
        m_lastError = QString("无法将版本 %1 写入版本表 '%2': %3").arg(version).arg(VERSION_TABLE).arg(query.lastError().text());
        qCritical() << m_lastError;
        return false;
    }
    qInfo() << "数据库版本已在表中更新为:" << version;
    this->m_currentDbFileVersion = version; // Keep member variable in sync
    return true;
}

bool DatabaseManager::updateDatabaseSchema(int targetVersion, const QString &updateScriptPath)
{
    if (!isDatabaseConnected())
    {
        m_lastError = "数据库未连接";
        qWarning() << m_lastError;
        return false;
    }

    if (targetVersion <= m_currentDbFileVersion)
    {
        m_lastError = QString("目标版本 %1 不大于当前版本 %2").arg(targetVersion).arg(m_currentDbFileVersion);
        qWarning() << m_lastError;
        return false;
    }

    // 从文件中读取更新脚本
    QString updateScript = readSqlScriptFromFile(updateScriptPath);
    if (updateScript.isEmpty())
    {
        return false; // 错误信息已在readSqlScriptFromFile中设置
    }

    // 开始事务，确保要么全部成功，要么全部失败
    m_db.transaction();

    // 执行更新脚本
    if (!executeSqlScript(updateScript))
    {
        m_db.rollback();
        return false;
    }

    // 更新版本号
    QSqlQuery query(m_db);
    if (!query.exec(QString("INSERT INTO %1 (version) VALUES (%2)").arg(VERSION_TABLE).arg(targetVersion)))
    {
        m_lastError = QString("无法更新数据库版本: %1").arg(query.lastError().text());
        qCritical() << m_lastError;
        m_db.rollback();
        return false;
    }

    // 提交事务
    if (!m_db.commit())
    {
        m_lastError = QString("无法提交事务: %1").arg(m_db.lastError().text());
        qCritical() << m_lastError;
        m_db.rollback();
        return false;
    }

    m_currentDbFileVersion = targetVersion;
    qInfo() << "数据库已成功升级到版本: " << targetVersion;
    return true;
}

bool DatabaseManager::registerVersionTable()
{
    return createVersionTableIfNotExists();
}

QSqlDatabase DatabaseManager::database() const
{
    return m_db;
}

bool DatabaseManager::executeQuery(const QString &queryStr)
{
    if (!isDatabaseConnected())
    {
        m_lastError = "数据库未连接";
        qWarning() << m_lastError;
        return false;
    }

    QSqlQuery query(m_db);
    if (!query.exec(queryStr))
    {
        m_lastError = QString("执行查询失败: %1\n查询: %2").arg(query.lastError().text()).arg(queryStr);
        qCritical() << m_lastError;
        return false;
    }

    return true;
}

bool DatabaseManager::executeSqlScript(const QString &scriptContent)
{
    if (!isDatabaseConnected())
    {
        m_lastError = "数据库未连接";
        qWarning() << m_lastError;
        return false;
    }

    // 更复杂的SQL解析，处理包含BEGIN...END块的触发器
    QString normalizedScript = scriptContent;
    normalizedScript.replace("\r\n", "\n"); // 统一换行符

    QStringList statements;
    QString currentStatement;
    bool insideTrigger = false;

    QStringList lines = normalizedScript.split("\n");
    for (const QString &line : lines)
    {
        QString trimmedLine = line.trimmed();

        // 忽略注释行和空行
        if (trimmedLine.startsWith("--") || trimmedLine.isEmpty())
        {
            continue;
        }

        // 检测触发器开始
        if (trimmedLine.contains("CREATE TRIGGER", Qt::CaseInsensitive) && !insideTrigger)
        {
            insideTrigger = true;
        }

        currentStatement += line + "\n";

        // 如果不在触发器内部且行以分号结尾，或者在触发器内部且发现END;，提取语句
        if ((!insideTrigger && trimmedLine.endsWith(";")) ||
            (insideTrigger && trimmedLine.contains("END;", Qt::CaseInsensitive)))
        {

            if (insideTrigger && trimmedLine.contains("END;", Qt::CaseInsensitive))
            {
                insideTrigger = false;
            }

            if (!currentStatement.trimmed().isEmpty())
            {
                statements.append(currentStatement.trimmed());
                currentStatement.clear();
            }
        }
    }

    // 处理最后一个未结束的语句（如果有）
    if (!currentStatement.trimmed().isEmpty())
    {
        statements.append(currentStatement.trimmed());
    }

    // 执行所有提取的语句
    for (const QString &statement : statements)
    {
        qDebug() << "执行SQL语句：" << statement.left(100) << (statement.length() > 100 ? "..." : "");
        if (!executeQuery(statement))
        {
            return false; // 错误信息已在executeQuery中设置
        }
    }

    return true;
}

bool DatabaseManager::executeSqlScriptFromFile(const QString &scriptFilePath)
{
    QString scriptContent = readSqlScriptFromFile(scriptFilePath);
    if (scriptContent.isEmpty())
    {
        return false; // 错误信息已在readSqlScriptFromFile中设置
    }

    return executeSqlScript(scriptContent);
}

QString DatabaseManager::readSqlScriptFromFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        m_lastError = QString("无法打开SQL脚本文件: %1").arg(file.errorString());
        qCritical() << m_lastError;
        return QString();
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    if (content.isEmpty())
    {
        m_lastError = QString("SQL脚本文件为空: %1").arg(filePath);
        qWarning() << m_lastError;
    }

    return content;
}

#include "databasemanager_1.cpp"

// ============== 管脚选择对话框新功能实现 ==============

bool DatabaseManager::setPinColumnVisibility(qint64 tableId, int pinId, bool isVisible)
{
    // 首先，我们需要根据 pinId 获取 pinName，因为列配置表是使用名称来识别的
    QSqlQuery nameQuery;
    nameQuery.prepare("SELECT pin_name FROM pin_list WHERE id = ?");
    nameQuery.bindValue(0, pinId);
    if (!nameQuery.exec() || !nameQuery.next()) {
        qWarning() << "setPinColumnVisibility: Failed to find pin name for pinId" << pinId << nameQuery.lastError();
        return false;
    }
    QString pinName = nameQuery.value(0).toString();

    // 然后，更新列配置表
    QSqlQuery updateQuery;
    updateQuery.prepare("UPDATE VectorTableColumnConfiguration SET is_visible = ? "
                        "WHERE master_record_id = ? AND column_name = ?");
    updateQuery.bindValue(0, isVisible ? 1 : 0);
    updateQuery.bindValue(1, tableId);
    updateQuery.bindValue(2, pinName);

    if (!updateQuery.exec()) {
        qWarning() << "setPinColumnVisibility: Failed to update visibility for pin" << pinName << updateQuery.lastError();
        return false;
    }

    return true;
}

QList<int> DatabaseManager::getAvailablePlaceholderPinIds(qint64 tableId)
{
    QList<int> placeholderIds;
    QSqlQuery query;
    // 查询与该向量表关联的、且被标记为占位符的管脚ID
    query.prepare("SELECT p.id FROM pin_list p "
                  "JOIN vector_table_pins vtp ON p.id = vtp.pin_id "
                  "WHERE vtp.table_id = ? AND p.is_placeholder = 1");
    query.bindValue(0, tableId);

    if (!query.exec()) {
        qWarning() << "getAvailablePlaceholderPinIds: Failed to query placeholder pins" << query.lastError();
        return placeholderIds; // 返回空列表
    }

    while (query.next()) {
        placeholderIds.append(query.value(0).toInt());
    }

    return placeholderIds;
}

bool DatabaseManager::replacePinInSlot(qint64 tableId, int oldPinId, int newPinId)
{
    QString oldPinName, newPinName;

    // 使用独立的查询对象确保隔离，并使用 bindValue
    {
        QSqlQuery nameQuery;
        nameQuery.prepare("SELECT pin_name FROM pin_list WHERE id = ?");
        nameQuery.bindValue(0, oldPinId);
        if (!nameQuery.exec() || !nameQuery.next()) {
            qWarning() << "replacePinInSlot: Failed to find old pin name for pinId" << oldPinId << nameQuery.lastError();
            return false;
        }
        oldPinName = nameQuery.value(0).toString();
    }

    {
        QSqlQuery nameQuery;
        nameQuery.prepare("SELECT pin_name FROM pin_list WHERE id = ?");
        nameQuery.bindValue(0, newPinId);
        if (!nameQuery.exec() || !nameQuery.next()) {
            qWarning() << "replacePinInSlot: Failed to find new pin name for pinId" << newPinId << nameQuery.lastError();
            return false;
        }
        newPinName = nameQuery.value(0).toString();
    }


    // 2. 更新 VectorTableColumnConfiguration 表
    QSqlQuery updateConfigQuery;
    updateConfigQuery.prepare("UPDATE VectorTableColumnConfiguration SET column_name = ?, is_visible = 1 "
                              "WHERE master_record_id = ? AND column_name = ?");
    updateConfigQuery.bindValue(0, newPinName);
    updateConfigQuery.bindValue(1, tableId);
    updateConfigQuery.bindValue(2, oldPinName);

    if (!updateConfigQuery.exec()) {
        qWarning() << "replacePinInSlot: Failed to update ColumnConfiguration" << updateConfigQuery.lastError();
        return false;
    }

    // 3. 更新 vector_table_pins 表
    QSqlQuery updatePinsQuery;
    updatePinsQuery.prepare("UPDATE vector_table_pins SET pin_id = ? "
                            "WHERE table_id = ? AND pin_id = ?");
    updatePinsQuery.bindValue(0, newPinId);
    updatePinsQuery.bindValue(1, tableId);
    updatePinsQuery.bindValue(2, oldPinId);

    if (!updatePinsQuery.exec()) {
        qWarning() << "replacePinInSlot: Failed to update vector_table_pins" << updatePinsQuery.lastError();
        return false;
    }
    
    if (updatePinsQuery.numRowsAffected() == 0) {
        qWarning() << "replacePinInSlot: Update on vector_table_pins affected 0 rows. Inconsistency may exist.";
    }

    return true;
}

QList<int> DatabaseManager::getAllAssociatedPinIds(qint64 tableId)
{
    QList<int> pinIds;
    QSqlQuery query;
    query.prepare("SELECT pin_id FROM vector_table_pins WHERE table_id = ?");
    query.bindValue(0, tableId);

    if (!query.exec()) {
        qWarning() << "getAllAssociatedPinIds: Failed to query associated pin IDs for tableId" << tableId << query.lastError();
        return pinIds;
    }

    while (query.next()) {
        pinIds.append(query.value(0).toInt());
    }

    return pinIds;
}