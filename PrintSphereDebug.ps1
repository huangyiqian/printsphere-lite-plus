# PrintSphere Lite 调试工具
# 使用方法: powershell -ExecutionPolicy Bypass -File 本文件.ps1

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# ===== 配置 =====
$baseUrl = "http://192.168.31.113:8081"
$script:currentBrightness = 25
$script:currentLayout = "dashboard"

# ===== 辅助函数 =====
function Invoke-ApiGet($path) {
    try {
        $resp = Invoke-RestMethod -Uri "$baseUrl$path" -TimeoutSec 5
        return $resp
    } catch {
        return $null
    }
}

function Invoke-ApiPost($path, $body) {
    try {
        $resp = Invoke-RestMethod -Uri "$baseUrl$path" -Method Post `
            -ContentType "application/json" -Body $body -TimeoutSec 5
        return $resp
    } catch {
        return $null
    }
}

function Update-Status {
    $status = Invoke-ApiGet("/api/status")
    if ($status) {
        $txtStatus.Text = ($status | ConvertTo-Json -Depth 5)
        # 解析字段
        if ($status.printer -and $status.printer.print) {
            $p = $status.printer.print
            $sbStatus.Text = "状态: $($p.gcode_state) | 进度: $($p.mc_percent)% | 层: $($p.current_layer)/$($p.total_layers)"
            $sbStatus.Text += " | 喷嘴: $($p.nozzle_temper)°C | 热床: $($p.bed_temper)°C"
            if ($p.mc_remaining_time) {
                $h = [Math]::Floor($p.mc_remaining_time / 3600)
                $m = [Math]::Floor(($p.mc_remaining_time % 3600) / 60)
                $sbStatus.Text += " | 剩余: ${h}h${m}m"
            }
        } else {
            $sbStatus.Text = "状态: 空闲 | 打印机: $($status.printer_name)"
        }
    } else {
        $txtStatus.Text = "无法连接 ESP"
        $sbStatus.Text = "无法连接 $baseUrl"
    }
}

# ===== 创建主窗口 =====
$form = New-Object System.Windows.Forms.Form
$form.Text = "PrintSphere Lite 调试工具"
$form.Size = New-Object System.Drawing.Size(720, 600)
$form.StartPosition = "CenterScreen"
$form.Font = New-Object System.Drawing.Font("Microsoft YaHei", 9)

# 状态栏
$statusStrip = New-Object System.Windows.Forms.StatusStrip
$sbStatus = New-Object System.Windows.Forms.ToolStripStatusLabel
$sbStatus.Text = "就绪"
$sbStatus.AutoSize = $true
$statusStrip.Items.Add($sbStatus) | Out-Null
$form.Controls.Add($statusStrip)

# ===== Tab 页面 =====
$tabControl = New-Object System.Windows.Forms.TabControl
$tabControl.Dock = "Fill"
$form.Controls.Add($tabControl)

# ---- Tab 1: 状态查看 ----
$tab1 = New-Object System.Windows.Forms.TabPage
$tab1.Text = "状态查看"
$tabControl.TabPages.Add($tab1)

$btnRefresh = New-Object System.Windows.Forms.Button
$btnRefresh.Text = "刷新状态"
$btnRefresh.Location = New-Object System.Drawing.Point(12, 12)
$btnRefresh.Size = New-Object System.Drawing.Size(100, 30)
$btnRefresh.Add_Click({ Update-Status })
$tab1.Controls.Add($btnRefresh)

$btnRefreshDisplay = New-Object System.Windows.Forms.Button
$btnRefreshDisplay.Text = "触发屏幕刷新"
$btnRefreshDisplay.Location = New-Object System.Drawing.Point(120, 12)
$btnRefreshDisplay.Size = New-Object System.Drawing.Size(120, 30)
$btnRefreshDisplay.Add_Click({
    $r = Invoke-ApiGet("/api/refresh")
    if ($r -and $r.ok) { $sbStatus.Text = "屏幕已刷新" } else { $sbStatus.Text = "刷新失败" }
})
$tab1.Controls.Add($btnRefreshDisplay)

$txtStatus = New-Object System.Windows.Forms.TextBox
$txtStatus.Multiline = $true
$txtStatus.ScrollBars = "Both"
$txtStatus.Font = New-Object System.Drawing.Font("Consolas", 9)
$txtStatus.Location = New-Object System.Drawing.Point(12, 50)
$txtStatus.Size = New-Object System.Drawing.Size(680, 460)
$txtStatus.ReadOnly = $true
$tab1.Controls.Add($txtStatus)

# ---- Tab 2: 布局切换 ----
$tab2 = New-Object System.Windows.Forms.TabPage
$tab2.Text = "布局切换"
$tabControl.TabPages.Add($tab2)

$lblLayout = New-Object System.Windows.Forms.Label
$lblLayout.Text = "选择屏幕布局："
$lblLayout.Location = New-Object System.Drawing.Point(20, 20)
$lblLayout.Size = New-Object System.Drawing.Size(150, 24)
$tab2.Controls.Add($lblLayout)

$layouts = @("classic", "dashboard", "clock")
$layoutNames = @{classic="经典样式"; dashboard="仪表盘"; clock="时钟"}

$ypos = 55
foreach ($lay in $layouts) {
    $btn = New-Object System.Windows.Forms.Button
    $btn.Text = "$($layoutNames[$lay]) ($lay)"
    $btn.Location = New-Object System.Drawing.Point(30, $ypos)
    $btn.Size = New-Object System.Drawing.Size(240, 40)
    $btn.Tag = $lay
    $btn.Add_Click({
        $lay = $this.Tag
        $body = "{`"layout`":`"$lay`"}"
        $r = Invoke-ApiPost("/api/config", $body)
        if ($r -and $r.ok) {
            $script:currentLayout = $lay
            $sbStatus.Text = "布局已切换为: $($layoutNames[$lay])"
        } else {
            $sbStatus.Text = "切换布局失败"
        }
    })
    $tab2.Controls.Add($btn)
    $ypos += 50
}

$lblCurrentLayout = New-Object System.Windows.Forms.Label
$lblCurrentLayout.Text = "当前布局: dashboard"
$lblCurrentLayout.Location = New-Object System.Drawing.Point(30, $ypos + 10)
$lblCurrentLayout.Size = New-Object System.Drawing.Size(300, 24)
$tab2.Controls.Add($lblCurrentLayout)

# ---- Tab 3: 亮度调节 ----
$tab3 = New-Object System.Windows.Forms.TabPage
$tab3.Text = "亮度调节"
$tabControl.TabPages.Add($tab3)

$lblBrightness = New-Object System.Windows.Forms.Label
$lblBrightness.Text = "屏幕亮度:"
$lblBrightness.Location = New-Object System.Drawing.Point(20, 25)
$lblBrightness.Size = New-Object System.Drawing.Size(80, 24)
$tab3.Controls.Add($lblBrightness)

$txtBright = New-Object System.Windows.Forms.TextBox
$txtBright.Text = "25"
$txtBright.Location = New-Object System.Drawing.Point(100, 22)
$txtBright.Size = New-Object System.Drawing.Size(50, 24)
$tab3.Controls.Add($txtBright)

$trackBright = New-Object System.Windows.Forms.TrackBar
$trackBright.Location = New-Object System.Drawing.Point(160, 20)
$trackBright.Size = New-Object System.Drawing.Size(300, 45)
$trackBright.Minimum = 0
$trackBright.Maximum = 100
$trackBright.Value = 25
$trackBright.TickFrequency = 10
$trackBright.Add_Scroll({
    $txtBright.Text = $trackBright.Value
})
$tab3.Controls.Add($trackBright)

$btnSetBrightness = New-Object System.Windows.Forms.Button
$btnSetBrightness.Text = "设置亮度"
$btnSetBrightness.Location = New-Object System.Drawing.Point(20, 70)
$btnSetBrightness.Size = New-Object System.Drawing.Size(100, 30)
$btnSetBrightness.Add_Click({
    $val = [int]$txtBright.Text
    if ($val -lt 0) { $val = 0 }
    if ($val -gt 100) { $val = 100 }
    $body = "{`"brightness`":$val}"
    $r = Invoke-ApiPost("/api/config", $body)
    if ($r -and $r.ok) {
        $script:currentBrightness = $val
        $sbStatus.Text = "亮度已设为: $val"
    } else {
        $sbStatus.Text = "设置亮度失败"
    }
})
$tab3.Controls.Add($btnSetBrightness)

# ---- Tab 4: 亮度定时 ----
$tab4 = New-Object System.Windows.Forms.TabPage
$tab4.Text = "亮度定时"
$tabControl.TabPages.Add($tab4)

$lblScheduleInfo = New-Object System.Windows.Forms.Label
$lblScheduleInfo.Text = "定时切换亮度（两个时段）:"
$lblScheduleInfo.Location = New-Object System.Drawing.Point(20, 15)
$lblScheduleInfo.Size = New-Object System.Drawing.Size(300, 24)
$tab4.Controls.Add($lblScheduleInfo)

$lblNote = New-Object System.Windows.Forms.Label
$lblNote.Text = "未勾选 = 禁用定时"
$lblNote.Location = New-Object System.Drawing.Point(20, 42)
$lblNote.Size = New-Object System.Drawing.Size(300, 20)
$lblNote.ForeColor = "Gray"
$tab4.Controls.Add($lblNote)

$chkEnable = New-Object System.Windows.Forms.CheckBox
$chkEnable.Text = "启用亮度定时"
$chkEnable.Location = New-Object System.Drawing.Point(20, 68)
$chkEnable.Size = New-Object System.Drawing.Size(120, 24)
$tab4.Controls.Add($chkEnable)

$group1 = New-Object System.Windows.Forms.GroupBox
$group1.Text = "时段 1"
$group1.Location = New-Object System.Drawing.Point(20, 100)
$group1.Size = New-Object System.Drawing.Size(310, 90)
$tab4.Controls.Add($group1)

$lblS1 = New-Object System.Windows.Forms.Label
$lblS1.Text = "开始:"
$lblS1.Location = New-Object System.Drawing.Point(10, 25)
$lblS1.Size = New-Object System.Drawing.Size(40, 24)
$group1.Controls.Add($lblS1)

$dtS1S = New-Object System.Windows.Forms.DateTimePicker
$dtS1S.Format = "Time"
$dtS1S.ShowUpDown = $true
$dtS1S.Value = [DateTime]"08:00"
$dtS1S.Location = New-Object System.Drawing.Point(55, 22)
$dtS1S.Size = New-Object System.Drawing.Size(90, 24)
$group1.Controls.Add($dtS1S)

$lblE1 = New-Object System.Windows.Forms.Label
$lblE1.Text = "结束:"
$lblE1.Location = New-Object System.Drawing.Point(155, 25)
$lblE1.Size = New-Object System.Drawing.Size(40, 24)
$group1.Controls.Add($lblE1)

$dtS1E = New-Object System.Windows.Forms.DateTimePicker
$dtS1E.Format = "Time"
$dtS1E.ShowUpDown = $true
$dtS1E.Value = [DateTime]"22:00"
$dtS1E.Location = New-Object System.Drawing.Point(200, 22)
$dtS1E.Size = New-Object System.Drawing.Size(90, 24)
$group1.Controls.Add($dtS1E)

$lblB1 = New-Object System.Windows.Forms.Label
$lblB1.Text = "亮度:"
$lblB1.Location = New-Object System.Drawing.Point(10, 55)
$lblB1.Size = New-Object System.Drawing.Size(40, 24)
$group1.Controls.Add($lblB1)

$trackB1 = New-Object System.Windows.Forms.TrackBar
$trackB1.Location = New-Object System.Drawing.Point(55, 52)
$trackB1.Size = New-Object System.Drawing.Size(180, 30)
$trackB1.Minimum = 0
$trackB1.Maximum = 100
$trackB1.Value = 100
$trackB1.TickFrequency = 20
$group1.Controls.Add($trackB1)

$lblB1Val = New-Object System.Windows.Forms.Label
$lblB1Val.Text = "100"
$lblB1Val.Location = New-Object System.Drawing.Point(240, 55)
$lblB1Val.Size = New-Object System.Drawing.Size(40, 24)
$group1.Controls.Add($lblB1Val)

$trackB1.Add_Scroll({ $lblB1Val.Text = $trackB1.Value })

$group2 = New-Object System.Windows.Forms.GroupBox
$group2.Text = "时段 2"
$group2.Location = New-Object System.Drawing.Point(20, 200)
$group2.Size = New-Object System.Drawing.Size(310, 90)
$tab4.Controls.Add($group2)

$lblS2 = New-Object System.Windows.Forms.Label
$lblS2.Text = "开始:"
$lblS2.Location = New-Object System.Drawing.Point(10, 25)
$lblS2.Size = New-Object System.Drawing.Size(40, 24)
$group2.Controls.Add($lblS2)

$dtS2S = New-Object System.Windows.Forms.DateTimePicker
$dtS2S.Format = "Time"
$dtS2S.ShowUpDown = $true
$dtS2S.Value = [DateTime]"06:00"
$dtS2S.Location = New-Object System.Drawing.Point(55, 22)
$dtS2S.Size = New-Object System.Drawing.Size(90, 24)
$group2.Controls.Add($dtS2S)

$lblE2 = New-Object System.Windows.Forms.Label
$lblE2.Text = "结束:"
$lblE2.Location = New-Object System.Drawing.Point(155, 25)
$lblE2.Size = New-Object System.Drawing.Size(40, 24)
$group2.Controls.Add($lblE2)

$dtS2E = New-Object System.Windows.Forms.DateTimePicker
$dtS2E.Format = "Time"
$dtS2E.ShowUpDown = $true
$dtS2E.Value = [DateTime]"22:00"
$dtS2E.Location = New-Object System.Drawing.Point(200, 22)
$dtS2E.Size = New-Object System.Drawing.Size(90, 24)
$group2.Controls.Add($dtS2E)

$lblB2 = New-Object System.Windows.Forms.Label
$lblB2.Text = "亮度:"
$lblB2.Location = New-Object System.Drawing.Point(10, 55)
$lblB2.Size = New-Object System.Drawing.Size(40, 24)
$group2.Controls.Add($lblB2)

$trackB2 = New-Object System.Windows.Forms.TrackBar
$trackB2.Location = New-Object System.Drawing.Point(55, 52)
$trackB2.Size = New-Object System.Drawing.Size(180, 30)
$trackB2.Minimum = 0
$trackB2.Maximum = 100
$trackB2.Value = 30
$trackB2.TickFrequency = 20
$group2.Controls.Add($trackB2)

$lblB2Val = New-Object System.Windows.Forms.Label
$lblB2Val.Text = "30"
$lblB2Val.Location = New-Object System.Drawing.Point(240, 55)
$lblB2Val.Size = New-Object System.Drawing.Size(40, 24)
$group2.Controls.Add($lblB2Val)

$trackB2.Add_Scroll({ $lblB2Val.Text = $trackB2.Value })

$btnSetSchedule = New-Object System.Windows.Forms.Button
$btnSetSchedule.Text = "保存定时设置"
$btnSetSchedule.Location = New-Object System.Drawing.Point(20, 300)
$btnSetSchedule.Size = New-Object System.Drawing.Size(140, 35)
$btnSetSchedule.Add_Click({
    if (-not $chkEnable.Checked) {
        $body = "{`"brightness_schedule`":`"`"}"
        $r = Invoke-ApiPost("/api/config", $body)
        if ($r -and $r.ok) { $sbStatus.Text = "定时已禁用" }
        else { $sbStatus.Text = "设置失败" }
        return
    }
    $slots = @()
    # 时段1
    $s1h = $dtS1S.Value.Hour; $s1m = $dtS1S.Value.Minute
    $e1h = $dtS1E.Value.Hour; $e1m = $dtS1E.Value.Minute
    $b1 = $trackB1.Value
    $slots += @{sh=$s1h; sm=$s1m; eh=$e1h; em=$e1m; b=$b1}
    # 时段2
    $s2h = $dtS2S.Value.Hour; $s2m = $dtS2S.Value.Minute
    $e2h = $dtS2E.Value.Hour; $e2m = $dtS2E.Value.Minute
    $b2 = $trackB2.Value
    $slots += @{sh=$s2h; sm=$s2m; eh=$e2h; em=$e2m; b=$b2}
    $body = "{`"brightness_schedule`":$($slots | ConvertTo-Json -Compress)}"
    $r = Invoke-ApiPost("/api/config", $body)
    if ($r -and $r.ok) { $sbStatus.Text = "亮度定时已保存" }
    else { $sbStatus.Text = "保存定时失败" }
})
$tab4.Controls.Add($btnSetSchedule)

# ---- Tab 5: 关于 ----
$tab5 = New-Object System.Windows.Forms.TabPage
$tab5.Text = "关于"
$tabControl.TabPages.Add($tab5)

$lblAbout = New-Object System.Windows.Forms.Label
$lblAbout.Text = "PrintSphere Lite 调试工具`n`n基于 ESP Web API 开发`n`nAPI 端点: http://192.168.31.113:8081`n`n支持的操作:`n- 查看实时打印机状态`n- 切换屏幕布局(经典/仪表盘/时钟)`n- 调节屏幕亮度`n- 配置亮度定时任务`n- 触发屏幕刷新"
$lblAbout.Location = New-Object System.Drawing.Point(20, 20)
$lblAbout.Size = New-Object System.Drawing.Size(550, 300)
$lblAbout.Font = New-Object System.Drawing.Font("Microsoft YaHei", 10)
$tab5.Controls.Add($lblAbout)

# ===== 初始化 =====
$form.Add_Shown({
    $form.Activate()
    Update-Status
})

# ===== 显示窗口 =====
$form.ShowDialog() | Out-Null
