# Bambu MQTT 可获取字段参考

本文档整理当前项目已经接入、解析或在界面中使用过的 Bambu 打印机状态字段，供其他项目参考。

注意：Bambu 不同机型、不同固件版本、是否连接 AMS、是否双喷嘴，返回字段会有差异。另一个项目接入时不要只依赖单一字段名，建议按本文的兼容规则解析。

## 1. 订阅与刷新方式

当前项目的思路：

- 每台打印机订阅对应设备的 MQTT 状态上报主题。
- 周期性发送 `pushall` 请求，让设备完整推送一次状态。
- 上屏、下屏各自关联一台设备，也可以进入待机页。
- 后台可以固定选择设备，也可以按打印状态和剩余时间做自动调度。

常见主题方向：

```text
device/{serial}/report
```

其中 `{serial}` 是设备序列号 / deviceId。

## 2. 设备基础信息

| 字段 | 类型 | 说明 |
|---|---:|---|
| `name` | string | 打印机名称，通常来自云端设备列表 |
| `deviceId` / `serial` | string | 设备 ID / 序列号 |
| `model` | string | 打印机型号，可能是底层型号值 |
| `taskImage` | string | 当前任务封面图地址，可能为空 |

### 型号映射

部分接口返回的是底层型号值，需要映射成用户可读型号。

| 显示型号 | 底层设备型号值 |
|---|---|
| X1C | `BL-P001` |
| P1P | `C11` |
| P1S | `C12` |
| X1E | `C13` |
| A1 mini | `N1` |
| A1 | `N2S` |
| X2D | `N6-V2` |
| P2S | `N7-V2` |
| H2C | `O1C2-V2` |
| H2D | `O1D` |
| H2S | `O1S` |

## 3. 打印状态

### 建议兼容读取字段

| 数据 | 兼容字段名 |
|---|---|
| 打印状态 | `printStatus`, `gcode_state`, `print_status`, `status`, `state` |

### 状态值映射

| 原始值 | 建议中文显示 |
|---|---|
| `RUNNING` | 打印中 |
| `FINISH` / `FINISHED` / `COMPLETE` | 完成 |
| `PAUSE` / `PAUSED` | 暂停 |
| `IDLE` | 空闲 |
| `FAILED` / `FAILURE` / `ERROR` | 失败 |
| `OFFLINE` | 离线 |
| `PREPARE` | 准备中 |
| `SLICING` | 切片中 |
| `INIT` | 初始化 |
| 其他未知值 | 未知，或直接显示原始值 |

注意事项：

- 未识别状态不要显示空白，至少显示“未知”或原始值。
- 状态字段在不同来源里字段名不完全一致，必须做多字段兼容。
- 如果日志里状态正常但屏幕不显示，优先检查 UI label 是否被隐藏、裁切、覆盖或画到屏幕外，不要只改解析层。

## 4. 打印进度

| 数据 | 类型 | 兼容字段名 |
|---|---:|---|
| 打印进度 | number | `printProgress`, `mc_percent`, `percent`, `progress` |
| 剩余时间，单位分钟 | number | `remainingTime`, `mc_remaining_time`, `remaining_minutes`, `remainingMinutes` |
| 当前层数 | number | `layerNum`, `layer_num`, `current_layer`, `currentLayer` |
| 总层数 | number | `totalLayerNum`, `total_layer_num`, `total_layers`, `totalLayers` |

注意事项：

- `remainingTime` 通常是分钟，需要前端转换成 `0min`、`1h 5min`、`122h 30min` 等显示格式。
- 层数为 `0/0`、缺字段或总层数为 0 时，界面应避免除零或显示异常。
- 长时间文本要考虑圆屏空间，必要时使用滚动或缩短格式。

## 5. 喷嘴温度

### 单喷嘴设备

单喷嘴设备通常返回：

| 字段 | 类型 | 说明 |
|---|---:|---|
| `activeNozzle` | string | 通常为 `single` |
| `nozzleTemp` | number | 主喷嘴当前温度 |
| `targetNozzleTemp` | number | 主喷嘴目标温度 |

兼容字段：

| 数据 | 兼容字段名 |
|---|---|
| 主喷嘴当前温度 | `nozzleTemp`, `nozzle_temper`, `nozzle_temp`, `nozzleTemperature` |
| 主喷嘴目标温度 | `targetNozzleTemp`, `nozzle_target_temper`, `target_nozzle_temp` |

显示建议：

- 单喷嘴设备只显示“主喷嘴”。
- 不要强行显示左喷嘴、右喷嘴，避免出现不存在的喷嘴温度。

### 双喷嘴设备

双喷嘴设备通常返回：

| 字段 | 类型 | 说明 |
|---|---:|---|
| `activeNozzle` | string | `left` / `right` |
| `leftNozzleTemp` | number | 左喷嘴当前温度 |
| `leftTargetNozzleTemp` | number | 左喷嘴目标温度 |
| `rightNozzleTemp` | number | 右喷嘴当前温度 |
| `rightTargetNozzleTemp` | number | 右喷嘴目标温度 |

兼容字段：

| 数据 | 兼容字段名 |
|---|---|
| 左喷嘴当前温度 | `leftNozzleTemp`, `left_nozzle_temp`, `left_nozzle_temper`, `tool0_nozzle_temp` |
| 左喷嘴目标温度 | `leftTargetNozzleTemp`, `left_target_nozzle_temp`, `tool0_target_nozzle_temp` |
| 右喷嘴当前温度 | `rightNozzleTemp`, `right_nozzle_temp`, `right_nozzle_temper`, `tool1_nozzle_temp` |
| 右喷嘴目标温度 | `rightTargetNozzleTemp`, `right_target_nozzle_temp`, `tool1_target_nozzle_temp` |

部分设备也可能通过嵌套结构返回：

```text
device.extruder.info[]
```

其中常见字段包括：

| 字段 | 说明 |
|---|---|
| `id` | 工具编号 |
| `temp` | 当前温度 |
| `target_temp` | 目标温度 |

注意事项：

- 双喷嘴模式只显示左喷嘴和右喷嘴，不显示主喷嘴。
- 不要把 `nozzleTemp` 当成第三个喷嘴。
- 未上报的喷嘴字段可能为空，应隐藏对应项目，不要显示历史值或默认值。
- 当前项目曾遇到过双喷嘴异常值，比如某个喷嘴温度被拼接成异常大数字，因此温度显示前建议做合理范围校验。

## 6. 热床与仓温

| 数据 | 类型 | 兼容字段名 |
|---|---:|---|
| 热床当前温度 | number | `bedTemp`, `bed_temper`, `bed_temp`, `bedTemperature` |
| 热床目标温度 | number | `targetBedTemp`, `bed_target_temper`, `bed_target_temp` |
| 仓温当前温度 | number | `chamberTemp`, `chamber_temper`, `chamber_temp`, `chamberTemperature` |
| 仓温目标温度 | number | `chamberTargetTemp`, `chamber_target_temper`, `chamber_target_temp` |

注意事项：

- 仓温优先使用 `chamberTemp`。
- 不是所有机型都有仓温传感器。没有仓温传感器时应隐藏仓温，不要显示默认值。
- P1S 等机型可能没有有效仓温传感器；P2S 等机型如果实际返回 `chamberTemp`，则可以显示。
- 不要把无效值、默认值或历史值当成真实仓温。

## 7. 灯光状态

| 字段 | 类型 | 说明 |
|---|---:|---|
| `chamberLight` / `chamber_light` | string | 主腔灯状态，常见值 `on` / `off` |
| `chamberLight2` / `chamber_light2` | string | 第二路腔灯状态，常见值 `on` / `off` |
| `workLight` / `work_light` | string | 工作灯状态，常见值 `on` / `off` |

部分设备可能通过数组返回：

```text
lights_report[]
```

其中可根据 `node` 判断：

| `node` | 说明 |
|---|---|
| `chamber_light` | 主腔灯 |
| `chamber_light2` | 第二路腔灯 |

当前项目 UI 后期只保留一个腔灯状态图标，通常优先使用 `chamberLight`。

## 8. 耗材类型

| 字段 | 类型 | 说明 |
|---|---:|---|
| `trayType` / `tray_type` | string | 当前使用中的耗材类型 |
| `filament_type` | string | 耗材类型兼容字段 |
| `vt_tray.tray_type` | string | 虚拟料盘 / 当前料盘里的耗材类型 |

当前可能值包括：

```text
ABS
ABS-GF
ASA
ASA-AERO
ASA-CF
BVOH
EVA
HIPS
PA
PA-CF
PA-GF
PA6-CF
PC
PCTG
PE
PE-CF
PET-CF
PETG
PETG-CF
PHA
PLA
PLA-AERO
PLA-CF
PP
PP-CF
PP-GF
PPA-CF
PPA-GF
PPS
PPS-CF
PVA
TPU
TPU-AMS
```

注意事项：

- `trayType` 表示当前使用中的耗材类型。
- 耗材类型不一定绝对来自 AMS。AMS 是外置设备，设备未连接 AMS 时也可能通过任务、虚拟料盘或其他状态字段提供耗材信息。
- 不要只依赖 AMS 槽位数据判断当前耗材。
- 如果字段为空，UI 应隐藏耗材标签或显示“未知”，不要写死成 `PLA Basic`。

## 9. 错误信息

| 字段 | 类型 | 说明 |
|---|---:|---|
| `hmsErrors` | array | 当前 HMS 错误列表，只包含可展示字段 |

注意事项：

- HMS 错误列表可能为空。
- 另一个项目如果只做状态监控，可以先展示错误数量；后续再展开具体错误内容。

## 10. 项目内部状态，不一定来自打印机 MQTT

这些值当前项目会用到，但不一定是打印机 MQTT 原始字段：

| 数据 | 来源 | 说明 |
|---|---|---|
| Wi-Fi 图标 | ESP32 本机 Wi-Fi 状态 / RSSI | 表示监控屏自身网络连接 |
| MQTT 连接状态 | ESP32 内部状态 | 表示监控屏是否连上 MQTT |
| mDNS 地址 | ESP32 本机服务 | 例如 `bambu-monitor.local` |
| 后台 IP | ESP32 本机 WebServer | 例如 `http://192.168.x.x/` |

## 11. 接入时的重点注意事项

### 11.1 字段名必须兼容

同一含义可能存在多个字段名，例如打印状态可能来自：

```text
printStatus
gcode_state
print_status
status
state
```

不要只读一个字段。

### 11.2 不要显示无效默认值

以下情况建议隐藏，而不是显示假数据：

- 仓温字段不存在。
- 喷嘴字段为空。
- 双喷嘴设备没有上报其中一个喷嘴。
- 目标温度为空。
- 层数总数为 0。

### 11.3 UI 要防止长文本遮挡

圆屏空间很小，以下字段可能过长：

- 打印机名称
- 设备型号
- 耗材类型
- 剩余时间，例如 `122h 30min`
- 任务名称

建议策略：

- 短文本直接居中显示。
- 超过宽度的文本滚动显示。
- 滚动区域必须裁切，不能遮挡进度环或其他元素。

### 11.4 温度要做合理范围校验

建议显示前进行基础校验：

- 喷嘴温度通常不应为异常大数。
- 热床温度通常不应为异常大数。
- 仓温没有字段时不要用默认 0℃ 或 5℃ 误导用户。

### 11.5 耗材来源不要只看 AMS

AMS 是外置设备，不连接 AMS 时仍然可能有当前耗材类型。

推荐优先顺序：

1. 当前任务 / 当前使用中的 `trayType`
2. `vt_tray.tray_type`
3. 兼容字段 `filament_type`
4. AMS 当前槽位信息
5. 全部为空时隐藏或显示未知

### 11.6 状态更新后要触发 UI 刷新

解析成功不代表显示成功。另一个项目如果遇到状态不显示，应检查完整链路：

1. 原始 payload 是否有值。
2. 解析后的状态变量是否更新。
3. UI 刷新是否被触发。
4. label 是否可见。
5. label 是否被其他元素覆盖。
6. label 是否超出屏幕或裁切区域。

## 12. 当前项目尚未重点使用的扩展字段

这些字段后续可以继续扩展，但当前主界面没有作为核心展示：

| 字段 | 说明 |
|---|---|
| `taskImage` | 当前任务封面图 |
| `workLight` | 工作灯 |
| `hmsErrors` | HMS 错误 |
| 耗材颜色 | 当前项目未作为主要显示 |
| 风扇速度 | 当前项目未作为主要显示 |
| 打印任务名称 | 当前项目未作为主要显示 |

