@echo off
setlocal enabledelayedexpansion
chcp 65001

rem Run it through a cmd with the x64 Visual C++ Toolset enabled.

set LOCAL_PATH=%~dp0
set FILE_N=-[%~n0]:

rem Print batch params (debug purpose)
echo %FILE_N% [Batch params]: %*

rem ============================================================================
rem -- Parse arguments ---------------------------------------------------------
rem ============================================================================

set DOC_STRING=Build LibCarla.
set USAGE_STRING=Usage: %FILE_N% [-h^|--help] [--rebuild] [--build] [--clean] [--no-pull]

set BUILD_STREETMAP=false
set GIT_PULL=true
set CURRENT_STREETMAP_COMMIT=260273d6b7c3f28988cda31fd33441de7e272958
set STREETMAP_BRANCH=master
set STREETMAP_REPO=https://github.com/carla-simulator/StreetMap.git

:: build air plugin
set BUILD_AIR=true
set GIT_PULL=true
set AIR_BRANCH=main
set AIR_REPO=https://github.com/OpenHUTB/air.git
set ENGINE_BRANCH=hutb

:arg-parse
if not "%1"=="" (
    if "%1"=="--rebuild" (
        set REMOVE_INTERMEDIATE=true
        set BUILD_STREETMAP=true
    )
    if "%1"=="--build" (
        set BUILD_STREETMAP=true
    )
    if "%1"=="--no-pull" (
        set GIT_PULL=false
    )
    if "%1"=="--clean" (
        set REMOVE_INTERMEDIATE=true
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
rem -- Local Variables ---------------------------------------------------------
rem ============================================================================

rem Set the visual studio solution directory
rem
set CARLA_PLUGINS_PATH=%ROOT_PATH:/=\%Unreal\CarlaUE4\Plugins\
set CARLA_STREETMAP_PLUGINS_PATH=%ROOT_PATH:/=\%Unreal\CarlaUE4\Plugins\StreetMap\

set AIR_PLUGIN_PATH=%ROOT_PATH:/=\%Unreal\CarlaUE4\Plugins\AirSim\
set AIR_BUILD_PATH=%ROOT_PATH:/=\%Build\AirSim\

set ENGINE_PATH=%ROOT_PATH:/=\%Build\engine\

set CONTENT_PATH=%ROOT_PATH:/=\%Unreal\CarlaUE4\Content\


:: 下载并构建引擎
if not exist "%ENGINE_PATH%" (
    echo %FILE_N% Engine directory: "%ENGINE_PATH%"
    if exist "%CACHE_DIR:/=\%engine\" (
        xcopy /q /Y /S /I "%CACHE_DIR:/=\%engine\"  %ENGINE_PATH%
    ) else (
        git clone -b hutb https://github.com/OpenHUTB/engine.git %ENGINE_PATH%
    )
    cd "%ENGINE_PATH%"
    echo %cd%
    if %IS_DEBUG% == true (
        echo Building engine in Debug mode...
        echo CALL Build.bat --is_debug --git-code %git_code%
        CALL Build.bat --is_debug --git-code %git_code%
    ) else (
        echo Building engine in Release mode...
        echo CALL Build.bat --git-code %git_code%
        CALL Build.bat --git-code %git_code%
    )

    echo Build engine completed.
    :: 设置用户的 UE4_ROOT 环境变量
    setx UE4_ROOT %ENGINE_PATH%
    echo New UE4_ROOT set to %ENGINE_PATH%
) else (
    echo %FILE_N% Engine directory already exists: "%ENGINE_PATH%"
    rem cd /d "%ENGINE_PATH%"
    rem git fetch --all
    rem git reset --hard origin/%ENGINE_BRANCH%
    rem git pull
)

:: 下载资产
if not exist "%CONTENT_PATH%" (
    echo %FILE_N% Content directory: "%CONTENT_PATH%"
    if exist "%CACHE_DIR:/=\%Content\" (
        xcopy /q /Y /S /I "%CACHE_DIR:/=\%Content\"  %CONTENT_PATH%
    ) else (
        git clone https://OpenHUTB:T8w6TYB_r71gGTP3A02B@git.code.tencent.com/OpenHUTB/Content.git %CONTENT_PATH%  &&  cd %CONTENT_PATH%  && git lfs pull
    )
) else (
    echo %FILE_N% Content directory already exists: "%CONTENT_PATH%", executing git pull.
    cd /d "%CONTENT_PATH%"
    git fetch --all
    git reset --hard origin/master
    git pull
    git lfs pull
)


rem Build STREETMAP

if  %GIT_PULL% == true (
    if not exist "%CARLA_STREETMAP_PLUGINS_PATH%" (
        echo StreetMap cache directory: "%CACHE_DIR:/=\%StreetMap\"
        if exist "%CACHE_DIR:/=\%StreetMap\" (
            xcopy /q /Y /S /I "%CACHE_DIR:/=\%StreetMap\"  %CARLA_STREETMAP_PLUGINS_PATH%
        ) else (
            git clone -b %STREETMAP_BRANCH% %STREETMAP_REPO% %CARLA_STREETMAP_PLUGINS_PATH%
        )
    )
    cd /d "%AIR_BUILD_PATH%"
    :: git fetch
    :: git checkout %CURRENT_STREETMAP_COMMIT%
)


:: Build AIR
if %BUILD_AIR% == true (
    if exist "%AIR_BUILD_PATH%\Unreal\Plugins\AirSim" (
        echo AirSim plugin already exists in build path: "%AIR_BUILD_PATH%\Unreal\Plugins\AirSim"
        rem cd "%AIR_BUILD_PATH%"
        rem git fetch --all
        rem git reset --hard origin/%AIR_BRANCH%
        rem git pull
    ) else (
        echo Air cache directory: "%CACHE_DIR:/=\%AirSim\"
        if exist "%CACHE_DIR:/=\%AirSim\" (
            :: /H:  copy hidden .git directory
            xcopy /q /Y /S /I /H "%CACHE_DIR:/=\%AirSim\"  %AIR_BUILD_PATH%
        ) else (
            git clone -b %AIR_BRANCH% %AIR_REPO% %AIR_BUILD_PATH%
        )
    )
    cd "%AIR_BUILD_PATH%"
    echo %cd%
    :: Build AirSim
    :: CALL clean_rebuild.bat
    rem CALL build.cmd --Debug
    CALL build.cmd
    xcopy /q /Y /S /I "%AIR_BUILD_PATH:/=\%Unreal\Plugins\AirSim\"  %AIR_PLUGIN_PATH%
)


goto success

rem ============================================================================
rem -- Messages and Errors -----------------------------------------------------
rem ============================================================================

:success
    if %BUILD_STREETMAP% == true echo %FILE_N% STREETMAP has been successfully installed in "%CARLA_PLUGINS_PATH%"!
    goto good_exit

:good_exit
    endlocal
    exit /b 0

:bad_exit
    endlocal
    exit /b %errorlevel%
