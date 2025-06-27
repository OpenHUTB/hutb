:: @echo off 
:: 需要预装的软件：Visual Studio 2019：https://aka.ms/vs/14/release/vs_community.exe
:: 当前目录下需要的软件包括：
:: carla1, unreal
:: CMake, GnuWin32, 
:: dotnet, Python37

:: 解决命令行显示的中文乱码（添加支持utf-8功能）
chcp 65001 > nul

setlocal enabledelayedexpansion
    
REM 获取当前脚本的完整路径    
set scriptPath=%~dp0
echo %scriptPath%

REM 去掉路径末尾的反斜杠，如果有的话
set scriptDir=!scriptPath:~0,-1!

    
REM 拼接子目录路径到脚本路径，设置环境变量UE4_ROOT    
set UE4_ROOT=!scriptDir!\unreal
echo %UE4_ROOT%
    
REM 显示设置的环境变量    
:: echo 环境变量UE4_ROOT已设置为：!UE4_ROOT!    
    
REM 定义相对路径列表    
set "RelativePaths=CMake\bin;dotnet;GnuWin32\bin;Python37;Python37\Scripts"    
  
  
:: 获取当前的Path环境变量值  
for /f "tokens=2 delims==" %%a in ('set Path') do set "CurrentPath=%%a" 
REM 获取当前的Path环境变量  
set "currentPath=%path%"  
    
REM 将每个相对路径转换为绝对路径，并添加到newPath中    
for %%p in (%RelativePaths%) do (    
    set "absPath=!scriptDir!\%%p"    
    set "currentPath=!currentPath!;!absPath!"    
)  
REM 临时添加路径到Path环境变量（在当前会话中有效）  
set "path=%currentPath%"

  
REM 显示更新后的Path环境变量    
echo path环境变量已更新为：    
echo !path!    

REM 定义相对于当前脚本的路径  
set "UnrealEnginePath=unreal\Engine\Binaries\Win64"  
set "uprojectPath=carla1\Unreal\CarlaUE4\CarlaUE4.uproject"  
  
REM 构建完整的UnrealEnginePath和uprojectPath路径  
set "FullUnrealEnginePath=%~dp0!UnrealEnginePath!"  
set "FulluprojectPath=%~dp0!uprojectPath!"  
  
REM 切换到UnrealEnginePath目录（如果需要）  
if exist "!FullUnrealEnginePath!" (  
    cd /d "!FullUnrealEnginePath!"  
) else (  
    echo Unreal Engine path not found.  
    exit /b  
)  
  
REM 启动UE4Editor.exe并传递uprojectPath的完整路径  
start "" "UE4Editor.exe" "!FulluprojectPath!"  
 

REM 结束本地环境变量设置    
endlocal    
  
REM 注意：此脚本设置的环境变量只在当前命令行会话中有效   
:: pause