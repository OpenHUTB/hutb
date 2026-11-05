#requires -Version 5.1
<#
.SYNOPSIS
    拉取 OceanPlugin（UE4.26 原生海洋 / 水下材质库）

.DESCRIPTION
    OceanPlugin 是第三方仓库（234 MB），体积大且非本项目代码，因此不入库。
    本脚本把它下载到 Plugins/OceanPlugin。

    来源：https://github.com/UE4-OceanProject/OceanPlugin
    许可：允许在任何 UE4 项目中使用（含商业），无需署名；
          唯一限制是不能把插件本身当商品转售。
          全文见拉取后的 LICENSE 文件。

    说明：本机 git 协议访问 GitHub 可能被重置，所以优先用 HTTPS 下载 zip；
    若下载失败，请按脚本末尾提示手动下载。

.PARAMETER Force
    已存在时也重新拉取。

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File Unreal\CarlaUE4\Plugins\setup_ocean_plugin.ps1
#>

param(
    [switch]$Force
)

$ErrorActionPreference = "Stop"

# 脚本就在 Plugins/ 目录下，OceanPlugin 与它同级
$target = Join-Path $PSScriptRoot "OceanPlugin"
$uplugin = Join-Path $target "OceanPlugin.uplugin"

Write-Host "=== OceanPlugin 安装 ===" -ForegroundColor Cyan
Write-Host "目标目录: $target"

if ((Test-Path $uplugin) -and (-not $Force)) {
    Write-Host "[OK] 已安装，跳过。需要重装请加 -Force" -ForegroundColor Green
    exit 0
}

if ((Test-Path $target) -and $Force) {
    Write-Host "清理旧目录..." -ForegroundColor DarkGray
    Remove-Item $target -Recurse -Force
}

$zipUrl = "https://codeload.github.com/UE4-OceanProject/OceanPlugin/zip/refs/heads/master"
$tmpZip = Join-Path $env:TEMP "OceanPlugin.zip"
$tmpDir = Join-Path $env:TEMP "OceanPlugin_extract"

$downloaded = $false
try {
    Write-Host "下载中（约 200 MB）..." -ForegroundColor DarkGray
    Invoke-WebRequest -Uri $zipUrl -OutFile $tmpZip -UseBasicParsing -TimeoutSec 1800
    $sizeMb = [math]::Round((Get-Item $tmpZip).Length / 1MB, 1)
    Write-Host "[OK] 已下载 $sizeMb MB" -ForegroundColor Green
    $downloaded = $true
}
catch {
    Write-Host "[WARN] 自动下载失败：$($_.Exception.Message)" -ForegroundColor Yellow
}

if ($downloaded) {
    if (Test-Path $tmpDir) { Remove-Item $tmpDir -Recurse -Force }
    Write-Host "解压..." -ForegroundColor DarkGray
    Expand-Archive -Path $tmpZip -DestinationPath $tmpDir -Force

    # zip 里是一层 OceanPlugin-master/ 目录
    $inner = Get-ChildItem $tmpDir -Directory | Select-Object -First 1
    if (-not $inner) {
        Write-Host "[ERROR] 解压结果为空" -ForegroundColor Red
        exit 1
    }

    New-Item -ItemType Directory -Path $target -Force | Out-Null
    Copy-Item (Join-Path $inner.FullName "*") $target -Recurse -Force

    # 去掉 git 历史，省 900 MB
    $gitDir = Join-Path $target ".git"
    if (Test-Path $gitDir) { Remove-Item $gitDir -Recurse -Force }

    Remove-Item $tmpZip -Force -ErrorAction SilentlyContinue
    Remove-Item $tmpDir -Recurse -Force -ErrorAction SilentlyContinue
}

# --- 校验 ---
Write-Host ""
if (Test-Path $uplugin) {
    $files = Get-ChildItem $target -Recurse -File -ErrorAction SilentlyContinue
    $mb = [math]::Round((($files | Measure-Object Length -Sum).Sum) / 1MB, 1)
    Write-Host "[OK] OceanPlugin 安装完成：$($files.Count) 个文件，$mb MB" -ForegroundColor Green

    $keyAssets = @(
        "Content\Ocean\Materials\M_Underwater_PP.uasset",
        "Content\Ocean\Materials\M_Ocean_Versions\M_Ocean_Ultra.uasset",
        "Content\Lake\Materials\M_Underwater_Lake_PP.uasset"
    )
    foreach ($a in $keyAssets) {
        $p = Join-Path $target $a
        if (Test-Path $p) {
            Write-Host "[OK] $(Split-Path $a -Leaf)" -ForegroundColor Green
        }
        else {
            Write-Host "[WARN] 缺少关键资产: $a" -ForegroundColor Yellow
        }
    }
    Write-Host ""
    Write-Host "下一步：重新生成工程文件并编译" -ForegroundColor Cyan
    exit 0
}
else
{
    Write-Host "[ERROR] 安装失败" -ForegroundColor Red
    Write-Host ""
    Write-Host "请手动下载：" -ForegroundColor Yellow
    Write-Host "  1. 打开 https://github.com/UE4-OceanProject/OceanPlugin" -ForegroundColor Yellow
    Write-Host "  2. Code -> Download ZIP" -ForegroundColor Yellow
    Write-Host "  3. 解压后把里面的内容放到：" -ForegroundColor Yellow
    Write-Host "     $target" -ForegroundColor Yellow
    Write-Host "     （该目录下应直接看到 OceanPlugin.uplugin）" -ForegroundColor Yellow
    exit 1
}
