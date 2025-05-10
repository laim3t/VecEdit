cmake_minimum_required(VERSION 3.16)

project(VecEdit VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Sql)

# 设置日志输出相关
add_compile_definitions(QT_MESSAGELOGCONTEXT)  # 启用日志上下文信息
# 查找 Qt6 的 Core, Gui, Widgets 组件
find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)

set(PROJECT_SOURCES
        app/main.cpp
        app/mainwindow.cpp
        app/mainwindow.h
        database/databasemanager.cpp
        database/databasemanager.h
        database/databaseviewdialog.cpp
        database/databaseviewdialog.h
        pin/pinlistdialog.cpp
        pin/pinlistdialog.h
        pin/pingroupdialog.h
        pin/pingroupdialog.cpp
        timeset/timesetdialog.cpp
        timeset/timesetdialog.h
        timeset/timesetedgedialog.cpp
        timeset/timesetedgedialog.h
        timeset/filltimesetdialog.cpp
        timeset/filltimesetdialog.h
        timeset/replacetimesetdialog.cpp
        timeset/replacetimesetdialog.h
        pin/pinvalueedit.h
        pin/pinvalueedit.cpp
        pin/vectorpinsettingsdialog.h
        pin/vectorpinsettingsdialog.cpp
        pin/pinsettingsdialog.h
        pin/pinsettingsdialog.cpp
        vector/vectortabledelegate.h
        vector/vectortabledelegate.cpp
        vector/vectordatahandler.h
        vector/vectordatahandler.cpp
        vector/deleterangevectordialog.h
        vector/deleterangevectordialog.cpp
        common/dialogmanager.h
        common/dialogmanager.cpp
        common/tablestylemanager.h
        common/tablestylemanager.cpp
        common/logger.h
        common/logger.cpp
        timeset/timesetdataaccess.h
        timeset/timesetdataaccess.cpp
        timeset/timesetui.h
        timeset/timesetui.cpp
        timeset/timesetedgemanager.h
        timeset/timesetedgemanager.cpp
        pin/pinselectionmanager.h
        pin/pinselectionmanager.cpp
        vector/vectordatamanager.h
        vector/vectordatamanager.cpp
        resources/qt/icons.qrc
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(VecEdit
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET VecEdit APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(VecEdit SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(VecEdit
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(VecEdit PRIVATE 
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Sql
)

# 添加包含目录
target_include_directories(VecEdit PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} # 保持根目录
    ${CMAKE_CURRENT_SOURCE_DIR}/app
    ${CMAKE_CURRENT_SOURCE_DIR}/timeset
    ${CMAKE_CURRENT_SOURCE_DIR}/pin
    ${CMAKE_CURRENT_SOURCE_DIR}/vector
    ${CMAKE_CURRENT_SOURCE_DIR}/database
    ${CMAKE_CURRENT_SOURCE_DIR}/common
)

# 复制schema.sql到构建目录
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/resources/db/schema.sql
               ${CMAKE_CURRENT_BINARY_DIR}/schema.sql COPYONLY)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.VecEdit)
endif()
set_target_properties(VecEdit PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS VecEdit
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(VecEdit)
endif()
if (WIN32 AND QT_VERSION_MAJOR EQUAL 6)
    add_custom_command(TARGET VecEdit POST_BUILD
      COMMAND ${Qt6_DIR}/../../../bin/windeployqt
              --no-translations --no-quick-import
              "$<TARGET_FILE:VecEdit>"
    )
endif()
