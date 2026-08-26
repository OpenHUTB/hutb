@echo off
:: resolve display issue in Chinese (add support for utf-8)
chcp 65001 > nul
setlocal EnableDelayedExpansion

set FILE_N=-[%~n0]:

set skip_prerequisites=false
set launch=false
rem launch editor directly, used for quick test after build/package
set direct_launch=false
rem to solve the issue of "Missing D:/hutb/Build/engine/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe after build" when generate CarlaUE4.sln according to CarlaUE4.uproject
set generate_project_files=false
set package=false
set interactive=false
set python_path=python
set python_root=


rem -- extract dependencies --


rem -- set path --

set scriptPath=%~dp0
echo %scriptPath%

set scriptDir=!scriptPath:~0,-1!

set UE4_ROOT=!scriptDir!\Build\engine
echo %UE4_ROOT%

set "RelativePaths=Build\dependencies\prerequisites\CMake\bin;Build\dependencies\prerequisites\dotnet;Build\dependencies\prerequisites\GnuWin32\bin;Build\dependencies\prerequisites\miniconda3\envs\hutb_3.8;Build\dependencies\prerequisites\miniconda3\envs\hutb_3.8\Scripts"
  

for /f "tokens=2 delims==" %%a in ('set Path') do set "CurrentPath=%%a" 
set "currentPath=%path%"

for %%p in (%RelativePaths%) do (    
    set "absPath=!scriptDir!\%%p"    
    set "currentPath=!currentPath!;!absPath!"    
)
set "path=%currentPath%"

echo !path!


rem -- PARSE COMMAND LINE ARGUMENTS --

:parse
    if "%1"=="" (
        goto main
    )
    if "%1"=="--interactive" (
        set interactive=true
    ) else if "%1"=="-i" (
        set interactive=true
    ) else if "%1"=="--skip-prerequisites" (
        set skip_prerequisites=true
    ) else if "%1"=="-s" (
        set skip_prerequisites=true
    ) else if "%1"=="--package" (
        set package=true
    ) else if "%1"=="-p" (
        set package=true
    ) else if "%1"=="--launch" (
        set launch=true
    ) else if "%1"=="-g" (
        set generate_project_files=true
    ) else if "%1"=="-l" (
        set launch=true
    ) else if "%1"=="-d" (
        set direct_launch=true
    ) else if "%1"=="--direct-launch" (
        set direct_launch=true
    ) else (
        echo %1 | findstr /B /C:"--python-root=" >nul
        if not errorlevel 1 (
            set python_root="%1"
            set python_root="!python_root:--python-root=!"
        ) else if "%1"=="--python-root" (
            set python_root=%2
            shift
        ) else if "%1"=="-pyroot" (
            set python_root=%2
            shift
        ) else (
            echo Unknown argument "%1"
            exit /b
        )
    )
    shift
    goto parse

rem -- MAIN --

:main

if %generate_project_files% == true (
    echo Generating project files...
    rem --engine is rquired to display the Plugin in the Unreal Editor, otherwise the plugin will be hidden in the editor
    call "%UE4_ROOT%\Engine\Build\BatchFiles\GenerateProjectFiles.bat" -project="%scriptDir%\Unreal\CarlaUE4\CarlaUE4.uproject" -game -engine -progress || exit /b
)

rem if direct_launch is true, skip all the setup and launch game directly, used for quick test after build/package
if %direct_launch% == true (
    echo Directly launching Unreal Editor, log to launch.log...
    set UE4_Editor_path=!scriptDir!\Build\engine\Engine\Binaries\Win64\UE4Editor.exe
    set uproject_path=!scriptDir!\Unreal\CarlaUE4\CarlaUE4.uproject
    if not exist "!UE4_Editor_path!" (
        echo UE4Editor.exe not found at !UE4_Editor_path!, please check if the build step is finished and the file exists.
        exit /b
    ) else (
        echo Found UE4Editor.exe at !UE4_Editor_path!, launching...
        start "" "!UE4_Editor_path!" "!uproject_path!" >launch.log 2>&1
    )
    exit /b
)


rem ============================================================================
rem -- Download prerequisites from https://git.code.tencent.com/OpenHUTB/dependencies
rem ============================================================================

:: Download  https://gitee.com/OpenHUTB/sw/releases/download/up/git.zip to Build\git.zip and unzip it to Build\git
if not exist "%cd%\Build" (
    mkdir "%cd%\Build"
) else (
    echo "%cd%\Build" folder already exists.
)

if not exist "%cd%\Build\git" (
    if not exist "%cd%\Build\git.zip" (
        echo Downloading git.zip ...
        pushd "%cd%\Build"
        curl -L -o git.zip https://gitee.com/OpenHUTB/sw/releases/download/up/git.zip || exit /b
        popd
    )
    echo Unzipping git...
    powershell -Command "Expand-Archive -Path '%cd%\Build\git.zip' -DestinationPath '%cd%\Build\' -Force" || exit /b
) else (
    echo "%cd%\Build\git" folder already exists.
)

:: clone prerequisites https://git.code.tencent.com/OpenHUTB/dependencies to Build\prerequisites
set GIT_LFS_SKIP_SMUDGE=1
if not exist "%cd%\Build\dependencies" (
    echo Cloning dependencies repo ...
    pushd "%cd%\Build"
    "%cd%\Build\git\cmd\git.exe" clone https://OpenHUTB:T8w6TYB_r71gGTP3A02B@git.code.tencent.com/OpenHUTB/dependencies.git  &&  cd dependencies  && git lfs pull
    popd
) else (
    echo dependencies repo already exists, updating dependencies repository.
    pushd "%cd%\Build\dependencies"
    :: discard any local changes
    :: "%cd%\Build\git\cmd\git.exe" reset --hard
    :: "%cd%\Build\git\cmd\git.exe" pull
    :: "%cd%\Build\git\cmd\git.exe" lfs pull
    popd
)

if exist "%cd%\Build\dependencies\" (
    :: TODO: extract all *.zip files to current directory in dependencies repository
    echo dependencies repo is ready.
    pushd "%cd%\Build\dependencies\"

    if not exist "prerequisites\7zip" (
        echo Unzipping 7zip ...
        powershell -Command "Expand-Archive -Path 'prerequisites\7zip.zip' -DestinationPath 'prerequisites\' -Force" || exit /b
    ) else (
        echo 7zip folder already exists.
    )


    rem ---------------------------------------------------------------------------------------------------------------
    rem Unzip Plugins
    rem ---------------------------------------------------------------------------------------------------------------
    rem Unzip RoadRunner Plugins
    if not exist "%cd%\Unreal\CarlaUE4\Plugins\RoadRunnerRuntime" (
        echo Unzipping Roadrunner Plugins ...
        "prerequisites\7zip\7z.exe" x "Plugins\RoadRunner_Plugins.zip" -o"%cd%\Unreal\CarlaUE4\Plugins\" -y >nul
    ) else (
        echo RoadRunner Plugins already exists.
    )
    rem Unzip CesiumForUnreal Plugin
    if not exist "%cd%\Unreal\CarlaUE4\Plugins\CesiumForUnreal" (
        echo Unzipping CesiumForUnreal Plugin ...
        "prerequisites\7zip\7z.exe" x "Plugins\CesiumForUnreal-426-v1.18.0-ue4.zip" -o"%cd%\Unreal\CarlaUE4\Plugins\" -y >nul
    ) else (
        echo CesiumForUnreal Plugin already exists.
    )
    rem Unzip glTFForUE4 Plugin (For Mujoco .glb file input)
    if not exist "%cd%\Unreal\CarlaUE4\Plugins\glTFForUE4" (
        echo Unzipping glTFForUE4 Plugin ...
        "prerequisites\7zip\7z.exe" x "Plugins\glTFForUE4.zip" -o"%cd%\Unreal\CarlaUE4\Plugins\" -y >nul
    ) else (
        echo glTFForUE4 Plugin already exists.
    )

        
    rem ============================================================================
    rem -- Initial UnrealRoboticsLab dependencies ----------------------------------
    rem ============================================================================

    echo %FILE_N% Initial UnrealRoboticsLab dependencies...
    set UnrealRoboticsLab_FOLDER=%cd%\Unreal\CarlaUE4\Plugins\UnrealRoboticsLab\
    echo UnrealRoboticsLab_FOLDER: %UnrealRoboticsLab_FOLDER%
    echo "%UnrealRoboticsLab_FOLDER%third_party\install" folder...
    if not exist "%cd%\Unreal\CarlaUE4\Plugins\UnrealRoboticsLab\third_party\install" (
        "prerequisites\7zip\7z.exe" x "Plugins\mujoco-3.3.5-windows-x86_64.zip" -o"%cd%\Unreal\CarlaUE4\Plugins\UnrealRoboticsLab\third_party\install\MuJoCo" -y
        "prerequisites\7zip\7z.exe" x "Plugins\CoACD.zip" -o"%cd%\Unreal\CarlaUE4\Plugins\UnrealRoboticsLab\third_party\install\" -y
        "prerequisites\7zip\7z.exe" x "Plugins\libzmq.zip" -o"%cd%\Unreal\CarlaUE4\Plugins\UnrealRoboticsLab\third_party\install\" -y
    ) else (
        echo Found UnrealRoboticsLab dependencies at "%UnrealRoboticsLab_FOLDER%third_party\install\".
    )


    rem ---------------------------------------------------------------------------------------------------------------
    rem Unzip dependencies
    rem ---------------------------------------------------------------------------------------------------------------
    rem fix no XINPUT1_3.dll error when lanunch UE4Editor
    rem install directx_Jun2010_redist.exe when DirectX folder not exist
    REG QUERY HKEY_CURRENT_USER\Software\Microsoft |find "DirectX" >nul
    if not exist "%SystemRoot%\system32\xinput1_3.dll" (
        echo DirectX not found.
        echo Unzipping DirectX ...
        "prerequisites\7zip\7z.exe" x "prerequisites\directx.zip" -o"prerequisites\" -y >nul
        echo Install DirectX ...
        call prerequisites\directx\DXSETUP.exe /silent || exit /b
    ) else (
        echo DirectX found!
    )

    rem Unzip DirectX Runtime
    if not exist "prerequisites\DirectX_Runtime" (
        echo Unzipping DirectX Runtime ...
        "prerequisites\7zip\7z.exe" x "prerequisites\DirectX_Runtime.zip" -o"prerequisites\" -y >nul
    ) else (
        echo DirectX Runtime already exists.
    )

    if not exist "prerequisites\miniconda3\" (
        echo Unzipping miniconda...
        "prerequisites\7zip\7z.exe" x "prerequisites\miniconda3.zip" -o"prerequisites\" -y >nul
        rem set script_path=%~dp0
        rem set PATH=!script_path!Build\dependencies\prerequisites\miniconda3;!PATH!
        rem set PATH=!script_path!Build\dependencies\prerequisites\miniconda3\Scripts;!PATH!
        rem set PATH=!script_path!Build\dependencies\prerequisites\miniconda3\Library\bin;!PATH!
        rem echo After add miniconda3 path:
        rem echo !PATH!
        rem where conda
        rem conda env list
        rem call "%cd%\Build\dependencies\prerequisites\miniconda3\Scripts\conda.exe" tos accept
        rem for /l %%i in (14,-1,7) do (
        rem     echo "If conda viural environment hutb_3.%%i already exists, delete it"
        rem     call "%cd%\Build\dependencies\prerequisites\miniconda3\Scripts\conda.exe" remove -n hutb_3.%%i --all --yes
        rem     echo "Creating new conda environment hutb_3.%%i ..."
        rem     call "%cd%\Build\dependencies\prerequisites\miniconda3\Scripts\conda.exe" create -n hutb_3.%%i python=3.%%i --yes
        rem     call "%cd%\Build\dependencies\prerequisites\miniconda3\envs\hutb_3.%%i\Scripts\pip.exe" install -r %cd%\PythonAPI\carla\requirements.txt
        rem )
    ) else (
        echo miniconda3 folder already exists.
    )
    rem %WINDIR%\System32\cmd.exe "/K" %cd%\Build\dependencies\prerequisites\miniconda3\Scripts\activate.bat hutb_3.8

    if not exist "prerequisites\CMake\" (
        echo Unzipping CMake ...
        "prerequisites\7zip\7z.exe" x "prerequisites\CMake.zip" -o"prerequisites\" -y >nul
    ) else (
        echo CMake folder already exists.
    )
    
    if not exist "prerequisites\dotnet\" (
        echo Unzipping dotnet ...
        "prerequisites\7zip\7z.exe" x "prerequisites\dotnet.zip" -o"prerequisites\" -y >nul
    ) else (
        echo dotnet folder already exists.
    )
    if not exist "prerequisites\git\" (
        echo Unzipping git ...
        "prerequisites\7zip\7z.exe" x "prerequisites\git.zip" -o"prerequisites\" -y >nul
    ) else (
        echo git folder already exists.
    )
    if not exist "prerequisites\GnuWin32\" (
        echo Unzipping GnuWin32 ...
        "prerequisites\7zip\7z.exe" x "prerequisites\GnuWin32.zip" -o"prerequisites\" -y >nul
    ) else (
        echo GnuWin32 folder already exists.
    )


    rem Extract src zip files
    if not exist "%cd%\Build\boost-1.90.0-source" (
        echo Unzipping Build\dependencies\src\boost-1_90_0.zip ...
        "prerequisites\7zip\7z.exe" x "src\boost-1_90_0.zip" -o"%cd%\Build\" -y >nul
    ) else (
        echo Build\boost-1.90.0-source folder already exists.
    )
    if not exist "%cd%\Build\chrono-src" (
        echo Unzipping chrono-src.zip ...
        "prerequisites\7zip\7z.exe" x "src\chrono-src.zip" -o"%cd%\Build\" -y >nul
    ) else (
        echo Build\chrono-src folder already exists.
    )
    if not exist "%cd%\Build\eigen-3.3.7" (
        echo Unzipping eigen-3.3.7.zip ...
        "prerequisites\7zip\7z.exe" x "src\eigen-3.3.7.zip" -o"%cd%\Build\" -y >nul
    ) else (
        echo Build\eigen-3.3.7 folder already exists.
    )
    if not exist "%cd%\Build\fastDDS-src" (
        echo Unzipping fastDDS-src.zip ...
        "prerequisites\7zip\7z.exe" x "src\fastDDS-src.zip" -o"%cd%\Build\" -y >nul
    ) else (
        echo Build\fastDDS-src folder already exists.
    )
    if not exist "%cd%\Build\gtest-src" (
        echo Unzipping gtest-src.zip ...
        "prerequisites\7zip\7z.exe" x "src\gtest-src.zip" -o"%cd%\Build\" -y >nul
    ) else (
        echo Build\gtest-src folder already exists.
    )
    if not exist "%cd%\Build\libpng-1.2.37-source" (
        echo Unzipping libpng-1.2.37-source.zip ...
        "prerequisites\7zip\7z.exe" x "src\libpng-1.2.37-source.zip" -o"%cd%\Build\" -y >nul
    ) else (
        echo Build\libpng-1.2.37-source folder already exists.
    )
    if not exist "%cd%\Build\osm2odr-source" (
        echo Unzipping osm2odr-source.zip ...
        "prerequisites\7zip\7z.exe" x "src\osm2odr-source.zip" -o"%cd%\Build\" -y >nul
    ) else (
        echo Build\osm2odr-source folder already exists.
    )
    if not exist "%cd%\Build\proj-src" (
        echo Unzipping proj-src.zip ...
        "prerequisites\7zip\7z.exe" x "src\proj-src.zip" -o"%cd%\Build\" -y >nul
    ) else (
        echo Build\proj-src folder already exists.
    )
    if not exist "%cd%\Build\recast-src" (
        echo Unzipping recast-src.zip ...
        "prerequisites\7zip\7z.exe" x "src\recast-src.zip" -o"%cd%\Build\" -y >nul
    ) else (
        echo Build\recast-src folder already exists.
    )
    if not exist "%cd%\Build\rpclib-src" (
        echo Unzipping rpclib-src.zip ...
        "prerequisites\7zip\7z.exe" x "src\rpclib-src.zip" -o"%cd%\Build\" -y >nul
    ) else (
        echo Build\rpclib-src folder already exists.
    )
    if not exist "%cd%\Build\sqlite3-src" (
        echo Unzipping sqlite3-src.zip ...
        "prerequisites\7zip\7z.exe" x "src\sqlite3-src.zip" -o"%cd%\Build\" -y >nul
    ) else (
        echo Build\sqlite3-src folder already exists.
    )
    if not exist "%cd%\Build\xerces-c-3.2.3-source" (
        echo Unzipping xerces-c-3.2.3-source.zip ...
        "prerequisites\7zip\7z.exe" x "src\xerces-c-3.2.3-source.zip" -o"%cd%\Build\" -y >nul
    ) else (
        echo Build\xerces-c-3.2.3-source folder already exists.
    )
    if not exist "%cd%\Build\zlib-source" (
        echo Unzipping zlib-source.zip ...
        "prerequisites\7zip\7z.exe" x "src\zlib-source.zip" -o"%cd%\Build\" -y >nul
    ) else (
        echo Build\zlib-source folder already exists.
    )

    popd
) else (
    echo Build\dependencies\ not found, please check the dependencies repository clone.
    exit /b
)


rem ============================================================================
rem -- Installation prerequisites ----------------------------------------------
rem ============================================================================

if not "%python_root%"=="" (
    set python_path=%python_root%\python
)

rem -- PREREQUISITES INSTALL STEP --

if %skip_prerequisites%==false (
    echo Installing prerequisites...
    call Util/InstallersWin/install_prerequisites.bat --python-path=%python_path% || exit /b
) else (
    echo Skipping prerequisites install step.
)


rem Activate VS terminal development environment:
set "vs_env_bat="
rem for vs 2019
if exist "%programfiles(x86)%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set "vs_env_bat=%programfiles(x86)%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
)
if exist "%programfiles(x86)%\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat" (
    set "vs_env_bat=%programfiles(x86)%\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"
)
if exist "%programfiles(x86)%\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
    set "vs_env_bat=%programfiles(x86)%\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
)

rem for vs 2022
if exist "%ProgramW6432%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set "vs_env_bat=%ProgramW6432%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
)
if exist "%ProgramW6432%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
    set "vs_env_bat=%ProgramW6432%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
)
if exist "%ProgramW6432%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
    set "vs_env_bat=%ProgramW6432%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
)

if not "%vs_env_bat%"=="" (
    echo Activating "x64 Native Tools Command Prompt" terminal environment.
    call "%vs_env_bat%" || exit /b
) else (
    echo Could not find vcvars64.bat for VS, aborting setup...
    exit 1
)

rem make LibCarla ARGS="--chrono" >LibCarla.log

rem make PythonAPI ARGS="--chrono" >python.log

rem call %cd%\Build\dependencies\prerequisites\GnuWin32\bin\make launch ARGS="--chrono"

if %launch% == true (
    echo Launching Unreal Editor, log to launch.log...
    make launch ARGS="--chrono" >launch.log
)
if %package% == true (
    echo Packaging HUTB, log to package.log...
    make package ARGS="--chrono" >package.log
)



:get_current_time_in_seconds
    for /f %%a in ('powershell -command "[int]((Get-Date -UFormat '%%s') -split ',.')[0]"') do set %1=%%a
    goto :eof
