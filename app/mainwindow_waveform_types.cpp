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
    
    // 检查是否存在特殊波形类型 (RZ, R0, SBC)
    if (waveId == 2 || waveId == 3 || waveId == 4) // RZ(2) 或 R0(3) 或 SBC(4)
    {
        // 清空原有数据点，确保完全重新绘制
        m_r0Points.clear();
        m_rzPoints.clear();
        m_sbcPoints.clear();
        
        // 为R0/RZ/SBC波形捕获原始电平状态
        QVector<bool> originalStates;
        
        // 计算管脚的高低电平Y坐标
        double highY = 0.0, lowY = 0.0;
        
        // 从mainLineData中找出高低电平的Y坐标值
        // 注意：我们需要确保highY是最大值，lowY是最小值
        bool foundValidY = false;
        for (int i = 0; i < mainLineData.size(); ++i) {
            if (!qIsNaN(mainLineData[i])) {
                if (!foundValidY) {
                    highY = mainLineData[i];
                    lowY = mainLineData[i];
                    foundValidY = true;
                } else {
                    if (mainLineData[i] > highY) {
                        highY = mainLineData[i];
                    }
                    if (mainLineData[i] < lowY) {
                        lowY = mainLineData[i];
                    }
                }
            }
        }
        
        // 如果没有找到有效的Y坐标，使用默认值
        if (!foundValidY) {
            // 使用绝对默认值
            // 注意：这里确保lowY是底部值，highY是顶部值
            const int pinIndex = (pinId - 1) % 10; // 获取管脚索引，限制在0-9
            const double PIN_HEIGHT = 25.0; // 每个管脚波形的高度
            const double PIN_GAP = 10.0;    // 管脚之间的间隔
            
            // 计算当前管脚的Y轴位置
            double pinBaseY = pinIndex * (PIN_HEIGHT + PIN_GAP); // 当前管脚的基准Y坐标
            highY = pinBaseY + PIN_HEIGHT; // 高电平Y坐标
            lowY = pinBaseY; // 低电平Y坐标
            
            qDebug() << "applyWaveformPattern - 使用默认Y坐标 - 管脚ID:" << pinId 
                     << " 名称:" << getPinNameById(pinId)
                     << " 高电平Y:" << highY << " 低电平Y:" << lowY;
        }
        
        // 捕获原始电平状态
        if (waveId == 3 || waveId == 2 || waveId == 4) {
            for (int i = 0; i < mainLineData.size(); ++i) {
                // 判断原始电平是高(1)还是低(0)
                // 使用中点作为阈值
                double midY = (highY + lowY) / 2.0;
                originalStates.append(!qIsNaN(mainLineData[i]) && mainLineData[i] > midY);
            }
        }

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
                // 添加到要处理的点列表
                if (waveId == 3) // R0
                {
                    R0WavePoint r0Point;
                    r0Point.cycleIndex = i;
                    r0Point.t1fXPos = t1fXPos;
                    r0Point.isOne = (i < originalStates.size()) ? originalStates[i] : false;
                    r0Point.highY = highY;
                    r0Point.lowY = lowY;
                    r0Point.pinId = pinId;
                    r0Point.pinName = getPinNameById(pinId);
                    m_r0Points.append(r0Point);
                }
                else if (waveId == 2) // RZ
                {
                    RZWavePoint rzPoint;
                    rzPoint.cycleIndex = i;
                    rzPoint.t1fXPos = t1fXPos;
                    rzPoint.isOne = (i < originalStates.size()) ? originalStates[i] : false;
                    rzPoint.highY = highY;
                    rzPoint.lowY = lowY;
                    rzPoint.pinId = pinId;
                    rzPoint.pinName = getPinNameById(pinId);
                    m_rzPoints.append(rzPoint);
                }
                else if (waveId == 4) // SBC
                {
                    SbcWavePoint sbcPoint;
                    sbcPoint.cycleIndex = i;
                    sbcPoint.t1fXPos = t1fXPos;
                    sbcPoint.isOne = (i < originalStates.size()) ? originalStates[i] : false;
                    sbcPoint.highY = highY;
                    sbcPoint.lowY = lowY;
                    sbcPoint.pinId = pinId;
                    sbcPoint.pinName = getPinNameById(pinId);
                    m_sbcPoints.append(sbcPoint);
                }
            }
        }
        
        // 输出调试信息
        QString pointCountMsg;
        if (waveId == 3)
            pointCountMsg = QString("R0波形点数: %1").arg(m_r0Points.size());
        else if (waveId == 2)
            pointCountMsg = QString("RZ波形点数: %1").arg(m_rzPoints.size());
        else if (waveId == 4)
            pointCountMsg = QString("SBC波形点数: %1").arg(m_sbcPoints.size());
            
        qDebug() << "applyWaveformPattern - 生成的" << pointCountMsg;
    }
}

// 绘制R0和RZ波形效果
void MainWindow::drawWaveformPatterns()
{
    if (!m_waveformPlot) return;
    
    // === 完整重绘R0波形 ===
    if (!m_r0Points.isEmpty())
    {
        // 按管脚ID对波形点进行分组
        QMap<int, QList<R0WavePoint>> pointsByPin;
        for (const auto& point : m_r0Points) {
            pointsByPin[point.pinId].append(point);
        }
        
        QPen wavePen(Qt::red, 2.5);
        qDebug() << "drawWaveformPatterns - 绘制R0波形点：" << m_r0Points.size() 
                 << "个，分布在" << pointsByPin.size() << "个管脚上 (已修正逻辑)";

        // 为每个管脚分别绘制波形
        QMapIterator<int, QList<R0WavePoint>> it(pointsByPin);
        while (it.hasNext()) {
            it.next();
            int pinId = it.key();
            const QList<R0WavePoint>& pinPoints = it.value();
            
            if (pinPoints.isEmpty()) continue;
            
            qDebug() << "  绘制管脚ID:" << pinId << " 名称:" << pinPoints.first().pinName 
                     << " 波形点数:" << pinPoints.size();
            
            // 按周期顺序完整绘制该管脚的R0波形
            for (int i = 0; i < pinPoints.size(); ++i)
            {
                const auto& point = pinPoints[i];
                double segmentStartX = point.cycleIndex + m_currentXOffset;
                double segmentEndX = (point.cycleIndex + 1.0) + m_currentXOffset;
                double t1fX = point.t1fXPos;
                double highY = point.highY;
                double lowY = point.lowY;

                qDebug() << "  R0周期 " << point.cycleIndex << ": 原始值=" << (point.isOne ? "1" : "0");

                if (point.isOne)
                {
                    // 原始值为'1', 整个周期保持高电平
                    QCPItemLine *highLine = new QCPItemLine(m_waveformPlot);
                    highLine->setProperty("isR0Line", true);
                    highLine->setPen(wavePen);
                    highLine->start->setCoords(segmentStartX, highY);
                    highLine->end->setCoords(segmentEndX, highY);
                }
                else
                {
                    // 原始值为'0', 执行"归1"操作
                    // 1. 从周期起点到T1F点的水平低电平线
                    QCPItemLine *initialLowLine = new QCPItemLine(m_waveformPlot);
                    initialLowLine->setProperty("isR0Line", true);
                    initialLowLine->setPen(wavePen);
                    initialLowLine->start->setCoords(segmentStartX, lowY);
                    initialLowLine->end->setCoords(t1fX, lowY);

                    // 2. 在T1F点创建一条垂直线 (低 -> 高)
                    QCPItemLine *t1fVertLine = new QCPItemLine(m_waveformPlot);
                    t1fVertLine->setProperty("isR0Line", true);
                    t1fVertLine->setPen(wavePen);
                    t1fVertLine->start->setCoords(t1fX, lowY);
                    t1fVertLine->end->setCoords(t1fX, highY);

                    // 3. 从T1F点到周期结束的水平高电平线
                    QCPItemLine *highLine = new QCPItemLine(m_waveformPlot);
                    highLine->setProperty("isR0Line", true);
                    highLine->setPen(wavePen);
                    highLine->start->setCoords(t1fX, highY);
                    highLine->end->setCoords(segmentEndX, highY);
                }

                // 绘制周期之间的过渡垂直线
                if (i < pinPoints.size() - 1)
                {
                    const auto& nextPoint = pinPoints[i+1];
                    // 仅在两个连续的周期之间绘制
                    if (point.cycleIndex + 1 == nextPoint.cycleIndex)
                    {
                        // 当前R0周期结束时总是高电平。如果下一个周期从低电平开始（即输入为0），则绘制垂直下降沿
                        if (!nextPoint.isOne)
                        {
                            QCPItemLine *endVertLine = new QCPItemLine(m_waveformPlot);
                            endVertLine->setProperty("isR0Line", true);
                            endVertLine->setPen(wavePen);
                            endVertLine->start->setCoords(segmentEndX, highY);
                            endVertLine->end->setCoords(segmentEndX, nextPoint.lowY);
                        }
                    }
                }
            }
        }
    }
    
    // === 完整重绘RZ波形 ===
    if (!m_rzPoints.isEmpty())
    {
        // 按管脚ID对波形点进行分组
        QMap<int, QList<RZWavePoint>> pointsByPin;
        for (const auto& point : m_rzPoints) {
            pointsByPin[point.pinId].append(point);
        }
        
        QPen wavePen(Qt::red, 2.5);
        qDebug() << "drawWaveformPatterns - 绘制RZ波形点：" << m_rzPoints.size() 
                 << "个，分布在" << pointsByPin.size() << "个管脚上 (已修正逻辑)";

        // 为每个管脚分别绘制波形
        QMapIterator<int, QList<RZWavePoint>> it(pointsByPin);
        while (it.hasNext()) {
            it.next();
            int pinId = it.key();
            const QList<RZWavePoint>& pinPoints = it.value();
            
            if (pinPoints.isEmpty()) continue;
            
            qDebug() << "  绘制管脚ID:" << pinId << " 名称:" << pinPoints.first().pinName 
                     << " 波形点数:" << pinPoints.size();
            
            // 按周期顺序完整绘制该管脚的RZ波形
            for (int i = 0; i < pinPoints.size(); ++i)
            {
                const auto& point = pinPoints[i];
                double segmentStartX = point.cycleIndex + m_currentXOffset;
                double segmentEndX = (point.cycleIndex + 1.0) + m_currentXOffset;
                double t1fX = point.t1fXPos;
                double highY = point.highY;
                double lowY = point.lowY;

                qDebug() << "  RZ周期 " << point.cycleIndex << ": 原始值=" << (point.isOne ? "1" : "0");

                if (point.isOne)
                {
                    // 原始值为'1', 执行"归0"操作
                    // 1. 从周期起点到T1F点是高电平
                    QCPItemLine *initialHighLine = new QCPItemLine(m_waveformPlot);
                    initialHighLine->setProperty("isRZLine", true);
                    initialHighLine->setPen(wavePen);
                    initialHighLine->start->setCoords(segmentStartX, highY);
                    initialHighLine->end->setCoords(t1fX, highY);

                    // 2. 在T1F点创建垂直线 (高 -> 低)
                    QCPItemLine *t1fVertLine = new QCPItemLine(m_waveformPlot);
                    t1fVertLine->setProperty("isRZLine", true);
                    t1fVertLine->setPen(wavePen);
                    t1fVertLine->start->setCoords(t1fX, highY);
                    t1fVertLine->end->setCoords(t1fX, lowY);

                    // 3. T1F点到周期结束是低电平
                    QCPItemLine *lowLine = new QCPItemLine(m_waveformPlot);
                    lowLine->setProperty("isRZLine", true);
                    lowLine->setPen(wavePen);
                    lowLine->start->setCoords(t1fX, lowY);
                    lowLine->end->setCoords(segmentEndX, lowY);
                }
                else
                {
                    // 原始值为'0', 整个周期保持低电平
                    QCPItemLine *lowLine = new QCPItemLine(m_waveformPlot);
                    lowLine->setProperty("isRZLine", true);
                    lowLine->setPen(wavePen);
                    lowLine->start->setCoords(segmentStartX, lowY);
                    lowLine->end->setCoords(segmentEndX, lowY);
                }

                // 绘制周期之间的过渡垂直线
                if (i < pinPoints.size() - 1)
                {
                    const auto& nextPoint = pinPoints[i+1];
                    if (point.cycleIndex + 1 == nextPoint.cycleIndex)
                    {
                        // 当前RZ周期结束时总是低电平。如果下一个周期从高电平开始（即输入为1），则绘制垂直上升沿
                        if (nextPoint.isOne)
                        {
                            QCPItemLine *endVertLine = new QCPItemLine(m_waveformPlot);
                            endVertLine->setProperty("isRZLine", true);
                            endVertLine->setPen(wavePen);
                            endVertLine->start->setCoords(segmentEndX, lowY);
                            endVertLine->end->setCoords(segmentEndX, nextPoint.highY);
                        }
                    }
                }
            }
        }
    }
    
    // === 完整重绘SBC波形 ===
    if (!m_sbcPoints.isEmpty())
    {
        // 按管脚ID对波形点进行分组
        QMap<int, QList<SbcWavePoint>> pointsByPin;
        for (const auto& point : m_sbcPoints) {
            pointsByPin[point.pinId].append(point);
        }
        
        QPen wavePen(Qt::red, 2.5);
        qDebug() << "drawWaveformPatterns - 绘制SBC波形点：" << m_sbcPoints.size() 
                 << "个，分布在" << pointsByPin.size() << "个管脚上 (已修正逻辑)";

        // 为每个管脚分别绘制波形
        QMapIterator<int, QList<SbcWavePoint>> it(pointsByPin);
        while (it.hasNext()) {
            it.next();
            int pinId = it.key();
            const QList<SbcWavePoint>& pinPoints = it.value();
            
            if (pinPoints.isEmpty()) continue;
            
            qDebug() << "  绘制管脚ID:" << pinId << " 名称:" << pinPoints.first().pinName 
                     << " 波形点数:" << pinPoints.size();
            
            // 按周期顺序完整绘制该管脚的SBC波形
            for (int i = 0; i < pinPoints.size(); ++i)
            {
                const auto& point = pinPoints[i];
                double segmentStartX = point.cycleIndex + m_currentXOffset;
                double segmentEndX = (point.cycleIndex + 1.0) + m_currentXOffset;
                double t1fX = point.t1fXPos;
                double highY = point.highY;
                double lowY = point.lowY;

                qDebug() << "  SBC周期 " << point.cycleIndex << ": 原始值=" << (point.isOne ? "1" : "0");

                if (point.isOne) // 输入为'1', 归0 (高 -> 低)
                {
                    QCPItemLine *highLine = new QCPItemLine(m_waveformPlot);
                    highLine->setProperty("isSBCLine", true);
                    highLine->setPen(wavePen);
                    highLine->start->setCoords(segmentStartX, highY);
                    highLine->end->setCoords(t1fX, highY);

                    QCPItemLine *vertLine = new QCPItemLine(m_waveformPlot);
                    vertLine->setProperty("isSBCLine", true);
                    vertLine->setPen(wavePen);
                    vertLine->start->setCoords(t1fX, highY);
                    vertLine->end->setCoords(t1fX, lowY);

                    QCPItemLine *lowLine = new QCPItemLine(m_waveformPlot);
                    lowLine->setProperty("isSBCLine", true);
                    lowLine->setPen(wavePen);
                    lowLine->start->setCoords(t1fX, lowY);
                    lowLine->end->setCoords(segmentEndX, lowY);
                }
                else // 输入为'0', 归1 (低 -> 高)
                {
                    QCPItemLine *lowLine = new QCPItemLine(m_waveformPlot);
                    lowLine->setProperty("isSBCLine", true);
                    lowLine->setPen(wavePen);
                    lowLine->start->setCoords(segmentStartX, lowY);
                    lowLine->end->setCoords(t1fX, lowY);

                    QCPItemLine *vertLine = new QCPItemLine(m_waveformPlot);
                    vertLine->setProperty("isSBCLine", true);
                    vertLine->setPen(wavePen);
                    vertLine->start->setCoords(t1fX, lowY);
                    vertLine->end->setCoords(t1fX, highY);

                    QCPItemLine *highLine = new QCPItemLine(m_waveformPlot);
                    highLine->setProperty("isSBCLine", true);
                    highLine->setPen(wavePen);
                    highLine->start->setCoords(t1fX, highY);
                    highLine->end->setCoords(segmentEndX, highY);
                }

                // 绘制周期之间的过渡垂直线
                if (i < pinPoints.size() - 1)
                {
                    const auto& nextPoint = pinPoints[i+1];
                    if (point.cycleIndex + 1 == nextPoint.cycleIndex)
                    {
                        double currentCycleEndY = point.isOne ? lowY : highY;
                        double nextCycleStartY = nextPoint.isOne ? nextPoint.highY : nextPoint.lowY;

                        if (currentCycleEndY != nextCycleStartY)
                        {
                             QCPItemLine *endVertLine = new QCPItemLine(m_waveformPlot);
                             endVertLine->setProperty("isSBCLine", true);
                             endVertLine->setPen(wavePen);
                             endVertLine->start->setCoords(segmentEndX, currentCycleEndY);
                             endVertLine->end->setCoords(segmentEndX, nextCycleStartY);
                        }
                    }
                }
            }
        }
    }
    
    // 清空临时存储的点
    m_r0Points.clear();
    m_rzPoints.clear();
    m_sbcPoints.clear();
}