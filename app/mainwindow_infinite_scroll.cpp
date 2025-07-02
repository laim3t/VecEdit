// ==========================================================
//  Headers for: mainwindow_infinite_scroll.cpp
// ==========================================================
#include "mainwindow.h"

// Qt Widgets & Core
#include <QScrollBar>
#include <QTableView>
#include <QDebug>
#include <QElapsedTimer>

// Project-specific headers
#include "vectortablemodel.h"

// 无限滚动相关成员变量 - 声明为全局变量以便在其他文件中访问
namespace InfiniteScroll {
    bool g_isLoadingData = false; // 全局加载状态标志，防止重复触发
    const int SCROLL_THRESHOLD = 50; // 滚动条触发阈值（距离底部的像素数）
    const int DEBOUNCE_TIME_MS = 200; // 防抖间隔时间（毫秒）
    QElapsedTimer g_lastLoadTime; // 最后一次加载时间
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
    int threshold = maximum - InfiniteScroll::SCROLL_THRESHOLD;
    
    qDebug() << funcName << " - 滚动位置:" << value << "，最大值:" << maximum 
             << "，阈值:" << threshold << "，是否需要加载:" << (value >= threshold);

    // 检查是否接近底部
    if (value >= threshold)
    {
        // 防抖：检查距离上次加载是否已过足够时间
        if (!InfiniteScroll::g_lastLoadTime.isValid() || InfiniteScroll::g_lastLoadTime.elapsed() > InfiniteScroll::DEBOUNCE_TIME_MS)
        {
            qDebug() << funcName << " - 触发加载更多数据";
            loadMoreDataIfNeeded();
            InfiniteScroll::g_lastLoadTime.restart(); // 重置计时器
        }
        else
        {
            qDebug() << funcName << " - 防抖期间，忽略加载请求，距上次加载:" << InfiniteScroll::g_lastLoadTime.elapsed() << "ms";
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
    
    // 显示加载中状态
    QString loadingMsg = tr("正在加载第 %1 页数据...").arg(currentPage + 1);
    statusBar()->showMessage(loadingMsg);
    
    // 异步加载下一页数据
    QApplication::setOverrideCursor(Qt::WaitCursor);
    
    // 调用模型的appendPage方法
    bool success = m_vectorTableModel->appendPage();
    
    // 恢复光标并显示结果
    QApplication::restoreOverrideCursor();
    
    if (success)
    {
        int newPage = m_vectorTableModel->currentPage();
        int rowCount = m_vectorTableModel->rowCount();
        int totalRows = m_vectorTableModel->totalRows();
        
        QString successMsg = tr("加载完成 - 当前页: %1，已加载: %2/%3 行").arg(newPage).arg(rowCount).arg(totalRows);
        statusBar()->showMessage(successMsg);
        
        qDebug() << funcName << " - 成功追加一页数据，当前页:" << newPage
                 << "，当前行数:" << rowCount << "，总行数:" << totalRows
                 << "，加载进度:" << (rowCount * 100 / totalRows) << "%";
                 
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