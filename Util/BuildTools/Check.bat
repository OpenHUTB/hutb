@echo off
setlocal enabledelayedexpansion
chcp 65001

:: Modified from Package.bat

rem don't remove next two empty lines after next
set LF=^


rem Bat script that compiles and exports the carla project (carla.org)
rem Run it through a cmd with the x64 Visual C++ Toolset enabled.
rem https://wiki.unrealengine.com/How_to_package_your_game_with_commands

rem 只表示将要“运行的”bat命令的folder，不包含bat名称自己。
rem 注意，不是“运行处”的folder （该功能用%cd%实现）
rem %0-%9代表的是batch文件的参数。%1-%9 是batch名称之后的命令行参数，%0代表batch文件自己。
rem d表示盘符，p表示不带盘符的路径，那么dp就表示带盘符的路径
set LOCAL_PATH=%~dp0
rem 当前运行脚本的文件名
rem -[Check]
set FILE_N=-[%~n0]:

rem 打印批处理脚本的参数（为了调试）
rem Print batch params (debug purpose)
:: -[Check]: [Batch params]:
echo %FILE_N% [Batch params]: %*

rem 衡量测试的总共时间
rem Measure overall execution time of testing
call :get_current_time_in_seconds T_START_OVERALL

:: 解析参数
rem ==============================================================================
rem -- Parse arguments -----------------------------------------------------------
rem ==============================================================================

set DOC_STRING="Run unit tests."
set USAGE_STRING="Usage: %FILE_N% [-h|--help] [--gdb] [--xml] [--gtest_args=ARGS] [--python-version=VERSION]"

set IS_DEBUG=false

set XML_OUTPUT=false
set LIBCARLA_RELEASE=false
set LIBCARLA_DEBUG=false
set SMOKE_TESTS=false
set VR_TESTS=false
set PYTHON_API=false
set RUN_BENCHMARK=false
set AIR_TESTS=false
set MEASURE_TIME=true
set UPLOAD_DOWNLOAD=false

rem set home_dir=%LOCAL_PATH%..\..\
rem 相对路径转换为完整的绝对路径
rem for %%i in ("%home_dir%") do set home_dir=%%~fi
set python_dir=%ROOT_PATH%Build\dependencies\prerequisites\miniconda3\envs\hutb_3.8\
set python_path=%python_dir%python.exe
set pip_path=%python_dir%Scripts\pip.exe
echo python_path: %python_path%

:arg-parse
if not "%1"=="" (
    if "%1"=="--all" (
        set SMOKE_TESTS=true
        set LIBCARLA_RELEASE=true
        set LIBCARLA_DEBUG=true
        set PYTHON_API=true
        set UPLOAD_DOWNLOAD=true
    )

    if "%1"=="--debug" (
        set IS_DEBUG=true
    )

    if "%1"=="--upload" (
        set UPLOAD_DOWNLOAD=true
        shift
    )

    if "%1"=="--xml" (
        set XML_OUTPUT=true
        shift
    )

    if "%1"=="--smoke" (
        set SMOKE_TESTS=true
        shift
    )

    if "%1"=="--air" (
        set AIR_TESTS=true
        shift
    )

    if "%1"=="--vr" (
        set VR_TESTS=true
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

rem Get the directory where CarlaUE4 is located (refer to Package.bat).
for /f %%i in ('git rev-parse --short HEAD') do set CARLA_VERSION=%%i
if not defined CARLA_VERSION goto bad_exit

rem If the "dirty" suffix is ​​present, it indicates that the package was compiled after switching between multiple versions, and this package is also used during testing.
rem (Solve the problem of not being able to find the executable file in the dirty directory during testing)
if exist %INSTALLATION_DIR%UE4Carla/%CARLA_VERSION%-dirty/ (
    set CARLA_VERSION=%CARLA_VERSION%-dirty
)

if %UPLOAD_DOWNLOAD%==true (
    cd /d %ROOT_PATH%Util
    rem  --distpath %INSTALLATION_DIR%UE4Carla
    rem %pip_path% install Pyinstaller
    %python_dir%Scripts\pyinstaller.exe hutb_downloader.spec

    rem %pip_path% install gitpython
    rem Upload the compiled package to the remote server and download the compiled package.
    rem python %ROOT_PATH%Util\download_from_git.py -u release
    cd %ROOT_PATH%Util\dist\
    hutb_downloader.exe  -u release
    rem Test download distribution package
    hutb_downloader.exe
    cd /d %ROOT_PATH%
) else (
    echo Skipping upload and download of package for version %CARLA_VERSION%.
)



rem The directory of CarlaUE4.exe
set BUILD_FOLDER=%INSTALLATION_DIR%UE4Carla/%CARLA_VERSION%/
rem debug only (rename with no dirty)
if %IS_DEBUG%==true (
    set BUILD_FOLDER=%INSTALLATION_DIR%UE4Carla\debug\
)

set exe_dir=%BUILD_FOLDER:\=/%WindowsNoEditor/%
set exe_path=%BUILD_FOLDER:/=\%WindowsNoEditor\CarlaUE4.exe

rem If exist CarlaUE4.exe process, kill it
for /f "tokens=5" %%a in ('netstat -ano ^| findstr :3654') do taskkill /F /PID %%a
:: use command "start" to launch a new service process. Otherwise stuck
if exist %exe_path% (
    :: prevent to Choose Vehicle when launching the service, which will cause the service to be stuck and fail to run smoke tests.
    cd /d %exe_dir%
    if %IS_DEBUG%==false (
        echo Unreal service is launching with command: start %exe_path% -RenderOffscreen --carla-rpc-port=3654 --carla-streaming-port=0 -nosound
        start %exe_path% -RenderOffscreen --carla-rpc-port=3654 --carla-streaming-port=0 -nosound
    ) else (
        echo Unreal service is launching with command: start %exe_path% --carla-rpc-port=3654 --carla-streaming-port=0 -nosound
        start %exe_path% --carla-rpc-port=3654 --carla-streaming-port=0 -nosound
    )
    
) else (
    echo Error: %exe_path% not exitst.
    goto bad_exit
)


rem ============================================================================
rem -- Install Python packages -------------------------------------------------
rem ============================================================================
if %IS_DEBUG%==false (
    for /l %%i in (14,-1,7) do (
        call conda activate hutb_3.%%i
        pip install -r %ROOT_PATH:/=\%PythonAPI\test\requirements.txt -i http://mirrors.aliyun.com/pypi/simple --trusted-host mirrors.aliyun.com
        if %%i==7 (
            rem Python 3.7 whl file has "cp37m" in its name, while Python 3.8-3.11 whl files have "cp3%%i" in their names.
            echo pip install --force-reinstall  %BUILD_FOLDER:\=/%WindowsNoEditor/PythonAPI/carla/dist/hutb-%API_VERSION%-cp3%%i-cp3%%im-win_amd64.whl
            pip uninstall --yes hutb
            pip install  %BUILD_FOLDER:\=/%WindowsNoEditor/PythonAPI/carla/dist/hutb-%API_VERSION%-cp3%%i-cp3%%im-win_amd64.whl
        ) else (
            echo pip install --force-reinstall  %BUILD_FOLDER:\=/%WindowsNoEditor/PythonAPI/carla/dist/hutb-%API_VERSION%-cp3%%i-cp3%%i-win_amd64.whl
            pip uninstall --yes hutb
            pip install  %BUILD_FOLDER:\=/%WindowsNoEditor/PythonAPI/carla/dist/hutb-%API_VERSION%-cp3%%i-cp3%%i-win_amd64.whl
        )
    )
)


:: goto success



rem ============================================================================
rem -- Run Carla-Air example tests ---------------------------------------------
rem ============================================================================

:: AIR_TESTS
if %AIR_TESTS%==true (
    echo Running Carla-Air example tests...
    for /l %%i in (14,-1,7) do (
        echo Running Carla-Air Python API for Python %%i example tests.
        call conda activate hutb_3.%%i
        cd %ROOT_PATH:/=\%PythonAPI\examples\air\
        rem AirSim: python 3.7 - 3.11 is normal, but 3.12, 3.13, 3.14 is abnormal: No module named 'backports'
        rem requirements: backports.weakref, backports.ssl_match_hostname
        python 01_hello_world.py --port 3654
        python 02_weather_control.py --port 3654
        python 03_spawn_traffic.py --port 3654
        python 04_sensor_capture.py --port 3654
        echo Finished Carla-Air Python API for Python %%i example tests.
        if %errorlevel% equ 0 (
            echo AIR test passed with errorlevel %errorlevel%.
        ) else (
            echo AIR test failed with errorlevel %errorlevel%.
            goto bad_exit
        )
    )
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
    for /l %%i in (14,-1,7) do (
        echo Running Python API for Python Python.%%i unit tests.
        where conda
        :: call D:\software\anaconda3\Scripts\activate.bat hutb_3.%%i && python --version
        call conda activate hutb_3.%%i
        echo Current Python path: 
        where python
        echo Current Pip path:
        where pip
        cd %ROOT_PATH:/=\%PythonAPI\test\unit\
        python -m nose2 test_transform
        python -m nose2 test_vehicle
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
    for /l %%i in (14,-1,7) do (
        echo Running smoke tests for Python 3.%%i
        call conda activate hutb_3.%%i
        echo Current Python path: 
        where python
        echo python -m nose2 -v %smoke_list%
        python -m nose2 -v %smoke_list%
    )
)



call :get_current_time_in_seconds T_END_DO_TEST
set /A ELAPSED_TIME=!T_END_DO_TEST! - !T_START_DO_TEST!
if %MEASURE_TIME%==true if %SMOKE_TESTS%==true echo %FILE_N% [TIME]: Running smoke test took !ELAPSED_TIME! seconds.


rem ============================================================================
rem -- Run VR tests ---------------------------------------------------------
rem ============================================================================

call :get_current_time_in_seconds T_START_DO_TEST


if %VR_TESTS%==true (
    echo Testing VR... 
    echo Current directory: %cd%
    for /l %%i in (14,-1,7) do (
        echo Running VR tests for Python 3.%%i
        call conda activate hutb_3.%%i
        echo Current Python path: 
        where python
        timeout /t 10 /nobreak > NUL
        echo Switch to VR mode...
        python ../util/config.py -p 3654 --map Town10HD?GAME=VR
        python ./function/test_VR_diagram_mode.py
        rem 判断是否正常退出
        if %errorlevel% equ 0 (
            echo VR test passed.
        ) else (
            echo VR test failed with errorlevel %errorlevel%.
            goto bad_exit
        )
    )
)


call :get_current_time_in_seconds T_END_DO_TEST
set /A ELAPSED_TIME=!T_END_DO_TEST! - !T_START_DO_TEST!
if %MEASURE_TIME%==true if %VR_TESTS%==true echo %FILE_N% [TIME]: Running VR test took !ELAPSED_TIME! seconds.


rem ============================================================================
rem -- kill service process after test -----------------------------------------
rem ============================================================================
:: pause 5 seconds to wait for the service to be ready, otherwise the service process will be not killed with no test task.
timeout /t 10 /nobreak > NUL
echo Killing Unreal service process after test...
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
