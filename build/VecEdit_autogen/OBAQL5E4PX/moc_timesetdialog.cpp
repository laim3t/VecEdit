/****************************************************************************
** Meta object code from reading C++ file 'timesetdialog.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../timeset/timesetdialog.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'timesetdialog.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.8.3. It"
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
struct qt_meta_tag_ZN17WaveComboDelegateE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN17WaveComboDelegateE = QtMocHelpers::stringData(
    "WaveComboDelegate"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN17WaveComboDelegateE[] = {

 // content:
      12,       // revision
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

Q_CONSTINIT const QMetaObject WaveComboDelegate::staticMetaObject = { {
    QMetaObject::SuperData::link<QStyledItemDelegate::staticMetaObject>(),
    qt_meta_stringdata_ZN17WaveComboDelegateE.offsetsAndSizes,
    qt_meta_data_ZN17WaveComboDelegateE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN17WaveComboDelegateE_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<WaveComboDelegate, std::true_type>
    >,
    nullptr
} };

void WaveComboDelegate::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<WaveComboDelegate *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *WaveComboDelegate::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *WaveComboDelegate::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN17WaveComboDelegateE.stringdata0))
        return static_cast<void*>(this);
    return QStyledItemDelegate::qt_metacast(_clname);
}

int WaveComboDelegate::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QStyledItemDelegate::qt_metacall(_c, _id, _a);
    return _id;
}
namespace {
struct qt_meta_tag_ZN13TimeSetDialogE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN13TimeSetDialogE = QtMocHelpers::stringData(
    "TimeSetDialog",
    "addTimeSet",
    "",
    "removeTimeSet",
    "updatePeriod",
    "value",
    "timeSetSelectionChanged",
    "editTimeSetProperties",
    "QTreeWidgetItem*",
    "item",
    "column",
    "addEdgeItem",
    "removeEdgeItem",
    "editEdgeItem",
    "onItemDoubleClicked",
    "onPropertyItemChanged",
    "onPinSelectionChanged",
    "updatePinSelection",
    "onAccepted",
    "onRejected"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN13TimeSetDialogE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      14,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   98,    2, 0x08,    1 /* Private */,
       3,    0,   99,    2, 0x08,    2 /* Private */,
       4,    1,  100,    2, 0x08,    3 /* Private */,
       6,    0,  103,    2, 0x08,    5 /* Private */,
       7,    2,  104,    2, 0x08,    6 /* Private */,
      11,    0,  109,    2, 0x08,    9 /* Private */,
      12,    0,  110,    2, 0x08,   10 /* Private */,
      13,    2,  111,    2, 0x08,   11 /* Private */,
      14,    2,  116,    2, 0x08,   14 /* Private */,
      15,    2,  121,    2, 0x08,   17 /* Private */,
      16,    0,  126,    2, 0x08,   20 /* Private */,
      17,    2,  127,    2, 0x08,   21 /* Private */,
      18,    0,  132,    2, 0x08,   24 /* Private */,
      19,    0,  133,    2, 0x08,   25 /* Private */,

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

Q_CONSTINIT const QMetaObject TimeSetDialog::staticMetaObject = { {
    QMetaObject::SuperData::link<QDialog::staticMetaObject>(),
    qt_meta_stringdata_ZN13TimeSetDialogE.offsetsAndSizes,
    qt_meta_data_ZN13TimeSetDialogE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN13TimeSetDialogE_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<TimeSetDialog, std::true_type>,
        // method 'addTimeSet'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'removeTimeSet'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updatePeriod'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        // method 'timeSetSelectionChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'editTimeSetProperties'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QTreeWidgetItem *, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'addEdgeItem'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'removeEdgeItem'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'editEdgeItem'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QTreeWidgetItem *, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onItemDoubleClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QTreeWidgetItem *, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onPropertyItemChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QTreeWidgetItem *, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onPinSelectionChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updatePinSelection'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QTreeWidgetItem *, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onAccepted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onRejected'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void TimeSetDialog::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<TimeSetDialog *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->addTimeSet(); break;
        case 1: _t->removeTimeSet(); break;
        case 2: _t->updatePeriod((*reinterpret_cast< std::add_pointer_t<double>>(_a[1]))); break;
        case 3: _t->timeSetSelectionChanged(); break;
        case 4: _t->editTimeSetProperties((*reinterpret_cast< std::add_pointer_t<QTreeWidgetItem*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 5: _t->addEdgeItem(); break;
        case 6: _t->removeEdgeItem(); break;
        case 7: _t->editEdgeItem((*reinterpret_cast< std::add_pointer_t<QTreeWidgetItem*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 8: _t->onItemDoubleClicked((*reinterpret_cast< std::add_pointer_t<QTreeWidgetItem*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 9: _t->onPropertyItemChanged((*reinterpret_cast< std::add_pointer_t<QTreeWidgetItem*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 10: _t->onPinSelectionChanged(); break;
        case 11: _t->updatePinSelection((*reinterpret_cast< std::add_pointer_t<QTreeWidgetItem*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 12: _t->onAccepted(); break;
        case 13: _t->onRejected(); break;
        default: ;
        }
    }
}

const QMetaObject *TimeSetDialog::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *TimeSetDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN13TimeSetDialogE.stringdata0))
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
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 14)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 14;
    }
    return _id;
}
QT_WARNING_POP
