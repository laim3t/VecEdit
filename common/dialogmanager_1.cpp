bool DialogManager::showAddPinsDialog()
{
    // 创建并显示管脚添加对话框
    PinListDialog dialog(m_parent);

    if (dialog.exec() == QDialog::Accepted)
    {
        // 获取用户设置的管脚列表
        QList<QString> pinNames = dialog.getPinNames();

        // 添加到数据库
        if (!pinNames.isEmpty())
        {
            // 获取数据库连接
            QSqlDatabase db = DatabaseManager::instance()->database();

            // 开始事务
            db.transaction();

            // 循环添加每个管脚
            bool success = true;
            for (const QString &pinName : pinNames)
            {
                QSqlQuery query(db);
                query.prepare("INSERT INTO pin_list (pin_name, pin_note, pin_nav_note) VALUES (?, ?, ?)");
                query.addBindValue(pinName);
                query.addBindValue(""); // pin_note为空
                query.addBindValue(""); // pin_nav_note为空

                if (!query.exec())
                {
                    qDebug() << "添加管脚失败:" << query.lastError().text();
                    success = false;
                    break;
                }
            }

            // 提交或回滚事务
            if (success)
            {
                db.commit();
                return true;
            }
            else
            {
                db.rollback();
                return false;
            }
        }
    }

    return false;
}

bool DialogManager::showTimeSetDialog(bool isInitialSetup)
{
    qDebug() << "DialogManager::showTimeSetDialog - 开始显示TimeSet对话框，初始设置模式:" << isInitialSetup;

    // 在非初始设置模式下，检查向量表是否存在
    if (!isInitialSetup)
    {
        // 检查向量表是否存在
        QSqlDatabase db = DatabaseManager::instance()->database();
        QSqlQuery query(db);

        if (query.exec("SELECT COUNT(*) FROM vector_tables"))
        {
            if (query.next())
            {
                int count = query.value(0).toInt();
                qDebug() << "DialogManager::showTimeSetDialog - 检查向量表数量:" << count;

                if (count <= 0)
                {
                    qDebug() << "DialogManager::showTimeSetDialog - 非初始设置模式下未找到向量表，提前终止";
                    QMessageBox::information(m_parent, "提示", "没有找到向量表，请先创建向量表");
                    return false;
                }
            }
        }
        else
        {
            qDebug() << "DialogManager::showTimeSetDialog - 查询向量表失败:" << query.lastError().text();
        }
    }
    else
    {
        qDebug() << "DialogManager::showTimeSetDialog - 初始设置模式，跳过向量表检查";
    }

    // 创建并显示TimeSet对话框
    qDebug() << "DialogManager::showTimeSetDialog - 创建TimeSet对话框实例";
    TimeSetDialog dialog(m_parent, isInitialSetup);

    qDebug() << "DialogManager::showTimeSetDialog - 显示对话框";
    if (dialog.exec() == QDialog::Accepted)
    {
        qDebug() << "DialogManager::showTimeSetDialog - 用户接受了对话框";
        return true;
    }

    qDebug() << "DialogManager::showTimeSetDialog - 用户取消了对话框";
    return false;
}

void DialogManager::showDatabaseViewDialog()
{
    // 创建并显示数据库视图对话框
    DatabaseViewDialog *dialog = new DatabaseViewDialog(m_parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose); // 关闭时自动删除
    dialog->exec();
}

bool DialogManager::showPinGroupDialog()
{
    qDebug() << "DialogManager::showPinGroupDialog - 显示管脚分组对话框";

    // 检查向量表是否存在
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery query(db);

    if (query.exec("SELECT COUNT(*) FROM vector_tables"))
    {
        if (query.next())
        {
            int count = query.value(0).toInt();
            qDebug() << "DialogManager::showPinGroupDialog - 检查向量表数量:" << count;

            if (count <= 0)
            {
                qDebug() << "DialogManager::showPinGroupDialog - 未找到向量表，提前终止";
                QMessageBox::information(m_parent, "提示", "没有找到向量表，请先创建向量表");
                return false;
            }
        }
    }
    else
    {
        qDebug() << "DialogManager::showPinGroupDialog - 查询向量表失败:" << query.lastError().text();
    }

    // 创建并显示管脚分组对话框
    PinGroupDialog dialog(m_parent);

    qDebug() << "DialogManager::showPinGroupDialog - 显示对话框";
    if (dialog.exec() == QDialog::Accepted)
    {
        qDebug() << "DialogManager::showPinGroupDialog - 用户接受了对话框";
        return true;
    }

    qDebug() << "DialogManager::showPinGroupDialog - 用户取消了对话框";
    return false;
}