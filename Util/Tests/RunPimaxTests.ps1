#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$UE4Root = $env:UE4_ROOT,
    [string]$ReportPath
)
$ErrorActionPreference = 'Stop'
try {
    $repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
    if (-not $UE4Root) { throw 'Specify -UE4Root or set UE4_ROOT.' }
    $editor = Join-Path $UE4Root 'Engine\Binaries\Win64\UE4Editor-Cmd.exe'
    $project = Join-Path $repo 'Unreal\CarlaUE4\CarlaUE4.uproject'
    if (-not (Test-Path -LiteralPath $editor)) { throw "Missing $editor; build the Windows editor first." }
    if (-not $ReportPath) {
        $ReportPath = Join-Path $repo ('Unreal\CarlaUE4\Saved\Automation\Pimax-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    }
    $ReportPath = [IO.Path]::GetFullPath($ReportPath)
    # Refuse an old report directory: a failed run must never be mistaken for a previous success.
    if (Test-Path -LiteralPath $ReportPath) { throw 'ReportPath already exists. Choose a new directory for this run.' }
    New-Item -ItemType Directory -Path $ReportPath | Out-Null
    $log = Join-Path $ReportPath 'automation.log'
    $arguments = @('"' + $project + '"', '/Engine/Maps/Entry', '-unattended', '-NullRHI', '-nosound',
        '-nop4', '-nosplash', '-nohmd', '"-ExecCmds=Automation RunTests HUTB.Pimax"',
        '"-TestExit=Automation Test Queue Empty"', '"-ReportExportPath=' + $ReportPath + '"', '"-abslog=' + $log + '"')
    $process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit(600000)) {
        $process.Kill()
        throw "Automation timed out after 10 minutes. See $log"
    }
    if ($process.ExitCode -ne 0) { throw "Editor exited with code $($process.ExitCode). See $log" }
    $report = Get-Content -LiteralPath (Join-Path $ReportPath 'index.json') -Raw | ConvertFrom-Json
    $expected = @('HUTB.Pimax.EyeTracking.Freshness', 'HUTB.Pimax.GazeDisplay.CameraAttachment',
        'HUTB.Pimax.GazeDisplay.FixedAnchor', 'HUTB.Pimax.SeatInput.DeadZone', 'HUTB.Pimax.Runtime.DiscoveryPaths')
    foreach ($name in $expected) {
        if (-not @($report.tests | Where-Object { $_.fullTestPath -eq $name -and $_.state -eq 'Success' }).Count) {
            throw "Missing or failed test: $name. See $ReportPath"
        }
    }
    if ($report.failed -ne 0 -or $report.notRun -ne 0) { throw "Incomplete or failed tests. See $ReportPath" }
    Write-Host "PASS: $($report.succeeded) tests. Report: $ReportPath"
} catch {
    Write-Error $_ -ErrorAction Continue
    exit 1
}
