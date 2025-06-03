#include "waveformview.h"
#include <QPainter>
#include <QResizeEvent>
#include <QDebug>
#include <QApplication>
#include <QStyle>

// WaveformView 实现
WaveformView::WaveformView(QWidget *parent)
    : QWidget(parent),
      m_startTime(0),
      m_endTime(50),
      m_gridVisible(true),
      m_waveHeight(30),
      m_timeScaleFactor(10)
{
    // 设置视图属性
    setMinimumHeight(100);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // 设置背景色
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(240, 240, 240));
    setAutoFillBackground(true);
    setPalette(pal);

    // 使窗口可以接收焦点
    setFocusPolicy(Qt::StrongFocus);
}

WaveformView::~WaveformView()
{
}

void WaveformView::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 绘制背景
    painter.fillRect(event->rect(), QColor(240, 240, 240));

    // 绘制网格
    if (m_gridVisible)
    {
        drawGrid(painter);
    }

    // 绘制时间轴
    drawTimeAxis(painter);

    // 绘制每个管脚的波形
    int yPos = 40; // 起始Y位置，预留顶部空间给时间轴

    for (auto it = m_waveformData.begin(); it != m_waveformData.end(); ++it)
    {
        // 绘制管脚名称
        painter.setPen(Qt::black);
        painter.drawText(5, yPos + m_waveHeight / 2, it.key());

        // 绘制波形
        drawWaveform(painter, it.key(), it.value(), yPos);

        // 下一个波形的Y位置
        yPos += m_waveHeight + 10;
    }
}

void WaveformView::drawWaveform(QPainter &painter, const QString &pinName, const QVector<PinValue> &values, int y)
{
    const int xStart = 80; // 留出左侧空间显示管脚名称
    const int width = (width() - xStart) / values.size();
    const int height = m_waveHeight;

    // 设置画笔
    QPen wavePen(Qt::darkBlue, 2);
    painter.setPen(wavePen);

    // 绘制波形
    int x = xStart;
    int lastY = 0;

    for (int i = 0; i < values.size(); ++i)
    {
        int value = static_cast<int>(values[i]);
        int waveY = 0;

        switch (values[i])
        {
        case PinValue::Low: // 0
            waveY = y + height - 5;
            painter.drawLine(x, waveY, x + width, waveY);
            break;
        case PinValue::High: // 1
            waveY = y + 5;
            painter.drawLine(x, waveY, x + width, waveY);
            break;
        case PinValue::Unknown: // X
            waveY = y + height / 2;
            painter.drawText(x + width / 2 - 5, waveY + 5, "X");
            break;
        case PinValue::HighZ: // Z
            waveY = y + height / 2;
            painter.drawText(x + width / 2 - 5, waveY + 5, "Z");
            break;
        case PinValue::HighImp: // H
            waveY = y + 5;
            painter.drawLine(x, waveY, x + width, waveY);
            painter.drawText(x + width / 2 - 5, waveY - 5, "H");
            break;
        }

        // 如果有前一个值，并且与当前值不同，绘制一条垂直线表示变化
        if (i > 0 && lastY != waveY)
        {
            painter.drawLine(x, lastY, x, waveY);
        }

        lastY = waveY;
        x += width;
    }
}

void WaveformView::drawTimeAxis(QPainter &painter)
{
    const int xStart = 80; // 留出左侧空间
    const int yAxis = 20;
    const int width = this->width() - xStart;

    // 设置画笔
    QPen axisPen(Qt::black, 1);
    painter.setPen(axisPen);

    // 绘制水平时间轴线
    painter.drawLine(xStart, yAxis, xStart + width, yAxis);

    // 绘制刻度线和数字
    const int timeRange = m_endTime - m_startTime;
    const int tickCount = 10; // 时间轴上的刻度数量

    for (int i = 0; i <= tickCount; ++i)
    {
        int x = xStart + (width * i) / tickCount;
        int time = m_startTime + (timeRange * i) / tickCount;

        // 绘制刻度线
        painter.drawLine(x, yAxis - 3, x, yAxis + 3);

        // 绘制时间值
        painter.drawText(x - 10, yAxis - 5, QString::number(time));
    }
}

void WaveformView::drawGrid(QPainter &painter)
{
    const int xStart = 80; // 留出左侧空间
    const int width = this->width() - xStart;
    const int height = this->height();

    // 设置网格画笔
    QPen gridPen(QColor(200, 200, 200), 1, Qt::DotLine);
    painter.setPen(gridPen);

    // 绘制垂直网格线
    const int timeRange = m_endTime - m_startTime;
    const int gridCount = 20; // 网格数量

    for (int i = 0; i <= gridCount; ++i)
    {
        int x = xStart + (width * i) / gridCount;
        painter.drawLine(x, 30, x, height);
    }

    // 绘制水平网格线
    int yPos = 40; // 起始Y位置

    for (auto it = m_waveformData.begin(); it != m_waveformData.end(); ++it)
    {
        painter.drawLine(0, yPos, width() - 1, yPos);                               // 上边界
        painter.drawLine(0, yPos + m_waveHeight, width() - 1, yPos + m_waveHeight); // 下边界
        yPos += m_waveHeight + 10;
    }
}

void WaveformView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    recalculateLayout();
}

void WaveformView::recalculateLayout()
{
    // 根据当前大小调整视图参数
    if (m_waveformData.isEmpty())
        return;

    // 确保高度足够容纳所有波形
    int minHeight = 40 + (m_waveHeight + 10) * m_waveformData.size();

    if (height() < minHeight)
    {
        setMinimumHeight(minHeight);
    }
}

void WaveformView::setPinWaveformData(const QString &pinName, const QVector<PinValue> &values)
{
    m_waveformData[pinName] = values;
    recalculateLayout();
    update();
}

void WaveformView::clearWaveforms()
{
    m_waveformData.clear();
    update();
}

void WaveformView::clearPinWaveform(const QString &pinName)
{
    if (m_waveformData.contains(pinName))
    {
        m_waveformData[pinName].clear();
        update();
    }
}

void WaveformView::removePin(const QString &pinName)
{
    if (m_waveformData.contains(pinName))
    {
        m_waveformData.remove(pinName);
        recalculateLayout();
        update();
    }
}

void WaveformView::setTimeRange(int startTime, int endTime)
{
    if (startTime < endTime)
    {
        m_startTime = startTime;
        m_endTime = endTime;
        update();
    }
}

void WaveformView::setGridVisible(bool visible)
{
    m_gridVisible = visible;
    update();
}

void WaveformView::setDefaultPins(const QStringList &pins)
{
    // 为每个管脚创建默认空波形
    foreach (const QString &pin, pins)
    {
        if (!m_waveformData.contains(pin))
        {
            QVector<PinValue> emptyWave;
            m_waveformData[pin] = emptyWave;
        }
    }

    recalculateLayout();
    update();
}

// WaveformViewContainer 实现
WaveformViewContainer::WaveformViewContainer(QWidget *parent)
    : QWidget(parent)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);

    // 创建滚动区域
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // 创建波形视图
    m_waveformView = new WaveformView(m_scrollArea);
    m_scrollArea->setWidget(m_waveformView);

    m_layout->addWidget(m_scrollArea);

    // 设置最小大小
    setMinimumHeight(150);
}

WaveformViewContainer::~WaveformViewContainer()
{
}