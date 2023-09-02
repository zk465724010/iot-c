::bat脚本的函数以标签(:tag)的形式定义
::以'call :tag args'的方式调用
@echo off

:: 获取当前文件夹名称
::pushd %1 & for %%i in (.) do set CURRENT_DIR_NAME=%%~ni
:: 获取当前路径
set ROOT=%cd%
set BUILD_DIR=build
set BUILD_TYPE=Debug
set INSTALL_PREFIX=%ROOT%\%BUILD_DIR%\install

call :main %BUILD_TYPE% %INSTALL_PREFIX%
goto end

:main
    :: 设置窗口大小
    @mode con lines=50 cols=80
    :: 设置窗口标题
    title cmake project ...
    call :check
    call :build %*
    echo.
    echo.
    echo.
    color 02
    echo   ----------------------------
    echo     Complete compilation *_*
    echo   ----------------------------
    ::timeout /t 10
    pause
    goto end

:check
    :: 检查编译目录
    if not exist %BUILD_DIR% ( mkdir %BUILD_DIR% )
    goto end
 
:build
    cd %BUILD_DIR%
    cmake .. -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=%1 -DCMAKE_INSTALL_PREFIX=%2
    goto end

:end
