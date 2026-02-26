@echo off

set ninja_version=1.12.1

set python_path=python
set python_version_default=3.8.10

rem https://learn.microsoft.com/en-us/visualstudio/install/workload-component-id-vs-community?view=vs-2022&preserve-view=true
set visual_studio_components=^
    Microsoft.VisualStudio.Workload.NativeDesktop ^
    Microsoft.VisualStudio.Workload.NativeGame ^
    Microsoft.VisualStudio.Workload.ManagedDesktop ^
    Microsoft.VisualStudio.Component.Windows10SDK.22621 ^
    Microsoft.VisualStudio.Component.Windows11SDK.26100 ^
    Microsoft.VisualStudio.Component.VC.CMake.Project ^
    Microsoft.Net.Component.4.8.SDK ^
    Microsoft.Net.ComponentGroup.4.8.1.DeveloperTools ^
    Microsoft.VisualStudio.Component.VC.Llvm.Clang ^
    Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset ^
    Microsoft.VisualStudio.ComponentGroup.NativeDesktop.Llvm.Clang ^
    Microsoft.VisualStudio.Component.VC.14.36.17.6.x86.x64 ^
    Microsoft.Component.PythonTools

rem -- PARSE COMMAND LINE ARGUMENTS --

:parse
    if "%1"=="" (
        goto main
    )
    echo %1 | findstr /B /C:"--python-path=" >nul
    if not errorlevel 1 (
        set python_path="%1"
        set python_path="!python_path:--python-path=!"
    ) else if "%1"=="--python-path" (
        set python_path=%2
        shift
    ) else if "%1"=="-pypath" (
        set python_path=%2
        shift
    ) else (
        echo Unknown argument "%1"
        exit /b
    )
    shift
    goto parse

rem -- MAIN --

:main

rem -- INSTALL VISUAL STUDIO IF NOT FOUND --
setlocal EnableDelayedExpansion

set "vs_19_found=false"
if exist "%programfiles(x86)%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set "vs_19_found=true"
)
if exist "%programfiles(x86)%\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat" (
    set "vs_19_found=true"
)
if exist "%programfiles(x86)%\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
    set "vs_19_found=true"
)
if "!vs_19_found!"=="true" (
    echo Found Visual Studio 2019.
    goto end_vs_install
) else (
    echo Could not find Visual Studio 2019.
)

set "vs_22_found=false"
if exist "%ProgramW6432%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set "vs_22_found=true"
)
if exist "%ProgramW6432%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
    set "vs_22_found=true"
)
if exist "%ProgramW6432%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
    set "vs_22_found=true"
)
if "!vs_22_found!"=="true" (
    echo Found Visual Studio 2022.
    goto end_vs_install
) else (
    echo Could not find Visual Studio 2022. Downloading...
)

if not exist %cd%\Temp (
    mkdir %cd%\Temp
)
pushd Temp
curl -L -O https://aka.ms/vs/17/release/vs_community.exe || exit /b
popd Temp
%cd%\Temp\vs_community.exe --add %visual_studio_components% --installWhileDownloading --passive --wait || exit /b
del %cd%\Temp\vs_community.exe
rmdir %cd%\Temp

:end_vs_install


rem -- INSTALL PYTHON PACKAGES --
