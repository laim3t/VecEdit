
    // 行偏移缓存相关方法实现
    void BinaryFileHelper::clearAllRowOffsetCaches()
    {
        const QString funcName = "BinaryFileHelper::clearAllRowOffsetCaches";
        QMutexLocker locker(&s_cacheMutex);

        qDebug() << funcName << "- 清除所有文件的行偏移缓存，共" << s_fileRowOffsetCache.size() << "个文件";
        s_fileRowOffsetCache.clear();
    }

    BinaryFileHelper::RowOffsetCache BinaryFileHelper::getRowOffsetCache(const QString &binFilePath, const QDateTime &fileLastModified)
    {
        const QString funcName = "BinaryFileHelper::getRowOffsetCache";
        QMutexLocker locker(&s_cacheMutex);

        // 检查内存缓存是否存在
        if (!s_fileRowOffsetCache.contains(binFilePath))
        {
            // 尝试从索引文件读取
            RowOffsetCache indexCache = loadRowOffsetIndex(binFilePath, fileLastModified);
            if (!indexCache.isEmpty())
            {
                // 如果从索引文件读取成功，将其加入内存缓存
                qDebug() << funcName << "- 从索引文件加载了行偏移数据，共" << indexCache.size() << "行";
                s_fileRowOffsetCache[binFilePath] = indexCache;
                return indexCache;
            }

            return RowOffsetCache();
        }

        // 获取缓存
        const RowOffsetCache &cache = s_fileRowOffsetCache.value(binFilePath);

        // 检查缓存是否为空
        if (cache.isEmpty())
        {
            return RowOffsetCache();
        }

        // 检查文件修改时间是否与缓存创建时间一致
        // 我们使用第一个非零时间戳元素作为整个缓存的创建时间
        quint64 cacheTimestamp = 0;
        for (const auto &entry : cache)
        {
            if (entry.timestamp > 0)
            {
                cacheTimestamp = entry.timestamp;
                break;
            }
        }

        // 如果所有行都是timestamp=0，说明缓存完全失效
        if (cacheTimestamp == 0)
        {
            qDebug() << funcName << "- 缓存中所有行都已标记为过期:" << binFilePath;
            return RowOffsetCache();
        }

        quint64 fileTimestamp = fileLastModified.toSecsSinceEpoch();

        // 如果文件比缓存新，缓存无效
        if (fileTimestamp > cacheTimestamp)
        {
            qDebug() << funcName << "- 文件已更新，缓存已过期:" << binFilePath
                     << "文件时间:" << fileTimestamp << "缓存时间:" << cacheTimestamp;
            return RowOffsetCache();
        }

        // 检查是否有部分行的缓存已过期（timestamp=0）
        bool hasInvalidatedRows = false;
        int invalidCount = 0;
        for (const auto &entry : cache)
        {
            if (entry.timestamp == 0)
            {
                hasInvalidatedRows = true;
                invalidCount++;
            }
        }

        if (hasInvalidatedRows)
        {
            qDebug() << funcName << "- 缓存中有" << invalidCount << "行已标记为过期，需要重新扫描这些行";
            // 返回原始缓存，稍后在扫描过程中会处理这些过期行
        }

        qDebug() << funcName << "- 使用内存缓存的行偏移数据，共" << cache.size() << "行:" << binFilePath;
        return cache;
    }

    void BinaryFileHelper::setRowOffsetCache(const QString &binFilePath, const RowOffsetCache &rowPositions)
    {
        const QString funcName = "BinaryFileHelper::setRowOffsetCache";
        QMutexLocker locker(&s_cacheMutex);

        // 简单的缓存大小限制，防止内存无限增长
        const int MAX_CACHED_FILES = 50;

        // 如果缓存达到上限，移除最旧的一个
        if (s_fileRowOffsetCache.size() >= MAX_CACHED_FILES && !s_fileRowOffsetCache.contains(binFilePath))
        {
            // 找到最旧的缓存项并移除
            QString oldestFile;
            quint64 oldestTimestamp = UINT64_MAX;

            for (auto it = s_fileRowOffsetCache.begin(); it != s_fileRowOffsetCache.end(); ++it)
            {
                if (!it.value().isEmpty() && it.value().first().timestamp < oldestTimestamp)
                {
                    oldestTimestamp = it.value().first().timestamp;
                    oldestFile = it.key();
                }
            }

            if (!oldestFile.isEmpty())
            {
                qDebug() << funcName << "- 缓存已满，移除最旧的缓存项:" << oldestFile;
                s_fileRowOffsetCache.remove(oldestFile);
            }
        }

        // 存储新的缓存
        qDebug() << funcName << "- 添加行偏移缓存，文件:" << binFilePath << "，行数:" << rowPositions.size();
        s_fileRowOffsetCache[binFilePath] = rowPositions;

        // 同时保存到索引文件
        bool indexSaved = saveRowOffsetIndex(binFilePath, rowPositions);
        qDebug() << funcName << "- 索引文件保存" << (indexSaved ? "成功" : "失败");
    }

    QString BinaryFileHelper::getIndexFilePath(const QString &binFilePath)
    {
        return binFilePath + ".index";
    }

    bool BinaryFileHelper::saveRowOffsetIndex(const QString &binFilePath, const RowOffsetCache &rowPositions)
    {
        const QString funcName = "BinaryFileHelper::saveRowOffsetIndex";

        if (rowPositions.isEmpty())
        {
            qDebug() << funcName << "- 空缓存，不保存索引文件";
            return false;
        }

        QString indexFilePath = getIndexFilePath(binFilePath);
        QFile indexFile(indexFilePath);

        if (!indexFile.open(QIODevice::WriteOnly))
        {
            qWarning() << funcName << "- 无法创建索引文件:" << indexFilePath;
            return false;
        }

        QElapsedTimer timer;
        timer.start();

        // 优化：使用缓冲区提高写入性能
        const int BUFFER_SIZE = 1024 * 1024; // 1MB 缓冲区
        QByteArray buffer;
        buffer.reserve(BUFFER_SIZE);

        QDataStream out(&buffer, QIODevice::WriteOnly);
        out.setByteOrder(QDataStream::LittleEndian);

        // 写入魔数和版本
        out << quint32(INDEX_FILE_MAGIC);
        out << quint32(INDEX_FILE_VERSION);

        // 写入数据文件的最后修改时间
        QFileInfo dataFileInfo(binFilePath);
        out << quint64(dataFileInfo.lastModified().toSecsSinceEpoch());

        // 写入行数
        out << quint32(rowPositions.size());

        // 估算每行索引数据的大小
        const int ESTIMATED_BYTES_PER_ROW = 16; // 估计每行需要16字节 (offset 8字节 + dataSize 4字节 + 对齐/预留)
        const int ROWS_PER_FLUSH = BUFFER_SIZE / ESTIMATED_BYTES_PER_ROW;

        // 分批写入行偏移数据
        int rowsWritten = 0;
        int batchCount = 0;

        for (const auto &pos : rowPositions)
        {
            out << pos.offset;
            out << pos.dataSize;
            // 不写入timestamp，节省空间

            rowsWritten++;

            // 当缓冲区接近满时，刷新到文件
            if (rowsWritten % ROWS_PER_FLUSH == 0 || rowsWritten == rowPositions.size())
            {
                indexFile.write(buffer);
                buffer.clear();
                out.device()->seek(0); // 重置缓冲区流位置
                batchCount++;
            }
        }

        // 确保最后的数据被写入
        if (!buffer.isEmpty())
        {
            indexFile.write(buffer);
            batchCount++;
        }

        indexFile.close();

        if (out.status() != QDataStream::Ok)
        {
            qWarning() << funcName << "- 写入索引文件时出错:" << out.status();
            QFile::remove(indexFilePath);
            return false;
        }

        qDebug() << funcName << "- 成功创建索引文件:" << indexFilePath
                 << "，包含" << rowPositions.size() << "行索引"
                 << "，共" << batchCount << "批次，耗时:" << timer.elapsed() << "ms";
        return true;
    }

    BinaryFileHelper::RowOffsetCache BinaryFileHelper::loadRowOffsetIndex(const QString &binFilePath, const QDateTime &fileLastModified)
    {
        const QString funcName = "BinaryFileHelper::loadRowOffsetIndex";
        RowOffsetCache cache;

        QString indexFilePath = getIndexFilePath(binFilePath);
        QFile indexFile(indexFilePath);

        if (!indexFile.exists())
        {
            qDebug() << funcName << "- 索引文件不存在:" << indexFilePath;
            return cache;
        }

        if (!indexFile.open(QIODevice::ReadOnly))
        {
            qWarning() << funcName << "- 无法打开索引文件:" << indexFilePath;
            return cache;
        }

        QElapsedTimer timer;
        timer.start();

        // 获取文件大小
        qint64 fileSize = indexFile.size();
        if (fileSize < 20) // 至少需要头部信息
        {
            qWarning() << funcName << "- 索引文件太小，可能已损坏:" << indexFilePath;
            return cache;
        }

        // 优化：对大文件使用内存映射方式读取
        const qint64 MMAP_THRESHOLD = 10 * 1024 * 1024; // 10MB以上使用内存映射
        bool useMMap = (fileSize > MMAP_THRESHOLD);

        if (useMMap)
        {
            qDebug() << funcName << "- 使用内存映射方式读取大索引文件:" << fileSize / (1024 * 1024) << "MB";

            uchar *mappedData = indexFile.map(0, fileSize);
            if (!mappedData)
            {
                qWarning() << funcName << "- 内存映射失败，回退到常规读取";
                useMMap = false;
            }
            else
            {
                // 使用内存映射数据创建数据流
                QByteArray mappedByteArray = QByteArray::fromRawData(reinterpret_cast<char *>(mappedData), fileSize);
                QDataStream in(mappedByteArray);
                in.setByteOrder(QDataStream::LittleEndian);

                // 读取并验证魔数和版本
                quint32 magic, version;
                in >> magic;
                in >> version;

                if (magic != INDEX_FILE_MAGIC || version > INDEX_FILE_VERSION)
                {
                    qWarning() << funcName << "- 索引文件格式不兼容，魔数:" << magic << "，版本:" << version;
                    indexFile.unmap(mappedData);
                    return cache;
                }

                // 读取数据文件的最后修改时间并验证
                quint64 indexedFileTimestamp;
                in >> indexedFileTimestamp;

                quint64 actualFileTimestamp = fileLastModified.toSecsSinceEpoch();
                if (indexedFileTimestamp != actualFileTimestamp)
                {
                    qDebug() << funcName << "- 数据文件已被修改，索引文件已过期:"
                             << "索引时间:" << indexedFileTimestamp
                             << "，文件时间:" << actualFileTimestamp;
                    qDebug() << funcName << "- 尝试保留索引文件结构，后续对修改行单独处理";
                }

                // 读取行数
                quint32 rowCount;
                in >> rowCount;

                if (rowCount == 0 || rowCount > 50000000) // 设置合理的上限，防止损坏的索引文件
                {
                    qWarning() << funcName << "- 索引文件中的行数不合理:" << rowCount;
                    indexFile.unmap(mappedData);
                    return cache;
                }

                // 预分配空间提高性能
                cache.reserve(rowCount);
                quint64 currentTimestamp = QDateTime::currentSecsSinceEpoch();

                // 批量读取数据以提高性能
                for (quint32 i = 0; i < rowCount; i++)
                {
                    RowPositionInfo pos;
                    in >> pos.offset;
                    in >> pos.dataSize;
                    pos.timestamp = currentTimestamp;

                    if (in.status() != QDataStream::Ok)
                    {
                        qWarning() << funcName << "- 读取行" << i << "的索引数据时出错";
                        cache.clear();
                        indexFile.unmap(mappedData);
                        return cache;
                    }

                    cache.append(pos);
                }

                indexFile.unmap(mappedData);
                qDebug() << funcName << "- 成功从索引文件加载" << cache.size() << "行的偏移数据，耗时:"
                         << timer.elapsed() << "ms";
                return cache;
            }
        }

        // 如果内存映射失败或文件较小，使用传统方式读取
        if (!useMMap)
        {
            QDataStream in(&indexFile);
            in.setByteOrder(QDataStream::LittleEndian);

            // 读取并验证魔数和版本
            quint32 magic, version;
            in >> magic;
            in >> version;

            if (magic != INDEX_FILE_MAGIC || version > INDEX_FILE_VERSION)
            {
                qWarning() << funcName << "- 索引文件格式不兼容，魔数:" << magic << "，版本:" << version;
                return cache;
            }

            // 读取数据文件的最后修改时间并验证
            quint64 indexedFileTimestamp;
            in >> indexedFileTimestamp;

            quint64 actualFileTimestamp = fileLastModified.toSecsSinceEpoch();
            if (indexedFileTimestamp != actualFileTimestamp)
            {
                qDebug() << funcName << "- 数据文件已被修改，索引文件已过期:"
                         << "索引时间:" << indexedFileTimestamp
                         << "，文件时间:" << actualFileTimestamp;
                qDebug() << funcName << "- 尝试保留索引文件结构，后续对修改行单独处理";
            }

            // 读取行数
            quint32 rowCount;
            in >> rowCount;

            if (rowCount == 0 || rowCount > 50000000) // 设置合理的上限，防止损坏的索引文件
            {
                qWarning() << funcName << "- 索引文件中的行数不合理:" << rowCount;
                return cache;
            }

            // 预分配空间提高性能
            cache.reserve(rowCount);

            // 使用缓冲区批量读取以提高性能
            const int BATCH_SIZE = 10000;
            quint64 currentTimestamp = QDateTime::currentSecsSinceEpoch();

            // 批量读取数据以提高性能
            for (quint32 i = 0; i < rowCount; i += BATCH_SIZE)
            {
                // 计算当前批次实际大小
                quint32 batchEnd = qMin(i + BATCH_SIZE, rowCount);
                quint32 currentBatchSize = batchEnd - i;

                for (quint32 j = 0; j < currentBatchSize; j++)
                {
                    RowPositionInfo pos;
                    in >> pos.offset;
                    in >> pos.dataSize;
                    pos.timestamp = currentTimestamp;

                    if (in.status() != QDataStream::Ok)
                    {
                        qWarning() << funcName << "- 读取行" << (i + j) << "的索引数据时出错";
                        cache.clear();
                        return cache;
                    }

                    cache.append(pos);
                }
            }
        }

        qDebug() << funcName << "- 成功从索引文件加载" << cache.size() << "行的偏移数据，耗时:"
                 << timer.elapsed() << "ms";

        return cache;
    }