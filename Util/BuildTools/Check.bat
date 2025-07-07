chcp 65001
@echo off
setlocal enabledelayedexpansion
:: Modified from Package.bat

rem don't remove next two empty lines after next
set LF=^


rem Bat script that compiles and exports the carla project (carla.org)
rem Run it through a cmd with the x64 Visual C++ Toolset enabled.
rem https://wiki.unrealengine.com/How_to_package_your_game_with_commands

:: 只表示将要“运行的”bat命令的folder，不包含bat名称自己。
:: 注意，不是“运行处”的folder （该功能用%cd%实现）
:: %0-%9代表的是batch文件的参数。%1-%9 是batch名称之后的命令行参数，%0代表batch文件自己。
:: d表示盘符，p表示不带盘符的路径，那么dp就表示带盘符的路径
set LOCAL_PATH=%~dp0
:: 当前运行脚本的文件名
:: -[Check]
set FILE_N=-[%~n0]:

:: 打印批处理脚本的参数（为了调试）
rem Print batch params (debug purpose)
:: -[Check]: [Batch params]:
echo %FILE_N% [Batch params]: %*

:: 衡量测试的总共时间
rem Measure overall execution time of testing
call :get_current_time_in_seconds T_START_OVERALL

:: 解析参数
rem ==============================================================================
rem -- Parse arguments -----------------------------------------------------------
rem ==============================================================================

set DOC_STRING="Run unit tests."
set USAGE_STRING="Usage: %FILE_N% [-h|--help] [--gdb] [--xml] [--gtest_args=ARGS] [--python-version=VERSION]"

set XML_OUTPUT=false
set LIBCARLA_RELEASE=false
set LIBCARLA_DEBUG=false
set SMOKE_TESTS=false
set PYTHON_API=false
set RUN_BENCHMARK=false
set MEASURE_TIME=true
set python_dir=C:\software\anaconda3\envs\carla_dev\
set python_path=%python_dir%python.exe
set pip_path=%python_dir%Scripts\pip.exe

:arg-parse
if not "%1"=="" (
    if "%1"=="--all" (
        set SMOKE_TESTS=true
        set LIBCARLA_RELEASE=true
        set LIBCARLA_DEBUG=true
        set PYTHON_API=true
    )

    if "%1"=="--xml" (
        set XML_OUTPUT=true
        shift
    )

    if "%1"=="--smoke" (
        set SMOKE_TESTS=true
        shift
    )

    if "%1"=="--python-api" (
        set PYTHON_API=true
        shift
    )

    if "%1"=="-h" (
        echo %DOC_STRING%
        echo %USAGE_STRING%
        GOTO :eof
    )

    if "%1"=="--help" (
        echo %DOC_STRING%
        echo %USAGE_STRING%
        GOTO :eof
    )

    shift
    goto :arg-parse
)


rem ============================================================================
rem -- Launch Serve for test ---------------------------------------------------
rem ============================================================================

:: 获取CarlaUE4所在的目录（参考Package.bat）
for /f %%i in ('git describe --tags --dirty --always') do set CARLA_VERSION=%%i
if not defined CARLA_VERSION goto bad_exit

set BUILD_FOLDER=%INSTALLATION_DIR%UE4Carla/%CARLA_VERSION%/
:: 仅用于调试
:: set BUILD_FOLDER=C:\ProgramData\Jenkins\.jenkins\workspace\carla\Build\UE4Carla\8617b519\

set exe_path=%BUILD_FOLDER:/=\%WindowsNoEditor\CarlaUE4.exe

:: 必须使用start来启动一个新的服务进程，否则会卡死 -RenderOffscreen
echo Unreal service is launching with command: start %exe_path% -RenderOffscreen ...
:: 如果exe文件不存在，会卡在这里
if exist %exe_path% (
    start %exe_path% -RenderOffscreen
) else (
    echo Error: %exe_path% not exitst.
    goto bad_exit
)

:: 安装最新编译的PythonAPI
pushd %ROOT_PATH%PythonAPI\carla\dist
:: TODO python 使用指定版本编译hutb；
:: pip install --force-reinstall C:\ProgramData\Jenkins\.jenkins\workspace\carla\PythonAPI\carla\dist\hutb-1.0.0-cp37-cp37m-win_amd64.whl
echo %pip_path% install --force-reinstall  %BUILD_FOLDER:/=\%WindowsNoEditor\PythonAPI\carla\dist\hutb-1.0.0-cp37-cp37m-win_amd64.whl
%pip_path% install --force-reinstall  %BUILD_FOLDER:/=\%WindowsNoEditor\PythonAPI\carla\dist\hutb-1.0.0-cp37-cp37m-win_amd64.whl
where pip
popd %ROOT_PATH%PythonAPI\carla\dist


rem ============================================================================
rem -- Run Python API unit tests -----------------------------------------------
rem ============================================================================

echo pushd %ROOT_PATH%PythonAPI\test\unit
pushd %ROOT_PATH%PythonAPI\test\unit
cd %ROOT_PATH%PythonAPI\test\unit

if %XML_OUTPUT%==true (
    set EXTRA_ARGS="-X"
) else (
    set EXTRA_ARGS=""
)

if %PYTHON_API%==true (
    echo Running Python API for Python %PY_VERSION% unit tests.
    :: TODO pip install nose2 -i http://mirrors.aliyun.com/pypi/simple --trusted-host mirrors.aliyun.com
    echo Current directory: %cd%
    echo Test command: %python_path% -m nose2
    :: %python_path% -m nose2
    %python_path% -m unittest test_transform.TestTransform.test_list_rotation_and_translation_location
    %python_path% -m unittest test_vehicle.TestVehicleControl.test_default_values

    if %XML_OUTPUT%==true (
        move test-results.xml %CARLA_TEST_RESULTS_FOLDER%\python-api-3.xml
    )
)

popd %ROOT_PATH%PythonAPI\test\unit



rem ============================================================================
rem -- Run smoke tests ---------------------------------------------------------
rem ============================================================================

call :get_current_time_in_seconds T_START_DO_TEST

if %SMOKE_TESTS%==true (
    echo test connection ...
    :: TODO 替换为相对Python环境路径
    %python_path%  %ROOT_PATH%PythonAPI/util/test_connection.py -p 2000 --timeout=60.0
    echo test connection done.
)

call :get_current_time_in_seconds T_END_DO_TEST
set /A ELAPSED_TIME=!T_END_DO_TEST! - !T_START_DO_TEST!
if %MEASURE_TIME%==true if %SMOKE_TESTS%==true echo %FILE_N% [TIME]: Running smoke test took !ELAPSED_TIME! seconds.



rem ============================================================================
:: 杀死服务端
for /f "tokens=5" %%a in ('netstat -ano ^| findstr :2000') do taskkill /F /PID %%a


call :get_current_time_in_seconds T_END_OVERALL
set /A ELAPSED_TIME=!T_END_OVERALL! - !T_START_OVERALL!
if %MEASURE_TIME%==true echo %FILE_N% [TIME]: Overall testing took !ELAPSED_TIME! seconds.


goto success




rem ============================================================================
rem -- Helper functions --------------------------------------------------------
rem ============================================================================

:get_current_time_in_seconds
    for /f %%a in ('powershell -command "[int]((Get-Date -UFormat '%%s') -split ',.')[0]"') do set %1=%%a
    goto :eof


rem ============================================================================
rem -- Messages and Errors -----------------------------------------------------
rem ============================================================================

:success
    echo.
    goto good_exit

:good_exit
    endlocal
    exit /b 0

:bad_exit
    endlocal
    exit /b 1
