#include "pinsettingsdialog.h"
#include "../common/tablestylemanager.h"
#include "../database/databasemanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QSqlDatabase>
#include <QToolTip>

PinSettingsDialog::PinSettingsDialog(QWidget *parent)
    : QDialog(parent), m_currentStationCount(1)
{
    qDebug() << "PinSettingsDialog::PinSettingsDialog - 初始化管脚设置对话框";
    setupUI();
    loadPinsData();
    updateTable();
}

PinSettingsDialog::~PinSettingsDialog()
{
    qDebug() << "PinSettingsDialog::~PinSettingsDialog - 销毁管脚设置对话框";
}

void PinSettingsDialog::setupUI()
{
    qDebug() << "PinSettingsDialog::setupUI - 设置对话框UI";

    // 设置对话框属性
    setWindowTitle("管脚设置");
    setMinimumSize(600, 450);

    // 创建主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // 创建顶部布局（工位选择）
    QHBoxLayout *topLayout = new QHBoxLayout();
    QLabel *stationLabel = new QLabel("工位数：", this);
    m_stationCountSpinBox = new QSpinBox(this);
    m_stationCountSpinBox->setMinimum(1);
    m_stationCountSpinBox->setMaximum(10); // 设置合理的最大工位数
    m_stationCountSpinBox->setValue(m_currentStationCount);
    m_stationCountSpinBox->setToolTip("设置需要配置的工位数量");

    topLayout->addWidget(stationLabel);
    topLayout->addWidget(m_stationCountSpinBox);

    // 添加说明标签
    QLabel *infoLabel = new QLabel("在下表中设置每个管脚的通道个数、注释及各个工位的位索引", this);
    infoLabel->setStyleSheet("color: #666; font-style: italic;");
    topLayout->addWidget(infoLabel);
    topLayout->addStretch();

    mainLayout->addLayout(topLayout);

    // 创建表格
    m_pinSettingsTable = new QTableWidget(this);
    m_pinSettingsTable->setAlternatingRowColors(true);

    // 应用表格样式
    TableStyleManager::applyTableStyle(m_pinSettingsTable);

    mainLayout->addWidget(m_pinSettingsTable);

    // 创建按钮布局
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_okButton = new QPushButton("确定", this);
    m_cancelButton = new QPushButton("取消", this);

    buttonLayout->addStretch();
    buttonLayout->addWidget(m_okButton);
    buttonLayout->addWidget(m_cancelButton);

    mainLayout->addLayout(buttonLayout);

    // 连接信号槽
    connect(m_stationCountSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PinSettingsDialog::onStationCountChanged);
    connect(m_okButton, &QPushButton::clicked, this, &PinSettingsDialog::onAccepted);
    connect(m_cancelButton, &QPushButton::clicked, this, &PinSettingsDialog::onRejected);
}

void PinSettingsDialog::loadPinsData()
{
    qDebug() << "PinSettingsDialog::loadPinsData - 加载管脚数据";

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << "PinSettingsDialog::loadPinsData - 无法连接到数据库";
        return;
    }

    // 加载所有管脚及其注释
    QSqlQuery pinQuery("SELECT id, pin_name, pin_note FROM pin_list ORDER BY pin_name");
    if (pinQuery.exec())
    {
        while (pinQuery.next())
        {
            int pinId = pinQuery.value(0).toInt();
            QString pinName = pinQuery.value(1).toString();
            QString pinNote = pinQuery.value(2).toString();

            m_allPins[pinId] = pinName;
            m_pinNotes[pinId] = pinNote;
            m_channelCounts[pinId] = 1; // 默认通道数为1

            qDebug() << "  加载管脚: ID=" << pinId << ", 名称=" << pinName << ", 注释=" << pinNote;
        }
    }
    else
    {
        qWarning() << "PinSettingsDialog::loadPinsData - 查询管脚数据失败:" << pinQuery.lastError().text();
    }

    // 加载现有的管脚设置
    QSqlQuery settingsQuery("SELECT pin_id, station_number, station_bit_index, channel_count FROM pin_settings");
    if (settingsQuery.exec())
    {
        while (settingsQuery.next())
        {
            int pinId = settingsQuery.value(0).toInt();
            int stationNumber = settingsQuery.value(1).toInt();
            int stationBitIndex = settingsQuery.value(2).toInt();
            int channelCount = settingsQuery.value(3).toInt();

            // 保存现有设置
            m_existingSettings[pinId][stationNumber] = stationBitIndex;

            // 保存通道数
            m_channelCounts[pinId] = channelCount;

            // 更新工位数
            m_currentStationCount = qMax(m_currentStationCount, stationNumber + 1);
            qDebug() << "  已有设置: 管脚ID=" << pinId
                     << ", 工位=" << stationNumber
                     << ", 位索引=" << stationBitIndex
                     << ", 通道数=" << channelCount;
        }
    }
    else
    {
        qWarning() << "PinSettingsDialog::loadPinsData - 查询管脚设置数据失败:" << settingsQuery.lastError().text();
    }

    // 更新工位数SpinBox的值
    m_stationCountSpinBox->setValue(m_currentStationCount);
}

void PinSettingsDialog::updateTable()
{
    qDebug() << "PinSettingsDialog::updateTable - 更新表格，工位数=" << m_currentStationCount;

    // 保存当前的垂直滚动条位置
    int scrollPosition = m_pinSettingsTable->verticalScrollBar() ? m_pinSettingsTable->verticalScrollBar()->value() : 0;

    // 清空表格
    m_pinSettingsTable->clear();

    // 设置列数：管脚列、通道个数列、注释列，然后每个工位一列
    int stationColumnsStart = 3; // 前三列是固定的：管脚、通道个数、注释
    int columnCount = stationColumnsStart + m_currentStationCount;
    m_pinSettingsTable->setColumnCount(columnCount);

    // 设置表头
    QStringList headers;
    headers << "管脚"
            << "通道-个数"
            << "注释";
    for (int i = 0; i < m_currentStationCount; ++i)
    {
        // 将工位编号从0开始改为从1开始显示
        headers << QString("通道-工位%1").arg(i + 1);
    }
    m_pinSettingsTable->setHorizontalHeaderLabels(headers);

    // 设置工具提示
    for (int i = 0; i < m_pinSettingsTable->columnCount(); ++i)
    {
        if (i == 0)
        {
            m_pinSettingsTable->horizontalHeaderItem(i)->setToolTip("管脚名称");
        }
        else if (i == 1)
        {
            m_pinSettingsTable->horizontalHeaderItem(i)->setToolTip("设置通道个数");
        }
        else if (i == 2)
        {
            m_pinSettingsTable->horizontalHeaderItem(i)->setToolTip("管脚相关说明");
        }
        else
        {
            int stationNumber = i - stationColumnsStart + 1; // 计算工位号（从1开始）
            m_pinSettingsTable->horizontalHeaderItem(i)->setToolTip(
                QString("设置通道在工位%1的位索引值").arg(stationNumber));
        }
    }

    // 设置列宽
    m_pinSettingsTable->setColumnWidth(0, 120); // 管脚名称
    m_pinSettingsTable->setColumnWidth(1, 80);  // 通道个数
    m_pinSettingsTable->setColumnWidth(2, 200); // 注释

    // 设置其余列弹性拉伸
    m_pinSettingsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_pinSettingsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_pinSettingsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    for (int i = stationColumnsStart; i < columnCount; ++i)
    {
        m_pinSettingsTable->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Stretch);
    }

    // 设置行数
    m_pinSettingsTable->setRowCount(m_allPins.size());

    // 填充数据
    int row = 0;
    for (auto it = m_allPins.begin(); it != m_allPins.end(); ++it, ++row)
    {
        int pinId = it.key();
        QString pinName = it.value();

        // 设置管脚名称
        QTableWidgetItem *nameItem = new QTableWidgetItem(pinName);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable); // 不可编辑
        nameItem->setData(Qt::UserRole, pinId);                      // 存储管脚ID
        m_pinSettingsTable->setItem(row, 0, nameItem);

        // 设置通道个数
        QTableWidgetItem *channelCountItem = new QTableWidgetItem(QString::number(m_channelCounts[pinId]));
        channelCountItem->setTextAlignment(Qt::AlignCenter);
        m_pinSettingsTable->setItem(row, 1, channelCountItem);

        // 设置注释
        QTableWidgetItem *noteItem = new QTableWidgetItem(m_pinNotes[pinId]);
        m_pinSettingsTable->setItem(row, 2, noteItem);

        // 设置每个工位的管脚位索引
        for (int stationNumber = 0; stationNumber < m_currentStationCount; ++stationNumber)
        {
            QTableWidgetItem *stationItem = new QTableWidgetItem();
            stationItem->setTextAlignment(Qt::AlignCenter);

            // 如果有现有设置，则使用它
            if (m_existingSettings.contains(pinId) && m_existingSettings[pinId].contains(stationNumber))
            {
                int bitIndex = m_existingSettings[pinId][stationNumber];
                stationItem->setText(QString::number(bitIndex));
            }
            m_pinSettingsTable->setItem(row, stationNumber + stationColumnsStart, stationItem);
        }
    }

    // 恢复滚动条位置
    if (m_pinSettingsTable->verticalScrollBar())
    {
        m_pinSettingsTable->verticalScrollBar()->setValue(scrollPosition);
    }
}

void PinSettingsDialog::onStationCountChanged(int value)
{
    qDebug() << "PinSettingsDialog::onStationCountChanged - 工位数变更为" << value;

    // 更新当前工位数
    m_currentStationCount = value;

    // 更新表格
    updateTable();
}

void PinSettingsDialog::onAccepted()
{
    qDebug() << "PinSettingsDialog::onAccepted - 用户点击确定按钮";

    // 验证输入数据
    for (int row = 0; row < m_pinSettingsTable->rowCount(); ++row)
    {
        // 验证通道个数
        QTableWidgetItem *channelCountItem = m_pinSettingsTable->item(row, 1);
        if (channelCountItem)
        {
            bool ok;
            int channelCount = channelCountItem->text().toInt(&ok);
            if (!ok || channelCount <= 0)
            {
                QMessageBox::warning(this, "输入错误",
                                     QString("第 %1 行的通道个数必须是大于0的数字").arg(row + 1));
                return;
            }
        }

        // 验证工位位索引
        int stationColumnsStart = 3;
        for (int col = stationColumnsStart; col < m_pinSettingsTable->columnCount(); ++col)
        {
            QTableWidgetItem *stationItem = m_pinSettingsTable->item(row, col);
            if (stationItem && !stationItem->text().isEmpty())
            {
                bool ok;
                stationItem->text().toInt(&ok);
                if (!ok)
                {
                    int stationNumber = col - stationColumnsStart + 1;
                    QMessageBox::warning(this, "输入错误",
                                         QString("第 %1 行工位 %2 的位索引必须是数字")
                                             .arg(row + 1)
                                             .arg(stationNumber));
                    return;
                }
            }
        }
    }

    // 检查位索引值的唯一性
    if (!checkBitIndexUniqueness())
    {
        return;
    }

    // 保存设置
    if (saveSettings())
    {
        qDebug() << "PinSettingsDialog::onAccepted - 设置保存成功";
        accept();
    }
    else
    {
        qDebug() << "PinSettingsDialog::onAccepted - 设置保存失败";
    }
}

bool PinSettingsDialog::checkBitIndexUniqueness()
{
    qDebug() << "PinSettingsDialog::checkBitIndexUniqueness - 检查位索引值唯一性";

    // 用于存储每个工位的位索引使用情况
    // 结构: <工位号, <位索引值, <管脚ID, 管脚名称>>>
    QMap<int, QMap<int, QPair<int, QString>>> usedBitIndices;

    int stationColumnsStart = 3;

    // 收集所有使用的位索引值
    for (int row = 0; row < m_pinSettingsTable->rowCount(); ++row)
    {
        QTableWidgetItem *nameItem = m_pinSettingsTable->item(row, 0);
        if (!nameItem)
            continue;

        int pinId = nameItem->data(Qt::UserRole).toInt();
        QString pinName = nameItem->text();

        for (int stationNumber = 0; stationNumber < m_currentStationCount; ++stationNumber)
        {
            QTableWidgetItem *stationItem = m_pinSettingsTable->item(row, stationNumber + stationColumnsStart);
            if (!stationItem || stationItem->text().isEmpty())
                continue;

            bool ok;
            int bitIndex = stationItem->text().toInt(&ok);
            if (!ok)
                continue; // 跳过非数字值（前面已验证过）

            // 检查该工位的该位索引是否已被使用
            if (usedBitIndices[stationNumber].contains(bitIndex))
            {
                // 已被使用，记录冲突信息
                QPair<int, QString> existingPin = usedBitIndices[stationNumber][bitIndex];

                // 显示错误消息，告知冲突详情
                QString errorMsg = QString("%1 管脚在%2工位硬编码与分配重复").arg(pinName).arg(stationNumber + 1);
                QString detailMsg = QString("管脚 %1 和管脚 %2 在工位 %3 使用了相同的位索引值 %4。\n\n"
                                            "每个工位的位索引值必须唯一。请修改其中一个管脚的位索引值。")
                                        .arg(pinName)
                                        .arg(existingPin.second)
                                        .arg(stationNumber + 1)
                                        .arg(bitIndex);

                QMessageBox msgBox(this);
                msgBox.setIcon(QMessageBox::Warning);
                msgBox.setWindowTitle("位索引冲突");
                msgBox.setText(errorMsg);
                msgBox.setInformativeText(detailMsg);
                msgBox.setStandardButtons(QMessageBox::Ok);
                msgBox.exec();

                // 选中冲突的单元格以便用户修改
                m_pinSettingsTable->setCurrentItem(stationItem);
                stationItem->setSelected(true);

                qDebug() << "PinSettingsDialog::checkBitIndexUniqueness - 发现冲突:"
                         << "管脚" << pinName << "和管脚" << existingPin.second
                         << "在工位" << (stationNumber + 1) << "使用了相同的位索引" << bitIndex;

                return false;
            }

            // 记录该位索引已被使用
            usedBitIndices[stationNumber][bitIndex] = qMakePair(pinId, pinName);
        }
    }

    qDebug() << "PinSettingsDialog::checkBitIndexUniqueness - 未发现位索引冲突";
    return true;
}

void PinSettingsDialog::onRejected()
{
    qDebug() << "PinSettingsDialog::onRejected - 用户点击取消按钮";

    // 确认是否放弃修改
    if (isDataModified())
    {
        QMessageBox::StandardButton reply = QMessageBox::question(this, "确认",
                                                                  "您有未保存的修改，确定要放弃这些修改吗？",
                                                                  QMessageBox::Yes | QMessageBox::No,
                                                                  QMessageBox::No);
        if (reply == QMessageBox::No)
        {
            return;
        }
    }

    reject();
}

bool PinSettingsDialog::isDataModified()
{
    // 检查是否有未保存的修改
    for (int row = 0; row < m_pinSettingsTable->rowCount(); ++row)
    {
        QTableWidgetItem *nameItem = m_pinSettingsTable->item(row, 0);
        if (!nameItem)
            continue;

        int pinId = nameItem->data(Qt::UserRole).toInt();

        // 检查通道个数是否修改
        QTableWidgetItem *channelCountItem = m_pinSettingsTable->item(row, 1);
        if (channelCountItem)
        {
            bool ok;
            int channelCount = channelCountItem->text().toInt(&ok);
            if (ok && channelCount != m_channelCounts[pinId])
            {
                return true;
            }
        }

        // 检查注释是否修改
        QTableWidgetItem *noteItem = m_pinSettingsTable->item(row, 2);
        if (noteItem && noteItem->text() != m_pinNotes[pinId])
        {
            return true;
        }

        // 检查工位设置是否修改
        int stationColumnsStart = 3;
        for (int stationNumber = 0; stationNumber < m_currentStationCount; ++stationNumber)
        {
            QTableWidgetItem *stationItem = m_pinSettingsTable->item(row, stationNumber + stationColumnsStart);
            if (!stationItem)
                continue;

            bool hasOriginalValue = m_existingSettings.contains(pinId) &&
                                    m_existingSettings[pinId].contains(stationNumber);

            if (stationItem->text().isEmpty())
            {
                if (hasOriginalValue)
                {
                    return true;
                }
            }
            else
            {
                bool ok;
                int bitIndex = stationItem->text().toInt(&ok);
                if (ok)
                {
                    if (!hasOriginalValue || bitIndex != m_existingSettings[pinId][stationNumber])
                    {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

bool PinSettingsDialog::saveSettings()
{
    qDebug() << "PinSettingsDialog::saveSettings - 开始保存管脚设置";

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << "PinSettingsDialog::saveSettings - 无法连接到数据库";
        QMessageBox::critical(this, "错误", "无法连接到数据库");
        return false;
    }

    // 开始事务
    db.transaction();

    try
    {
        // 首先从表格中收集最新的数据
        QMap<int, QString> updatedNotes;     // 存储更新后的注释
        QMap<int, int> updatedChannelCounts; // 存储更新后的通道个数

        for (int row = 0; row < m_pinSettingsTable->rowCount(); ++row)
        {
            // 获取管脚ID
            QTableWidgetItem *nameItem = m_pinSettingsTable->item(row, 0);
            if (!nameItem)
                continue;
            int pinId = nameItem->data(Qt::UserRole).toInt();

            // 获取通道个数
            QTableWidgetItem *channelCountItem = m_pinSettingsTable->item(row, 1);
            if (channelCountItem)
            {
                bool ok;
                int channelCount = channelCountItem->text().toInt(&ok);
                if (ok && channelCount > 0)
                {
                    updatedChannelCounts[pinId] = channelCount;
                }
                else
                {
                    updatedChannelCounts[pinId] = 1; // 默认值为1
                }
            }
            else
            {
                updatedChannelCounts[pinId] = 1; // 默认值为1
            }

            // 获取注释
            QTableWidgetItem *noteItem = m_pinSettingsTable->item(row, 2);
            if (noteItem)
            {
                updatedNotes[pinId] = noteItem->text();
            }
            else
            {
                updatedNotes[pinId] = "";
            }
        }

        // 更新pin_list表中的注释
        QSqlQuery updateNoteQuery;
        updateNoteQuery.prepare("UPDATE pin_list SET pin_note = ? WHERE id = ?");

        for (auto it = updatedNotes.begin(); it != updatedNotes.end(); ++it)
        {
            int pinId = it.key();
            QString note = it.value();

            updateNoteQuery.bindValue(0, note);
            updateNoteQuery.bindValue(1, pinId);

            if (!updateNoteQuery.exec())
            {
                throw std::runtime_error(updateNoteQuery.lastError().text().toStdString());
            }

            qDebug() << "  更新注释: 管脚ID=" << pinId << ", 注释=" << note;
        }

        // 清空pin_settings表
        QSqlQuery clearQuery("DELETE FROM pin_settings");
        if (!clearQuery.exec())
        {
            throw std::runtime_error(clearQuery.lastError().text().toStdString());
        }

        // 插入新的pin_settings数据
        QSqlQuery insertQuery;
        insertQuery.prepare("INSERT INTO pin_settings (pin_id, channel_count, station_bit_index, station_number) "
                            "VALUES (?, ?, ?, ?)");

        // 遍历表格中的每个管脚
        int stationColumnsStart = 3; // 前三列是固定的：管脚、通道个数、注释
        for (int row = 0; row < m_pinSettingsTable->rowCount(); ++row)
        {
            // 获取管脚ID
            QTableWidgetItem *nameItem = m_pinSettingsTable->item(row, 0);
            if (!nameItem)
                continue;

            int pinId = nameItem->data(Qt::UserRole).toInt();
            int channelCount = updatedChannelCounts[pinId];

            // 遍历每个工位
            for (int stationNumber = 0; stationNumber < m_currentStationCount; ++stationNumber)
            {
                QTableWidgetItem *stationItem = m_pinSettingsTable->item(row, stationNumber + stationColumnsStart);
                if (!stationItem)
                    continue;

                QString bitIndexText = stationItem->text().trimmed();
                if (bitIndexText.isEmpty())
                    continue; // 跳过空值

                bool ok;
                int bitIndex = bitIndexText.toInt(&ok);
                if (!ok)
                    continue; // 跳过非数字值

                // 插入数据
                insertQuery.bindValue(0, pinId);
                insertQuery.bindValue(1, channelCount);
                insertQuery.bindValue(2, bitIndex);
                insertQuery.bindValue(3, stationNumber);

                if (!insertQuery.exec())
                {
                    throw std::runtime_error(insertQuery.lastError().text().toStdString());
                }

                qDebug() << "  保存设置: 管脚ID=" << pinId
                         << ", 通道数=" << channelCount
                         << ", 工位=" << stationNumber
                         << ", 位索引=" << bitIndex;
            }
        }

        // 提交事务
        db.commit();
        QMessageBox::information(this, "成功", "管脚设置已保存");
        return true;
    }
    catch (const std::exception &e)
    {
        // 回滚事务
        db.rollback();
        qWarning() << "PinSettingsDialog::saveSettings - 保存失败:" << e.what();
        QMessageBox::critical(this, "错误", QString("保存管脚设置失败: %1").arg(e.what()));
        return false;
    }
}