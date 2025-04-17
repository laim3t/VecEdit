#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUI();

    // 设置窗口标题和大小
    setWindowTitle("VecEdit - 向量编辑器");
    resize(1024, 768);
}

MainWindow::~MainWindow()
{
    // Qt对象会自动清理
}

void MainWindow::setupUI()
{
    // 创建场景和视图
    scene = new QGraphicsScene(this);
    scene->setSceneRect(0, 0, 2000, 2000);

    view = new QGraphicsView(scene, this);
    view->setRenderHint(QPainter::Antialiasing);
    view->setDragMode(QGraphicsView::RubberBandDrag);

    // 设置中心部件
    setCentralWidget(view);
}
