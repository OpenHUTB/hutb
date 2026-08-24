@echo off
setlocal enabledelayedexpansion

echo.
echo Checking Microsoft Visual C++ 2022 Installation Status...


:: check VC_redist 64 is installed or not
:: wmic product where "name like 'Microsoft Visual C++ 2022 x64 Additional Runtime%%'"

:: wmic product where "name like 'Microsoft Visual C++ 2022 x64 Additional Runtime%%'" | findstr /i /c:"No Instance(s) Available." >nul

for /f "delims=" %%a in ('wmic product where "name like 'Microsoft Visual C++ 2022 x64 Additional Runtime%%'" 2^>^&1') do (
    echo %%a | findstr /i /c:"No Instance(s)" >nul && set found=1
)

if !found!==1 (
    echo No VC_redist 64 installed.
    echo Installing...
    start /wait %cd%\CarlaUE4\vc_redist.x64.exe /install /quiet /norestart
    echo VC_redist 64 install success.
) else (
    echo VC_redist 64 installed
)


echo Microsoft Visual C++ 2022 check finished.

CarlaUE4.exe

endlocal