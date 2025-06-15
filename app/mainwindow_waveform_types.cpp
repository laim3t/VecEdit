// ==========================================================
//  波形图类型相关实现：mainwindow_waveform_types.cpp
// ==========================================================

#include <QDebug>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlDatabase>
#include <QMap>
#include <QPair>

// 获取指定TimeSet ID和管脚ID对应的波形类型和T1F值
bool MainWindow::getWaveTypeAndT1F(int timeSetId, int pinId, int &waveId, double &t1f)
{
    // 初始化返回值
    waveId = 1; // 默认为NRZ (ID=1)
    t1f = 750.0; // 默认T1F值

    // 检查参数有效性
    if (timeSetId <= 0)
    {
        qWarning() << "getWaveTypeAndT1F - 无效的TimeSet ID:" << timeSetId;
        return false;
    }

    // 获取数据库连接
    QSqlDatabase db = DatabaseManager::instance()->database();
    if (!db.isOpen())
    {
        qWarning() << "getWaveTypeAndT1F - 数据库连接失败";
        return false;
    }

    // 查询波形类型和T1F值
    QSqlQuery query(db);
    query.prepare("SELECT wave_id, T1F FROM timeset_settings WHERE timeset_id = ? AND pin_id = ?");
    query.addBindValue(timeSetId);
    query.addBindValue(pinId);

    if (!query.exec())
    {
        qWarning() << "getWaveTypeAndT1F - 查询失败: " << query.lastError().text();
        return false;
    }

    if (query.next())
    {
        waveId = query.value(0).toInt();
        t1f = query.value(1).toDouble();
        qDebug() << "getWaveTypeAndT1F - TimeSet:" << timeSetId << " Pin:" << pinId 
                 << " Wave ID:" << waveId << " T1F:" << t1f;
        return true;
    }
    else
    {
        qWarning() << "getWaveTypeAndT1F - 未找到记录，使用默认值 Wave ID:" << waveId << " T1F:" << t1f;
        return false;
    }
}

// 获取波形类型名称 (NRZ, R0, RZ, SBC)
QString MainWindow::getWaveTypeName(int waveId)
{
    // 波形类型映射
    static QMap<int, QString> waveTypes = {
        {1, "NRZ"},
        {2, "RZ"},
        {3, "R0"},
        {4, "SBC"}
    };

    // 返回波形类型名称
    return waveTypes.value(waveId, "Unknown");
}

// 应用R0和RZ波形效果
void MainWindow::applyWaveformPattern(int timeSetId, int pinId,
                                       QVector<double> &xData, QVector<double> &mainLineData,
                                       double t1rRatio, double period)
{
    // 获取波形类型和T1F值
    int waveId = 1; // 默认NRZ
    double t1f = 750.0; // 默认T1F值
    
    if (!getWaveTypeAndT1F(timeSetId, pinId, waveId, t1f))
    {
        qDebug() << "applyWaveformPattern - 获取波形类型失败，使用默认值 Wave ID:" << waveId;
    }
    
    // 计算T1F作为周期的比例
    double t1fRatio = t1f / period;
    QString waveTypeName = getWaveTypeName(waveId);
    
    qDebug() << "applyWaveformPattern - 应用波形类型:" << waveTypeName << "(ID:" << waveId << ")"
             << " T1F比例:" << t1fRatio << " (T1F=" << t1f << "ns, 周期=" << period << "ns)";
    
    // 如果是默认的NRZ类型，无需特殊处理，直接返回
    if (waveId == 1) // NRZ
    {
        return;
    }
    
    // 定义高低电平值
    const double Y_HIGH_TOP = 20.0; // 高电平值
    const double Y_LOW_BOTTOM = 0.0; // 低电平值
    
    // 检查是否存在R0或RZ波形类型
    if (waveId == 2 || waveId == 3) // RZ(2) 或 R0(3)
    {
        // 清空原有数据点，确保完全重新绘制
        m_r0Points.clear();
        m_rzPoints.clear();
        
        // 完全重置波形数据 - 我们将手动绘制所有线段
        // 将所有的原始数据点设为不可见
        for (int i = 0; i < mainLineData.size(); ++i)
        {
            mainLineData[i] = qQNaN();
        }
        
        // 生成每个周期的波形点
        for (int i = 0; i < xData.size() - 1; i++) // 最后一个点是为了扩展而添加的，不处理
        {
            // 创建一个新的数据点在T1F位置
            double t1fXPos = xData[i] + t1fRatio;
            
            // 如果是有效的周期点
            if (t1fXPos < xData[i+1]) 
            {
                // 标记需要在T1F点添加新的波形处理
                QPair<int, double> t1fPoint;
                t1fPoint.first = i;        // 区间起点
                t1fPoint.second = t1fXPos; // T1F点的X坐标
                
                // 添加到要处理的点列表
                if (waveId == 3) // R0
                {
                    m_r0Points.append(t1fPoint);
                }
                else if (waveId == 2) // RZ
                {
                    m_rzPoints.append(t1fPoint);
                }
            }
        }
        
        qDebug() << "applyWaveformPattern - 生成的波形点数: " 
                 << (waveId == 3 ? m_r0Points.size() : m_rzPoints.size());
    }
}

// 绘制R0和RZ波形效果
void MainWindow::drawWaveformPatterns()
{
    if (!m_waveformPlot) return;
    
    // 定义高低电平值
    const double Y_HIGH_TOP = 20.0;
    const double Y_LOW_BOTTOM = 0.0;
    
    // === 完整重绘R0波形 ===
    if (!m_r0Points.isEmpty())
    {
        QPen wavePen(Qt::red, 2.5);
        qDebug() << "drawWaveformPatterns - 绘制R0波形点：" << m_r0Points.size() << "个";
        
        // 首先绘制第一个点的初始垂直线(如果是第一个点)
        if (!m_r0Points.isEmpty() && m_r0Points.first().first == 0)
        {
            double startX = m_currentXOffset; // 第一个点的X坐标
            QCPItemLine *firstVertLine = new QCPItemLine(m_waveformPlot);
            firstVertLine->setProperty("isR0Line", true);
            firstVertLine->setPen(wavePen);
            firstVertLine->start->setCoords(startX, Y_HIGH_TOP);  // 默认从高电平开始
            firstVertLine->end->setCoords(startX, Y_LOW_BOTTOM);  // 垂直线下拉到低电平
        }
        
        // 按周期顺序完整绘制R0波形
        for (int i = 0; i < m_r0Points.size(); ++i)
        {
            int segmentStart = m_r0Points[i].first;
            double t1fX = m_r0Points[i].second;
            double segmentStartX = segmentStart + m_currentXOffset;
            double segmentEndX = (segmentStart + 1.0) + m_currentXOffset;
            
            qDebug() << "  R0周期 " << i << ": 起点=" << segmentStartX << ", T1F=" << t1fX << ", 终点=" << segmentEndX;
            
            // 1. 从周期起点到T1F点的水平低电平线
            QCPItemLine *initialLowLine = new QCPItemLine(m_waveformPlot);
            initialLowLine->setProperty("isR0Line", true);
            initialLowLine->setPen(wavePen);
            initialLowLine->start->setCoords(segmentStartX, Y_LOW_BOTTOM);
            initialLowLine->end->setCoords(t1fX, Y_LOW_BOTTOM);
            
            // 2. 在T1F点创建一条垂直线，表示检测点状态变化(从低到高)
            QCPItemLine *t1fVertLine = new QCPItemLine(m_waveformPlot);
            t1fVertLine->setProperty("isR0Line", true);
            t1fVertLine->setPen(wavePen);
            t1fVertLine->start->setCoords(t1fX, Y_LOW_BOTTOM);
            t1fVertLine->end->setCoords(t1fX, Y_HIGH_TOP);
            
            // 3. 在T1F点后添加一条水平高电平线，直到周期结束点
            QCPItemLine *highLine = new QCPItemLine(m_waveformPlot);
            highLine->setProperty("isR0Line", true);
            highLine->setPen(wavePen);
            highLine->start->setCoords(t1fX, Y_HIGH_TOP);
            highLine->end->setCoords(segmentEndX, Y_HIGH_TOP);
            
            // 4. 如果不是最后一个周期，绘制周期结束的垂直线
            if (i < m_r0Points.size() - 1)
            {
                double nextStartX = (m_r0Points[i+1].first) + m_currentXOffset;
                
                // 如果下一个周期与当前周期相邻，绘制垂直过渡线
                if (segmentStart + 1 == m_r0Points[i+1].first)
                {
                    QCPItemLine *endVertLine = new QCPItemLine(m_waveformPlot);
                    endVertLine->setProperty("isR0Line", true);
                    endVertLine->setPen(wavePen);
                    endVertLine->start->setCoords(segmentEndX, Y_HIGH_TOP);
                    endVertLine->end->setCoords(segmentEndX, Y_LOW_BOTTOM);
                }
            }
        }
    }
    
    // === 完整重绘RZ波形 ===
    if (!m_rzPoints.isEmpty())
    {
        QPen wavePen(Qt::red, 2.5);
        qDebug() << "drawWaveformPatterns - 绘制RZ波形点：" << m_rzPoints.size() << "个";
        
        // 首先绘制第一个点的初始垂直线(如果是第一个点)
        if (!m_rzPoints.isEmpty() && m_rzPoints.first().first == 0)
        {
            double startX = m_currentXOffset; // 第一个点的X坐标
            QCPItemLine *firstVertLine = new QCPItemLine(m_waveformPlot);
            firstVertLine->setProperty("isRZLine", true);
            firstVertLine->setPen(wavePen);
            firstVertLine->start->setCoords(startX, Y_LOW_BOTTOM);  // 默认从低电平开始
            firstVertLine->end->setCoords(startX, Y_HIGH_TOP);     // 垂直线上拉到高电平
        }
        
        // 按周期顺序完整绘制RZ波形
        for (int i = 0; i < m_rzPoints.size(); ++i)
        {
            int segmentStart = m_rzPoints[i].first;
            double t1fX = m_rzPoints[i].second;
            double segmentStartX = segmentStart + m_currentXOffset;
            double segmentEndX = (segmentStart + 1.0) + m_currentXOffset;
            
            qDebug() << "  RZ周期 " << i << ": 起点=" << segmentStartX << ", T1F=" << t1fX << ", 终点=" << segmentEndX;
            
            // 1. 从周期起点到T1F点的水平高电平线
            QCPItemLine *initialHighLine = new QCPItemLine(m_waveformPlot);
            initialHighLine->setProperty("isRZLine", true);
            initialHighLine->setPen(wavePen);
            initialHighLine->start->setCoords(segmentStartX, Y_HIGH_TOP);
            initialHighLine->end->setCoords(t1fX, Y_HIGH_TOP);
            
            // 2. 在T1F点创建一条垂直线，表示检测点状态变化(从高到低)
            QCPItemLine *t1fVertLine = new QCPItemLine(m_waveformPlot);
            t1fVertLine->setProperty("isRZLine", true);
            t1fVertLine->setPen(wavePen);
            t1fVertLine->start->setCoords(t1fX, Y_HIGH_TOP);
            t1fVertLine->end->setCoords(t1fX, Y_LOW_BOTTOM);
            
            // 3. 在T1F点后添加一条水平低电平线，直到周期结束点
            QCPItemLine *lowLine = new QCPItemLine(m_waveformPlot);
            lowLine->setProperty("isRZLine", true);
            lowLine->setPen(wavePen);
            lowLine->start->setCoords(t1fX, Y_LOW_BOTTOM);
            lowLine->end->setCoords(segmentEndX, Y_LOW_BOTTOM);
            
            // 4. 如果不是最后一个周期，绘制周期结束的垂直线
            if (i < m_rzPoints.size() - 1)
            {
                double nextStartX = (m_rzPoints[i+1].first) + m_currentXOffset;
                
                // 如果下一个周期与当前周期相邻，绘制垂直过渡线
                if (segmentStart + 1 == m_rzPoints[i+1].first)
                {
                    QCPItemLine *endVertLine = new QCPItemLine(m_waveformPlot);
                    endVertLine->setProperty("isRZLine", true);
                    endVertLine->setPen(wavePen);
                    endVertLine->start->setCoords(segmentEndX, Y_LOW_BOTTOM);
                    endVertLine->end->setCoords(segmentEndX, Y_HIGH_TOP);
                }
            }
        }
    }
    
    // 清空临时存储的点
    m_r0Points.clear();
    m_rzPoints.clear();
} 