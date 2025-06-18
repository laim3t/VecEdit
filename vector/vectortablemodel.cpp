#include "vectortablemodel.h"
#include "../database/databasemanager.h"
#include "../common/logger.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QFileInfo>
#include <QUuid>
#include <QDebug>
#include <QDir>
#include <QCoreApplication>

namespace Vector
{

    VectorTableModel::VectorTableModel(QObject *parent)
        : QAbstractTableModel(parent),
          m_tableId(-1),
          m_rowCount(0),
          m_columnCount(0),
          m_schemaVersion(0),
          m_cacheHits(0),
          m_cacheMisses(0)
    {
        // 创建唯一的数据库连接名
        m_dbConnectionName = QString("VectorTableModel_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));

        qDebug() << "VectorTableModel创建，连接名:" << m_dbConnectionName;
    }

    VectorTableModel::~VectorTableModel()
    {
        qDebug() << "VectorTableModel销毁，连接名:" << m_dbConnectionName;
        qDebug() << "缓存统计 - 命中:" << m_cacheHits << "次, 未命中:" << m_cacheMisses << "次";
        qDebug() << "缓存命中率:" << (m_cacheHits + m_cacheMisses > 0 ? (double)m_cacheHits / (m_cacheHits + m_cacheMisses) * 100 : 0) << "%";

        cleanup();
    }

    int VectorTableModel::rowCount(const QModelIndex &parent) const
    {
        // 如果parent有效，则表示这是一个子项
        // 由于我们是一个简单的表格模型，不支持树形结构，所以返回0
        if (parent.isValid())
        {
            return 0;
        }

        return m_rowCount;
    }

    int VectorTableModel::columnCount(const QModelIndex &parent) const
    {
        if (parent.isValid())
        {
            return 0;
        }

        return m_columnCount;
    }

    QVariant VectorTableModel::data(const QModelIndex &index, int role) const
    {
        // 验证索引有效性
        if (!index.isValid() || m_tableId < 0 || m_columns.isEmpty())
        {
            return QVariant();
        }

        int row = index.row();
        int column = index.column();

        // 范围检查
        if (row < 0 || row >= m_rowCount || column < 0 || column >= m_columnCount)
        {
            return QVariant();
        }

        // 默认只处理DisplayRole和EditRole
        if (role != Qt::DisplayRole && role != Qt::EditRole)
        {
            return QVariant();
        }

        // 按需获取行数据
        RowData rowData = fetchRowData(row);

        // 确保列索引在有效范围内
        if (column < rowData.size())
        {
            return rowData.at(column);
        }

        return QVariant();
    }

    QVariant VectorTableModel::headerData(int section, Qt::Orientation orientation, int role) const
    {
        // 只处理DisplayRole
        if (role != Qt::DisplayRole)
        {
            return QVariant();
        }

        // 水平表头使用列名
        if (orientation == Qt::Horizontal)
        {
            if (section >= 0 && section < m_columns.size())
            {
                return m_columns.at(section).name;
            }
        }
        // 垂直表头使用行号(从1开始)
        else if (orientation == Qt::Vertical)
        {
            return section + 1;
        }

        return QVariant();
    }

    bool VectorTableModel::setTable(int tableId)
    {
        // 如果是同一个表，不做任何操作
        if (m_tableId == tableId)
        {
            qDebug() << "VectorTableModel::setTable - 已加载表ID:" << tableId;
            return true;
        }

        // 清理旧资源
        cleanup();

        // 设置新表ID
        m_tableId = tableId;

        // 加载表元数据
        beginResetModel();
        bool success = loadTableMetadata();
        endResetModel();

        if (!success)
        {
            m_tableId = -1; // 重置表ID
            return false;
        }

        qDebug() << "VectorTableModel::setTable - 成功加载表ID:" << tableId
                 << "行数:" << m_rowCount
                 << "列数:" << m_columnCount;

        return true;
    }

    bool VectorTableModel::loadTableMetadata()
    {
        // 确保有效的数据库连接
        if (!ensureDatabaseConnection())
        {
            return false;
        }

        // 清空旧的元数据
        m_rowCount = 0;
        m_columnCount = 0;
        m_columns.clear();
        m_binaryFilePath.clear();

        QSqlDatabase db = QSqlDatabase::database(m_dbConnectionName);

        // 获取表映射关系 - 查询VectorTableMasterRecord表获取二进制文件信息
        QSqlQuery query(db);
        query.prepare("SELECT row_count, binary_data_filename, data_schema_version FROM VectorTableMasterRecord "
                      "WHERE original_vector_table_id = ? OR id = ?");
        query.addBindValue(m_tableId);
        query.addBindValue(m_tableId);

        if (!query.exec())
        {
            qWarning() << "VectorTableModel::loadTableMetadata - 查询失败:" << query.lastError().text();
            return false;
        }

        if (!query.next())
        {
            // 尝试使用旧的表结构（兼容性支持）
            query.clear();
            query.prepare("SELECT table_name FROM vector_tables WHERE id = ?");
            query.addBindValue(m_tableId);

            if (!query.exec() || !query.next())
            {
                qWarning() << "VectorTableModel::loadTableMetadata - 表ID未找到:" << m_tableId;
                return false;
            }

            QString tableName = query.value(0).toString();
            qWarning() << "VectorTableModel::loadTableMetadata - 使用旧表结构:" << tableName;

            // 使用旧表结构的处理逻辑
            QString binFileName = QString("table_%1_data.vbindata").arg(m_tableId);
            m_rowCount = 1000; // 默认值，实际应该从二进制文件头读取
            m_schemaVersion = 1;

            // 获取二进制文件路径
            QString dbPath = DatabaseManager::instance()->database().databaseName();
            QFileInfo dbInfo(dbPath);
            QString dbDir = dbInfo.absolutePath();

            // 尝试几个可能的位置
            QStringList possibleDirs;
            possibleDirs << dbDir // 数据库所在目录
                         << dbDir + "/TEST3211_vbindata" 
                         << dbDir + "/../tests/database/TEST3211_vbindata";
            
            // 添加额外的可能路径（基于数据库文件所在的实际路径）
            QString dbFileName = dbInfo.fileName();
            QString dbBaseName = dbInfo.baseName();
            
            // 添加以数据库名称命名的子目录
            possibleDirs << dbDir + "/" + dbBaseName + "_vbindata";
            
            // 添加项目目录和父目录的可能位置
            QString appDir = QCoreApplication::applicationDirPath();
            possibleDirs << appDir + "/database";
            possibleDirs << appDir + "/../database";
            possibleDirs << appDir + "/tests/database";
            possibleDirs << appDir + "/../tests/database";
            
            // 日志输出当前数据库路径和我们尝试查找的文件
            qDebug() << "VectorTableModel::loadTableMetadata - 数据库路径: " << dbPath;
            qDebug() << "VectorTableModel::loadTableMetadata - 尝试寻找二进制文件: " << binFileName;
            qDebug() << "VectorTableModel::loadTableMetadata - 将在以下目录中查找:";
            foreach (const QString &dir, possibleDirs) {
                qDebug() << " - " << dir;
            }

            bool found = false;
            for (const QString &dir : possibleDirs)
            {
                QString path = QDir(dir).filePath(binFileName);
                if (QFile::exists(path))
                {
                    m_binaryFilePath = path;
                    found = true;
                    qDebug() << "VectorTableModel::loadTableMetadata - 找到二进制文件: " << path;
                    break;
                }
            }

            if (!found)
            {
                // 尝试直接使用相对路径（相对于数据库文件）和绝对路径
                if (QFile::exists(binFileName)) {
                    m_binaryFilePath = QFileInfo(binFileName).absoluteFilePath();
                    found = true;
                    qDebug() << "VectorTableModel::loadTableMetadata - 使用直接文件名找到二进制文件: " << m_binaryFilePath;
                } else {
                    // 尝试使用数据库目录作为基准的相对路径
                    QString relativePath = QDir(dbDir).absoluteFilePath(binFileName);
                    if (QFile::exists(relativePath)) {
                        m_binaryFilePath = relativePath;
                        found = true;
                        qDebug() << "VectorTableModel::loadTableMetadata - 使用相对路径找到二进制文件: " << m_binaryFilePath;
                    }
                }
            }

            if (!found)
            {
                qWarning() << "VectorTableModel::loadTableMetadata - 无法找到二进制文件:" << binFileName;
                return false;
            }
        }
        else
        {
            // 使用新的VectorTableMasterRecord表结构
            m_rowCount = query.value(0).toInt();
            QString binFileName = query.value(1).toString();
            m_schemaVersion = query.value(2).toInt();

            // 获取二进制文件路径
            QString dbPath = DatabaseManager::instance()->database().databaseName();
            QFileInfo dbInfo(dbPath);
            QString dbDir = dbInfo.absolutePath();

            // 尝试几个可能的位置
            QStringList possibleDirs;
            possibleDirs << dbDir // 数据库所在目录
                         << dbDir + "/TEST3211_vbindata" 
                         << dbDir + "/../tests/database/TEST3211_vbindata";
            
            // 添加额外的可能路径（基于数据库文件所在的实际路径）
            QString dbFileName = dbInfo.fileName();
            QString dbBaseName = dbInfo.baseName();
            
            // 添加以数据库名称命名的子目录
            possibleDirs << dbDir + "/" + dbBaseName + "_vbindata";
            
            // 添加项目目录和父目录的可能位置
            QString appDir = QCoreApplication::applicationDirPath();
            possibleDirs << appDir + "/database";
            possibleDirs << appDir + "/../database";
            possibleDirs << appDir + "/tests/database";
            possibleDirs << appDir + "/../tests/database";
            
            // 日志输出当前数据库路径和我们尝试查找的文件
            qDebug() << "VectorTableModel::loadTableMetadata - 数据库路径: " << dbPath;
            qDebug() << "VectorTableModel::loadTableMetadata - 尝试寻找二进制文件: " << binFileName;
            qDebug() << "VectorTableModel::loadTableMetadata - 将在以下目录中查找:";
            foreach (const QString &dir, possibleDirs) {
                qDebug() << " - " << dir;
            }

            bool found = false;
            for (const QString &dir : possibleDirs)
            {
                QString path = QDir(dir).filePath(binFileName);
                if (QFile::exists(path))
                {
                    m_binaryFilePath = path;
                    found = true;
                    qDebug() << "VectorTableModel::loadTableMetadata - 找到二进制文件: " << path;
                    break;
                }
            }

            if (!found)
            {
                // 尝试直接使用相对路径（相对于数据库文件）和绝对路径
                if (QFile::exists(binFileName)) {
                    m_binaryFilePath = QFileInfo(binFileName).absoluteFilePath();
                    found = true;
                    qDebug() << "VectorTableModel::loadTableMetadata - 使用直接文件名找到二进制文件: " << m_binaryFilePath;
                } else {
                    // 尝试使用数据库目录作为基准的相对路径
                    QString relativePath = QDir(dbDir).absoluteFilePath(binFileName);
                    if (QFile::exists(relativePath)) {
                        m_binaryFilePath = relativePath;
                        found = true;
                        qDebug() << "VectorTableModel::loadTableMetadata - 使用相对路径找到二进制文件: " << m_binaryFilePath;
                    }
                }
            }

            if (!found)
            {
                qWarning() << "VectorTableModel::loadTableMetadata - 无法找到二进制文件:" << binFileName;
                return false;
            }
        }

        // 检查二进制文件是否存在
        if (m_binaryFilePath.isEmpty() || !QFile::exists(m_binaryFilePath))
        {
            qWarning() << "VectorTableModel::loadTableMetadata - 二进制文件不存在:" << m_binaryFilePath;
            return false;
        }

        qDebug() << "VectorTableModel::loadTableMetadata - 使用二进制文件:" << m_binaryFilePath;

        // 尝试从VectorTableColumnConfiguration获取列信息
        query.clear();

        // 先尝试找到master_record_id
        int masterRecordId = -1;
        query.prepare("SELECT id FROM VectorTableMasterRecord WHERE original_vector_table_id = ? OR id = ?");
        query.addBindValue(m_tableId);
        query.addBindValue(m_tableId);

        if (query.exec() && query.next())
        {
            masterRecordId = query.value(0).toInt();
        }

        if (masterRecordId > 0)
        {
            // 使用新表结构
            query.clear();
            query.prepare("SELECT id, column_name, column_type, column_order, data_properties "
                          "FROM VectorTableColumnConfiguration "
                          "WHERE master_record_id = ? AND IsVisible = 1 "
                          "ORDER BY column_order ASC");
            query.addBindValue(masterRecordId);
        }
        else
        {
            // 尝试使用旧表结构或创建默认列
            query.clear();
            query.prepare("SELECT vcp.id, pl.pin_name, vcp.pin_type, vcp.id as col_order, NULL as data_props "
                          "FROM vector_table_pins vcp "
                          "JOIN pin_list pl ON vcp.pin_id = pl.id "
                          "WHERE vcp.table_id = ? "
                          "UNION ALL "
                          "SELECT -1, 'Label', 0, -5, NULL "
                          "UNION ALL "
                          "SELECT -2, 'Instruction', 0, -4, NULL "
                          "UNION ALL "
                          "SELECT -3, 'TimeSet', 0, -3, NULL "
                          "UNION ALL "
                          "SELECT -4, 'Capture', 0, -2, NULL "
                          "UNION ALL "
                          "SELECT -5, 'Comment', 0, -1, NULL "
                          "ORDER BY col_order");
            query.addBindValue(m_tableId);
        }

        if (!query.exec())
        {
            qWarning() << "VectorTableModel::loadTableMetadata - 无法加载列信息:"
                       << query.lastError().text();
            return false;
        }

        // 处理列信息
        bool hasColumns = false;
        while (query.next())
        {
            hasColumns = true;

            ColumnInfo column;
            column.id = query.value(0).toInt();
            column.name = query.value(1).toString();

            // 处理列类型
            if (masterRecordId > 0)
            {
                // 新表结构，直接转换字符串类型
                QString typeStr = query.value(2).toString();
                column.type = Vector::columnDataTypeFromString(typeStr);
            }
            else
            {
                // 旧表结构，假设使用枚举值
                int typeValue = query.value(2).toInt();
                column.type = static_cast<Vector::ColumnDataType>(typeValue);
            }

            column.order = query.value(3).toInt();

            // 处理数据属性（如果有）
            QVariant dataProps = query.value(4);
            if (dataProps.isValid() && !dataProps.isNull())
            {
                // 这里可以解析JSON属性，但现在简单存储原始字符串
                column.original_type_str = dataProps.toString();
            }

            m_columns.append(column);
        }

        // 如果没有列，创建默认列
        if (!hasColumns)
        {
            // 添加一些基本列
            ColumnInfo labelCol;
            labelCol.id = -1;
            labelCol.name = "Label";
            labelCol.type = Vector::ColumnDataType::TEXT;
            labelCol.order = 0;
            m_columns.append(labelCol);

            ColumnInfo instrCol;
            instrCol.id = -2;
            instrCol.name = "Instruction";
            instrCol.type = Vector::ColumnDataType::INSTRUCTION_ID;
            instrCol.order = 1;
            m_columns.append(instrCol);

            ColumnInfo timesetCol;
            timesetCol.id = -3;
            timesetCol.name = "TimeSet";
            timesetCol.type = Vector::ColumnDataType::TIMESET_ID;
            timesetCol.order = 2;
            m_columns.append(timesetCol);

            qDebug() << "VectorTableModel::loadTableMetadata - 使用默认列结构";
        }

        // 设置列数
        m_columnCount = m_columns.size();

        // 确保索引文件存在
        ensureIndexFileExists();

        return true;
    }

    // 尝试打开并保持与数据库的连接
    bool VectorTableModel::ensureDatabaseConnection()
    {
        // 检查连接是否已存在且有效
        if (QSqlDatabase::contains(m_dbConnectionName))
        {
            QSqlDatabase db = QSqlDatabase::database(m_dbConnectionName);
            if (db.isOpen())
            {
                return true;
            }
        }

        // 创建新连接
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", m_dbConnectionName);
        db.setDatabaseName(DatabaseManager::instance()->database().databaseName());

        if (!db.open())
        {
            qWarning() << "VectorTableModel::ensureDatabaseConnection - 无法打开数据库:"
                       << db.lastError().text();
            return false;
        }

        // 设置外键约束
        QSqlQuery query(db);
        query.exec("PRAGMA foreign_keys = ON");

        return true;
    }

    // 确保二进制文件被打开
    bool VectorTableModel::ensureBinaryFileOpen() const
    {
        // 检查文件是否已打开
        if (m_binaryFile.isOpen())
        {
            return true;
        }

        // 如果未打开，尝试打开文件
        m_binaryFile.setFileName(m_binaryFilePath);
        if (!m_binaryFile.open(QIODevice::ReadOnly))
        {
            qWarning() << "VectorTableModel::ensureBinaryFileOpen - 无法打开二进制文件:"
                       << m_binaryFile.errorString();
            return false;
        }

        return true;
    }

    // 确保索引文件存在，如果不存在则创建
    bool VectorTableModel::ensureIndexFileExists() const
    {
        // 创建索引文件 - 只检查是否存在，不主动创建（暂时先返回true）
        // 在实际项目中，需要调用BinaryFileHelper中的相应方法创建索引
        bool success = QFile::exists(m_binaryFilePath);
        if (success)
        {
            qDebug() << "VectorTableModel::ensureIndexFileExists - 二进制文件存在，假设索引有效";
        }
        else
        {
            qDebug() << "VectorTableModel::ensureIndexFileExists - 二进制文件不存在，无法创建索引";
        }

        return success;
    }

    // 获取行数据，优先从缓存中获取
    RowData VectorTableModel::fetchRowData(int rowIndex) const
    {
        // 首先从缓存中查找
        if (m_rowCache.contains(rowIndex))
        {
            m_cacheHits++;
            return m_rowCache[rowIndex];
        }

        // 缓存未命中，需要从文件中读取
        m_cacheMisses++;

        RowData rowData;

        // 调用BinaryFileHelper直接从文件读取数据
        bool success = Persistence::BinaryFileHelper::readRowFromBinary(
            m_binaryFilePath,
            m_columns,
            m_schemaVersion,
            rowIndex,
            rowData,
            true // 使用索引加速
        );

        if (!success)
        {
            qWarning() << "VectorTableModel::fetchRowData - 读取行" << rowIndex << "失败";
            return RowData();
        }

        // 将数据添加到缓存
        if (m_rowCache.size() >= MAX_CACHE_SIZE)
        {
            // 如果缓存已满，移除最早添加的项(简单LRU策略)
            // 实际应用中可能需要更复杂的缓存淘汰策略
            m_rowCache.remove(m_rowCache.firstKey());
        }

        m_rowCache[rowIndex] = rowData;

        return rowData;
    }

    // 清理资源
    void VectorTableModel::cleanup()
    {
        // 关闭二进制文件
        if (m_binaryFile.isOpen())
        {
            m_binaryFile.close();
        }

        // 关闭数据库连接
        if (QSqlDatabase::contains(m_dbConnectionName))
        {
            QSqlDatabase::database(m_dbConnectionName).close();
        }

        // 清除行缓存
        m_rowCache.clear();
        m_cacheHits = 0;
        m_cacheMisses = 0;

        // 重置基本变量
        m_rowCount = 0;
        m_columnCount = 0;
        m_columns.clear();
        m_binaryFilePath.clear();
    }

} // namespace Vector