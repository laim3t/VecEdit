#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QString>
#include <QVariant>

class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    // 单例模式，确保整个应用程序只有一个数据库连接实例
    static DatabaseManager *instance();
    ~DatabaseManager();

    // 支持的最新数据库 Schema 版本号
    static const int LATEST_DB_SCHEMA_VERSION = 3;

    // 初始化新数据库（从schema.sql创建）
    bool initializeNewDatabase(const QString &dbFilePath, const QString &schemaFilePath);

    // 打开现有数据库连接
    bool openExistingDatabase(const QString &dbFilePath);

    // 关闭数据库连接
    void closeDatabase();

    // 检查数据库是否已连接
    bool isDatabaseConnected() const;

    // 返回最近的错误信息
    QString lastError() const;

    // 版本管理相关方法
    int getCurrentDatabaseVersion() const;
    bool updateDatabaseSchema(int targetVersion, const QString &updateScriptPath);
    bool registerVersionTable(); // 如果不存在版本表，则创建它

    // 获取数据库连接的访问方法（例如，给其他类使用）
    QSqlDatabase database() const;

    // 执行一个简单的SQL查询
    bool executeQuery(const QString &queryStr);

    // 执行多个SQL语句（从文件或字符串）
    bool executeSqlScript(const QString &scriptContent);
    bool executeSqlScriptFromFile(const QString &scriptFilePath);

    // 初始化固定表的默认数据
    bool initializeDefaultData();

    // 当前数据库的版本 (从 db_version 表读取)
    int m_currentDbFileVersion;

private:
    // 升级数据库 Schema 到特定版本的辅助函数
    bool performSchemaUpgradeToV2();
    bool performSchemaUpgradeToV3();

    // 将旧格式(SQLite表)的向量表数据迁移到新格式(二进制文件)
    bool migrateVectorDataToBinaryFiles();

    // 内部函数，用于更新数据库中的版本号记录
    bool setCurrentDbFileVersionInTable(int version);

    // 确保版本表存在并返回当前版本（如果存在）
    int ensureVersionTableAndGetCurrentVersion();

    // 私有构造函数（单例模式的一部分）
    explicit DatabaseManager(QObject *parent = nullptr);

    // 禁止复制构造和赋值操作
    DatabaseManager(const DatabaseManager &) = delete;
    DatabaseManager &operator=(const DatabaseManager &) = delete;

    // 从文件读取SQL脚本
    QString readSqlScriptFromFile(const QString &filePath);

    // 创建版本表如果不存在
    bool createVersionTableIfNotExists();

    // 初始化特定表的固定数据
    bool initializeInstructionOptions();
    bool initializePinOptions();
    bool initializeTypeOptions();
    bool initializeWaveOptions();

    // 用于存储最后发生的错误
    QString m_lastError;

    // 数据库连接
    QSqlDatabase m_db;

    // 当前打开的数据库文件路径
    QString m_dbFilePath;

    // 单例实例
    static DatabaseManager *m_instance;

    // 数据库版本表名 (确保只有一个定义)
    static const QString VERSION_TABLE;
};

#endif // DATABASEMANAGER_H