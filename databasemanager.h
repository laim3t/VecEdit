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

private:
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

    // 单例实例
    static DatabaseManager *m_instance;

    // 当前数据库的版本
    int m_currentVersion;

    // 数据库版本表名
    QString VERSION_TABLE = "db_version";
};

#endif // DATABASEMANAGER_H