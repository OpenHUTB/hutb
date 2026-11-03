<#
.SYNOPSIS
    CarlaAir 联合仿真一键启动与自动化调试器 (Windows Launcher)

.DESCRIPTION
    本脚本是面向开发者在本地开发与算法调试阶段的一键启动工具，用于自动化拉起 CARLA 与 AirSim 联合仿真环境。

    【与 StartOpenHUTB.bat / CarlaUE4.exe 的区别与工作流定位】：
    - StartOpenHUTB.bat / CarlaUE4.exe：
      面向最终发布/打包（Package）版本用户的标准启动方式。主要用于部署后直接运行打包好的二进制文件，并自动检查 VC++ 运行库。
    - CarlaAir.ps1：
      面向开发人员与算法测试人员的 PowerShell 自动化启动脚本。具备以下高级特性：
      1. 自动定位本地 Carla 可执行文件（支持 Editor 模式、开发构建与 WindowsNoEditor 打包模式）；
      2. 灵活指定仿真地图（默认 Town10HD）、窗口分辨率、画质等级；
      3. 支持载具视角快速切换（通过 --vehicle 参数在空中无人机 drone 与水下机器人 rov 之间自动切换）；
      4. 自动分发对应的 AirSim 配置文件到 ~/Documents/AirSim/settings.json；
      5. 自动检测 CARLA (2000) 和 AirSim (41451) 端口连通性，并在就绪后自动拉起背景交通流进程 (auto_traffic.py)；
      6. 支持实例健康监控、日志追踪 (--log) 与一键清理停止 (--kill)。

.EXAMPLE
    # 启动水下机器人 ROV 仿真（默认地图 Town10HD）
    .\CarlaAir.ps1 --vehicle rov

    # 启动空中多旋翼无人机仿真
    .\CarlaAir.ps1 --vehicle drone

    # 在 Town10HD 上启动 ROV，禁用背景交通流，并挂接控制台查看输出
    .\CarlaAir.ps1 Town10HD --vehicle rov --no-traffic --fg

    # 停止当前运行的 CarlaAir 实例
    .\CarlaAir.ps1 --kill
#>

$ErrorActionPreference = "Stop"

function Show-Usage {
    @"
Usage: .\CarlaAir.ps1 [MAP] [OPTIONS]

Workflow Note:
  - For packaged end-user deployments: Use StartOpenHUTB.bat or CarlaUE4.exe.
  - For development & automated co-simulation testing: Use .\CarlaAir.ps1.

Options:
  --vehicle TYPE           Vehicle type to spawn: 'rov' (default) or 'drone'
  --res WxH                Window resolution (default: 1280x720)
  --port PORT              CARLA RPC port (default: 2000)
  --quality LEVEL          Quality level: Low, Medium, High, Epic
  --fg                     Keep this PowerShell session attached to the process
  --kill                   Stop the last CarlaAir instance started by this script
  --log                    Tail the CarlaAir log file
  --no-traffic             Do not auto-start auto_traffic.py
  --traffic-vehicles N     Vehicle count for auto traffic (default: 30)
  --traffic-walkers N      Walker count for auto traffic (default: 50)
  --package-root PATH      Explicit WindowsNoEditor root
  --python PATH            Explicit python.exe for auto traffic
  --help                   Show this help
"@ | Write-Host
}

function Test-Port {
    param([int]$Port)
    try {
        $client = New-Object System.Net.Sockets.TcpClient
        $iar = $client.BeginConnect("127.0.0.1", $Port, $null, $null)
        $ok = $iar.AsyncWaitHandle.WaitOne(500)
        if (-not $ok) {
            $client.Close()
            return $false
        }
        $client.EndConnect($iar)
        $client.Close()
        return $true
    } catch {
        return $false
    }
}

function Resolve-CondaExe {
    $commandConda = Get-Command conda -ErrorAction SilentlyContinue
    $candidates = @(
        $env:CONDA_EXE,
        $commandConda.Source,
        "D:\Code\Anaconda3\Scripts\conda.exe",
        (Join-Path $env:LOCALAPPDATA "miniconda3\Scripts\conda.exe"),
        (Join-Path $env:LOCALAPPDATA "anaconda3\Scripts\conda.exe"),
        (Join-Path $env:USERPROFILE "miniconda3\Scripts\conda.exe"),
        (Join-Path $env:USERPROFILE "anaconda3\Scripts\conda.exe"),
        "C:\ProgramData\miniconda3\Scripts\conda.exe",
        "C:\ProgramData\anaconda3\Scripts\conda.exe"
    ) | Where-Object { $_ }

    foreach ($candidate in $candidates | Select-Object -Unique) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    return $null
}

function Resolve-TrafficPython {
    param([string]$ExplicitPath)

    if ($ExplicitPath) {
        if (-not (Test-Path $ExplicitPath)) {
            throw "Python path not found: $ExplicitPath"
        }
        return (Resolve-Path $ExplicitPath).Path
    }

    if ($env:CARLAAIR_PYTHON_EXE -and (Test-Path $env:CARLAAIR_PYTHON_EXE)) {
        return (Resolve-Path $env:CARLAAIR_PYTHON_EXE).Path
    }

    $pathPython = Get-Command python -ErrorAction SilentlyContinue
    if ($pathPython) {
        try {
            & $pathPython.Source -c "import carla" 2>$null
            if ($LASTEXITCODE -eq 0) {
                return $pathPython.Source
            }
        } catch {
        }
    }

    $condaExe = Resolve-CondaExe
    if ($condaExe) {
        try {
            $resolved = & $condaExe run -n carlaAir python -c "import carla, sys; print(sys.executable)" 2>$null
            if ($LASTEXITCODE -eq 0 -and $resolved) {
                $resolvedLines = @($resolved) | ForEach-Object { "$_".Trim() } | Where-Object { $_ }
                $candidate = $resolvedLines | Select-Object -Last 1
                if ($candidate -and (Test-Path $candidate)) {
                    return (Resolve-Path $candidate).Path
                }
            }
        } catch {
        }
    }

    return $null
}

function Resolve-CarlaBinary {
    param([string]$RepoRoot, [string]$ExplicitPackageRoot)

    $packageRoots = @()
    if ($ExplicitPackageRoot) {
        $packageRoots += $ExplicitPackageRoot
    } else {
        $packageRoots += $RepoRoot
        $packageRoots += Join-Path $RepoRoot "WindowsNoEditor"
        $packageRoots += Get-ChildItem (Join-Path $RepoRoot "Build\UE4Carla") -Directory -ErrorAction SilentlyContinue | ForEach-Object {
            Join-Path $_.FullName "WindowsNoEditor"
        }
    }

    foreach ($packageRoot in $packageRoots | Select-Object -Unique) {
        if (-not $packageRoot) { continue }

        $shippingExe = Join-Path $packageRoot "CarlaUE4\Binaries\Win64\CarlaUE4-Win64-Shipping.exe"
        if (Test-Path $shippingExe) {
            return @{
                Binary = (Resolve-Path $shippingExe).Path
                WorkingDirectory = (Resolve-Path $packageRoot).Path
                NeedsProjectArg = $true
            }
        }

        $rootExe = Join-Path $packageRoot "CarlaUE4.exe"
        if (Test-Path $rootExe) {
            return @{
                Binary = (Resolve-Path $rootExe).Path
                WorkingDirectory = (Resolve-Path $packageRoot).Path
                NeedsProjectArg = $false
            }
        }
    }

    $devExe = Join-Path $RepoRoot "Unreal\CarlaUE4\Binaries\Win64\CarlaUE4.exe"
    if (Test-Path $devExe) {
        return @{
            Binary = (Resolve-Path $devExe).Path
            WorkingDirectory = Split-Path -Parent $devExe
            NeedsProjectArg = $false
        }
    }

    $editorCandidates = @()
    if ($env:UE4_ROOT) {
        $p = Join-Path $env:UE4_ROOT "Engine\Binaries\Win64\UE4Editor.exe"
        if (Test-Path $p) { $editorCandidates += $p }
    }
    $defaultEditor = "E:\Projects\hutb_editor\hutb_editor\unreal\Engine\Binaries\Win64\UE4Editor.exe"
    if (Test-Path $defaultEditor) { $editorCandidates += $defaultEditor }

    $uproject = Join-Path $RepoRoot "Unreal\CarlaUE4\CarlaUE4.uproject"
    if (($editorCandidates.Count -gt 0) -and (Test-Path $uproject)) {
        return @{
            Binary = (Resolve-Path $editorCandidates[0]).Path
            WorkingDirectory = Split-Path -Parent $uproject
            NeedsProjectArg = $false
            IsEditor = $true
            ProjectFile = (Resolve-Path $uproject).Path
        }
    }

    throw "No Windows Carla binary found. Build the project first with .\BuildWindows.ps1."
}

function Resolve-CarlaLogFile {
    param(
        [string]$RepoRoot,
        [string]$ExplicitPackageRoot
    )

    $candidates = New-Object System.Collections.Generic.List[string]
    $candidates.Add((Join-Path $RepoRoot "CarlaAir.log"))

    $packageRoots = @()
    if ($ExplicitPackageRoot) {
        $packageRoots += $ExplicitPackageRoot
    } else {
        $packageRoots += $RepoRoot
        $packageRoots += Join-Path $RepoRoot "WindowsNoEditor"
        $packageRoots += Get-ChildItem (Join-Path $RepoRoot "Build\UE4Carla") -Directory -ErrorAction SilentlyContinue | ForEach-Object {
            Join-Path $_.FullName "WindowsNoEditor"
        }
    }

    foreach ($packageRoot in $packageRoots | Select-Object -Unique) {
        if (-not $packageRoot) { continue }
        $candidates.Add((Join-Path $packageRoot "CarlaAir.log"))
        $candidates.Add((Join-Path $packageRoot "CarlaUE4\Saved\Logs\CarlaUE4.log"))
        $candidates.Add((Join-Path $packageRoot "CarlaUE4\Binaries\Win64\CarlaUE4.log"))
    }

    foreach ($candidate in $candidates | Select-Object -Unique) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    $checkedPaths = ($candidates | Select-Object -Unique) -join ", "
    throw "No log file found. Checked: $checkedPaths"
}

function Assert-NonNegative {
    param(
        [string]$Name,
        [int]$Value
    )

    if ($Value -lt 0) {
        throw "$Name must be a non-negative integer."
    }
}

function Stop-CarlaAirProcess {
    param([string]$RepoRoot)

    foreach ($pidFile in @((Join-Path $RepoRoot ".carlaair.pid"), (Join-Path $RepoRoot ".traffic.pid"))) {
        if (-not (Test-Path $pidFile)) { continue }
        $pidValue = Get-Content $pidFile -ErrorAction SilentlyContinue | Select-Object -First 1
        $pidInt = 0
        if ([int]::TryParse($pidValue, [ref]$pidInt)) {
            $process = Get-Process -Id $pidInt -ErrorAction SilentlyContinue
            if ($process) {
                Stop-Process -Id $pidInt -Force
            }
        }
        Remove-Item $pidFile -ErrorAction SilentlyContinue
    }

    Get-Process CarlaUE4, CarlaUE4-Win64-Shipping -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
}

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$mapName = "Town10HD"
$vehicleType = "rov"
$resX = 1280
$resY = 720
$carlaPort = 2000
$airsimPort = 41451
$quality = "Epic"
$foreground = $false
$showLog = $false
$killOnly = $false
$autoTraffic = $true
$trafficVehicles = 30
$trafficWalkers = 50
$explicitPackageRoot = $null
$explicitPython = $null

for ($i = 0; $i -lt $args.Count; $i++) {
    $arg = [string]$args[$i]
    switch -Regex ($arg) {
        '^--help$|^-h$' { Show-Usage; exit 0 }
        '^--vehicle$' { $i++; $vehicleType = ([string]$args[$i]).ToLower(); continue }
        '^--fg$' { $foreground = $true; continue }
        '^--kill$' { $killOnly = $true; continue }
        '^--log$' { $showLog = $true; continue }
        '^--no-traffic$' { $autoTraffic = $false; continue }
        '^--traffic-vehicles$' { $i++; $trafficVehicles = [int]$args[$i]; continue }
        '^--traffic-walkers$' { $i++; $trafficWalkers = [int]$args[$i]; continue }
        '^--res$' {
            $i++
            $parts = ([string]$args[$i]).Split("x")
            if ($parts.Count -ne 2) { throw "Invalid resolution: $($args[$i])" }
            $resX = [int]$parts[0]
            $resY = [int]$parts[1]
            continue
        }
        '^--port$' { $i++; $carlaPort = [int]$args[$i]; continue }
        '^--quality$' { $i++; $quality = [string]$args[$i]; continue }
        '^--package-root$' { $i++; $explicitPackageRoot = [string]$args[$i]; continue }
        '^--python$' { $i++; $explicitPython = [string]$args[$i]; continue }
        '^Town.*|^town.*' { $mapName = $arg; continue }
        default { throw "Unknown option: $arg" }
    }
}

Assert-NonNegative -Name "--traffic-vehicles" -Value $trafficVehicles
Assert-NonNegative -Name "--traffic-walkers" -Value $trafficWalkers
Assert-NonNegative -Name "--port" -Value $carlaPort

$pidFile = Join-Path $repoRoot ".carlaair.pid"
$trafficPidFile = Join-Path $repoRoot ".traffic.pid"
$logFile = Join-Path $repoRoot "CarlaAir.log"
$trafficLogFile = Join-Path $repoRoot "traffic.log"
$trafficErrFile = Join-Path $repoRoot "traffic.err.log"

if ($killOnly) {
    Stop-CarlaAirProcess -RepoRoot $repoRoot
    Write-Host "CarlaAir stopped."
    exit 0
}

if ($showLog) {
    $resolvedLogFile = Resolve-CarlaLogFile -RepoRoot $repoRoot -ExplicitPackageRoot $explicitPackageRoot
    Get-Content $resolvedLogFile -Wait -Tail 100
    exit 0
}

$binaryInfo = Resolve-CarlaBinary -RepoRoot $repoRoot -ExplicitPackageRoot $explicitPackageRoot
$trafficPython = if ($autoTraffic) { Resolve-TrafficPython -ExplicitPath $explicitPython } else { $null }

$settingsFileName = if ($vehicleType -eq "drone" -or $vehicleType -eq "multirotor") {
    "settings_drone.json"
} else {
    "settings_rov.json"
}
$airsimSettingsSource = Join-Path $repoRoot "AirSimConfig\$settingsFileName"
if (-not (Test-Path $airsimSettingsSource)) {
    $fallback = Join-Path $repoRoot "AirSimConfig\settings.json"
    if (Test-Path $fallback) {
        $airsimSettingsSource = $fallback
    } else {
        throw "AirSim settings template not found: $airsimSettingsSource"
    }
}

$airsimSettingsDir = Join-Path $env:USERPROFILE "Documents\AirSim"
$airsimSettingsTarget = Join-Path $airsimSettingsDir "settings.json"
New-Item -ItemType Directory -Force -Path $airsimSettingsDir | Out-Null
Copy-Item $airsimSettingsSource $airsimSettingsTarget -Force
Write-Host "AirSim Settings: Dispatched $settingsFileName -> $airsimSettingsTarget"

Stop-CarlaAirProcess -RepoRoot $repoRoot

$launchArgs = New-Object System.Collections.Generic.List[string]
if ($binaryInfo.IsEditor) {
    $launchArgs.Add("`"$($binaryInfo.ProjectFile)`"")
    $launchArgs.Add($mapName)
    $launchArgs.Add("-game")
} elseif ($binaryInfo.NeedsProjectArg) {
    $launchArgs.Add("CarlaUE4")
    $launchArgs.Add($mapName)
} else {
    $launchArgs.Add($mapName)
}
$launchArgs.Add("-windowed")
$launchArgs.Add("-ResX=$resX")
$launchArgs.Add("-ResY=$resY")
$launchArgs.Add("-carla-rpc-port=$carlaPort")
$launchArgs.Add("-quality-level=$quality")
$launchArgs.Add("-TexturePoolSize=2048")
$launchArgs.Add("-unattended")
$launchArgs.Add("-nosound")
$launchArgs.Add("-UseVSync")
$launchArgs.Add("-abslog=`"$logFile`"")

Write-Host "============================================"
Write-Host "  CarlaAir - Windows Launcher"
Write-Host "============================================"
Write-Host "  Map:        $mapName"
Write-Host "  Vehicle:    $vehicleType ($settingsFileName)"
Write-Host "  Resolution: ${resX}x${resY}"
Write-Host "  CARLA Port: $carlaPort"
Write-Host "  AirSim Port: $airsimPort"
Write-Host "  Quality:    $quality"
Write-Host "  Binary:     $($binaryInfo.Binary)"
Write-Host "============================================"

if ($foreground) {
    if ($autoTraffic) {
        Write-Warning "Foreground mode does not auto-start auto_traffic.py. Start it separately after CARLA is ready if needed."
    }
    Push-Location $binaryInfo.WorkingDirectory
    try {
        & $binaryInfo.Binary @launchArgs
        exit $LASTEXITCODE
    } finally {
        Pop-Location
    }
}

$process = Start-Process -FilePath $binaryInfo.Binary -ArgumentList $launchArgs -WorkingDirectory $binaryInfo.WorkingDirectory -PassThru
Set-Content -Path $pidFile -Value $process.Id

Write-Host "Waiting for CARLA port..."
$carlaReady = $false
$maxAttemptsCarla = if ($binaryInfo.IsEditor) { 900 } else { 120 }
for ($attempt = 0; $attempt -lt $maxAttemptsCarla; $attempt++) {
    Start-Sleep -Seconds 2
    if ($process.HasExited) {
        throw "CarlaAir exited early. Check $logFile"
    }
    if (Test-Port -Port $carlaPort) {
        Write-Host "  CARLA (port $carlaPort): Ready"
        $carlaReady = $true
        break
    }
    if ($attempt % 10 -eq 0 -and $attempt -gt 0) {
        Write-Host "  Still waiting for CARLA port ($($attempt * 2)s / $($maxAttemptsCarla * 2)s)..."
    }
}
if (-not $carlaReady) {
    throw "CARLA port $carlaPort did not become ready within $($maxAttemptsCarla * 2) seconds. Check $logFile"
}

Write-Host "Waiting for AirSim port..."
$airsimReady = $false
$maxAttemptsAirSim = if ($binaryInfo.IsEditor) { 300 } else { 60 }
for ($attempt = 0; $attempt -lt $maxAttemptsAirSim; $attempt++) {
    Start-Sleep -Seconds 2
    if (Test-Port -Port $airsimPort) {
        Write-Host "  AirSim (port $airsimPort): Ready"
        $airsimReady = $true
        break
    }
    if ($attempt % 10 -eq 0 -and $attempt -gt 0) {
        Write-Host "  Still waiting for AirSim port ($($attempt * 2)s / $($maxAttemptsAirSim * 2)s)..."
    }
}
if (-not $airsimReady) {
    throw "AirSim port $airsimPort did not become ready within 120 seconds. Check $logFile"
}

if ($autoTraffic) {
    if (($trafficVehicles -eq 0) -and ($trafficWalkers -eq 0)) {
        Write-Host "Traffic disabled by count (0 vehicles + 0 walkers)."
    } elseif ($trafficPython) {
        $autoTrafficScript = Join-Path $repoRoot "PythonAPI\examples\air\auto_traffic.py"
        if (-not (Test-Path $autoTrafficScript)) {
            $autoTrafficScript = Join-Path $repoRoot "auto_traffic.py"
        }
        $trafficArgs = @(
            $autoTrafficScript,
            "--vehicles", [string]$trafficVehicles,
            "--walkers", [string]$trafficWalkers,
            "--port", [string]$carlaPort
        )
        $trafficProcess = Start-Process -FilePath $trafficPython -ArgumentList $trafficArgs -WorkingDirectory $repoRoot -RedirectStandardOutput $trafficLogFile -RedirectStandardError $trafficErrFile -PassThru
        Set-Content -Path $trafficPidFile -Value $trafficProcess.Id
        Write-Host "Traffic process started: $($trafficProcess.Id)"
    } else {
        Write-Warning "auto_traffic.py was not started because no Python with the 'carla' module was found."
    }
}

Write-Host ""
Write-Host "CarlaAir is ready."
Write-Host "Log: $logFile"

if (-not $foreground) {
    Wait-Process -Id $process.Id
}
