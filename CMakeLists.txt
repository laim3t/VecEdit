cmake_minimum_required(VERSION 3.16)

project(VecEdit VERSION 0.0.0.2 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 显式设置 Qt5_DIR 以确保 CMake 找到正确的 Qt 安装
set(Qt5_DIR C:/Qt/5.15.2/mingw81_64/lib/cmake/Qt5)

find_package(Qt5 REQUIRED COMPONENTS Widgets Sql Concurrent)

# 设置日志输出相关
add_compile_definitions(QT_MESSAGELOGCONTEXT)  # 启用日志上下文信息

set(PROJECT_SOURCES
        app/main.cpp
        app/mainwindow.cpp
        app/mainwindow.h
        database/databasemanager.cpp
        database/databasemanager.h
        database/databasemanager_migrator.cpp
        database/databaseviewdialog.cpp
        database/databaseviewdialog.h
        database/binaryfilehelper.h
        database/binaryfilehelper.cpp
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
        common/tablestyleperformancetester.h
        common/tablestyleperformancetester.cpp
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
        migration/datamigrator.h
        migration/datamigrator.cpp
        resources/qt/icons.qrc
)

# Configure a version header file to pass version info to C++ code
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/version.h"
    @ONLY
)

if(WIN32)
    # Configure a version resource file for Windows
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/version.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
        @ONLY
    )
    # Add the generated .rc file to the sources for Windows
    list(APPEND PROJECT_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/version.rc")
endif()

set(CMAKE_AUTORCC_OPTIONS "") # 为 Qt5 清除 AUTORCC 选项
set_source_files_properties("resources/qt/icons.qrc" PROPERTIES AUTORCC_OPTIONS "--no-compress") # 为 Qt5 rcc 设置 --no-compress 选项
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
    if(MINGW)
        set_target_properties(VecEdit PROPERTIES LINK_FLAGS "-mconsole")
    endif()
endif()

target_link_libraries(VecEdit PRIVATE
    Qt5::Widgets
    Qt5::Sql
    Qt5::Concurrent
)

# 添加包含目录
target_include_directories(VecEdit PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR} # For generated version.h
    ${CMAKE_CURRENT_SOURCE_DIR} # 保持根目录
    ${CMAKE_CURRENT_SOURCE_DIR}/app
    ${CMAKE_CURRENT_SOURCE_DIR}/timeset
    ${CMAKE_CURRENT_SOURCE_DIR}/pin
    ${CMAKE_CURRENT_SOURCE_DIR}/vector
    ${CMAKE_CURRENT_SOURCE_DIR}/database
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/migration
)

# 复制schema.sql到构建目录
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/resources/db/schema.sql
               ${CMAKE_CURRENT_BINARY_DIR}/schema.sql COPYONLY)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.VecEdit)
set_target_properties(VecEdit PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS VecEdit
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Deployment using windeployqt
if (WIN32)
    add_custom_command(TARGET VecEdit POST_BUILD
        COMMAND C:/Qt/5.15.2/mingw81_64/bin/windeployqt.exe # Hardcoded path to windeployqt
                --no-translations
                # For MinGW, windeployqt usually handles compiler runtime DLLs.
                # Add specific modules if needed, e.g., --svg, --multimedia, etc.
                # By default, it inspects the executable for dependencies.
                "$<TARGET_FILE:VecEdit>"
        COMMENT "Running windeployqt for Qt5 to deploy $<TARGET_FILE_NAME:VecEdit>"
    )
endif()
