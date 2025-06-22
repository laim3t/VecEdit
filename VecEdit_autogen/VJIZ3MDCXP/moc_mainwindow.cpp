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
    QByteArrayData data[109];
    char stringdata0[1856];
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
QT_MOC_LITERAL(17, 258, 15), // "openVectorTable"
QT_MOC_LITERAL(18, 274, 7), // "tableId"
QT_MOC_LITERAL(19, 282, 9), // "tableName"
QT_MOC_LITERAL(20, 292, 29), // "onVectorTableSelectionChanged"
QT_MOC_LITERAL(21, 322, 5), // "index"
QT_MOC_LITERAL(22, 328, 12), // "onTabChanged"
QT_MOC_LITERAL(23, 341, 19), // "saveVectorTableData"
QT_MOC_LITERAL(24, 361, 17), // "addNewVectorTable"
QT_MOC_LITERAL(25, 379, 22), // "showPinSelectionDialog"
QT_MOC_LITERAL(26, 402, 20), // "showVectorDataDialog"
QT_MOC_LITERAL(27, 423, 10), // "startIndex"
QT_MOC_LITERAL(28, 434, 26), // "addRowToCurrentVectorTable"
QT_MOC_LITERAL(29, 461, 24), // "deleteCurrentVectorTable"
QT_MOC_LITERAL(30, 486, 24), // "deleteSelectedVectorRows"
QT_MOC_LITERAL(31, 511, 23), // "deleteVectorRowsInRange"
QT_MOC_LITERAL(32, 535, 20), // "showFillVectorDialog"
QT_MOC_LITERAL(33, 556, 24), // "fillVectorForVectorTable"
QT_MOC_LITERAL(34, 581, 5), // "value"
QT_MOC_LITERAL(35, 587, 10), // "QList<int>"
QT_MOC_LITERAL(36, 598, 14), // "selectedUiRows"
QT_MOC_LITERAL(37, 613, 21), // "fillVectorWithPattern"
QT_MOC_LITERAL(38, 635, 17), // "QMap<int,QString>"
QT_MOC_LITERAL(39, 653, 11), // "rowValueMap"
QT_MOC_LITERAL(40, 665, 21), // "showFillTimeSetDialog"
QT_MOC_LITERAL(41, 687, 25), // "fillTimeSetForVectorTable"
QT_MOC_LITERAL(42, 713, 9), // "timeSetId"
QT_MOC_LITERAL(43, 723, 24), // "showReplaceTimeSetDialog"
QT_MOC_LITERAL(44, 748, 28), // "replaceTimeSetForVectorTable"
QT_MOC_LITERAL(45, 777, 13), // "fromTimeSetId"
QT_MOC_LITERAL(46, 791, 11), // "toTimeSetId"
QT_MOC_LITERAL(47, 803, 22), // "refreshVectorTableData"
QT_MOC_LITERAL(48, 826, 25), // "openTimeSetSettingsDialog"
QT_MOC_LITERAL(49, 852, 20), // "setupVectorTablePins"
QT_MOC_LITERAL(50, 873, 21), // "openPinSettingsDialog"
QT_MOC_LITERAL(51, 895, 8), // "gotoLine"
QT_MOC_LITERAL(52, 904, 28), // "onFontZoomSliderValueChanged"
QT_MOC_LITERAL(53, 933, 15), // "onFontZoomReset"
QT_MOC_LITERAL(54, 949, 8), // "closeTab"
QT_MOC_LITERAL(55, 958, 15), // "loadCurrentPage"
QT_MOC_LITERAL(56, 974, 12), // "loadNextPage"
QT_MOC_LITERAL(57, 987, 12), // "loadPrevPage"
QT_MOC_LITERAL(58, 1000, 14), // "changePageSize"
QT_MOC_LITERAL(59, 1015, 7), // "newSize"
QT_MOC_LITERAL(60, 1023, 10), // "jumpToPage"
QT_MOC_LITERAL(61, 1034, 7), // "pageNum"
QT_MOC_LITERAL(62, 1042, 18), // "onTableCellChanged"
QT_MOC_LITERAL(63, 1061, 3), // "row"
QT_MOC_LITERAL(64, 1065, 6), // "column"
QT_MOC_LITERAL(65, 1072, 18), // "onTableRowModified"
QT_MOC_LITERAL(66, 1091, 24), // "showPinColumnContextMenu"
QT_MOC_LITERAL(67, 1116, 3), // "pos"
QT_MOC_LITERAL(68, 1120, 23), // "refreshSidebarNavigator"
QT_MOC_LITERAL(69, 1144, 20), // "onSidebarItemClicked"
QT_MOC_LITERAL(70, 1165, 16), // "QTreeWidgetItem*"
QT_MOC_LITERAL(71, 1182, 4), // "item"
QT_MOC_LITERAL(72, 1187, 16), // "onPinItemClicked"
QT_MOC_LITERAL(73, 1204, 20), // "onTimeSetItemClicked"
QT_MOC_LITERAL(74, 1225, 24), // "onVectorTableItemClicked"
QT_MOC_LITERAL(75, 1250, 18), // "onLabelItemClicked"
QT_MOC_LITERAL(76, 1269, 15), // "updateMenuState"
QT_MOC_LITERAL(77, 1285, 28), // "updateVectorColumnProperties"
QT_MOC_LITERAL(78, 1314, 27), // "calculateAndDisplayHexValue"
QT_MOC_LITERAL(79, 1342, 12), // "selectedRows"
QT_MOC_LITERAL(80, 1355, 16), // "onHexValueEdited"
QT_MOC_LITERAL(81, 1372, 16), // "validateHexInput"
QT_MOC_LITERAL(82, 1389, 4), // "text"
QT_MOC_LITERAL(83, 1394, 17), // "setupWaveformView"
QT_MOC_LITERAL(84, 1412, 18), // "updateWaveformView"
QT_MOC_LITERAL(85, 1431, 18), // "toggleWaveformView"
QT_MOC_LITERAL(86, 1450, 4), // "show"
QT_MOC_LITERAL(87, 1455, 29), // "onWaveformPinSelectionChanged"
QT_MOC_LITERAL(88, 1485, 20), // "onShowAllPinsChanged"
QT_MOC_LITERAL(89, 1506, 5), // "state"
QT_MOC_LITERAL(90, 1512, 30), // "onWaveformContextMenuRequested"
QT_MOC_LITERAL(91, 1543, 26), // "setupWaveformClickHandling"
QT_MOC_LITERAL(92, 1570, 22), // "highlightWaveformPoint"
QT_MOC_LITERAL(93, 1593, 8), // "rowIndex"
QT_MOC_LITERAL(94, 1602, 8), // "pinIndex"
QT_MOC_LITERAL(95, 1611, 19), // "jumpToWaveformPoint"
QT_MOC_LITERAL(96, 1631, 7), // "pinName"
QT_MOC_LITERAL(97, 1639, 20), // "saveCurrentTableData"
QT_MOC_LITERAL(98, 1660, 23), // "onWaveformDoubleClicked"
QT_MOC_LITERAL(99, 1684, 12), // "QMouseEvent*"
QT_MOC_LITERAL(100, 1697, 5), // "event"
QT_MOC_LITERAL(101, 1703, 21), // "onWaveformValueEdited"
QT_MOC_LITERAL(102, 1725, 19), // "on_action_triggered"
QT_MOC_LITERAL(103, 1745, 7), // "checked"
QT_MOC_LITERAL(104, 1753, 35), // "onProjectStructureItemDoubleC..."
QT_MOC_LITERAL(105, 1789, 17), // "updateWindowTitle"
QT_MOC_LITERAL(106, 1807, 6), // "dbPath"
QT_MOC_LITERAL(107, 1814, 15), // "onWindowResized"
QT_MOC_LITERAL(108, 1830, 25) // "onPinValueEditingFinished"

    },
    "MainWindow\0windowResized\0\0createNewProject\0"
    "openExistingProject\0closeCurrentProject\0"
    "showDatabaseViewDialog\0showAddPinsDialog\0"
    "addSinglePin\0deletePins\0addPinsToDatabase\0"
    "QList<QString>\0pinNames\0showTimeSetDialog\0"
    "isInitialSetup\0showPinGroupDialog\0"
    "loadVectorTable\0openVectorTable\0tableId\0"
    "tableName\0onVectorTableSelectionChanged\0"
    "index\0onTabChanged\0saveVectorTableData\0"
    "addNewVectorTable\0showPinSelectionDialog\0"
    "showVectorDataDialog\0startIndex\0"
    "addRowToCurrentVectorTable\0"
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
    "text\0setupWaveformView\0updateWaveformView\0"
    "toggleWaveformView\0show\0"
    "onWaveformPinSelectionChanged\0"
    "onShowAllPinsChanged\0state\0"
    "onWaveformContextMenuRequested\0"
    "setupWaveformClickHandling\0"
    "highlightWaveformPoint\0rowIndex\0"
    "pinIndex\0jumpToWaveformPoint\0pinName\0"
    "saveCurrentTableData\0onWaveformDoubleClicked\0"
    "QMouseEvent*\0event\0onWaveformValueEdited\0"
    "on_action_triggered\0checked\0"
    "onProjectStructureItemDoubleClicked\0"
    "updateWindowTitle\0dbPath\0onWindowResized\0"
    "onPinValueEditingFinished"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_MainWindow[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      78,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,  404,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       3,    0,  405,    2, 0x08 /* Private */,
       4,    0,  406,    2, 0x08 /* Private */,
       5,    0,  407,    2, 0x08 /* Private */,
       6,    0,  408,    2, 0x08 /* Private */,
       7,    0,  409,    2, 0x08 /* Private */,
       8,    0,  410,    2, 0x08 /* Private */,
       9,    0,  411,    2, 0x08 /* Private */,
      10,    1,  412,    2, 0x08 /* Private */,
      13,    1,  415,    2, 0x08 /* Private */,
      13,    0,  418,    2, 0x28 /* Private | MethodCloned */,
      15,    0,  419,    2, 0x08 /* Private */,
      16,    0,  420,    2, 0x08 /* Private */,
      17,    2,  421,    2, 0x08 /* Private */,
      20,    1,  426,    2, 0x08 /* Private */,
      22,    1,  429,    2, 0x08 /* Private */,
      23,    0,  432,    2, 0x08 /* Private */,
      24,    0,  433,    2, 0x08 /* Private */,
      25,    2,  434,    2, 0x08 /* Private */,
      26,    3,  439,    2, 0x08 /* Private */,
      26,    2,  446,    2, 0x28 /* Private | MethodCloned */,
      28,    0,  451,    2, 0x08 /* Private */,
      29,    0,  452,    2, 0x08 /* Private */,
      30,    0,  453,    2, 0x08 /* Private */,
      31,    0,  454,    2, 0x08 /* Private */,
      32,    0,  455,    2, 0x08 /* Private */,
      33,    2,  456,    2, 0x08 /* Private */,
      37,    1,  461,    2, 0x08 /* Private */,
      40,    0,  464,    2, 0x08 /* Private */,
      41,    2,  465,    2, 0x08 /* Private */,
      43,    0,  470,    2, 0x08 /* Private */,
      44,    3,  471,    2, 0x08 /* Private */,
      47,    0,  478,    2, 0x08 /* Private */,
      48,    0,  479,    2, 0x08 /* Private */,
      49,    0,  480,    2, 0x08 /* Private */,
      50,    0,  481,    2, 0x08 /* Private */,
      51,    0,  482,    2, 0x08 /* Private */,
      52,    1,  483,    2, 0x08 /* Private */,
      53,    0,  486,    2, 0x08 /* Private */,
      54,    1,  487,    2, 0x08 /* Private */,
      55,    0,  490,    2, 0x08 /* Private */,
      56,    0,  491,    2, 0x08 /* Private */,
      57,    0,  492,    2, 0x08 /* Private */,
      58,    1,  493,    2, 0x08 /* Private */,
      60,    1,  496,    2, 0x08 /* Private */,
      62,    2,  499,    2, 0x08 /* Private */,
      65,    1,  504,    2, 0x08 /* Private */,
      66,    1,  507,    2, 0x08 /* Private */,
      68,    0,  510,    2, 0x08 /* Private */,
      69,    2,  511,    2, 0x08 /* Private */,
      72,    2,  516,    2, 0x08 /* Private */,
      73,    2,  521,    2, 0x08 /* Private */,
      74,    2,  526,    2, 0x08 /* Private */,
      75,    2,  531,    2, 0x08 /* Private */,
      76,    0,  536,    2, 0x08 /* Private */,
      77,    2,  537,    2, 0x08 /* Private */,
      78,    2,  542,    2, 0x08 /* Private */,
      80,    3,  547,    2, 0x08 /* Private */,
      81,    1,  554,    2, 0x08 /* Private */,
      83,    0,  557,    2, 0x08 /* Private */,
      84,    0,  558,    2, 0x08 /* Private */,
      85,    1,  559,    2, 0x08 /* Private */,
      87,    1,  562,    2, 0x08 /* Private */,
      88,    1,  565,    2, 0x08 /* Private */,
      90,    1,  568,    2, 0x08 /* Private */,
      91,    0,  571,    2, 0x08 /* Private */,
      92,    2,  572,    2, 0x08 /* Private */,
      92,    1,  577,    2, 0x28 /* Private | MethodCloned */,
      95,    2,  580,    2, 0x08 /* Private */,
      97,    0,  585,    2, 0x08 /* Private */,
      98,    1,  586,    2, 0x08 /* Private */,
     101,    0,  589,    2, 0x08 /* Private */,
     102,    1,  590,    2, 0x08 /* Private */,
     104,    2,  593,    2, 0x08 /* Private */,
     105,    1,  598,    2, 0x08 /* Private */,
     105,    0,  601,    2, 0x28 /* Private | MethodCloned */,
     107,    0,  602,    2, 0x08 /* Private */,
     108,    0,  603,    2, 0x08 /* Private */,

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
    QMetaType::Void, QMetaType::Int, QMetaType::QString,   18,   19,
    QMetaType::Void, QMetaType::Int,   21,
    QMetaType::Void, QMetaType::Int,   21,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::QString,   18,   19,
    QMetaType::Void, QMetaType::Int, QMetaType::QString, QMetaType::Int,   18,   19,   27,
    QMetaType::Void, QMetaType::Int, QMetaType::QString,   18,   19,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString, 0x80000000 | 35,   34,   36,
    QMetaType::Void, 0x80000000 | 38,   39,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, 0x80000000 | 35,   42,   36,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::Int, 0x80000000 | 35,   45,   46,   36,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   34,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   21,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   59,
    QMetaType::Void, QMetaType::Int,   61,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,   63,   64,
    QMetaType::Void, QMetaType::Int,   63,
    QMetaType::Void, QMetaType::QPoint,   67,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 70, QMetaType::Int,   71,   64,
    QMetaType::Void, 0x80000000 | 70, QMetaType::Int,   71,   64,
    QMetaType::Void, 0x80000000 | 70, QMetaType::Int,   71,   64,
    QMetaType::Void, 0x80000000 | 70, QMetaType::Int,   71,   64,
    QMetaType::Void, 0x80000000 | 70, QMetaType::Int,   71,   64,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,   63,   64,
    QMetaType::Void, 0x80000000 | 35, QMetaType::Int,   79,   64,
    QMetaType::Void, QMetaType::Int, QMetaType::Int, QMetaType::QString,   63,   64,   34,
    QMetaType::Void, QMetaType::QString,   82,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,   86,
    QMetaType::Void, QMetaType::Int,   21,
    QMetaType::Void, QMetaType::Int,   89,
    QMetaType::Void, QMetaType::QPoint,   67,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,   93,   94,
    QMetaType::Void, QMetaType::Int,   93,
    QMetaType::Void, QMetaType::Int, QMetaType::QString,   93,   96,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 99,  100,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,  103,
    QMetaType::Void, 0x80000000 | 70, QMetaType::Int,   71,   64,
    QMetaType::Void, QMetaType::QString,  106,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

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
        case 13: _t->openVectorTable((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2]))); break;
        case 14: _t->onVectorTableSelectionChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 15: _t->onTabChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 16: _t->saveVectorTableData(); break;
        case 17: _t->addNewVectorTable(); break;
        case 18: _t->showPinSelectionDialog((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2]))); break;
        case 19: _t->showVectorDataDialog((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2])),(*reinterpret_cast< int(*)>(_a[3]))); break;
        case 20: _t->showVectorDataDialog((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2]))); break;
        case 21: _t->addRowToCurrentVectorTable(); break;
        case 22: _t->deleteCurrentVectorTable(); break;
        case 23: _t->deleteSelectedVectorRows(); break;
        case 24: _t->deleteVectorRowsInRange(); break;
        case 25: _t->showFillVectorDialog(); break;
        case 26: _t->fillVectorForVectorTable((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< const QList<int>(*)>(_a[2]))); break;
        case 27: _t->fillVectorWithPattern((*reinterpret_cast< const QMap<int,QString>(*)>(_a[1]))); break;
        case 28: _t->showFillTimeSetDialog(); break;
        case 29: _t->fillTimeSetForVectorTable((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QList<int>(*)>(_a[2]))); break;
        case 30: _t->showReplaceTimeSetDialog(); break;
        case 31: _t->replaceTimeSetForVectorTable((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2])),(*reinterpret_cast< const QList<int>(*)>(_a[3]))); break;
        case 32: _t->refreshVectorTableData(); break;
        case 33: _t->openTimeSetSettingsDialog(); break;
        case 34: _t->setupVectorTablePins(); break;
        case 35: _t->openPinSettingsDialog(); break;
        case 36: _t->gotoLine(); break;
        case 37: _t->onFontZoomSliderValueChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 38: _t->onFontZoomReset(); break;
        case 39: _t->closeTab((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 40: _t->loadCurrentPage(); break;
        case 41: _t->loadNextPage(); break;
        case 42: _t->loadPrevPage(); break;
        case 43: _t->changePageSize((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 44: _t->jumpToPage((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 45: _t->onTableCellChanged((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 46: _t->onTableRowModified((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 47: _t->showPinColumnContextMenu((*reinterpret_cast< const QPoint(*)>(_a[1]))); break;
        case 48: _t->refreshSidebarNavigator(); break;
        case 49: _t->onSidebarItemClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 50: _t->onPinItemClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 51: _t->onTimeSetItemClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 52: _t->onVectorTableItemClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 53: _t->onLabelItemClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 54: _t->updateMenuState(); break;
        case 55: _t->updateVectorColumnProperties((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 56: _t->calculateAndDisplayHexValue((*reinterpret_cast< const QList<int>(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 57: _t->onHexValueEdited((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2])),(*reinterpret_cast< const QString(*)>(_a[3]))); break;
        case 58: _t->validateHexInput((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 59: _t->setupWaveformView(); break;
        case 60: _t->updateWaveformView(); break;
        case 61: _t->toggleWaveformView((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 62: _t->onWaveformPinSelectionChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 63: _t->onShowAllPinsChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 64: _t->onWaveformContextMenuRequested((*reinterpret_cast< const QPoint(*)>(_a[1]))); break;
        case 65: _t->setupWaveformClickHandling(); break;
        case 66: _t->highlightWaveformPoint((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 67: _t->highlightWaveformPoint((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 68: _t->jumpToWaveformPoint((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2]))); break;
        case 69: _t->saveCurrentTableData(); break;
        case 70: _t->onWaveformDoubleClicked((*reinterpret_cast< QMouseEvent*(*)>(_a[1]))); break;
        case 71: _t->onWaveformValueEdited(); break;
        case 72: _t->on_action_triggered((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 73: _t->onProjectStructureItemDoubleClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 74: _t->updateWindowTitle((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 75: _t->updateWindowTitle(); break;
        case 76: _t->onWindowResized(); break;
        case 77: _t->onPinValueEditingFinished(); break;
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
        case 26:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 1:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QList<int> >(); break;
            }
            break;
        case 29:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 1:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QList<int> >(); break;
            }
            break;
        case 31:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 2:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QList<int> >(); break;
            }
            break;
        case 56:
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
        if (_id < 78)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 78;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 78)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 78;
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
