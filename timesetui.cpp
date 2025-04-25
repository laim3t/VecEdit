#include "timesetui.h"

TimeSetUIManager::TimeSetUIManager(QDialog *dialog) : m_dialog(dialog)
{
    setupMainLayout();
    setupTreeWidget();
    setupPinSelection();
    setupButtonBox();
}

void TimeSetUIManager::setupMainLayout()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(m_dialog);

    mainSplitter = new QSplitter(Qt::Horizontal, m_dialog);
    mainLayout->addWidget(mainSplitter);

    // 左侧为TimeSet树
    QWidget *leftWidget = new QWidget(mainSplitter);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    // TimeSet树
    timeSetTree = new QTreeWidget(leftWidget);
    leftLayout->addWidget(timeSetTree);

    // TimeSet操作按钮
    QHBoxLayout *timeSetButtonLayout = new QHBoxLayout();
    addTimeSetButton = new QPushButton("添加TimeSet", leftWidget);
    removeTimeSetButton = new QPushButton("删除TimeSet", leftWidget);
    timeSetButtonLayout->addWidget(addTimeSetButton);
    timeSetButtonLayout->addWidget(removeTimeSetButton);
    leftLayout->addLayout(timeSetButtonLayout);

    // 边沿操作按钮
    QHBoxLayout *edgeButtonLayout = new QHBoxLayout();
    addEdgeButton = new QPushButton("添加边沿参数", leftWidget);
    removeEdgeButton = new QPushButton("删除边沿参数", leftWidget);
    edgeButtonLayout->addWidget(addEdgeButton);
    edgeButtonLayout->addWidget(removeEdgeButton);
    leftLayout->addLayout(edgeButtonLayout);

    // 右侧显示所有可用管脚
    QWidget *rightWidget = new QWidget(mainSplitter);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(10, 10, 10, 10);

    // 创建标题
    QLabel *availablePinsLabel = new QLabel("可用管脚列表", rightWidget);
    QFont titleFont = availablePinsLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    availablePinsLabel->setFont(titleFont);
    availablePinsLabel->setAlignment(Qt::AlignCenter);
    rightLayout->addWidget(availablePinsLabel);
    rightLayout->addSpacing(10);

    // 创建管脚列表显示
    availablePinsList = new QListWidget(rightWidget);
    availablePinsList->setAlternatingRowColors(true);
    rightLayout->addWidget(availablePinsList);

    // 添加说明文本
    QLabel *hintLabel = new QLabel("双击TimeSet名称可修改名称和周期", rightWidget);
    hintLabel->setAlignment(Qt::AlignCenter);
    rightLayout->addWidget(hintLabel);

    // 添加左右分区到分割器
    mainSplitter->addWidget(leftWidget);
    mainSplitter->addWidget(rightWidget);
    mainSplitter->setStretchFactor(0, 2);
    mainSplitter->setStretchFactor(1, 1);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, m_dialog);
    mainLayout->addWidget(buttonBox);
}

void TimeSetUIManager::setupTreeWidget()
{
    timeSetTree->setHeaderLabels(QStringList() << "名称/周期(单位)"
                                               << "T1R"
                                               << "T1F"
                                               << "STBR"
                                               << "WAVE");
    timeSetTree->setColumnCount(5);
    timeSetTree->setAlternatingRowColors(true);
    timeSetTree->setSelectionMode(QAbstractItemView::SingleSelection);

    // 设置列宽更合理
    timeSetTree->header()->setSectionResizeMode(0, QHeaderView::Interactive); // 改为Interactive模式允许用户调整大小
    for (int i = 1; i < 5; i++)
    {
        timeSetTree->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
        // 设置最小列宽，保证数据显示完整
        timeSetTree->setColumnWidth(i, 80);
    }

    // 设置第一列的最小宽度，确保能显示完整的timeSet名称和周期信息
    timeSetTree->setColumnWidth(0, 280);

    // 设置更大的行高
    timeSetTree->setStyleSheet("QTreeWidget::item { height: 25px; }");

    // 设置表头样式
    QFont headerFont = timeSetTree->header()->font();
    headerFont.setBold(true);
    timeSetTree->header()->setFont(headerFont);

    // 设置表头对齐方式
    timeSetTree->headerItem()->setTextAlignment(1, Qt::AlignCenter);
    timeSetTree->headerItem()->setTextAlignment(2, Qt::AlignCenter);
    timeSetTree->headerItem()->setTextAlignment(3, Qt::AlignCenter);
    timeSetTree->headerItem()->setTextAlignment(4, Qt::AlignCenter);

    // 启用编辑功能
    timeSetTree->setEditTriggers(QAbstractItemView::DoubleClicked |
                                 QAbstractItemView::EditKeyPressed);

    // 设置所有列可编辑
    timeSetTree->setItemsExpandable(true);
}

void TimeSetUIManager::setupPinSelection()
{
    // 不需要任何操作，因为我们已经在setupMainLayout中创建了可用管脚列表
}

void TimeSetUIManager::setupButtonBox()
{
    // 初始禁用按钮，直到有TimeSet被选中
    addEdgeButton->setEnabled(false);
    removeEdgeButton->setEnabled(false);
}