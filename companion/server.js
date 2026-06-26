const http = require("http");
const https = require("https");
const fs = require("fs");
const path = require("path");
const os = require("os");
const { execFile } = require("child_process");

const BACKEND_VERSION = "backend-v0.4.56-usb-first";
const CHANGELOG = [
  {
    version: BACKEND_VERSION,
    changes: [
      "所有 ESP 配置写入统一为 USB 串口优先、HTTP 兜底：WiFi、云 token、打印机选择、亮度和本地打印机列表均遵循同一规则",
      "执行配置写入前先通过当前串口重新确认 ESP 身份，避免历史设备档案或旧 IP 接收配置",
      "固件功能沿用 v0.4.54 原厂 10 位背光 PWM，并统一正式版本号"
    ]
  },
  {
    version: "backend-v0.4.55-brightness-target",
    changes: [
      "修复多 ESP 历史档案下应用亮度可能优先写入旧 IP，当前 USB 设备仍保持 50%，随后页面又被设备回报覆盖的问题",
      "应用亮度前先通过当前串口重新确认 ESP 身份，亮度写入改为当前 USB 串口优先、HTTP 仅作兜底",
      "固件沿用 v0.4.54 已验证的 SD2 原厂 10 位 PWM，仅统一正式版本号"
    ]
  },
  {
    version: "backend-v0.4.54-brightness-pwm",
    changes: [
      "修复 v0.4.53 四档配置已写入但屏幕亮度肉眼不变化的问题：背光 PWM 改回 SD2 原厂固件使用的 0-1023 范围和反相占空比",
      "后端亮度选项、每台 ESP 独立保存、五分钟自动降亮和新任务恢复逻辑保持不变"
    ]
  },
  {
    version: "backend-v0.4.53-brightness",
    changes: [
      "配置页面新增 25% / 50% / 75% / 100% 四档屏幕亮度，并按每台 ESP 独立保存",
      "新增“应用屏幕亮度”按钮；首次配置 WiFi 和同步云配置时也会一并写入亮度",
      "显示 ESP 当前实际亮度和自动节能状态；不改 Bambu 云登录、打印机选择和 MQTT 数据解析"
    ]
  },
  {
    version: "backend-v0.4.52-release",
    changes: [
      "统一正式交付版本号为 v0.4.52，固件和后端使用同一版本号，方便客户确认和售后回退",
      "包含 v0.4.47 的多 ESP 自动切换、设备 IP 身份绑定、串口写入确认和串口任务排队修复",
      "本次只同步正式版本标识，不改变已通过实机测试的配置、屏幕和云 MQTT 行为"
    ]
  },
  {
    version: "backend-v0.4.47-auto-device-switch",
    changes: [
      "配置页面保持打开时每 8 秒低频识别一次当前 USB ESP，拔下第一台再插入第二台后会自动切换设备档案并刷新 IP",
      "用户正在配置 WiFi 或检测设备时暂停后台识别，避免两个串口操作互相抢占",
      "后端串口命令统一排队执行，页面初始化改为先识别 ESP 再扫描 WiFi，避免自动检测和 WiFi 扫描同时占用 COM 口",
      "保留 v0.4.46 的未联网设备旧 IP 清理，以及 v0.4.45 的串口写入确认"
    ]
  },
  {
    version: "backend-v0.4.46-esp-ip-identity",
    changes: [
      "修复更换 ESP 后，串口已识别新设备但页面仍显示该档案历史 IP、可能优先向旧地址写入的问题",
      "串口状态明确返回未获取 IP 时立即清除当前 ESP 的旧 host/last_ip；未确认联网时配置 WiFi 强制使用当前 USB 串口",
      "ESP HTTP 配置端口继续统一使用 8081；不改固件、屏幕和 Bambu 云 MQTT 逻辑"
    ]
  },
  {
    version: "backend-v0.4.45-serial-write-ack",
    changes: [
      "修复第二台或恢复出厂后的 ESP 通过 USB 配置 WiFi 时，后端误报串口写入成功但设备实际没有收到配置的问题",
      "USB 写入改为等待 ESP 返回 JSON 确认，未收到确认时明确报告失败，不再把 Windows 串口文件流写入完成当作设备已接收",
      "保留原有 HTTP 优先、串口兜底、单次单 ESP、防误写和 Bambu 云 MQTT 配置流程"
    ]
  },
  {
    version: "backend-v0.4.44-x2d-model-fix",
    changes: [
      "Bambu 云绑定列表新增 X2D 机型归一化：X2D 的 dev_model_name=N6-V2 时，后端打印机列表显示为 X2D",
      "保留 v0.4.43 的 ESP 本地网页打印机列表同步按钮和 ESP IP 自动兜底逻辑"
    ]
  },
  {
    version: "backend-v0.4.43-esp-printer-list-sync",
    changes: [
      "后端新增“同步打印机列表到 ESP 本地网页”按钮，可把电脑端已读取的 Bambu 打印机列表主动推送到 ESP 的 /printers.json",
      "刷新打印机列表、选择显示打印机和手动同步都会返回 ESP 列表同步结果，避免 ESP 网页一直显示“暂无已同步打印机”却不知道原因",
      "ESP 地址未手动填写时，会自动使用上次检测到的 ESP WiFi IP 作为无线同步目标",
      "同步结果只把 HTTP 2xx 当作成功；不改变 ESP 屏幕显示、云 MQTT 数据解析、USB 串口兜底和单次单 ESP 防误写流程"
    ]
  },
  {
    version: "backend-v0.4.42-esp-web-config",
    changes: [
      "刷新 Bambu 打印机列表或同步云配置时，会把精简打印机列表同步到 ESP，供 ESP 本地网页无线切换显示打印机",
      "ESP 端新增内置网页后，后端继续负责 Bambu 云登录和 token 获取，避免把云登录流程塞进 ESP8266",
      "不改变原有 USB 串口兜底写入、ESP HTTP 写入和单次单 ESP 防误写流程"
    ]
  },
  {
    version: "backend-v0.4.35-history-profile-polish",
    changes: [
      "优化历史 ESP 档案显示：同一串口当前检测到另一台 ESP 时，旧档案会标记为离线，避免下拉框看起来像多台设备都在线",
      "自动检测后仍自动选中当前唯一连接的 ESP，历史档案保留用于以后这台设备再次插入时识别",
      "继续保留 v0.4.34 的单次单 ESP 防误写和流程简化"
    ]
  },
  {
    version: "backend-v0.4.34-flow-polish",
    changes: [
      "优化单次单 ESP 配置流程：检测到多台 ESP 时页面直接禁用 WiFi 写入、同步和选择打印机，避免用户误点后才报错",
      "减少重复串口检测：选择打印机后同步配置、保存 WiFi 后写入配置不再重复扫描两次，配置响应更快",
      "页面文案改成客户视角的“检测当前 ESP / 重新检测 ESP”，弱化 COM 口概念，只保留高级写入信息"
    ]
  },
  {
    version: "backend-v0.4.33-single-esp-config-mode",
    changes: [
      "配置流程改为单次只连接一台 ESP：自动检测发现多台 ESP 时会提示只保留当前要配置的一台，避免多串口误写",
      "电脑端保留公共默认 WiFi、Bambu 登录信息和打印机列表，新 ESP 首次识别时自动继承，方便连续配置下一台设备",
      "刷新/保存 WiFi 会同步更新公共默认 WiFi；选择打印机后目标仍按当前单台 ESP 档案锁定"
    ]
  },
  {
    version: "backend-v0.4.32-target-http-fix",
    changes: [
      "修复多 ESP 同步云配置仍可能写到 COM7 的根因：HTTP 写入 URL 不再重新读取全局当前 ESP，而是使用本次选择的目标 ESP 档案",
      "选择打印机时按按钮携带的 ESP ID 直接锁定目标档案，保存打印机和同步配置都只写入这台 ESP",
      "同步结果返回目标 ESP ID、串口和 HTTP 地址，便于确认实际写入到哪台设备"
    ]
  },
  {
    version: "backend-v0.4.31-auto-detect-esp",
    changes: [
      "新增自动检测 ESP：后端枚举所有串口并逐个读取 ESP 身份，按真实 MAC/device_id 自动修正每台 ESP 对应的 COM 口",
      "页面启动和刷新串口后会自动检测不同串口状态，减少用户手动选择 COM 口和手动读取 ESP 的步骤",
      "已保存 ESP 设备按串口号排序，显示真实串口和在线状态，避免多台设备列表里都显示同一个 COM 口"
    ]
  },
  {
    version: "backend-v0.4.30-select-target-fix",
    changes: [
      "修复点击“显示这台”时可能推送到旧串口的问题：选择打印机后直接使用刚保存的当前 ESP 档案写入，不再重新猜测目标设备",
      "“显示这台”请求会带上页面当前 ESP 身份和串口，后端发现页面状态与后端当前设备不一致会阻止写入，避免误写到其他 ESP",
      "简化页面流程文案：“显示这台”改为“显示这台并同步”，选打印机即自动写入 ESP，手动同步按钮只作为重试入口"
    ]
  },
  {
    version: "backend-v0.4.29-multi-esp-save-fix",
    changes: [
      "修复多 ESP 档案保存顺序：切换到某台 ESP 后保存 WiFi、串口、地址或打印机，会先写入当前 ESP 档案，再同步页面显示",
      "补充接口自测覆盖：默认档案禁止写配置，COM7/COM11 档案 WiFi 相互隔离，串口读取能正确切换 ESP 身份"
    ]
  },
  {
    version: "backend-v0.4.28-multi-esp-redesign",
    changes: [
      "重做单 Bambu 账号多 ESP 设备流程：云账号共用，每台 ESP 独立保存 WiFi、串口、地址和显示的打印机",
      "后端强制先读取当前串口 ESP 身份，再允许保存 WiFi 或选择打印机，避免配置写入默认档案后丢失",
      "从旧单 ESP 配置升级时，首次读取真实 ESP 会迁移原有 WiFi、打印机和串口配置到该 ESP 档案"
    ]
  },
  {
    version: "backend-v0.4.27-wifi-detect-feedback",
    changes: [
      "检测 ESP WiFi 增加 USB 串口状态兜底，局域网找不到时也能提示 ESP 是否已响应、是否拿到 WiFi IP",
      "保存并配置 ESP WiFi、检测 ESP WiFi 按钮点击后立即显示进度并临时禁用，避免长时间扫描时看起来没有反应"
    ]
  },
  {
    version: "backend-v0.4.26-config-write-fix",
    changes: [
      "配置保存改为先写临时文件再原子替换，避免读取 ESP 时 config.json 目标文件短暂占用导致 EPERM",
      "保留 v0.4.25 的串口列表合并逻辑，继续稳定显示 COM7、COM11 等多个端口"
    ]
  },
  {
    version: "backend-v0.4.25-serial-merge-fix",
    changes: [
      "后端串口列表合并 Win32_SerialPort 和 mode 两种来源，避免其中一种只返回 COM7 时漏掉 COM11",
      "串口列表按 COM 编号去重排序，读取 ESP 下拉框稳定显示所有已连接端口"
    ]
  },
  {
    version: "backend-v0.4.24-serial-list-fix",
    changes: [
      "后端串口列表改为和刷固件工具一致：优先 Win32_SerialPort，失败再用 mode 兜底",
      "修复页面选择 COM11 后被自动刷新改回当前档案 COM7，导致读取 ESP 仍使用旧串口"
    ]
  },
  {
    version: "backend-v0.4.23-esp-url-scan-fix",
    changes: [
      "修复 ESP 未连 WiFi 时返回 (IP unset) 被保存为 last_ip 后导致 ERR_INVALID_URL",
      "WiFi 扫描接口跟随页面当前串口，避免多 ESP 时仍扫描旧串口"
    ]
  },
  {
    version: "backend-v0.4.22-multi-esp-profiles",
    changes: [
      "后端支持多台 ESP 配置档案，Bambu 账号共用，每台 ESP 单独绑定 WiFi、串口、打印机",
      "新增读取当前串口 ESP 身份，按 MAC/device_id 自动切换当前配置档案"
    ]
  },
  {
    version: "backend-v0.4.21-esp-serial-wifi-scan",
    changes: [
      "WiFi 扫描支持通过 USB 串口请求 ESP 扫描附近 2.4G WiFi",
      "后端扫描顺序改为 ESP HTTP、ESP 串口、电脑扫描、手动输入兜底"
    ]
  },
  {
    version: "backend-v0.4.20-esp-wifi-scan",
    changes: [
      "WiFi 扫描改为优先通过 ESP 获取附近 2.4G WiFi",
      "ESP 不在线时保留电脑扫描和手动输入兜底",
      "server-state.json 被占用时不再导致配置工具启动失败"
    ]
  },
  {
    version: "backend-v0.4.19-wifi-scan",
    changes: [
      "第 2 步增加附近 WiFi 自动扫描和下拉选择",
      "保留手动输入 WiFi 名称，兼容隐藏网络或电脑无法扫描 WiFi 的情况"
    ]
  },
  {
    version: "backend-v0.4.18-model-fields",
    changes: [
      "参考 PrintSphere 完整工程扩展 Bambu 云 MQTT 字段兼容",
      "后端打印机列表增加 A1/A1 mini/P1/P2/H2/X1 系列型号名规范化显示"
    ]
  },
  {
    version: "backend-v0.4.15-release",
    changes: [
      "清理客户交付无关脚本和调试文档",
      "客户启动入口改为中文文件名“打开配置工具.bat”",
      "优化中文使用说明，明确 Bambu 云 MQTT 配置流程和隐私数据排除规则"
    ]
  },
  {
    version: "backend-v0.4.14-launcher",
    changes: [
      "后端默认从 8795 开始自动寻找可用端口，不需要客户改源码",
      "启动成功后写出 server-state.json，启动器据此自动打开正确地址",
      "更新中文客户入口脚本，双击即可启动配置工具并打开浏览器"
    ]
  },
  {
    version: "backend-v0.4.13-flow",
    changes: [
      "后端页面改成登录、配置 WiFi、选择打印机的步骤流程",
      "未保存 WiFi 前禁止选择打印机，避免 ESP 只收到打印机名称",
      "保存 WiFi 后自动写入 ESP，并尝试检测 ESP WiFi 是否已配置成功",
      "ESP HTTP 状态检测会自动保存发现到的 ESP 局域网地址"
    ]
  },
  {
    version: "backend-v0.4.12-clean",
    changes: [
      "清理后端乱码文案和调试遗留逻辑",
      "修复串口设备路径，确保写入 \\\\.\\COMx 而不是字面量模板字符串",
      "串口写入前固定设置 115200 波特率，并等待 ESP 串口复位后再发送配置",
      "保留 Bambu 云登录、打印机列表、HTTP 优先写入 ESP、串口兜底写入 ESP"
    ]
  }
];

function pickPort() {
  const candidates = [process.env.PORT, process.argv[2], "8795"];
  for (const item of candidates) {
    const port = Number(item);
    if (Number.isInteger(port) && port > 0 && port < 65536) return port;
  }
  return 8795;
}

let PORT = pickPort();
const DATA_DIR = path.join(__dirname, "data");
const CONFIG_FILE = path.join(DATA_DIR, "config.json");
const DEVICES_FILE = path.join(DATA_DIR, "devices.json");
const HISTORY_FILE = path.join(DATA_DIR, "device-history.jsonl");
const SERVER_STATE_FILE = path.join(DATA_DIR, "server-state.json");

const DEFAULT_CONFIG = {
  wifi: { ssid: "", password: "" },
  cloud: { region: "cn", account: "", access_token: "", mqtt_username: "", token_expires_at: 0 },
  printer: { serial: "", access_code: "", display_name: "", model: "" },
  display: { brightness: 100, active_brightness: 100, dimmed: false },
  esp: { host: "", port: 8081, serial_port: "COM7", wifi_status: "unknown", last_ip: "", last_checked_at: 0, last_error: "" },
  defaults: { wifi: { ssid: "", password: "" } },
  active_esp_id: "default",
  esp_devices: {}
};

ensureData();

function ensureData() {
  fs.mkdirSync(DATA_DIR, { recursive: true });
  if (!fs.existsSync(CONFIG_FILE)) writeJson(CONFIG_FILE, DEFAULT_CONFIG);
  if (!fs.existsSync(DEVICES_FILE)) writeJson(DEVICES_FILE, { updated_at: 0, devices: [] });
}

function mergeConfig(base, extra) {
  const out = JSON.parse(JSON.stringify(base));
  for (const [key, value] of Object.entries(extra || {})) {
    if (value && typeof value === "object" && !Array.isArray(value)) out[key] = mergeConfig(out[key] || {}, value);
    else out[key] = value;
  }
  return out;
}

function normalizeBrightness(value) {
  const level = Number(value);
  if (level <= 25) return 25;
  if (level <= 50) return 50;
  if (level <= 75) return 75;
  return 100;
}

function readJson(file, fallback) {
  try {
    return JSON.parse(fs.readFileSync(file, "utf8").replace(/^\uFEFF/, ""));
  } catch {
    return fallback;
  }
}

function writeJson(file, value) {
  const text = JSON.stringify(value, null, 2);
  const tmp = `${file}.${process.pid}.${Date.now()}.tmp`;
  fs.writeFileSync(tmp, text, "utf8");
  try {
    fs.renameSync(tmp, file);
  } catch (error) {
    try {
      if (fs.existsSync(file)) fs.unlinkSync(file);
      fs.renameSync(tmp, file);
    } catch {
      try { fs.unlinkSync(tmp); } catch {}
      throw error;
    }
  }
}

function defaultEspProfile(id = "default") {
  return {
    id,
    label: id === "default" ? "默认 ESP" : id,
    chip_id: "",
    mac: "",
    device_id: id,
    last_seen_at: 0,
    wifi: JSON.parse(JSON.stringify(DEFAULT_CONFIG.wifi)),
    printer: JSON.parse(JSON.stringify(DEFAULT_CONFIG.printer)),
    display: JSON.parse(JSON.stringify(DEFAULT_CONFIG.display)),
    esp: JSON.parse(JSON.stringify(DEFAULT_CONFIG.esp))
  };
}

function normalizeEspId(value) {
  const raw = String(value || "").trim().toLowerCase();
  const cleaned = raw.replace(/[^a-z0-9_-]/g, "");
  return cleaned || "default";
}

function espIdFromStatus(status) {
  const deviceId = normalizeEspId(status && status.device_id);
  if (deviceId !== "default") return deviceId;
  const mac = normalizeEspId(status && status.mac);
  if (mac !== "default") return `esp-${mac}`;
  const chip = normalizeEspId(status && status.chip_id);
  if (chip !== "default") return `esp-${chip}`;
  return "default";
}

function espLabel(profile) {
  if (!profile) return "未选择 ESP";
  const port = profile.esp && profile.esp.serial_port;
  const state = profile.esp && profile.esp.last_error ? "离线" : (profile.last_seen_at ? "已识别" : "");
  const parts = [port, profile.label, profile.printer && (profile.printer.display_name || profile.printer.serial), state]
    .filter(Boolean);
  return parts.join(" / ") || profile.id || "ESP";
}

function serialPortNumber(port) {
  const match = String(port || "").match(/^COM(\d+)$/i);
  return match ? Number(match[1]) : 9999;
}

function hasActiveEsp(cfg) {
  return Boolean(cfg && cfg.active_esp_id && cfg.active_esp_id !== "default");
}

function hasSingleEspData(cfg) {
  return Boolean(
    cfg && (
      (cfg.wifi && (cfg.wifi.ssid || cfg.wifi.password)) ||
      (cfg.printer && (cfg.printer.serial || cfg.printer.display_name)) ||
      (cfg.esp && (cfg.esp.host || cfg.esp.last_ip || cfg.esp.serial_port))
    )
  );
}

function ensureDefaults(cfg) {
  cfg.defaults = mergeConfig(DEFAULT_CONFIG.defaults, cfg.defaults || {});
  if (!cfg.defaults.wifi.ssid && cfg.wifi && cfg.wifi.ssid) cfg.defaults.wifi.ssid = cfg.wifi.ssid;
  if (!cfg.defaults.wifi.password && cfg.wifi && cfg.wifi.password) cfg.defaults.wifi.password = cfg.wifi.password;
  return cfg;
}

function ensureEspProfiles(cfg) {
  ensureDefaults(cfg);
  cfg.esp_devices = cfg.esp_devices && typeof cfg.esp_devices === "object" && !Array.isArray(cfg.esp_devices) ? cfg.esp_devices : {};
  cfg.active_esp_id = normalizeEspId(cfg.active_esp_id);
  if (!cfg.esp_devices[cfg.active_esp_id]) {
    cfg.esp_devices[cfg.active_esp_id] = {
      ...defaultEspProfile(cfg.active_esp_id),
      wifi: mergeConfig(DEFAULT_CONFIG.wifi, cfg.defaults.wifi && (cfg.defaults.wifi.ssid || cfg.defaults.wifi.password) ? cfg.defaults.wifi : cfg.wifi),
      printer: mergeConfig(DEFAULT_CONFIG.printer, cfg.printer),
      display: mergeConfig(DEFAULT_CONFIG.display, cfg.display),
      esp: mergeConfig(DEFAULT_CONFIG.esp, cfg.esp)
    };
  }
  for (const [id, item] of Object.entries(cfg.esp_devices)) {
    const cleanId = normalizeEspId(id);
    const profile = mergeConfig(defaultEspProfile(cleanId), item);
    profile.id = cleanId;
    profile.device_id = normalizeEspId(profile.device_id || cleanId);
    profile.esp.serial_port = normalizeSerialPort(profile.esp.serial_port);
    profile.esp.port = Number(profile.esp.port || 8081);
    profile.esp.host = normalizeEspHost(profile.esp.host);
    profile.esp.last_ip = normalizeEspHost(profile.esp.last_ip);
    profile.display = mergeConfig(DEFAULT_CONFIG.display, profile.display);
    profile.display.brightness = normalizeBrightness(profile.display.brightness);
    profile.display.active_brightness = normalizeBrightness(profile.display.active_brightness);
    profile.display.dimmed = Boolean(profile.display.dimmed);
    if (!profile.wifi.ssid && !profile.wifi.password && (cfg.defaults.wifi.ssid || cfg.defaults.wifi.password)) {
      profile.wifi = mergeConfig(DEFAULT_CONFIG.wifi, cfg.defaults.wifi);
    }
    cfg.esp_devices[cleanId] = profile;
    if (cleanId !== id) delete cfg.esp_devices[id];
  }
  if (!cfg.esp_devices[cfg.active_esp_id]) cfg.active_esp_id = Object.keys(cfg.esp_devices)[0] || "default";
  const active = cfg.esp_devices[cfg.active_esp_id] || defaultEspProfile(cfg.active_esp_id);
  cfg.wifi = mergeConfig(DEFAULT_CONFIG.wifi, active.wifi);
  cfg.printer = mergeConfig(DEFAULT_CONFIG.printer, active.printer);
  cfg.display = mergeConfig(DEFAULT_CONFIG.display, active.display);
  cfg.esp = mergeConfig(DEFAULT_CONFIG.esp, active.esp);
  cfg.esp.host = normalizeEspHost(cfg.esp.host);
  cfg.esp.last_ip = normalizeEspHost(cfg.esp.last_ip);
  return cfg;
}

function persistActiveProfile(cfg) {
  ensureDefaults(cfg);
  cfg.esp_devices = cfg.esp_devices && typeof cfg.esp_devices === "object" && !Array.isArray(cfg.esp_devices) ? cfg.esp_devices : {};
  cfg.active_esp_id = normalizeEspId(cfg.active_esp_id);
  const current = mergeConfig(defaultEspProfile(cfg.active_esp_id), cfg.esp_devices[cfg.active_esp_id] || {});
  current.wifi = mergeConfig(DEFAULT_CONFIG.wifi, cfg.wifi);
  current.printer = mergeConfig(DEFAULT_CONFIG.printer, cfg.printer);
  current.display = mergeConfig(DEFAULT_CONFIG.display, cfg.display);
  current.display.brightness = normalizeBrightness(current.display.brightness);
  current.esp = mergeConfig(DEFAULT_CONFIG.esp, cfg.esp);
  current.esp.serial_port = normalizeSerialPort(current.esp.serial_port);
  current.esp.port = Number(current.esp.port || 8081);
  current.esp.host = normalizeEspHost(current.esp.host);
  current.esp.last_ip = normalizeEspHost(current.esp.last_ip);
  cfg.esp_devices[cfg.active_esp_id] = current;
  if (current.wifi.ssid || current.wifi.password) cfg.defaults.wifi = mergeConfig(DEFAULT_CONFIG.wifi, current.wifi);
  return cfg;
}

function readConfig() {
  const cfg = ensureEspProfiles(mergeConfig(DEFAULT_CONFIG, readJson(CONFIG_FILE, DEFAULT_CONFIG)));
  if (!cfg.esp.port || Number(cfg.esp.port) === 80 || Number(cfg.esp.port) === 8080) cfg.esp.port = 8081;
  cfg.esp.serial_port = normalizeSerialPort(cfg.esp.serial_port);
  cfg.esp.host = normalizeEspHost(cfg.esp.host);
  cfg.esp.last_ip = normalizeEspHost(cfg.esp.last_ip);
  return cfg;
}

function saveConfig(next) {
  const cfg = mergeConfig(DEFAULT_CONFIG, next);
  cfg.esp_devices = cfg.esp_devices && typeof cfg.esp_devices === "object" && !Array.isArray(cfg.esp_devices) ? cfg.esp_devices : {};
  cfg.active_esp_id = normalizeEspId(cfg.active_esp_id);
  if (!cfg.esp_devices[cfg.active_esp_id]) cfg.esp_devices[cfg.active_esp_id] = defaultEspProfile(cfg.active_esp_id);
  persistActiveProfile(cfg);
  ensureEspProfiles(cfg);
  cfg.esp.serial_port = normalizeSerialPort(cfg.esp.serial_port);
  cfg.esp.host = normalizeEspHost(cfg.esp.host);
  cfg.esp.last_ip = normalizeEspHost(cfg.esp.last_ip);
  writeJson(CONFIG_FILE, cfg);
  return cfg;
}

function isPrivateIpv4(address) {
  const parts = String(address || "").split(".").map((part) => Number(part));
  if (parts.length !== 4 || parts.some((part) => !Number.isInteger(part) || part < 0 || part > 255)) return false;
  return parts[0] === 10 || (parts[0] === 172 && parts[1] >= 16 && parts[1] <= 31) || (parts[0] === 192 && parts[1] === 168);
}

function normalizeEspHost(host) {
  let value = String(host || "").trim();
  if (!value || /^\(.*\)$/i.test(value)) return "";
  value = value.replace(/^https?:\/\//i, "").split("/")[0].split("?")[0].trim();
  if (value.includes("@")) value = value.split("@").pop();
  if (value.startsWith("[") && value.includes("]")) value = value.slice(1, value.indexOf("]"));
  else if ((value.match(/:/g) || []).length === 1) value = value.split(":")[0];
  if (/^\d{1,3}(\.\d{1,3}){3}$/.test(value)) return isPrivateIpv4(value) ? value : "";
  if (/^[a-z0-9.-]+$/i.test(value) && value.includes(".")) return value;
  return "";
}

function serviceUrls() {
  const urls = [`http://127.0.0.1:${PORT}/`];
  for (const [name, list] of Object.entries(os.networkInterfaces())) {
    if (/virtual|vmware|virtualbox|vbox|vethernet|loopback|bluetooth/i.test(name)) continue;
    for (const item of list || []) {
      if (/^192\.168\.56\./.test(item.address)) continue;
      if (item.family === "IPv4" && !item.internal && isPrivateIpv4(item.address)) urls.push(`http://${item.address}:${PORT}/`);
    }
  }
  return [...new Set(urls)];
}

function publicConfig() {
  const cfg = readConfig();
  const wifiSaved = Boolean(cfg.wifi.ssid && cfg.wifi.password);
  const espWifiConfigured = cfg.esp.wifi_status === "configured";
  const espReady = cfg.active_esp_id && cfg.active_esp_id !== "default";
  const profiles = Object.values(cfg.esp_devices || {}).filter((profile) => profile && profile.id !== "default").sort((a, b) => {
    const pa = serialPortNumber(a && a.esp && a.esp.serial_port);
    const pb = serialPortNumber(b && b.esp && b.esp.serial_port);
    if (pa !== pb) return pa - pb;
    return String(a && a.label || a && a.id || "").localeCompare(String(b && b.label || b && b.id || ""));
  }).map((profile) => ({
    id: profile.id,
    label: espLabel(profile),
    chip_id: profile.chip_id || "",
    mac: profile.mac || "",
    serial_port: profile.esp && profile.esp.serial_port || "",
    printer: profile.printer || {},
    display: profile.display || DEFAULT_CONFIG.display,
    wifi_saved: Boolean(profile.wifi && profile.wifi.ssid && profile.wifi.password),
    wifi_status: profile.esp && profile.esp.wifi_status || "unknown",
    last_seen_at: profile.last_seen_at || 0
  }));
  return {
    meta: { version: BACKEND_VERSION, urls: serviceUrls() },
    cloud: {
      region: cfg.cloud.region,
      account: cfg.cloud.account,
      logged_in: Boolean(cfg.cloud.access_token),
      mqtt_username: cfg.cloud.mqtt_username ? "已获取" : ""
    },
    steps: {
      cloud_ready: Boolean(cfg.cloud.access_token),
      esp_ready: Boolean(espReady),
      ready_to_configure_wifi: Boolean(espReady),
      wifi_saved: wifiSaved,
      esp_wifi_configured: espWifiConfigured,
      printer_selected: Boolean(cfg.printer.serial),
      ready_to_select_printer: Boolean(cfg.cloud.access_token && wifiSaved && espReady)
    },
    wifi: { ssid: cfg.wifi.ssid, password_saved: Boolean(cfg.wifi.password) },
    printer: cfg.printer,
    display: cfg.display,
    esp: cfg.esp,
    active_esp_id: cfg.active_esp_id,
    active_esp_label: espLabel(cfg.esp_devices[cfg.active_esp_id]),
    esp_profiles: profiles
  };
}

function apiBase(region) {
  return region === "global" ? "https://api.bambulab.com" : "https://api.bambulab.cn";
}

function mqttHost(region) {
  return region === "global" ? "us.mqtt.bambulab.com" : "cn.mqtt.bambulab.com";
}

function errorText(error) {
  if (!error) return "未知网络错误";
  const parts = [];
  if (error.name) parts.push(error.name);
  if (error.code) parts.push(error.code);
  if (error.message) parts.push(error.message);
  if (Array.isArray(error.errors)) {
    for (const item of error.errors) {
      const detail = [item.code, item.address, item.port, item.message].filter(Boolean).join(" ");
      if (detail) parts.push(detail);
    }
  }
  return parts.filter(Boolean).join(": ") || String(error);
}

function cloudRequest(region, method, pathname, body, token) {
  return new Promise((resolve) => {
    const url = new URL(pathname, apiBase(region));
    const payload = body ? Buffer.from(JSON.stringify(body)) : null;
    const site = region === "global" ? "https://bambulab.com" : "https://bambulab.cn";
    const headers = {
      Accept: "application/json",
      "Content-Type": "application/json;charset=UTF-8",
      "Accept-Language": region === "global" ? "en-US,en;q=0.9" : "zh-CN,zh;q=0.9,en;q=0.8",
      Origin: site,
      Referer: `${site}/`,
      "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/125 Safari/537.36"
    };
    if (payload) headers["Content-Length"] = payload.length;
    if (token) headers.Authorization = `Bearer ${token}`;

    const req = https.request(url, { method, headers, timeout: 20000 }, (res) => {
      let text = "";
      res.setEncoding("utf8");
      res.on("data", (chunk) => { text += chunk; });
      res.on("end", () => {
        let json = null;
        try { json = text ? JSON.parse(text) : null; } catch {}
        resolve({ status: res.statusCode || 0, headers: res.headers, json, text });
      });
    });
    req.on("timeout", () => req.destroy(new Error("请求超时")));
    req.on("error", (error) => resolve({ status: 0, error: errorText(error), json: null, text: "" }));
    if (payload) req.write(payload);
    req.end();
  });
}

function findDeep(value, keys) {
  if (!value || typeof value !== "object") return "";
  for (const key of keys) {
    if (Object.prototype.hasOwnProperty.call(value, key) && value[key] != null && value[key] !== "") return String(value[key]);
  }
  for (const child of Object.values(value)) {
    if (child && typeof child === "object") {
      const found = findDeep(child, keys);
      if (found) return found;
    }
  }
  return "";
}

function pickToken(json, headers) {
  return findDeep(json, ["accessToken", "access_token", "token", "idToken"]) ||
    String(headers.authorization || headers.Authorization || "").replace(/^Bearer\s+/i, "");
}

function cloudOk(result) {
  if (!result || result.status < 200 || result.status >= 300) return false;
  if (!result.json || typeof result.json !== "object") return true;
  const code = result.json.code ?? result.json.errorCode ?? result.json.statusCode;
  if (code == null || code === "") return true;
  return code === 0 || code === "0" || code === 200 || code === "200" || code === "SUCCESS";
}

function cloudError(result) {
  if (!result) return "Bambu 云服务没有返回结果";
  if (result.error) return result.error;
  if (result.json && typeof result.json === "object") return result.json.message || result.json.msg || result.json.error || JSON.stringify(result.json);
  return result.text || `HTTP ${result.status}`;
}

async function sendCode(input) {
  const region = input.region || "cn";
  const account = String(input.account || input.phone || input.email || "").trim();
  if (!account) throw new Error("请填写手机号或邮箱");
  const next = readConfig();
  next.cloud.region = region;
  next.cloud.account = account;
  saveConfig(next);

  const isEmail = account.includes("@");
  const pathname = isEmail ? "/v1/user-service/user/sendemail/code" : "/v1/user-service/user/sendsmscode";
  const payloads = isEmail
    ? [{ email: account, type: "codeLogin" }, { account, type: "codeLogin" }, { email: account }]
    : [{ phone: account, type: "codeLogin" }, { account, type: "codeLogin" }, { phone: account }, { mobile: account }];

  let last = null;
  for (const payload of payloads) {
    last = await cloudRequest(region, "POST", pathname, payload);
    if (cloudOk(last)) return { ok: true, status: last.status, message: "验证码已发送，请查看手机或邮箱" };
  }
  throw new Error(`验证码发送失败：${cloudError(last)}`);
}

async function login(input) {
  const region = input.region || "cn";
  const account = String(input.account || input.phone || input.email || "").trim();
  const code = String(input.code || input.verification_code || "").trim();
  if (!account) throw new Error("请填写手机号或邮箱");
  if (!code) throw new Error("请填写验证码");

  const current = readConfig();
  current.cloud.region = region;
  current.cloud.account = account;
  saveConfig(current);

  const tries = account.includes("@")
    ? [{ email: account, code, type: "codeLogin" }, { account, code, type: "codeLogin" }, { account, code }]
    : [{ phone: account, code, type: "codeLogin" }, { account, code, type: "codeLogin" }, { phone: account, code }, { mobile: account, code }, { account, code }];
  let last = null;
  for (const payload of tries) {
    last = await cloudRequest(region, "POST", "/v1/user-service/user/login", payload);
    if (cloudOk(last)) {
      const token = pickToken(last.json, last.headers);
      if (!token) throw new Error("登录成功但没有拿到 access token");
      const next = readConfig();
      next.cloud.region = region;
      next.cloud.account = account;
      next.cloud.access_token = token;
      saveConfig(next);
      await refreshMqttUsername();
      await fetchBindings();
      return { ok: true, status: last.status, token_present: true, mqtt_username: Boolean(readConfig().cloud.mqtt_username) };
    }
  }
  throw new Error(cloudError(last) || `Bambu 云登录失败，HTTP ${last?.status || 0}`);
}

async function refreshMqttUsername() {
  const cfg = readConfig();
  if (!cfg.cloud.access_token) return "";
  const paths = [
    "/v1/design-user-service/my/preference",
    "/v1/user-service/my/profile",
    "/v1/user-service/user/info",
    "/v1/user-service/my/user"
  ];
  for (const pathname of paths) {
    const res = await cloudRequest(cfg.cloud.region, "GET", pathname, null, cfg.cloud.access_token);
    if (res.status >= 200 && res.status < 300) {
      let uid = findDeep(res.json, ["uid", "userId", "user_id", "id"]);
      if (uid && !String(uid).startsWith("u_")) uid = `u_${uid}`;
      if (uid) {
        const next = readConfig();
        next.cloud.mqtt_username = uid;
        saveConfig(next);
        return uid;
      }
    }
  }
  return "";
}

function normalizeDevice(dev) {
  const serial = String(dev.dev_id || dev.serial || dev.device_id || "").trim();
  const rawModel = String(dev.dev_model_name || dev.model || dev.product_name || dev.dev_product_name || dev.productName || "").trim();
  return {
    serial,
    display_name: String(dev.name || dev.dev_name || dev.device_name || serial || "").trim(),
    model: normalizeModelName(rawModel),
    access_code: String(dev.dev_access_code || dev.access_code || dev.lan_access_code || "").trim(),
    online: dev.online !== false,
    print_status: String(dev.print_status || dev.status || "").trim(),
    raw: dev
  };
}

function normalizeModelName(model) {
  const raw = String(model || "").trim();
  const normalized = raw.replace(/[^a-z0-9]/gi, "").toUpperCase();
  if (!normalized) return raw;
  if (normalized.includes("A1MINI") || normalized === "N1") return "A1 mini";
  if (normalized === "A1" || normalized.includes("BAMBULABA1") || normalized === "N2S") return "A1";
  if (normalized.includes("P1P") || normalized === "C11") return "P1P";
  if (normalized.includes("P1S") || normalized === "C12") return "P1S";
  if (normalized.includes("P2S")) return "P2S";
  if (normalized.includes("X2D") || normalized === "N6V2") return "X2D";
  if (normalized.includes("H2DPRO")) return "H2D Pro";
  if (normalized.includes("H2D")) return "H2D";
  if (normalized.includes("H2S")) return "H2S";
  if (normalized.includes("H2C")) return "H2C";
  if (normalized.includes("X1E") || normalized === "AP02") return "X1E";
  if (normalized.includes("X1CARBON") || normalized.includes("X1C") || normalized === "AP05") return "X1C";
  if (normalized.includes("X1")) return "X1";
  return raw;
}

async function fetchBindings() {
  const cfg = readConfig();
  if (!cfg.cloud.access_token) throw new Error("尚未登录 Bambu 云服务");
  const res = await cloudRequest(cfg.cloud.region, "GET", "/v1/iot-service/api/user/bind", null, cfg.cloud.access_token);
  if (res.status < 200 || res.status >= 300) return res;
  const raw = Array.isArray(res.json?.devices) ? res.json.devices : [];
  const devices = raw.map(normalizeDevice).filter((item) => item.serial);
  const snapshot = { updated_at: Date.now(), devices };
  writeJson(DEVICES_FILE, snapshot);
  fs.appendFileSync(HISTORY_FILE, JSON.stringify(snapshot) + "\n", "utf8");
  const esp_printer_sync = await syncPrinterListToEsp(devices).catch((error) => ({ ok: false, error: error.message }));
  return { ok: true, status: res.status, devices, esp_printer_sync };
}

async function selectPrinter(input) {
  await ensureSingleConnectedEsp();
  const body = typeof input === "object" && input !== null ? input : { serial: input };
  const serial = body.serial;
  const expectedEspId = body.active_esp_id ? normalizeEspId(body.active_esp_id) : "";
  const expectedSerialPort = body.serial_port ? normalizeSerialPort(body.serial_port) : "";
  const current = readConfig();
  const targetEspId = expectedEspId || current.active_esp_id;
  if (!current.esp_devices[targetEspId]) {
    throw new Error("没有找到目标 ESP 档案，请先点击“检测当前 ESP”。");
  }
  current.active_esp_id = targetEspId;
  ensureEspProfiles(current);
  ensureWifiSavedForPrinter(current);
  if (expectedSerialPort && expectedSerialPort !== normalizeSerialPort(current.esp.serial_port)) {
    throw new Error(`页面串口是 ${expectedSerialPort}，但目标 ESP 档案串口是 ${current.esp.serial_port}。请先点击“检测当前 ESP”确认目标设备。`);
  }
  const snapshot = readJson(DEVICES_FILE, { devices: [] });
  const device = snapshot.devices.find((item) => item.serial === serial);
  if (!device) throw new Error("没有在已保存的打印机列表中找到这台打印机，请先刷新打印机列表");
  const next = current;
  next.printer.serial = device.serial;
  next.printer.access_code = device.access_code || next.printer.access_code || "";
  next.printer.display_name = device.display_name || device.serial;
  next.printer.model = device.model || "";
  const saved = saveConfig(next);
  const esp_config = await pushConfigToEsp(saved, { skipSingleCheck: true });
  return {
    device,
    target_esp_id: saved.active_esp_id,
    target_serial_port: saved.esp.serial_port,
    target_esp_label: espLabel(saved.esp_devices[saved.active_esp_id]),
    esp_config
  };
}

function ensureWifiSavedForPrinter(cfg = readConfig()) {
  ensureActiveEspSelected(cfg);
  if (!cfg.wifi.ssid || !cfg.wifi.password) {
    throw new Error("请先完成第 2 步：填写 WiFi 并点击“保存并配置 ESP WiFi”，再选择打印机。");
  }
}

function ensureActiveEspSelected(cfg = readConfig()) {
  if (!hasActiveEsp(cfg)) {
    throw new Error("请先连接当前要配置的一台 ESP，并点击“检测当前 ESP”。");
  }
}

async function ensureSingleConnectedEsp() {
  const ports = (await listSerialPortObjects()).map((item) => normalizeSerialPort(item.port)).filter(Boolean);
  const uniquePorts = [...new Set(ports)];
  if (uniquePorts.length <= 1) return;
  const scanResults = await Promise.all(uniquePorts.map(async (port) => ({
    port,
    result: await serialJsonCommand(port, { cmd: "status" }, "\"ok\"", 3000)
  })));
  const detected = scanResults.filter((item) => item.result.ok);
  if (detected.length > 1) {
    throw new Error("检测到多台 ESP 同时连接。请只保留当前要配置的一台 ESP，再继续写入 WiFi 或选择打印机。");
  }
}

function espUrl(pathname, config) {
  const cfg = config || readConfig();
  return `http://${cfg.esp.host}:${Number(cfg.esp.port || 8081)}${pathname}`;
}

function espConfigUrl(payload, config) {
  const params = new URLSearchParams();
  for (const [key, value] of Object.entries(payload)) {
    if (value !== undefined && value !== null && String(value).length) params.set(key, String(value));
  }
  return `${espUrl("/api/config-url", config)}?${params.toString()}`;
}

function compactPrintersForEsp(devices) {
  return (Array.isArray(devices) ? devices : [])
    .filter((item) => item && item.serial)
    .slice(0, 16)
    .map((item) => ({
      serial: String(item.serial || "").trim(),
      name: String(item.display_name || item.name || item.serial || "").trim().slice(0, 40),
      model: String(item.model || "").trim().slice(0, 20)
    }));
}

async function syncPrinterListToEsp(devices, config) {
  const cfg = config ? ensureEspProfiles(mergeConfig(DEFAULT_CONFIG, config)) : readConfig();
  cfg.esp.host = normalizeEspHost(cfg.esp.host || cfg.esp.last_ip);
  const printers = compactPrintersForEsp(devices);
  if (!printers.length) return { ok: false, skipped: true, reason: "no printers" };
  let serialError = "USB 串口未连接";
  const serialPayload = { printers };
  if (Buffer.byteLength(JSON.stringify(serialPayload), "utf8") <= 1900) {
    const ports = await listSerialPorts();
    if (ports.includes(normalizeSerialPort(cfg.esp.serial_port))) {
      const serialResult = await pushConfigToSerial(cfg.esp.serial_port, serialPayload);
      if (serialResult.status) {
        return { ok: true, transport: "serial", target_serial_port: cfg.esp.serial_port, count: printers.length, status: serialResult.status, acknowledged: true };
      }
      serialError = serialResult.error || "ESP 未确认打印机列表";
    }
  } else {
    serialError = "打印机列表超过串口单次写入长度";
  }
  if (!cfg.esp.host) return { ok: false, skipped: true, reason: "no esp host", serial_error: serialError };
  const params = new URLSearchParams();
  params.set("printers_json", JSON.stringify({ printers }));
  const targetUrl = `${espUrl("/api/printers-url", cfg)}?${params.toString()}`;
  const result = await localRequestReliable(targetUrl, "GET", null, 2000);
  return {
    ok: result.status >= 200 && result.status < 300,
    transport: "http",
    target_host: cfg.esp.host,
    target_port: Number(cfg.esp.port || 8081),
    target_url: espUrl("/api/printers-url", cfg),
    count: printers.length,
    status: result.status || 0,
    error: result.error || "",
    serial_error: serialError,
    fallback: result.fallback || ""
  };
}

async function syncSavedPrinterListToEsp(input = {}) {
  const body = input && typeof input === "object" ? input : {};
  const cfg = readConfig();
  const host = normalizeEspHost(body.host);
  if (Object.prototype.hasOwnProperty.call(body, "host")) cfg.esp.host = host;
  if (!cfg.esp.host && cfg.esp.last_ip) cfg.esp.host = normalizeEspHost(cfg.esp.last_ip);
  if (Object.prototype.hasOwnProperty.call(body, "port")) cfg.esp.port = Number(body.port || 8081);
  const saved = saveConfig(cfg);
  const devices = readJson(DEVICES_FILE, { devices: [] }).devices || [];
  const result = await syncPrinterListToEsp(devices, saved);
  return {
    ...result,
    message: result.ok
      ? `已同步 ${result.count} 台打印机到 ESP 本地网页。`
      : `同步失败：${result.reason || result.error || "请确认 ESP 地址和端口是否正确"}`
  };
}

function localRequest(url, method, body, timeoutMs = 1500) {
  return new Promise((resolve) => {
    const payload = body ? Buffer.from(JSON.stringify(body)) : null;
    const u = new URL(url);
    const headers = payload ? { "Content-Type": "application/json", "Content-Length": payload.length, Connection: "close" } : { Connection: "close" };
    const req = http.request(u, { method, headers, timeout: timeoutMs }, (res) => {
      let text = "";
      res.setEncoding("utf8");
      res.on("data", (chunk) => { text += chunk; });
      res.on("end", () => {
        let json = null;
        try { json = text ? JSON.parse(text) : null; } catch {}
        resolve({ status: res.statusCode || 0, json, text });
      });
    });
    req.on("timeout", () => req.destroy(new Error("ESP 请求超时")));
    req.on("error", (error) => resolve({ status: 0, error: error.message }));
    if (payload) req.write(payload);
    req.end();
  });
}

function localRequestViaPowerShell(url, method, body, timeoutMs = 2500) {
  return new Promise((resolve) => {
    const payload = body ? JSON.stringify(body) : "";
    const payloadB64 = Buffer.from(payload, "utf8").toString("base64");
    const timeoutSec = Math.max(1, Math.ceil(timeoutMs / 1000));
    const script = [
      "& { param($uri,$method,$bodyB64,[int]$timeout)",
      "$ErrorActionPreference='Stop'",
      "$body=[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($bodyB64))",
      "$p=@{Uri=$uri;Method=$method;UseBasicParsing=$true;TimeoutSec=$timeout}",
      "if($body){$p.ContentType='application/json';$p.Body=$body}",
      "$r=Invoke-WebRequest @p",
      "$r.Content",
      "}"
    ].join(";");
    execFile("powershell.exe", ["-NoProfile", "-Command", script, url, method, payloadB64, String(timeoutSec)], { timeout: timeoutMs + 1000 }, (error, stdout, stderr) => {
      const text = String(stdout || "").trim();
      if (error) return resolve({ status: 0, error: String(stderr || error.message).trim() });
      let json = null;
      try { json = text ? JSON.parse(text) : null; } catch {}
      resolve({ status: 200, json, text, fallback: "powershell" });
    });
  });
}

async function localRequestReliable(url, method, body, timeoutMs = 1500) {
  const result = await localRequest(url, method, body, timeoutMs);
  if (result.status || !/EACCES/i.test(String(result.error || "")) || process.platform !== "win32") return result;
  return localRequestViaPowerShell(url, method, body, Math.max(timeoutMs + 1500, 3000));
}

function isUsableEspStatus(result) {
  const json = result && result.json;
  if (!result || !result.status || !json || json.ok !== true) return false;
  return String(json.ap_ssid || "").startsWith("PrintSphereLite-") || Object.prototype.hasOwnProperty.call(json, "mqtt_connected");
}

function isWifiConfiguredStatus(json) {
  return Boolean(normalizeEspHost(json && json.ip));
}

function activateEspProfileFromStatus(status, serialPort, host) {
  const source = mergeConfig(DEFAULT_CONFIG, readJson(CONFIG_FILE, DEFAULT_CONFIG));
  const raw = ensureEspProfiles(mergeConfig(DEFAULT_CONFIG, source));
  const detectedId = espIdFromStatus(status);
  const id = detectedId !== "default" ? detectedId : `port-${normalizeSerialPort(serialPort).toLowerCase()}`;
  const existing = raw.esp_devices[id];
  const seedFromSingleEsp = !existing && raw.active_esp_id === "default" && hasSingleEspData(source);
  const seed = seedFromSingleEsp ? {
    wifi: mergeConfig(DEFAULT_CONFIG.wifi, source.wifi),
    printer: mergeConfig(DEFAULT_CONFIG.printer, source.printer),
    display: mergeConfig(DEFAULT_CONFIG.display, source.display),
    esp: mergeConfig(DEFAULT_CONFIG.esp, source.esp)
  } : {};
  const current = mergeConfig(defaultEspProfile(id), existing || seed);
  if (!existing && !current.wifi.ssid && (raw.defaults.wifi.ssid || raw.defaults.wifi.password)) {
    current.wifi = mergeConfig(DEFAULT_CONFIG.wifi, raw.defaults.wifi);
  }
  current.id = id;
  current.device_id = normalizeEspId(status && status.device_id || id);
  current.chip_id = String(status && status.chip_id || current.chip_id || "").trim();
  current.mac = String(status && status.mac || current.mac || "").trim();
  current.label = current.mac ? `ESP ${current.mac}` : (current.chip_id ? `ESP ${current.chip_id}` : `ESP ${normalizeSerialPort(serialPort)}`);
  current.last_seen_at = Date.now();
  current.esp.serial_port = normalizeSerialPort(serialPort || current.esp.serial_port);
  current.esp.port = Number(current.esp.port || 8081);
  if (host) current.esp.host = normalizeEspHost(host);
  const statusHasIp = Boolean(status && Object.prototype.hasOwnProperty.call(status, "ip"));
  const statusIp = normalizeEspHost(status && status.ip);
  if (statusHasIp) {
    current.esp.host = statusIp;
    current.esp.last_ip = statusIp;
  }
  if (status && Object.prototype.hasOwnProperty.call(status, "mqtt_connected")) current.esp.last_error = "";
  current.esp.wifi_status = isWifiConfiguredStatus(status) ? "configured" : (status && status.ap_ssid ? "ap_only" : current.esp.wifi_status);
  current.esp.last_checked_at = Date.now();
  if (status && status.serial) current.printer.serial = String(status.serial);
  if (status && status.name) current.printer.display_name = String(status.name);
  if (status && Object.prototype.hasOwnProperty.call(status, "brightness")) current.display.brightness = normalizeBrightness(status.brightness);
  if (status && Object.prototype.hasOwnProperty.call(status, "brightness_active")) current.display.active_brightness = normalizeBrightness(status.brightness_active);
  if (status && Object.prototype.hasOwnProperty.call(status, "brightness_dimmed")) current.display.dimmed = Boolean(status.brightness_dimmed);
  raw.esp_devices[id] = current;
  raw.active_esp_id = id;
  raw.wifi = mergeConfig(DEFAULT_CONFIG.wifi, current.wifi);
  raw.printer = mergeConfig(DEFAULT_CONFIG.printer, current.printer);
  raw.display = mergeConfig(DEFAULT_CONFIG.display, current.display);
  raw.esp = mergeConfig(DEFAULT_CONFIG.esp, current.esp);
  writeJson(CONFIG_FILE, raw);
  return readConfig();
}

function rememberEspStatus(result, host, error) {
  let cfg = readConfig();
  if (isUsableEspStatus(result)) cfg = activateEspProfileFromStatus(result.json, cfg.esp.serial_port, host);
  cfg.esp.last_checked_at = Date.now();
  if (host) cfg.esp.host = host;
  if (isUsableEspStatus(result)) {
    const ip = normalizeEspHost(result.json.ip);
    cfg.esp.host = normalizeEspHost(cfg.esp.host);
    cfg.esp.last_ip = ip || normalizeEspHost(cfg.esp.last_ip);
    cfg.esp.wifi_status = isWifiConfiguredStatus(result.json) ? "configured" : "ap_only";
    cfg.esp.last_error = "";
  } else {
    cfg.esp.wifi_status = "unknown";
    cfg.esp.last_error = error || result?.error || "没有检测到 ESP HTTP 状态";
  }
  saveConfig(cfg);
  return cfg.esp;
}

function localLanPrefixes() {
  const prefixes = [];
  for (const [name, list] of Object.entries(os.networkInterfaces())) {
    if (/virtual|vmware|virtualbox|vbox|vethernet|loopback|bluetooth/i.test(name)) continue;
    for (const item of list || []) {
      if (item.family !== "IPv4" || item.internal || !isPrivateIpv4(item.address)) continue;
      if (/^192\.168\.56\./.test(item.address)) continue;
      const parts = item.address.split(".");
      prefixes.push(parts.slice(0, 3).join("."));
    }
  }
  return [...new Set(prefixes)];
}

async function scanForEsp(port) {
  const prefixes = localLanPrefixes();
  for (const prefix of prefixes) {
    const hosts = Array.from({ length: 254 }, (_, i) => `${prefix}.${i + 1}`);
    let index = 0;
    let found = null;
    async function worker() {
      while (!found && index < hosts.length) {
        const host = hosts[index++];
        const result = await localRequest(`http://${host}:${port}/api/status`, "GET", null, 350);
        if (isUsableEspStatus(result)) found = { host, result };
      }
    }
    await Promise.all(Array.from({ length: 32 }, worker));
    if (found) return found;
  }
  return null;
}

async function checkEspStatusViaSerial(port) {
  const serialPort = normalizeSerialPort(port || readConfig().esp.serial_port);
  const result = await serialJsonCommand(serialPort, { cmd: "status" }, "\"ok\"", 9000);
  if (!result.ok) {
    return {
      ok: false,
      source: "serial",
      port: serialPort,
      error: result.error || "USB 串口没有响应"
    };
  }
  const cfg = activateEspProfileFromStatus(result.json, serialPort, "");
  const ip = normalizeEspHost(result.json && result.json.ip);
  return {
    ok: Boolean(ip),
    source: "serial",
    port: serialPort,
    status: result.json,
    message: ip ? `ESP 已通过 USB 检测到 WiFi IP：${ip}` : "ESP USB 可通信，但还没有连上 WiFi。请确认 WiFi 名称和密码是否正确，等待 10-20 秒后再检测。",
    esp: cfg.esp
  };
}

async function checkEspStatus({ discover = false } = {}) {
  const cfg = readConfig();
  const port = Number(cfg.esp.port || 8081);
  if (cfg.esp.host) {
    const result = await localRequestReliable(`http://${cfg.esp.host}:${port}/api/status`, "GET", null, 1200);
    if (isUsableEspStatus(result)) {
      const esp = rememberEspStatus(result, cfg.esp.host);
      const ip = normalizeEspHost(result.json && result.json.ip);
      return { ok: Boolean(ip), source: "http", host: cfg.esp.host, status: result.json, message: ip ? `ESP 已通过 HTTP 检测到 WiFi IP：${ip}` : "ESP HTTP 已响应，但还没有连上 WiFi。", esp };
    }
    if (!discover) return { ok: false, source: "http", host: cfg.esp.host, error: result.error || "ESP HTTP 无响应", message: result.error || "ESP HTTP 无响应", esp: rememberEspStatus(result, "", result.error) };
  }
  const serial = await checkEspStatusViaSerial(cfg.esp.serial_port);
  if (serial.ok || serial.status || !discover) return serial;
  if (discover) {
    const found = await scanForEsp(port);
    if (found) {
      const esp = rememberEspStatus(found.result, found.host);
      const ip = normalizeEspHost(found.result.json && found.result.json.ip);
      return { ok: Boolean(ip), source: "discover", host: found.host, status: found.result.json, discovered: true, message: ip ? `局域网已找到 ESP：${ip}` : "局域网找到 ESP，但它还没有连上 WiFi。", esp };
    }
  }
  const error = serial.error ? `USB 串口检测失败：${serial.error}；局域网自动搜索也没有找到设备` : (cfg.esp.host ? "ESP 地址无响应，局域网自动搜索也没有找到设备" : "未填写 ESP 地址，局域网自动搜索没有找到设备");
  return { ok: false, source: "detect", error, message: error, esp: rememberEspStatus(null, "", error) };
}

function espScanCandidates() {
  const cfg = readConfig();
  const port = Number(cfg.esp.port || 8081);
  const hosts = [cfg.esp.host, cfg.esp.last_ip, "192.168.4.1"]
    .map((host) => normalizeEspHost(host))
    .filter(Boolean);
  return [...new Set(hosts)].map((host) => ({ host, port }));
}

async function listEspWifiNetworks() {
  const errors = [];
  for (const item of espScanCandidates()) {
    const result = await localRequestReliable(`http://${item.host}:${item.port}/api/wifi/scan`, "GET", null, 9000);
    const networks = normalizeWifiNetworks(result.json && result.json.networks);
    if (result.status && result.json && result.json.ok === true) {
      return {
        ok: true,
        source: "esp",
        host: item.host,
        networks,
        count: networks.length,
        message: networks.length ? `ESP 已扫描到 ${networks.length} 个 WiFi` : "ESP 没有扫描到 WiFi，可直接手动输入 WiFi 名称。"
      };
    }
    errors.push(`${item.host}:${item.port} ${result.error || result.status || "无响应"}`);
  }
  return { ok: false, source: "esp", networks: [], count: 0, message: "没有连接到 ESP 的配置接口", error: errors.join("; ") };
}

let serialCommandQueue = Promise.resolve();

function queueSerialCommand(task) {
  const result = serialCommandQueue.then(task, task);
  serialCommandQueue = result.then(() => undefined, () => undefined);
  return result;
}

function serialJsonCommand(serialPort, payloadObject, marker, timeoutMs = 35000) {
  return queueSerialCommand(() => new Promise((resolve) => {
    const portName = normalizeSerialPort(serialPort);
    const payload = JSON.stringify(payloadObject);
    const payloadB64 = Buffer.from(payload, "utf8").toString("base64");
    const script = [
      "& { param($portName,$payloadB64,$marker,[int]$timeoutMs)",
      "$ErrorActionPreference='Stop'",
      "$payload=[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($payloadB64))",
      "$port=New-Object System.IO.Ports.SerialPort $portName,115200,None,8,one",
      "$port.ReadTimeout=500;$port.WriteTimeout=1000;$port.DtrEnable=$false;$port.RtsEnable=$false",
      "$deadline=[DateTime]::Now.AddMilliseconds($timeoutMs)",
      "try{",
      "  $port.Open();Start-Sleep -Milliseconds 1200;$port.DiscardInBuffer();$port.WriteLine($payload)",
      "  while([DateTime]::Now -lt $deadline){",
      "    try{$line=$port.ReadLine().Trim();if($line.StartsWith('{') -and (!$marker -or $line.Contains($marker))){Write-Output $line;break}}catch [TimeoutException]{}",
      "  }",
      "}finally{if($port.IsOpen){$port.Close()}}",
      "}"
    ].join(";");
    execFile("powershell.exe", ["-NoProfile", "-Command", script, portName, payloadB64, marker || "", String(timeoutMs)], { timeout: timeoutMs + 3000, maxBuffer: 1024 * 1024 }, (error, stdout, stderr) => {
      const text = String(stdout || "").trim();
      if (error || !text) {
        return resolve({
          ok: false,
          port: portName,
          text,
          error: String(stderr || error?.message || "串口无响应").trim()
        });
      }
      let json = null;
      try { json = JSON.parse(text.split(/\r?\n/).find((line) => line.trim().startsWith("{") && line.includes("networks")) || text); } catch (parseError) {
        try { json = JSON.parse(text.split(/\r?\n/).find((line) => line.trim().startsWith("{")) || text); } catch {
          return resolve({ ok: false, port: portName, error: parseError.message, text });
        }
      }
      resolve({ ok: true, port: portName, json, text });
    });
  }));
}

async function readEspStatusViaSerial(port) {
  const serialPort = normalizeSerialPort(port || readConfig().esp.serial_port);
  const result = await serialJsonCommand(serialPort, { cmd: "status" }, "\"ok\"", 9000);
  if (!result.ok) return { ok: false, port: serialPort, error: result.error || "串口无响应" };
  const cfg = activateEspProfileFromStatus(result.json, serialPort, "");
  return { ok: true, port: serialPort, status: result.json, active_esp_id: cfg.active_esp_id, active_esp_label: espLabel(cfg.esp_devices[cfg.active_esp_id]), esp: cfg.esp };
}

async function autoDetectEspPorts() {
  const before = readConfig();
  const originalActive = before.active_esp_id;
  const ports = (await listSerialPortObjects()).map((item) => normalizeSerialPort(item.port)).filter(Boolean);
  const uniquePorts = [...new Set(ports)];
  const results = [];
  const detectedIds = [];

  const scanResults = await Promise.all(uniquePorts.map(async (port) => ({
    port,
    result: await serialJsonCommand(port, { cmd: "status" }, "\"ok\"", 6500)
  })));
  for (const item of scanResults) {
    const { port, result } = item;
    if (result.ok) {
      const cfg = activateEspProfileFromStatus(result.json, port, "");
      const id = cfg.active_esp_id;
      detectedIds.push(id);
      results.push({
        ok: true,
        port,
        active_esp_id: id,
        label: espLabel(cfg.esp_devices[id]),
        mac: result.json && result.json.mac || "",
        chip_id: result.json && result.json.chip_id || ""
      });
    } else {
      results.push({ ok: false, port, error: result.error || "串口无响应" });
    }
  }

  const cfg = readConfig();
  const detectedByPort = new Map(results.filter((item) => item.ok).map((item) => [item.port, item.active_esp_id]));
  for (const item of results.filter((item) => !item.ok)) {
    for (const profile of Object.values(cfg.esp_devices || {})) {
      if (!profile || profile.id === "default") continue;
      if (normalizeSerialPort(profile.esp && profile.esp.serial_port) !== item.port) continue;
      if (detectedIds.includes(profile.id)) continue;
      profile.esp.last_error = item.error || "串口无响应";
      profile.esp.last_checked_at = Date.now();
    }
  }
  for (const profile of Object.values(cfg.esp_devices || {})) {
    if (!profile || profile.id === "default" || detectedIds.includes(profile.id)) continue;
    const port = normalizeSerialPort(profile.esp && profile.esp.serial_port);
    const detectedId = detectedByPort.get(port);
    if (!uniquePorts.includes(port)) profile.esp.last_error = "当前未连接";
    else if (detectedId && detectedId !== profile.id) profile.esp.last_error = `当前 ${port} 检测到的是另一台 ESP`;
    else continue;
    profile.esp.last_checked_at = Date.now();
  }

  const okResults = results.filter((item) => item.ok);
  if (okResults.length === 1) cfg.active_esp_id = okResults[0].active_esp_id;
  else if (cfg.esp_devices[originalActive]) cfg.active_esp_id = originalActive;
  else if (okResults[0]) cfg.active_esp_id = okResults[0].active_esp_id;
  ensureEspProfiles(cfg);
  writeJson(CONFIG_FILE, cfg);
  const message = okResults.length > 1
    ? "检测到多台 ESP。为避免写错设备，请只保留当前要配置的一台 ESP 连接电脑，然后重新自动检测。"
    : (okResults.length === 1 ? `已自动选择 ${okResults[0].port} 上的 ESP。` : "没有检测到 ESP，请确认只连接当前要配置的一台 ESP。");
  return {
    ok: okResults.length <= 1,
    single_device_ready: okResults.length === 1,
    multiple_devices: okResults.length > 1,
    message,
    ports: uniquePorts,
    results,
    detected: okResults,
    active_esp_id: readConfig().active_esp_id,
    config: publicConfig()
  };
}

function activateEspProfile(id) {
  const cfg = readConfig();
  const cleanId = normalizeEspId(id);
  if (!cfg.esp_devices[cleanId]) throw new Error("没有找到这个 ESP 设备档案，请先点击“检测当前 ESP”");
  cfg.active_esp_id = cleanId;
  const profile = cfg.esp_devices[cleanId];
  cfg.wifi = mergeConfig(DEFAULT_CONFIG.wifi, profile.wifi);
  cfg.printer = mergeConfig(DEFAULT_CONFIG.printer, profile.printer);
  cfg.esp = mergeConfig(DEFAULT_CONFIG.esp, profile.esp);
  saveConfig(cfg);
  const next = readConfig();
  return { ok: true, active_esp_id: next.active_esp_id, active_esp_label: espLabel(next.esp_devices[next.active_esp_id]), esp: next.esp, printer: next.printer };
}

async function listSerialWifiNetworks(port) {
  const cfg = readConfig();
  const serialPort = normalizeSerialPort(port || cfg.esp.serial_port);
  const result = await serialJsonCommand(serialPort, { cmd: "wifi_scan" }, "networks", 35000);
  if (!result.ok) {
    return {
      ok: false,
      source: "esp-serial",
      networks: [],
      count: 0,
      message: `没有通过 USB 串口 ${serialPort} 获取到 ESP WiFi 列表`,
      error: result.error || "串口无响应"
    };
  }
  const networks = normalizeWifiNetworks(result.json.networks);
  return {
    ok: true,
    source: "esp-serial",
    port: serialPort,
    networks,
    count: networks.length,
    message: networks.length ? `ESP 通过 USB 串口扫描到 ${networks.length} 个 WiFi` : "ESP 通过 USB 串口没有扫描到 WiFi，可直接手动输入 WiFi 名称。"
  };
}

async function listWifiNetworks(port) {
  const espResult = await listEspWifiNetworks();
  if (espResult.ok) return espResult;

  const serialResult = await listSerialWifiNetworks(port);
  if (serialResult.ok) return serialResult;

  const pcResult = await listPcWifiNetworks();
  if (pcResult.networks && pcResult.networks.length) {
    return {
      ...pcResult,
      message: `没有从 ESP HTTP 或 USB 串口获取到 WiFi 列表，已临时使用电脑扫描结果。建议确认 ESP 已连接 USB 后重新扫描。`
    };
  }
  return {
    ...pcResult,
    ok: false,
    source: "esp",
    message: `${espResult.message}；${serialResult.message}。请确认 ESP 已连接 USB 并选择正确串口，或连接 ESP 热点 PrintSphereLite-xxxx 后重新扫描；也可以直接手动输入 WiFi 名称。`
  };
}

function normalizeSerialPort(port) {
  const raw = String(port || "").trim().toUpperCase();
  return /^COM\d+$/.test(raw) ? raw : "COM7";
}

function listSerialPortObjects() {
  return new Promise((resolve) => {
    const script = [
      "$ErrorActionPreference='SilentlyContinue'",
      "$map=@{}",
      "Get-CimInstance Win32_SerialPort | ForEach-Object {",
      "  $p=([string]$_.DeviceID).ToUpperInvariant()",
      "  if($p -match '^COM\\d+$'){ $map[$p]=($(if($_.Description){$_.Description}else{$_.Name})) }",
      "}",
      "$mode=cmd.exe /c mode",
      "[regex]::Matches($mode,'COM\\d+') | ForEach-Object {",
      "  $p=$_.Value.ToUpperInvariant()",
      "  if(-not $map.ContainsKey($p)){ $map[$p]='' }",
      "}",
      "$ports=$map.Keys | Sort-Object {[int]($_ -replace '^COM','')} | ForEach-Object { [pscustomobject]@{ port=$_; name=$map[$_] } }",
      "$ports | ConvertTo-Json -Compress"
    ].join(";");
    execFile("powershell.exe", ["-NoProfile", "-Command", script], { timeout: 8000, maxBuffer: 1024 * 1024 }, (error, stdout) => {
      if (!error) {
        try {
          const parsed = JSON.parse(String(stdout || "[]").trim() || "[]");
          const list = (Array.isArray(parsed) ? parsed : [parsed])
            .filter((item) => item && item.port)
            .map((item) => ({ port: normalizeSerialPort(item.port), name: String(item.name || "串口设备") }))
            .sort((a, b) => Number(a.port.slice(3)) - Number(b.port.slice(3)));
          if (list.length) return resolve(list);
        } catch {}
      }
      execFile("cmd.exe", ["/c", "mode"], { timeout: 5000 }, (modeError, modeStdout) => {
        if (modeError) return resolve([]);
        const ports = Array.from(new Set(String(modeStdout).match(/COM\d+/gi) || []))
          .map((port) => normalizeSerialPort(port))
          .sort((a, b) => Number(a.slice(3)) - Number(b.slice(3)))
          .map((port) => ({ port, name: "" }));
        resolve(ports);
      });
    });
  });
}

async function listSerialPorts() {
  return (await listSerialPortObjects()).map((item) => item.port);
}

function parseWifiNetworks(output) {
  const seen = new Map();
  let current = null;
  for (const line of String(output || "").split(/\r?\n/)) {
    const ssidMatch = line.match(/^\s*SSID\s+\d+\s*:\s*(.*)\s*$/i);
    if (ssidMatch) {
      const ssid = ssidMatch[1].trim();
      current = null;
      if (ssid) {
        if (!seen.has(ssid)) seen.set(ssid, { ssid, signal: 0, bssid_count: 0 });
        current = seen.get(ssid);
      }
      continue;
    }
    if (!current) continue;
    if (/^\s*BSSID\s+\d+\s*:/i.test(line)) current.bssid_count += 1;
    const signalMatch = line.match(/^\s*(?:Signal|信号)\s*:\s*(\d+)\s*%/i);
    if (signalMatch) current.signal = Math.max(current.signal, Number(signalMatch[1]) || 0);
  }
  return Array.from(seen.values())
    .sort((a, b) => (b.signal - a.signal) || a.ssid.localeCompare(b.ssid, "zh-CN"));
}

function rssiToSignal(rssi) {
  const value = Number(rssi);
  if (!Number.isFinite(value)) return 0;
  if (value <= -100) return 0;
  if (value >= -50) return 100;
  return Math.round(2 * (value + 100));
}

function normalizeWifiNetworks(networks) {
  const seen = new Map();
  for (const item of networks || []) {
    const ssid = String(item && item.ssid || "").trim();
    if (!ssid) continue;
    const signal = Number(item.signal) || rssiToSignal(item.rssi);
    const current = seen.get(ssid);
    if (!current || signal > current.signal) seen.set(ssid, { ssid, signal });
  }
  return Array.from(seen.values())
    .sort((a, b) => (b.signal - a.signal) || a.ssid.localeCompare(b.ssid, "zh-CN"));
}

function wifiScanErrorMessage(error, stdout, stderr) {
  const text = `${stdout || ""}\n${stderr || ""}\n${error && error.message ? error.message : ""}`;
  if (/location|位置|privacy|隐私/i.test(text)) return "自动扫描需要 Windows 允许桌面应用访问位置服务；也可以直接手动输入 WiFi 名称。";
  if (/elevation|administrator|管理员|提升/i.test(text)) return "自动扫描被 Windows 权限拦截；可以用管理员身份重新打开配置工具，或直接手动输入 WiFi 名称。";
  if (/wireless|wlan|无线/i.test(text)) return "没有读取到无线网卡信息；如果这台电脑没有 WiFi 网卡，请直接手动输入 WiFi 名称。";
  return "WiFi 扫描失败，可直接手动输入 WiFi 名称。";
}

function listPcWifiNetworks() {
  return new Promise((resolve) => {
    const command = "chcp 65001>nul & netsh wlan show networks mode=bssid";
    execFile("cmd.exe", ["/d", "/s", "/c", command], { timeout: 12000, maxBuffer: 1024 * 1024 }, (error, stdout, stderr) => {
      if (error) {
        return resolve({
          ok: false,
          networks: [],
          message: wifiScanErrorMessage(error, stdout, stderr),
          error: error.message
        });
      }
      const networks = parseWifiNetworks(stdout);
      resolve({
        ok: true,
        source: "computer",
        networks,
        count: networks.length,
        message: networks.length ? `电脑已扫描到 ${networks.length} 个 WiFi` : "电脑未扫描到 WiFi，可直接手动输入 WiFi 名称。"
      });
    });
  });
}

async function pushConfigToSerial(port, payload) {
  const serialPort = normalizeSerialPort(port);
  const result = await serialJsonCommand(serialPort, payload, "\"ok\"", 10000);
  if (!result.ok) return { status: 0, error: result.error || "ESP 未确认收到串口配置" };
  if (!result.json || result.json.ok !== true) {
    return { status: 0, error: result.json && result.json.error || "ESP 拒绝了串口配置" };
  }
  return { status: 200, json: result.json, port: serialPort, baud: 115200, acknowledged: true };
}

async function pushConfigToEsp(config, options = {}) {
  if (!options.skipSingleCheck) await ensureSingleConnectedEsp();
  const cfg = config ? ensureEspProfiles(mergeConfig(DEFAULT_CONFIG, config)) : readConfig();
  ensureActiveEspSelected(cfg);
  if (!cfg.cloud.access_token) throw new Error("尚未登录 Bambu 云服务");
  if (!cfg.printer.serial) throw new Error("尚未选择打印机");
  if (!cfg.wifi.ssid || !cfg.wifi.password) throw new Error("尚未配置 WiFi，请先完成第 2 步。");
  let username = cfg.cloud.mqtt_username;
  if (!username) username = await refreshMqttUsername();
  if (!username) throw new Error("没有获取到 MQTT 用户名，请重新登录 Bambu 云服务");
  const payload = {
    wifi_ssid: cfg.wifi.ssid,
    wifi_password: cfg.wifi.password,
    region: cfg.cloud.region,
    mqtt_host: mqttHost(cfg.cloud.region),
    mqtt_username: username,
    token: cfg.cloud.access_token,
    serial: cfg.printer.serial,
    name: cfg.printer.display_name || cfg.printer.serial,
    brightness: normalizeBrightness(cfg.display.brightness)
  };
  const serialResult = await pushConfigToSerial(cfg.esp.serial_port, payload);
  if (serialResult.status) {
    const devices = readJson(DEVICES_FILE, { devices: [] }).devices || [];
    const printer_sync = await syncPrinterListToEsp(devices, cfg).catch((error) => ({ ok: false, error: error.message }));
    return { ok: true, transport: "serial", target_esp_id: cfg.active_esp_id, target_serial_port: cfg.esp.serial_port, target_host: cfg.esp.host || "", printer_sync, ...serialResult };
  }
  if (cfg.esp.host) {
    const targetUrl = espConfigUrl(payload, cfg);
    const httpResult = await localRequestReliable(targetUrl, "GET", null, 1500);
    if (httpResult.status >= 200 && httpResult.status < 300) {
      const devices = readJson(DEVICES_FILE, { devices: [] }).devices || [];
      const printer_sync = await syncPrinterListToEsp(devices, cfg).catch((error) => ({ ok: false, error: error.message }));
      return { ok: true, transport: "http", target_esp_id: cfg.active_esp_id, target_serial_port: cfg.esp.serial_port, target_host: cfg.esp.host, target_url: targetUrl.split("?")[0], printer_sync, ...httpResult };
    }
  }
  if (cfg.esp.host) return { ok: false, transport: "failed", target_esp_id: cfg.active_esp_id, target_serial_port: cfg.esp.serial_port, target_host: cfg.esp.host, serial_error: serialResult.error, http_error: "ESP HTTP 写入失败" };
  return { ok: false, transport: "failed", target_esp_id: cfg.active_esp_id, target_serial_port: cfg.esp.serial_port, target_host: "", serial_error: serialResult.error };
}

async function pushWifiToEsp(options = {}) {
  if (!options.skipSingleCheck) await ensureSingleConnectedEsp();
  const cfg = readConfig();
  ensureActiveEspSelected(cfg);
  if (!cfg.wifi.ssid || !cfg.wifi.password) throw new Error("请填写 WiFi 名称和密码");
  const payload = {
    wifi_ssid: cfg.wifi.ssid,
    wifi_password: cfg.wifi.password,
    brightness: normalizeBrightness(cfg.display.brightness)
  };
  const serialResult = await pushConfigToSerial(cfg.esp.serial_port, payload);
  if (serialResult.status) return { ok: true, transport: "serial", ...serialResult };
  if (cfg.esp.host) {
    const httpResult = await localRequestReliable(espConfigUrl(payload, cfg), "GET", null, 1500);
    if (httpResult.status >= 200 && httpResult.status < 300) return { ok: true, transport: "http", ...httpResult };
  }
  if (cfg.esp.host) return { ok: false, transport: "failed", serial_error: serialResult.error, http_error: "ESP HTTP 写入失败" };
  return { ok: false, transport: "failed", serial_error: serialResult.error };
}

async function pushBrightnessToEsp(options = {}) {
  if (!options.skipSingleCheck) await ensureSingleConnectedEsp();
  const cfg = readConfig();
  ensureActiveEspSelected(cfg);
  const payload = { brightness: normalizeBrightness(cfg.display.brightness) };
  const serialResult = await pushConfigToSerial(cfg.esp.serial_port, payload);
  if (serialResult.status) return { ok: true, transport: "serial", ...serialResult };
  if (cfg.esp.host) {
    const httpResult = await localRequestReliable(espConfigUrl(payload, cfg), "GET", null, 1500);
    if (httpResult.status >= 200 && httpResult.status < 300) return { ok: true, transport: "http", ...httpResult };
    return { ok: false, transport: "failed", serial_error: serialResult.error, http_error: httpResult.error || `HTTP ${httpResult.status || 0}` };
  }
  return { ok: false, transport: "failed", serial_error: serialResult.error };
}

async function configureWifiAndDetect(options = {}) {
  const cfg = readConfig();
  const hasPrinterAndCloud = Boolean(cfg.cloud.access_token && cfg.printer.serial);
  const write = hasPrinterAndCloud ? await pushConfigToEsp(null, options) : await pushWifiToEsp(options);
  await new Promise((resolve) => setTimeout(resolve, write.transport === "serial" ? 3500 : 1200));
  const detection = await checkEspStatus({ discover: true });
  const writeText = write.ok ? `WiFi 配置已通过 ${write.transport === "serial" ? "USB 串口" : "ESP HTTP"} 写入。` : `WiFi 写入失败：${write.serial_error || write.http_error || "未知错误"}`;
  const detectText = detection.ok ? (detection.message || "ESP WiFi 已配置成功。") : (detection.message || detection.error || "ESP WiFi 状态未确认。");
  return {
    ok: Boolean(write.ok),
    message: `${writeText}${detectText}`,
    write,
    detection
  };
}

function html() {
  const urls = serviceUrls();
  const urlLine = urls.length > 1 ? `电脑本机：${urls[0]}　手机同 WiFi：${urls.slice(1).join(" 或 ")}` : `电脑本机：${urls[0]}`;
  return `<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>PrintSphere Lite 配置工具</title><style>
body{margin:0;font-family:system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;background:#f6f7f8;color:#1f2933}
main{max-width:1080px;margin:0 auto;padding:24px}h1{font-size:24px;margin:0 0 18px}.grid{display:grid;grid-template-columns:1fr 1fr;gap:16px}
section{background:white;border:1px solid #dde2e7;border-radius:8px;padding:16px}label{display:block;font-size:13px;color:#52606d;margin:10px 0 4px}
input,select{width:100%;box-sizing:border-box;border:1px solid #ccd3db;border-radius:6px;padding:9px 10px;font-size:14px;background:white}
button{border:0;border-radius:6px;background:#1f7a4d;color:white;padding:9px 12px;font-size:14px;cursor:pointer;margin:8px 8px 0 0}
button.secondary{background:#52606d}button:disabled{background:#a7b0ba;cursor:not-allowed}.muted{color:#718096;font-size:13px}.warn{color:#b45309}.ok{color:#137333}.bad{color:#b42318}
.fieldrow{display:grid;grid-template-columns:1fr auto;gap:8px;align-items:end}.fieldrow button{margin:0;white-space:nowrap}
.profilebar{background:white;border:1px solid #dde2e7;border-radius:8px;padding:12px;margin:0 0 16px}.profilebar b{font-size:14px}.profilebar span{font-size:13px;color:#52606d}
.steps{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;margin:14px 0 16px}.step{background:white;border:1px solid #dde2e7;border-radius:8px;padding:12px}.step b{display:block;font-size:14px}.step span{font-size:13px;color:#718096}.step.done{border-color:#84c89a;background:#f2fbf5}.step.active{border-color:#f2b84b;background:#fff8e7}
table{width:100%;border-collapse:collapse;margin-top:10px}
td,th{border-bottom:1px solid #edf0f2;text-align:left;padding:8px;font-size:13px}
pre{white-space:pre-wrap;background:#101820;color:#d9e2ec;border-radius:8px;padding:12px;min-height:80px;max-height:260px;overflow:auto}
@media(max-width:760px){.grid,.fieldrow{grid-template-columns:1fr}main{padding:14px}.fieldrow button{margin-top:8px}}
</style></head><body><main><h1>PrintSphere Lite 配置工具</h1><p class="muted">${urlLine}　版本：${BACKEND_VERSION}</p>
<div class="steps">
<div id="stepCloud" class="step"><b>1. Bambu 云服务登录</b><span>先获取账号 token</span></div>
<div id="stepWifi" class="step"><b>2. 配置 ESP WiFi</b><span>写入并检测 ESP</span></div>
<div id="stepPrinter" class="step"><b>3. 选择显示打印机</b><span>最后写入云 MQTT 配置</span></div>
</div>
<div class="profilebar"><b>当前 ESP 设备</b><span id="activeEspLine">未读取</span>
<label>已保存的 ESP 设备</label><div class="fieldrow"><select id="espProfileList" onchange="activateEsp()"><option value="">暂无设备档案</option></select><button class="secondary" onclick="autoDetectEsp(true)">检测当前 ESP</button></div>
<p id="autoDetectLine" class="muted">打开页面后会自动检测当前 ESP。配置时请只连接当前要配置的一台 ESP。</p>
<p class="muted">多台 ESP 共用同一个 Bambu 云账号和默认 WiFi；每次只连接一台 ESP 配置，配置完拔掉再插下一台。</p></div>
<div class="grid"><section><h2>1. Bambu 云服务登录</h2>
<label>区域</label><select id="region"><option value="cn">中国区</option><option value="global">国际区</option></select>
<label>手机号或邮箱</label><input id="account" placeholder="手机号或邮箱">
<label>验证码</label><input id="code" placeholder="收到验证码后填写">
<button onclick="sendCode()">发送验证码</button><button onclick="login()">登录并读取打印机</button>
<p class="muted">电脑端用于登录、选择打印机和写入配置；ESP 保存 token 后会自己连接 Bambu 云 MQTT。</p></section>
<section><h2>2. WiFi 和 ESP 写入方式</h2>
<label>ESP 扫描到的附近 WiFi</label><div class="fieldrow"><select id="wifiList" onchange="chooseWifi()"><option value="">正在扫描附近 WiFi...</option></select><button class="secondary" onclick="scanWifi(true)">重新扫描 WiFi</button></div>
<p id="wifiScanLine" class="muted">插着 USB 可通过串口扫描；也可以连接 ESP 热点或填写 ESP 地址后扫描。下方始终支持手动输入。</p>
<label>手动输入 WiFi 名称</label><input id="wifiSsid" placeholder="2.4G WiFi SSID">
<label>WiFi 密码</label><input id="wifiPassword" type="password" placeholder="留空则不修改已保存密码">
<label>串口</label><input id="serialPort" placeholder="COM7">
<label>检测到的串口</label><select id="serialList" onchange="chooseSerial()"><option value="">请先刷新串口</option></select>
<button class="secondary" onclick="loadPorts(true)">重新检测 ESP</button><button class="secondary" onclick="readEspDevice()">高级：读取当前串口</button>
<label>ESP 地址，可选</label><input id="espHost" placeholder="同 WiFi 填 192.168.x.x；连接 ESP 热点填 192.168.4.1">
<p class="muted">ESP 已连上 WiFi 后优先填 ESP 地址；否则用 USB 串口写入配置。未完成这一步前不能选择打印机。</p>
<label>ESP 端口，可选</label><input id="espPort" placeholder="8081">
<label>屏幕亮度</label><select id="brightness"><option value="25">25%</option><option value="50">50%</option><option value="75">75%</option><option value="100">100%</option></select>
<button id="brightnessBtn" onclick="applyBrightnessSetting()">应用屏幕亮度</button><button id="saveSettingsBtn" onclick="saveEsp(false)">只保存写入设置</button><button id="configureWifiBtn" onclick="saveEsp(true)">保存并配置 ESP WiFi</button><button id="detectWifiBtn" class="secondary" onclick="checkEsp(true)">检测 ESP WiFi</button><button id="pushConfigBtn" onclick="pushConfig()">同步云配置到 ESP</button>
<p id="brightnessLine" class="muted"></p><p id="espLine" class="muted"></p><p id="wifiLine" class="muted"></p></section></div>
<section style="margin-top:16px"><h2>3. 打印机列表</h2><button id="loadDevicesBtn" onclick="loadDevices()">刷新打印机列表</button><button id="syncPrinterListBtn" class="secondary" onclick="syncPrinterList()">同步打印机列表到 ESP 本地网页</button><p class="muted">如果 ESP 自己的局域网页面显示“暂无已同步打印机”，请先确认上方 ESP 地址正确，再点击同步。</p><div id="devices"></div></section>
<section style="margin-top:16px"><h2>当前配置</h2><div id="statusLine"></div><pre id="log"></pre></section>
</main><script>
const $=id=>document.getElementById(id);
let multiEspBlocked=false;
let operationBusy=false;
let autoDetectBusy=false;
function log(x){$("log").textContent=(typeof x==="string"?x:JSON.stringify(x,null,2));}
async function api(path,opt={}){const r=await fetch(path,{headers:{"content-type":"application/json"},...opt});const t=await r.text();let j;try{j=JSON.parse(t)}catch{j={text:t}};if(!r.ok)throw j;return j}
function setBusy(ids,busy){operationBusy=busy;ids.forEach(id=>{const el=$(id);if(el)el.disabled=busy;});}
function setActionState(c){const espReady=!!(c&&c.steps&&c.steps.esp_ready)&&!multiEspBlocked;const canPush=!!(c&&c.steps&&c.steps.ready_to_select_printer&&c.steps.printer_selected)&&!multiEspBlocked;if($("configureWifiBtn"))$("configureWifiBtn").disabled=!espReady;if($("brightnessBtn"))$("brightnessBtn").disabled=!espReady;if($("saveSettingsBtn"))$("saveSettingsBtn").disabled=multiEspBlocked;if($("pushConfigBtn"))$("pushConfigBtn").disabled=!canPush;}
function setIdleValue(id,value){const el=$(id);if(!el)return;if(document.activeElement===el)return;if((value===undefined||value===null||value==="")&&el.value)return;el.value=value||"";}
function setStep(id,done,active){const el=$(id);if(!el)return;el.className="step"+(done?" done":"")+(active?" active":"");}
function wifiStatusText(c){if(c.esp.wifi_status==="configured")return "WiFi 已配置"+(c.esp.last_ip?"，ESP IP："+c.esp.last_ip:"");if(c.esp.wifi_status==="ap_only")return "ESP 已响应，但还没有连上 WiFi";if(c.esp.last_error)return "WiFi 状态未确认："+c.esp.last_error;return "WiFi 状态未检测";}
function updateSteps(c){setStep("stepCloud",c.steps.cloud_ready,!c.steps.cloud_ready);setStep("stepWifi",c.steps.esp_wifi_configured,c.steps.cloud_ready&&!c.steps.esp_wifi_configured);setStep("stepPrinter",c.steps.printer_selected,c.steps.cloud_ready&&c.steps.esp_ready&&c.steps.wifi_saved&&!c.steps.printer_selected);const btn=$("loadDevicesBtn");if(btn)btn.disabled=!c.steps.cloud_ready;}
function renderEspProfiles(c){const list=$("espProfileList");if(!list)return;const profiles=c.esp_profiles||[];list.innerHTML=(profiles.length?profiles:[{id:"",label:"暂无设备档案"}]).map(p=>"<option value='"+esc(p.id)+"'"+(p.id===c.active_esp_id?" selected":"")+">"+esc(p.label||p.id)+"</option>").join("");$("activeEspLine").textContent="："+(c.active_esp_label||c.active_esp_id||"未读取")+"（当前串口 "+(c.esp.serial_port||"--")+"）";}
async function refresh(){const c=await api("/api/config");if(document.activeElement!==$("region"))$("region").value=c.cloud.region;setIdleValue("account",c.cloud.account);if(![$("wifiSsid"),$("wifiPassword"),$("wifiList")].includes(document.activeElement))setIdleValue("wifiSsid",c.wifi.ssid);$("wifiPassword").placeholder=c.wifi.password_saved?"已保存，留空则不修改":"WiFi 密码";setIdleValue("espHost",c.esp.host||c.esp.last_ip);if(document.activeElement!==$("espPort"))$("espPort").value=c.esp.port||8081;if(document.activeElement!==$("brightness"))$("brightness").value=String(c.display&&c.display.brightness||100);if(![$("serialPort"),$("serialList")].includes(document.activeElement))$("serialPort").value=c.esp.serial_port||"COM7";renderEspProfiles(c);updateSteps(c);setActionState(c);$("statusLine").innerHTML="当前 ESP："+(c.active_esp_label||c.active_esp_id||"--")+"　打印机："+(c.printer.display_name||c.printer.serial||"未选择")+"　云账号："+(c.cloud.logged_in?"已登录":"未登录")+"　MQTT 用户名："+(c.cloud.mqtt_username||"--");$("brightnessLine").textContent="用户亮度："+(c.display&&c.display.brightness||100)+"%　当前亮度："+(c.display&&c.display.active_brightness||c.display&&c.display.brightness||100)+"%"+(c.display&&c.display.dimmed?"（自动节能）":"");$("espLine").textContent="当前写入方式："+(c.esp.host||c.esp.last_ip?"优先 ESP HTTP，失败后串口兜底":"USB 串口写入")+"（"+($("serialPort").value||"--")+"）";$("wifiLine").className=c.esp.wifi_status==="configured"?"ok":(c.esp.wifi_status==="ap_only"?"warn":"muted");$("wifiLine").textContent=wifiStatusText(c);log(c)}
async function sendCode(){try{log(await api("/api/cloud/send-code",{method:"POST",body:JSON.stringify({region:$("region").value,account:$("account").value})}))}catch(e){log(e)}}
async function login(){try{log(await api("/api/cloud/login",{method:"POST",body:JSON.stringify({region:$("region").value,account:$("account").value,code:$("code").value})}));await loadDevices()}catch(e){log(e)}}
async function saveEsp(configureWifi){const busy=["saveSettingsBtn","configureWifiBtn","detectWifiBtn","pushConfigBtn","brightnessBtn"];const line=$("wifiLine");try{setBusy(busy,true);if(line){line.className="muted";line.textContent=configureWifi?"正在写入 ESP WiFi，随后会检测 ESP 是否连上 WiFi...":"正在保存写入设置...";}const body={wifi_ssid:$("wifiSsid").value,wifi_password:$("wifiPassword").value,brightness:Number($("brightness").value||100),host:$("espHost").value,port:Number($("espPort").value||8081),serial_port:$("serialPort").value||"COM7",configure_wifi:Boolean(configureWifi)};log(configureWifi?"正在写入 ESP WiFi，并检测连接状态...":"正在保存写入设置...");const d=await api("/api/esp",{method:"POST",body:JSON.stringify(body)});log(d);$("wifiPassword").value="";await refresh();if(configureWifi&&line){line.className=d.ok&&(d.detection&&d.detection.ok)?"ok":(d.ok?"warn":"bad");line.textContent=d.message||"ESP WiFi 配置流程已完成，请查看下方日志。";}}catch(e){if(line){line.className="bad";line.textContent=(e&&e.error)||"保存或配置 ESP WiFi 失败。";}log(e)}finally{setBusy(busy,false);try{setActionState(await api("/api/config"))}catch{}}}
async function applyBrightnessSetting(){const busy=["brightnessBtn","configureWifiBtn","pushConfigBtn"];const line=$("brightnessLine");try{setBusy(busy,true);if(line){line.className="muted";line.textContent="正在写入屏幕亮度...";}const body={brightness:Number($("brightness").value||100),apply_brightness:true,host:$("espHost").value,port:Number($("espPort").value||8081),serial_port:$("serialPort").value||"COM7"};const d=await api("/api/esp",{method:"POST",body:JSON.stringify(body)});log(d);await refresh();if(line){line.className=d.ok?"ok":"bad";line.textContent=d.ok?"屏幕亮度已写入当前 ESP。":"屏幕亮度写入失败。";}}catch(e){if(line){line.className="bad";line.textContent=(e&&e.error)||"屏幕亮度写入失败。";}log(e)}finally{setBusy(busy,false);try{setActionState(await api("/api/config"))}catch{}}}
async function pushConfig(){try{log(await api("/api/esp/push-config",{method:"POST"}));await refresh()}catch(e){log(e)}}
async function checkEsp(discover){const busy=["detectWifiBtn","configureWifiBtn"];const line=$("wifiLine");try{setBusy(busy,true);if(line){line.className="muted";line.textContent=discover?"正在检测 ESP WiFi：先试已知地址和 USB 串口，必要时搜索局域网...":"正在检测 ESP WiFi...";}log("正在检测 ESP WiFi...");const d=await api("/api/esp/status?discover="+(discover?"1":"0"));log(d);await refresh();if(line){line.className=d.ok?"ok":"warn";line.textContent=d.message||d.error||(d.ok?"ESP WiFi 已配置。":"ESP WiFi 状态未确认。");}}catch(e){if(line){line.className="bad";line.textContent=(e&&e.error)||"检测 ESP WiFi 失败。";}log(e)}finally{setBusy(busy,false);try{setActionState(await api("/api/config"))}catch{}}}
function renderSerialPorts(items){const list=$("serialList");if(!list)return;const cur=String($("serialPort").value||"").toUpperCase();const arr=(items||[]).map(x=>typeof x==="string"?{port:x,name:""}:x).filter(x=>x&&x.port);list.innerHTML=(arr.length?arr:[{port:"",name:"未检测到串口"}]).map(p=>"<option value='"+esc(p.port)+"'"+(p.port===cur?" selected":"")+">"+esc(p.port+(p.name?("　"+p.name):""))+"</option>").join("");}
function chooseSerial(){const v=$("serialList").value;if(v)$("serialPort").value=v;}
async function activateEsp(){try{const id=$("espProfileList").value;if(!id)return;log(await api("/api/esp/activate",{method:"POST",body:JSON.stringify({id})}));await refresh();api("/api/devices").then(d=>renderDevices(d.devices||[]))}catch(e){log(e)}}
async function readEspDevice(){try{log("正在读取当前串口 ESP 身份...");log(await api("/api/esp/read-device",{method:"POST",body:JSON.stringify({serial_port:$("serialPort").value||"COM7"})}));await refresh();api("/api/devices").then(d=>renderDevices(d.devices||[]))}catch(e){log(e)}}
function renderAutoDetect(d){const line=$("autoDetectLine");if(!line)return;const results=d&&d.results||[];multiEspBlocked=!!(d&&d.multiple_devices);if(!results.length){line.className="warn";line.textContent="没有检测到串口，请确认当前要配置的 ESP 已插入 USB。";return;}const ok=results.filter(x=>x.ok);const fail=results.filter(x=>!x.ok);if(multiEspBlocked){line.className="bad";line.textContent=d.message+" 已识别："+ok.map(x=>x.port+" "+(x.mac||x.active_esp_id)).join("；");return;}line.className=ok.length?"ok":"warn";line.textContent=d&&d.message?d.message:(ok.length?("已识别 "+ok.length+" 台 ESP："+ok.map(x=>x.port+" "+(x.mac||x.active_esp_id)).join("；")+(fail.length?"；未响应："+fail.map(x=>x.port).join("、"):"")):("没有串口返回 ESP 身份："+fail.map(x=>x.port).join("、")));}
async function autoDetectEsp(showLog){if(autoDetectBusy||(!showLog&&operationBusy))return;autoDetectBusy=true;const line=$("autoDetectLine");try{if(line){line.className="muted";line.textContent="正在检测当前 ESP...";}const d=await api("/api/esp/auto-detect",{method:"POST"});renderAutoDetect(d);if(showLog)log(d);await refresh();api("/api/devices").then(x=>renderDevices(x.devices||[]));}catch(e){if(line){line.className="bad";line.textContent=(e&&e.error)||"自动检测 ESP 失败。";}if(showLog)log(e)}finally{autoDetectBusy=false}}
function renderWifiNetworks(networks){const list=$("wifiList");if(!list)return;const current=String($("wifiSsid").value||"");const opts=['<option value="">请选择扫描到的 WiFi，或在下方手动输入</option>'];(networks||[]).forEach(n=>{const ssid=String(n.ssid||"");const label=ssid+(n.signal?("（"+n.signal+"%）"):"");opts.push("<option value='"+esc(ssid)+"'"+(ssid===current?" selected":"")+">"+esc(label)+"</option>")});list.innerHTML=opts.join("");}
function chooseWifi(){const v=$("wifiList").value;if(v){$("wifiSsid").value=v;$("wifiPassword").focus();}}
async function scanWifi(showLog){const line=$("wifiScanLine");if(line){line.className="muted";line.textContent="正在通过 ESP 扫描附近 WiFi...";}try{const d=await api("/api/wifi/networks?serial_port="+encodeURIComponent($("serialPort").value||"COM7"));renderWifiNetworks(d.networks||[]);if(line){const ok=(d.networks||[]).length>0;line.className=ok?"ok":"warn";line.textContent=d.message||(ok?("已扫描到 "+d.networks.length+" 个 WiFi，可选择或手动输入。"):"未扫描到 WiFi，可手动输入。");}if(showLog)log(d)}catch(e){renderWifiNetworks([]);if(line){line.className="warn";line.textContent=(e&&e.error)||"扫描失败，可手动输入 WiFi 名称。";}if(showLog)log(e)}}
async function loadPorts(detect){try{const d=await api("/api/serial/ports");const ports=d.ports||[];const items=d.items||ports;renderSerialPorts(items);if(ports.length){const cur=String($("serialPort").value||"").toUpperCase();if((!cur||!ports.includes(cur))&&document.activeElement!==$("serialList"))$("serialPort").value=ports[0];renderSerialPorts(items);}if(detect)await autoDetectEsp(false);else log(d)}catch(e){log(e)}}
async function loadDevices(){try{const d=await api("/api/cloud/bindings");renderDevices(d.devices||[]);if(d.esp_printer_sync)log({message:d.esp_printer_sync.ok?"打印机列表已同步到 ESP 本地网页":"打印机列表尚未同步到 ESP 本地网页",esp_printer_sync:d.esp_printer_sync});else log(d);await refresh()}catch(e){log(e)}}
async function syncPrinterList(){try{const body={host:$("espHost").value,port:Number($("espPort").value||8081)};const d=await api("/api/esp/sync-printers",{method:"POST",body:JSON.stringify(body)});log(d);await refresh()}catch(e){log(e)}}
async function renderDevices(devs){const c=await api("/api/config");const disabled=!c.steps.ready_to_select_printer||multiEspBlocked;let warn="";if(disabled)warn='<p class="warn">'+(multiEspBlocked?'当前同时连接了多台 ESP，请只保留当前要配置的一台。':(!c.steps.esp_ready?'请先连接一台 ESP，并点击“检测当前 ESP”。':'请先完成第 1 步登录和第 2 步 WiFi 保存，再选择打印机。'))+'</p>';$("devices").innerHTML=warn+'<table><tr><th>名称</th><th>型号</th><th>序列号</th><th>状态</th><th></th></tr>'+devs.map(d=>'<tr><td>'+esc(d.display_name)+'</td><td>'+esc(d.model)+'</td><td>'+esc(d.serial)+'</td><td>'+esc(d.print_status||"")+'</td><td><button class="selectPrinterBtn" data-serial="'+esc(d.serial)+'" data-esp="'+esc(c.active_esp_id||'')+'" data-port="'+esc(c.esp.serial_port||'')+'" '+(disabled?'disabled':'')+'>显示这台并同步</button></td></tr>').join("")+'</table>';document.querySelectorAll(".selectPrinterBtn").forEach(b=>{b.onclick=()=>selectPrinter(b.dataset.serial,b.dataset.esp,b.dataset.port);});}
function esc(s){return String(s||"").replace(/[&<>"']/g,c=>({"&":"&amp;","<":"&lt;",">":"&gt;","\\\"":"&quot;","'":"&#39;"}[c]))}
async function selectPrinter(serial,active_esp_id,serial_port){try{log("正在写入当前 ESP："+(active_esp_id||"--")+" / "+(serial_port||"--"));log(await api("/api/printer/select",{method:"POST",body:JSON.stringify({serial,active_esp_id,serial_port})}));await refresh()}catch(e){log(e)}}
async function initialize(){await refresh();await loadPorts(true);await scanWifi(false);api("/api/devices").then(d=>renderDevices(d.devices||[]))}initialize();setInterval(refresh,5000);setInterval(()=>{if(document.visibilityState==="visible")autoDetectEsp(false)},8000);
</script></body></html>`;
}

async function readBody(req) {
  let text = "";
  for await (const chunk of req) text += chunk;
  return text ? JSON.parse(text) : {};
}

function send(res, status, body, type = "application/json") {
  res.writeHead(status, { "Content-Type": `${type}; charset=utf-8`, "Access-Control-Allow-Origin": "*" });
  res.end(type === "application/json" ? JSON.stringify(body, null, 2) : body);
}

function publicError(error) {
  const message = errorText(error);
  if (message.includes("EACCES") && message.includes("443")) return "当前配置工具进程无法访问 Bambu 云服务 443 端口，请重启配置工具后再试。";
  return message;
}

const server = http.createServer(async (req, res) => {
  try {
    const url = new URL(req.url, `http://${req.headers.host}`);
    if (req.method === "OPTIONS") return send(res, 204, {});
    if (req.method === "GET" && url.pathname === "/") return send(res, 200, html(), "text/html");
    if (req.method === "GET" && url.pathname === "/api/version") return send(res, 200, { version: BACKEND_VERSION, changes: CHANGELOG, urls: serviceUrls() });
    if (req.method === "GET" && url.pathname === "/api/config") return send(res, 200, publicConfig());
    if (req.method === "GET" && url.pathname === "/api/devices") return send(res, 200, readJson(DEVICES_FILE, { devices: [] }));
    if (req.method === "GET" && url.pathname === "/api/serial/ports") {
      const items = await listSerialPortObjects();
      return send(res, 200, { ports: items.map((item) => item.port), items });
    }
    if (req.method === "GET" && url.pathname === "/api/wifi/networks") return send(res, 200, await listWifiNetworks(url.searchParams.get("serial_port")));
    if (req.method === "GET" && url.pathname === "/api/esp/profiles") return send(res, 200, { active_esp_id: readConfig().active_esp_id, profiles: publicConfig().esp_profiles });
    if (req.method === "POST" && url.pathname === "/api/esp/activate") return send(res, 200, activateEspProfile((await readBody(req)).id));
    if (req.method === "POST" && url.pathname === "/api/esp/auto-detect") return send(res, 200, await autoDetectEspPorts());
    if (req.method === "POST" && url.pathname === "/api/esp/read-device") return send(res, 200, await readEspStatusViaSerial((await readBody(req)).serial_port));
    if (req.method === "POST" && url.pathname === "/api/cloud/send-code") return send(res, 200, await sendCode(await readBody(req)));
    if (req.method === "POST" && url.pathname === "/api/cloud/login") return send(res, 200, await login(await readBody(req)));
    if (req.method === "GET" && url.pathname === "/api/cloud/bindings") return send(res, 200, await fetchBindings());
    if (req.method === "POST" && url.pathname === "/api/printer/select") return send(res, 200, { ok: true, ...(await selectPrinter(await readBody(req))) });
    if (req.method === "POST" && url.pathname === "/api/esp/sync-printers") return send(res, 200, await syncSavedPrinterListToEsp(await readBody(req)));
    if (req.method === "POST" && url.pathname === "/api/esp") {
      const body = await readBody(req);
      let next = readConfig();
      const wantsWifiWrite = Boolean(body.configure_wifi || String(body.wifi_ssid || "").trim() || body.wifi_password);
      const wantsEspWrite = Boolean(wantsWifiWrite || body.apply_brightness);
      if (wantsEspWrite) {
        await ensureSingleConnectedEsp();
        const detected = await readEspStatusViaSerial(body.serial_port || next.esp.serial_port);
        if (!detected.ok) throw new Error(detected.error || "当前 USB ESP 身份读取失败，请重新插拔设备后再试");
        next = readConfig();
        ensureActiveEspSelected(next);
      }
      if (Object.prototype.hasOwnProperty.call(body, "wifi_ssid")) next.wifi.ssid = String(body.wifi_ssid || "").trim();
      if (body.wifi_password) next.wifi.password = String(body.wifi_password);
      if (Object.prototype.hasOwnProperty.call(body, "brightness")) next.display.brightness = normalizeBrightness(body.brightness);
      next.esp.host = next.esp.wifi_status === "configured" ? normalizeEspHost(body.host) : "";
      next.esp.port = Number(body.port || 8081);
      next.esp.serial_port = normalizeSerialPort(body.serial_port);
      const saved = saveConfig(next);
      if (body.configure_wifi) return send(res, 200, { ok: true, saved: true, ...(await configureWifiAndDetect({ skipSingleCheck: true })) });
      if (body.apply_brightness) {
        const write = await pushBrightnessToEsp({ skipSingleCheck: true });
        return send(res, 200, { ok: Boolean(write.ok), saved: true, display: saved.display, write });
      }
      return send(res, 200, { ok: true, esp: saved.esp, display: saved.display });
    }
    if (req.method === "GET" && url.pathname === "/api/esp/status") return send(res, 200, await checkEspStatus({ discover: url.searchParams.get("discover") === "1" }));
    if (req.method === "POST" && url.pathname === "/api/esp/push-config") return send(res, 200, await pushConfigToEsp());
    send(res, 404, { error: "没有这个接口" });
  } catch (error) {
    send(res, 500, { error: publicError(error) });
  }
});

function writeServerState() {
  try {
    writeJson(SERVER_STATE_FILE, {
      version: BACKEND_VERSION,
      port: PORT,
      urls: serviceUrls(),
      started_at: Date.now()
    });
  } catch (error) {
    console.warn("server-state.json write failed:", error.message);
  }
}

function listenWithFallback(port) {
  PORT = port;
  server.removeAllListeners("error");
  server.removeAllListeners("listening");
  server.once("error", (error) => {
    if (error && error.code === "EADDRINUSE" && PORT < 8895) {
      console.log(`端口 ${PORT} 已被占用，尝试 ${PORT + 1}...`);
      listenWithFallback(PORT + 1);
      return;
    }
    console.error(error);
    process.exit(1);
  });
  server.once("listening", () => {
    writeServerState();
    console.log(`PrintSphere Lite 配置工具已启动: http://127.0.0.1:${PORT}/`);
    for (const item of serviceUrls().filter((item) => !item.includes("127.0.0.1"))) console.log(`局域网访问地址: ${item}`);
  });
  server.listen(PORT, "0.0.0.0");
}

listenWithFallback(PORT);

process.on("SIGINT", () => process.exit(0));
