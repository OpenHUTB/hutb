@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Open_Underwater_Editor.ps1" %*
if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] Launcher failed with code %ERRORLEVEL%.
    pause
)
