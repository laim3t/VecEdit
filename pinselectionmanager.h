#ifndef PINSELECTIONMANAGER_H
#define PINSELECTIONMANAGER_H

#include <QListWidget>
#include <QDialog>
#include <QTableWidget>
#include "timesetdataaccess.h"

class PinSelectionManager
{
public:
    PinSelectionManager(QListWidget *pinListWidget, TimeSetDataAccess *dataAccess);

    // 管脚选择相关方法
    void populatePinList(const QMap<int, QString> &pinList);
    void selectPinsForTimeSet(int timeSetId);
    QList<int> getSelectedPinIds() const;

    // 对话框显示方法
    void showPinSelectionDialog(int tableId, const QString &tableName, QWidget *parent);
    void showPinSelectionDialogStandalone(int tableId, const QString &tableName, QWidget *parent);

private:
    QListWidget *m_pinListWidget;
    TimeSetDataAccess *m_dataAccess;
};

#endif // PINSELECTIONMANAGER_H