$ErrorActionPreference = "Stop"

try {
  [Console]::OutputEncoding = New-Object System.Text.UTF8Encoding($false)
  $OutputEncoding = New-Object System.Text.UTF8Encoding($false)
} catch {}

function Write-Title($text) {
  Write-Host ""
  Write-Host "==== $text ====" -ForegroundColor Cyan
}

function Pause-Exit($code) {
  Write-Host ""
  Read-Host "按回车键退出"
  exit $code
}

function Join-CnPath($base, $codePoints) {
  $name = -join ($codePoints | ForEach-Object { [char]$_ })
  return Join-Path $base $name
}

function Get-SerialPorts {
  $ports = @()
  try {
    $ports = Get-CimInstance Win32_SerialPort |
      Sort-Object DeviceID |
      ForEach-Object {
        [pscustomobject]@{
          Port = $_.DeviceID
          Name = if ($_.Description) { $_.Description } else { $_.Name }
        }
      }
  } catch {
    $ports = @()
  }

  if (-not $ports -or $ports.Count -eq 0) {
    try {
      $modeText = cmd.exe /c mode
      $ports = [regex]::Matches($modeText, "COM\d+") |
        ForEach-Object { $_.Value.ToUpperInvariant() } |
        Sort-Object -Unique |
        ForEach-Object { [pscustomobject]@{ Port = $_; Name = "串口设备" } }
    } catch {
      $ports = @()
    }
  }

  return @($ports)
}

function Select-Port {
  $ports = Get-SerialPorts
  if ($ports.Count -eq 0) {
    Write-Host "没有检测到串口。" -ForegroundColor Yellow
    Write-Host "请先安装 CH340/CH341 驱动，或重新插拔 ESP 后再运行本工具。"
    return ""
  }

  Write-Host "检测到以下串口："
  for ($i = 0; $i -lt $ports.Count; $i++) {
    $index = $i + 1
    Write-Host ("  {0}. {1}  {2}" -f $index, $ports[$i].Port, $ports[$i].Name)
  }

  while ($true) {
    $answer = Read-Host "请输入串口编号，直接回车默认选择 1"
    if ([string]::IsNullOrWhiteSpace($answer)) { return $ports[0].Port }
    $n = 0
    if ([int]::TryParse($answer, [ref]$n) -and $n -ge 1 -and $n -le $ports.Count) {
      return $ports[$n - 1].Port
    }
    Write-Host "输入不正确，请重新输入。" -ForegroundColor Yellow
  }
}

function Select-Firmware($defaultFirmware, $firmwareDir) {
  if (Test-Path -LiteralPath $defaultFirmware) {
    Write-Host "默认固件：$defaultFirmware"
    $useDefault = Read-Host "直接使用默认固件请回车；输入 C 可手动选择其他 .bin 固件"
    if ($useDefault -notmatch "^[cC]$") {
      return (Resolve-Path -LiteralPath $defaultFirmware).Path
    }
  }

  Add-Type -AssemblyName System.Windows.Forms
  $dialog = New-Object System.Windows.Forms.OpenFileDialog
  $dialog.Title = "请选择要刷入的 ESP8266 固件"
  $dialog.Filter = "固件文件 (*.bin)|*.bin|所有文件 (*.*)|*.*"
  $dialog.Multiselect = $false
  $dialog.CheckFileExists = $true

  if (Test-Path -LiteralPath $firmwareDir) {
    $dialog.InitialDirectory = (Resolve-Path -LiteralPath $firmwareDir).Path
  } else {
    $dialog.InitialDirectory = $PSScriptRoot
  }

  $result = $dialog.ShowDialog()
  if ($result -ne [System.Windows.Forms.DialogResult]::OK) {
    return ""
  }
  return $dialog.FileName
}

Write-Title "PrintSphere Lite 一键刷固件工具"
Write-Host "适用硬件：ESP8266EX / NodeMCU / CH340G 串口模块"
Write-Host "烧录过程会清空 ESP 中保存的 WiFi、Bambu token 和打印机选择。"

$tool = Join-Path $PSScriptRoot "tools\esptool.exe"
$driverDir = Join-CnPath $PSScriptRoot @(0x9A71, 0x52A8)
$driver = Join-Path $driverDir "CH341SER.EXE"
$firmwareDir = Join-CnPath (Join-Path $PSScriptRoot "..") @(0x56FA, 0x4EF6)
$defaultFirmware = Join-Path $firmwareDir "printsphere-lite-esp8266.bin"

if (-not (Test-Path -LiteralPath $tool)) {
  Write-Host "未找到烧录工具：$tool" -ForegroundColor Red
  Pause-Exit 1
}

Write-Title "1. 驱动检查"
if (Test-Path -LiteralPath $driver) {
  Write-Host "如果电脑第一次连接 ESP，建议先安装 CH340/CH341 驱动。"
  $install = Read-Host "需要打开驱动安装程序吗？输入 Y 打开，直接回车跳过"
  if ($install -match "^[yY]$") {
    Start-Process -FilePath $driver -Verb RunAs
    Write-Host "驱动安装完成后，请重新插拔 ESP，再回到本窗口继续。"
    Read-Host "准备好后按回车继续"
  }
} else {
  Write-Host "当前工具包没有包含驱动安装器，跳过驱动安装。" -ForegroundColor Yellow
}

Write-Title "2. 选择固件"
$firmware = Select-Firmware $defaultFirmware $firmwareDir
if (-not $firmware) {
  Write-Host "未选择固件，已取消。" -ForegroundColor Yellow
  Pause-Exit 1
}
Write-Host "将刷入固件：$firmware"

Write-Title "3. 选择串口"
$port = Select-Port
if (-not $port) {
  Pause-Exit 1
}
Write-Host "将使用串口：$port"

Write-Title "4. 开始烧录"
Write-Host "请确认 ESP 已通过 USB 连接电脑。"
Read-Host "按回车开始擦除并刷入固件"

$commonArgs = @("-vv", "-cd", "nodemcu", "-cb", "115200", "-cp", $port)

Write-Host ""
Write-Host "正在擦除 Flash..."
& $tool @commonArgs "-ce"
if ($LASTEXITCODE -ne 0) {
  Write-Host "擦除失败。请检查串口是否被其他软件占用，或按住 BOOT/FLASH 键后重试。" -ForegroundColor Red
  Pause-Exit $LASTEXITCODE
}

Write-Host ""
Write-Host "正在写入固件..."
& $tool @commonArgs "-ca", "0x00000", "-cf", $firmware, "-cr"
if ($LASTEXITCODE -ne 0) {
  Write-Host "写入失败。请检查 USB 线、串口、驱动和固件文件。" -ForegroundColor Red
  Pause-Exit $LASTEXITCODE
}

Write-Host ""
Write-Host "刷入完成。" -ForegroundColor Green
Write-Host "如果屏幕没有自动重启，请拔插 ESP 电源。"
Pause-Exit 0
