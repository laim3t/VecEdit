// ==========================================================
//  Headers for: mainwindow_pins_timeset.cpp
// ==========================================================
#include "app/mainwindow.h"

// Qt Widgets & Core
#include <QMessageBox>
#include <QInputDialog>
#include <QDialog>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTimer>

// Project-specific headers
#include "database/databasemanager.h"
#include "pin/vectorpinsettingsdialog.h"
#include "pin/pinsettingsdialog.h"
#include "timeset/filltimesetdialog.h"
#include "timeset/replacetimesetdialog.h"
#include "migration/datamigrator.h"
#include "common/utils/pathutils.h"
#include "../database/binaryfilehelper.h"



#include "mainwindow_pins.cpp"
#include "mainwindow_timeset_ops.cpp"