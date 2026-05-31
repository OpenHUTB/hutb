@echo off
setlocal enabledelayedexpansion
chcp 65001

rem 重新构建所有的 Python 虚拟环境

rem BAT script that creates the client python api of LibCarla (carla.org).
rem Run it through a cmd with the x64 Visual C++ Toolset enabled.

set LOCAL_PATH=%~dp0
set FILE_N=-[%~n0]:

rem Print batch params (debug purpose)
echo %FILE_N% [Batch params]: %*


set script_path=%~dp0
echo Script path: %script_path%

rem 回退两级目录，获取仓库根目录路径
for %%i in ("%script_path%\..\..") do set "hutb_root=%%~fi"

set prerequisites_dir=!hutb_root!\Build\dependencies\prerequisites\
echo Prerequisites directory: %prerequisites_dir%

set conda_root=%prerequisites_dir:/=\%miniconda3\

if exist "%conda_root%" (
    echo Removing existing miniconda for rebuild it.
    rmdir /s /q %conda_root%
)

if not exist "%prerequisites_dir%miniconda3" (
    echo Miniconda3 not found in prerequisites directory. Please download and extract it in %prerequisites_dir%miniconda3.
    echo Unzipping miniconda: "%prerequisites_dir%7zip\7z.exe" x "%prerequisites_dir%miniconda3_raw.zip" -o"%prerequisites_dir%" -y
    call "%prerequisites_dir%7zip\7z.exe" x "%prerequisites_dir%miniconda3_raw.zip" -o"%prerequisites_dir%" -y >nul
) else (
    echo Miniconda3 found in prerequisites directory.
)

set conda_dir=%prerequisites_dir%miniconda3\condabin\

REM 获取当前的Path环境变量
set "currentPath=%path%"  
    
set "currentPath=!conda_dir!;!currentPath!"

REM 临时添加路径到Path环境变量（在当前会话中有效）  
set "path=%currentPath%"

  
REM 显示更新后的Path环境变量    
echo Path is already updated to:
echo !path!


rem conda env list
for /l %%i in (14,-1,7) do (
    :: offline resource: https://repo.anaconda.com/pkgs/main/win-64/
    echo "If conda virtual environment hutb_3.%%i already exists, delete it"
    call conda remove -n hutb_3.%%i --all --yes
    rem Remove residual virtual environment files to avoid "Permission denied" errors when creating virtual environments.
    rem get hutb_3.%%i virtual environment directory, if it exists, and remove it
    for /f "tokens=2" %%a in ('conda env list ^| findstr hutb_3.%%i') do (
        echo ENV_DIR: %%a
        rem remove the environment directory if it still exists after conda remove
        if exist "%%a" (
            echo "Removing existing conda environment: %%a"
            rmdir /s /q "%%a"
        )
    )
    
    rem fix: CondaToSNonInteractiveError: Terms of Service have not been accepted for the following channels. Please accept or remove them before proceeding:
    call conda tos accept --override-channels --channel https://repo.anaconda.com/pkgs/main
    call conda tos accept --override-channels --channel https://repo.anaconda.com/pkgs/r
    conda tos accept --override-channels --channel https://repo.anaconda.com/pkgs/msys2
    echo "Creating new conda environment hutb_3.%%i ..."
    call conda create -n hutb_3.%%i python=3.%%i --yes

    rem Install requirements for python virtual environments
    %WINDIR%\System32\WindowsPowerShell\v1.0\powershell.exe ^
        -ExecutionPolicy ByPass -NoExit -Command^
        "& %conda_root%shell\condabin\conda-hook.ps1 ; conda activate %conda_root% ";^
        conda activate hutb_3.%%i;^
        python --version;^
        pip install -r %hutb_root%\PythonAPI\requirements_hutb_3.7-14.txt -i http://mirrors.aliyun.com/pypi/simple --trusted-host mirrors.aliyun.com;^
        pip list;^
        exit 0;
)

echo Finished creating conda environments for Python 3.7 to 3.14.
