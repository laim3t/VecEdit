/****************************************************************************
** Meta object code from reading C++ file 'mainwindow.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../app/mainwindow.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>
#include <QtCore/QList>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'mainwindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.8.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN10MainWindowE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN10MainWindowE = QtMocHelpers::stringData(
    "MainWindow",
    "windowResized",
    "",
    "createNewProject",
    "openExistingProject",
    "closeCurrentProject",
    "showDatabaseViewDialog",
    "showAddPinsDialog",
    "addSinglePin",
    "deletePins",
    "addPinsToDatabase",
    "pinNames",
    "showTimeSetDialog",
    "isInitialSetup",
    "showPinGroupDialog",
    "loadVectorTable",
    "onVectorTableSelectionChanged",
    "index",
    "onTabChanged",
    "saveVectorTableData",
    "addNewVectorTable",
    "showPinSelectionDialog",
    "tableId",
    "tableName",
    "showVectorDataDialog",
    "startIndex",
    "addRowToCurrentVectorTable",
    "deleteCurrentVectorTable",
    "deleteSelectedVectorRows",
    "deleteVectorRowsInRange",
    "showFillTimeSetDialog",
    "fillTimeSetForVectorTable",
    "timeSetId",
    "QList<int>",
    "selectedUiRows",
    "showReplaceTimeSetDialog",
    "replaceTimeSetForVectorTable",
    "fromTimeSetId",
    "toTimeSetId",
    "refreshVectorTableData",
    "openTimeSetSettingsDialog",
    "setupVectorTablePins",
    "openPinSettingsDialog",
    "gotoLine",
    "onFontZoomSliderValueChanged",
    "value",
    "onFontZoomReset",
    "closeTab"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN10MainWindowE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      36,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,  230,    2, 0x06,    1 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       3,    0,  231,    2, 0x08,    2 /* Private */,
       4,    0,  232,    2, 0x08,    3 /* Private */,
       5,    0,  233,    2, 0x08,    4 /* Private */,
       6,    0,  234,    2, 0x08,    5 /* Private */,
       7,    0,  235,    2, 0x08,    6 /* Private */,
       8,    0,  236,    2, 0x08,    7 /* Private */,
       9,    0,  237,    2, 0x08,    8 /* Private */,
      10,    1,  238,    2, 0x08,    9 /* Private */,
      12,    1,  241,    2, 0x08,   11 /* Private */,
      12,    0,  244,    2, 0x28,   13 /* Private | MethodCloned */,
      14,    0,  245,    2, 0x08,   14 /* Private */,
      15,    0,  246,    2, 0x08,   15 /* Private */,
      16,    1,  247,    2, 0x08,   16 /* Private */,
      18,    1,  250,    2, 0x08,   18 /* Private */,
      19,    0,  253,    2, 0x08,   20 /* Private */,
      20,    0,  254,    2, 0x08,   21 /* Private */,
      21,    2,  255,    2, 0x08,   22 /* Private */,
      24,    3,  260,    2, 0x08,   25 /* Private */,
      24,    2,  267,    2, 0x28,   29 /* Private | MethodCloned */,
      26,    0,  272,    2, 0x08,   32 /* Private */,
      27,    0,  273,    2, 0x08,   33 /* Private */,
      28,    0,  274,    2, 0x08,   34 /* Private */,
      29,    0,  275,    2, 0x08,   35 /* Private */,
      30,    0,  276,    2, 0x08,   36 /* Private */,
      31,    2,  277,    2, 0x08,   37 /* Private */,
      35,    0,  282,    2, 0x08,   40 /* Private */,
      36,    3,  283,    2, 0x08,   41 /* Private */,
      39,    0,  290,    2, 0x08,   45 /* Private */,
      40,    0,  291,    2, 0x08,   46 /* Private */,
      41,    0,  292,    2, 0x08,   47 /* Private */,
      42,    0,  293,    2, 0x08,   48 /* Private */,
      43,    0,  294,    2, 0x08,   49 /* Private */,
      44,    1,  295,    2, 0x08,   50 /* Private */,
      46,    0,  298,    2, 0x08,   52 /* Private */,
      47,    1,  299,    2, 0x08,   53 /* Private */,

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
    QMetaType::Bool, QMetaType::QStringList,   11,
    QMetaType::Bool, QMetaType::Bool,   13,
    QMetaType::Bool,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   17,
    QMetaType::Void, QMetaType::Int,   17,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::QString,   22,   23,
    QMetaType::Void, QMetaType::Int, QMetaType::QString, QMetaType::Int,   22,   23,   25,
    QMetaType::Void, QMetaType::Int, QMetaType::QString,   22,   23,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, 0x80000000 | 33,   32,   34,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::Int, 0x80000000 | 33,   37,   38,   34,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   45,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   17,

       0        // eod
};

Q_CONSTINIT const QMetaObject MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_meta_stringdata_ZN10MainWindowE.offsetsAndSizes,
    qt_meta_data_ZN10MainWindowE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN10MainWindowE_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<MainWindow, std::true_type>,
        // method 'windowResized'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'createNewProject'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'openExistingProject'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'closeCurrentProject'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'showDatabaseViewDialog'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'showAddPinsDialog'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'addSinglePin'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'deletePins'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'addPinsToDatabase'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QList<QString> &, std::false_type>,
        // method 'showTimeSetDialog'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'showTimeSetDialog'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'showPinGroupDialog'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'loadVectorTable'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onVectorTableSelectionChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onTabChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'saveVectorTableData'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'addNewVectorTable'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'showPinSelectionDialog'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'showVectorDataDialog'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'showVectorDataDialog'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'addRowToCurrentVectorTable'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'deleteCurrentVectorTable'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'deleteSelectedVectorRows'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'deleteVectorRowsInRange'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'showFillTimeSetDialog'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'fillTimeSetForVectorTable'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QList<int> &, std::false_type>,
        // method 'showReplaceTimeSetDialog'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'replaceTimeSetForVectorTable'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QList<int> &, std::false_type>,
        // method 'refreshVectorTableData'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'openTimeSetSettingsDialog'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'setupVectorTablePins'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'openPinSettingsDialog'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'gotoLine'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onFontZoomSliderValueChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onFontZoomReset'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'closeTab'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>
    >,
    nullptr
} };

void MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<MainWindow *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
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
        case 8: { bool _r = _t->addPinsToDatabase((*reinterpret_cast< std::add_pointer_t<QList<QString>>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 9: { bool _r = _t->showTimeSetDialog((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 10: { bool _r = _t->showTimeSetDialog();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 11: _t->showPinGroupDialog(); break;
        case 12: _t->loadVectorTable(); break;
        case 13: _t->onVectorTableSelectionChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 14: _t->onTabChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 15: _t->saveVectorTableData(); break;
        case 16: _t->addNewVectorTable(); break;
        case 17: _t->showPinSelectionDialog((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 18: _t->showVectorDataDialog((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3]))); break;
        case 19: _t->showVectorDataDialog((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 20: _t->addRowToCurrentVectorTable(); break;
        case 21: _t->deleteCurrentVectorTable(); break;
        case 22: _t->deleteSelectedVectorRows(); break;
        case 23: _t->deleteVectorRowsInRange(); break;
        case 24: _t->showFillTimeSetDialog(); break;
        case 25: _t->fillTimeSetForVectorTable((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QList<int>>>(_a[2]))); break;
        case 26: _t->showReplaceTimeSetDialog(); break;
        case 27: _t->replaceTimeSetForVectorTable((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QList<int>>>(_a[3]))); break;
        case 28: _t->refreshVectorTableData(); break;
        case 29: _t->openTimeSetSettingsDialog(); break;
        case 30: _t->setupVectorTablePins(); break;
        case 31: _t->openPinSettingsDialog(); break;
        case 32: _t->gotoLine(); break;
        case 33: _t->onFontZoomSliderValueChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 34: _t->onFontZoomReset(); break;
        case 35: _t->closeTab((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 25:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 1:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QList<int> >(); break;
            }
            break;
        case 27:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 2:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QList<int> >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (MainWindow::*)();
            if (_q_method_type _q_method = &MainWindow::windowResized; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
    }
}

const QMetaObject *MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN10MainWindowE.stringdata0))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 36)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 36;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 36)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 36;
    }
    return _id;
}

// SIGNAL 0
void MainWindow::windowResized()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
QT_WARNING_POP
