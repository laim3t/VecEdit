#include "addrowdialog.h"
#include "../database/databasemanager.h"
#include "../vector/vectordatahandler.h"
#include "../timeset/timesetdialog.h"
#include "../timeset/timesetdataaccess.h"

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

AddRowDialog::AddRowDialog(QWidget *parent)
    : QDialog(parent),
      m_totalRows(0),
      m_remainingRows(8388608), // 默认最大行数
      m_tableId(0)
{
    setupUi();
    setupTable();
    setupConnections();
    loadTimeSets();

    // 获取当前选中的向量表ID
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery query(db);
    query.prepare("SELECT id FROM vector_tables WHERE is_current = 1 LIMIT 1");
    if (query.exec() && query.next())
    {
        m_tableId = query.value(0).toInt();

        // 获取当前表的总行数
        query.prepare("SELECT COUNT(*) FROM vector_table_data WHERE table_id = ?");
        query.addBindValue(m_tableId);
        if (query.exec() && query.next())
        {
            m_totalRows = query.value(0).toInt();
            m_totalRowsLabel->setText(QString::number(m_totalRows));

            // 更新剩余可用行数
            updateRemainingRowsDisplay();
        }
    }

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &AddRowDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &AddRowDialog::reject);
}

AddRowDialog::AddRowDialog(const QMap<int, QString> &pinInfo, QWidget *parent)
    : AddRowDialog(parent) // 调用委托构造函数
{
    // 存储传入的管脚信息并重新设置表格
    m_pinInfo = pinInfo;
    setupTable(); // 再次调用以使用新的管脚信息
}

AddRowDialog::~AddRowDialog()
{
}

void AddRowDialog::setupUi()
{
    setWindowTitle(tr("向量行数据录入"));
    setMinimumWidth(600);
    setMinimumHeight(500);
    setModal(true); // 设置为模态对话框

    // 创建主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // 顶部区域 - TimeSet设置按钮
    QHBoxLayout *topLayout = new QHBoxLayout();
    m_timeSetButton = new QPushButton(tr("TimeSet设置"), this);
    topLayout->addWidget(m_timeSetButton);
    topLayout->addStretch(); // 推动按钮到左侧
    mainLayout->addLayout(topLayout);

    // 添加注释标签
    QLabel *noteLabel = new QLabel(tr("注释: 请确保添加的数据下方表格中行和列的值。"), this);
    noteLabel->setStyleSheet("color: black;");
    mainLayout->addWidget(noteLabel);

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

    // 优先使用通过构造函数传入的管脚信息
    if (!m_pinInfo.isEmpty())
    {
        m_pinOptions = m_pinInfo; // 使用传入的管脚信息
        m_previewTable->setColumnCount(m_pinInfo.size());

        QList<QString> pinNames = m_pinInfo.values();
        QSqlDatabase db = DatabaseManager::instance()->database();

        for (int i = 0; i < pinNames.size(); ++i)
        {
            const QString &pinName = pinNames.at(i);
            
            // 查询管脚类型
            QSqlQuery pinQuery(db);
            pinQuery.prepare("SELECT type_id FROM pin_list WHERE pin_name = ?");
            pinQuery.addBindValue(pinName);

            QString typeName = "In"; // 默认类型
            if (pinQuery.exec() && pinQuery.next())
            {
                int typeId = pinQuery.value(0).toInt();
                QSqlQuery typeQuery(db);
                typeQuery.prepare("SELECT type_name FROM pin_types WHERE id = ?");
                typeQuery.addBindValue(typeId);
                if (typeQuery.exec() && typeQuery.next())
                {
                    typeName = typeQuery.value(0).toString();
                }
            }

            QString headerText = QString("%1\nx1\n%2").arg(pinName).arg(typeName);
            QTableWidgetItem *headerItem = new QTableWidgetItem(headerText);
            headerItem->setTextAlignment(Qt::AlignCenter);
            m_previewTable->setHorizontalHeaderItem(i, headerItem);
        }

        m_previewTable->setRowCount(1); // 默认显示一行
        return; // 使用传入数据后直接返回
    }

    // --- Fallback Logic ---
    // 如果没有传入管脚信息，则执行旧的数据库查询逻辑
    // 在新架构下获取当前表的管脚信息，用于设置表格列
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery query(db);

    // 获取当前选中的向量表ID
    query.prepare("SELECT id FROM vector_tables WHERE is_current = 1 LIMIT 1");
    if (query.exec() && query.next())
    {
        int tableId = query.value(0).toInt();
        m_tableId = tableId;

        // 查询当前表的所有列配置，并过滤出管脚列
        QList<QPair<QString, int>> pinColumns; // 存储管脚名称和顺序

        // 从VectorTableColumnConfiguration表获取所有列信息，使用正确的列名
        query.prepare("SELECT id, column_name, column_order, column_type, default_value, is_visible FROM VectorTableColumnConfiguration "
                      "WHERE master_record_id = (SELECT id FROM VectorTableMasterRecord WHERE original_vector_table_id = ?) "
                      "ORDER BY column_order");
        query.addBindValue(tableId);

        if (query.exec())
        {
            while (query.next())
            {
                QString columnName = query.value(1).toString();
                int columnOrder = query.value(2).toInt();
                QString columnType = query.value(3).toString();

                // 根据Vector::columnDataTypeFromString函数的逻辑，除了标准列名外，其他列名都视为管脚列
                if (columnName != "Label" && columnName != "Instruction" &&
                    columnName != "TimeSet" && columnName != "Capture" &&
                    columnName != "EXT" && columnName != "Comment")
                {
                    // 确认这是一个管脚列
                    pinColumns.append(qMakePair(columnName, columnOrder));
                    // 保存到选项映射
                    m_pinOptions[columnOrder] = columnName;
                }
            }
        }
        else
        {
            qWarning() << "查询向量表列配置失败:" << query.lastError().text();
        }

        // 设置表格列数和表头
        if (!pinColumns.isEmpty())
        {
            m_previewTable->setColumnCount(pinColumns.size());

            // 查询每个管脚的信息（类型）
            for (int i = 0; i < pinColumns.size(); i++)
            {
                const auto &pinInfo = pinColumns[i];
                const QString &pinName = pinInfo.first;

                // 尝试从pin_list表查询管脚信息
                QSqlQuery pinQuery(db);
                pinQuery.prepare("SELECT type_id FROM pin_list WHERE pin_name = ?");
                pinQuery.addBindValue(pinName);

                QString typeName = "In"; // 默认类型
                if (pinQuery.exec() && pinQuery.next())
                {
                    int typeId = pinQuery.value(0).toInt();

                    // 查询类型名称
                    QSqlQuery typeQuery(db);
                    typeQuery.prepare("SELECT type_name FROM pin_types WHERE id = ?");
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
            for (int col = 0; col < m_previewTable->columnCount(); col++)
            {
                QTableWidgetItem *item = new QTableWidgetItem("X");
                m_previewTable->setItem(0, col, item);
            }

            // 调整表格列宽
            m_previewTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        }
        else
        {
            qWarning() << "未找到管脚列";
            // 创建一个空表格，提示用户先添加管脚
            m_previewTable->setColumnCount(1);
            m_previewTable->setHorizontalHeaderItem(0, new QTableWidgetItem("请先添加管脚"));
            m_previewTable->setRowCount(1);
            QTableWidgetItem *item = new QTableWidgetItem("未配置管脚");
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            m_previewTable->setItem(0, 0, item);
        }
    }
}

void AddRowDialog::setupConnections()
{
    // 连接信号与槽
    connect(m_appendToEndCheckBox, &QCheckBox::stateChanged, this, &AddRowDialog::onAppendToEndStateChanged);
    connect(m_addRowButton, &QPushButton::clicked, this, &AddRowDialog::onAddRowButtonClicked);
    connect(m_removeRowButton, &QPushButton::clicked, this, &AddRowDialog::onRemoveRowButtonClicked);
    connect(m_timeSetButton, &QPushButton::clicked, this, &AddRowDialog::onTimeSetButtonClicked);

    // 行数变化时更新剩余可用行数显示
    connect(m_rowCountSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value)
            { updateRemainingRowsDisplay(); });
}

void AddRowDialog::loadTimeSets()
{
    // 加载TimeSet选项
    QSqlDatabase db = DatabaseManager::instance()->database();
    QSqlQuery query(db);

    if (query.exec("SELECT id, timeset_name FROM timeset_list ORDER BY id"))
    {
        m_timeSetComboBox->clear();
        m_timeSetOptions.clear();

        while (query.next())
        {
            int timesetId = query.value(0).toInt();
            QString timesetName = query.value(1).toString();

            m_timeSetOptions[timesetId] = timesetName;
            m_timeSetComboBox->addItem(timesetName, timesetId);
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

    // 添加空单元格
    for (int col = 0; col < m_previewTable->columnCount(); col++)
    {
        QTableWidgetItem *item = new QTableWidgetItem("");
        m_previewTable->setItem(currentRowCount, col, item);
    }
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
    return m_timeSetComboBox->currentData().toInt();
}

QList<QStringList> AddRowDialog::getTableData() const
{
    QList<QStringList> result;

    for (int row = 0; row < m_previewTable->rowCount(); row++)
    {
        QStringList rowData;
        for (int col = 0; col < m_previewTable->columnCount(); col++)
        {
            QTableWidgetItem *item = m_previewTable->item(row, col);
            // 如果单元格为空或内容为空，返回"X"以匹配旧轨道的行为
            rowData.append(item && !item->text().isEmpty() ? item->text() : "X");
        }
        result.append(rowData);
    }

    return result;
}

const QMap<int, QString>& AddRowDialog::getPinOptions() const
{
    return m_pinOptions;
}