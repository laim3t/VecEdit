#pragma once
#include <QTableWidget>
#include <QTableView>
#include "vector/vectortablemodel.h"

class PuppetMasterTableWidget : public QTableWidget
{
    Q_OBJECT
public:
    explicit PuppetMasterTableWidget(VectorDataHandler *dataHandler, QWidget *parent = nullptr);
    VectorTableModel *model() const { return m_model; }
    // 重写关键API，但不使用override关键字，因为QTableWidget中item不是虚函数
    QTableWidgetItem *item(int row, int column) const;
private slots:
    // 双向同步槽函数
    void syncSelectionFromViewToPuppet();
    void syncSelectionFromPuppetToView();
    void syncDataChangeFromModelToPuppet(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    void syncResetFromModelToPuppet();
    void syncItemChangeFromPuppetToModel(QTableWidgetItem *item);

private:
    QTableView *m_view;        // 用户看到的UI
    VectorTableModel *m_model; // 核心数据
    bool m_isSyncing;          // 同步锁
};