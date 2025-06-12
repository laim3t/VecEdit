/****************************************************************************
** Meta object code from reading C++ file 'databaseviewdialog.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../database/databaseviewdialog.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'databaseviewdialog.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_DatabaseViewDialog_t {
    QByteArrayData data[12];
    char stringdata0[152];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_DatabaseViewDialog_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_DatabaseViewDialog_t qt_meta_stringdata_DatabaseViewDialog = {
    {
QT_MOC_LITERAL(0, 0, 18), // "DatabaseViewDialog"
QT_MOC_LITERAL(1, 19, 22), // "onTableTreeItemClicked"
QT_MOC_LITERAL(2, 42, 0), // ""
QT_MOC_LITERAL(3, 43, 16), // "QTreeWidgetItem*"
QT_MOC_LITERAL(4, 60, 4), // "item"
QT_MOC_LITERAL(5, 65, 6), // "column"
QT_MOC_LITERAL(6, 72, 22), // "onRefreshButtonClicked"
QT_MOC_LITERAL(7, 95, 15), // "onFilterChanged"
QT_MOC_LITERAL(8, 111, 5), // "index"
QT_MOC_LITERAL(9, 117, 12), // "executeQuery"
QT_MOC_LITERAL(10, 130, 10), // "exportData"
QT_MOC_LITERAL(11, 141, 10) // "printTable"

    },
    "DatabaseViewDialog\0onTableTreeItemClicked\0"
    "\0QTreeWidgetItem*\0item\0column\0"
    "onRefreshButtonClicked\0onFilterChanged\0"
    "index\0executeQuery\0exportData\0printTable"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_DatabaseViewDialog[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       6,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    2,   44,    2, 0x08 /* Private */,
       6,    0,   49,    2, 0x08 /* Private */,
       7,    1,   50,    2, 0x08 /* Private */,
       9,    0,   53,    2, 0x08 /* Private */,
      10,    0,   54,    2, 0x08 /* Private */,
      11,    0,   55,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void, 0x80000000 | 3, QMetaType::Int,    4,    5,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    8,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void DatabaseViewDialog::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<DatabaseViewDialog *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->onTableTreeItemClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 1: _t->onRefreshButtonClicked(); break;
        case 2: _t->onFilterChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 3: _t->executeQuery(); break;
        case 4: _t->exportData(); break;
        case 5: _t->printTable(); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject DatabaseViewDialog::staticMetaObject = { {
    QMetaObject::SuperData::link<QDialog::staticMetaObject>(),
    qt_meta_stringdata_DatabaseViewDialog.data,
    qt_meta_data_DatabaseViewDialog,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *DatabaseViewDialog::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *DatabaseViewDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_DatabaseViewDialog.stringdata0))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int DatabaseViewDialog::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 6)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 6;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
