#include "databasemanager.h"

// 静态实例初始化为nullptr
DatabaseManager *DatabaseManager::m_instance = nullptr;

DatabaseManager *DatabaseManager::instance()
{
    if (!m_instance)
    {
        m_instance = new DatabaseManager();
    }
    return m_instance;
}

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent), m_currentVersion(0)
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

    // 创建版本表并设置初始版本为1
    if (!createVersionTableIfNotExists())
    {
        m_db.close();
        dbFile.remove();
        return false;
    }

    // 设置初始版本
    QSqlQuery query(m_db);
    if (!query.exec("INSERT INTO " + VERSION_TABLE + " (version) VALUES (1)"))
    {
        m_lastError = QString("无法设置初始数据库版本: %1").arg(query.lastError().text());
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

    m_currentVersion = 1;
    qInfo() << "数据库已成功初始化: " << dbFilePath;
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
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(dbFilePath);

    // 打开数据库
    if (!m_db.open())
    {
        m_lastError = QString("无法打开数据库: %1").arg(m_db.lastError().text());
        qCritical() << m_lastError;
        return false;
    }

    // 检查版本表是否存在，获取当前版本
    QSqlQuery query(m_db);
    if (!query.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='" + VERSION_TABLE + "'"))
    {
        m_lastError = QString("无法检查版本表: %1").arg(query.lastError().text());
        qCritical() << m_lastError;
        m_db.close();
        return false;
    }

    if (!query.next())
    {
        // 版本表不存在，尝试创建
        if (!createVersionTableIfNotExists())
        {
            m_db.close();
            return false;
        }
        // 假设这是版本1
        m_currentVersion = 1;
    }
    else
    {
        // 版本表存在，获取当前版本
        if (!query.exec("SELECT MAX(version) FROM " + VERSION_TABLE))
        {
            m_lastError = QString("无法获取数据库版本: %1").arg(query.lastError().text());
            qCritical() << m_lastError;
            m_db.close();
            return false;
        }

        if (query.next())
        {
            m_currentVersion = query.value(0).toInt();
        }
        else
        {
            m_currentVersion = 0;
        }
    }

    qInfo() << "数据库已成功打开: " << dbFilePath << "，当前版本: " << m_currentVersion;
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
    return m_currentVersion;
}

bool DatabaseManager::updateDatabaseSchema(int targetVersion, const QString &updateScriptPath)
{
    if (!isDatabaseConnected())
    {
        m_lastError = "数据库未连接";
        qWarning() << m_lastError;
        return false;
    }

    if (targetVersion <= m_currentVersion)
    {
        m_lastError = QString("目标版本 %1 不大于当前版本 %2").arg(targetVersion).arg(m_currentVersion);
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

    m_currentVersion = targetVersion;
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

    // 将脚本分割成单独的SQL语句
    QStringList statements = scriptContent.split(';', Qt::SkipEmptyParts);

    for (const QString &statement : statements)
    {
        QString trimmedStatement = statement.trimmed();
        if (!trimmedStatement.isEmpty())
        {
            if (!executeQuery(trimmedStatement))
            {
                return false; // 错误信息已在executeQuery中设置
            }
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
    if (!file.exists())
    {
        m_lastError = QString("SQL脚本文件不存在: %1").arg(filePath);
        qWarning() << m_lastError;
        return QString();
    }

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
        {5, "X"}};

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