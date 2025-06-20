#include "vectortablemodel.h"
#include "vectordatahandler.h"
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
#include <QApplication>
#include <QElapsedTimer>

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
        // 不再创建唯一的数据库连接名，直接使用主连接的名称
        m_dbConnectionName = DatabaseManager::instance()->database().connectionName();

        qDebug() << "VectorTableModel创建，使用共享连接:" << m_dbConnectionName;
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
        qDebug() << "[CERTAINTY_CHECK] Entering VectorTableModel::setTable with tableId:" << tableId;
        
        // ========= 100% CERTAINTY PROBE: PART 2 =========
        // 在执行任何模型操作之前，先进行一次独立的验证性读取
        QSqlDatabase modelDbHandle = DatabaseManager::instance()->database();
        qDebug() << "[CERTAINTY_CHECK] In setTable, got a DB handle. Connection name:" << modelDbHandle.connectionName();
        QSqlQuery modelValidator(modelDbHandle);
        modelValidator.prepare("SELECT row_count FROM VectorTableMasterRecord WHERE id = ?");
        modelValidator.addBindValue(tableId);
        if (modelValidator.exec() && modelValidator.next()) {
            int count = modelValidator.value(0).toInt();
            qDebug() << "[CERTAINTY_CHECK] --- CRITICAL --- Model's validator read row_count =" << count << "for tableId" << tableId;
        } else {
            qWarning() << "[CERTAINTY_CHECK] --- CRITICAL --- Model's validator FAILED to read row_count. Error:" << modelValidator.lastError().text();
        }
        // ===================================================
        
        // **关键修复：在执行任何操作前，都从管理器获取最新的数据库句柄实例**
        m_dbConnectionName = modelDbHandle.connectionName(); // 更新连接名
        
        qDebug() << "[UI_REFRESH_DEBUG] Entering VectorTableModel::setTable with tableId:" << tableId;
        
        // 如果是同一个表，不做任何操作，但增加检查确保句柄有效
        if (m_tableId == tableId && DatabaseManager::instance()->database().isOpen())
        {
            qDebug() << "[UI_REFRESH_DEBUG] tableId is the same, aborting.";
            return true;
        }

        // 清理旧资源
        cleanup();

        // 设置新表ID
        m_tableId = tableId;

        // 加载表元数据
        beginResetModel();
        bool success = loadTableMetadata();
        
        qDebug() << "[UI_REFRESH_DEBUG] loadTableMetadata returned:" << success << ". The model's internal m_rowCount is now:" << m_rowCount;
        
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

        // 使用主数据库连接而非自己的连接
        QSqlDatabase db = DatabaseManager::instance()->database();

        // 获取表映射关系 - 查询VectorTableMasterRecord表获取二进制文件信息
        QSqlQuery query(db);
        query.prepare("SELECT row_count, binary_data_filename, data_schema_version FROM VectorTableMasterRecord "
                      "WHERE original_vector_table_id = ? OR id = ?");
        query.addBindValue(m_tableId);
        query.addBindValue(m_tableId);

        if (!query.exec())
        {
            qWarning() << "VectorTableModel::loadTableMetadata - 查询失败:" << query.lastError().text();
            qWarning() << "[UI_REFRESH_DEBUG] --- CRITICAL --- In loadTableMetadata, query execution FAILED for tableId" << m_tableId << ". Error:" << query.lastError().text();
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
                qWarning() << "[UI_REFRESH_DEBUG] --- CRITICAL --- In loadTableMetadata, FAILED to find tableId" << m_tableId << "in both old and new tables.";
                return false;
            }

            QString tableName = query.value(0).toString();
            qWarning() << "VectorTableModel::loadTableMetadata - 使用旧表结构:" << tableName;
            qDebug() << "[UI_REFRESH_DEBUG] --- CRITICAL --- In loadTableMetadata, using old table structure for tableId" << m_tableId << ", tableName:" << tableName;

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
            foreach (const QString &dir, possibleDirs)
            {
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
                if (QFile::exists(binFileName))
                {
                    m_binaryFilePath = QFileInfo(binFileName).absoluteFilePath();
                    found = true;
                    qDebug() << "VectorTableModel::loadTableMetadata - 使用直接文件名找到二进制文件: " << m_binaryFilePath;
                }
                else
                {
                    // 尝试使用数据库目录作为基准的相对路径
                    QString relativePath = QDir(dbDir).absoluteFilePath(binFileName);
                    if (QFile::exists(relativePath))
                    {
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

            qDebug() << "[UI_REFRESH_DEBUG] --- CRITICAL --- In loadTableMetadata, for tableId" << m_tableId << ", read row_count from DB. Value:" << m_rowCount;
            qDebug() << "[UI_REFRESH_DEBUG] Binary file name:" << binFileName << ", schema version:" << m_schemaVersion;

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
            foreach (const QString &dir, possibleDirs)
            {
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
                if (QFile::exists(binFileName))
                {
                    m_binaryFilePath = QFileInfo(binFileName).absoluteFilePath();
                    found = true;
                    qDebug() << "VectorTableModel::loadTableMetadata - 使用直接文件名找到二进制文件: " << m_binaryFilePath;
                }
                else
                {
                    // 尝试使用数据库目录作为基准的相对路径
                    QString relativePath = QDir(dbDir).absoluteFilePath(binFileName);
                    if (QFile::exists(relativePath))
                    {
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

    // 尝试获取主数据库连接
    bool VectorTableModel::ensureDatabaseConnection()
    {
        // 直接使用DatabaseManager提供的主连接
        // 不再创建新连接，避免事务隔离问题
        if (DatabaseManager::instance()->isDatabaseConnected())
            {
                return true;
            }
        else
        {
            qWarning() << "VectorTableModel::ensureDatabaseConnection - 主数据库连接未建立";
            return false;
        }
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

        // 不再关闭数据库连接，因为使用的是主连接
        // 主连接由 DatabaseManager 负责管理

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

    Qt::ItemFlags VectorTableModel::flags(const QModelIndex &index) const
    {
        if (!index.isValid() || m_tableId < 0 || m_columns.isEmpty())
        {
            return Qt::NoItemFlags;
        }

        // 基础标志：可启用、可选择
        Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;

        int column = index.column();
        if (column < 0 || column >= m_columnCount)
        {
            return flags; // 返回基础标志
        }

        // 根据列类型确定是否可编辑
        if (column < m_columns.size())
        {
            const ColumnInfo &colInfo = m_columns.at(column);

            // 特殊列处理：（这些列通常需要特殊控件或特殊逻辑）
            // 对于测试阶段，我们先让大多数类型都可编辑
            switch (colInfo.type)
            {
            case ColumnDataType::TEXT:           // 普通文本允许编辑
            case ColumnDataType::PIN_STATE_ID:   // 管脚状态允许编辑
            case ColumnDataType::INSTRUCTION_ID: // 指令ID允许编辑
            case ColumnDataType::TIMESET_ID:     // TimeSet ID允许编辑
            case ColumnDataType::INTEGER:        // 整数允许编辑
            case ColumnDataType::REAL:           // 实数允许编辑
            case ColumnDataType::BOOLEAN:        // 布尔值允许编辑
                flags |= Qt::ItemIsEditable;
                break;

            case ColumnDataType::JSON_PROPERTIES: // JSON属性暂不支持直接编辑
            default:
                // 保持为不可编辑
                break;
            }
        }

        return flags;
    }

    bool VectorTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
    {
        // 只处理编辑角色
        if (role != Qt::EditRole)
        {
            return false;
        }

        // 验证索引有效性
        if (!index.isValid() || m_tableId < 0)
        {
            qWarning() << "VectorTableModel::setData - 索引无效或表ID无效";
            return false;
        }

        int row = index.row();
        int column = index.column();

        // 范围检查
        if (row < 0 || row >= m_rowCount || column < 0 || column >= m_columnCount)
        {
            qWarning() << "VectorTableModel::setData - 行或列超出范围，行: " << row
                       << ", 列: " << column << ", 最大行: " << m_rowCount
                       << ", 最大列: " << m_columnCount;
            return false;
        }

        // 验证数据有效性
        if (!validateCellData(column, value))
        {
            qWarning() << "VectorTableModel::setData - 数据验证失败，列: " << column
                       << ", 值: " << value;
            return false;
        }

        // 检查值是否真的发生了变化
        QVariant oldValue = data(index, Qt::DisplayRole);
        if (oldValue == value)
        {
            // 值未变化，不需要更新
            return true;
        }

        QElapsedTimer timer;
        timer.start();

        // 尝试更新数据到后端存储
        bool success = VectorDataHandler::instance().updateCellData(m_tableId, row, column, value);

        if (success)
        {
            // 更新成功，更新本地缓存
            updateRowCache(row, column, value);

            // 发出数据变更信号，通知视图更新UI
            emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});

            qDebug() << "VectorTableModel::setData - 成功更新数据，表ID: " << m_tableId
                     << ", 行: " << row << ", 列: " << column << ", 耗时: " << timer.elapsed() << "毫秒";
            return true;
        }
        else
        {
            qWarning() << "VectorTableModel::setData - 更新数据失败，表ID: " << m_tableId
                       << ", 行: " << row << ", 列: " << column;
            return false;
        }
    }

    bool VectorTableModel::validateCellData(int column, const QVariant &value) const
    {
        // 确保列索引有效
        if (column < 0 || column >= m_columns.size())
        {
            qWarning() << "VectorTableModel::validateCellData - 列索引无效: " << column;
            return false;
        }

        // 获取列信息
        const ColumnInfo &colInfo = m_columns.at(column);

        // 根据列类型进行验证
        switch (colInfo.type)
        {
        case ColumnDataType::TEXT:
            // 文本类型：几乎任何内容都是有效的
            return true;

        case ColumnDataType::INTEGER:
        {
            // 整数类型：确保可以转换为整数
            bool ok;
            value.toInt(&ok);
            return ok;
        }

        case ColumnDataType::REAL:
        {
            // 实数类型：确保可以转换为浮点数
            bool ok;
            value.toDouble(&ok);
            return ok;
        }

        case ColumnDataType::INSTRUCTION_ID:
        {
            // 指令ID：确保是有效的整数或预设值
            bool ok;
            int instructionId = value.toInt(&ok);
            return ok && instructionId >= 0; // 假设指令ID必须是非负整数
        }

        case ColumnDataType::TIMESET_ID:
        {
            // TimeSet ID：确保是有效的整数或预设值
            bool ok;
            int timesetId = value.toInt(&ok);
            return ok && timesetId >= 0; // 假设TimeSet ID必须是非负整数
        }

        case ColumnDataType::PIN_STATE_ID:
        {
            // 管脚状态：确保是有效的状态值 (常见值为 "0", "1", "L", "H", "X", "Z" 等)
            QString pinState = value.toString().trimmed().toUpper();
            return pinState.isEmpty() ||                 // 允许空值
                   pinState == "0" || pinState == "1" || // 数字状态
                   pinState == "L" || pinState == "H" || // 低/高状态
                   pinState == "X" || pinState == "Z" || // 不定/高阻状态
                   pinState == "N" || pinState == "P";   // 可能的其他有效状态
        }

        case ColumnDataType::BOOLEAN:
        {
            // 布尔值：确保可以转换为布尔值
            if (value.type() == QVariant::Bool)
                return true;

            QString str = value.toString().trimmed().toLower();
            return str == "true" || str == "false" || str == "1" || str == "0" ||
                   str == "yes" || str == "no" || str == "y" || str == "n";
        }

        case ColumnDataType::JSON_PROPERTIES:
            // JSON属性：暂不支持通过常规方式编辑
            return false;

        default:
            qWarning() << "VectorTableModel::validateCellData - 未知的列类型: " << static_cast<int>(colInfo.type);
            return false;
        }
    }

    void VectorTableModel::updateRowCache(int rowIndex, int colIndex, const QVariant &value)
    {
        // 检查缓存中是否已有该行数据
        if (m_rowCache.contains(rowIndex))
        {
            // 确保列索引有效
            if (colIndex < m_rowCache[rowIndex].size())
            {
                // 更新缓存中的值
                m_rowCache[rowIndex][colIndex] = value;
                qDebug() << "VectorTableModel::updateRowCache - 更新缓存，行: " << rowIndex
                         << ", 列: " << colIndex << ", 新值: " << value;
            }
            else
            {
                qWarning() << "VectorTableModel::updateRowCache - 列索引超出缓存数据范围，行: "
                           << rowIndex << ", 列: " << colIndex << ", 缓存列数: "
                           << m_rowCache[rowIndex].size();
            }
        }
        else
        {
            // 行不在缓存中，不进行任何操作
            // 下次访问时会从文件中重新加载，自然包含更新后的数据
            qDebug() << "VectorTableModel::updateRowCache - 行不在缓存中，行: " << rowIndex;
        }
    }

    bool VectorTableModel::loadPage(int page)
    {
        qDebug() << "[UI_REFRESH_DEBUG] Entering VectorTableModel::loadPage with page:" << page << ", tableId:" << m_tableId;
        
        if (m_tableId == -1)
        {
            qDebug() << "[UI_REFRESH_DEBUG] m_tableId is -1, aborting loadPage.";
            return false;
        }
            
        // **关键修复：在重新加载页面数据前，获取最新的数据库句柄实例**
        QSqlDatabase db = DatabaseManager::instance()->database();
        m_dbConnectionName = db.connectionName(); // 更新连接名
        qDebug() << "[UI_REFRESH_DEBUG] Using database connection:" << m_dbConnectionName;

        beginResetModel();
        qDebug() << "[UI_REFRESH_DEBUG] Called beginResetModel(), clearing row cache";
        m_rowCache.clear();

        // 这是一个简化的实现，实际加载逻辑在 fetchMore 中
        // 这里我们只需要确保模型状态被重置，以便fetchMore能正确工作

        endResetModel();
        qDebug() << "[UI_REFRESH_DEBUG] Called endResetModel()";

        // fetchMore 会根据当前rowCount决定是否加载新数据
        // 我们通过重置模型来触发它
        qDebug() << "[UI_REFRESH_DEBUG] About to check canFetchMore(). Current rowCount:" << m_rowCount;
        if (canFetchMore(QModelIndex()))
        {
            qDebug() << "[UI_REFRESH_DEBUG] canFetchMore() returned true, calling fetchMore()";
            fetchMore(QModelIndex());
            qDebug() << "[UI_REFRESH_DEBUG] After fetchMore(), new rowCount:" << m_rowCount;
        }
        else
        {
            qDebug() << "[UI_REFRESH_DEBUG] canFetchMore() returned false, no data to fetch";
        }
        
        qDebug() << "[UI_REFRESH_DEBUG] Exiting loadPage() with rowCount:" << m_rowCount;
        return true;
    }

} // namespace Vector