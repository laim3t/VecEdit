/****************************************************************************
** Meta object code from reading C++ file 'mainwindow.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../app/mainwindow.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#include <QtCore/QList>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'mainwindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_MainWindow_t {
    QByteArrayData data[82];
    char stringdata0[1378];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_MainWindow_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_MainWindow_t qt_meta_stringdata_MainWindow = {
    {
QT_MOC_LITERAL(0, 0, 10), // "MainWindow"
QT_MOC_LITERAL(1, 11, 13), // "windowResized"
QT_MOC_LITERAL(2, 25, 0), // ""
QT_MOC_LITERAL(3, 26, 16), // "createNewProject"
QT_MOC_LITERAL(4, 43, 19), // "openExistingProject"
QT_MOC_LITERAL(5, 63, 19), // "closeCurrentProject"
QT_MOC_LITERAL(6, 83, 22), // "showDatabaseViewDialog"
QT_MOC_LITERAL(7, 106, 17), // "showAddPinsDialog"
QT_MOC_LITERAL(8, 124, 12), // "addSinglePin"
QT_MOC_LITERAL(9, 137, 10), // "deletePins"
QT_MOC_LITERAL(10, 148, 17), // "addPinsToDatabase"
QT_MOC_LITERAL(11, 166, 14), // "QList<QString>"
QT_MOC_LITERAL(12, 181, 8), // "pinNames"
QT_MOC_LITERAL(13, 190, 17), // "showTimeSetDialog"
QT_MOC_LITERAL(14, 208, 14), // "isInitialSetup"
QT_MOC_LITERAL(15, 223, 18), // "showPinGroupDialog"
QT_MOC_LITERAL(16, 242, 15), // "loadVectorTable"
QT_MOC_LITERAL(17, 258, 29), // "onVectorTableSelectionChanged"
QT_MOC_LITERAL(18, 288, 5), // "index"
QT_MOC_LITERAL(19, 294, 12), // "onTabChanged"
QT_MOC_LITERAL(20, 307, 19), // "saveVectorTableData"
QT_MOC_LITERAL(21, 327, 17), // "addNewVectorTable"
QT_MOC_LITERAL(22, 345, 22), // "showPinSelectionDialog"
QT_MOC_LITERAL(23, 368, 7), // "tableId"
QT_MOC_LITERAL(24, 376, 9), // "tableName"
QT_MOC_LITERAL(25, 386, 20), // "showVectorDataDialog"
QT_MOC_LITERAL(26, 407, 10), // "startIndex"
QT_MOC_LITERAL(27, 418, 26), // "addRowToCurrentVectorTable"
QT_MOC_LITERAL(28, 445, 24), // "deleteCurrentVectorTable"
QT_MOC_LITERAL(29, 470, 24), // "deleteSelectedVectorRows"
QT_MOC_LITERAL(30, 495, 23), // "deleteVectorRowsInRange"
QT_MOC_LITERAL(31, 519, 20), // "showFillVectorDialog"
QT_MOC_LITERAL(32, 540, 24), // "fillVectorForVectorTable"
QT_MOC_LITERAL(33, 565, 5), // "value"
QT_MOC_LITERAL(34, 571, 10), // "QList<int>"
QT_MOC_LITERAL(35, 582, 14), // "selectedUiRows"
QT_MOC_LITERAL(36, 597, 21), // "fillVectorWithPattern"
QT_MOC_LITERAL(37, 619, 17), // "QMap<int,QString>"
QT_MOC_LITERAL(38, 637, 11), // "rowValueMap"
QT_MOC_LITERAL(39, 649, 21), // "showFillTimeSetDialog"
QT_MOC_LITERAL(40, 671, 25), // "fillTimeSetForVectorTable"
QT_MOC_LITERAL(41, 697, 9), // "timeSetId"
QT_MOC_LITERAL(42, 707, 24), // "showReplaceTimeSetDialog"
QT_MOC_LITERAL(43, 732, 28), // "replaceTimeSetForVectorTable"
QT_MOC_LITERAL(44, 761, 13), // "fromTimeSetId"
QT_MOC_LITERAL(45, 775, 11), // "toTimeSetId"
QT_MOC_LITERAL(46, 787, 22), // "refreshVectorTableData"
QT_MOC_LITERAL(47, 810, 25), // "openTimeSetSettingsDialog"
QT_MOC_LITERAL(48, 836, 20), // "setupVectorTablePins"
QT_MOC_LITERAL(49, 857, 21), // "openPinSettingsDialog"
QT_MOC_LITERAL(50, 879, 8), // "gotoLine"
QT_MOC_LITERAL(51, 888, 28), // "onFontZoomSliderValueChanged"
QT_MOC_LITERAL(52, 917, 15), // "onFontZoomReset"
QT_MOC_LITERAL(53, 933, 8), // "closeTab"
QT_MOC_LITERAL(54, 942, 15), // "loadCurrentPage"
QT_MOC_LITERAL(55, 958, 12), // "loadNextPage"
QT_MOC_LITERAL(56, 971, 12), // "loadPrevPage"
QT_MOC_LITERAL(57, 984, 14), // "changePageSize"
QT_MOC_LITERAL(58, 999, 7), // "newSize"
QT_MOC_LITERAL(59, 1007, 10), // "jumpToPage"
QT_MOC_LITERAL(60, 1018, 7), // "pageNum"
QT_MOC_LITERAL(61, 1026, 18), // "onTableCellChanged"
QT_MOC_LITERAL(62, 1045, 3), // "row"
QT_MOC_LITERAL(63, 1049, 6), // "column"
QT_MOC_LITERAL(64, 1056, 18), // "onTableRowModified"
QT_MOC_LITERAL(65, 1075, 24), // "showPinColumnContextMenu"
QT_MOC_LITERAL(66, 1100, 3), // "pos"
QT_MOC_LITERAL(67, 1104, 23), // "refreshSidebarNavigator"
QT_MOC_LITERAL(68, 1128, 20), // "onSidebarItemClicked"
QT_MOC_LITERAL(69, 1149, 16), // "QTreeWidgetItem*"
QT_MOC_LITERAL(70, 1166, 4), // "item"
QT_MOC_LITERAL(71, 1171, 16), // "onPinItemClicked"
QT_MOC_LITERAL(72, 1188, 20), // "onTimeSetItemClicked"
QT_MOC_LITERAL(73, 1209, 24), // "onVectorTableItemClicked"
QT_MOC_LITERAL(74, 1234, 18), // "onLabelItemClicked"
QT_MOC_LITERAL(75, 1253, 15), // "updateMenuState"
QT_MOC_LITERAL(76, 1269, 28), // "updateVectorColumnProperties"
QT_MOC_LITERAL(77, 1298, 27), // "calculateAndDisplayHexValue"
QT_MOC_LITERAL(78, 1326, 12), // "selectedRows"
QT_MOC_LITERAL(79, 1339, 16), // "onHexValueEdited"
QT_MOC_LITERAL(80, 1356, 16), // "validateHexInput"
QT_MOC_LITERAL(81, 1373, 4) // "text"

    },
    "MainWindow\0windowResized\0\0createNewProject\0"
    "openExistingProject\0closeCurrentProject\0"
    "showDatabaseViewDialog\0showAddPinsDialog\0"
    "addSinglePin\0deletePins\0addPinsToDatabase\0"
    "QList<QString>\0pinNames\0showTimeSetDialog\0"
    "isInitialSetup\0showPinGroupDialog\0"
    "loadVectorTable\0onVectorTableSelectionChanged\0"
    "index\0onTabChanged\0saveVectorTableData\0"
    "addNewVectorTable\0showPinSelectionDialog\0"
    "tableId\0tableName\0showVectorDataDialog\0"
    "startIndex\0addRowToCurrentVectorTable\0"
    "deleteCurrentVectorTable\0"
    "deleteSelectedVectorRows\0"
    "deleteVectorRowsInRange\0showFillVectorDialog\0"
    "fillVectorForVectorTable\0value\0"
    "QList<int>\0selectedUiRows\0"
    "fillVectorWithPattern\0QMap<int,QString>\0"
    "rowValueMap\0showFillTimeSetDialog\0"
    "fillTimeSetForVectorTable\0timeSetId\0"
    "showReplaceTimeSetDialog\0"
    "replaceTimeSetForVectorTable\0fromTimeSetId\0"
    "toTimeSetId\0refreshVectorTableData\0"
    "openTimeSetSettingsDialog\0"
    "setupVectorTablePins\0openPinSettingsDialog\0"
    "gotoLine\0onFontZoomSliderValueChanged\0"
    "onFontZoomReset\0closeTab\0loadCurrentPage\0"
    "loadNextPage\0loadPrevPage\0changePageSize\0"
    "newSize\0jumpToPage\0pageNum\0"
    "onTableCellChanged\0row\0column\0"
    "onTableRowModified\0showPinColumnContextMenu\0"
    "pos\0refreshSidebarNavigator\0"
    "onSidebarItemClicked\0QTreeWidgetItem*\0"
    "item\0onPinItemClicked\0onTimeSetItemClicked\0"
    "onVectorTableItemClicked\0onLabelItemClicked\0"
    "updateMenuState\0updateVectorColumnProperties\0"
    "calculateAndDisplayHexValue\0selectedRows\0"
    "onHexValueEdited\0validateHexInput\0"
    "text"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_MainWindow[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      58,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,  304,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       3,    0,  305,    2, 0x08 /* Private */,
       4,    0,  306,    2, 0x08 /* Private */,
       5,    0,  307,    2, 0x08 /* Private */,
       6,    0,  308,    2, 0x08 /* Private */,
       7,    0,  309,    2, 0x08 /* Private */,
       8,    0,  310,    2, 0x08 /* Private */,
       9,    0,  311,    2, 0x08 /* Private */,
      10,    1,  312,    2, 0x08 /* Private */,
      13,    1,  315,    2, 0x08 /* Private */,
      13,    0,  318,    2, 0x28 /* Private | MethodCloned */,
      15,    0,  319,    2, 0x08 /* Private */,
      16,    0,  320,    2, 0x08 /* Private */,
      17,    1,  321,    2, 0x08 /* Private */,
      19,    1,  324,    2, 0x08 /* Private */,
      20,    0,  327,    2, 0x08 /* Private */,
      21,    0,  328,    2, 0x08 /* Private */,
      22,    2,  329,    2, 0x08 /* Private */,
      25,    3,  334,    2, 0x08 /* Private */,
      25,    2,  341,    2, 0x28 /* Private | MethodCloned */,
      27,    0,  346,    2, 0x08 /* Private */,
      28,    0,  347,    2, 0x08 /* Private */,
      29,    0,  348,    2, 0x08 /* Private */,
      30,    0,  349,    2, 0x08 /* Private */,
      31,    0,  350,    2, 0x08 /* Private */,
      32,    2,  351,    2, 0x08 /* Private */,
      36,    1,  356,    2, 0x08 /* Private */,
      39,    0,  359,    2, 0x08 /* Private */,
      40,    2,  360,    2, 0x08 /* Private */,
      42,    0,  365,    2, 0x08 /* Private */,
      43,    3,  366,    2, 0x08 /* Private */,
      46,    0,  373,    2, 0x08 /* Private */,
      47,    0,  374,    2, 0x08 /* Private */,
      48,    0,  375,    2, 0x08 /* Private */,
      49,    0,  376,    2, 0x08 /* Private */,
      50,    0,  377,    2, 0x08 /* Private */,
      51,    1,  378,    2, 0x08 /* Private */,
      52,    0,  381,    2, 0x08 /* Private */,
      53,    1,  382,    2, 0x08 /* Private */,
      54,    0,  385,    2, 0x08 /* Private */,
      55,    0,  386,    2, 0x08 /* Private */,
      56,    0,  387,    2, 0x08 /* Private */,
      57,    1,  388,    2, 0x08 /* Private */,
      59,    1,  391,    2, 0x08 /* Private */,
      61,    2,  394,    2, 0x08 /* Private */,
      64,    1,  399,    2, 0x08 /* Private */,
      65,    1,  402,    2, 0x08 /* Private */,
      67,    0,  405,    2, 0x08 /* Private */,
      68,    2,  406,    2, 0x08 /* Private */,
      71,    2,  411,    2, 0x08 /* Private */,
      72,    2,  416,    2, 0x08 /* Private */,
      73,    2,  421,    2, 0x08 /* Private */,
      74,    2,  426,    2, 0x08 /* Private */,
      75,    0,  431,    2, 0x08 /* Private */,
      76,    2,  432,    2, 0x08 /* Private */,
      77,    2,  437,    2, 0x08 /* Private */,
      79,    0,  442,    2, 0x08 /* Private */,
      80,    1,  443,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Bool,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Bool, 0x80000000 | 11,   12,
    QMetaType::Bool, QMetaType::Bool,   14,
    QMetaType::Bool,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   18,
    QMetaType::Void, QMetaType::Int,   18,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::QString,   23,   24,
    QMetaType::Void, QMetaType::Int, QMetaType::QString, QMetaType::Int,   23,   24,   26,
    QMetaType::Void, QMetaType::Int, QMetaType::QString,   23,   24,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString, 0x80000000 | 34,   33,   35,
    QMetaType::Void, 0x80000000 | 37,   38,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, 0x80000000 | 34,   41,   35,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::Int, 0x80000000 | 34,   44,   45,   35,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   33,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   18,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   58,
    QMetaType::Void, QMetaType::Int,   60,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,   62,   63,
    QMetaType::Void, QMetaType::Int,   62,
    QMetaType::Void, QMetaType::QPoint,   66,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 69, QMetaType::Int,   70,   63,
    QMetaType::Void, 0x80000000 | 69, QMetaType::Int,   70,   63,
    QMetaType::Void, 0x80000000 | 69, QMetaType::Int,   70,   63,
    QMetaType::Void, 0x80000000 | 69, QMetaType::Int,   70,   63,
    QMetaType::Void, 0x80000000 | 69, QMetaType::Int,   70,   63,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,   62,   63,
    QMetaType::Void, 0x80000000 | 34, QMetaType::Int,   78,   63,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   81,

       0        // eod
};

void MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<MainWindow *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->windowResized(); break;
        case 1: _t->createNewProject(); break;
        case 2: _t->openExistingProject(); break;
        case 3: _t->closeCurrentProject(); break;
        case 4: _t->showDatabaseViewDialog(); break;
        case 5: { bool _r = _t->showAddPinsDialog();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 6: _t->addSinglePin(); break;
        case 7: _t->deletePins(); break;
        case 8: { bool _r = _t->addPinsToDatabase((*reinterpret_cast< const QList<QString>(*)>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 9: { bool _r = _t->showTimeSetDialog((*reinterpret_cast< bool(*)>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 10: { bool _r = _t->showTimeSetDialog();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 11: _t->showPinGroupDialog(); break;
        case 12: _t->loadVectorTable(); break;
        case 13: _t->onVectorTableSelectionChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 14: _t->onTabChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 15: _t->saveVectorTableData(); break;
        case 16: _t->addNewVectorTable(); break;
        case 17: _t->showPinSelectionDialog((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2]))); break;
        case 18: _t->showVectorDataDialog((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2])),(*reinterpret_cast< int(*)>(_a[3]))); break;
        case 19: _t->showVectorDataDialog((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2]))); break;
        case 20: _t->addRowToCurrentVectorTable(); break;
        case 21: _t->deleteCurrentVectorTable(); break;
        case 22: _t->deleteSelectedVectorRows(); break;
        case 23: _t->deleteVectorRowsInRange(); break;
        case 24: _t->showFillVectorDialog(); break;
        case 25: _t->fillVectorForVectorTable((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< const QList<int>(*)>(_a[2]))); break;
        case 26: _t->fillVectorWithPattern((*reinterpret_cast< const QMap<int,QString>(*)>(_a[1]))); break;
        case 27: _t->showFillTimeSetDialog(); break;
        case 28: _t->fillTimeSetForVectorTable((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QList<int>(*)>(_a[2]))); break;
        case 29: _t->showReplaceTimeSetDialog(); break;
        case 30: _t->replaceTimeSetForVectorTable((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2])),(*reinterpret_cast< const QList<int>(*)>(_a[3]))); break;
        case 31: _t->refreshVectorTableData(); break;
        case 32: _t->openTimeSetSettingsDialog(); break;
        case 33: _t->setupVectorTablePins(); break;
        case 34: _t->openPinSettingsDialog(); break;
        case 35: _t->gotoLine(); break;
        case 36: _t->onFontZoomSliderValueChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 37: _t->onFontZoomReset(); break;
        case 38: _t->closeTab((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 39: _t->loadCurrentPage(); break;
        case 40: _t->loadNextPage(); break;
        case 41: _t->loadPrevPage(); break;
        case 42: _t->changePageSize((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 43: _t->jumpToPage((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 44: _t->onTableCellChanged((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 45: _t->onTableRowModified((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 46: _t->showPinColumnContextMenu((*reinterpret_cast< const QPoint(*)>(_a[1]))); break;
        case 47: _t->refreshSidebarNavigator(); break;
        case 48: _t->onSidebarItemClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 49: _t->onPinItemClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 50: _t->onTimeSetItemClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 51: _t->onVectorTableItemClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 52: _t->onLabelItemClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 53: _t->updateMenuState(); break;
        case 54: _t->updateVectorColumnProperties((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 55: _t->calculateAndDisplayHexValue((*reinterpret_cast< const QList<int>(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 56: _t->onHexValueEdited(); break;
        case 57: _t->validateHexInput((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 8:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QList<QString> >(); break;
            }
            break;
        case 25:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 1:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QList<int> >(); break;
            }
            break;
        case 28:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 1:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QList<int> >(); break;
            }
            break;
        case 30:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 2:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QList<int> >(); break;
            }
            break;
        case 55:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QList<int> >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (MainWindow::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&MainWindow::windowResized)) {
                *result = 0;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_meta_stringdata_MainWindow.data,
    qt_meta_data_MainWindow,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_MainWindow.stringdata0))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 58)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 58;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 58)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 58;
    }
    return _id;
}

// SIGNAL 0
void MainWindow::windowResized()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
