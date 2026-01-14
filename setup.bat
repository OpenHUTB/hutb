@echo off
:: resolve display issue in Chinese (add support for utf-8)
chcp 65001 > nul
setlocal EnableDelayedExpansion

set FILE_N=-[%~n0]:

set skip_prerequisites=false
set launch=false
set interactive=false
set python_path=python
set python_root=

rem -- PARSE COMMAND LINE ARGUMENTS --

:parse
    if "%1"=="" (
        goto main
    )
    if "%1"=="--interactive" (
        set interactive=true
    ) else if "%1"=="-i" (
        set interactive=true
    ) else if "%1"=="--skip-prerequisites" (
        set skip_prerequisites=true
    ) else if "%1"=="-p" (
        set skip_prerequisites=true
    ) else if "%1"=="--launch" (
        set launch=true
    ) else if "%1"=="-l" (
        set launch=true
    ) else (
        echo %1 | findstr /B /C:"--python-root=" >nul
        if not errorlevel 1 (
            set python_root="%1"
            set python_root="!python_root:--python-root=!"
        ) else if "%1"=="--python-root" (
            set python_root=%2
            shift
        ) else if "%1"=="-pyroot" (
            set python_root=%2
            shift
        ) else (
            echo Unknown argument "%1"
            exit /b
        )
    )
    shift
    goto parse

rem -- MAIN --

:main

rem ============================================================================
rem -- Download prerequisites from https://git.code.tencent.com/OpenHUTB/dependencies
rem ============================================================================

:: Download  https://gitee.com/OpenHUTB/sw/releases/download/up/git.zip to Build\git.zip and unzip it to Build\git
if not exist "%cd%\Build" (
    mkdir "%cd%\Build"
) else (
    if not exist "%cd%\Build\git" (
        if not exist "%cd%\Build\git.zip" (
            echo Downloading git.zip ...
            pushd "%cd%\Build"
            curl -L -o git.zip https://gitee.com/OpenHUTB/sw/releases/download/up/git.zip || exit /b
            popd
        )
        echo Unzipping git...
        powershell -Command "Expand-Archive -Path '%cd%\Build\git.zip' -DestinationPath '%cd%\Build\' -Force" || exit /b
    )
)

:: clone prerequisites https://git.code.tencent.com/OpenHUTB/dependencies to Build\prerequisites
set GIT_LFS_SKIP_SMUDGE=1
if not exist "%cd%\Build\dependencies" (
    echo Cloning dependencies repo ...
    pushd "%cd%\Build"
    "%cd%\Build\git\cmd\git.exe" clone https://OpenHUTB:T8w6TYB_r71gGTP3A02B@git.code.tencent.com/OpenHUTB/dependencies.git  &&  cd dependencies  && git lfs pull
    popd
) else (
    echo dependencies repo already exists, updating dependencies repository.
    pushd "%cd%\Build\dependencies"
    :: discard any local changes
    "%cd%\Build\git\cmd\git.exe" reset --hard
    "%cd%\Build\git\cmd\git.exe" pull
    "%cd%\Build\git\cmd\git.exe" lfs pull
    popd
)

if exist "%cd%\Build\dependencies\" (
    :: TODO: extract all *.zip files to current directory in dependencies repository
    echo dependencies repo is ready.
    pushd "%cd%\Build\dependencies\"

    if not exist "prerequisites\7zip" (
        echo Unzipping 7zip ...
        powershell -Command "Expand-Archive -Path 'prerequisites\7zip.zip' -DestinationPath 'prerequisites\' -Force" || exit /b
    ) else (
        echo 7zip folder already exists.
    )

    rem fix no XINPUT1_3.dll error when lanunch UE4Editor
    rem install directx_Jun2010_redist.exe when DirectX folder not exist
    REG QUERY HKEY_CURRENT_USER\Software\Microsoft |find "DirectX" >nul
    if not exist "%SystemRoot%\system32\xinput1_3.dll" (
        echo DirectX not found.
        echo Unzipping DirectX ...
        "prerequisites\7zip\7z.exe" x "prerequisites\directx.zip" -o"prerequisites\" -y >nul
        echo Install DirectX ...
        call prerequisites\directx\DXSETUP.exe /silent || exit /b
    ) else (
        echo DirectX found!
    )

    if not exist "prerequisites\miniconda3\" (
        echo Unzipping miniconda...
        "prerequisites\7zip\7z.exe" x "prerequisites\miniconda3.zip" -o"prerequisites\" -y >nul
        rem set script_path=%~dp0
        rem set PATH=!script_path!Build\dependencies\prerequisites\miniconda3;!PATH!
        rem set PATH=!script_path!Build\dependencies\prerequisites\miniconda3\Scripts;!PATH!
        rem set PATH=!script_path!Build\dependencies\prerequisites\miniconda3\Library\bin;!PATH!
        rem echo After add miniconda3 path:
        rem echo !PATH!
        rem where conda
        rem conda env list
        rem call "%cd%\Build\dependencies\prerequisites\miniconda3\Scripts\conda.exe" tos accept
        rem for /l %%i in (14,-1,7) do (
        rem     echo "If conda viural environment hutb_3.%%i already exists, delete it"
        rem     call "%cd%\Build\dependencies\prerequisites\miniconda3\Scripts\conda.exe" remove -n hutb_3.%%i --all --yes
        rem     echo "Creating new conda environment hutb_3.%%i ..."
        rem     call "%cd%\Build\dependencies\prerequisites\miniconda3\Scripts\conda.exe" create -n hutb_3.%%i python=3.%%i --yes
        rem     call "%cd%\Build\dependencies\prerequisites\miniconda3\envs\hutb_3.%%i\Scripts\pip.exe" install -r %cd%\PythonAPI\carla\requirements.txt
        rem )
    ) else (
        echo miniconda3 folder already exists.
    )
    rem %WINDIR%\System32\cmd.exe "/K" %cd%\Build\dependencies\prerequisites\miniconda3\Scripts\activate.bat hutb_3.8

    if not exist "prerequisites\CMake\" (
        echo Unzipping CMake ...
        "prerequisites\7zip\7z.exe" x "prerequisites\CMake.zip" -o"prerequisites\" -y >nul
    ) else (
        echo CMake folder already exists.
    )
    
    if not exist "prerequisites\dotnet\" (
        echo Unzipping dotnet ...
        "prerequisites\7zip\7z.exe" x "prerequisites\dotnet.zip" -o"prerequisites\" -y >nul
    ) else (
        echo dotnet folder already exists.
    )
    if not exist "prerequisites\GnuWin32\" (
        echo Unzipping GnuWin32 ...
        "prerequisites\7zip\7z.exe" x "prerequisites\GnuWin32.zip" -o"prerequisites\" -y >nul
    ) else (
        echo GnuWin32 folder already exists.
    )
    popd
) else (
    echo Build\dependencies\ not found, please check the dependencies repository clone.
    exit /b
)


rem ============================================================================
rem -- Installation prerequisites ----------------------------------------------
rem ============================================================================

if not "%python_root%"=="" (
    set python_path=%python_root%\python
)

rem -- PREREQUISITES INSTALL STEP --

if %skip_prerequisites%==false (
    echo Installing prerequisites...
    call Util/InstallersWin/install_prerequisites.bat --python-path=%python_path% || exit /b
) else (
    echo Skipping prerequisites install step.
)


rem Activate VS terminal development environment:
set "vs_env_bat="
if exist "%PROGRAMFILES%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set "vs_env_bat=%PROGRAMFILES%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
)
if exist "%PROGRAMFILES%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
    set "vs_env_bat=%PROGRAMFILES%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
)
if exist "%PROGRAMFILES%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
    set "vs_env_bat=%PROGRAMFILES%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
)

if not "%vs_env_bat%"=="" (
    echo Activating "x64 Native Tools Command Prompt" terminal environment.
    call "%vs_env_bat%" || exit /b
) else (
    echo Could not find vcvars64.bat for VS 2022, aborting setup...
    exit 1
)


call %cd%\Build\dependencies\prerequisites\GnuWin32\bin\make launch ARGS="--chrono"



:get_current_time_in_seconds
    for /f %%a in ('powershell -command "[int]((Get-Date -UFormat '%%s') -split ',.')[0]"') do set %1=%%a
    goto :eof
