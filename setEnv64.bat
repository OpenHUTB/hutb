chcp 65001

set http_proxy=http://127.0.0.1:7890
set https_proxy=http://127.0.0.1:7890

:: python.exe
set PATH=C:\software\anaconda3;%PATH%

:: where.exe
set make_path="C:\Windows\System32"
set PATH=%make_path%;%PATH%

:: make.exe
set make_path="C:\software\GnuWin32\bin"
set PATH=%make_path%;%PATH%


:: vs 2019
%comspec% /k "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"

call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"


:: python 
:: C:\software\anaconda3\envs\carla_dev
call C:\software\anaconda3\Scripts\activate.bat carla_dev && python --version
:: conda activate carla_dev
echo Python version after activate carla_dev:
call python --version
:: call exit /b


cd C:\carla
