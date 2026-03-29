@echo off
setlocal enabledelayedexpansion
chcp 65001

rem BAT script that creates the client python api of LibCarla (carla.org).
rem Run it through a cmd with the x64 Visual C++ Toolset enabled.

set LOCAL_PATH=%~dp0
set FILE_N=-[%~n0]:

rem Print batch params (debug purpose)
echo %FILE_N% [Batch params]: %*

rem ============================================================================
rem -- Parse arguments ---------------------------------------------------------
rem ============================================================================

set DOC_STRING=Build and package CARLA Python API.
set "USAGE_STRING=Usage: %FILE_N% [-h^|--help] [--rebuild]  [--clean]"

set REMOVE_INTERMEDIATE=false
set BUILD_FOR_PYTHON2=false
set BUILD_FOR_PYTHON3=false

:arg-parse
if not "%1"=="" (
    if "%1"=="--rebuild" (
        set REMOVE_INTERMEDIATE=true
        rem We don't provide support for py2 right now
        set BUILD_FOR_PYTHON2=false
        set BUILD_FOR_PYTHON3=true
    )

    if "%1"=="--py2" (
        set BUILD_FOR_PYTHON2=true
    )

    if "%1"=="--py3" (
        set BUILD_FOR_PYTHON3=true
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

set PYTHON_LIB_PATH=%ROOT_PATH:/=\%PythonAPI\carla\

if %REMOVE_INTERMEDIATE% == false (
    if %BUILD_FOR_PYTHON3% == false (
        if %BUILD_FOR_PYTHON2% == false (
          echo Nothing selected to be done.
          goto :eof
        )
    )
)

if %REMOVE_INTERMEDIATE% == true (
    rem Remove directories
    for %%G in (
        "%PYTHON_LIB_PATH%build",
        "%PYTHON_LIB_PATH%dist",
        "%PYTHON_LIB_PATH%source\carla.egg-info"
    ) do (
        if exist %%G (
            echo %FILE_N% Cleaning %%G
            rmdir /s/q %%G
        )
    )
    if %BUILD_FOR_PYTHON3% == false (
        if %BUILD_FOR_PYTHON2% == false (
            goto good_exit
        )
    )
)

cd "%PYTHON_LIB_PATH%"
rem if exist "%PYTHON_LIB_PATH%dist" goto already_installed

rem ============================================================================
rem -- Check for py ------------------------------------------------------------
rem ============================================================================

where python 1>nul
if %errorlevel% neq 0 goto error_py

rem for /l %%i in (14,-1,7) do (
rem     :: offline resource: https://repo.anaconda.com/pkgs/main/win-64/
rem     echo "If conda viural environment hutb_3.%%i already exists, delete it"
rem     call conda remove -n hutb_3.%%i --all --yes
rem     echo "Creating new conda environment hutb_3.%%i ..."
rem     call conda create -n hutb_3.%%i python=3.%%i --yes
rem )


rem Build for Python 2
rem
if %BUILD_FOR_PYTHON2%==true (
    goto py2_not_supported
)

set conda_root=%ROOT_PATH:/=\%Build\dependencies\prerequisites\miniconda3\
set boost_src_dir=%BOOST_INSTALL_FOLDER:/=\%boost-%BOOST_VERSION:.=_%
echo boost source directory: %boost_src_dir%

rem Build for Python 3
rem
if %BUILD_FOR_PYTHON3%==true (
    where conda >nul 2>&1
    if %errorlevel%==0 (
        echo Conda is already installed.
    ) else (
        echo TODO: Installing anaconda with silent mode
    )

    for /l %%i in (14,-1,7) do (
        rem remove boost build before
        echo BOOST_VERSION: %BOOST_VERSION%
        echo BOOST_INSTALL_FOLDER: %BOOST_INSTALL_FOLDER%
        if exist "%BOOST_INSTALL_FOLDER%" (
            echo Delete all boost files: %BOOST_INSTALL_FOLDER:/=\%*
            del /f /s /q %BOOST_INSTALL_FOLDER:/=\%*  >nul
            rem remove empty directory
            rd /s /q %BOOST_INSTALL_FOLDER:/=\%  >nul
            echo Delete boost source code: %boost_src_dir:/=\%*
            del /f /s /q %boost_src_dir:/=\%*  >nul
            rd /s /q %boost_src_dir:/=\%  >nul
        )
        
        cd "%ROOT_PATH%"
        echo %FILE_N% Root path: %ROOT_PATH%
        echo Building LibCarla and osm with Python 3.%%i ...
        %WINDIR%\System32\WindowsPowerShell\v1.0\powershell.exe ^
            -ExecutionPolicy ByPass -NoExit -Command^
            "& %conda_root%shell\condabin\conda-hook.ps1 ; conda activate %conda_root% ";^
            conda activate hutb_3.%%i;^
            python --version;^
            pip list;^
            make LibCarla;^
            make osm2odr; ^
            exit 0;

        cd "%PYTHON_LIB_PATH%"
        echo Building Python API with Python 3.%%i ...
        %WINDIR%\System32\WindowsPowerShell\v1.0\powershell.exe ^
            -ExecutionPolicy ByPass -NoExit -Command^
            "& %conda_root%shell\condabin\conda-hook.ps1 ; conda activate %conda_root% ";^
            conda activate hutb_3.%%i;^
            python --version;^
            pip list;^
            python setup.py bdist_wheel; ^
            exit 0;

        echo errorlevel: %errorlevel%
        if not exist "%PYTHON_LIB_PATH%dist\" (
            goto error_build_wheel
        )
    )

    :: Even if no .whl file is generated, errorlevel will be equal to 0
    :: if %errorlevel% neq 0 goto error_build_wheel
)

goto success

rem ============================================================================
rem -- Messages and Errors -----------------------------------------------------
rem ============================================================================

:success
    echo.
    if %BUILD_FOR_PYTHON3%==true echo %FILE_N% Carla lib for python has been successfully installed in "%PYTHON_LIB_PATH%dist"!
    goto good_exit

:already_installed
    echo.
    echo %FILE_N% [ERROR] Already installed in "%PYTHON_LIB_PATH%dist"
    goto good_exit

:py2_not_supported
    echo.
    echo %FILE_N% [ERROR] Python 2 is not currently suported in Windows.
    goto bad_exit

:error_py
    echo.
    echo %FILE_N% [ERROR] An error ocurred while executing the py.
    echo %FILE_N% [ERROR] Possible causes:
    echo %FILE_N% [ERROR]  - Make sure "py" is installed.
    echo %FILE_N% [ERROR]  - py = python launcher. This utility is bundled with Python installation but not installed by default.
    echo %FILE_N% [ERROR]  - Make sure it is available on your Windows "py".
    goto bad_exit

:error_build_wheel
    echo.
    echo %FILE_N% [ERROR] An error occurred while building the wheel file.
    goto bad_exit

:good_exit
    endlocal
    exit /b 0

:bad_exit
    endlocal
    exit /b %errorlevel%

