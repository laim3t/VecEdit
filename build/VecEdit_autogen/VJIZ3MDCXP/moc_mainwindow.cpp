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
    QByteArrayData data[68];
    char stringdata0[1126];
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
QT_MOC_LITERAL(31, 519, 21), // "showFillTimeSetDialog"
QT_MOC_LITERAL(32, 541, 25), // "fillTimeSetForVectorTable"
QT_MOC_LITERAL(33, 567, 9), // "timeSetId"
QT_MOC_LITERAL(34, 577, 10), // "QList<int>"
QT_MOC_LITERAL(35, 588, 14), // "selectedUiRows"
QT_MOC_LITERAL(36, 603, 24), // "showReplaceTimeSetDialog"
QT_MOC_LITERAL(37, 628, 28), // "replaceTimeSetForVectorTable"
QT_MOC_LITERAL(38, 657, 13), // "fromTimeSetId"
QT_MOC_LITERAL(39, 671, 11), // "toTimeSetId"
QT_MOC_LITERAL(40, 683, 22), // "refreshVectorTableData"
QT_MOC_LITERAL(41, 706, 25), // "openTimeSetSettingsDialog"
QT_MOC_LITERAL(42, 732, 20), // "setupVectorTablePins"
QT_MOC_LITERAL(43, 753, 21), // "openPinSettingsDialog"
QT_MOC_LITERAL(44, 775, 8), // "gotoLine"
QT_MOC_LITERAL(45, 784, 28), // "onFontZoomSliderValueChanged"
QT_MOC_LITERAL(46, 813, 5), // "value"
QT_MOC_LITERAL(47, 819, 15), // "onFontZoomReset"
QT_MOC_LITERAL(48, 835, 8), // "closeTab"
QT_MOC_LITERAL(49, 844, 15), // "loadCurrentPage"
QT_MOC_LITERAL(50, 860, 12), // "loadNextPage"
QT_MOC_LITERAL(51, 873, 12), // "loadPrevPage"
QT_MOC_LITERAL(52, 886, 14), // "changePageSize"
QT_MOC_LITERAL(53, 901, 7), // "newSize"
QT_MOC_LITERAL(54, 909, 10), // "jumpToPage"
QT_MOC_LITERAL(55, 920, 7), // "pageNum"
QT_MOC_LITERAL(56, 928, 18), // "onTableCellChanged"
QT_MOC_LITERAL(57, 947, 3), // "row"
QT_MOC_LITERAL(58, 951, 6), // "column"
QT_MOC_LITERAL(59, 958, 18), // "onTableRowModified"
QT_MOC_LITERAL(60, 977, 23), // "refreshSidebarNavigator"
QT_MOC_LITERAL(61, 1001, 20), // "onSidebarItemClicked"
QT_MOC_LITERAL(62, 1022, 16), // "QTreeWidgetItem*"
QT_MOC_LITERAL(63, 1039, 4), // "item"
QT_MOC_LITERAL(64, 1044, 16), // "onPinItemClicked"
QT_MOC_LITERAL(65, 1061, 20), // "onTimeSetItemClicked"
QT_MOC_LITERAL(66, 1082, 24), // "onVectorTableItemClicked"
QT_MOC_LITERAL(67, 1107, 18) // "onLabelItemClicked"

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
    "deleteVectorRowsInRange\0showFillTimeSetDialog\0"
    "fillTimeSetForVectorTable\0timeSetId\0"
    "QList<int>\0selectedUiRows\0"
    "showReplaceTimeSetDialog\0"
    "replaceTimeSetForVectorTable\0fromTimeSetId\0"
    "toTimeSetId\0refreshVectorTableData\0"
    "openTimeSetSettingsDialog\0"
    "setupVectorTablePins\0openPinSettingsDialog\0"
    "gotoLine\0onFontZoomSliderValueChanged\0"
    "value\0onFontZoomReset\0closeTab\0"
    "loadCurrentPage\0loadNextPage\0loadPrevPage\0"
    "changePageSize\0newSize\0jumpToPage\0"
    "pageNum\0onTableCellChanged\0row\0column\0"
    "onTableRowModified\0refreshSidebarNavigator\0"
    "onSidebarItemClicked\0QTreeWidgetItem*\0"
    "item\0onPinItemClicked\0onTimeSetItemClicked\0"
    "onVectorTableItemClicked\0onLabelItemClicked"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_MainWindow[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      49,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,  259,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       3,    0,  260,    2, 0x08 /* Private */,
       4,    0,  261,    2, 0x08 /* Private */,
       5,    0,  262,    2, 0x08 /* Private */,
       6,    0,  263,    2, 0x08 /* Private */,
       7,    0,  264,    2, 0x08 /* Private */,
       8,    0,  265,    2, 0x08 /* Private */,
       9,    0,  266,    2, 0x08 /* Private */,
      10,    1,  267,    2, 0x08 /* Private */,
      13,    1,  270,    2, 0x08 /* Private */,
      13,    0,  273,    2, 0x28 /* Private | MethodCloned */,
      15,    0,  274,    2, 0x08 /* Private */,
      16,    0,  275,    2, 0x08 /* Private */,
      17,    1,  276,    2, 0x08 /* Private */,
      19,    1,  279,    2, 0x08 /* Private */,
      20,    0,  282,    2, 0x08 /* Private */,
      21,    0,  283,    2, 0x08 /* Private */,
      22,    2,  284,    2, 0x08 /* Private */,
      25,    3,  289,    2, 0x08 /* Private */,
      25,    2,  296,    2, 0x28 /* Private | MethodCloned */,
      27,    0,  301,    2, 0x08 /* Private */,
      28,    0,  302,    2, 0x08 /* Private */,
      29,    0,  303,    2, 0x08 /* Private */,
      30,    0,  304,    2, 0x08 /* Private */,
      31,    0,  305,    2, 0x08 /* Private */,
      32,    2,  306,    2, 0x08 /* Private */,
      36,    0,  311,    2, 0x08 /* Private */,
      37,    3,  312,    2, 0x08 /* Private */,
      40,    0,  319,    2, 0x08 /* Private */,
      41,    0,  320,    2, 0x08 /* Private */,
      42,    0,  321,    2, 0x08 /* Private */,
      43,    0,  322,    2, 0x08 /* Private */,
      44,    0,  323,    2, 0x08 /* Private */,
      45,    1,  324,    2, 0x08 /* Private */,
      47,    0,  327,    2, 0x08 /* Private */,
      48,    1,  328,    2, 0x08 /* Private */,
      49,    0,  331,    2, 0x08 /* Private */,
      50,    0,  332,    2, 0x08 /* Private */,
      51,    0,  333,    2, 0x08 /* Private */,
      52,    1,  334,    2, 0x08 /* Private */,
      54,    1,  337,    2, 0x08 /* Private */,
      56,    2,  340,    2, 0x08 /* Private */,
      59,    1,  345,    2, 0x08 /* Private */,
      60,    0,  348,    2, 0x08 /* Private */,
      61,    2,  349,    2, 0x08 /* Private */,
      64,    2,  354,    2, 0x08 /* Private */,
      65,    2,  359,    2, 0x08 /* Private */,
      66,    2,  364,    2, 0x08 /* Private */,
      67,    2,  369,    2, 0x08 /* Private */,

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
    QMetaType::Void, QMetaType::Int, 0x80000000 | 34,   33,   35,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::Int, 0x80000000 | 34,   38,   39,   35,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   46,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   18,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   53,
    QMetaType::Void, QMetaType::Int,   55,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,   57,   58,
    QMetaType::Void, QMetaType::Int,   57,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 62, QMetaType::Int,   63,   58,
    QMetaType::Void, 0x80000000 | 62, QMetaType::Int,   63,   58,
    QMetaType::Void, 0x80000000 | 62, QMetaType::Int,   63,   58,
    QMetaType::Void, 0x80000000 | 62, QMetaType::Int,   63,   58,
    QMetaType::Void, 0x80000000 | 62, QMetaType::Int,   63,   58,

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
        case 24: _t->showFillTimeSetDialog(); break;
        case 25: _t->fillTimeSetForVectorTable((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QList<int>(*)>(_a[2]))); break;
        case 26: _t->showReplaceTimeSetDialog(); break;
        case 27: _t->replaceTimeSetForVectorTable((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2])),(*reinterpret_cast< const QList<int>(*)>(_a[3]))); break;
        case 28: _t->refreshVectorTableData(); break;
        case 29: _t->openTimeSetSettingsDialog(); break;
        case 30: _t->setupVectorTablePins(); break;
        case 31: _t->openPinSettingsDialog(); break;
        case 32: _t->gotoLine(); break;
        case 33: _t->onFontZoomSliderValueChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 34: _t->onFontZoomReset(); break;
        case 35: _t->closeTab((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 36: _t->loadCurrentPage(); break;
        case 37: _t->loadNextPage(); break;
        case 38: _t->loadPrevPage(); break;
        case 39: _t->changePageSize((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 40: _t->jumpToPage((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 41: _t->onTableCellChanged((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 42: _t->onTableRowModified((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 43: _t->refreshSidebarNavigator(); break;
        case 44: _t->onSidebarItemClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 45: _t->onPinItemClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 46: _t->onTimeSetItemClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 47: _t->onVectorTableItemClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 48: _t->onLabelItemClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
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
        case 27:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 2:
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
        if (_id < 49)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 49;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 49)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 49;
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
