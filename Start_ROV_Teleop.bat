@echo off
setlocal
cd /d "%~dp0"
python teleop_rov.py
if %ERRORLEVEL% neq 0 pause
