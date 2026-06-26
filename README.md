# PrintSphere Lite

PrintSphere Lite 是一款基于 ESP8266EX 和 240x240 ST7789 屏幕的 Bambu 打印状态显示设备。

设备通过 WiFi 连接 Bambu 云 MQTT，实时显示打印机状态。配置完成后，ESP 会独立联网刷新数据，电脑端工具只在首次配置、重新登录账号、切换打印机或更新配置时使用。

## 功能

- 显示打印机名称、打印状态、进度百分比和边框进度条
- 显示喷嘴温度、热床温度、当前层数、总层数和剩余时间
- 支持单喷嘴和常见双喷嘴机型字段兼容
- 支持 A1 / A1 mini、P1P / P1S、P2S、H2 系列、X1 系列、X2D 等 Bambu 机型的常见云端字段
- 支持 25%、50%、75%、100% 四档屏幕亮度
- 无任务或任务完成五分钟后自动降到 25% 亮度，新任务开始后恢复用户设置亮度
- 支持 USB 串口配置，ESP 局域网页面可切换已同步打印机
- 支持一台 Bambu 账号配置多台 ESP，建议一次只连接一台 ESP 进行配置

## 硬件

- ESP8266EX / NodeMCU 兼容模块
- CH340/CH341 USB 串口模块
- 240x240 ST7789 屏幕

当前固件是按本项目使用的 SD2 硬件接线适配的。其他硬件需要检查 `platformio.ini` 和 `include/User_Setup.h` 中的屏幕引脚。

## 外壳 3D 模型

- 中国大陆地区：[MakerWorld 中国大陆模型页](https://makerworld.com.cn/models/2587841?appSharePlatform=copy)
- 海外地区：[MakerWorld 国际模型页](https://makerworld.com/models/2891359?appSharePlatform=copy)

## 目录结构

```text
src/              ESP8266 固件源码
include/          TFT_eSPI 屏幕配置
companion/        Windows 电脑端配置工具源码
固件/             最新发布固件 bin
刷固件工具/       一键刷固件脚本
platformio.ini    PlatformIO 构建配置
build-release.bat  生成 Release 交付包
```

## 使用方式

如果从 GitHub 下载，请进入 [Releases](https://github.com/ccord34/printsphere-lite/releases) 页面，下载名称包含“完整交付包”的 `.zip` 文件。不要下载 GitHub 自动生成的 `Source code` 压缩包，源码包不包含 Windows 后端运行环境、烧录工具和驱动。

1. 将 ESP 通过 USB 连接到 Windows 电脑。
2. 打开发布包里的 `后端配置工具\打开配置工具.bat`。
3. 登录 Bambu 云服务账号。
4. 选择或手动输入 2.4G WiFi，并填写 WiFi 密码。
5. 点击“保存并配置 ESP WiFi”。
6. 刷新打印机列表，选择需要显示的打印机。
7. 点击“显示这台并同步”。
8. 屏幕开始显示打印状态后，可以断开电脑 USB，改用普通 USB 电源供电。

设备连接 WiFi 后，也可以访问 ESP 的本地页面：

```text
http://ESP的IP地址:8081/
```

电脑端工具同步过打印机列表后，ESP 本地页面可以在电脑关闭时切换已同步的打印机。

## 开发构建

安装 PlatformIO 后执行：

```powershell
platformio run -e sd2
```

生成的固件位于：

```text
.pio/build/sd2/firmware.bin
```

发布前请复制最新固件到：

```text
固件/printsphere-lite-esp8266.bin
```

## 后端配置工具

后端使用 Node.js 内置模块实现，无需 npm install。

开发时可以运行：

```powershell
node companion/server.js 8795
```

发布包中可以放入 `companion/node/node.exe`，这样客户电脑无需单独安装 Node.js。源码仓库默认不提交 `companion/node/`。

## 隐私和发布检查

不要提交客户或测试运行数据：

```text
companion/data/config.json
companion/data/devices.json
companion/data/device-history.jsonl
companion/data/server-state.json
```

这些文件可能包含 WiFi 名称、WiFi 密码、Bambu token、打印机序列号、设备 IP 或设备名。`.gitignore` 已默认排除。

发布前建议执行：

```powershell
rg -n -i "token|access_token|refresh_token|wifi_password|ssid|password|serial|dev_id|192\.168|COM7|COM11"
```

如果命中真实账号、真实 WiFi、真实 token 或真实设备名，必须先删除或脱敏。

## License

This project is source-available for personal, educational, research, and
non-commercial use only. Commercial use, resale, production batches, paid
service integration, or customer delivery packages require separate written
authorization.

See [LICENSE](LICENSE) for details.
