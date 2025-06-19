/****************************************************************************
** Meta object code from reading C++ file 'mainwindow.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../app/mainwindow.h"
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
    char stringdata0[1840];
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
QT_MOC_LITERAL(62, 1042, 24), // "showPinColumnContextMenu"
QT_MOC_LITERAL(63, 1067, 3), // "pos"
QT_MOC_LITERAL(64, 1071, 23), // "refreshSidebarNavigator"
QT_MOC_LITERAL(65, 1095, 20), // "onSidebarItemClicked"
QT_MOC_LITERAL(66, 1116, 16), // "QTreeWidgetItem*"
QT_MOC_LITERAL(67, 1133, 4), // "item"
QT_MOC_LITERAL(68, 1138, 6), // "column"
QT_MOC_LITERAL(69, 1145, 16), // "onPinItemClicked"
QT_MOC_LITERAL(70, 1162, 20), // "onTimeSetItemClicked"
QT_MOC_LITERAL(71, 1183, 24), // "onVectorTableItemClicked"
QT_MOC_LITERAL(72, 1208, 18), // "onLabelItemClicked"
QT_MOC_LITERAL(73, 1227, 15), // "updateMenuState"
QT_MOC_LITERAL(74, 1243, 28), // "updateVectorColumnProperties"
QT_MOC_LITERAL(75, 1272, 3), // "row"
QT_MOC_LITERAL(76, 1276, 27), // "calculateAndDisplayHexValue"
QT_MOC_LITERAL(77, 1304, 12), // "selectedRows"
QT_MOC_LITERAL(78, 1317, 16), // "validateHexInput"
QT_MOC_LITERAL(79, 1334, 4), // "text"
QT_MOC_LITERAL(80, 1339, 17), // "setupWaveformView"
QT_MOC_LITERAL(81, 1357, 18), // "updateWaveformView"
QT_MOC_LITERAL(82, 1376, 18), // "toggleWaveformView"
QT_MOC_LITERAL(83, 1395, 4), // "show"
QT_MOC_LITERAL(84, 1400, 29), // "onWaveformPinSelectionChanged"
QT_MOC_LITERAL(85, 1430, 20), // "onShowAllPinsChanged"
QT_MOC_LITERAL(86, 1451, 5), // "state"
QT_MOC_LITERAL(87, 1457, 30), // "onWaveformContextMenuRequested"
QT_MOC_LITERAL(88, 1488, 26), // "setupWaveformClickHandling"
QT_MOC_LITERAL(89, 1515, 22), // "highlightWaveformPoint"
QT_MOC_LITERAL(90, 1538, 8), // "rowIndex"
QT_MOC_LITERAL(91, 1547, 8), // "pinIndex"
QT_MOC_LITERAL(92, 1556, 19), // "jumpToWaveformPoint"
QT_MOC_LITERAL(93, 1576, 7), // "pinName"
QT_MOC_LITERAL(94, 1584, 20), // "saveCurrentTableData"
QT_MOC_LITERAL(95, 1605, 23), // "onWaveformDoubleClicked"
QT_MOC_LITERAL(96, 1629, 12), // "QMouseEvent*"
QT_MOC_LITERAL(97, 1642, 5), // "event"
QT_MOC_LITERAL(98, 1648, 21), // "onWaveformValueEdited"
QT_MOC_LITERAL(99, 1670, 19), // "on_action_triggered"
QT_MOC_LITERAL(100, 1690, 7), // "checked"
QT_MOC_LITERAL(101, 1698, 35), // "onProjectStructureItemDoubleC..."
QT_MOC_LITERAL(102, 1734, 17), // "updateWindowTitle"
QT_MOC_LITERAL(103, 1752, 6), // "dbPath"
QT_MOC_LITERAL(104, 1759, 18), // "addHighlightForPin"
QT_MOC_LITERAL(105, 1778, 26), // "QList<QPair<QString,int> >"
QT_MOC_LITERAL(106, 1805, 10), // "pinColumns"
QT_MOC_LITERAL(107, 1816, 15), // "Vector::RowData"
QT_MOC_LITERAL(108, 1832, 7) // "rowData"

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
    "showPinColumnContextMenu\0pos\0"
    "refreshSidebarNavigator\0onSidebarItemClicked\0"
    "QTreeWidgetItem*\0item\0column\0"
    "onPinItemClicked\0onTimeSetItemClicked\0"
    "onVectorTableItemClicked\0onLabelItemClicked\0"
    "updateMenuState\0updateVectorColumnProperties\0"
    "row\0calculateAndDisplayHexValue\0"
    "selectedRows\0validateHexInput\0text\0"
    "setupWaveformView\0updateWaveformView\0"
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
    "updateWindowTitle\0dbPath\0addHighlightForPin\0"
    "QList<QPair<QString,int> >\0pinColumns\0"
    "Vector::RowData\0rowData"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_MainWindow[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      74,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,  384,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       3,    0,  385,    2, 0x08 /* Private */,
       4,    0,  386,    2, 0x08 /* Private */,
       5,    0,  387,    2, 0x08 /* Private */,
       6,    0,  388,    2, 0x08 /* Private */,
       7,    0,  389,    2, 0x08 /* Private */,
       8,    0,  390,    2, 0x08 /* Private */,
       9,    0,  391,    2, 0x08 /* Private */,
      10,    1,  392,    2, 0x08 /* Private */,
      13,    1,  395,    2, 0x08 /* Private */,
      13,    0,  398,    2, 0x28 /* Private | MethodCloned */,
      15,    0,  399,    2, 0x08 /* Private */,
      16,    0,  400,    2, 0x08 /* Private */,
      17,    2,  401,    2, 0x08 /* Private */,
      20,    1,  406,    2, 0x08 /* Private */,
      22,    1,  409,    2, 0x08 /* Private */,
      23,    0,  412,    2, 0x08 /* Private */,
      24,    0,  413,    2, 0x08 /* Private */,
      25,    2,  414,    2, 0x08 /* Private */,
      26,    3,  419,    2, 0x08 /* Private */,
      26,    2,  426,    2, 0x28 /* Private | MethodCloned */,
      28,    0,  431,    2, 0x08 /* Private */,
      29,    0,  432,    2, 0x08 /* Private */,
      30,    0,  433,    2, 0x08 /* Private */,
      31,    0,  434,    2, 0x08 /* Private */,
      32,    0,  435,    2, 0x08 /* Private */,
      33,    2,  436,    2, 0x08 /* Private */,
      37,    1,  441,    2, 0x08 /* Private */,
      40,    0,  444,    2, 0x08 /* Private */,
      41,    2,  445,    2, 0x08 /* Private */,
      43,    0,  450,    2, 0x08 /* Private */,
      44,    3,  451,    2, 0x08 /* Private */,
      47,    0,  458,    2, 0x08 /* Private */,
      48,    0,  459,    2, 0x08 /* Private */,
      49,    0,  460,    2, 0x08 /* Private */,
      50,    0,  461,    2, 0x08 /* Private */,
      51,    0,  462,    2, 0x08 /* Private */,
      52,    1,  463,    2, 0x08 /* Private */,
      53,    0,  466,    2, 0x08 /* Private */,
      54,    1,  467,    2, 0x08 /* Private */,
      55,    0,  470,    2, 0x08 /* Private */,
      56,    0,  471,    2, 0x08 /* Private */,
      57,    0,  472,    2, 0x08 /* Private */,
      58,    1,  473,    2, 0x08 /* Private */,
      60,    1,  476,    2, 0x08 /* Private */,
      62,    1,  479,    2, 0x08 /* Private */,
      64,    0,  482,    2, 0x08 /* Private */,
      65,    2,  483,    2, 0x08 /* Private */,
      69,    2,  488,    2, 0x08 /* Private */,
      70,    2,  493,    2, 0x08 /* Private */,
      71,    2,  498,    2, 0x08 /* Private */,
      72,    2,  503,    2, 0x08 /* Private */,
      73,    0,  508,    2, 0x08 /* Private */,
      74,    2,  509,    2, 0x08 /* Private */,
      76,    2,  514,    2, 0x08 /* Private */,
      78,    1,  519,    2, 0x08 /* Private */,
      80,    0,  522,    2, 0x08 /* Private */,
      81,    0,  523,    2, 0x08 /* Private */,
      82,    1,  524,    2, 0x08 /* Private */,
      84,    1,  527,    2, 0x08 /* Private */,
      85,    1,  530,    2, 0x08 /* Private */,
      87,    1,  533,    2, 0x08 /* Private */,
      88,    0,  536,    2, 0x08 /* Private */,
      89,    2,  537,    2, 0x08 /* Private */,
      89,    1,  542,    2, 0x28 /* Private | MethodCloned */,
      92,    2,  545,    2, 0x08 /* Private */,
      94,    0,  550,    2, 0x08 /* Private */,
      95,    1,  551,    2, 0x08 /* Private */,
      98,    0,  554,    2, 0x08 /* Private */,
      99,    1,  555,    2, 0x08 /* Private */,
     101,    2,  558,    2, 0x08 /* Private */,
     102,    1,  563,    2, 0x08 /* Private */,
     102,    0,  566,    2, 0x28 /* Private | MethodCloned */,
     104,    4,  567,    2, 0x08 /* Private */,

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
    QMetaType::Void, QMetaType::QPoint,   63,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 66, QMetaType::Int,   67,   68,
    QMetaType::Void, 0x80000000 | 66, QMetaType::Int,   67,   68,
    QMetaType::Void, 0x80000000 | 66, QMetaType::Int,   67,   68,
    QMetaType::Void, 0x80000000 | 66, QMetaType::Int,   67,   68,
    QMetaType::Void, 0x80000000 | 66, QMetaType::Int,   67,   68,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,   75,   68,
    QMetaType::Void, 0x80000000 | 35, QMetaType::Int,   77,   68,
    QMetaType::Void, QMetaType::QString,   79,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,   83,
    QMetaType::Void, QMetaType::Int,   21,
    QMetaType::Void, QMetaType::Int,   86,
    QMetaType::Void, QMetaType::QPoint,   63,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,   90,   91,
    QMetaType::Void, QMetaType::Int,   90,
    QMetaType::Void, QMetaType::Int, QMetaType::QString,   90,   93,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 96,   97,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,  100,
    QMetaType::Void, 0x80000000 | 66, QMetaType::Int,   67,   68,
    QMetaType::Void, QMetaType::QString,  103,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::Int, 0x80000000 | 105, 0x80000000 | 107,   90,   91,  106,  108,

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
        case 45: _t->showPinColumnContextMenu((*reinterpret_cast< const QPoint(*)>(_a[1]))); break;
        case 46: _t->refreshSidebarNavigator(); break;
        case 47: _t->onSidebarItemClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 48: _t->onPinItemClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 49: _t->onTimeSetItemClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 50: _t->onVectorTableItemClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 51: _t->onLabelItemClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 52: _t->updateMenuState(); break;
        case 53: _t->updateVectorColumnProperties((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 54: _t->calculateAndDisplayHexValue((*reinterpret_cast< const QList<int>(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 55: _t->validateHexInput((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 56: _t->setupWaveformView(); break;
        case 57: _t->updateWaveformView(); break;
        case 58: _t->toggleWaveformView((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 59: _t->onWaveformPinSelectionChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 60: _t->onShowAllPinsChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 61: _t->onWaveformContextMenuRequested((*reinterpret_cast< const QPoint(*)>(_a[1]))); break;
        case 62: _t->setupWaveformClickHandling(); break;
        case 63: _t->highlightWaveformPoint((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 64: _t->highlightWaveformPoint((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 65: _t->jumpToWaveformPoint((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2]))); break;
        case 66: _t->saveCurrentTableData(); break;
        case 67: _t->onWaveformDoubleClicked((*reinterpret_cast< QMouseEvent*(*)>(_a[1]))); break;
        case 68: _t->onWaveformValueEdited(); break;
        case 69: _t->on_action_triggered((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 70: _t->onProjectStructureItemDoubleClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 71: _t->updateWindowTitle((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 72: _t->updateWindowTitle(); break;
        case 73: _t->addHighlightForPin((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2])),(*reinterpret_cast< const QList<QPair<QString,int> >(*)>(_a[3])),(*reinterpret_cast< const Vector::RowData(*)>(_a[4]))); break;
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
        case 54:
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
        if (_id < 74)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 74;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 74)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 74;
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
