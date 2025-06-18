@echo off
echo 正在复制Qt SQL驱动到测试程序目录...

:: 创建sqldrivers目录
if not exist "sqldrivers" mkdir sqldrivers

:: 复制SQLite驱动
set QT_DIR=C:\Qt\5.15.2\mingw81_32
if exist "%QT_DIR%\plugins\sqldrivers\qsqlite.dll" (
    copy "%QT_DIR%\plugins\sqldrivers\qsqlite.dll" "sqldrivers\"
    echo SQLite驱动复制成功！
) else (
    echo 错误：找不到SQLite驱动！
    echo 请调整QT_DIR变量以指向正确的Qt安装目录
)

:: 提示运行测试程序
echo.
echo 现在您可以运行 test_vector_model.exe 测试程序
echo.
pause 