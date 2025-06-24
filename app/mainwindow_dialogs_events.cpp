// ==========================================================
//  Headers for: mainwindow_dialogs_events.cpp
// ==========================================================
#include "app/mainwindow.h"

// Qt Widgets & Core
#include <QMessageBox>
#include <QDebug>
#include <QInputDialog>
#include <QTableWidget>
#include <QMenu>
#include <QTreeWidget>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QHeaderView>
#include <QFileInfo>
#include <QTimer>

// Project-specific headers
#include "database/databasemanager.h"
#include "common/dialogmanager.h"
#include "timeset/timesetdialog.h"
#include "vector/vectordatahandler.h"
#include "common/tablestylemanager.h"
#include "vector/vectortabledelegate.h"

#include "mainwindow_dialogs.cpp"
#include "mainwindow_events_sidebar.cpp"
#include "mainwindow_events_views.cpp"
#include "mainwindow_actions_utils.cpp"