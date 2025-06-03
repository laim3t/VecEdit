#ifndef WAVEFORMVIEW_H
#define WAVEFORMVIEW_H

#include <QWidget>
#include <QPainter>
#include <QPaintEvent>
#include <QMap>
#include <QVector>
#include <QString>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QHBoxLayout>

// 波形图类 - 用于显示管脚波形
class WaveformView : public QWidget
{
    Q_OBJECT

public:
    // 波形值类型枚举
    enum class PinValue
    {
        Low = 0,     // 0
        High = 1,    // 1
        Unknown = 2, // X
        HighZ = 3,   // Z
        HighImp = 4  // H
    };

    explicit WaveformView(QWidget *parent = nullptr);
    ~WaveformView();

    // 设置波形数据
    void setPinWaveformData(const QString &pinName, const QVector<PinValue> &values);

    // 清除所有波形数据
    void clearWaveforms();

    // 清除特定管脚的波形数据
    void clearPinWaveform(const QString &pinName);

    // 移除特定管脚
    void removePin(const QString &pinName);

    // 设置时间范围
    void setTimeRange(int startTime, int endTime);

    // 设置网格可见性
    void setGridVisible(bool visible);

    // 设置默认管脚值
    void setDefaultPins(const QStringList &pins);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    // 绘制波形的辅助方法
    void drawWaveform(QPainter &painter, const QString &pinName, const QVector<PinValue> &values, int y);
    void drawTimeAxis(QPainter &painter);
    void drawGrid(QPainter &painter);

    // 管脚波形数据
    QMap<QString, QVector<PinValue>> m_waveformData;

    // 时间范围
    int m_startTime;
    int m_endTime;

    // 视图配置
    bool m_gridVisible;
    int m_waveHeight;      // 每个波形的高度
    int m_timeScaleFactor; // 时间刻度因子

    // 视图计算参数
    void recalculateLayout();
};

// 波形视图容器类 - 包含波形视图和滚动条
class WaveformViewContainer : public QWidget
{
    Q_OBJECT

public:
    explicit WaveformViewContainer(QWidget *parent = nullptr);
    ~WaveformViewContainer();

    // 获取波形视图
    WaveformView *waveformView() const { return m_waveformView; }

private:
    WaveformView *m_waveformView;
    QScrollArea *m_scrollArea;
    QVBoxLayout *m_layout;
};

#endif // WAVEFORMVIEW_H