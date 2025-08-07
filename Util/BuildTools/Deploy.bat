@echo off
setlocal enabledelayedexpansion
chcp 65001

rem ==============================================================================
rem -- Set up environment --------------------------------------------------------
rem ==============================================================================

set REPLACE_LATEST=true
:: 是否部署到pypi.org，默认不部署，只有执行 make deploy ARGS="--deploy-to-pypi"
set DEPLOY_2_PYPI=false
set AWS_COPY=aws s3 cp

rem ==============================================================================
rem -- Parse arguments -----------------------------------------------------------
rem ==============================================================================

set DOC_STRING=Upload latest build to S3

set USAGE_STRING="Usage: $0 [-h|--help]  [--summary-output=SUMMARY_OUTPUT] [--workdir=WORKING_DIRECTORY] [--replace-latest] [--deploy-to-pypi] [--dry-run]"

:arg-parse
if not "%1"=="" (
    if "%1"=="--summary-output" (
        set SUMMARY_OUTPUT_PATH=%2
        shift
    )

    if "%1"=="--workdir" (
        set WORKDIR=%2
        shift
    )

    if "%1"=="--replace-latest" (
        set REPLACE_LATEST=true
    )

    if "%1"=="--deploy-to-pypi" (
        set DEPLOY_2_PYPI=true
    )

    :: rem 表示进行排练，只显示上传文件到亚马逊云的命令
    if "%1"=="--dry-run" (
      set AWS_COPY=rem aws s3 cp
    )

    if "%1"=="--help" (
        echo %DOC_STRING%
        echo %USAGE_STRING%
        GOTO :eof
    )

    shift
    goto :arg-parse
)

rem Get repository version
call :get_git_repository_version
if not defined REPOSITORY_TAG goto error_carla_version
echo REPOSITORY_TAG = !REPOSITORY_TAG!
:: 用于测试
:: set REPOSITORY_TAG=v1.1.0-24-gd41f90e61

rem Last package data
if defined WORKDIR (
    set "CARLA_DIST_FOLDER=%WORKDIR%\Build\UE4Carla"
) else (
    set "CARLA_DIST_FOLDER=%~dp0%\Build\UE4Carla"
)
set PACKAGE=CARLA_%REPOSITORY_TAG%.zip
set PACKAGE_PATH=%CARLA_DIST_FOLDER%\%PACKAGE%
set PACKAGE2=AdditionalMaps_%REPOSITORY_TAG%.zip
set PACKAGE_PATH2=%CARLA_DIST_FOLDER%\%PACKAGE2%
set PythonAPI_FOLDER=%~dp0%\PythonAPI\carla\dist

:: 对象URL的前缀
set ENDPOINT=https://hutb.s3.ap-southeast-2.amazonaws.com

set S3_PREFIX=s3://hutb
set URL_PREFIX=%ENDPOINT%/hutb

set LATEST_DEPLOY_URI=!S3_PREFIX!/Dev/CARLA_Latest.zip
set LATEST_DEPLOY_URI2=!S3_PREFIX!/Dev/AdditionalMaps_Latest.zip

rem Check for TAG version
echo %REPOSITORY_TAG% | findstr /R /C:"^[0-9]*\.[0-9]*\.[0-9]*.$" 1>nul
if %errorlevel% == 0 (
  echo Detected release version with tag %REPOSITORY_TAG%
  set DEPLOY_NAME=CARLA_%REPOSITORY_TAG%.zip
  set DEPLOY_NAME2=AdditionalMaps_%REPOSITORY_TAG%.zip
) else (
  echo Detected non-release version with tag %REPOSITORY_TAG%
  set S3_PREFIX=!S3_PREFIX!/Dev
  set URL_PREFIX=%URL_PREFIX%/Dev
  git log --pretty=format:%%cd_%%h --date=format:%%Y%%m%%d -n 1 > tempo1234
  set /p DEPLOY_NAME= < tempo1234
  del tempo1234
  set DEPLOY_NAME=!DEPLOY_NAME!.zip
  echo deploy name = !DEPLOY_NAME!
  
  git log --pretty=format:%%h -n 1 > tempo1234
  set /p DEPLOY_NAME2= < tempo1234
  del tempo1234
  set DEPLOY_NAME2=AdditionalMaps_!DEPLOY_NAME2!.zip
  echo deploy name2 = !DEPLOY_NAME2!
)
echo Version detected: %REPOSITORY_TAG%
echo Using package %PACKAGE% as %DEPLOY_NAME%


rem ==============================================================================
rem -- 上传 PythonAPI 到 Pypi ----------------------------------------------------
rem ==============================================================================

if %DEPLOY_2_PYPI%==true (
  set HTTPS_PROXY=127.0.0.1:7890
  echo deploy %PythonAPI_FOLDER% to pypi...
  for /r "%PythonAPI_FOLDER%" %%F in (*.whl) do (
    echo Upload "%%F" to Pypi ...
    :: 需要安装 pip install twine
    twine upload "%%F"
  )
  :: twin upload 
  goto good_exit
)


:: 检查需要上传的包是否存在
if not exist "%PACKAGE_PATH%" (
  echo Latest package not found, please run 'make package'
  goto :bad_exit
)


rem ==============================================================================
rem -- Upload --------------------------------------------------------------------
rem ==============================================================================

aws configure set aws_access_key_id AKIA2CSTT3LD3A
aws configure set aws_secret_access_key l6h6lUhmejk1oEo2CZiNUfDhi8fyK1z9haBDEaRF
aws configure list
:: echo upload test file
:: echo %AWS_COPY% Build\CMakeLists.txt.in s3://hutb/
:: %AWS_COPY% Build\CMakeLists.txt.in s3://hutb/

echo upload released Carla package 
set DEPLOY_URI=!S3_PREFIX!/%DEPLOY_NAME%
%AWS_COPY% %PACKAGE_PATH% %DEPLOY_URI% --endpoint-url %ENDPOINT%
echo Latest build uploaded to %DEPLOY_URI%

set DEPLOY_URI2=!S3_PREFIX!/%DEPLOY_NAME2%
%AWS_COPY% %PACKAGE_PATH2% %DEPLOY_URI2% --endpoint-url %ENDPOINT%
echo Latest build uploaded to %DEPLOY_URI2%

rem ==============================================================================
rem -- Replace Latest ------------------------------------------------------------
rem ==============================================================================

if %REPLACE_LATEST%==true (
  %AWS_COPY% %DEPLOY_URI% %LATEST_DEPLOY_URI% --endpoint-url %ENDPOINT%
  echo Latest build updated as %LATEST_DEPLOY_URI%
  %AWS_COPY% %DEPLOY_URI2% %LATEST_DEPLOY_URI2% --endpoint-url %ENDPOINT%
  echo Latest build updated as %LATEST_DEPLOY_URI2%
)

rem ==============================================================================
rem -- Summary output ------------------------------------------------------------
rem ==============================================================================

if defined SUMMARY_OUTPUT_PATH (
    echo package_uri=%URL_PREFIX%/%DEPLOY_NAME%>> "%SUMMARY_OUTPUT_PATH%"
    echo additional_maps_package_uri=%URL_PREFIX%/%DEPLOY_NAME2%>> "%SUMMARY_OUTPUT_PATH%"
)

rem ============================================================================
rem -- Helper functions --------------------------------------------------------
rem ============================================================================

:get_git_repository_version
   %ROOT_PATH:/=\%Util\BuildTools\Environment.bat %*

rem ==============================================================================
rem -- ...and we are done --------------------------------------------------------
rem ==============================================================================

echo Success!

:success
    echo.
    goto good_exit

:error_carla_version
    echo.
    echo %FILE_N% [ERROR] Carla Version is not set
    goto bad_exit

:good_exit
    endlocal
    exit /b 0

:bad_exit
    endlocal
    exit /b 1
