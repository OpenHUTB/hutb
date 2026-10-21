#requires -Version 5.1
param([switch]$ValidateOnly, [switch]$CheckOnly)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName Microsoft.VisualBasic
[Windows.Forms.Application]::EnableVisualStyles()
$launcher = Join-Path $PSScriptRoot 'StartPimaxVR.ps1'
$packaged = Test-Path -LiteralPath (Join-Path $PSScriptRoot 'CarlaUE4.exe')
$projectDir = if ($packaged) { Join-Path $PSScriptRoot 'CarlaUE4' } else { Join-Path $PSScriptRoot 'Unreal\CarlaUE4' }
$form = New-Object Windows.Forms.Form
$form.Text = 'HUTB 驾驶实验启动器'
$form.ClientSize = New-Object Drawing.Size(660,678)
$form.StartPosition = 'CenterScreen'
$form.FormBorderStyle = 'FixedDialog'
$form.MaximizeBox = $false
$form.Font = New-Object Drawing.Font('Microsoft YaHei UI',9)
function Label($text,$y) {
    $c = New-Object Windows.Forms.Label
    $c.Text=$text; $c.Location=New-Object Drawing.Point(18,$y); $c.Size=New-Object Drawing.Size(120,26)
    $form.Controls.Add($c)
}
function TextBox($text,$y) {
    $c=New-Object Windows.Forms.TextBox
    $c.Text=$text; $c.Location=New-Object Drawing.Point(145,$y); $c.Size=New-Object Drawing.Size(490,27)
    $form.Controls.Add($c); return $c
}
Label '驾驶设备' 22
$device=New-Object Windows.Forms.ComboBox
$device.DropDownStyle='DropDownList'; $device.Location=New-Object Drawing.Point(145,18)
$device.Size=New-Object Drawing.Size(490,27)
$device.Items.AddRange(@('Pimax 手柄','罗技方向盘（G29 / G920 / G923）')); $device.SelectedIndex=0
$form.Controls.Add($device)
# Maps and ego vehicles are discovered from the project content, so the lists stay
# correct for a source tree and for a packaged build without hardcoding either.
function Get-InstalledMaps {
    $root=Join-Path $projectDir 'Content\Carla\Maps'
    $found=@()
    if (Test-Path -LiteralPath $root -PathType Container) {
        foreach ($f in @(Get-ChildItem -LiteralPath $root -Filter 'Town*.umap' -File)) { $found+='/Game/Carla/Maps/'+$f.BaseName }
        foreach ($d in @(Get-ChildItem -LiteralPath $root -Directory -Filter 'Town*')) {
            # Large maps are nested: /Game/Carla/Maps/Town12/Town12
            if (Test-Path -LiteralPath (Join-Path $d.FullName ($d.Name+'.umap')) -PathType Leaf) { $found+='/Game/Carla/Maps/'+$d.Name+'/'+$d.Name }
        }
    }
    return @($found | Sort-Object)
}
function Get-EgoVehicles {
    $dir=Join-Path $projectDir 'Config\EgoVehicles'
    if (-not (Test-Path -LiteralPath $dir -PathType Container)) { return @() }
    return @(Get-ChildItem -LiteralPath $dir -Filter '*.ini' -File | ForEach-Object { $_.BaseName } | Sort-Object)
}
Label '地图' 64
$map=New-Object Windows.Forms.ComboBox
$map.DropDownStyle='DropDown'
$map.Location=New-Object Drawing.Point(145,60); $map.Size=New-Object Drawing.Size(490,27)
foreach ($m in Get-InstalledMaps) { [void]$map.Items.Add($m) }
$defaultMap='/Game/Carla/Maps/Town02'
foreach ($preferred in @('/Game/Carla/Maps/Town10HD_Opt','/Game/Carla/Maps/Town02')) {
    if ($map.Items -contains $preferred) { $defaultMap=$preferred; break }
}
if ($map.Items.Count -and $map.Items -notcontains $defaultMap) { $defaultMap=[string]$map.Items[0] }
$map.Text=$defaultMap
$form.Controls.Add($map)
Label 'UE4 引擎目录' 106
$engine=TextBox $env:UE4_ROOT 102
if (-not $engine.Text -or -not (Test-Path (Join-Path $engine.Text 'Engine\Binaries\Win64\UE4Editor.exe'))) {
    # Fall back to any sibling folder that really is an engine, rather than a fixed folder name:
    # the repo's parent directory differs per machine and per checkout.
    $parent = Split-Path $PSScriptRoot
    foreach ($dir in @(Get-ChildItem -LiteralPath $parent -Directory -ErrorAction SilentlyContinue)) {
        if (Test-Path -LiteralPath (Join-Path $dir.FullName 'Engine\Binaries\Win64\UE4Editor.exe')) {
            $engine.Text = $dir.FullName; break
        }
    }
}
if ($packaged) { $engine.Text='打包版无需填写'; $engine.Enabled=$false }
$record=New-Object Windows.Forms.CheckBox
$record.Text='记录驾驶数据（眼动、操作、头姿、扫视事件、道路及红绿灯）'
$record.Location=New-Object Drawing.Point(18,150); $record.Size=New-Object Drawing.Size(490,28)
$form.Controls.Add($record)
Label '记录频率（Hz）' 193
$rate=New-Object Windows.Forms.NumericUpDown
$rate.Location=New-Object Drawing.Point(145,189); $rate.Minimum=1; $rate.Maximum=500; $rate.Value=120
$form.Controls.Add($rate)
$screenshot=New-Object Windows.Forms.CheckBox
$screenshot.Text='同步记录内屏截图（驾驶员视角）'
$screenshot.Location=New-Object Drawing.Point(18,220); $screenshot.Size=New-Object Drawing.Size(320,28)
$form.Controls.Add($screenshot)
$screenshotRateLabel=New-Object Windows.Forms.Label
$screenshotRateLabel.Text='截图频率（Hz）'; $screenshotRateLabel.Location=New-Object Drawing.Point(345,224); $screenshotRateLabel.Size=New-Object Drawing.Size(120,26)
$form.Controls.Add($screenshotRateLabel)
$screenshotRate=New-Object Windows.Forms.NumericUpDown
$screenshotRate.Location=New-Object Drawing.Point(475,220); $screenshotRate.Minimum=1; $screenshotRate.Maximum=60; $screenshotRate.Value=20
$form.Controls.Add($screenshotRate)
# Optional render downgrade (sg.ShadowQuality/EffectsQuality/PostProcessQuality = 0).
# Unchecked by default so normal launches preserve existing engine/user scalability settings.
$perf=New-Object Windows.Forms.CheckBox
$perf.Text='渲染降档（关阴影/特效/后处理）'
$perf.Location=New-Object Drawing.Point(18,258); $perf.Size=New-Object Drawing.Size(290,28)
$form.Controls.Add($perf)
# Free-form extra cvars, independent of the switch above. 'ini:' routes to [ConsoleVariables]
# (read-only cvars), the rest go to -ExecCmds. Measured 2026-10-11: r.BasePassOutputsVelocity 0 +
# r.CustomDepth 0 gave 30.5 fps vs 32.3 fps without, and forced ~1225 shader recompiles, so
# scenery renders untextured for ~2 minutes. Default empty.
$perfCvars=TextBox '' 254
$perfCvars.Size=New-Object Drawing.Size(310,27); $perfCvars.Location=New-Object Drawing.Point(325,254)
# The tracker samples at 120 Hz while VR renders far slower; this mode moves polling to a
# dedicated thread AND runs the I-VT saccade detector on that 120 Hz stream (thresholds
# calibrated 2026-10-11 on 49k real samples: onset 120 / offset 60 deg/s, min 20 ms).
$eyeHz=New-Object Windows.Forms.CheckBox
$eyeHz.Text='眼动 120 Hz 采样（独立线程；扫视也按 120 Hz 检测）'
$eyeHz.Location=New-Object Drawing.Point(18,296); $eyeHz.Size=New-Object Drawing.Size(490,28)
$form.Controls.Add($eyeHz)
# Async capture keeps the game thread off the GPU readback. Measured 2026-10-11: one
# synchronous 20 Hz capture cost a whole extra presented frame (1554 captures -> 1550 rows
# whose ue_frame jumped by 2). Only meaningful together with the screenshot option above.
$asyncShot=New-Object Windows.Forms.CheckBox
$asyncShot.Text='异步截图（实验）'
$asyncShot.Location=New-Object Drawing.Point(516,296); $asyncShot.Size=New-Object Drawing.Size(132,28)
$form.Controls.Add($asyncShot)
Label '数据根目录' 351
$outputDir=TextBox (Join-Path $projectDir 'Saved\DrivingStudy') 347
Label '被试' 391
$participant=New-Object Windows.Forms.ComboBox
$participant.DropDownStyle='DropDown'
$participant.Location=New-Object Drawing.Point(145,387); $participant.Size=New-Object Drawing.Size(400,27)
$form.Controls.Add($participant)
$newParticipant=New-Object Windows.Forms.Button
$newParticipant.Text='新建被试'; $newParticipant.Location=New-Object Drawing.Point(555,386)
$newParticipant.Size=New-Object Drawing.Size(80,29)
$form.Controls.Add($newParticipant)
$pathLabel=New-Object Windows.Forms.Label
$pathLabel.Location=New-Object Drawing.Point(145,421); $pathLabel.Size=New-Object Drawing.Size(490,22)
$pathLabel.ForeColor=[Drawing.Color]::DarkGreen
$form.Controls.Add($pathLabel)
Label '自车车型' 459
$vehicle=New-Object Windows.Forms.ComboBox
$vehicle.DropDownStyle='DropDownList'
$vehicle.Location=New-Object Drawing.Point(145,455); $vehicle.Size=New-Object Drawing.Size(490,27)
foreach ($v in Get-EgoVehicles) { [void]$vehicle.Items.Add($v) }
$currentVehicle=''
$configPath=Join-Path $projectDir 'Config\DReyeVRConfig.ini'
if (Test-Path -LiteralPath $configPath -PathType Leaf) {
    foreach ($line in @(Get-Content -LiteralPath $configPath)) {
        if ($line -match '^\s*VehicleType\s*=\s*"?([^"#;]+)"?') { $currentVehicle=$Matches[1].Trim(); break }
    }
}
if ($vehicle.Items -contains $currentVehicle) { $vehicle.SelectedIndex=$vehicle.Items.IndexOf($currentVehicle) }
elseif ($vehicle.Items.Count) { $vehicle.SelectedIndex=0 }
$form.Controls.Add($vehicle)
$hint=New-Object Windows.Forms.Label
$hint.Text='先在 Pimax Play 完成眼动校准；方向盘需连接并在 G HUB 中正常识别。启动时把所选车型写入 DReyeVRConfig.ini（引擎无车型命令行开关）。'
$hint.Location=New-Object Drawing.Point(18,490); $hint.Size=New-Object Drawing.Size(620,42)
$form.Controls.Add($hint)
$status=New-Object Windows.Forms.TextBox
$status.Multiline=$true; $status.ReadOnly=$true; $status.ScrollBars='Vertical'
$status.Location=New-Object Drawing.Point(18,576); $status.Size=New-Object Drawing.Size(620,90)
$form.Controls.Add($status)
$check=New-Object Windows.Forms.Button
$check.Text='检查设置'; $check.Location=New-Object Drawing.Point(18,538); $check.Size=New-Object Drawing.Size(140,30)
$start=New-Object Windows.Forms.Button
$start.Text='启动 VR'; $start.Location=New-Object Drawing.Point(172,538); $start.Size=New-Object Drawing.Size(140,30)
$open=New-Object Windows.Forms.Button
$open.Text='打开数据目录'; $open.Location=New-Object Drawing.Point(326,538); $open.Size=New-Object Drawing.Size(160,30)
$form.Controls.AddRange(@($check,$start,$open))
# Each participant gets its own subfolder: <数据根目录>\<被试>.
function Valid-ParticipantName([string]$name) {
    if (-not $name -or $name.Trim() -ne $name) { return $false }
    if ($name.StartsWith('.') -or $name.EndsWith('.') -or $name.Contains('..')) { return $false }
    if ($name -match '[\\/:*?"<>|\x00-\x1F]') { return $false }
    if ($name -match '^(?i)(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\..*)?$') { return $false }
    return $true
}
function Get-RecordDirectory {
    $name=$participant.Text
    if (($record.Checked -or $screenshot.Checked) -and (Valid-ParticipantName $name)) { return Join-Path $outputDir.Text $name }
    return $outputDir.Text
}
function Refresh-Participants {
    $keep=$participant.Text
    $participant.Items.Clear()
    try {
        if (Test-Path -LiteralPath $outputDir.Text -PathType Container) {
            # GetDirectories returns strings, not DirectoryInfo: take the name with GetFileName.
            foreach ($dir in [IO.Directory]::GetDirectories($outputDir.Text) | Sort-Object) {
                [void]$participant.Items.Add([IO.Path]::GetFileName($dir))
            }
        }
    } catch { }
    $participant.Text=$keep
}
function Update-PathLabel {
    if (-not ($record.Checked -or $screenshot.Checked)) { $pathLabel.Text='' }
    elseif (-not (Valid-ParticipantName $participant.Text)) { $pathLabel.Text='请填写合法被试名（不能为 . 或 ..，不能含 \ / : * ? " < > |）' }
    else { $pathLabel.Text='本被试数据目录：'+(Get-RecordDirectory) }
}
$record.Add_CheckedChanged({ Update-PathLabel })
$screenshot.Add_CheckedChanged({ Update-PathLabel })
$participant.Add_TextUpdate({ Update-PathLabel })
$participant.Add_SelectedIndexChanged({ Update-PathLabel })
$outputDir.Add_TextChanged({ Refresh-Participants; Update-PathLabel })
$newParticipant.Add_Click({
    try {
        $name=[Microsoft.VisualBasic.Interaction]::InputBox('被试编号或姓名，将作为数据根目录下的子文件夹：','新建被试','')
        if (-not $name) { return }
        if (-not (Valid-ParticipantName $name)) { $status.Text='被试名不能包含 \ / : * ? " < > | 或首尾空格。'; return }
        [IO.Directory]::CreateDirectory((Join-Path $outputDir.Text $name)) | Out-Null
        Refresh-Participants; $participant.Text=$name; Update-PathLabel
        $status.Text="已创建被试目录：$(Join-Path $outputDir.Text $name)"
    } catch { $status.Text=$_.Exception.Message }
})
Refresh-Participants
$script:child=$null
$timer=New-Object Windows.Forms.Timer
$timer.Interval=250
function Get-LaunchArgs([bool]$OnlyCheck) {
    $values=@('-NoProfile','-ExecutionPolicy','Bypass','-File',$launcher,'-InputDevice',@('controller','wheel')[$device.SelectedIndex],'-Map',$map.Text)
    if ($vehicle.SelectedItem) { $values+=@('-EgoVehicle',[string]$vehicle.SelectedItem) }
    if ($packaged) { $values+=@('-PackageRoot',$PSScriptRoot) } else { $values+=@('-UE4Root',$engine.Text) }
    if ($record.Checked) {
        if (-not (Valid-ParticipantName $participant.Text)) { throw '请先填写或选择被试，数据按被试分目录保存。' }
        $recordDir=Get-RecordDirectory
        if (-not $OnlyCheck) { [IO.Directory]::CreateDirectory($recordDir) | Out-Null }
        $values+=@('-RecordDriving','-RecordHz',[string]$rate.Value,'-RecordDirectory',$recordDir)
    }
    if ($screenshot.Checked) {
        if (-not $record.Checked) {
            if (-not (Valid-ParticipantName $participant.Text)) { throw '请先填写或选择被试，数据按被试分目录保存。' }
            $shotDir=Get-RecordDirectory
            if (-not $OnlyCheck) { [IO.Directory]::CreateDirectory($shotDir) | Out-Null }
            $values+=@('-ScreenshotDriving','-ScreenshotHz',[string]$screenshotRate.Value,'-RecordDirectory',$shotDir)
        } else {
            $values+=@('-ScreenshotDriving','-ScreenshotHz',[string]$screenshotRate.Value)
        }
        if ($asyncShot.Checked) { $values+='-AsyncScreenshot' }
    }
    if ($eyeHz.Checked) {
        $values+=@('-EyeThreadHz','120')
    }
    if ($perf.Checked) { $values+='-PerfDowngrade' }
    $cvars=$perfCvars.Text.Trim()
    if ($cvars) { $values+=@('-PerfCvars',$cvars) }
    if ($OnlyCheck) { $values+='-CheckOnly' }
    return $values
}
function Run-Launcher([bool]$OnlyCheck) {
    if ($script:child) { return }
    try {
        $values=Get-LaunchArgs $OnlyCheck
        # Paths are passed as arguments, never evaluated as PowerShell source.
        foreach ($value in $values) { if ($value.Contains('"') -or $value.Contains("`n")) { throw '路径或地图名称不能包含引号或换行。' } }
        $info=New-Object Diagnostics.ProcessStartInfo
        $info.FileName="$PSHOME\powershell.exe"
        $info.Arguments=($values | ForEach-Object { '"' + ($_ -replace '(\\+)$','$1$1') + '"' }) -join ' '
        $info.UseShellExecute=$false; $info.CreateNoWindow=$true
        $info.RedirectStandardOutput=$true; $info.RedirectStandardError=$true
        $script:child=[Diagnostics.Process]::Start($info)
        # Drain pipes asynchronously so even verbose errors cannot block the child process.
        $script:stdout=$script:child.StandardOutput.ReadToEndAsync()
        $script:stderr=$script:child.StandardError.ReadToEndAsync()
        $check.Enabled=$false; $start.Enabled=$false
        $status.Text='正在检查，请稍候……'; $timer.Start()
    } catch { $status.Text=$_.Exception.Message }
}
$check.Add_Click({ Run-Launcher $true })
$start.Add_Click({ Run-Launcher $false })
$open.Add_Click({
    try {
        $target=if ((Valid-ParticipantName $participant.Text)) { Join-Path $outputDir.Text $participant.Text } else { $outputDir.Text }
        if (Test-Path -LiteralPath $target -PathType Container) { Start-Process explorer.exe -ArgumentList ('"'+$target+'"') }
        elseif (Test-Path -LiteralPath $outputDir.Text -PathType Container) { Start-Process explorer.exe -ArgumentList ('"'+$outputDir.Text+'"') }
        else { $status.Text='目录尚未创建；开始记录后会自动创建。' }
    } catch { $status.Text=$_.Exception.Message }
})
$timer.Add_Tick({
    if ($script:child -and $script:child.HasExited) {
        $timer.Stop()
        $status.Text=$script:stdout.Result + $script:stderr.Result
        if ($script:child.ExitCode -ne 0) { $status.Text="检查或启动失败：`r`n"+$status.Text }
        $script:child.Dispose(); $script:child=$null
        $check.Enabled=$true; $start.Enabled=$true
    }
})
$form.Add_FormClosing({ param($sender,$event)
    if ($script:child) { $event.Cancel=$true; $status.AppendText("`r`n请等待当前启动检查结束后再关闭窗口。") }
})
if ($CheckOnly) {
    Run-Launcher $true
    if (-not $script:child -or -not $script:child.WaitForExit(30000)) { throw 'Preflight did not finish' }
    Write-Output $script:stdout.Result
    if ($script:child.ExitCode -ne 0) { throw $script:stderr.Result }
    $script:child.Dispose(); $script:child=$null
} elseif ($ValidateOnly) {
    if ($form.Controls.Count -lt 25 -or $rate.Value -ne 120 -or $record.Checked -or $screenshot.Checked -or $screenshotRate.Value -ne 20 -or $perf.Checked -or $eyeHz.Checked -or $asyncShot.Checked) { throw 'Invalid UI defaults' }
    if (-not $perfCvars.Enabled -or $perfCvars.Text) { throw 'Extra render arguments must start empty but editable.' }
    if ($participant.DropDownStyle -ne 'DropDown') { throw 'Participant box must allow typing a new name.' }
    if ((Valid-ParticipantName 'S01') -ne $true) { throw 'A legal participant name was rejected.' }
    foreach ($bad in @('','.','..','..a','a..','.hidden','trailing.','CON','NUL','com1.txt','A/B','A\B','A:B','A*B','A"B','A<B','A>B','A|B',' S01 ')) {
        if (Valid-ParticipantName $bad) { throw "An illegal participant name was accepted: [$bad]" }
    }
    $participant.Text='S01'
    if ((Get-RecordDirectory) -ne $outputDir.Text) { throw 'Recording off must not append a participant folder.' }
    $record.Checked=$true
    if ((Get-RecordDirectory) -ne (Join-Path $outputDir.Text 'S01')) { throw 'Participant subfolder is not applied.' }
    $record.Checked=$false
    $screenshot.Checked=$true
    if ((Get-RecordDirectory) -ne (Join-Path $outputDir.Text 'S01')) { throw 'Standalone screenshot must apply participant subfolder.' }
    $screenshot.Checked=$false; $participant.Text=''
    # Existing participants must be listed when the root directory changes.
    $tempBase=Join-Path ([IO.Path]::GetTempPath()) ('hutb-launcher-'+[Guid]::NewGuid().ToString('N').Substring(0,8))
    try {
        $keepBase=$outputDir.Text
        [IO.Directory]::CreateDirectory((Join-Path $tempBase 'S02')) | Out-Null
        $outputDir.Text=$tempBase
        if ($participant.Items -notcontains 'S02') { throw 'Existing participant folders are not listed.' }
        $outputDir.Text=$keepBase
    } finally { if (Test-Path -LiteralPath $tempBase) { [IO.Directory]::Delete($tempBase,$true) } }
    # Map and vehicle choices are discovered from the project, not hardcoded.
    if (@($map.Items | Where-Object { $_ -notmatch '^/Game/[A-Za-z0-9_/]+$' }).Count) { throw 'Map list contains an invalid asset path.' }
    if (-not $map.Text) { throw 'No map selected.' }
    if (Test-Path -LiteralPath (Join-Path $projectDir 'Content\Carla\Maps') -PathType Container) {
        if ($map.Items.Count -lt 1) { throw 'Maps are installed but none are listed.' }
        if ($map.Items -notcontains '/Game/Carla/Maps/Town02') { throw 'Flat map Town02 is missing from the list.' }
        if ($map.Items -notcontains '/Game/Carla/Maps/Town12/Town12') { throw 'Nested large maps are not listed.' }
    }
    if ($vehicle.Items.Count -lt 1) { throw 'No ego vehicle definitions found.' }
    if ($vehicle.Items -notcontains $vehicle.Text) { throw 'Selected vehicle is not one of the definitions.' }
    if ($currentVehicle -and $vehicle.Items -notcontains $currentVehicle) { throw "DReyeVRConfig.ini names an unknown vehicle: $currentVehicle" }
    # The render downgrade is opt-in and must reach the child launcher as -PerfDowngrade. Free-form cvars are independent.
    if (@(Get-LaunchArgs $true) -contains '-PerfDowngrade') { throw 'Render downgrade leaks into the command line while unchecked.' }
    $perf.Checked=$true
    if (@(Get-LaunchArgs $true) -notcontains '-PerfDowngrade') { throw 'The downgrade does not reach the launcher.' }
    $perf.Checked=$false
    if (@(Get-LaunchArgs $true) -contains '-PerfDowngrade') { throw 'Unticking the downgrade must drop -PerfDowngrade.' }
    if (-not $perfCvars.Enabled) { throw 'The free-form render box must stay editable.' }
    $perfCvars.Text='r.CustomDepth 0'
    $perfArgs=Get-LaunchArgs $true
    $perfIndex=[Array]::IndexOf($perfArgs,'-PerfCvars')
    if ($perfIndex -lt 0 -or $perfArgs[$perfIndex+1] -ne $perfCvars.Text) { throw 'Render arguments are not passed to the launcher.' }
    $perfCvars.Text='   '
    if (@(Get-LaunchArgs $true) -contains '-PerfCvars') { throw 'Blank render arguments must be dropped.' }
    $perfCvars.Text=''; $perf.Checked=$false
    # The 120 Hz eye-sampling mode is opt-in and must reach the child launcher verbatim.
    if ((Get-LaunchArgs $true) -contains '-EyeThreadHz') { throw 'Eye 120 Hz mode leaks into the command line while unchecked.' }
    $eyeHz.Checked=$true
    $eyeArgs=Get-LaunchArgs $true
    $eyeIndex=[Array]::IndexOf($eyeArgs,'-EyeThreadHz')
    if ($eyeIndex -lt 0 -or $eyeArgs[$eyeIndex+1] -ne '120') { throw 'Eye 120 Hz mode is not passed to the launcher.' }
    $eyeHz.Checked=$false
    # Async screenshots are opt-in, ride along with the screenshot option, and must reach the launcher.
    $participant.Text='S01'
    if ((Get-LaunchArgs $true) -contains '-AsyncScreenshot') { throw 'Async screenshots leak into the command line while unchecked.' }
    $screenshot.Checked=$true
    $shotOnlyArgs=Get-LaunchArgs $true
    $shotDirIndex=[Array]::IndexOf($shotOnlyArgs,'-RecordDirectory')
    if ($shotDirIndex -lt 0 -or $shotOnlyArgs[$shotDirIndex+1] -ne (Join-Path $outputDir.Text 'S01')) { throw 'Standalone screenshot does not pass participant directory.' }
    if ($shotOnlyArgs -contains '-AsyncScreenshot') { throw 'Async screenshots leak while their own box is unchecked.' }
    $asyncShot.Checked=$true
    if ((Get-LaunchArgs $true) -notcontains '-AsyncScreenshot') { throw 'Async screenshots are not passed to the launcher.' }
    $asyncShot.Checked=$false; $screenshot.Checked=$false; $participant.Text=''
    # Layout guard: a screenshot cannot be taken headless, so assert the geometry instead.
    $area=New-Object Drawing.Rectangle(0,0,$form.ClientSize.Width,$form.ClientSize.Height)
    $boxes=@($form.Controls | ForEach-Object { New-Object Drawing.Rectangle($_.Location,$_.Size) })
    $names=@($form.Controls | ForEach-Object { $type=$_.GetType().Name; if ($_.Text) { "$type '$($_.Text)'" } else { $type } })
    for ($i=0; $i -lt $boxes.Count; $i++) {
        if (-not $area.Contains($boxes[$i])) { throw "Control $($names[$i]) is outside the window." }
        for ($j=$i+1; $j -lt $boxes.Count; $j++) {
            if ($boxes[$i].IntersectsWith($boxes[$j])) { throw "Controls overlap: $($names[$i]) and $($names[$j])." }
        }
    }
    Write-Output 'PASS: launcher UI constructs; recording defaults OFF; 120 Hz; screenshots OFF; 20 Hz; render downgrade OFF; extra render args empty; eye 120 Hz thread OFF; async screenshots OFF; controller default; per-participant subfolder; map+vehicle discovery; layout clean.'
} else { [void]$form.ShowDialog() }
$timer.Dispose(); $form.Dispose()
