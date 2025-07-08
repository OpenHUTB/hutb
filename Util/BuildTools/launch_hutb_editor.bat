@echo off 
:: 需要预装的软件：Visual Studio 2019：https://aka.ms/vs/14/release/vs_community.exe
:: 当前目录下需要的软件包括：
:: carla1, unreal
:: CMake, GnuWin32, 
:: dotnet, Python37

:: 解决命令行显示的中文乱码（添加支持utf-8功能）
chcp 65001 > nul

setlocal enabledelayedexpansion


:: 安装 vs2022
if exist "%ProgramFiles%\Microsoft Visual Studio\2022\" (
    echo Visual Studio 2022 exist.
) else (
    echo Visual Studio 2022 not exist
    set visual_studio_components=^
        Microsoft.VisualStudio.Workload.NativeDesktop ^
        Microsoft.VisualStudio.Workload.NativeGame ^
        Microsoft.VisualStudio.Workload.ManagedDesktop ^
        Microsoft.VisualStudio.Component.Windows10SDK.22621 ^
        Microsoft.VisualStudio.Component.VC.CMake.Project ^
        Microsoft.Net.Component.4.8.SDK ^
        Microsoft.Net.ComponentGroup.4.8.1.DeveloperTools ^
        Microsoft.VisualStudio.Component.VC.Llvm.Clang ^
        Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset ^
        Microsoft.VisualStudio.ComponentGroup.NativeDesktop.Llvm.Clang ^
        Microsoft.VisualStudio.Component.VC.14.36.17.6.x86.x64 ^
        Microsoft.Component.PythonTools

    :: --quiet      执行安装时不显示任何用户界面
    :: --passive    显示用户界面，但不请求用户进行任何交互
    :: --wait       返回退出代码之前，该进程会等待安装完成。自动安装过程中，需要等待安装完成以便处理从该安装返回的代码时，此进程非常有用。
    :: --norestart  如果存在，则带有 --passive 或 --quiet 的命令不会自动重启计算机（若需要）。如果未指定 --passive 和 --quiet，则忽略此命令行。
    vs_community.exe --add %visual_studio_components% --passive --wait --norestart || exit /b
)


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


exist

:: 编译虚幻引擎源代码
:: TODO：如果存在 UnrealEditor.exe，则跳过虚幻引擎的从头编译
if not exist %UE4_ROOT:/=\%\Engine\Binaries\Win64\UE4Editor.exe (
    cd unreal
    :: /t:Build                                 指定构建目标
    :: /p:Configuration="Development Editor"    构建配置（调试用 DebugGame Editor，发布用 Shipping）
    :: /p:Platform="Win64"                      目标平台
    :: /m                                       启用多核编译（加快速度）
    "%PROGRAMFILES%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" UE4.sln /t:Build /p:Configuration="Development Editor" /p:Platform="Win64" /m
    cd ..
)



    
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

call "%PROGRAMFILES%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" || exit /b
cd !scriptDir!\carla1
  
REM 启动UE4Editor.exe并传递uprojectPath的完整路径  
:: start "" "UE4Editor.exe" "!FulluprojectPath!"  




:: 编译hutb源代码
make launch ARGS="--chrono"
 

REM 结束本地环境变量设置    
endlocal    
  
REM 注意：此脚本设置的环境变量只在当前命令行会话中有效   
:: pause