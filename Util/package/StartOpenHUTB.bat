@echo off
setlocal enabledelayedexpansion

echo.
echo Checking Microsoft Visual C++ 2022 Installation Status...


:: check VC_redist 64 is installed or not
rem Took too long time
rem for /f "delims=" %%a in ('wmic product where "name like 'Microsoft Visual C++ 2022 x64 Additional Runtime%%'" 2^>^&1') do (
rem     echo %%a | findstr /i /c:"No Instance(s)" >nul && set found=1
rem )

set "found=0"

:: 1. 检查 64 位运行时（优先）
reg query "HKLM\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64" /v Installed 2>nul | find "0x1" >nul
if not errorlevel 1 (
    set "found=1"
    echo found x64 VS Runtimes.
    goto :check_done
)

:: 2. 如果 64 位没找到，检查 32 位运行时
reg query "HKLM\SOFTWARE\WOW6432Node\Microsoft\VisualStudio\14.0\VC\Runtimes\x86" /v Installed 2>nul | find "0x1" >nul
if not errorlevel 1 (
    set "found=1"
    echo found x86 VS Runtimes.
    goto :check_done
)

:check_done


if !found!==1 (
    echo VC_redist 64 installed
) else (
    echo No VC_redist 64 installed.
    echo Installing...
    start /wait %cd%\CarlaUE4\vc_redist.x64.exe /install /quiet /norestart
    echo VC_redist 64 install success.
)


echo Microsoft Visual C++ 2022 check finished.

start "" "CarlaUE4.exe"
echo OpenHUTB launched.

endlocal