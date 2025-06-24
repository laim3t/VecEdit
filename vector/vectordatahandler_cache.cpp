void VectorDataHandler::initializeCache()
{
    if (m_cacheInitialized)
    {
        qDebug() << "VectorDataHandler::initializeCache - 缓存已经初始化，跳过";
        return;
    }

    qDebug() << "VectorDataHandler::initializeCache - 开始初始化缓存";

    // 加载指令和TimeSet缓存
    loadInstructionCache();
    loadTimesetCache();

    m_cacheInitialized = true;
    qDebug() << "VectorDataHandler::initializeCache - 缓存初始化完成";
}

void VectorDataHandler::loadInstructionCache()
{
    qDebug() << "VectorDataHandler::loadInstructionCache - 开始加载指令缓存";
    m_instructionCache.clear();

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << "VectorDataHandler::loadInstructionCache - 数据库未打开";
        return;
    }

    // 一次性获取所有指令
    QSqlQuery allInstructionsQuery(db);
    if (allInstructionsQuery.exec("SELECT id, instruction_value FROM instruction_options"))
    {
        int count = 0;
        while (allInstructionsQuery.next())
        {
            int id = allInstructionsQuery.value(0).toInt();
            QString value = allInstructionsQuery.value(1).toString();
            m_instructionCache[id] = value;
            count++;
        }
        qDebug() << "VectorDataHandler::loadInstructionCache - 已加载" << count << "个指令到缓存";
    }
    else
    {
        qWarning() << "VectorDataHandler::loadInstructionCache - 查询失败:" << allInstructionsQuery.lastError().text();
    }
}

void VectorDataHandler::loadTimesetCache()
{
    qDebug() << "VectorDataHandler::loadTimesetCache - 开始加载TimeSet缓存";
    m_timesetCache.clear();

    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << "VectorDataHandler::loadTimesetCache - 数据库未打开";
        return;
    }

    // 一次性获取所有TimeSet
    QSqlQuery allTimesetsQuery(db);
    if (allTimesetsQuery.exec("SELECT id, timeset_name FROM timeset_list"))
    {
        int count = 0;
        while (allTimesetsQuery.next())
        {
            int id = allTimesetsQuery.value(0).toInt();
            QString name = allTimesetsQuery.value(1).toString();
            m_timesetCache[id] = name;
            count++;
        }
        qDebug() << "VectorDataHandler::loadTimesetCache - 已加载" << count << "个TimeSet到缓存";
    }
    else
    {
        qWarning() << "VectorDataHandler::loadTimesetCache - 查询失败:" << allTimesetsQuery.lastError().text();
    }
}

void VectorDataHandler::clearCache()
{
    qDebug() << "VectorDataHandler::clearCache - 清除所有缓存数据";
    m_instructionCache.clear();
    m_timesetCache.clear();
    m_cacheInitialized = false;
}

void VectorDataHandler::clearTableDataCache(int tableId)
{
    const QString funcName = "VectorDataHandler::clearTableDataCache";

    if (m_tableDataCache.contains(tableId))
    {
        qDebug() << funcName << " - 清除表" << tableId << "的数据缓存";
        m_tableDataCache.remove(tableId);
        m_tableCacheTimestamp.remove(tableId);
        m_tableBinaryFilePath.remove(tableId);
        m_tableBinaryFileMD5.remove(tableId);
    }
}

void VectorDataHandler::clearAllTableDataCache()
{
    const QString funcName = "VectorDataHandler::clearAllTableDataCache";
    qDebug() << funcName << " - 清除所有表数据缓存";

    m_tableDataCache.clear();
    m_tableCacheTimestamp.clear();
    m_tableBinaryFilePath.clear();
    m_tableBinaryFileMD5.clear();
}

// 检查表数据缓存是否有效
bool VectorDataHandler::isTableDataCacheValid(int tableId, const QString &binFilePath)
{
    const QString funcName = "VectorDataHandler::isTableDataCacheValid";

    // 检查是否有缓存
    if (!m_tableDataCache.contains(tableId))
    {
        qDebug() << funcName << " - 表" << tableId << "没有缓存数据";
        return false;
    }

    // 检查文件路径是否匹配
    if (m_tableBinaryFilePath.value(tableId) != binFilePath)
    {
        qDebug() << funcName << " - 表" << tableId << "的文件路径已变更";
        return false;
    }

    // 检查文件是否存在
    QFile file(binFilePath);
    if (!file.exists())
    {
        qDebug() << funcName << " - 表" << tableId << "的二进制文件不存在";
        return false;
    }

    // 检查文件修改时间是否晚于缓存时间
    QFileInfo fileInfo(binFilePath);
    QDateTime fileModTime = fileInfo.lastModified();
    QDateTime cacheTime = m_tableCacheTimestamp.value(tableId);

    if (cacheTime.isValid() && fileModTime > cacheTime)
    {
        qDebug() << funcName << " - 表" << tableId << "的文件已被修改，缓存已过期";
        return false;
    }

    // 可选：检查MD5校验和
    if (m_tableBinaryFileMD5.contains(tableId))
    {
        if (file.open(QIODevice::ReadOnly))
        {
            QCryptographicHash hash(QCryptographicHash::Md5);
            if (hash.addData(&file))
            {
                QByteArray fileHash = hash.result();
                if (fileHash != m_tableBinaryFileMD5.value(tableId))
                {
                    qDebug() << funcName << " - 表" << tableId << "的文件内容已变更，MD5不匹配";
                    file.close();
                    return false;
                }
            }
            file.close();
        }
    }

    qDebug() << funcName << " - 表" << tableId << "的缓存有效";
    return true;
}

// 更新表数据缓存
void VectorDataHandler::updateTableDataCache(int tableId, const QList<Vector::RowData> &rows, const QString &binFilePath)
{
    const QString funcName = "VectorDataHandler::updateTableDataCache";

    // 更新缓存数据
    m_tableDataCache[tableId] = rows;

    // 更新时间戳
    m_tableCacheTimestamp[tableId] = QDateTime::currentDateTime();

    // 更新文件路径
    m_tableBinaryFilePath[tableId] = binFilePath;

    // 可选：更新MD5校验和
    QFile file(binFilePath);
    if (file.open(QIODevice::ReadOnly))
    {
        QCryptographicHash hash(QCryptographicHash::Md5);
        if (hash.addData(&file))
        {
            m_tableBinaryFileMD5[tableId] = hash.result();
        }
        file.close();
    }

    qDebug() << funcName << " - 已更新表" << tableId << "的缓存，" << rows.size() << "行数据";
}

// 清除指定表的所有修改标记
void VectorDataHandler::clearModifiedRows(int tableId)
{
    const QString funcName = "VectorDataHandler::clearModifiedRows";

    int clearedCount = 0;
    if (m_modifiedRows.contains(tableId))
    {
        clearedCount = m_modifiedRows[tableId].size();
        m_modifiedRows[tableId].clear();
    }

    qDebug() << funcName << " - 已清除表ID:" << tableId << "的所有修改标记，共" << clearedCount << "行";
}