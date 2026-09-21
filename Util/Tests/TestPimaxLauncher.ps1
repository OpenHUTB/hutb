#requires -Version 5.1
# Isolated fixtures and mocked process calls: never starts Pimax, SteamVR or the game.
param([string]$Case)
$ErrorActionPreference = 'Stop'
$launcher = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\StartPimaxVR.ps1'))
if (-not $Case) {
    $cases = @{
        preflight = 'Preflight passed'
        launch = 'PASS: mocked launch order and arguments'
        relativeDll = 'PvrDllPath must be a local absolute path'
        missingDll = 'Explicit PVR DLL does not exist'
        disabledPlugin = 'SteamVR is disabled'
        disabledEye = 'Set Pimax.EnablePvrEyeTracking=True'
        missingBinding = 'Outdated SteamVR seat bindings'
    }
    foreach ($name in ($cases.Keys | Sort-Object)) {
        # Windows PowerShell wraps native stderr as ErrorRecord; negative cases are intentional.
        $ErrorActionPreference = 'Continue'
        $output = & "$PSHOME\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath -Case $name 2>&1
        $code = $LASTEXITCODE
        $ErrorActionPreference = 'Stop'
        $expectedCode = if ($name -in @('preflight','launch')) { 0 } else { 1 }
        if ($code -ne $expectedCode -or ($output -join "`n") -notlike ('*' + $cases[$name] + '*')) {
            throw "FAIL: $name (exit $code). $output"
        }
        Write-Host "PASS: $name"
    }
    return
}

$fixture = Join-Path ([IO.Path]::GetTempPath()) ('HUTB Pimax launcher test ' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $fixture | Out-Null
try {
    foreach ($dir in @('CarlaUE4\Config\SteamVRBindings','Pimax\PimaxClient\pimaxui','SteamVR\bin\win64')) {
        New-Item -ItemType Directory -Path (Join-Path $fixture $dir) -Force | Out-Null
    }
    foreach ($file in @('CarlaUE4.exe','libPVRClient64.dll','Pimax\PimaxClient\pimaxui\PimaxClient.exe','SteamVR\bin\win64\vrmonitor.exe')) {
        New-Item -ItemType File -Path (Join-Path $fixture $file) | Out-Null
    }
    $enabled = if ($Case -eq 'disabledPlugin') { 'false' } else { 'true' }
    Set-Content (Join-Path $fixture 'CarlaUE4\CarlaUE4.uproject') ('{"Plugins":[{"Name":"SteamVR","Enabled":' + $enabled + '}]}')
    $eye = if ($Case -eq 'disabledEye') { 'False' } else { 'True' }
    Set-Content (Join-Path $fixture 'CarlaUE4\Config\DReyeVRConfig.ini') @"
[Pimax]
EnablePvrEyeTracking=$eye
AllowUnknownPimaxDevice=False
AllowedVendorIds=0x34A4
AllowedProductIds=0x0044
[EgoSensor]
DrawDebugFocusTrace=True
UseCameraRelativeGazeDisplay=True
"@
    $action = '/actions/main/in/SeatMove_DReyeVR_X,SeatMove_DReyeVR_Y X Y_axis2d'
    $bindingAction = if ($Case -eq 'missingBinding') { 'outdated' } else { $action }
    Set-Content (Join-Path $fixture 'CarlaUE4\Config\SteamVRBindings\steamvr_manifest.json') ('{"actions":[{"name":"' + $action + '","type":"vector2"}]}')
    Set-Content (Join-Path $fixture 'CarlaUE4\Config\SteamVRBindings\oculus_touch.json') ('{"bindings":{"/actions/main":{"sources":[{"inputs":{"position":{"output":"' + $bindingAction + '"}}}]}}}')
    $dll = Join-Path $fixture 'libPVRClient64.dll'
    if ($Case -eq 'relativeDll') { $dll = 'relative.dll' }
    if ($Case -eq 'missingDll') { $dll = Join-Path $fixture 'missing.dll' }
    $global:pimaxTestCalls = [Collections.Generic.List[string]]::new()
    function Get-ItemProperty { param($Path,$ErrorAction) return @() }
    function Get-Process {
        param($Name,$ErrorAction)
        if ($Name -in @('vrserver','vrcompositor') -and $global:pimaxTestCalls.Contains('vrmonitor.exe')) { return $true }
    }
    function Start-Process {
        param($FilePath,$ArgumentList,$WorkingDirectory,$WindowStyle)
        $file = Split-Path $FilePath -Leaf
        $global:pimaxTestCalls.Add($file)
        if ($file -eq 'CarlaUE4.exe') {
            if ($env:PIMAX_PVR_DLL -ne $dll -or
                $ArgumentList -notcontains '-vr' -or
                $ArgumentList -notcontains '/Game/Carla/Maps/Town02?game=/Script/CarlaUE4.DReyeVRGameMode') {
                throw 'Incorrect game environment or VR arguments'
            }
        }
    }
    $oldDll = $env:PIMAX_PVR_DLL
    $global:LASTEXITCODE = 0
    & $launcher -PackageRoot $fixture -PimaxRoot (Join-Path $fixture 'Pimax') -PvrDllPath $dll -SteamVRRoot (Join-Path $fixture 'SteamVR') -CheckOnly:($Case -ne 'launch')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    if ($Case -eq 'launch') {
        if (($global:pimaxTestCalls -join ',') -ne 'PimaxClient.exe,vrmonitor.exe,CarlaUE4.exe' -or $env:PIMAX_PVR_DLL -ne $oldDll) {
            throw 'Incorrect launch order or environment was not restored'
        }
        Write-Host 'PASS: mocked launch order and arguments'
    } elseif ($global:pimaxTestCalls.Count -ne 0) { throw 'CheckOnly must not launch processes' }
} finally {
    # Only remove the unique directory created by this test, never a user-supplied path.
    $resolved = [IO.Path]::GetFullPath($fixture)
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if ($resolved.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase) -and
        (Split-Path $resolved -Leaf) -like 'HUTB Pimax launcher test *') {
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
