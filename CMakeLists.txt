cmake_minimum_required(VERSION 3.16)

project(VecEdit VERSION 0.0.4 LANGUAGES CXX)

# 启用 Qt 的自动化功能
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 Qt5 包
find_package(Qt5 REQUIRED COMPONENTS Widgets Sql Concurrent PrintSupport)

# 从找到的 Qt5 包中安全地获取其 bin 目录的路径
get_filename_component(QT_BIN_DIR "${Qt5_DIR}/../../bin" ABSOLUTE)

# 显式查找 windeployqt.exe 并打印路径以供调试
find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")
if(NOT WINDEPLOYQT_EXECUTABLE)
    message(FATAL_ERROR "Could not find windeployqt.exe. Please ensure it is in your PATH or Qt installation's bin directory.")
else()
    message(STATUS "Found windeployqt.exe at: ${WINDEPLOYQT_EXECUTABLE}")
endif()

# 设置日志输出相关的编译定义
add_compile_definitions(QT_MESSAGELOGCONTEXT)

# 定义项目源文件
set(PROJECT_SOURCES
        app/main.cpp
        app/mainwindow.cpp
        app/mainwindow.h
        app/puppetmastertablewidget.cpp
        app/puppetmastertablewidget.h
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
        vector/vectortablemodel.cpp
        vector/vectortablemodel.h
        vector/vectordatahandler.h
        vector/vectordatahandler.cpp
        vector/deleterangevectordialog.h
        vector/deleterangevectordialog.cpp
        vector/fillvectordialog.h
        vector/fillvectordialog.cpp
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

# 添加 QCustomPlot 库
set(QCUSTOMPLOT_SOURCES
    common/qcustomplot.cpp
    common/qcustomplot.h
)

# 创建可执行文件
add_executable(VecEdit WIN32 MACOSX_BUNDLE
    ${PROJECT_SOURCES}
    ${PROJECT_RESOURCES}
    ${QCUSTOMPLOT_SOURCES}
)

# 链接所需的 Qt 库
target_link_libraries(VecEdit PRIVATE
    Qt5::Widgets
    Qt5::Sql
    Qt5::Concurrent
    Qt5::PrintSupport  # QCustomPlot 需要 PrintSupport 模块
)

# 【新】为 VecEdit 目标启用预编译头文件
target_precompile_headers(VecEdit PRIVATE app/pch.h)

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

# 将安装路径设置回默认值，或一个您希望的固定目录
# set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install")

# 1. 定义安装规则
install(TARGETS VecEdit
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime
    BUNDLE DESTINATION . COMPONENT Runtime
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Runtime
)

if(WIN32)
    # 2. 【新流程】在"安装"阶段（而不是打包阶段）部署依赖项
    #    这会把所有依赖项和 VecEdit.exe 一起复制到 CMAKE_INSTALL_PREFIX 目录
    install(CODE "
        message(STATUS \"Deploying Qt dependencies to '${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}'...\")
        execute_process(
            COMMAND \"${WINDEPLOYQT_EXECUTABLE}\"
                --no-translations
                --no-compiler-runtime
                \"${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/VecEdit.exe\"
        )
    " COMPONENT Runtime)

    # 3. (仅限 MinGW) 手动部署 C++ 运行时库
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
        set(MINGW_RUNTIME_LIBS
            libgcc_s_dw2-1.dll
            libstdc++-6.dll
            libwinpthread-1.dll
        )
        foreach(LIB_NAME ${MINGW_RUNTIME_LIBS})
            if(EXISTS "${MINGW_BIN_DIR}/${LIB_NAME}")
                install(FILES "${MINGW_BIN_DIR}/${LIB_NAME}"
                    DESTINATION ${CMAKE_INSTALL_BINDIR}
                    COMPONENT Runtime
                )
            endif()
        endforeach()
    endif()
endif()

# 4. 配置 CPack 用于创建最终的发布包
#    CPack 现在只负责将 ${CMAKE_BINARY_DIR}/install 目录（由上一步的 install 命令生成）的内容打包
set(CPACK_INSTALLED_DIRECTORIES "${CMAKE_BINARY_DIR}/install;/")

set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "VecEdit Application")

# 指定打包格式
set(CPACK_GENERATOR "ZIP")

# 必须在最后包含 CPack 模块
include(CPack)