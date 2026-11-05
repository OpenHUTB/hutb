# ================================================================
# OpenHUTB 水下机器人 (ROV) 仿真编辑器一键启动器
# ================================================================
param(
    [string]$TargetMap = "Town10HD"
)

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

Write-Host "================================================================" -ForegroundColor Cyan
Write-Host "       OpenHUTB 水下机器人 (ROV) 仿真编辑器一键启动器" -ForegroundColor Cyan
Write-Host "================================================================" -ForegroundColor Cyan

$rootDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$uproject = Join-Path $rootDir "Unreal\CarlaUE4\CarlaUE4.uproject"
$settingsSrc = Join-Path $rootDir "Unreal\CarlaUE4\settings_rov.json"
$airsimUserDir = Join-Path $env:USERPROFILE "Documents\AirSim"
$settingsDst = Join-Path $airsimUserDir "settings.json"

# 1. 自动同步 AirSim ROV 配置文件
if (-not (Test-Path $airsimUserDir)) {
    New-Item -ItemType Directory -Path $airsimUserDir -Force | Out-Null
}
if (Test-Path $settingsSrc) {
    Copy-Item -Path $settingsSrc -Destination $settingsDst -Force
    Write-Host "[OK] 水下 ROV 仿真配置已生效: $settingsDst" -ForegroundColor Green
} else {
    Write-Host "[WARN] 未找到配置模板: $settingsSrc" -ForegroundColor Yellow
}

# 2. 定位 UE4Editor 可执行文件
$ue4Editor = $null
if ($env:UE4_ROOT -and (Test-Path (Join-Path $env:UE4_ROOT "Engine\Binaries\Win64\UE4Editor.exe"))) {
    $ue4Editor = Join-Path $env:UE4_ROOT "Engine\Binaries\Win64\UE4Editor.exe"
}
if (-not $ue4Editor) {
    $defaultEditor = "E:\Projects\hutb_editor\hutb_editor\unreal\Engine\Binaries\Win64\UE4Editor.exe"
    if (Test-Path $defaultEditor) {
        $ue4Editor = $defaultEditor
    }
}

if (-not $ue4Editor) {
    Write-Host "[ERROR] 未找到 UE4Editor.exe，请检查 UE4 引擎安装路径。" -ForegroundColor Red
    Read-Host "按回车键退出..."
    exit 1
}

# 3. 确定关卡路径
$mapArg = if ($TargetMap.StartsWith("/Game/")) { $TargetMap } else { "/Game/Carla/Maps/$TargetMap" }

Write-Host "[INFO] 正在启动虚幻编辑器 (UE4Editor)..." -ForegroundColor Yellow
Write-Host "  * 引擎路径: $ue4Editor"
Write-Host "  * 工程项目: $uproject"
Write-Host "  * 目标关卡: $mapArg"
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host "【操作提示】:" -ForegroundColor Green
Write-Host "1. 编辑器打开后，在视口中点击顶部工具栏绿色的【Play (运行)】按钮；"
Write-Host "2. 在游戏视口中鼠标点击一下获取焦点："
Write-Host "   - [W/S] 前进 / 后退"
Write-Host "   - [A/D] 左平移 / 右平移"
Write-Host "   - [Space/C] 垂直上升 / 下潜"
Write-Host "   - [Q/E] 偏航左转 / 右转"
Write-Host "   - [0~4 / V] 切换双目相机与跟随视角"
Write-Host "3. 松开所有按键，ROV 自动定深自稳，绝不下坠掉出世界！" -ForegroundColor Cyan
Write-Host "================================================================" -ForegroundColor Cyan

Start-Process -FilePath $ue4Editor -ArgumentList "`"$uproject`" `"$mapArg`""
Write-Host "[OK] 虚幻编辑器已在后台拉起，请查看任务栏或稍候视口加载。" -ForegroundColor Green
Start-Sleep -Seconds 3
