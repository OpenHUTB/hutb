chcp 65001

set http_proxy=http://127.0.0.1:10090
set https_proxy=http://127.0.0.1:10090

:: python.exe
set PATH=C:\software\anaconda3;%PATH%

:: where.exe
set make_path="C:\Windows\System32"
set PATH=%make_path%;%PATH%

:: make.exe
set make_path="C:\software\GnuWin32\bin"
set PATH=%make_path%;%PATH%


:: development machine
getmac /v | find /i "5CB28FFF-BD3B-4EF9-BA05-1E8E6D826689"
if %errorlevel%==0 (
    echo This is development machine.

    :: conda
    set conda_path="D:\software\anaconda3\Scripts"
    set "PATH=%conda_path%;%PATH%"

    :: vs 2022
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
) else (
    echo This is not development machine.

    set conda_path="C:\software\anaconda3\Scripts"
    set "PATH=%conda_path%;%PATH%"

    :: vs 2019
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
)


:: %comspec% /k "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"



:: python 
:: C:\software\anaconda3\envs\carla_dev
call %conda_path%\activate.bat carla_dev && python --version
:: conda activate carla_dev
echo Python version after activate carla_dev:
call python --version
:: call exit /b


cd %~dp0
