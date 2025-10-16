@echo off
setlocal enabledelayedexpansion
chcp 65001


:: Run this script in Windows PowerShell with administrator privileges

:: Stop action runner service
net stop actions.runner.OpenHUTB.hutb


:: start actions-runner
call C:\actions-runner\run.cmd


