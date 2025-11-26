@echo off
setlocal enabledelayedexpansion
chcp 65001

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
for /f %%i in ('git rev-parse --short HEAD') do set CARLA_VERSION=%%i
if not defined CARLA_VERSION goto bad_exit

:: 如果存在dirty后缀，则表示是多个版本切换后编译的包，测试时也使用该包
:: （解决测试时候找不到dirty目录中的可执行文件的问题）
if exist %INSTALLATION_DIR%UE4Carla/%CARLA_VERSION%-dirty/ (
    set CARLA_VERSION=%CARLA_VERSION%-dirty
)

set BUILD_FOLDER=%INSTALLATION_DIR%UE4Carla/%CARLA_VERSION%/
:: debug only (rename with no dirty)
if %IS_DEBUG%==true (
    set BUILD_FOLDER=D:\hutb\Build\UE4Carla\e392521d5-dirty_Carla\
)

set exe_path=%BUILD_FOLDER:/=\%WindowsNoEditor\CarlaUE4.exe

:: If exist CarlaUE4.exe process, kill it
for /f "tokens=5" %%a in ('netstat -ano ^| findstr :3654') do taskkill /F /PID %%a
:: use command "start" to launch a new service process. Otherwise stuck
echo Unreal service is launching with command: start %exe_path% -RenderOffscreen --carla-rpc-port=3654 --carla-streaming-port=0 -nosound
if exist %exe_path% (
    start %exe_path% -RenderOffscreen --carla-rpc-port=3654 --carla-streaming-port=0 -nosound
) else (
    echo Error: %exe_path% not exitst.
    goto bad_exit
)



rem ============================================================================
rem -- Run Python API unit tests -----------------------------------------------
rem ============================================================================


if %XML_OUTPUT%==true (
    set EXTRA_ARGS="-X"
) else (
    set EXTRA_ARGS=""
)

if %PYTHON_API%==true (
    echo Current directory: %cd%
    for /l %%i in (7,-1,7) do (
        echo Running Python API for Python Python.%%i unit tests.
        where conda
        :: call D:\software\anaconda3\Scripts\activate.bat hutb_3.%%i && python --version
        call conda activate hutb_3.%%i
        echo Current Python path: 
        where python
        echo Current Pip path:
        where pip
        pip install nose2 -i http://mirrors.aliyun.com/pypi/simple --trusted-host mirrors.aliyun.com
        if %%i==7 (
            echo pip install --force-reinstall  %BUILD_FOLDER:\=/%WindowsNoEditor/PythonAPI/carla/dist/hutb-%API_VERSION%-cp3%%i-cp3%%im-win_amd64.whl
            pip uninstall --yes hutb
            pip install  %BUILD_FOLDER:\=/%WindowsNoEditor/PythonAPI/carla/dist/hutb-%API_VERSION%-cp3%%i-cp3%%im-win_amd64.whl
        ) else (
            echo pip install --force-reinstall  %BUILD_FOLDER:\=/%WindowsNoEditor/PythonAPI/carla/dist/hutb-%API_VERSION%-cp3%%i-cp3%%i-win_amd64.whl
            pip uninstall --yes hutb
            pip install  %BUILD_FOLDER:\=/%WindowsNoEditor/PythonAPI/carla/dist/hutb-%API_VERSION%-cp3%%i-cp3%%i-win_amd64.whl
        )
        cd %ROOT_PATH%PythonAPI\test\unit\
        if %IS_DEBUG%==true (
            :: skip test client
            python -m nose2 test_transform
            python -m nose2 test_vehicle
        ) else (
            :: PythonAPI client version == git rev-parse --short HEAD
            :: python -m nose2
            python -m nose2 test_transform
            python -m nose2 test_vehicle
        )
    )

    if %XML_OUTPUT%==true (
        move test-results.xml %CARLA_TEST_RESULTS_FOLDER%\python-api-3.xml
    )
)


cd %ROOT_PATH:/=\%PythonAPI\test\



rem ============================================================================
rem -- Run smoke tests ---------------------------------------------------------
rem ============================================================================

call :get_current_time_in_seconds T_START_DO_TEST


if %SMOKE_TESTS%==true (
    echo Current directory: %cd%
    for /f "delims=*" %%a in (smoke_test_list.txt) do (
        set smoke_list=%%a
        goto :read_done
    )
    :read_done
    echo Smoke list: %smoke_list%
    for /l %%i in (7,-1,7) do (
        echo Running smoke tests for Python 3.%%i
        call conda activate hutb_3.%%i
        echo Current Python path: 
        where python
        pip install nose2 -i http://mirrors.aliyun.com/pypi/simple --trusted-host mirrors.aliyun.com
        if %%i==7 (
            echo pip install --force-reinstall  %BUILD_FOLDER:\=/%WindowsNoEditor/PythonAPI/carla/dist/hutb-%API_VERSION%-cp3%%i-cp3%%im-win_amd64.whl
            pip install --force-reinstall  %BUILD_FOLDER:\=/%WindowsNoEditor/PythonAPI/carla/dist/hutb-%API_VERSION%-cp3%%i-cp3%%im-win_amd64.whl
        ) else (
            echo pip install --force-reinstall  %BUILD_FOLDER:\=/%WindowsNoEditor/PythonAPI/carla/dist/hutb-%API_VERSION%-cp3%%i-cp3%%i-win_amd64.whl
            pip uninstall --yes hutb
            pip install  %BUILD_FOLDER:\=/%WindowsNoEditor/PythonAPI/carla/dist/hutb-%API_VERSION%-cp3%%i-cp3%%i-win_amd64.whl
        )
        pip install -r requirements.txt -i http://mirrors.aliyun.com/pypi/simple --trusted-host mirrors.aliyun.com
        echo python -m nose2 -v %smoke_list%
        python -m nose2 -v %smoke_list%
    )
)



call :get_current_time_in_seconds T_END_DO_TEST
set /A ELAPSED_TIME=!T_END_DO_TEST! - !T_START_DO_TEST!
if %MEASURE_TIME%==true if %SMOKE_TESTS%==true echo %FILE_N% [TIME]: Running smoke test took !ELAPSED_TIME! seconds.



rem ============================================================================
:: 杀死服务端
for /f "tokens=5" %%a in ('netstat -ano ^| findstr :3654') do taskkill /F /PID %%a


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
