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
        m_lastError = "SQLite驱动不可用 - 这将导致应用程序无法使用数据库功能";
        qCritical() << m_lastError;
        
        // 尝试输出所有可用的驱动程序，以帮助诊断
        QStringList availableDrivers = QSqlDatabase::drivers();
        qInfo() << "可用的数据库驱动程序:" << availableDrivers.join(", ");
        
        if (availableDrivers.isEmpty()) {
            qCritical() << "没有任何SQL驱动程序可用! 请确保Qt SQL模块已正确构建和链接。";
        }
        
        // 尝试强制加载SQLite驱动
        qInfo() << "尝试显式加载QSQLITE驱动...";
    } else {
        qInfo() << "SQLite驱动已可用。";
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
    // 检查数据库是否已连接
    if (!m_db.isOpen()) {
        qWarning() << "DatabaseManager::database - 数据库未连接";
        return QSqlDatabase(); // 返回无效数据库对象
    }
    
    // 输出连接信息，帮助跟踪
    qDebug() << "DatabaseManager::database - 返回数据库连接，名称:" << m_db.connectionName() 
             << ", 路径:" << m_db.databaseName()
             << ", 驱动:" << m_db.driverName()
             << ", 状态:" << (m_db.isOpen() ? "已打开" : "未打开");
             
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

bool DatabaseManager::createVersionTableIfNotExists()
{
    if (!isDatabaseConnected())
    {
        m_lastError = "数据库未连接";
        qWarning() << m_lastError;
        return false;
    }

    QSqlQuery query(m_db);
    if (!query.exec("CREATE TABLE IF NOT EXISTS " + VERSION_TABLE + " ("
                                                                    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                                                    "version INTEGER NOT NULL,"
                                                                    "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
                                                                    ")"))
    {
        m_lastError = QString("无法创建版本表: %1").arg(query.lastError().text());
        qCritical() << m_lastError;
        return false;
    }

    return true;
}

bool DatabaseManager::initializeDefaultData()
{
    if (!isDatabaseConnected())
    {
        m_lastError = "数据库未连接";
        qWarning() << m_lastError;
        return false;
    }

    // 开始事务确保所有操作都成功或全部失败
    m_db.transaction();

    // 初始化各个表的默认数据
    bool success = initializeInstructionOptions() &&
                   initializePinOptions() &&
                   initializeTypeOptions() &&
                   initializeWaveOptions();

    if (!success)
    {
        m_db.rollback();
        return false;
    }

    // 提交事务
    if (!m_db.commit())
    {
        m_lastError = QString("提交事务失败: %1").arg(m_db.lastError().text());
        qCritical() << m_lastError;
        m_db.rollback();
        return false;
    }

    qInfo() << "默认固定数据初始化成功";
    return true;
}

bool DatabaseManager::initializeInstructionOptions()
{
    QSqlQuery query(m_db);

    // 清空表（如果已有数据）
    if (!query.exec("DELETE FROM instruction_options"))
    {
        m_lastError = QString("清空instruction_options表失败: %1").arg(query.lastError().text());
        qCritical() << m_lastError;
        return false;
    }

    // 根据截图中的数据定义指令选项
    struct InstructionData
    {
        int id;
        QString value;
        int instructionClass;
    };

    QList<InstructionData> instructions = {
        {1, "INC", 0},
        {2, "CALL", 1},
        {3, "CLEAR_MATCH", 1},
        {4, "END_LOOPA", 1},
        {5, "IF_MATCH_JUMP", 1},
        {6, "IF_NOTMATCH_JUMP", 1},
        {7, "JUMP", 1},
        {8, "MATCH_END", 1},
        {9, "MATCH_START", 2},
        {10, "MJUMP", 2},
        {11, "REPEAT", 2},
        {12, "RETURN", 0},
        {13, "SET_LOOPA", 2},
        {14, "SET_MATCH", 0}};

    // 插入数据
    for (const auto &instr : instructions)
    {
        if (!query.exec(QString("INSERT INTO instruction_options (id, instruction_value, instruction_class) "
                                "VALUES (%1, '%2', %3)")
                            .arg(instr.id)
                            .arg(instr.value)
                            .arg(instr.instructionClass)))
        {
            m_lastError = QString("插入instruction_options数据失败: %1").arg(query.lastError().text());
            qCritical() << m_lastError;
            return false;
        }
    }

    return true;
}

bool DatabaseManager::initializePinOptions()
{
    QSqlQuery query(m_db);

    // 清空表
    if (!query.exec("DELETE FROM pin_options"))
    {
        m_lastError = QString("清空pin_options表失败: %1").arg(query.lastError().text());
        qCritical() << m_lastError;
        return false;
    }

    // 根据截图中的数据定义管脚选项
    struct PinOption
    {
        int id;
        QString value;
    };

    QList<PinOption> pinOptions = {
        {1, "0"},
        {2, "1"},
        {3, "L"},
        {4, "H"},
        {5, "X"},
        {6, "S"},
        {7, "V"},
        {8, "M"}};

    // 插入数据
    for (const auto &option : pinOptions)
    {
        if (!query.exec(QString("INSERT INTO pin_options (id, pin_value) VALUES (%1, '%2')")
                            .arg(option.id)
                            .arg(option.value)))
        {
            m_lastError = QString("插入pin_options数据失败: %1").arg(query.lastError().text());
            qCritical() << m_lastError;
            return false;
        }
    }

    return true;
}

bool DatabaseManager::initializeTypeOptions()
{
    QSqlQuery query(m_db);

    // 清空表
    if (!query.exec("DELETE FROM type_options"))
    {
        m_lastError = QString("清空type_options表失败: %1").arg(query.lastError().text());
        qCritical() << m_lastError;
        return false;
    }

    // 根据截图中的数据定义类型选项
    struct TypeOption
    {
        int id;
        QString typeName;
    };

    QList<TypeOption> typeOptions = {
        {1, "In"},
        {2, "Out"},
        {3, "InOut"}};

    // 插入数据
    for (const auto &option : typeOptions)
    {
        if (!query.exec(QString("INSERT INTO type_options (id, type_name) VALUES (%1, '%2')")
                            .arg(option.id)
                            .arg(option.typeName)))
        {
            m_lastError = QString("插入type_options数据失败: %1").arg(query.lastError().text());
            qCritical() << m_lastError;
            return false;
        }
    }

    return true;
}

bool DatabaseManager::initializeWaveOptions()
{
    QSqlQuery query(m_db);

    // 清空表
    if (!query.exec("DELETE FROM wave_options"))
    {
        m_lastError = QString("清空wave_options表失败: %1").arg(query.lastError().text());
        qCritical() << m_lastError;
        return false;
    }

    // 根据截图中的数据定义波形选项
    struct WaveOption
    {
        int id;
        QString waveType;
    };

    QList<WaveOption> waveOptions = {
        {1, "NRZ"},
        {2, "RZ"},
        {3, "RO"},
        {4, "SBC"}};

    // 插入数据
    for (const auto &option : waveOptions)
    {
        if (!query.exec(QString("INSERT INTO wave_options (id, wave_type) VALUES (%1, '%2')")
                            .arg(option.id)
                            .arg(option.waveType)))
        {
            m_lastError = QString("插入wave_options数据失败: %1").arg(query.lastError().text());
            qCritical() << m_lastError;
            return false;
        }
    }

    return true;
}

// ---- Begin performSchemaUpgradeToV2 ----
bool DatabaseManager::performSchemaUpgradeToV2()
{
    qInfo() << "DatabaseManager::performSchemaUpgradeToV2 - 开始数据库升级到版本2...";

    // 1. 执行 update_to_v2.sql 脚本
    QString updateScriptPath;
    if (QFile::exists(":/db/updates/update_to_v2.sql"))
    {
        updateScriptPath = ":/db/updates/update_to_v2.sql";
    }
    else
    {
        // Fallback logic for finding the script - adjust if necessary for your project structure
        QStringList potentialPaths;
        potentialPaths << QCoreApplication::applicationDirPath() + "/resources/db/updates/update_to_v2.sql";
        potentialPaths << QDir(QCoreApplication::applicationDirPath()).filePath("../../resources/db/updates/update_to_v2.sql");
        // Add more paths if needed, e.g. relative to build directory for development

        for (const QString &path : potentialPaths)
        {
            if (QFile::exists(path))
            {
                updateScriptPath = path;
                break;
            }
        }

        if (updateScriptPath.isEmpty())
        {
            m_lastError = "升级脚本 update_to_v2.sql 未找到。检查资源路径 : :/db/updates/update_to_v2.sql 或备用文件系统路径.";
            qCritical() << m_lastError;
            return false;
        }
    }

    qInfo() << "  执行升级脚本: " << updateScriptPath;
    if (!executeSqlScriptFromFile(updateScriptPath))
    {
        // m_lastError 应该由 executeSqlScriptFromFile 设置
        qCritical() << "  执行升级脚本 update_to_v2.sql 失败.";
        return false;
    }
    qInfo() << "  升级脚本执行成功，新表已创建 (如果不存在).";

    qInfo() << "  开始从 vector_tables 迁移元数据到 VectorTableMasterRecord 和 VectorTableColumnConfiguration...";

    QSqlQuery queryOldTables(m_db);
    if (!queryOldTables.exec("SELECT id, table_name FROM vector_tables"))
    {
        m_lastError = "无法查询旧的 vector_tables: " + queryOldTables.lastError().text();
        qCritical() << m_lastError;
        return false;
    }

    QFileInfo dbFileInfo(m_db.databaseName());
    QString dbFileBaseName = dbFileInfo.completeBaseName();
    QString dataSubDirName = dbFileBaseName + "_vbindata";
    QString dataDirPath = dbFileInfo.absolutePath() + QDir::separator() + dataSubDirName;
    QDir dataDir(dataDirPath);

    if (!dataDir.exists())
    {
        if (!dataDir.mkpath("."))
        {
            m_lastError = "无法创建二进制数据子目录: " + dataDirPath;
            qCritical() << m_lastError;
            return false;
        }
        qInfo() << "  已创建二进制数据子目录: " << dataDirPath;
    }

    while (queryOldTables.next())
    {
        int oldTableId = queryOldTables.value("id").toInt();
        QString oldTableName = queryOldTables.value("table_name").toString();
        qInfo() << QString("  处理旧表: '%1' (ID: %2)").arg(oldTableName).arg(oldTableId);

        QString relativeBinaryFilePath = dataSubDirName + QDir::separator() + QString("table_%1_data.vbindata").arg(oldTableId);
        relativeBinaryFilePath = QDir::toNativeSeparators(relativeBinaryFilePath);

        QString absoluteBinaryFilePath = QDir::cleanPath(dataDirPath + QDir::separator() + QString("table_%1_data.vbindata").arg(oldTableId));
        absoluteBinaryFilePath = QDir::toNativeSeparators(absoluteBinaryFilePath);

        QSqlQuery insertMaster(m_db);
        insertMaster.prepare("INSERT INTO VectorTableMasterRecord "
                             "(original_vector_table_id, table_name, binary_data_filename, "
                             "file_format_version, data_schema_version, row_count, column_count) "
                             "VALUES (?, ?, ?, 1, 1, 0, 0)");
        insertMaster.addBindValue(oldTableId);
        insertMaster.addBindValue(oldTableName);
        insertMaster.addBindValue(relativeBinaryFilePath);

        qint64 newMasterId = 0;
        if (!insertMaster.exec())
        {
            if (insertMaster.lastError().nativeErrorCode().toInt() == 19)
            { // SQLITE_CONSTRAINT
                qWarning() << "    VectorTableMasterRecord 记录可能已存在 (UNIQUE constraint failed for original_id or table_name). 尝试获取现有记录.";
                QSqlQuery findMaster(m_db);
                findMaster.prepare("SELECT id FROM VectorTableMasterRecord WHERE original_vector_table_id = ? OR table_name = ?");
                findMaster.addBindValue(oldTableId);
                findMaster.addBindValue(oldTableName);
                if (findMaster.exec() && findMaster.next())
                {
                    newMasterId = findMaster.value(0).toLongLong();
                    qInfo() << "    找到已存在的 VectorTableMasterRecord ID: " << newMasterId;
                }
                else
                {
                    m_lastError = QString("VectorTableMasterRecord 记录已存在但无法重新获取ID for original_table_id %1: %2").arg(oldTableId).arg(findMaster.lastError().text());
                    qCritical() << m_lastError;
                    return false;
                }
            }
            else
            {
                m_lastError = QString("无法在 VectorTableMasterRecord 中为旧表 '%1' (ID: %2) 创建记录: %3")
                                  .arg(oldTableName)
                                  .arg(oldTableId)
                                  .arg(insertMaster.lastError().text());
                qCritical() << m_lastError;
                return false;
            }
        }
        else
        {
            newMasterId = insertMaster.lastInsertId().toLongLong();
            qInfo() << "    已创建 VectorTableMasterRecord ID: " << newMasterId;
        }

        if (newMasterId == 0)
        {
            m_lastError = QString("未能为旧表ID %1 获取有效的 newMasterId.").arg(oldTableId);
            qCritical() << m_lastError;
            return false;
        }

        QSqlQuery deleteCols(m_db);
        deleteCols.prepare("DELETE FROM VectorTableColumnConfiguration WHERE master_record_id = ?");
        deleteCols.addBindValue(newMasterId);
        if (!deleteCols.exec())
        {
            m_lastError = "无法清理旧的列配置 for master_id " + QString::number(newMasterId) + ": " + deleteCols.lastError().text();
            qCritical() << m_lastError;
            return false;
        }
        qDebug() << "    已清理 VectorTableColumnConfiguration for master_id: " << newMasterId;

        QSqlQuery insertColumn(m_db);
        insertColumn.prepare("INSERT INTO VectorTableColumnConfiguration "
                             "(master_record_id, column_name, column_order, column_type, data_properties) "
                             "VALUES (?, ?, ?, ?, ?)");

        int currentColumnOrder = 0;
        struct FixedColumnInfo
        {
            QString name;
            QString type;
        };
        QList<FixedColumnInfo> fixedColumns = {
            {"Label", "TEXT"}, {"Instruction", "INSTRUCTION_ID"}, {"TimeSet", "TIMESET_ID"}, {"Capture", "TEXT"}, {"Ext", "TEXT"}, {"Comment", "TEXT"}};

        for (const auto &colInfo : fixedColumns)
        {
            insertColumn.bindValue(0, newMasterId);
            insertColumn.bindValue(1, colInfo.name);
            insertColumn.bindValue(2, currentColumnOrder++);
            insertColumn.bindValue(3, colInfo.type);
            insertColumn.bindValue(4, QVariant());
            if (!insertColumn.exec())
            {
                m_lastError = QString("无法为 Master ID %1 插入固定列 '%2': %3")
                                  .arg(newMasterId)
                                  .arg(colInfo.name)
                                  .arg(insertColumn.lastError().text());
                qCritical() << m_lastError;
                return false;
            }
        }
        qDebug() << "    已插入 " << fixedColumns.count() << " 个固定列配置.";

        QSqlQuery queryPins(m_db);
        queryPins.prepare("SELECT vtp.id as vtp_id, pl.id as pin_list_id, pl.pin_name, vtp.pin_channel_count, vtp.pin_type "
                          "FROM vector_table_pins vtp "
                          "JOIN pin_list pl ON vtp.pin_id = pl.id "
                          "WHERE vtp.table_id = ? ORDER BY vtp.id");
        queryPins.addBindValue(oldTableId);
        if (!queryPins.exec())
        {
            m_lastError = QString("无法查询旧表ID %1 的管脚信息: %2").arg(oldTableId).arg(queryPins.lastError().text());
            qCritical() << m_lastError;
            return false;
        }

        int pinColumnCount = 0;
        while (queryPins.next())
        {
            int pinListId = queryPins.value("pin_list_id").toInt();
            QString pinName = queryPins.value("pin_name").toString();
            int channelCount = queryPins.value("pin_channel_count").toInt();
            int typeOptionId = queryPins.value("pin_type").toInt();

            QJsonObject pinProps;
            pinProps["pin_list_id"] = pinListId;
            pinProps["channel_count"] = channelCount;
            pinProps["type_option_id"] = typeOptionId;
            QString propertiesJson = QJsonDocument(pinProps).toJson(QJsonDocument::Compact);

            insertColumn.bindValue(0, newMasterId);
            insertColumn.bindValue(1, pinName);
            insertColumn.bindValue(2, currentColumnOrder++);
            insertColumn.bindValue(3, "PIN_STATE_ID");
            insertColumn.bindValue(4, propertiesJson);
            if (!insertColumn.exec())
            {
                m_lastError = QString("无法为 Master ID %1 插入管脚列 '%2': %3")
                                  .arg(newMasterId)
                                  .arg(pinName)
                                  .arg(insertColumn.lastError().text());
                qCritical() << m_lastError;
                return false;
            }
            pinColumnCount++;
        }
        qDebug() << "    已插入 " << pinColumnCount << " 个管脚列配置.";

        int totalColumnCount = fixedColumns.count() + pinColumnCount;
        QSqlQuery updateMasterCounts(m_db);
        updateMasterCounts.prepare("UPDATE VectorTableMasterRecord SET column_count = ? WHERE id = ?");
        updateMasterCounts.addBindValue(totalColumnCount);
        updateMasterCounts.addBindValue(newMasterId);
        if (!updateMasterCounts.exec())
        {
            m_lastError = "无法更新 MasterRecord ID " + QString::number(newMasterId) + " 的 column_count: " + updateMasterCounts.lastError().text();
            qCritical() << m_lastError;
            return false;
        }
        qDebug() << "    已更新 MasterRecord column_count 为 " << totalColumnCount;

        QFile binaryFile(absoluteBinaryFilePath);
        if (binaryFile.exists())
        {
            qWarning() << "  二进制文件 " << absoluteBinaryFilePath << " 已存在。跳过创建。";
        }
        else
        {
            if (!binaryFile.open(QIODevice::WriteOnly))
            {
                m_lastError = QString("无法创建空的二进制文件: %1. Error: %2").arg(absoluteBinaryFilePath).arg(binaryFile.errorString());
                qCritical() << m_lastError;
                return false;
            }
            binaryFile.close();
            qInfo() << "    已创建空的二进制文件: " << absoluteBinaryFilePath;
        }
    }

    qInfo() << "  元数据迁移和列配置完成.";

    // 调用数据迁移函数，将旧表中的行数据迁移到二进制文件
    qInfo() << "  开始将向量表数据从SQLite迁移到二进制文件...";
    if (!migrateVectorDataToBinaryFiles())
    {
        qCritical() << "  向量表数据迁移失败: " << m_lastError;
        return false;
    }
    qInfo() << "  向量表数据迁移完成.";

    if (!setCurrentDbFileVersionInTable(2))
    {
        qCritical() << "  无法在 db_version 表中将版本更新到 2.";
        return false;
    }

    qInfo() << "DatabaseManager::performSchemaUpgradeToV2 - 数据库成功升级到版本2.";
    return true;
}
// ---- End performSchemaUpgradeToV2 ----

bool DatabaseManager::performSchemaUpgradeToV3()
{
    qInfo() << "DatabaseManager::performSchemaUpgradeToV3 - 开始数据库升级到版本3...";

    // 1. 执行 update_to_v3.sql 脚本
    QString updateScriptPath;
    if (QFile::exists(":/db/updates/update_to_v3.sql"))
    {
        updateScriptPath = ":/db/updates/update_to_v3.sql";
    }
    else
    {
        // 尝试查找脚本文件的备选路径
        QStringList potentialPaths;
        potentialPaths << QCoreApplication::applicationDirPath() + "/resources/db/updates/update_to_v3.sql";
        potentialPaths << QDir(QCoreApplication::applicationDirPath()).filePath("../../resources/db/updates/update_to_v3.sql");

        for (const QString &path : potentialPaths)
        {
            if (QFile::exists(path))
            {
                updateScriptPath = path;
                break;
            }
        }

        if (updateScriptPath.isEmpty())
        {
            m_lastError = "升级脚本 update_to_v3.sql 未找到。检查资源路径 : :/db/updates/update_to_v3.sql 或备用文件系统路径.";
            qCritical() << m_lastError;
            return false;
        }
    }

    qInfo() << "  执行升级脚本: " << updateScriptPath;
    if (!executeSqlScriptFromFile(updateScriptPath))
    {
        qCritical() << "  执行升级脚本 update_to_v3.sql 失败.";
        return false;
    }
    qInfo() << "  升级脚本执行成功，已添加 IsVisible 字段到 VectorTableColumnConfiguration 表.";

    // 更新数据库版本号
    if (!setCurrentDbFileVersionInTable(3))
    {
        qCritical() << "  无法在 db_version 表中将版本更新到 3.";
        return false;
    }

    m_currentDbFileVersion = 3;
    qInfo() << "DatabaseManager::performSchemaUpgradeToV3 - 数据库成功升级到版本3.";
    return true;
}