@echo off
setlocal
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

for /l %%i in (13,-1,7) do (
    :: offline resource: https://repo.anaconda.com/pkgs/main/win-64/
    echo "If conda viural environment hutb_3.%%i already exists, delete it"
    call conda remove -n hutb_3.%%i --all --yes
    echo "Creating new conda environment hutb_3.%%i ..."
    call conda create -n hutb_3.%%i python=3.%%i --yes
)


rem Build for Python 2
rem
if %BUILD_FOR_PYTHON2%==true (
    goto py2_not_supported
)

rem Build for Python 3
rem
if %BUILD_FOR_PYTHON3%==true (
    where conda >nul 2>&1
    if %errorlevel%==0 (
        echo Conda is already installed.
    ) else (
        echo TODO: Installing anaconda with silent mode
    )

    for /l %%i in (13,-1,7) do (
        rem remove boost build before
        echo BOOST_VERSION: %BOOST_VERSION%
        echo BOOST_INSTALL_FOLDER: %BOOST_INSTALL_FOLDER%
        if exist "%BOOST_INSTALL_FOLDER%" (
            echo Delete all boost files: %BOOST_INSTALL_FOLDER:/=\%*
            del /f /s /q %BOOST_INSTALL_FOLDER:/=\%*  >nul
            rem remove empty directory
            rd /s /q %BOOST_INSTALL_FOLDER:/=\%  >nul
            echo Delete boost source code: %BOOST_SOURCE_FOLDER:/=\%*
            del /f /s /q %BOOST_SOURCE_FOLDER:/=\%*  >nul
            rd /s /q %BOOST_SOURCE_FOLDER:/=\%  >nul
        )
        
        cd "%ROOT_PATH%"
        REM conda create --name hutb_3.%%i python=3.%%i --yes
        call conda activate hutb_3.%%i
        echo Current Python path: 
        where python
        make LibCarla
        make osm2odr

        cd "%PYTHON_LIB_PATH%"
        echo Building Python API Python 3.%%i
        python setup.py bdist_egg bdist_wheel
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

