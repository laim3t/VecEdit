// ==========================================================
//  导出功能辅助实现: mainwindow_export.cpp
// ==========================================================

void MainWindow::exportConstructionFile()
{
    // 检查当前是否有打开的项目
    if (!DatabaseManager::instance()->isDatabaseConnected())
    {
        QMessageBox::warning(this, tr("导出失败"), tr("没有打开的项目，请先打开或创建一个项目。"));
        return;
    }

    // 定义格式化函数，用于处理空值
    auto formatValue = [](const QString &val)
    { return val.isEmpty() ? "" : val; };

    // 获取保存文件的路径
    QString saveFilePath = QFileDialog::getSaveFileName(
        this,
        tr("导出构造文件"),
        QDir::homePath(),
        tr("文本文件 (*.txt);;所有文件 (*.*)"));

    // 如果用户取消了选择，直接返回
    if (saveFilePath.isEmpty())
    {
        return;
    }

    // 确保文件名有正确的扩展名
    if (!saveFilePath.endsWith(".txt", Qt::CaseInsensitive))
    {
        saveFilePath += ".txt";
    }

    QFile file(saveFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::critical(this, tr("导出失败"), tr("无法创建文件：%1").arg(saveFilePath));
        return;
    }

    QTextStream out(&file);
    out.setCodec("UTF-8");
    out.setGenerateByteOrderMark(false); // 不生成BOM标记，导出纯UTF-8格式

    // 明确指定数据库连接，防止 "database not open" 错误
    QSqlDatabase db = DatabaseManager::instance()->database();

    // 第一部分 - 版本信息
    out << "@@SPECIFICATIONS_DEFINE\n";
    out << "Section.Variable_2;1;//\n";
    out << "@@END_SPECIFICATIONS_DEFINE\n";

    // 第二部分 - TimeSet信息
    out << "@@TIMESET_DEFINE\n";

    QSqlQuery timesetListQuery(db);
    timesetListQuery.prepare("SELECT id, timeset_name, period FROM timeset_list");

    if (timesetListQuery.exec())
    {
        while (timesetListQuery.next())
        {
            int timesetId = timesetListQuery.value(0).toInt();
            QString timesetName = timesetListQuery.value(1).toString();
            double period = timesetListQuery.value(2).toDouble();

            QSqlQuery settingsQuery(db);
            settingsQuery.prepare(
                "SELECT "
                "   p.pin_name, "
                "   ts.T1R, "
                "   ts.T1F, "
                "   ts.STBR, "
                "   wo.wave_type "
                "FROM timeset_settings ts "
                "LEFT JOIN pin_list p ON ts.pin_id = p.id "
                "LEFT JOIN wave_options wo ON ts.wave_id = wo.id "
                "WHERE ts.timeset_id = ?");
            settingsQuery.addBindValue(timesetId);

            if (settingsQuery.exec())
            {
                QStringList pins;
                QStringList t1rs, t1fs, stbrs, waves;

                while (settingsQuery.next())
                {
                    pins.append(settingsQuery.value(0).toString());
                    t1rs.append(settingsQuery.value(1).toString());
                    t1fs.append(settingsQuery.value(2).toString());
                    stbrs.append(settingsQuery.value(3).toString());
                    waves.append(settingsQuery.value(4).toString());
                }

                QString pinStr = pins.join(":");
                QString t1rStr = t1rs.isEmpty() ? "" : t1rs.first();
                QString t1fStr = t1fs.isEmpty() ? "" : t1fs.first();
                QString stbrStr = stbrs.isEmpty() ? "" : stbrs.first();
                QString waveStr = waves.isEmpty() ? "" : waves.first();

                // 构建输出行 - 只包含注释字段，没有最后的分号和双斜杠
                out << formatValue(timesetName) << ";"
                    << period << ";"
                    << formatValue(pinStr) << ";"
                    << formatValue(t1rStr) << ";"
                    << formatValue(t1fStr) << ";"
                    << formatValue(stbrStr) << ";"
                    << formatValue(waveStr) << ";//\n"; // 保留注释字段作为"//"，但不添加额外的分号和双斜杠
            }
            else
            {
                qWarning() << "Failed to query timeset_settings for timesetId" << timesetId << ":" << settingsQuery.lastError().text();
            }
        }
    }
    else
    {
        qWarning() << "Failed to query timeset_list:" << timesetListQuery.lastError().text();
    }

    out << "@@END_TIMESET_DEFINE\n";

    // 第三部分 - Label信息
    out << "@@Label_DEFINE\n";

    // 获取所有向量表
    QSqlQuery tablesQuery(db);
    tablesQuery.prepare("SELECT id, table_name FROM vector_tables");

    if (tablesQuery.exec())
    {
        while (tablesQuery.next())
        {
            int tableId = tablesQuery.value(0).toInt();
            QString tableName = tablesQuery.value(1).toString();

            // 获取表的列配置，找出Label列的位置
            QList<Vector::ColumnInfo> columns = getCurrentColumnConfiguration(tableId);
            int labelColumnIndex = -1;

            for (int i = 0; i < columns.size(); i++)
            {
                if (columns[i].name.toLower() == "label")
                {
                    labelColumnIndex = i;
                    break;
                }
            }

            if (labelColumnIndex == -1)
            {
                qWarning() << "No Label column found for table" << tableId;
                continue;
            }

            // 从二进制文件中读取所有行数据
            QString binFileName;
            int rowCount = 0;
            int schemaVersion = 0;

            if (!loadVectorTableMeta(tableId, binFileName, columns, schemaVersion, rowCount))
            {
                qWarning() << "Failed to load vector table meta for table" << tableId;
                continue;
            }

            // 使用VectorDataHandler获取所有行数据
            bool success = false;
            // 使用正确的单例访问方法
            QList<QList<QVariant>> rows = VectorDataHandler::instance().getAllVectorRows(tableId, success);

            if (!success)
            {
                qWarning() << "Failed to get rows for table" << tableId;
                continue;
            }

            // 遍历所有行，提取Label字段的值
            for (const auto &row : rows)
            {
                if (labelColumnIndex < row.size() && !row[labelColumnIndex].toString().isEmpty())
                {
                    QString labelName = row[labelColumnIndex].toString();

                    // 构建输出行 - 保留注释字段为"//"但不添加额外内容
                    out << formatValue(labelName) << ";"
                        << formatValue(tableName) << ";//\n";
                }
            }
        }
    }
    else
    {
        qWarning() << "Failed to query vector_tables:" << tablesQuery.lastError().text();
    }

    out << "@@END_Label_DEFINE\n";

    // 第四部分 - 向量表信息 (表名、管脚名、管脚类型)
    out << "@@TABLE_DEFINE\n";

    // 获取所有向量表
    QSqlQuery vectorTablesQuery(db);
    vectorTablesQuery.prepare("SELECT id, table_name FROM vector_tables");

    if (vectorTablesQuery.exec())
    {
        while (vectorTablesQuery.next())
        {
            int tableId = vectorTablesQuery.value(0).toInt();
            QString tableName = vectorTablesQuery.value(1).toString();

            // 为每个表获取关联的管脚和类型信息
            QSqlQuery pinsQuery(db);
            pinsQuery.prepare(
                "SELECT vtp.id, pl.pin_name, to1.type_name "
                "FROM vector_table_pins vtp "
                "LEFT JOIN pin_list pl ON vtp.pin_id = pl.id "
                "LEFT JOIN type_options to1 ON vtp.pin_type = to1.id "
                "WHERE vtp.table_id = ? "
                "ORDER BY vtp.id");
            pinsQuery.addBindValue(tableId);

            if (pinsQuery.exec())
            {
                QStringList pinNames;
                QStringList pinTypes;

                while (pinsQuery.next())
                {
                    pinNames.append(pinsQuery.value(1).toString());
                    pinTypes.append(pinsQuery.value(2).toString().toLower()); // 转换为小写以匹配格式
                }

                // 只有当表至少有一个管脚时才输出
                if (!pinNames.isEmpty())
                {
                    out << formatValue(tableName) << ";"
                        << formatValue(pinNames.join(":")) << ";"
                        << formatValue(pinTypes.join(":")) << "\n"; // 不需要额外的 //
                }
            }
            else
            {
                qWarning() << "Failed to query pins for tableId" << tableId << ":" << pinsQuery.lastError().text();
            }
        }
    }
    else
    {
        qWarning() << "Failed to query vector_tables for TABLE section:" << vectorTablesQuery.lastError().text();
    }

    out << "@@END_TABLE_DEFINE\n";

    // 第五部分 - 向量行信息 (按表分组的向量行)
    // 再次获取所有向量表
    QSqlQuery patternTablesQuery(db);
    patternTablesQuery.prepare("SELECT id, table_name FROM vector_tables");

    if (patternTablesQuery.exec())
    {
        // 预先加载Timeset名称映射，用于ID到名称的转换
        QMap<int, QString> timesetNameMap;
        QSqlQuery timesetNameQuery(db);
        if (timesetNameQuery.exec("SELECT id, timeset_name FROM timeset_list"))
        {
            while (timesetNameQuery.next())
            {
                int id = timesetNameQuery.value(0).toInt();
                QString name = timesetNameQuery.value(1).toString();
                timesetNameMap[id] = name;
            }
        }

        // 预先加载指令名称映射，用于ID到名称的转换
        QMap<int, QString> instructionNameMap;
        QSqlQuery instructionNameQuery(db);
        if (instructionNameQuery.exec("SELECT id, instruction_value FROM instruction_options"))
        {
            while (instructionNameQuery.next())
            {
                int id = instructionNameQuery.value(0).toInt();
                QString name = instructionNameQuery.value(1).toString();
                instructionNameMap[id] = name;
            }
        }
        else
        {
            qWarning() << "Failed to query instruction_options:" << instructionNameQuery.lastError().text();
        }

        // 调试信息：打印加载的表格数量
        int totalTables = 0;
        while (patternTablesQuery.next())
            totalTables++;
        qDebug() << "导出：共找到" << totalTables << "个向量表";

        // 重新执行查询，因为上一个循环已经消耗了结果集
        patternTablesQuery.exec("SELECT id, table_name FROM vector_tables");

        while (patternTablesQuery.next())
        {
            int tableId = patternTablesQuery.value(0).toInt();
            QString tableName = patternTablesQuery.value(1).toString();

            qDebug() << "处理向量表: ID=" << tableId << ", 名称=" << tableName;

            // 获取表的列配置
            QList<Vector::ColumnInfo> columns = getCurrentColumnConfiguration(tableId);

            // 确定各个列的索引
            int labelIdx = -1, instructionIdx = -1, timesetIdx = -1,
                captureIdx = -1, extIdx = -1, commentIdx = -1;

            // 管脚列的索引列表 - 新增一个QList来保存所有管脚列的索引
            QList<int> pinColumnIndices;
            QStringList pinNames; // 对应的管脚名称

            for (int i = 0; i < columns.size(); i++)
            {
                QString colName = columns[i].name.toLower();
                if (colName == "label")
                    labelIdx = i;
                else if (colName == "instruction")
                    instructionIdx = i;
                else if (colName == "timeset")
                    timesetIdx = i;
                else if (colName == "capture")
                    captureIdx = i;
                else if (colName == "ext")
                    extIdx = i;
                else if (colName == "comment")
                    commentIdx = i;

                // 检查是否为管脚列 - 通过检查列名和列类型
                // 管脚列通常是PIN_STATE_ID类型，且不是上面识别的特殊列
                if (columns[i].type == Vector::ColumnDataType::PIN_STATE_ID ||
                    (!colName.isEmpty() && !columns[i].data_properties.isEmpty() &&
                     (columns[i].data_properties.contains("pin_id") || columns[i].data_properties.contains("pinId"))))
                {
                    // 这是一个管脚列，添加到索引列表
                    pinColumnIndices.append(i);
                    pinNames.append(columns[i].name); // 使用列名作为管脚名

                    qDebug() << "找到管脚列:" << columns[i].name << "在索引" << i
                             << "类型:" << columns[i].original_type_str
                             << "数据属性:" << columns[i].data_properties;
                }
            }

            // 从二进制文件中读取所有行数据
            QString binFileName;
            int rowCount = 0;
            int schemaVersion = 0;

            if (!loadVectorTableMeta(tableId, binFileName, columns, schemaVersion, rowCount))
            {
                qWarning() << "Failed to load vector table meta for table" << tableId;
                continue;
            }

            // 使用VectorDataHandler获取所有行数据
            bool success = false;
            QList<QList<QVariant>> rows = VectorDataHandler::instance().getAllVectorRows(tableId, success);

            if (!success || rows.isEmpty())
            {
                qWarning() << "Failed to get rows for table" << tableId;
                continue;
            }

            qDebug() << "Table:" << tableName << "读取到" << rows.size() << "行数据";

            // 为表写入头部，确保表名正确地显示
            out << "@@PATTERN_DEFINE " << tableName << "\n";

            // 遍历所有行，按照格式要求输出
            for (const auto &row : rows)
            {
                QString label = (labelIdx >= 0 && labelIdx < row.size()) ? row[labelIdx].toString() : "";

                // 构建管脚值字符串，格式为"值1:值2:值3..."
                QStringList pinValues;
                for (int pinIdx : pinColumnIndices)
                {
                    if (pinIdx < row.size())
                    {
                        pinValues.append(row[pinIdx].toString());
                    }
                    else
                    {
                        pinValues.append(""); // 对于没有值的列，添加空字符串
                    }
                }
                QString pinValueStr = pinValues.join(":");

                // 处理Instruction - 转换ID为名称
                QString instructionStr;
                if (instructionIdx >= 0 && instructionIdx < row.size())
                {
                    int instructionId = row[instructionIdx].toInt();

                    // 如果在映射中找到对应的指令名称，则使用；否则保留原始值
                    if (instructionNameMap.contains(instructionId) && !instructionNameMap[instructionId].isEmpty())
                    {
                        instructionStr = instructionNameMap[instructionId];
                    }
                    else
                    {
                        // 在数据库中直接查找指令名称
                        QSqlQuery instrQuery(db);
                        instrQuery.prepare("SELECT instruction_value FROM instruction_options WHERE id = ?");
                        instrQuery.addBindValue(instructionId);
                        if (instrQuery.exec() && instrQuery.next())
                        {
                            instructionStr = instrQuery.value(0).toString();
                        }
                        else
                        {
                            // 如果找不到指令名称，使用原始值
                            instructionStr = row[instructionIdx].toString();
                        }
                    }
                }

                // 处理Timeset - 转换ID为名称
                QString timesetStr;
                if (timesetIdx >= 0 && timesetIdx < row.size())
                {
                    int timesetId = row[timesetIdx].toInt();
                    timesetStr = timesetNameMap.value(timesetId, "timeset_" + row[timesetIdx].toString());
                }

                QString capture = (captureIdx >= 0 && captureIdx < row.size()) ? row[captureIdx].toString() : "";
                QString ext = (extIdx >= 0 && extIdx < row.size()) ? row[extIdx].toString() : "";
                QString comment = (commentIdx >= 0 && commentIdx < row.size()) ? row[commentIdx].toString() : "";

                // 构建行
                out << formatValue(label) << ";"
                    << formatValue(pinValueStr) << ";"
                    << formatValue(instructionStr) << ";"
                    << formatValue(timesetStr) << ";"
                    << formatValue(capture) << ";"
                    << formatValue(ext) << ";"
                    << formatValue(comment) << "\n";
            }

            // 为表写入尾部
            out << "@@END_PATTERN_DEFINE\n";
        }
    }
    else
    {
        qWarning() << "Failed to query vector_tables for PATTERN section:" << patternTablesQuery.lastError().text();
    }

    // 第六部分 - 管脚信息
    out << "@@PIN_DEFINE\n";

    // 使用SET来确保每个管脚只导出一次
    QSet<int> exportedPinIds;

    // 从pin_settings表获取管脚设置信息
    QSqlQuery pinSettingsQuery(db);
    pinSettingsQuery.prepare(
        "SELECT p.id, p.pin_name, ps.channel_count, ps.station_bit_index, ps.station_number "
        "FROM pin_settings ps "
        "JOIN pin_list p ON ps.pin_id = p.id "
        "ORDER BY p.id");

    if (pinSettingsQuery.exec())
    {
        while (pinSettingsQuery.next())
        {
            int pinId = pinSettingsQuery.value(0).toInt();

            // 如果这个管脚已经导出过，则跳过
            if (exportedPinIds.contains(pinId))
            {
                continue;
            }

            // 标记这个管脚已经导出
            exportedPinIds.insert(pinId);

            QString pinName = pinSettingsQuery.value(1).toString();
            int channelCount = pinSettingsQuery.value(2).toInt();
            int stationBitIndex = pinSettingsQuery.value(3).toInt();
            int stationNumber = pinSettingsQuery.value(4).toInt();

            // 构建工位值字符串 (格式为 "stationBitIndex:stationNumber")
            QString siteValue = QString("%1:%2").arg(stationBitIndex).arg(stationNumber);

            // 输出管脚信息，Comment1和Comment2暂时为"//"
            out << formatValue(pinName) << ";"
                << channelCount << ";"
                << stationBitIndex << ";"
                << formatValue(siteValue) << ";"
                << "//;//\n";
        }
    }
    else
    {
        qWarning() << "Failed to query pin_settings:" << pinSettingsQuery.lastError().text();
    }

    out << "@@END_PIN_DEFINE\n";

    // 第七部分 - 管脚组信息（目前为空占位符）
    out << "@@PINGROUP_DEFINE\n";
    out << "@@END_PINGROUP_DEFINE\n";

    file.close();

    QMessageBox::information(this, tr("导出成功"), tr("构造文件已成功导出到：%1").arg(saveFilePath));
}