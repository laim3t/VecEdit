cmake_minimum_required(VERSION 3.16)

project(VecEdit VERSION 0.0.0.2 LANGUAGES CXX)

# 启用 Qt 的自动化功能
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 Qt5 包
find_package(Qt5 REQUIRED COMPONENTS Widgets Sql Concurrent)

# 从找到的 Qt5 包中安全地获取其 bin 目录的路径
get_filename_component(QT_BIN_DIR "${Qt5_DIR}/../../bin" ABSOLUTE)

# 设置日志输出相关的编译定义
add_compile_definitions(QT_MESSAGELOGCONTEXT)

# 定义项目源文件
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
)

# 定义 Qt 资源文件
set(PROJECT_RESOURCES
    resources/qt/icons.qrc
)

# 配置版本头文件
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/version.h"
    @ONLY
)

# 为 Windows 配置版本资源文件
if(WIN32)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/version.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
        @ONLY
    )
    list(APPEND PROJECT_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/version.rc")
endif()

# 为 Qt 资源编译器设置不压缩选项
set_source_files_properties(${PROJECT_RESOURCES} PROPERTIES AUTORCC_OPTIONS "--no-compress")

# 创建可执行文件
add_executable(VecEdit WIN32 MACOSX_BUNDLE
    ${PROJECT_SOURCES}
    ${PROJECT_RESOURCES}
)

# 链接所需的 Qt 库
target_link_libraries(VecEdit PRIVATE
    Qt5::Widgets
    Qt5::Sql
    Qt5::Concurrent
)

# 在构建后将 schema.sql 复制到可执行文件目录
add_custom_command(TARGET VecEdit POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/db/schema.sql"
        "$<TARGET_FILE_DIR:VecEdit>/schema.sql"
    COMMENT "正在复制 schema.sql 以便进行开发..."
)

# 添加必要的包含目录
target_include_directories(VecEdit PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR} # 用于 version.h 等生成文件
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/app
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/database
    ${CMAKE_CURRENT_SOURCE_DIR}/migration
    ${CMAKE_CURRENT_SOURCE_DIR}/pin
    ${CMAKE_CURRENT_SOURCE_DIR}/timeset
    ${CMAKE_CURRENT_SOURCE_DIR}/vector
)

# --- 安装与打包 ---
include(GNUInstallDirs)

# 1. 定义安装规则
install(TARGETS VecEdit
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime
    BUNDLE DESTINATION . COMPONENT Runtime
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Runtime
)

if(WIN32)
    # 2. 安装 schema.sql 文件
    install(FILES resources/db/schema.sql DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime)

    # 3. 偵錯腳本：檢查 CPack 內部的環境變數
    install(CODE "
        message(STATUS \"--- [偵錯] 開始檢查 CPack 內部環境 ---\")

        # 執行 Windows 的 cmd 命令來印出 PATH
        execute_process(
            COMMAND cmd /c \"echo %PATH%\"
            OUTPUT_VARIABLE cpack_env_path
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        message(STATUS \"[偵錯] CPack 腳本看到的 PATH 是: \${cpack_env_path}\")

        # 讓 CPack 腳本自己去找 windeployqt
        execute_process(
            COMMAND where windeployqt
            RESULT_VARIABLE find_result
            ERROR_QUIET
        )

        if(NOT find_result EQUAL 0)
            message(FATAL_ERROR \"[偵錯] 失敗！CPack 腳本在它自己的 PATH 中找不到 'windeployqt'。這證實了環境變數沒有被正確傳遞！\")
        else()
             message(STATUS \"[偵錯] 成功！CPack 腳本可以找到 'windeployqt'。這意味著問題可能更複雜。\")
        endif()

        message(STATUS \"--- [偵錯] 檢查結束 ---\")
    " COMPONENT Runtime)
endif()

# 4. 配置 CPack 用于创建最终的发布包
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "VecEdit Application")

# 指定需要打包的组件
set(CPACK_COMPONENTS_ALL Runtime)

# 指定打包格式
set(CPACK_GENERATOR "ZIP")
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)

# 必须在最后包含 CPack 模块
include(CPack)