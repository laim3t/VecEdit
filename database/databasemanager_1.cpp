

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
