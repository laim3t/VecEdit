// ==========================================================
//  Headers for: mainwindow_infinite_scroll.cpp
// ==========================================================
#include "mainwindow.h"

// Qt Widgets & Core
#include <QScrollBar>
#include <QTableView>
#include <QDebug>
#include <QElapsedTimer>
#include <QTimer>

// Project-specific headers
#include "vectortablemodel.h"

// 无限滚动相关成员变量 - 声明为全局变量以便在其他文件中访问
namespace InfiniteScroll {
    bool g_isLoadingData = false; // 全局加载状态标志，防止重复触发
    const int SCROLL_THRESHOLD = 50; // 滚动条触发阈值（距离底部的像素数）
    const int PRELOAD_THRESHOLD = 200; // 预加载阈值（距离底部的像素数，应大于SCROLL_THRESHOLD）
    const int DEBOUNCE_TIME_MS = 200; // 防抖间隔时间（毫秒）
    QElapsedTimer g_lastLoadTime; // 最后一次加载时间
    bool g_preloadRequested = false; // 预加载请求标志
}

// 处理表格视图滚动事件
void MainWindow::onTableViewScrolled(int value)
{
    const QString funcName = "MainWindow::onTableViewScrolled";
    
    // 如果正在加载数据，不处理滚动事件
    if (InfiniteScroll::g_isLoadingData)
    {
        qDebug() << funcName << " - 正在加载数据中，忽略滚动事件";
        return;
    }

    // 获取滚动条
    QScrollBar* scrollBar = qobject_cast<QScrollBar*>(sender());
    if (!scrollBar)
    {
        qWarning() << funcName << " - 无法获取滚动条对象";
        return;
    }

    // 获取滚动条的当前位置、最大值和阈值
    int maximum = scrollBar->maximum();
    int loadThreshold = maximum - InfiniteScroll::SCROLL_THRESHOLD;
    int preloadThreshold = maximum - InfiniteScroll::PRELOAD_THRESHOLD;
    
    qDebug() << funcName << " - 滚动位置:" << value << "，最大值:" << maximum 
             << "，加载阈值:" << loadThreshold << "，预加载阈值:" << preloadThreshold
             << "，是否需要加载:" << (value >= loadThreshold);

    // 检查是否已经到达底部（需要立即加载）
    if (value >= loadThreshold)
    {
        // 防抖：检查距离上次加载是否已过足够时间
        if (!InfiniteScroll::g_lastLoadTime.isValid() || InfiniteScroll::g_lastLoadTime.elapsed() > InfiniteScroll::DEBOUNCE_TIME_MS)
        {
            qDebug() << funcName << " - 触发加载更多数据";
            loadMoreDataIfNeeded();
            InfiniteScroll::g_lastLoadTime.restart(); // 重置计时器
            InfiniteScroll::g_preloadRequested = false; // 重置预加载标志
        }
        else
        {
            qDebug() << funcName << " - 防抖期间，忽略加载请求，距上次加载:" << InfiniteScroll::g_lastLoadTime.elapsed() << "ms";
        }
    }
    // 检查是否接近底部（需要预加载）
    else if (value >= preloadThreshold && !InfiniteScroll::g_preloadRequested)
    {
        // 防抖：检查距离上次加载是否已过足够时间
        if (!InfiniteScroll::g_lastLoadTime.isValid() || InfiniteScroll::g_lastLoadTime.elapsed() > InfiniteScroll::DEBOUNCE_TIME_MS * 2)
        {
            qDebug() << funcName << " - 触发预加载数据";
            InfiniteScroll::g_preloadRequested = true; // 设置预加载标志，避免重复预加载
            
            // 使用Qt的单次定时器，延迟一小段时间后加载，避免影响当前滚动性能
            QTimer::singleShot(100, this, [this]() {
                loadMoreDataIfNeeded();
                InfiniteScroll::g_lastLoadTime.restart();
                InfiniteScroll::g_preloadRequested = false;
            });
        }
    }
}

// 根据需要加载更多数据
void MainWindow::loadMoreDataIfNeeded()
{
    const QString funcName = "MainWindow::loadMoreDataIfNeeded";
    
    // 确保当前视图是表格视图
    if (m_vectorStackedWidget->currentWidget() != m_vectorTableView)
    {
        qDebug() << funcName << " - 当前不是表格视图，无需加载";
        return;
    }
    
    // 确保模型存在
    if (!m_vectorTableModel)
    {
        qWarning() << funcName << " - 表格模型未初始化，无法加载更多数据";
        return;
    }
    
    // 获取当前表ID和页码
    int tableId = m_vectorTableModel->getCurrentTableId();
    int currentPage = m_vectorTableModel->currentPage();
    
    qDebug() << funcName << " - 准备加载更多数据，当前表ID:" << tableId 
             << "，当前页码:" << currentPage
             << "，当前行数:" << m_vectorTableModel->rowCount()
             << "，总行数:" << m_vectorTableModel->totalRows();
             
    if (tableId <= 0)
    {
        qWarning() << funcName << " - 无效的表ID";
        return;
    }
    
    // 检查是否已加载全部数据
    if (m_vectorTableModel->rowCount() >= m_vectorTableModel->totalRows())
    {
        qDebug() << funcName << " - 已加载全部数据，无需再加载";
        return;
    }
    
    // 设置加载状态
    InfiniteScroll::g_isLoadingData = true;
    
    // 创建加载进度指示器
    QProgressBar* progressBar = new QProgressBar();
    progressBar->setRange(0, 0); // 设置为不确定模式
    progressBar->setMaximumWidth(200);
    progressBar->setTextVisible(true);
    progressBar->setFormat(tr("正在加载第 %1 页...").arg(currentPage + 1));
    
    // 将进度条添加到状态栏
    statusBar()->addWidget(progressBar);
    statusBar()->showMessage(tr("正在加载数据..."));
    
    // 异步加载下一页数据
    QApplication::setOverrideCursor(Qt::WaitCursor);
    
    // 调用模型的appendPage方法
    bool success = m_vectorTableModel->appendPage();
    
    // 恢复光标并显示结果
    QApplication::restoreOverrideCursor();
    
    // 移除进度条
    statusBar()->removeWidget(progressBar);
    delete progressBar;
    
    if (success)
    {
        int newPage = m_vectorTableModel->currentPage();
        int rowCount = m_vectorTableModel->rowCount();
        int totalRows = m_vectorTableModel->totalRows();
        
        // 计算加载进度百分比
        double progressPercent = static_cast<double>(rowCount) * 100.0 / totalRows;
        
        // 格式化显示，包含进度百分比
        QString successMsg = tr("加载完成 - 页: %1，已加载: %2/%3 行 (%.2f%%)").arg(newPage).arg(rowCount).arg(totalRows).arg(progressPercent);
        statusBar()->showMessage(successMsg);
        qDebug() << funcName << " - 成功追加一页数据，当前页:" << newPage
                 << "，当前行数:" << rowCount << "，总行数:" << totalRows
                 << "，加载进度:" << QString::number(progressPercent, 'f', 2) << "%";
                 
        // 如果接近底部，预加载下一页
        if (rowCount > 0 && rowCount < totalRows && 
            static_cast<double>(rowCount) / totalRows > 0.9)
        {
            qDebug() << funcName << " - 接近数据底部，考虑预加载下一页";
        }
    }
    else
    {
        statusBar()->showMessage(tr("没有更多数据可加载"));
        qDebug() << funcName << " - 没有更多数据可加载";
    }
    
    // 重置加载状态
    InfiniteScroll::g_isLoadingData = false;
}