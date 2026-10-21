#requires -Version 5.1
<#
Launch a built Windows DReyeVR project or package after checking its VR prerequisites.
Example: .\StartPimaxVR.ps1 -UE4Root '<UE4 source engine root>' -CheckOnly
Example: .\StartPimaxVR.ps1 -PackageRoot '<extracted package folder>'
#>
[CmdletBinding()]
param(
    [string]$UE4Root = $env:UE4_ROOT,
    [string]$PackageRoot,
    [string]$Map = '/Game/Carla/Maps/Town02',
    [string]$PimaxRoot,
    [string]$PvrDllPath = $env:PIMAX_PVR_DLL,
    [string]$SteamVRRoot,
    [ValidateSet('controller','wheel')][string]$InputDevice = 'controller',
    [string]$EgoVehicle,
    [switch]$RecordDriving,
    [ValidateRange(1,500)][int]$RecordHz = 120,
    [string]$RecordDirectory,
    [switch]$ScreenshotDriving,
    [ValidateRange(1,60)][int]$ScreenshotHz = 20,
    [switch]$AsyncScreenshot,
    [ValidateRange(0,500)][int]$EyeThreadHz = 0,
    [switch]$PerfDowngrade,
    [string]$PerfCvars,
    [int]$ProfileFrames = 0,
    [switch]$CheckOnly,
    [ValidateRange(5,180)][int]$TimeoutSeconds = 60
)

$ErrorActionPreference = 'Stop'
function Require-File([string]$Path, [string]$Message) {
    if (-not $Path -or -not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw $Message }
    return (Get-Item -LiteralPath $Path).FullName
}
function Read-Ini([string]$Path) {
    $values = @{}; $section = ''
    foreach ($line in Get-Content -LiteralPath $Path) {
        $line = ($line -split '[#;]', 2)[0].Trim()
        if ($line -match '^\[(.+)\]$') { $section = $Matches[1] }
        elseif ($line -match '^([^=]+)=(.*)$') { $values["$section.$($Matches[1].Trim())"] = $Matches[2].Trim().Trim('"') }
    }
    return $values
}
function Find-FirstFile($Candidates) {
    foreach ($candidate in $Candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            return (Get-Item -LiteralPath $candidate).FullName
        }
    }
    return $null
}
function Set-IniValue([string]$Path, [string]$Section, [string]$Key, [string]$Value) {
    $lines = @(Get-Content -LiteralPath $Path)
    $inSection = $false; $found = $false
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match '^\s*\[(.+)\]\s*$') { $inSection = ($Matches[1] -eq $Section); continue }
        if ($inSection -and $lines[$i] -match ('^\s*' + [regex]::Escape($Key) + '\s*=')) {
            $found = $true
            $comment = ($lines[$i] -split '[#;]', 2)
            $newline = "$Key=`"$Value`""
            if ($comment.Count -gt 1) { $newline += ' #' + $comment[1] }
            if ($lines[$i] -ne $newline) { $lines[$i] = $newline; Set-Content -LiteralPath $Path -Value $lines -Encoding ASCII; return $true }
            return $false
        }
    }
    throw "Missing '$Key' in [$Section] of $Path"
}

try {
    if (-not [Environment]::Is64BitProcess) { throw 'Run this script in 64-bit Windows PowerShell.' }
    if ($Map -notmatch '^/Game/[A-Za-z0-9_/]+$') { throw 'Map must be a /Game/... asset path without command-line options.' }
    if (-not $PackageRoot -and (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'CarlaUE4.exe'))) {
        $PackageRoot = $PSScriptRoot
    }
    if ($PackageRoot) {
        $root = (Resolve-Path -LiteralPath $PackageRoot).Path
        $exe = Require-File (Join-Path $root 'CarlaUE4.exe') 'No CarlaUE4.exe in PackageRoot. Use a newly built VR-enabled package.'
        $projectDir = Join-Path $root 'CarlaUE4'
        $project = Join-Path $projectDir 'CarlaUE4.uproject'
        $gameArgs = @()
    } else {
        $projectDir = Join-Path $PSScriptRoot 'Unreal\CarlaUE4'
        $project = Require-File (Join-Path $projectDir 'CarlaUE4.uproject') 'Run the launcher from the HUTB source root.'
        if (-not $UE4Root) { throw 'Specify -UE4Root or set UE4_ROOT to the built Unreal Engine directory.' }
        $exe = Require-File (Join-Path $UE4Root 'Engine\Binaries\Win64\UE4Editor.exe') 'Build UE4 first; UE4Editor.exe is missing.'
        $null = Require-File (Join-Path $projectDir 'Binaries\Win64\UE4Editor-CarlaUE4.dll') 'Build CarlaUE4Editor Win64 Development first.'
        $mapFile = Join-Path $projectDir ('Content\' + $Map.Substring(6) + '.umap')
        $null = Require-File $mapFile 'Map is not installed. Choose an installed map with -Map /Game/Carla/Maps/YourMap.'
        $gameArgs = @('"' + $project + '"')
    }
    $null = Require-File $project 'Package must include CarlaUE4.uproject for VR preflight; rebuild with the updated packaging script.'
    $descriptor = Get-Content -LiteralPath $project -Raw | ConvertFrom-Json
    if ($InputDevice -eq 'wheel') {
        if (-not @($descriptor.Plugins | Where-Object { $_.Name -eq 'LogitechWheelPlugin' -and $_.Enabled -eq $true }).Count) {
            throw 'Enable LogitechWheelPlugin BEFORE building the wheel-enabled project.'
        }
        $wheel = @(Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue | Where-Object {
            $_.InstanceId -like 'USB\VID_046D*' -and $_.FriendlyName -match 'G29|G920|G923|Racing Wheel' -and $_.Status -eq 'OK'
        })
        if (-not $wheel.Count) { throw 'No connected Logitech wheel found. Connect it and check Logitech G HUB first.' }
        Write-Host "Wheel: $($wheel[0].FriendlyName)"
    }
    if (-not @($descriptor.Plugins | Where-Object { $_.Name -eq 'SteamVR' -and $_.Enabled -eq $true }).Count) {
        throw 'SteamVR is disabled in CarlaUE4.uproject. Enable it BEFORE building/packaging; editing an old package cannot add the plugin.'
    }
    $config = Require-File (Join-Path $projectDir 'Config\DReyeVRConfig.ini') 'Missing DReyeVRConfig.ini; rebuild using the updated packaging script.'
    $ini = Read-Ini $config
    foreach ($key in @('Pimax.EnablePvrEyeTracking','EgoSensor.DrawDebugFocusTrace','EgoSensor.UseCameraRelativeGazeDisplay')) {
        if ($ini[$key] -ne 'True') { throw "Set $key=True in $config and retry." }
    }
    if ($ini['Pimax.AllowUnknownPimaxDevice'] -ne 'True' -and
        (($ini['Pimax.AllowedVendorIds'] -split ',').Trim() -notcontains '0x34A4' -or
         ($ini['Pimax.AllowedProductIds'] -split ',').Trim() -notcontains '0x0044')) {
        throw 'Dream Air VID 0x34A4 / PID 0x0044 is missing from the configured allowlist.'
    }
    # The engine has no command-line switch for the ego vehicle: it reads
    # [EgoVehicle] VehicleType from DReyeVRConfig.ini once at process start, so the
    # launcher selects the model by writing that key before the game boots.
    $vehicleDir = Join-Path $projectDir 'Config\EgoVehicles'
    $availableVehicles = @(if (Test-Path -LiteralPath $vehicleDir -PathType Container) {
        Get-ChildItem -LiteralPath $vehicleDir -Filter '*.ini' -File | ForEach-Object { $_.BaseName } | Sort-Object
    })
    if (-not $availableVehicles.Count) { throw "No ego vehicle definitions in $vehicleDir." }
    if (-not $EgoVehicle) { $EgoVehicle = $ini['EgoVehicle.VehicleType'] }
    if (-not $EgoVehicle) { throw "No [EgoVehicle] VehicleType in $config and no -EgoVehicle given." }
    if ($availableVehicles -notcontains $EgoVehicle) {
        throw "Unknown ego vehicle '$EgoVehicle'. Available: $($availableVehicles -join ', ')."
    }
    if ($CheckOnly) { Write-Host "EgoVehicle: $EgoVehicle (available: $($availableVehicles -join ', '))" }
    elseif (Set-IniValue $config 'EgoVehicle' 'VehicleType' $EgoVehicle) {
        Write-Host "EgoVehicle: $EgoVehicle (written to $(Split-Path -Leaf $config))"
    } else { Write-Host "EgoVehicle: $EgoVehicle (already configured)" }
    $bindings = Join-Path $projectDir 'Config\SteamVRBindings'
    $manifest = Get-Content -LiteralPath (Require-File (Join-Path $bindings 'steamvr_manifest.json') 'Missing SteamVR action manifest.') -Raw | ConvertFrom-Json
    $touch = Get-Content -LiteralPath (Require-File (Join-Path $bindings 'oculus_touch.json') 'Missing Pimax/oculus_touch bindings.') -Raw | ConvertFrom-Json
    $seatAction = '/actions/main/in/SeatMove_DReyeVR_X,SeatMove_DReyeVR_Y X Y_axis2d'
    if (-not @($manifest.actions | Where-Object { $_.name -eq $seatAction -and $_.type -eq 'vector2' }).Count -or
        -not @($touch.bindings.'/actions/main'.sources | Where-Object { $_.inputs.position.output -eq $seatAction }).Count) {
        throw 'Outdated SteamVR seat bindings. Use the updated manifest and oculus_touch.json.'
    }

    $products = @(Get-ItemProperty @(
        'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*',
        'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*',
        'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*'
    ) -ErrorAction SilentlyContinue)
    $pimaxProducts = @($products | Where-Object { $_.DisplayName -match 'Pimax|PiTool' })
    $roots = @($PimaxRoot) + @($pimaxProducts.InstallLocation) + @(
        "$env:ProgramW6432\Pimax", "$env:ProgramFiles\Pimax", "${env:ProgramFiles(x86)}\Pimax")
    $roots = @($roots | Where-Object { $_ -and [IO.Path]::IsPathRooted($_) } | Select-Object -Unique)
    $clients = @($pimaxProducts | ForEach-Object { if ($_.DisplayIcon) { ($_.DisplayIcon -replace ',\d+$','').Trim('"') } })
    foreach ($root in $roots) { $clients += Join-Path $root 'PimaxClient\pimaxui\PimaxClient.exe' }
    $client = Find-FirstFile $clients
    if (-not $client) { throw 'Install official Pimax Play first (or specify -PimaxRoot). A standalone DLL is not sufficient.' }
    if ($PvrDllPath) {
        if ($PvrDllPath -notmatch '^[A-Za-z]:[\\/]') { throw 'PvrDllPath must be a local absolute path.' }
        $dll = Require-File $PvrDllPath 'Explicit PVR DLL does not exist.'
    } else {
        $dllCandidates = @((Join-Path ([Environment]::SystemDirectory) 'libPVRClient64.dll'))
        foreach ($root in $roots) {
            $dllCandidates += Join-Path $root 'Runtime\libPVRClient64.dll'
            $dllCandidates += Join-Path $root 'libPVRClient64.dll'
        }
        $dll = Find-FirstFile $dllCandidates
        if (-not $dll) { throw 'PVR client library is missing. Repair Pimax Play or pass -PvrDllPath to its official libPVRClient64.dll.' }
    }

    if (-not $SteamVRRoot) {
        $runtimeRoots = @()
        $vrpaths = Join-Path $env:LOCALAPPDATA 'openvr\openvrpaths.vrpath'
        if (Test-Path -LiteralPath $vrpaths) {
            $runtimeRoots += @((Get-Content -LiteralPath $vrpaths -Raw | ConvertFrom-Json).runtime)
        }
        $runtimeRoots += @($products | Where-Object { $_.DisplayName -eq 'SteamVR' } | ForEach-Object { $_.InstallLocation })
        foreach ($runtimeRoot in $runtimeRoots) {
            if ($runtimeRoot -and (Test-Path -LiteralPath (Join-Path $runtimeRoot 'bin\win64\vrmonitor.exe'))) {
                $SteamVRRoot = $runtimeRoot; break
            }
        }
    }
    if (-not $SteamVRRoot) { throw 'Install SteamVR through Steam and run it once, or specify -SteamVRRoot.' }
    $monitor = Require-File (Join-Path $SteamVRRoot 'bin\win64\vrmonitor.exe') 'SteamVR installation is incomplete.'
    $gameArgs += @("${Map}?game=/Script/CarlaUE4.DReyeVRGameMode", '-game', '-vr')
    $gameArgs += "-DReyeVRInput=$InputDevice"
    if ($RecordDriving -or $ScreenshotDriving) {
        if (-not $RecordDirectory) { $RecordDirectory = Join-Path $projectDir 'Saved\DrivingStudy' }
        if ($RecordDirectory -notmatch '^[A-Za-z]:[\\/]' -or $RecordDirectory.Contains('"')) {
            throw 'RecordDirectory must be a local absolute path without quotes.'
        }
        $RecordDirectory = [IO.Path]::GetFullPath($RecordDirectory).TrimEnd('\','/')
        if ($RecordDirectory -match '^[A-Za-z]:$') { $RecordDirectory += '/' }
    }
    if ($RecordDriving) {
        $gameArgs += @('-DReyeVRRecord', "-DReyeVRRecordHz=$RecordHz", ('"-DReyeVRRecordDir=' + $RecordDirectory + '"'))
        Write-Host "Recording: $RecordHz Hz CSV in $RecordDirectory"
    }
    if ($ScreenshotDriving) {
        $gameArgs += @('-DReyeVRScreenshot', "-DReyeVRScreenshotHz=$ScreenshotHz")
        if (-not $RecordDriving) {
            $gameArgs += ('"-DReyeVRRecordDir=' + $RecordDirectory + '"')
        }
        Write-Host "Screenshots: $ScreenshotHz Hz (640x480 JPEG) in $RecordDirectory"
        if ($AsyncScreenshot) {
            $gameArgs += '-DReyeVRScreenshotAsync'
            Write-Host 'Screenshots: async GPU readback + encode (game thread not blocked)'
        }
    }
    if ($EyeThreadHz -gt 0) {
        $gameArgs += "-DReyeVREyeThreadHz=$EyeThreadHz"
        Write-Host "Eye tracking: $EyeThreadHz Hz background sampling (independent of frame rate)"
    }
    $execCmds = @()
    if ($PerfDowngrade) {
        # Measured on this rig: shadows + effects + post-process off raises the recorded stream from
        # ~32 to ~60 rows/s. sg.* set at runtime is also persisted by the engine into
        # Saved/Config/*/Scalability.ini, so the launcher clears that leftover before every launch
        # (see below) instead of letting one session decide the next one's baseline.
        $execCmds += @('sg.ShadowQuality 0', 'sg.EffectsQuality 0', 'sg.PostProcessQuality 0')
        Write-Host 'Render quality: shadows/effects/post-process off (declared, not inherited)'
    }
    if ($PerfCvars) {
        if ($PerfCvars.Contains('"')) { throw 'PerfCvars must not contain quotes.' }
        # Directives starting with ini: target the Engine ini [ConsoleVariables] section, which is
        # applied after URendererSettings imports its own values, so even ECVF_ReadOnly cvars change.
        $directives = @($PerfCvars -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ })
        foreach ($directive in @($directives | Where-Object { $_ -like 'ini:*' })) {
            $gameArgs += ('"-' + $directive + '"')
            Write-Host "Perf ini: $($directive.Substring(4))"
        }
        $execCmds += @($directives | Where-Object { $_ -notlike 'ini:*' })
    }
    if ($ProfileFrames -gt 0) {
        if ($ProfileFrames -lt 200) { throw 'ProfileFrames must be at least 200 captured frames.' }
        # CsvProfile only writes its file on an explicit stop; -csvExecCmds fires a console command
        # once the profiler's own capture frame counter reaches the number.
        $execCmds += 'CsvProfile start'
        $gameArgs += ('"-csvExecCmds=' + $ProfileFrames + ':CsvProfile stop"')
        Write-Host "CSV profiling: capture stops after $ProfileFrames frames"
    }
    if ($execCmds) {
        $gameArgs += ('"-ExecCmds=' + ($execCmds -join ', ') + '"')
        Write-Host "ExecCmds: $($execCmds -join ', ')"
    }
    Write-Host "Pimax Play: $client"
    Write-Host "PVR DLL: $dll (provided by Pimax, not HUTB)"
    Write-Host "SteamVR: $monitor"
    Write-Host "Launch: $exe $($gameArgs -join ' ')"
    Write-Host 'Preflight passed. Enable/calibrate eye tracking in Pimax Play and connect the headset.'
    if ($CheckOnly) { return }

    # -PerfCvars/-PerfDowngrade sg.* values are persisted by the engine into
    # Saved/Config/*/Scalability.ini, and that file is read at the *next* startup, so one
    # session's downgrade silently becomes another's baseline. Clear it only when an explicit
    # performance option is passed so normal launches preserve existing user scalability files.
    if ($PerfDowngrade -or $PerfCvars) {
        foreach ($configDir in @('Windows', 'WindowsNoEditor', 'Win64')) {
            $staleIni = Join-Path $projectDir "Saved\Config\$configDir\Scalability.ini"
            if ((Test-Path -LiteralPath $staleIni) -and (Get-Item -LiteralPath $staleIni).Length -gt 0) {
                $backup = "$staleIni.stale-$(Get-Date -Format 'yyyyMMddHHmmssfff')"
                Move-Item -LiteralPath $staleIni -Destination $backup -Force
                Write-Host "Render quality: cleared inherited scalability override -> $(Split-Path -Leaf $backup)"
            }
        }
    }

    if (-not (Get-Process -Name PimaxClient -ErrorAction SilentlyContinue)) {
        Start-Process -FilePath $client -WorkingDirectory (Split-Path $client) -WindowStyle Hidden | Out-Null
    }
    if (-not (Get-Process -Name vrmonitor -ErrorAction SilentlyContinue)) {
        Start-Process -FilePath $monitor -WorkingDirectory (Split-Path $monitor) -WindowStyle Hidden | Out-Null
    }
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while (-not ((Get-Process -Name vrserver -ErrorAction SilentlyContinue) -and
                 (Get-Process -Name vrcompositor -ErrorAction SilentlyContinue))) {
        if ((Get-Date) -ge $deadline) { throw 'SteamVR did not start in time. Check Pimax connection/SteamVR errors, then retry.' }
        Start-Sleep -Seconds 1
    }
    # Only the launched game's environment changes; no machine-wide settings are written.
    $previousDll = $env:PIMAX_PVR_DLL
    try {
        $env:PIMAX_PVR_DLL = $dll
        Start-Process -FilePath $exe -ArgumentList $gameArgs -WorkingDirectory $projectDir | Out-Null
    } finally { $env:PIMAX_PVR_DLL = $previousDll }
    Write-Host 'Game launched. Check its log for first valid eye sample; running processes alone do not prove tracking works.'
} catch {
    Write-Error $_ -ErrorAction Continue
    exit 1
}
