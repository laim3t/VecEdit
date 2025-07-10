#include "addrowdialog.h"
#include "../database/databasemanager.h"
#include "../vector/vectordatahandler.h"
#include "../timeset/timesetdialog.h"
#include "../timeset/timesetdataaccess.h"
#include "../pin/pinvalueedit.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QCheckBox>
#include <QPushButton>
#include <QComboBox>
#include <QGroupBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QMessageBox>
#include <QDebug>
#include <QProgressDialog>
#include <QCoreApplication>

AddRowDialog::AddRowDialog(int tableId, const QMap<int, QString> &pinInfo, QWidget *parent)
    : QDialog(parent),
      m_totalRows(0),
      m_remainingRows(8388608), // 默认最大行数
      m_tableId(tableId),
      m_pinInfo(pinInfo)
{
    setupUi();
    setupTable();
    setupConnections();
    loadTimeSets();

    // 确保默认选择第一个TimeSet
    if (m_timeSetComboBox->count() > 0)
    {
        m_timeSetComboBox->setCurrentIndex(0);
        qDebug() << "AddRowDialog构造函数 - 默认选择第一个TimeSet:"
                 << "名称=" << m_timeSetComboBox->currentText()
                 << "，ID=" << m_timeSetComboBox->currentData().toInt();
    }

    // 获取当前表的总行数
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) FROM vector_table_data WHERE table_id = ?");
    query.addBindValue(m_tableId);
    if (query.exec() && query.next())
    {
        m_totalRows = query.value(0).toInt();
        m_totalRowsLabel->setText(QString::number(m_totalRows));

        // 更新剩余可用行数
        updateRemainingRowsDisplay();
    }

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &AddRowDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &AddRowDialog::reject);
}

AddRowDialog::~AddRowDialog()
{
}

void AddRowDialog::setupUi()
{
    setWindowTitle(tr("向量行数据录入"));
    setMinimumWidth(700);  // 增加宽度
    setMinimumHeight(600); // 增加高度
    resize(800, 650);      // 设置默认大小
    setModal(true);        // 设置为模态对话框

    // 创建主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // 顶部区域 - TimeSet设置按钮
    QHBoxLayout *topLayout = new QHBoxLayout();
    m_timeSetButton = new QPushButton(tr("TimeSet设置"), this);
    topLayout->addWidget(m_timeSetButton);
    topLayout->addStretch(); // 推动按钮到左侧
    mainLayout->addLayout(topLayout);

    // 添加注释标签 - 更新内容以匹配旧轨道
    QLabel *noteLabel = new QLabel(tr("注释: 请确保添加的行数是下方表格中行数的倍数。"), this);
    noteLabel->setStyleSheet("color: black;");
    mainLayout->addWidget(noteLabel);

    // 添加警告标签 - 用于显示整数倍验证失败的消息
    m_warningLabel = new QLabel("", this);
    m_warningLabel->setStyleSheet("color: red; font-weight: bold;");
    m_warningLabel->setVisible(false); // 默认隐藏
    mainLayout->addWidget(m_warningLabel);

    // 创建表格预览区域
    m_previewTable = new QTableWidget(this);
    m_previewTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_previewTable->setSelectionMode(QAbstractItemView::SingleSelection);
    mainLayout->addWidget(m_previewTable);

    // 创建表格操作按钮区域
    QHBoxLayout *tableButtonsLayout = new QHBoxLayout();
    tableButtonsLayout->addStretch();

    m_addRowButton = new QPushButton("+", this);
    m_addRowButton->setFixedSize(30, 30);
    m_addRowButton->setToolTip(tr("添加行"));
    tableButtonsLayout->addWidget(m_addRowButton);

    m_removeRowButton = new QPushButton("-", this);
    m_removeRowButton->setFixedSize(30, 30);
    m_removeRowButton->setToolTip(tr("删除行"));
    tableButtonsLayout->addWidget(m_removeRowButton);

    mainLayout->addLayout(tableButtonsLayout);

    // 创建底部信息和设置区域
    QHBoxLayout *bottomLayout = new QHBoxLayout();

    // 信息分组
    m_infoGroup = new QGroupBox(tr("信息"), this);
    QFormLayout *infoLayout = new QFormLayout(m_infoGroup);

    m_totalRowsLabel = new QLabel("0", this);
    infoLayout->addRow(tr("文件中总行数:"), m_totalRowsLabel);

    m_remainingRowsLabel = new QLabel(QString::number(m_remainingRows), this);
    infoLayout->addRow(tr("剩余可用行数:"), m_remainingRowsLabel);

    bottomLayout->addWidget(m_infoGroup);

    // 行操作分组
    m_rowGroup = new QGroupBox(tr("行"), this);
    QVBoxLayout *rowLayout = new QVBoxLayout(m_rowGroup);

    m_appendToEndCheckBox = new QCheckBox(tr("添加到最后"), m_rowGroup);
    m_appendToEndCheckBox->setChecked(true);
    rowLayout->addWidget(m_appendToEndCheckBox);

    QHBoxLayout *startRowLayout = new QHBoxLayout();
    m_startRowLabel = new QLabel(tr("向量插入:"), m_rowGroup);
    m_startRowSpinBox = new QSpinBox(m_rowGroup);
    m_startRowSpinBox->setRange(1, 999999999);
    m_startRowSpinBox->setValue(1);
    m_startRowSpinBox->setEnabled(false); // 默认禁用，因为"添加到最后"被选中
    startRowLayout->addWidget(m_startRowLabel);
    startRowLayout->addWidget(m_startRowSpinBox);
    rowLayout->addLayout(startRowLayout);

    QHBoxLayout *rowCountLayout = new QHBoxLayout();
    QLabel *rowCountLabel = new QLabel(tr("行数:"), m_rowGroup);
    m_rowCountSpinBox = new QSpinBox(m_rowGroup);
    m_rowCountSpinBox->setRange(1, 9999999);
    m_rowCountSpinBox->setValue(1);
    rowCountLayout->addWidget(rowCountLabel);
    rowCountLayout->addWidget(m_rowCountSpinBox);
    rowLayout->addLayout(rowCountLayout);

    bottomLayout->addWidget(m_rowGroup);

    // 设置分组
    m_settingsGroup = new QGroupBox(tr("设置"), this);
    QFormLayout *settingsLayout = new QFormLayout(m_settingsGroup);

    m_timeSetComboBox = new QComboBox(m_settingsGroup);
    settingsLayout->addRow(tr("TimeSet:"), m_timeSetComboBox);

    bottomLayout->addWidget(m_settingsGroup);

    mainLayout->addLayout(bottomLayout);

    // 创建标准按钮
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(m_buttonBox);

    setLayout(mainLayout);
}

void AddRowDialog::setupTable()
{
    m_previewTable->clear(); // 清空旧内容

    // --- Fallback Logic ---
    // 如果没有传入管脚信息，则执行旧的数据库查询逻辑
    QSqlDatabase db = DatabaseManager::instance()->database();

    // 如果没有传入管脚信息，则从数据库查询
    if (m_pinInfo.isEmpty() && m_tableId > 0)
    {
        QSqlQuery query(db);
        // 查询当前表的所有列配置，并过滤出管脚列
        QList<QPair<QString, int>> pinColumns; // 存储管脚名称和顺序

        // 从VectorTableColumnConfiguration表获取所有列信息
        query.prepare("SELECT column_name, column_order FROM VectorTableColumnConfiguration "
                      "WHERE master_record_id = (SELECT id FROM VectorTableMasterRecord WHERE original_vector_table_id = ?) "
                      "AND column_type = 'Pin' "
                      "ORDER BY column_order");
        query.addBindValue(m_tableId);

        if (query.exec())
        {
            while (query.next())
            {
                QString columnName = query.value(0).toString();
                int columnOrder = query.value(1).toInt();
                pinColumns.append(qMakePair(columnName, columnOrder));
                m_pinInfo[columnOrder] = columnName; // 填充m_pinInfo
            }
        }
        else
        {
            qWarning() << "查询向量表列配置失败:" << query.lastError().text();
        }
    }

    // 再次检查m_pinInfo，如果填充了数据，则继续设置表格
    if (!m_pinInfo.isEmpty())
    {
        m_pinOptions = m_pinInfo;
        m_previewTable->setColumnCount(m_pinInfo.size());

        QList<QString> pinNames = m_pinInfo.values();

        // 查询每个管脚的信息（类型）
        for (int i = 0; i < pinNames.size(); i++)
        {
            const QString &pinName = pinNames.at(i);

            // 从vector_table_pins表查询管脚类型
            QSqlQuery pinQuery(db);
            pinQuery.prepare("SELECT pin_type FROM vector_table_pins WHERE table_id = ? AND pin_id = (SELECT id FROM pin_list WHERE pin_name = ?)");
            pinQuery.addBindValue(m_tableId);
            pinQuery.addBindValue(pinName);

            QString typeName = "In"; // 默认类型
            if (pinQuery.exec() && pinQuery.next())
            {
                int typeId = pinQuery.value(0).toInt();

                // 查询类型名称
                QSqlQuery typeQuery(db);
                typeQuery.prepare("SELECT type_name FROM type_options WHERE id = ?");
                typeQuery.addBindValue(typeId);
                if (typeQuery.exec() && typeQuery.next())
                {
                    typeName = typeQuery.value(0).toString();
                }
            }

            // 创建表头项
            QString headerText = pinName + "\nx1\n" + typeName;
            QTableWidgetItem *headerItem = new QTableWidgetItem(headerText);
            headerItem->setTextAlignment(Qt::AlignCenter);
            QFont headerFont = headerItem->font();
            headerFont.setBold(true);
            headerItem->setFont(headerFont);

            m_previewTable->setHorizontalHeaderItem(i, headerItem);
        }

        // 默认添加一行
        m_previewTable->setRowCount(1);
        initializeRowWithDefaultValues(0);

        // 调整表格列宽 - 使用自定义的列宽策略
        // 首先让表格根据内容调整列宽
        m_previewTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

        // 确保表格能够滚动显示所有内容
        m_previewTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_previewTable->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

        // 设置较大的行高，以便完整显示表头
        m_previewTable->verticalHeader()->setDefaultSectionSize(25);

        // 设置表头高度足够显示多行文本
        m_previewTable->horizontalHeader()->setMinimumHeight(60);
        m_previewTable->horizontalHeader()->setMaximumHeight(60);

        // 确保表格有足够的高度
        m_previewTable->setMinimumHeight(200);
    }
    else
    {
        qWarning() << "未找到管脚列";
        // 创建一个空表格，提示用户先添加管脚
        m_previewTable->setColumnCount(1);
        m_previewTable->setHorizontalHeaderItem(0, new QTableWidgetItem("请先添加管脚"));
        m_previewTable->setRowCount(1);

        // 这里不使用PinValueLineEdit，因为这是一个提示信息而不是数据输入
        QTableWidgetItem *item = new QTableWidgetItem("未配置管脚");
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        item->setTextAlignment(Qt::AlignCenter);
        m_previewTable->setItem(0, 0, item);
    }
}

void AddRowDialog::setupConnections()
{
    // 连接信号与槽
    connect(m_appendToEndCheckBox, &QCheckBox::stateChanged, this, &AddRowDialog::onAppendToEndStateChanged);
    connect(m_addRowButton, &QPushButton::clicked, this, &AddRowDialog::onAddRowButtonClicked);
    connect(m_removeRowButton, &QPushButton::clicked, this, &AddRowDialog::onRemoveRowButtonClicked);
    connect(m_timeSetButton, &QPushButton::clicked, this, &AddRowDialog::onTimeSetButtonClicked);

    // 行数变化时更新剩余可用行数显示并验证整数倍关系
    connect(m_rowCountSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &AddRowDialog::validateRowCount);
    connect(m_rowCountSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &AddRowDialog::updateRemainingRowsDisplay);

    // 连接表格的itemChanged信号，用于处理直接编辑QTableWidgetItem的情况
    connect(m_previewTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item)
            {
        // 对于非PinValueLineEdit的单元格，手动验证输入
        if (item) {
            QString text = item->text();
            if (!text.isEmpty()) {
                QString validChars = "01LlHhXxSsVvMm";
                QChar ch = text[0];
                
                // 检查是否为有效字符
                if (!validChars.contains(ch)) {
                    item->setText("X"); // 无效字符，设为默认值
                } else if (ch.isLower()) {
                    // 将小写字母转换为大写
                    item->setText(text.toUpper());
                }
            }
        }
        
        // 更新行数验证
        validateRowCount(m_rowCountSpinBox->value()); });

    // 确保表格行选择行为
    ensureRowSelectionBehavior();
}

void AddRowDialog::ensureRowSelectionBehavior()
{
    // 确保点击单元格会选中整行
    m_previewTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    // 连接单元格点击事件，确保选中整行
    connect(m_previewTable, &QTableWidget::cellClicked, [this](int row, int column)
            {
        // 清除当前选择
        m_previewTable->clearSelection();
        // 选择整行
        m_previewTable->selectRow(row); });
}

void AddRowDialog::loadTimeSets()
{
    // 加载TimeSet选项
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery query(db);

    if (query.exec("SELECT id, timeset_name FROM timeset_list ORDER BY id"))
    {
        // 保存当前选择的TimeSet ID（如果有）
        int currentSelectedId = -1;
        if (m_timeSetComboBox->count() > 0)
        {
            currentSelectedId = m_timeSetComboBox->currentData().toInt();
        }

        m_timeSetComboBox->clear();
        m_timeSetOptions.clear();

        int indexToSelect = 0; // 默认选择第一个
        int itemCount = 0;

        while (query.next())
        {
            int timesetId = query.value(0).toInt();
            QString timesetName = query.value(1).toString();

            m_timeSetOptions[timesetId] = timesetName;
            m_timeSetComboBox->addItem(timesetName, timesetId);

            // 如果这是之前选择的ID，记录它的索引
            if (timesetId == currentSelectedId)
            {
                indexToSelect = itemCount;
            }

            itemCount++;
        }

        // 如果有项目，设置选择
        if (m_timeSetComboBox->count() > 0)
        {
            // 如果之前有选择并且仍然存在，选择它；否则选择第一个
            m_timeSetComboBox->setCurrentIndex(indexToSelect);

            // 添加调试日志
            int selectedId = m_timeSetComboBox->currentData().toInt();
            QString selectedName = m_timeSetComboBox->currentText();
            qDebug() << "AddRowDialog::loadTimeSets - 已选择TimeSet:" << selectedName
                     << "，ID:" << selectedId
                     << "，索引:" << indexToSelect
                     << "，总项数:" << m_timeSetComboBox->count();
        }
    }
    else
    {
        qWarning() << "加载TimeSet失败:" << query.lastError().text();
    }
}

void AddRowDialog::updateRemainingRowsDisplay()
{
    // 计算并更新剩余可用行数
    int currentRows = m_totalRows;
    int newRows = m_rowCountSpinBox->value();
    int remainingAfterAdd = m_remainingRows - newRows;

    if (remainingAfterAdd < 0)
    {
        remainingAfterAdd = 0;
        m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
        QMessageBox::warning(this, tr("警告"), tr("添加的行数超出了可用行数！"));
    }
    else
    {
        m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
    }

    m_remainingRowsLabel->setText(QString::number(remainingAfterAdd));
}

void AddRowDialog::validateRowCount(int value)
{
    int previewRows = m_previewTable->rowCount();

    if (previewRows > 0)
    {
        if (value % previewRows != 0)
        {
            // 不是整数倍，显示警告
            m_rowCountSpinBox->setStyleSheet("QSpinBox { background-color: #FFDDDD; }");
            m_warningLabel->setText(tr("警告: 行数(%1)必须是模式行数(%2)的整数倍!").arg(value).arg(previewRows));
            m_warningLabel->setVisible(true);
            m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
        }
        else
        {
            // 是整数倍，清除警告
            m_rowCountSpinBox->setStyleSheet("");
            m_warningLabel->setVisible(false);
            m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
        }
    }
    else
    {
        // 没有预览行，不进行验证
        m_rowCountSpinBox->setStyleSheet("");
        m_warningLabel->setVisible(false);
        m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    }
}

void AddRowDialog::onAppendToEndStateChanged(int state)
{
    // 启用或禁用"向量插入"输入框
    m_startRowSpinBox->setEnabled(state != Qt::Checked);
}

void AddRowDialog::onAddRowButtonClicked()
{
    // 添加一行到预览表格
    int currentRowCount = m_previewTable->rowCount();
    m_previewTable->setRowCount(currentRowCount + 1);

    // 使用辅助方法初始化新行的单元格为默认值"X"
    initializeRowWithDefaultValues(currentRowCount);

    // 在添加行后立即重新验证行数
    validateRowCount(m_rowCountSpinBox->value());
}

void AddRowDialog::onRemoveRowButtonClicked()
{
    // 删除选中的行
    QList<QTableWidgetItem *> selectedItems = m_previewTable->selectedItems();
    if (!selectedItems.isEmpty())
    {
        int row = selectedItems.first()->row();

        // 确保至少保留一行
        if (m_previewTable->rowCount() > 1)
        {
            m_previewTable->removeRow(row);

            // 删除行后重新验证行数
            validateRowCount(m_rowCountSpinBox->value());
        }
        else
        {
            QMessageBox::information(this, tr("提示"), tr("必须至少保留一行数据。"));
        }
    }
    else
    {
        QMessageBox::information(this, tr("提示"), tr("请先选择要删除的行。"));
    }
}

void AddRowDialog::onTimeSetButtonClicked()
{
    // 打开TimeSet设置对话框
    TimeSetDialog dialog(this, false);

    if (dialog.exec() == QDialog::Accepted)
    {
        // 刷新TimeSet列表
        loadTimeSets();
    }
}

void AddRowDialog::accept()
{
    // 最终验证行数是否是模式行数的整数倍
    int rowCount = m_rowCountSpinBox->value();
    int previewRows = m_previewTable->rowCount();

    if (previewRows > 0 && rowCount % previewRows != 0)
    {
        QMessageBox::warning(this, tr("验证失败"),
                             tr("设置的总行数必须是表格模式行数的整数倍！\n"
                                "当前表格有 %1 行，总行数 %2 不是其整数倍。")
                                 .arg(previewRows)
                                 .arg(rowCount));
        return;
    }

    // 检查是否超出可用行数
    if (rowCount > m_remainingRows)
    {
        QMessageBox::warning(this, tr("验证失败"),
                             tr("添加的行数超出了系统可用行数！\n"
                                "当前可用行数: %1, 设置的行数: %2")
                                 .arg(m_remainingRows)
                                 .arg(rowCount));
        return;
    }

    // 对于大量数据（超过1000行），显示进度对话框
    if (rowCount > 1000)
    {
        // 创建并显示不可取消的进度对话框（仅提供视觉反馈）
        QProgressDialog progressDialog(tr("正在准备数据..."), tr("处理中"), 0, 100, this);
        progressDialog.setWindowTitle(tr("添加向量行"));
        progressDialog.setWindowModality(Qt::WindowModal);
        progressDialog.setCancelButton(nullptr); // 不允许取消
        progressDialog.setMinimumDuration(0);    // 立即显示
        progressDialog.setValue(10);

        // 处理事件，确保对话框显示
        QCoreApplication::processEvents();

        // 模拟数据准备进度
        progressDialog.setLabelText(tr("正在验证数据..."));
        progressDialog.setValue(30);
        QCoreApplication::processEvents();

        progressDialog.setLabelText(tr("正在生成模式..."));
        progressDialog.setValue(60);
        QCoreApplication::processEvents();

        progressDialog.setLabelText(tr("即将完成..."));
        progressDialog.setValue(90);
        QCoreApplication::processEvents();
    }

    // 继续标准接受操作
    QDialog::accept();
}

int AddRowDialog::getRowCount() const
{
    return m_rowCountSpinBox->value();
}

int AddRowDialog::getStartRow() const
{
    return m_startRowSpinBox->value();
}

bool AddRowDialog::isAppendToEnd() const
{
    return m_appendToEndCheckBox->isChecked();
}

int AddRowDialog::getSelectedTimeSetId() const
{
    int timeSetId = m_timeSetComboBox->currentData().toInt();

    // 添加调试日志，帮助诊断问题
    qDebug() << "AddRowDialog::getSelectedTimeSetId - 选择的TimeSet ID:" << timeSetId
             << "，对应的名称:" << m_timeSetComboBox->currentText();

    // 确保返回有效的TimeSet ID
    if (timeSetId <= 0 && m_timeSetComboBox->count() > 0)
    {
        // 如果ID无效但有可选项，尝试获取当前选择的项
        timeSetId = m_timeSetComboBox->currentData().toInt();
        qDebug() << "AddRowDialog::getSelectedTimeSetId - 尝试重新获取ID:" << timeSetId;
    }

    return timeSetId;
}

QList<QStringList> AddRowDialog::getTableData() const
{
    QList<QStringList> result;

    for (int row = 0; row < m_previewTable->rowCount(); row++)
    {
        QStringList rowData;
        for (int col = 0; col < m_previewTable->columnCount(); col++)
        {
            // 获取单元格中的PinValueLineEdit控件
            PinValueLineEdit *editor = qobject_cast<PinValueLineEdit *>(m_previewTable->cellWidget(row, col));
            QString cellText;

            if (editor)
            {
                // 如果是PinValueLineEdit控件，获取其文本
                cellText = editor->text();
            }
            else
            {
                // 如果不是PinValueLineEdit控件（例如特殊单元格），尝试获取QTableWidgetItem
                QTableWidgetItem *item = m_previewTable->item(row, col);
                cellText = (item && !item->text().isEmpty()) ? item->text() : "X";
            }

            // 确保大写并验证有效字符
            if (cellText.length() > 0)
            {
                QString validChars = "01LHXSVM";
                QChar firstChar = cellText[0].toUpper();
                if (!validChars.contains(firstChar))
                {
                    cellText = "X"; // 如果无效，使用默认值
                }
                else
                {
                    cellText = QString(firstChar); // 只取第一个字符并确保大写
                }
            }
            else
            {
                cellText = "X"; // 空文本使用默认值
            }

            rowData.append(cellText);
        }
        result.append(rowData);
    }

    return result;
}

const QMap<int, QString> &AddRowDialog::getPinOptions() const
{
    return m_pinOptions;
}

// 初始化行的默认值为"X"
void AddRowDialog::initializeRowWithDefaultValues(int rowIndex)
{
    // 确保行索引有效
    if (rowIndex < 0 || rowIndex >= m_previewTable->rowCount())
        return;

    // 为该行的每个单元格设置默认值"X"并使用PinValueLineEdit控件
    for (int col = 0; col < m_previewTable->columnCount(); col++)
    {
        // 创建自定义的PinValueLineEdit控件，具有输入限制和大小写转换功能
        PinValueLineEdit *editor = new PinValueLineEdit(m_previewTable);
        editor->setText("X");

        // 将编辑器添加到表格
        m_previewTable->setCellWidget(rowIndex, col, editor);

        // 连接编辑完成信号，确保在用户完成编辑后更新行数验证
        connect(editor, &QLineEdit::editingFinished, this, [this]()
                { validateRowCount(m_rowCountSpinBox->value()); });
    }
}