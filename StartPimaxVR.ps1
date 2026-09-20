#requires -Version 5.1
<#
Launch a built Windows DReyeVR project or package after checking its VR prerequisites.
Example: .\StartPimaxVR.ps1 -UE4Root 'D:\UE4-hutb' -CheckOnly
Example: .\StartPimaxVR.ps1 -PackageRoot 'D:\HUTB'
#>
[CmdletBinding()]
param(
    [string]$UE4Root = $env:UE4_ROOT,
    [string]$PackageRoot,
    [string]$Map = '/Game/Carla/Maps/Town02',
    [string]$PimaxRoot,
    [string]$PvrDllPath = $env:PIMAX_PVR_DLL,
    [string]$SteamVRRoot,
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
    Write-Host "Pimax Play: $client"
    Write-Host "PVR DLL: $dll (provided by Pimax, not HUTB)"
    Write-Host "SteamVR: $monitor"
    Write-Host "Launch: $exe $($gameArgs -join ' ')"
    Write-Host 'Preflight passed. Enable/calibrate eye tracking in Pimax Play and connect the headset.'
    if ($CheckOnly) { return }

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
