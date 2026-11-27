@echo off
setlocal
set "PS_EXE=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if exist "%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe" set "PS_EXE=%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe"
if not defined UE4_ROOT (
    call :find_ue4_root
) else if not exist "%UE4_ROOT%\Engine\Binaries\Win64\UE4Editor.exe" (
    call :find_ue4_root
)
"%PS_EXE%" -NoProfile -STA -ExecutionPolicy Bypass -File "%~dp0StartDrivingStudy.ps1" %*
set "RC=%errorlevel%"
if not "%RC%"=="0" if "%~1"=="" pause
exit /b %RC%

:find_ue4_root
if exist "%~dp0Build\engine\Engine\Binaries\Win64\UE4Editor.exe" (
    set "UE4_ROOT=%~dp0Build\engine"
    goto :eof
)
for /d %%D in ("%~dp0..\UE4*") do (
    if exist "%%~fD\Engine\Binaries\Win64\UE4Editor.exe" (
        set "UE4_ROOT=%%~fD"
        goto :eof
    )
)
goto :eof
