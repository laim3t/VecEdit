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
    QByteArrayData data[118];
    char stringdata0[2089];
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
QT_MOC_LITERAL(4, 43, 27), // "createNewProjectWithNewArch"
QT_MOC_LITERAL(5, 71, 19), // "openExistingProject"
QT_MOC_LITERAL(6, 91, 30), // "openExistingProjectWithNewArch"
QT_MOC_LITERAL(7, 122, 19), // "closeCurrentProject"
QT_MOC_LITERAL(8, 142, 22), // "showDatabaseViewDialog"
QT_MOC_LITERAL(9, 165, 17), // "showAddPinsDialog"
QT_MOC_LITERAL(10, 183, 12), // "addSinglePin"
QT_MOC_LITERAL(11, 196, 10), // "deletePins"
QT_MOC_LITERAL(12, 207, 17), // "addPinsToDatabase"
QT_MOC_LITERAL(13, 225, 14), // "QList<QString>"
QT_MOC_LITERAL(14, 240, 8), // "pinNames"
QT_MOC_LITERAL(15, 249, 17), // "showTimeSetDialog"
QT_MOC_LITERAL(16, 267, 14), // "isInitialSetup"
QT_MOC_LITERAL(17, 282, 18), // "showPinGroupDialog"
QT_MOC_LITERAL(18, 301, 15), // "loadVectorTable"
QT_MOC_LITERAL(19, 317, 15), // "openVectorTable"
QT_MOC_LITERAL(20, 333, 7), // "tableId"
QT_MOC_LITERAL(21, 341, 9), // "tableName"
QT_MOC_LITERAL(22, 351, 29), // "onVectorTableSelectionChanged"
QT_MOC_LITERAL(23, 381, 5), // "index"
QT_MOC_LITERAL(24, 387, 12), // "onTabChanged"
QT_MOC_LITERAL(25, 400, 19), // "saveVectorTableData"
QT_MOC_LITERAL(26, 420, 17), // "addNewVectorTable"
QT_MOC_LITERAL(27, 438, 22), // "showPinSelectionDialog"
QT_MOC_LITERAL(28, 461, 20), // "showVectorDataDialog"
QT_MOC_LITERAL(29, 482, 10), // "startIndex"
QT_MOC_LITERAL(30, 493, 26), // "addRowToCurrentVectorTable"
QT_MOC_LITERAL(31, 520, 31), // "addRowToCurrentVectorTableModel"
QT_MOC_LITERAL(32, 552, 24), // "deleteCurrentVectorTable"
QT_MOC_LITERAL(33, 577, 24), // "deleteSelectedVectorRows"
QT_MOC_LITERAL(34, 602, 23), // "deleteVectorRowsInRange"
QT_MOC_LITERAL(35, 626, 20), // "showFillVectorDialog"
QT_MOC_LITERAL(36, 647, 24), // "fillVectorForVectorTable"
QT_MOC_LITERAL(37, 672, 5), // "value"
QT_MOC_LITERAL(38, 678, 10), // "QList<int>"
QT_MOC_LITERAL(39, 689, 14), // "selectedUiRows"
QT_MOC_LITERAL(40, 704, 21), // "fillVectorWithPattern"
QT_MOC_LITERAL(41, 726, 17), // "QMap<int,QString>"
QT_MOC_LITERAL(42, 744, 11), // "rowValueMap"
QT_MOC_LITERAL(43, 756, 21), // "showFillTimeSetDialog"
QT_MOC_LITERAL(44, 778, 25), // "fillTimeSetForVectorTable"
QT_MOC_LITERAL(45, 804, 9), // "timeSetId"
QT_MOC_LITERAL(46, 814, 24), // "showReplaceTimeSetDialog"
QT_MOC_LITERAL(47, 839, 28), // "replaceTimeSetForVectorTable"
QT_MOC_LITERAL(48, 868, 13), // "fromTimeSetId"
QT_MOC_LITERAL(49, 882, 11), // "toTimeSetId"
QT_MOC_LITERAL(50, 894, 22), // "refreshVectorTableData"
QT_MOC_LITERAL(51, 917, 25), // "openTimeSetSettingsDialog"
QT_MOC_LITERAL(52, 943, 20), // "setupVectorTablePins"
QT_MOC_LITERAL(53, 964, 21), // "openPinSettingsDialog"
QT_MOC_LITERAL(54, 986, 8), // "gotoLine"
QT_MOC_LITERAL(55, 995, 28), // "onFontZoomSliderValueChanged"
QT_MOC_LITERAL(56, 1024, 15), // "onFontZoomReset"
QT_MOC_LITERAL(57, 1040, 8), // "closeTab"
QT_MOC_LITERAL(58, 1049, 15), // "loadCurrentPage"
QT_MOC_LITERAL(59, 1065, 12), // "loadNextPage"
QT_MOC_LITERAL(60, 1078, 12), // "loadPrevPage"
QT_MOC_LITERAL(61, 1091, 14), // "changePageSize"
QT_MOC_LITERAL(62, 1106, 7), // "newSize"
QT_MOC_LITERAL(63, 1114, 10), // "jumpToPage"
QT_MOC_LITERAL(64, 1125, 7), // "pageNum"
QT_MOC_LITERAL(65, 1133, 18), // "onTableCellChanged"
QT_MOC_LITERAL(66, 1152, 3), // "row"
QT_MOC_LITERAL(67, 1156, 6), // "column"
QT_MOC_LITERAL(68, 1163, 18), // "onTableRowModified"
QT_MOC_LITERAL(69, 1182, 24), // "showPinColumnContextMenu"
QT_MOC_LITERAL(70, 1207, 3), // "pos"
QT_MOC_LITERAL(71, 1211, 23), // "refreshSidebarNavigator"
QT_MOC_LITERAL(72, 1235, 20), // "onSidebarItemClicked"
QT_MOC_LITERAL(73, 1256, 16), // "QTreeWidgetItem*"
QT_MOC_LITERAL(74, 1273, 4), // "item"
QT_MOC_LITERAL(75, 1278, 16), // "onPinItemClicked"
QT_MOC_LITERAL(76, 1295, 20), // "onTimeSetItemClicked"
QT_MOC_LITERAL(77, 1316, 24), // "onVectorTableItemClicked"
QT_MOC_LITERAL(78, 1341, 18), // "onLabelItemClicked"
QT_MOC_LITERAL(79, 1360, 15), // "updateMenuState"
QT_MOC_LITERAL(80, 1376, 28), // "updateVectorColumnProperties"
QT_MOC_LITERAL(81, 1405, 27), // "calculateAndDisplayHexValue"
QT_MOC_LITERAL(82, 1433, 12), // "selectedRows"
QT_MOC_LITERAL(83, 1446, 16), // "onHexValueEdited"
QT_MOC_LITERAL(84, 1463, 16), // "validateHexInput"
QT_MOC_LITERAL(85, 1480, 4), // "text"
QT_MOC_LITERAL(86, 1485, 25), // "onNewViewSelectionChanged"
QT_MOC_LITERAL(87, 1511, 14), // "QItemSelection"
QT_MOC_LITERAL(88, 1526, 8), // "selected"
QT_MOC_LITERAL(89, 1535, 10), // "deselected"
QT_MOC_LITERAL(90, 1546, 36), // "updateVectorColumnPropertiesF..."
QT_MOC_LITERAL(91, 1583, 35), // "calculateAndDisplayHexValueFo..."
QT_MOC_LITERAL(92, 1619, 24), // "updateErrorCountForModel"
QT_MOC_LITERAL(93, 1644, 24), // "onHexValueEditedForModel"
QT_MOC_LITERAL(94, 1669, 17), // "setupWaveformView"
QT_MOC_LITERAL(95, 1687, 18), // "updateWaveformView"
QT_MOC_LITERAL(96, 1706, 18), // "toggleWaveformView"
QT_MOC_LITERAL(97, 1725, 4), // "show"
QT_MOC_LITERAL(98, 1730, 29), // "onWaveformPinSelectionChanged"
QT_MOC_LITERAL(99, 1760, 20), // "onShowAllPinsChanged"
QT_MOC_LITERAL(100, 1781, 5), // "state"
QT_MOC_LITERAL(101, 1787, 30), // "onWaveformContextMenuRequested"
QT_MOC_LITERAL(102, 1818, 26), // "setupWaveformClickHandling"
QT_MOC_LITERAL(103, 1845, 22), // "highlightWaveformPoint"
QT_MOC_LITERAL(104, 1868, 8), // "rowIndex"
QT_MOC_LITERAL(105, 1877, 8), // "pinIndex"
QT_MOC_LITERAL(106, 1886, 19), // "jumpToWaveformPoint"
QT_MOC_LITERAL(107, 1906, 7), // "pinName"
QT_MOC_LITERAL(108, 1914, 20), // "saveCurrentTableData"
QT_MOC_LITERAL(109, 1935, 23), // "onWaveformDoubleClicked"
QT_MOC_LITERAL(110, 1959, 12), // "QMouseEvent*"
QT_MOC_LITERAL(111, 1972, 5), // "event"
QT_MOC_LITERAL(112, 1978, 21), // "onWaveformValueEdited"
QT_MOC_LITERAL(113, 2000, 19), // "on_action_triggered"
QT_MOC_LITERAL(114, 2020, 7), // "checked"
QT_MOC_LITERAL(115, 2028, 35), // "onProjectStructureItemDoubleC..."
QT_MOC_LITERAL(116, 2064, 17), // "updateWindowTitle"
QT_MOC_LITERAL(117, 2082, 6) // "dbPath"

    },
    "MainWindow\0windowResized\0\0createNewProject\0"
    "createNewProjectWithNewArch\0"
    "openExistingProject\0openExistingProjectWithNewArch\0"
    "closeCurrentProject\0showDatabaseViewDialog\0"
    "showAddPinsDialog\0addSinglePin\0"
    "deletePins\0addPinsToDatabase\0"
    "QList<QString>\0pinNames\0showTimeSetDialog\0"
    "isInitialSetup\0showPinGroupDialog\0"
    "loadVectorTable\0openVectorTable\0tableId\0"
    "tableName\0onVectorTableSelectionChanged\0"
    "index\0onTabChanged\0saveVectorTableData\0"
    "addNewVectorTable\0showPinSelectionDialog\0"
    "showVectorDataDialog\0startIndex\0"
    "addRowToCurrentVectorTable\0"
    "addRowToCurrentVectorTableModel\0"
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
    "text\0onNewViewSelectionChanged\0"
    "QItemSelection\0selected\0deselected\0"
    "updateVectorColumnPropertiesForModel\0"
    "calculateAndDisplayHexValueForModel\0"
    "updateErrorCountForModel\0"
    "onHexValueEditedForModel\0setupWaveformView\0"
    "updateWaveformView\0toggleWaveformView\0"
    "show\0onWaveformPinSelectionChanged\0"
    "onShowAllPinsChanged\0state\0"
    "onWaveformContextMenuRequested\0"
    "setupWaveformClickHandling\0"
    "highlightWaveformPoint\0rowIndex\0"
    "pinIndex\0jumpToWaveformPoint\0pinName\0"
    "saveCurrentTableData\0onWaveformDoubleClicked\0"
    "QMouseEvent*\0event\0onWaveformValueEdited\0"
    "on_action_triggered\0checked\0"
    "onProjectStructureItemDoubleClicked\0"
    "updateWindowTitle\0dbPath"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_MainWindow[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      84,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,  434,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       3,    0,  435,    2, 0x08 /* Private */,
       4,    0,  436,    2, 0x08 /* Private */,
       5,    0,  437,    2, 0x08 /* Private */,
       6,    0,  438,    2, 0x08 /* Private */,
       7,    0,  439,    2, 0x08 /* Private */,
       8,    0,  440,    2, 0x08 /* Private */,
       9,    0,  441,    2, 0x08 /* Private */,
      10,    0,  442,    2, 0x08 /* Private */,
      11,    0,  443,    2, 0x08 /* Private */,
      12,    1,  444,    2, 0x08 /* Private */,
      15,    1,  447,    2, 0x08 /* Private */,
      15,    0,  450,    2, 0x28 /* Private | MethodCloned */,
      17,    0,  451,    2, 0x08 /* Private */,
      18,    0,  452,    2, 0x08 /* Private */,
      19,    2,  453,    2, 0x08 /* Private */,
      22,    1,  458,    2, 0x08 /* Private */,
      24,    1,  461,    2, 0x08 /* Private */,
      25,    0,  464,    2, 0x08 /* Private */,
      26,    0,  465,    2, 0x08 /* Private */,
      27,    2,  466,    2, 0x08 /* Private */,
      28,    3,  471,    2, 0x08 /* Private */,
      28,    2,  478,    2, 0x28 /* Private | MethodCloned */,
      30,    0,  483,    2, 0x08 /* Private */,
      31,    0,  484,    2, 0x08 /* Private */,
      32,    0,  485,    2, 0x08 /* Private */,
      33,    0,  486,    2, 0x08 /* Private */,
      34,    0,  487,    2, 0x08 /* Private */,
      35,    0,  488,    2, 0x08 /* Private */,
      36,    2,  489,    2, 0x08 /* Private */,
      40,    1,  494,    2, 0x08 /* Private */,
      43,    0,  497,    2, 0x08 /* Private */,
      44,    2,  498,    2, 0x08 /* Private */,
      46,    0,  503,    2, 0x08 /* Private */,
      47,    3,  504,    2, 0x08 /* Private */,
      50,    0,  511,    2, 0x08 /* Private */,
      51,    0,  512,    2, 0x08 /* Private */,
      52,    0,  513,    2, 0x08 /* Private */,
      53,    0,  514,    2, 0x08 /* Private */,
      54,    0,  515,    2, 0x08 /* Private */,
      55,    1,  516,    2, 0x08 /* Private */,
      56,    0,  519,    2, 0x08 /* Private */,
      57,    1,  520,    2, 0x08 /* Private */,
      58,    0,  523,    2, 0x08 /* Private */,
      59,    0,  524,    2, 0x08 /* Private */,
      60,    0,  525,    2, 0x08 /* Private */,
      61,    1,  526,    2, 0x08 /* Private */,
      63,    1,  529,    2, 0x08 /* Private */,
      65,    2,  532,    2, 0x08 /* Private */,
      68,    1,  537,    2, 0x08 /* Private */,
      69,    1,  540,    2, 0x08 /* Private */,
      71,    0,  543,    2, 0x08 /* Private */,
      72,    2,  544,    2, 0x08 /* Private */,
      75,    2,  549,    2, 0x08 /* Private */,
      76,    2,  554,    2, 0x08 /* Private */,
      77,    2,  559,    2, 0x08 /* Private */,
      78,    2,  564,    2, 0x08 /* Private */,
      79,    0,  569,    2, 0x08 /* Private */,
      80,    2,  570,    2, 0x08 /* Private */,
      81,    2,  575,    2, 0x08 /* Private */,
      83,    0,  580,    2, 0x08 /* Private */,
      84,    1,  581,    2, 0x08 /* Private */,
      86,    2,  584,    2, 0x08 /* Private */,
      90,    2,  589,    2, 0x08 /* Private */,
      91,    2,  594,    2, 0x08 /* Private */,
      92,    2,  599,    2, 0x08 /* Private */,
      93,    0,  604,    2, 0x08 /* Private */,
      94,    0,  605,    2, 0x08 /* Private */,
      95,    0,  606,    2, 0x08 /* Private */,
      96,    1,  607,    2, 0x08 /* Private */,
      98,    1,  610,    2, 0x08 /* Private */,
      99,    1,  613,    2, 0x08 /* Private */,
     101,    1,  616,    2, 0x08 /* Private */,
     102,    0,  619,    2, 0x08 /* Private */,
     103,    2,  620,    2, 0x08 /* Private */,
     103,    1,  625,    2, 0x28 /* Private | MethodCloned */,
     106,    2,  628,    2, 0x08 /* Private */,
     108,    0,  633,    2, 0x08 /* Private */,
     109,    1,  634,    2, 0x08 /* Private */,
     112,    0,  637,    2, 0x08 /* Private */,
     113,    1,  638,    2, 0x08 /* Private */,
     115,    2,  641,    2, 0x08 /* Private */,
     116,    1,  646,    2, 0x08 /* Private */,
     116,    0,  649,    2, 0x28 /* Private | MethodCloned */,

 // signals: parameters
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Bool,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Bool, 0x80000000 | 13,   14,
    QMetaType::Bool, QMetaType::Bool,   16,
    QMetaType::Bool,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::QString,   20,   21,
    QMetaType::Void, QMetaType::Int,   23,
    QMetaType::Void, QMetaType::Int,   23,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::QString,   20,   21,
    QMetaType::Void, QMetaType::Int, QMetaType::QString, QMetaType::Int,   20,   21,   29,
    QMetaType::Void, QMetaType::Int, QMetaType::QString,   20,   21,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString, 0x80000000 | 38,   37,   39,
    QMetaType::Void, 0x80000000 | 41,   42,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, 0x80000000 | 38,   45,   39,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::Int, 0x80000000 | 38,   48,   49,   39,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   37,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   23,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   62,
    QMetaType::Void, QMetaType::Int,   64,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,   66,   67,
    QMetaType::Void, QMetaType::Int,   66,
    QMetaType::Void, QMetaType::QPoint,   70,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 73, QMetaType::Int,   74,   67,
    QMetaType::Void, 0x80000000 | 73, QMetaType::Int,   74,   67,
    QMetaType::Void, 0x80000000 | 73, QMetaType::Int,   74,   67,
    QMetaType::Void, 0x80000000 | 73, QMetaType::Int,   74,   67,
    QMetaType::Void, 0x80000000 | 73, QMetaType::Int,   74,   67,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,   66,   67,
    QMetaType::Void, 0x80000000 | 38, QMetaType::Int,   82,   67,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   85,
    QMetaType::Void, 0x80000000 | 87, 0x80000000 | 87,   88,   89,
    QMetaType::Void, 0x80000000 | 38, QMetaType::Int,   82,   67,
    QMetaType::Void, 0x80000000 | 38, QMetaType::Int,   82,   67,
    QMetaType::Void, 0x80000000 | 38, QMetaType::Int,   82,   67,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,   97,
    QMetaType::Void, QMetaType::Int,   23,
    QMetaType::Void, QMetaType::Int,  100,
    QMetaType::Void, QMetaType::QPoint,   70,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,  104,  105,
    QMetaType::Void, QMetaType::Int,  104,
    QMetaType::Void, QMetaType::Int, QMetaType::QString,  104,  107,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 110,  111,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,  114,
    QMetaType::Void, 0x80000000 | 73, QMetaType::Int,   74,   67,
    QMetaType::Void, QMetaType::QString,  117,
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
        case 2: _t->createNewProjectWithNewArch(); break;
        case 3: _t->openExistingProject(); break;
        case 4: _t->openExistingProjectWithNewArch(); break;
        case 5: _t->closeCurrentProject(); break;
        case 6: _t->showDatabaseViewDialog(); break;
        case 7: { bool _r = _t->showAddPinsDialog();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 8: _t->addSinglePin(); break;
        case 9: _t->deletePins(); break;
        case 10: { bool _r = _t->addPinsToDatabase((*reinterpret_cast< const QList<QString>(*)>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 11: { bool _r = _t->showTimeSetDialog((*reinterpret_cast< bool(*)>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 12: { bool _r = _t->showTimeSetDialog();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 13: _t->showPinGroupDialog(); break;
        case 14: _t->loadVectorTable(); break;
        case 15: _t->openVectorTable((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2]))); break;
        case 16: _t->onVectorTableSelectionChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 17: _t->onTabChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 18: _t->saveVectorTableData(); break;
        case 19: _t->addNewVectorTable(); break;
        case 20: _t->showPinSelectionDialog((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2]))); break;
        case 21: _t->showVectorDataDialog((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2])),(*reinterpret_cast< int(*)>(_a[3]))); break;
        case 22: _t->showVectorDataDialog((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2]))); break;
        case 23: _t->addRowToCurrentVectorTable(); break;
        case 24: _t->addRowToCurrentVectorTableModel(); break;
        case 25: _t->deleteCurrentVectorTable(); break;
        case 26: _t->deleteSelectedVectorRows(); break;
        case 27: _t->deleteVectorRowsInRange(); break;
        case 28: _t->showFillVectorDialog(); break;
        case 29: _t->fillVectorForVectorTable((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< const QList<int>(*)>(_a[2]))); break;
        case 30: _t->fillVectorWithPattern((*reinterpret_cast< const QMap<int,QString>(*)>(_a[1]))); break;
        case 31: _t->showFillTimeSetDialog(); break;
        case 32: _t->fillTimeSetForVectorTable((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QList<int>(*)>(_a[2]))); break;
        case 33: _t->showReplaceTimeSetDialog(); break;
        case 34: _t->replaceTimeSetForVectorTable((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2])),(*reinterpret_cast< const QList<int>(*)>(_a[3]))); break;
        case 35: _t->refreshVectorTableData(); break;
        case 36: _t->openTimeSetSettingsDialog(); break;
        case 37: _t->setupVectorTablePins(); break;
        case 38: _t->openPinSettingsDialog(); break;
        case 39: _t->gotoLine(); break;
        case 40: _t->onFontZoomSliderValueChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 41: _t->onFontZoomReset(); break;
        case 42: _t->closeTab((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 43: _t->loadCurrentPage(); break;
        case 44: _t->loadNextPage(); break;
        case 45: _t->loadPrevPage(); break;
        case 46: _t->changePageSize((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 47: _t->jumpToPage((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 48: _t->onTableCellChanged((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 49: _t->onTableRowModified((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 50: _t->showPinColumnContextMenu((*reinterpret_cast< const QPoint(*)>(_a[1]))); break;
        case 51: _t->refreshSidebarNavigator(); break;
        case 52: _t->onSidebarItemClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 53: _t->onPinItemClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 54: _t->onTimeSetItemClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 55: _t->onVectorTableItemClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 56: _t->onLabelItemClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 57: _t->updateMenuState(); break;
        case 58: _t->updateVectorColumnProperties((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 59: _t->calculateAndDisplayHexValue((*reinterpret_cast< const QList<int>(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 60: _t->onHexValueEdited(); break;
        case 61: _t->validateHexInput((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 62: _t->onNewViewSelectionChanged((*reinterpret_cast< const QItemSelection(*)>(_a[1])),(*reinterpret_cast< const QItemSelection(*)>(_a[2]))); break;
        case 63: _t->updateVectorColumnPropertiesForModel((*reinterpret_cast< const QList<int>(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 64: _t->calculateAndDisplayHexValueForModel((*reinterpret_cast< const QList<int>(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 65: _t->updateErrorCountForModel((*reinterpret_cast< const QList<int>(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 66: _t->onHexValueEditedForModel(); break;
        case 67: _t->setupWaveformView(); break;
        case 68: _t->updateWaveformView(); break;
        case 69: _t->toggleWaveformView((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 70: _t->onWaveformPinSelectionChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 71: _t->onShowAllPinsChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 72: _t->onWaveformContextMenuRequested((*reinterpret_cast< const QPoint(*)>(_a[1]))); break;
        case 73: _t->setupWaveformClickHandling(); break;
        case 74: _t->highlightWaveformPoint((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 75: _t->highlightWaveformPoint((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 76: _t->jumpToWaveformPoint((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2]))); break;
        case 77: _t->saveCurrentTableData(); break;
        case 78: _t->onWaveformDoubleClicked((*reinterpret_cast< QMouseEvent*(*)>(_a[1]))); break;
        case 79: _t->onWaveformValueEdited(); break;
        case 80: _t->on_action_triggered((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 81: _t->onProjectStructureItemDoubleClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 82: _t->updateWindowTitle((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 83: _t->updateWindowTitle(); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 10:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QList<QString> >(); break;
            }
            break;
        case 29:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 1:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QList<int> >(); break;
            }
            break;
        case 32:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 1:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QList<int> >(); break;
            }
            break;
        case 34:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 2:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QList<int> >(); break;
            }
            break;
        case 59:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QList<int> >(); break;
            }
            break;
        case 62:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 1:
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QItemSelection >(); break;
            }
            break;
        case 63:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QList<int> >(); break;
            }
            break;
        case 64:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QList<int> >(); break;
            }
            break;
        case 65:
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
        if (_id < 84)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 84;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 84)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 84;
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
