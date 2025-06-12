/****************************************************************************
** Meta object code from reading C++ file 'timesetdialog.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../timeset/timesetdialog.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'timesetdialog.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_WaveComboDelegate_t {
    QByteArrayData data[1];
    char stringdata0[18];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_WaveComboDelegate_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_WaveComboDelegate_t qt_meta_stringdata_WaveComboDelegate = {
    {
QT_MOC_LITERAL(0, 0, 17) // "WaveComboDelegate"

    },
    "WaveComboDelegate"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_WaveComboDelegate[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

       0        // eod
};

void WaveComboDelegate::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_UNUSED(_o);
    Q_UNUSED(_id);
    Q_UNUSED(_c);
    Q_UNUSED(_a);
}

QT_INIT_METAOBJECT const QMetaObject WaveComboDelegate::staticMetaObject = { {
    QMetaObject::SuperData::link<QStyledItemDelegate::staticMetaObject>(),
    qt_meta_stringdata_WaveComboDelegate.data,
    qt_meta_data_WaveComboDelegate,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *WaveComboDelegate::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *WaveComboDelegate::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_WaveComboDelegate.stringdata0))
        return static_cast<void*>(this);
    return QStyledItemDelegate::qt_metacast(_clname);
}

int WaveComboDelegate::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QStyledItemDelegate::qt_metacall(_c, _id, _a);
    return _id;
}
struct qt_meta_stringdata_TimeSetDialog_t {
    QByteArrayData data[20];
    char stringdata0[279];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_TimeSetDialog_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_TimeSetDialog_t qt_meta_stringdata_TimeSetDialog = {
    {
QT_MOC_LITERAL(0, 0, 13), // "TimeSetDialog"
QT_MOC_LITERAL(1, 14, 10), // "addTimeSet"
QT_MOC_LITERAL(2, 25, 0), // ""
QT_MOC_LITERAL(3, 26, 13), // "removeTimeSet"
QT_MOC_LITERAL(4, 40, 12), // "updatePeriod"
QT_MOC_LITERAL(5, 53, 5), // "value"
QT_MOC_LITERAL(6, 59, 23), // "timeSetSelectionChanged"
QT_MOC_LITERAL(7, 83, 21), // "editTimeSetProperties"
QT_MOC_LITERAL(8, 105, 16), // "QTreeWidgetItem*"
QT_MOC_LITERAL(9, 122, 4), // "item"
QT_MOC_LITERAL(10, 127, 6), // "column"
QT_MOC_LITERAL(11, 134, 11), // "addEdgeItem"
QT_MOC_LITERAL(12, 146, 14), // "removeEdgeItem"
QT_MOC_LITERAL(13, 161, 12), // "editEdgeItem"
QT_MOC_LITERAL(14, 174, 19), // "onItemDoubleClicked"
QT_MOC_LITERAL(15, 194, 21), // "onPropertyItemChanged"
QT_MOC_LITERAL(16, 216, 21), // "onPinSelectionChanged"
QT_MOC_LITERAL(17, 238, 18), // "updatePinSelection"
QT_MOC_LITERAL(18, 257, 10), // "onAccepted"
QT_MOC_LITERAL(19, 268, 10) // "onRejected"

    },
    "TimeSetDialog\0addTimeSet\0\0removeTimeSet\0"
    "updatePeriod\0value\0timeSetSelectionChanged\0"
    "editTimeSetProperties\0QTreeWidgetItem*\0"
    "item\0column\0addEdgeItem\0removeEdgeItem\0"
    "editEdgeItem\0onItemDoubleClicked\0"
    "onPropertyItemChanged\0onPinSelectionChanged\0"
    "updatePinSelection\0onAccepted\0onRejected"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_TimeSetDialog[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      14,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,   84,    2, 0x08 /* Private */,
       3,    0,   85,    2, 0x08 /* Private */,
       4,    1,   86,    2, 0x08 /* Private */,
       6,    0,   89,    2, 0x08 /* Private */,
       7,    2,   90,    2, 0x08 /* Private */,
      11,    0,   95,    2, 0x08 /* Private */,
      12,    0,   96,    2, 0x08 /* Private */,
      13,    2,   97,    2, 0x08 /* Private */,
      14,    2,  102,    2, 0x08 /* Private */,
      15,    2,  107,    2, 0x08 /* Private */,
      16,    0,  112,    2, 0x08 /* Private */,
      17,    2,  113,    2, 0x08 /* Private */,
      18,    0,  118,    2, 0x08 /* Private */,
      19,    0,  119,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Double,    5,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 8, QMetaType::Int,    9,   10,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 8, QMetaType::Int,    9,   10,
    QMetaType::Void, 0x80000000 | 8, QMetaType::Int,    9,   10,
    QMetaType::Void, 0x80000000 | 8, QMetaType::Int,    9,   10,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 8, QMetaType::Int,    9,   10,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void TimeSetDialog::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<TimeSetDialog *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->addTimeSet(); break;
        case 1: _t->removeTimeSet(); break;
        case 2: _t->updatePeriod((*reinterpret_cast< double(*)>(_a[1]))); break;
        case 3: _t->timeSetSelectionChanged(); break;
        case 4: _t->editTimeSetProperties((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 5: _t->addEdgeItem(); break;
        case 6: _t->removeEdgeItem(); break;
        case 7: _t->editEdgeItem((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 8: _t->onItemDoubleClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 9: _t->onPropertyItemChanged((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 10: _t->onPinSelectionChanged(); break;
        case 11: _t->updatePinSelection((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 12: _t->onAccepted(); break;
        case 13: _t->onRejected(); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject TimeSetDialog::staticMetaObject = { {
    QMetaObject::SuperData::link<QDialog::staticMetaObject>(),
    qt_meta_stringdata_TimeSetDialog.data,
    qt_meta_data_TimeSetDialog,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *TimeSetDialog::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *TimeSetDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_TimeSetDialog.stringdata0))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int TimeSetDialog::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 14)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 14;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 14)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 14;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
