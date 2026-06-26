// ============================================================
// SD2 PrintSphere Lite - ESP8266 Bambu Cloud MQTT display
// The companion backend only provisions cloud credentials and printer choice.
// ============================================================

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config.h"

TFT_eSPI tft;
WiFiServer apiServer(ESP_CONFIG_PORT);
BearSSL::WiFiClientSecure mqttNet;

#define LCD_BL_PIN 5

const char* FIRMWARE_VERSION = "firmware-v0.4.56-usb-first";

#define BG_BLACK  0x0000
#define C_RING    0x07E0
#define C_TRACK   0x2104
#define C_TEXT    0xFFFF
#define C_DIM     0x8410
#define C_CYAN    0x07FF
#define C_ORANGE  0xFD20
#define C_BLUE    0x5D1F
#define C_RED     0xF800

#define FRAME_X      4
#define FRAME_Y      4
#define FRAME_SIZE   232
#define FRAME_THICK  10
#define FRAME_SIDE   232
#define FRAME_TOTAL  (FRAME_SIDE * 4)

#define BAMBU_LOGO_W 24
#define BAMBU_LOGO_H 32
#define MAX_PRINTER_OPTIONS 16
const uint8_t BAMBU_LOGO[] PROGMEM = {
  0xFF, 0xE7, 0xFF, 0xFF, 0xEF, 0xFF, 0xFF, 0xEF,
  0xFF, 0xFF, 0xEF, 0xFF, 0xFF, 0xEF, 0xFF, 0xFF,
  0xEF, 0xFF, 0xFF, 0xEF, 0xFF, 0xFF, 0xEF, 0xFF,
  0xFF, 0xEF, 0xFF, 0xFF, 0xEF, 0xFF, 0xFF, 0xE7,
  0xFF, 0xFF, 0xE1, 0xFF, 0xFF, 0xEE, 0x3F, 0xFF,
  0xEF, 0x8F, 0xFF, 0xEF, 0xF1, 0xFF, 0xEF, 0xFC,
  0xFF, 0xCF, 0xFF, 0xFE, 0x2F, 0xFF, 0xF8, 0xEF,
  0xFF, 0xC7, 0xEF, 0xFF, 0x1F, 0xEF, 0xFF, 0xFF,
  0xEF, 0xFF, 0xFF, 0xEF, 0xFF, 0xFF, 0xEF, 0xFF,
  0xFF, 0xEF, 0xFF, 0xFF, 0xEF, 0xFF, 0xFF, 0xEF,
  0xFF, 0xFF, 0xEF, 0xFF, 0xFF, 0xEF, 0xFF, 0xFF,
  0xEF, 0xFF, 0xFF, 0xEF, 0xFF, 0xFF, 0xE7, 0xFF
};

struct PrinterState {
  float progress = -1;
  float nozzleTemp = -1;
  float leftNozzleTemp = -1;
  float rightNozzleTemp = -1;
  float bedTemp = -1;
  int remainingMin = -1;
  int currentLayer = -1;
  int totalLayers = -1;
  String status = "prepare";
  String displayName = "";
  String serial = "";
  bool online = true;
  bool dualNozzle = false;
};

struct StoredConfig {
  String wifiSsid = WIFI_SSID;
  String wifiPassword = WIFI_PASSWORD;
  String region = "cn";
  String mqttHost = "cn.mqtt.bambulab.com";
  String mqttUsername = "";
  String token = "";
  String serial = "";
  String name = "";
  uint8_t brightness = 100;
};

struct PrinterOption {
  String serial = "";
  String name = "";
  String model = "";
};

struct RenderCache {
  bool baseDrawn = false;
  bool offlineDrawn = false;
  int progress = -999;
  int progressPct = -999;
  int nozzleTemp = -999;
  int nozzleSide = -2;
  int bedTemp = -999;
  int remainingMin = -999;
  int currentLayer = -999;
  int totalLayers = -999;
  String status = "";
  String displayName = "";
};

PrinterState pr;
StoredConfig stored;
RenderCache cache;
PrinterOption printerOptions[MAX_PRINTER_OPTIONS];
uint8_t printerOptionCount = 0;

unsigned long lastWifiCheck = 0;
unsigned long lastDisplay = 0;
unsigned long lastMqttConnect = 0;
unsigned long lastMqttRequest = 0;
unsigned long lastMqttPing = 0;
uint16_t mqttPacketId = 1;
bool displayDirty = true;
bool serverStarted = false;
bool setupApStarted = false;
bool mqttReconnectPending = false;
bool wifiReconnectPending = false;
bool lastConfigMode = false;
String setupApSsid;
uint8_t mqttBuf[MQTT_BUFFER_SIZE];
String serialConfigLine;
String pendingHttpConfigBody;
bool pendingHttpConfig = false;
bool printerStatusReceived = false;
bool brightnessDimmed = false;
uint8_t appliedBrightness = 0;
unsigned long inactiveSince = 0;

const unsigned long DUAL_NOZZLE_SWITCH_MS = 3000;
const unsigned long AUTO_DIM_DELAY_MS = 5UL * 60UL * 1000UL;

void startEspServer();

bool wifiHasIp() {
  return WiFi.localIP() != IPAddress(0, 0, 0, 0);
}

bool httpNetworkReady() {
  return wifiHasIp() || setupApStarted;
}

bool configClientConnected() {
  return setupApStarted && WiFi.softAPgetStationNum() > 0;
}

float jsonFloat(JsonVariant v, float fallback) {
  if (v.is<float>() || v.is<int>()) return v.as<float>();
  if (v.is<const char*>()) return atof(v.as<const char*>());
  return fallback;
}

int jsonInt(JsonVariant v, int fallback) {
  if (v.is<int>()) return v.as<int>();
  if (v.is<float>()) return (int)v.as<float>();
  if (v.is<const char*>()) return atoi(v.as<const char*>());
  return fallback;
}

const char* jsonStringForKeys(JsonObject obj, const char* const* keys, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    const char* value = obj[keys[i]] | "";
    if (value[0]) return value;
  }
  return "";
}

float jsonFloatForKeys(JsonObject obj, const char* const* keys, size_t count, float fallback) {
  for (size_t i = 0; i < count; ++i) {
    float value = jsonFloat(obj[keys[i]], fallback);
    if (value != fallback) return value;
  }
  return fallback;
}

int jsonIntForKeys(JsonObject obj, const char* const* keys, size_t count, int fallback) {
  for (size_t i = 0; i < count; ++i) {
    int value = jsonInt(obj[keys[i]], fallback);
    if (value != fallback) return value;
  }
  return fallback;
}

float normalizePercent(float value) {
  if (value >= 0 && value <= 1.0f) return value * 100.0f;
  return value;
}

float normalizeTemp(float value) {
  if (value > 65535.0f) return (float)(((uint32_t)value) & 0xFFFF);
  return value;
}

void storeNozzleTemp(int side, float temp, bool invertToolSide = true) {
  if (temp == -999) return;
  temp = normalizeTemp(temp);
  int displaySide = side;
  if (invertToolSide) {
    if (side == 0) displaySide = 1;
    else if (side == 1) displaySide = 0;
  }
  if (displaySide == 0) pr.leftNozzleTemp = temp;
  else if (displaySide == 1) pr.rightNozzleTemp = temp;
  if (pr.nozzleTemp < 0 || displaySide == 0) pr.nozzleTemp = temp;
}

float firstTempFromInfo(JsonArray info, float fallback) {
  for (JsonObject item : info) {
    float temp = jsonFloat(item["temp"], -999);
    if (temp != -999) return normalizeTemp(temp);
  }
  return fallback;
}

bool applyNozzleInfo(JsonArray info) {
  if (info.isNull() || info.size() == 0) return false;

  bool any = false;
  bool dual = info.size() >= 2;
  int index = 0;
  for (JsonObject item : info) {
    float temp = jsonFloat(item["temp"], -999);
    if (temp == -999) {
      index++;
      continue;
    }
    int id = jsonInt(item["id"], index);
    if (id == 0 || id == 1) {
      storeNozzleTemp(id, temp);
      any = true;
    } else if (!dual) {
      pr.nozzleTemp = normalizeTemp(temp);
      any = true;
    }
    index++;
  }
  if (dual && any) pr.dualNozzle = true;
  return any;
}

bool applyNestedNozzleTemps(JsonObject print) {
  JsonObject device = print["device"].as<JsonObject>();
  if (device.isNull()) return false;

  bool got = applyNozzleInfo(device["extruder"]["info"].as<JsonArray>());
  if (applyNozzleInfo(device["nozzle"]["info"].as<JsonArray>())) got = true;
  return got;
}

float nestedNozzleTemp(JsonObject print, float fallback) {
  JsonObject device = print["device"].as<JsonObject>();
  if (device.isNull()) return fallback;

  float temp = firstTempFromInfo(device["extruder"]["info"].as<JsonArray>(), fallback);
  if (temp != fallback) return temp;
  temp = firstTempFromInfo(device["nozzle"]["info"].as<JsonArray>(), fallback);
  if (temp != fallback) return temp;
  return fallback;
}

float nestedBedTemp(JsonObject print, float fallback) {
  JsonObject device = print["device"].as<JsonObject>();
  if (device.isNull()) return fallback;

  int packed = jsonInt(device["bed"]["info"]["temp"], -999);
  if (packed != -999) return normalizeTemp((float)packed);
  float temp = firstTempFromInfo(device["bed"]["info"].as<JsonArray>(), fallback);
  if (temp != fallback) return temp;
  packed = jsonInt(device["bed_temp"], -999);
  if (packed != -999) return normalizeTemp((float)packed);
  return fallback;
}

bool hasToken(const String& s, const char* token) {
  String v = s;
  v.toLowerCase();
  return v.indexOf(token) >= 0;
}

bool isPrintingState(const String& s) {
  return hasToken(s, "running") || hasToken(s, "printing") || hasToken(s, "processing");
}

bool isPreparingState(const String& s) {
  return hasToken(s, "prepare") || hasToken(s, "starting") || hasToken(s, "heating") ||
         hasToken(s, "download") || s.equalsIgnoreCase("init") || s.equalsIgnoreCase("slicing");
}

bool isPausedState(const String& s) {
  return hasToken(s, "pause");
}

bool isFinishedState(const String& s) {
  return hasToken(s, "finish") || hasToken(s, "success") || hasToken(s, "done") ||
         hasToken(s, "complete");
}

bool isFailedState(const String& s) {
  return hasToken(s, "fail") || hasToken(s, "error") || hasToken(s, "cancel");
}

uint8_t normalizeBrightness(int value) {
  if (value <= 25) return 25;
  if (value <= 50) return 50;
  if (value <= 75) return 75;
  return 100;
}

void applyBrightness(uint8_t percent) {
  percent = normalizeBrightness(percent);
  if (appliedBrightness == percent) return;
  int duty = 1023 - (int)percent * 10;
  analogWrite(LCD_BL_PIN, duty);
  appliedBrightness = percent;
}

bool hasActiveTaskForBrightness() {
  if (!printerStatusReceived || !mqttNet.connected()) return false;
  return isPrintingState(pr.status) || isPreparingState(pr.status) || isPausedState(pr.status);
}

void updateBrightness(unsigned long now) {
  if (hasActiveTaskForBrightness()) {
    inactiveSince = 0;
    brightnessDimmed = false;
    applyBrightness(stored.brightness);
    return;
  }
  if (inactiveSince == 0) inactiveSince = now ? now : 1;
  brightnessDimmed = now - inactiveSince >= AUTO_DIM_DELAY_MS;
  applyBrightness(brightnessDimmed ? 25 : stored.brightness);
}

void wifiConnect() {
  if (WiFi.status() == WL_CONNECTED) return;
  if (!stored.wifiSsid.length()) {
    Serial.println("WiFi not configured");
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.begin(stored.wifiSsid.c_str(), stored.wifiPassword.c_str());
  for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; ++i) delay(500);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected, IP=");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connect timeout");
  }
}

void startSetupAp() {
  if (setupApStarted) return;
  setupApSsid = String("PrintSphereLite-") + String(ESP.getChipId(), HEX);
  setupApSsid.toUpperCase();
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  setupApStarted = WiFi.softAP(setupApSsid.c_str(), "printsphere", 1, false, 2);
  if (setupApStarted) {
    Serial.printf("Setup AP: %s / printsphere / http://%s:%d/\n",
                  setupApSsid.c_str(), WiFi.softAPIP().toString().c_str(), ESP_CONFIG_PORT);
  } else {
    Serial.println("Setup AP start failed");
  }
}

bool saveStoredConfig() {
  if (!LittleFS.begin()) return false;
  File file = LittleFS.open("/cloud.json", "w");
  if (!file) return false;
  JsonDocument doc;
  doc["wifi_ssid"] = stored.wifiSsid;
  doc["wifi_password"] = stored.wifiPassword;
  doc["region"] = stored.region;
  doc["mqtt_host"] = stored.mqttHost;
  doc["mqtt_username"] = stored.mqttUsername;
  doc["token"] = stored.token;
  doc["serial"] = stored.serial;
  doc["name"] = stored.name;
  doc["brightness"] = stored.brightness;
  serializeJson(doc, file);
  file.close();
  return true;
}

bool savePrinterOptions() {
  if (!LittleFS.begin()) return false;
  File file = LittleFS.open("/printers.json", "w");
  if (!file) return false;
  JsonDocument doc;
  JsonArray arr = doc["printers"].to<JsonArray>();
  for (uint8_t i = 0; i < printerOptionCount; ++i) {
    JsonObject item = arr.add<JsonObject>();
    item["serial"] = printerOptions[i].serial;
    item["name"] = printerOptions[i].name;
    item["model"] = printerOptions[i].model;
  }
  serializeJson(doc, file);
  file.close();
  return true;
}

void clearPrinterOptions() {
  for (uint8_t i = 0; i < MAX_PRINTER_OPTIONS; ++i) {
    printerOptions[i].serial = "";
    printerOptions[i].name = "";
    printerOptions[i].model = "";
  }
  printerOptionCount = 0;
}

bool loadPrinterArray(JsonArray arr) {
  if (arr.isNull()) return false;
  clearPrinterOptions();
  for (JsonObject item : arr) {
    if (printerOptionCount >= MAX_PRINTER_OPTIONS) break;
    const char* serial = item["serial"] | item["dev_id"] | item["device_id"] | "";
    if (!serial[0]) continue;
    printerOptions[printerOptionCount].serial = serial;
    printerOptions[printerOptionCount].name = item["name"] | item["display_name"] | item["dev_name"] | serial;
    printerOptions[printerOptionCount].model = item["model"] | "";
    printerOptionCount++;
  }
  return true;
}

void loadPrinterOptions() {
  clearPrinterOptions();
  if (!LittleFS.begin()) return;
  if (!LittleFS.exists("/printers.json")) return;
  File file = LittleFS.open("/printers.json", "r");
  if (!file) return;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, file);
  file.close();
  if (err) return;
  loadPrinterArray(doc["printers"].as<JsonArray>());
}

void loadStoredConfig() {
  if (!LittleFS.begin()) return;
  if (!LittleFS.exists("/cloud.json")) return;
  File file = LittleFS.open("/cloud.json", "r");
  if (!file) return;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, file);
  file.close();
  if (err) return;
  stored.wifiSsid = doc["wifi_ssid"] | stored.wifiSsid;
  stored.wifiPassword = doc["wifi_password"] | stored.wifiPassword;
  stored.region = doc["region"] | stored.region;
  stored.mqttHost = doc["mqtt_host"] | stored.mqttHost;
  stored.mqttUsername = doc["mqtt_username"] | "";
  stored.token = doc["token"] | "";
  stored.serial = doc["serial"] | "";
  stored.name = doc["name"] | "";
  stored.brightness = normalizeBrightness(doc["brightness"] | 100);
  pr.serial = stored.serial;
  pr.displayName = stored.name;
}

String compactMac() {
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  mac.toLowerCase();
  return mac;
}

String statusJson() {
  JsonDocument doc;
  doc["ok"] = true;
  doc["firmware_version"] = FIRMWARE_VERSION;
  doc["chip_id"] = String(ESP.getChipId(), HEX);
  doc["mac"] = WiFi.macAddress();
  doc["device_id"] = String("esp-") + compactMac();
  doc["ip"] = WiFi.localIP().toString();
  doc["ap_ip"] = WiFi.softAPIP().toString();
  doc["ap_ssid"] = setupApSsid;
  doc["serial"] = pr.serial.length() ? pr.serial : stored.serial;
  doc["name"] = pr.displayName.length() ? pr.displayName : stored.name;
  doc["mqtt_host"] = stored.mqttHost;
  doc["mqtt_username"] = stored.mqttUsername.length() ? "set" : "";
  doc["mqtt_connected"] = mqttNet.connected();
  doc["brightness"] = stored.brightness;
  doc["brightness_active"] = appliedBrightness;
  doc["brightness_dimmed"] = brightnessDimmed;
  doc["printer_count"] = printerOptionCount;
  doc["online"] = pr.online;
  doc["status"] = pr.status;
  doc["progress"] = pr.progress;
  doc["nozzle_temp"] = pr.nozzleTemp;
  doc["left_nozzle_temp"] = pr.leftNozzleTemp;
  doc["right_nozzle_temp"] = pr.rightNozzleTemp;
  doc["dual_nozzle"] = pr.dualNozzle;
  doc["bed_temp"] = pr.bedTemp;
  doc["remaining_min"] = pr.remainingMin;
  doc["current_layer"] = pr.currentLayer;
  doc["total_layers"] = pr.totalLayers;
  String out;
  serializeJson(doc, out);
  return out;
}

String jsonEscape(const String& text) {
  String out;
  out.reserve(text.length() + 8);
  for (size_t i = 0; i < text.length(); ++i) {
    char c = text[i];
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if ((uint8_t)c < 0x20) {
      char buf[7];
      snprintf(buf, sizeof(buf), "\\u%04x", (uint8_t)c);
      out += buf;
    } else {
      out += c;
    }
  }
  return out;
}

int rssiToSignal(int32_t rssi) {
  if (rssi <= -100) return 0;
  if (rssi >= -50) return 100;
  return 2 * (rssi + 100);
}

String wifiScanJson() {
  int count = WiFi.scanNetworks(false, true);
  String out = "{\"ok\":true,\"source\":\"esp\",\"networks\":[";
  int added = 0;
  for (int i = 0; i < count && added < 24; ++i) {
    String ssid = WiFi.SSID(i);
    if (!ssid.length()) continue;
    bool duplicate = false;
    for (int j = 0; j < i; ++j) {
      if (WiFi.SSID(j) == ssid) {
        duplicate = true;
        break;
      }
    }
    if (duplicate) continue;
    int32_t rssi = WiFi.RSSI(i);
    if (added) out += ',';
    out += "{\"ssid\":\"";
    out += jsonEscape(ssid);
    out += "\",\"rssi\":";
    out += String(rssi);
    out += ",\"signal\":";
    out += String(rssiToSignal(rssi));
    out += "}";
    added++;
  }
  WiFi.scanDelete();
  out += "],\"count\":";
  out += String(added);
  out += "}";
  return out;
}

String htmlEscape(const String& text) {
  String out;
  out.reserve(text.length() + 8);
  for (size_t i = 0; i < text.length(); ++i) {
    char c = text[i];
    if (c == '&') out += "&amp;";
    else if (c == '<') out += "&lt;";
    else if (c == '>') out += "&gt;";
    else if (c == '"') out += "&quot;";
    else out += c;
  }
  return out;
}

String printersJson() {
  JsonDocument doc;
  doc["ok"] = true;
  doc["selected_serial"] = stored.serial;
  JsonArray arr = doc["printers"].to<JsonArray>();
  for (uint8_t i = 0; i < printerOptionCount; ++i) {
    JsonObject item = arr.add<JsonObject>();
    item["serial"] = printerOptions[i].serial;
    item["name"] = printerOptions[i].name;
    item["model"] = printerOptions[i].model;
    item["selected"] = printerOptions[i].serial == stored.serial;
  }
  String out;
  serializeJson(doc, out);
  return out;
}

bool selectPrinterBySerial(const String& serial) {
  if (!serial.length()) return false;
  for (uint8_t i = 0; i < printerOptionCount; ++i) {
    if (printerOptions[i].serial == serial) {
      if (stored.serial != printerOptions[i].serial) {
        stored.serial = printerOptions[i].serial;
        stored.name = printerOptions[i].name.length() ? printerOptions[i].name : printerOptions[i].serial;
        pr.serial = stored.serial;
        pr.displayName = stored.name;
        pr.progress = -1;
        pr.nozzleTemp = -1;
        pr.leftNozzleTemp = -1;
        pr.rightNozzleTemp = -1;
        pr.dualNozzle = false;
        pr.bedTemp = -1;
        pr.remainingMin = -1;
        pr.currentLayer = -1;
        pr.totalLayers = -1;
        pr.status = "prepare";
        printerStatusReceived = false;
        inactiveSince = millis();
        cache.baseDrawn = false;
        mqttReconnectPending = true;
      }
      saveStoredConfig();
      displayDirty = true;
      return true;
    }
  }
  return false;
}

String espHomeHtml() {
  String ip = WiFi.localIP().toString();
  String ap = WiFi.softAPIP().toString();
  String selected = stored.name.length() ? stored.name : stored.serial;
  String body;
  body.reserve(5200);
  body += F("<!doctype html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">");
  body += F("<title>PrintSphere Lite ESP 配置</title><style>body{margin:0;background:#f6f7f8;color:#1f2933;font-family:system-ui,-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif}main{max-width:760px;margin:0 auto;padding:16px}section{background:#fff;border:1px solid #dde2e7;border-radius:8px;padding:14px;margin:12px 0}h1{font-size:22px;margin:6px 0 12px}h2{font-size:16px;margin:0 0 8px}label{display:block;font-size:13px;color:#52606d;margin:10px 0 4px}input,select{width:100%;box-sizing:border-box;border:1px solid #ccd3db;border-radius:6px;padding:9px;font-size:14px}button{border:0;border-radius:6px;background:#16784f;color:#fff;padding:9px 12px;font-size:14px;margin:8px 8px 0 0}.secondary{background:#52606d}.muted{color:#718096;font-size:13px}.ok{color:#137333}.warn{color:#b45309}.bad{color:#b42318}pre{white-space:pre-wrap;background:#101820;color:#d9e2ec;border-radius:8px;padding:10px;min-height:54px}</style></head><body><main>");
  body += F("<h1>PrintSphere Lite ESP 配置</h1><p class=\"muted\">固件：");
  body += FIRMWARE_VERSION;
  body += F("</p><section><h2>当前状态</h2><p>局域网 IP：");
  body += htmlEscape(ip);
  body += F("<br>配置热点：");
  body += htmlEscape(setupApSsid);
  body += F(" / http://");
  body += htmlEscape(ap);
  body += F(":");
  body += String(ESP_CONFIG_PORT);
  body += F("/<br>当前打印机：");
  body += htmlEscape(selected.length() ? selected : String("未选择"));
  body += F("<br>MQTT：");
  body += mqttNet.connected() ? F("<span class=\"ok\">已连接</span>") : F("<span class=\"warn\">未连接</span>");
  body += F("</p><button onclick=\"refreshStatus()\">刷新状态</button></section>");
  body += F("<section><h2>WiFi 配置</h2><p class=\"muted\">可连接 ESP 热点后在这里配置 WiFi。保存后 ESP 会自动重连。</p><label>附近 WiFi</label><select id=\"wifiList\" onchange=\"wifiSsid.value=this.value\"><option value=\"\">点击扫描</option></select><button class=\"secondary\" onclick=\"scanWifi()\">扫描 WiFi</button><label>WiFi 名称</label><input id=\"wifiSsid\" value=\"");
  body += htmlEscape(stored.wifiSsid);
  body += F("\"><label>WiFi 密码</label><input id=\"wifiPassword\" type=\"password\" placeholder=\"留空则不修改\"><button onclick=\"saveWifi()\">保存 WiFi</button></section>");
  body += F("<section><h2>切换显示打印机</h2><p class=\"muted\">打印机列表需要先由电脑端配置工具登录 Bambu 云后同步到 ESP。之后可在这里无线切换。</p><select id=\"printerList\"><option value=\"\">正在读取...</option></select><button onclick=\"selectPrinter()\">显示这台</button><button class=\"secondary\" onclick=\"loadPrinters()\">刷新列表</button></section>");
  body += F("<section><h2>日志</h2><pre id=\"log\">就绪</pre></section>");
  body += F("<script>const $=id=>document.getElementById(id);function log(x){$('log').textContent=typeof x==='string'?x:JSON.stringify(x,null,2)}async function api(u){const r=await fetch(u,{cache:'no-store'});const t=await r.text();try{return JSON.parse(t)}catch{return t}}async function refreshStatus(){log(await api('/api/status'))}async function scanWifi(){const d=await api('/api/wifi/scan');$('wifiList').innerHTML='<option value=\"\">请选择 WiFi</option>'+(d.networks||[]).map(n=>'<option value=\"'+n.ssid.replace(/\"/g,'&quot;')+'\">'+n.ssid+'（'+(n.signal||0)+'%）</option>').join('');log(d)}async function saveWifi(){const p=new URLSearchParams();p.set('wifi_ssid',$('wifiSsid').value);if($('wifiPassword').value)p.set('wifi_password',$('wifiPassword').value);log(await api('/api/config-url?'+p.toString()))}async function loadPrinters(){const d=await api('/api/printers');$('printerList').innerHTML=(d.printers||[]).length?(d.printers||[]).map(p=>'<option value=\"'+p.serial+'\" '+(p.selected?'selected':'')+'>'+((p.name||p.serial)+' / '+(p.model||''))+'</option>').join(''):'<option value=\"\">暂无已同步打印机</option>';log(d)}async function selectPrinter(){const s=$('printerList').value;if(!s)return log('请选择打印机');log(await api('/api/select-printer?serial='+encodeURIComponent(s)));setTimeout(refreshStatus,800)}loadPrinters();</script></main></body></html>");
  return body;
}

String applyConfigBody(const String& body, int& statusCode) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    statusCode = 400;
    return "{\"ok\":false,\"error\":\"bad json\"}";
  }

  const char* region = doc["region"] | "";
  const char* wifiSsid = doc["wifi_ssid"] | "";
  const char* wifiPassword = doc["wifi_password"] | "";
  const char* mqttHost = doc["mqtt_host"] | "";
  const char* mqttUsername = doc["mqtt_username"] | "";
  const char* token = doc["token"] | doc["access_token"] | "";
  const char* serial = doc["serial"] | doc["printer_serial"] | "";
  const char* name = doc["name"] | doc["display_name"] | "";
  const char* printersPayload = doc["printers_json"] | "";
  int requestedBrightness = doc["brightness"] | doc["brightness_percent"] | -1;
  bool wifiChanged = false;
  bool mqttChanged = false;
  bool brightnessChanged = false;
  if (printersPayload[0]) {
    JsonDocument printersDoc;
    if (!deserializeJson(printersDoc, printersPayload)) {
      JsonArray arr = printersDoc.is<JsonArray>() ? printersDoc.as<JsonArray>() : printersDoc["printers"].as<JsonArray>();
      if (loadPrinterArray(arr)) savePrinterOptions();
    }
  }
  if (doc["printers"].is<JsonArray>()) {
    if (loadPrinterArray(doc["printers"].as<JsonArray>())) savePrinterOptions();
  }
  if (wifiSsid[0] && stored.wifiSsid != wifiSsid) {
    stored.wifiSsid = wifiSsid;
    wifiChanged = true;
  }
  if (wifiPassword[0] && stored.wifiPassword != wifiPassword) {
    stored.wifiPassword = wifiPassword;
    wifiChanged = true;
  }
  if (region[0]) stored.region = region;
  if (mqttHost[0] && stored.mqttHost != mqttHost) {
    stored.mqttHost = mqttHost;
    mqttChanged = true;
  }
  if (mqttUsername[0] && stored.mqttUsername != mqttUsername) {
    stored.mqttUsername = mqttUsername;
    mqttChanged = true;
  }
  if (token[0] && stored.token != token) {
    stored.token = token;
    mqttChanged = true;
  }
  if (serial[0]) {
    if (stored.serial != serial) {
      stored.serial = serial;
      pr.serial = serial;
      cache.baseDrawn = false;
      pr.progress = -1;
      pr.nozzleTemp = -1;
      pr.leftNozzleTemp = -1;
      pr.rightNozzleTemp = -1;
      pr.dualNozzle = false;
      pr.bedTemp = -1;
      pr.remainingMin = -1;
      pr.currentLayer = -1;
      pr.totalLayers = -1;
      pr.status = "prepare";
      printerStatusReceived = false;
      inactiveSince = millis();
      mqttChanged = true;
    } else {
      pr.serial = serial;
    }
  }
  if (name[0]) {
    stored.name = name;
    pr.displayName = name;
    cache.baseDrawn = false;
  }
  if (requestedBrightness > 0) {
    uint8_t normalized = normalizeBrightness(requestedBrightness);
    if (stored.brightness != normalized) {
      stored.brightness = normalized;
      brightnessChanged = true;
    }
  }
  bool ok = saveStoredConfig();
  if (wifiChanged) {
    wifiReconnectPending = true;
  } else if (mqttChanged) {
    mqttReconnectPending = true;
  }
  displayDirty = true;
  if (brightnessChanged) updateBrightness(millis());

  JsonDocument outDoc;
  outDoc["ok"] = ok;
  outDoc["serial"] = stored.serial;
  outDoc["name"] = stored.name;
  outDoc["ip"] = WiFi.localIP().toString();
  outDoc["brightness"] = stored.brightness;
  outDoc["brightness_active"] = appliedBrightness;
  String out;
  serializeJson(outDoc, out);
  statusCode = ok ? 200 : 500;
  return out;
}

String handleSerialJson(const String& body, int& statusCode) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (!err) {
    const char* cmd = doc["cmd"] | "";
    if (String(cmd).equalsIgnoreCase("wifi_scan") || doc["wifi_scan"] == true) {
      statusCode = 200;
      return wifiScanJson();
    }
    if (String(cmd).equalsIgnoreCase("status")) {
      statusCode = 200;
      return statusJson();
    }
  }
  return applyConfigBody(body, statusCode);
}

String urlDecode(const String& text) {
  String out;
  out.reserve(text.length());
  for (size_t i = 0; i < text.length(); ++i) {
    char c = text[i];
    if (c == '+') {
      out += ' ';
    } else if (c == '%' && i + 2 < text.length()) {
      char hex[3] = { text[i + 1], text[i + 2], 0 };
      out += (char)strtol(hex, nullptr, 16);
      i += 2;
    } else {
      out += c;
    }
  }
  return out;
}

String configBodyFromQuery(const String& query) {
  JsonDocument doc;
  size_t start = 0;
  while (start < query.length()) {
    int amp = query.indexOf('&', start);
    if (amp < 0) amp = query.length();
    String pair = query.substring(start, amp);
    int eq = pair.indexOf('=');
    if (eq > 0) {
      String key = urlDecode(pair.substring(0, eq));
      String value = urlDecode(pair.substring(eq + 1));
      if (key == "wifi_ssid" || key == "wifi_password" || key == "region" ||
          key == "mqtt_host" || key == "mqtt_username" || key == "token" ||
          key == "access_token" || key == "serial" || key == "printer_serial" ||
          key == "name" || key == "display_name" || key == "printers_json" ||
          key == "brightness" || key == "brightness_percent") {
        doc[key] = value;
      }
    }
    start = amp + 1;
  }
  String body;
  serializeJson(doc, body);
  return body;
}

void sendHttpJson(WiFiClient& client, int statusCode, const String& body) {
  const char* statusText = statusCode == 200 ? "OK" :
                           statusCode == 202 ? "Accepted" :
                           statusCode == 204 ? "No Content" :
                           statusCode == 400 ? "Bad Request" : "Not Found";
  client.printf("HTTP/1.1 %d %s\r\n", statusCode, statusText);
  client.print("Content-Type: application/json; charset=utf-8\r\n");
  client.print("Access-Control-Allow-Origin: *\r\n");
  client.print("Access-Control-Allow-Methods: GET,POST,OPTIONS\r\n");
  client.print("Access-Control-Allow-Headers: content-type\r\n");
  client.print("Connection: close\r\n");
  client.printf("Content-Length: %u\r\n\r\n", body.length());
  client.print(body);
  client.flush();
  delay(1);
  client.stop();
}

void sendHttpHtml(WiFiClient& client, const String& body) {
  client.print("HTTP/1.1 200 OK\r\n");
  client.print("Content-Type: text/html; charset=utf-8\r\n");
  client.print("Cache-Control: no-store\r\n");
  client.print("Connection: close\r\n");
  client.printf("Content-Length: %u\r\n\r\n", body.length());
  client.print(body);
  client.flush();
  delay(1);
  client.stop();
}

void queueHttpConfig(WiFiClient& client, const String& body) {
  if (!body.length() || body.length() > 4096) {
    sendHttpJson(client, 400, "{\"ok\":false,\"error\":\"bad config size\"}");
    return;
  }
  pendingHttpConfigBody = body;
  pendingHttpConfig = true;
  sendHttpJson(client, 202, "{\"ok\":true,\"queued\":true}");
}

void handleApiClient() {
  if (!serverStarted) return;
  WiFiClient client = apiServer.available();
  if (!client) return;
  client.setTimeout(200);
  String line = client.readStringUntil('\n');
  line.trim();
  while (client.connected() && client.available()) {
    String discard = client.readStringUntil('\n');
    if (discard == "\r" || discard.length() == 0) break;
  }
  if (!line.length()) {
    client.stop();
    return;
  }
  int firstSpace = line.indexOf(' ');
  int secondSpace = line.indexOf(' ', firstSpace + 1);
  String method = firstSpace > 0 ? line.substring(0, firstSpace) : "";
  String target = (firstSpace > 0 && secondSpace > firstSpace) ? line.substring(firstSpace + 1, secondSpace) : "/";
  int q = target.indexOf('?');
  String path = q >= 0 ? target.substring(0, q) : target;
  String query = q >= 0 ? target.substring(q + 1) : "";
  if (method == "OPTIONS") {
    sendHttpJson(client, 204, "{}");
  } else if (method == "GET" && path == "/") {
    sendHttpHtml(client, espHomeHtml());
  } else if (method == "GET" && (path == "/api/status" || path == "/api/ping")) {
    sendHttpJson(client, 200, statusJson());
  } else if (method == "GET" && path == "/api/wifi/scan") {
    sendHttpJson(client, 200, wifiScanJson());
  } else if (method == "GET" && path == "/api/config-url") {
    queueHttpConfig(client, configBodyFromQuery(query));
  } else if (method == "GET" && path == "/api/printers-url") {
    queueHttpConfig(client, configBodyFromQuery(query));
  } else if (method == "GET" && path == "/api/printers") {
    sendHttpJson(client, 200, printersJson());
  } else if (method == "GET" && path == "/api/select-printer") {
    String serial = "";
    size_t start = 0;
    while (start < query.length()) {
      int amp = query.indexOf('&', start);
      if (amp < 0) amp = query.length();
      String pair = query.substring(start, amp);
      int eq = pair.indexOf('=');
      if (eq > 0 && urlDecode(pair.substring(0, eq)) == "serial") serial = urlDecode(pair.substring(eq + 1));
      start = amp + 1;
    }
    if (selectPrinterBySerial(serial)) sendHttpJson(client, 200, "{\"ok\":true}");
    else sendHttpJson(client, 400, "{\"ok\":false,\"error\":\"printer not found\"}");
  } else if (method == "GET" && path == "/api/refresh") {
    cache.baseDrawn = false;
    cache.offlineDrawn = false;
    displayDirty = true;
    sendHttpJson(client, 200, "{\"ok\":true}");
  } else {
    sendHttpJson(client, 404, "{\"ok\":false,\"error\":\"not found\"}");
  }
}

void restartEspServer() {
  if (serverStarted) {
    apiServer.close();
    delay(20);
    apiServer.begin();
    Serial.printf("ESP server restarted: sta=http://%s:%d/ ap=http://%s:%d/\n",
                  WiFi.localIP().toString().c_str(), ESP_CONFIG_PORT,
                  WiFi.softAPIP().toString().c_str(), ESP_CONFIG_PORT);
  }
}

void handleSerialConfig() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      serialConfigLine.trim();
      if (serialConfigLine.startsWith("{")) {
        int statusCode = 200;
        String response = handleSerialJson(serialConfigLine, statusCode);
        Serial.println(response);
      }
      serialConfigLine = "";
    } else if (serialConfigLine.length() < 2048) {
      serialConfigLine += c;
    } else {
      serialConfigLine = "";
    }
  }
}

void startEspServer() {
  if (serverStarted) return;
  apiServer.begin();
  serverStarted = true;
  Serial.printf("ESP server: sta=http://%s:%d/ ap=http://%s:%d/\n",
                WiFi.localIP().toString().c_str(), ESP_CONFIG_PORT,
                WiFi.softAPIP().toString().c_str(), ESP_CONFIG_PORT);
}

bool mqttConfigReady() {
  return stored.mqttHost.length() > 0 && stored.mqttUsername.length() > 0 &&
         stored.token.length() > 0 && stored.serial.length() > 0;
}

String reportTopic() {
  return String("device/") + stored.serial + "/report";
}

String requestTopic() {
  return String("device/") + stored.serial + "/request";
}

void mqttWriteRemainingLength(size_t len) {
  do {
    uint8_t digit = len % 128;
    len /= 128;
    if (len > 0) digit |= 0x80;
    mqttNet.write(&digit, 1);
  } while (len > 0);
}

void mqttWriteByte(uint8_t value) {
  mqttNet.write(&value, 1);
}

void mqttWriteString(const String& value) {
  uint16_t len = value.length();
  mqttWriteByte((uint8_t)(len >> 8));
  mqttWriteByte((uint8_t)(len & 0xff));
  mqttNet.write((const uint8_t*)value.c_str(), len);
}

bool mqttReadPacket(uint8_t* type, uint8_t* body, size_t bodySize, size_t* bodyLen, uint32_t timeoutMs) {
  uint32_t start = millis();
  while (!mqttNet.available()) {
    if (!mqttNet.connected() || millis() - start > timeoutMs) return false;
    handleApiClient();
    delay(1);
  }
  int header = mqttNet.read();
  if (header < 0) return false;

  size_t len = 0;
  int multiplier = 1;
  uint8_t digit = 0;
  do {
    start = millis();
    while (!mqttNet.available()) {
      if (!mqttNet.connected() || millis() - start > timeoutMs) return false;
      handleApiClient();
      delay(1);
    }
    digit = mqttNet.read();
    len += (digit & 127) * multiplier;
    multiplier *= 128;
  } while (digit & 128);

  if (len > bodySize) {
    for (size_t i = 0; i < len; ++i) {
      start = millis();
      while (!mqttNet.available()) {
        if (!mqttNet.connected() || millis() - start > timeoutMs) return false;
        handleApiClient();
        delay(1);
      }
      mqttNet.read();
    }
    return false;
  }

  size_t got = 0;
  while (got < len) {
    start = millis();
    while (!mqttNet.available()) {
      if (!mqttNet.connected() || millis() - start > timeoutMs) return false;
      handleApiClient();
      delay(1);
    }
    int n = mqttNet.read(body + got, len - got);
    if (n > 0) got += (size_t)n;
  }

  *type = (uint8_t)header >> 4;
  *bodyLen = len;
  return true;
}

bool mqttSendConnect() {
  String clientId = String("PrintSphereLite-") + String(ESP.getChipId(), HEX);
  size_t remaining = 10 + 2 + clientId.length() + 2 + stored.mqttUsername.length() + 2 + stored.token.length();
  mqttWriteByte(0x10);
  mqttWriteRemainingLength(remaining);
  mqttWriteString("MQTT");
  mqttWriteByte(4);
  mqttWriteByte(0xC2);
  mqttWriteByte(0);
  mqttWriteByte(60);
  mqttWriteString(clientId);
  mqttWriteString(stored.mqttUsername);
  mqttWriteString(stored.token);

  uint8_t type = 0;
  size_t len = 0;
  if (!mqttReadPacket(&type, mqttBuf, sizeof(mqttBuf), &len, 3000)) return false;
  return type == 2 && len >= 2 && mqttBuf[1] == 0;
}

bool mqttSendSubscribe(const String& topic) {
  uint16_t id = mqttPacketId++;
  size_t remaining = 2 + 2 + topic.length() + 1;
  mqttWriteByte(0x82);
  mqttWriteRemainingLength(remaining);
  mqttWriteByte((uint8_t)(id >> 8));
  mqttWriteByte((uint8_t)(id & 0xff));
  mqttWriteString(topic);
  mqttWriteByte(0);

  uint8_t type = 0;
  size_t len = 0;
  return mqttReadPacket(&type, mqttBuf, sizeof(mqttBuf), &len, 3000) && type == 9;
}

bool mqttSendPublish(const String& topic, const char* payload) {
  size_t payloadLen = strlen(payload);
  size_t remaining = 2 + topic.length() + payloadLen;
  mqttWriteByte(0x30);
  mqttWriteRemainingLength(remaining);
  mqttWriteString(topic);
  mqttNet.write((const uint8_t*)payload, payloadLen);
  return true;
}

void mqttSendPing() {
  mqttWriteByte(0xC0);
  mqttWriteByte(0);
  lastMqttPing = millis();
}

void publishMqttRequest(const char* payload) {
  if (!mqttNet.connected()) return;
  String topic = requestTopic();
  mqttSendPublish(topic, payload);
}

void requestPrinterState() {
  publishMqttRequest("{\"pushing\":{\"sequence_id\":\"0\",\"command\":\"pushall\"}}");
  lastMqttRequest = millis();
}

bool applyFlatToolNozzles(JsonObject print) {
  static const char* const tool0Keys[] = {
    "tool0_nozzle_temp", "tool0_nozzle_temper"
  };
  static const char* const tool1Keys[] = {
    "tool1_nozzle_temp", "tool1_nozzle_temper"
  };
  static const char* const leftKeys[] = {
    "left_nozzle_temp", "left_nozzle_temper"
  };
  static const char* const rightKeys[] = {
    "right_nozzle_temp", "right_nozzle_temper"
  };
  bool got = false;
  float f = jsonFloatForKeys(print, tool0Keys, sizeof(tool0Keys) / sizeof(tool0Keys[0]), -999);
  if (f != -999) {
    storeNozzleTemp(0, f, true);
    got = true;
  }
  f = jsonFloatForKeys(print, tool1Keys, sizeof(tool1Keys) / sizeof(tool1Keys[0]), -999);
  if (f != -999) {
    storeNozzleTemp(1, f, true);
    pr.dualNozzle = true;
    got = true;
  }
  f = jsonFloatForKeys(print, leftKeys, sizeof(leftKeys) / sizeof(leftKeys[0]), -999);
  if (f != -999) {
    storeNozzleTemp(0, f, false);
    got = true;
  }
  f = jsonFloatForKeys(print, rightKeys, sizeof(rightKeys) / sizeof(rightKeys[0]), -999);
  if (f != -999) {
    storeNozzleTemp(1, f, false);
    pr.dualNozzle = true;
    got = true;
  }
  return got;
}

void applyPrint(JsonObject print) {
  if (print.isNull()) return;

  static const char* const statusKeys[] = {
    "gcode_state", "print_status", "printStatus", "status", "task_status", "taskStatus", "state"
  };
  const char* status = jsonStringForKeys(print, statusKeys, sizeof(statusKeys) / sizeof(statusKeys[0]));
  if (status[0]) {
    pr.status = status;
    pr.status.toLowerCase();
    printerStatusReceived = true;
  }

  static const char* const progressKeys[] = {
    "mc_percent", "percent", "progress", "task_progress", "taskProgress",
    "print_progress", "printProgress", "printPercent"
  };
  float f = jsonFloatForKeys(print, progressKeys, sizeof(progressKeys) / sizeof(progressKeys[0]), -999);
  if (f != -999) pr.progress = normalizePercent(f);

  bool gotNestedNozzle = applyNestedNozzleTemps(print);
  bool gotFlatToolNozzle = applyFlatToolNozzles(print);
  if (!pr.dualNozzle && !gotNestedNozzle && !gotFlatToolNozzle) {
    static const char* const nozzleKeys[] = {
      "nozzle_temper", "nozzle_temp", "nozzle_temperature", "nozzleTemperature",
      "hotend_temp", "hotend_temperature"
    };
    f = jsonFloatForKeys(print, nozzleKeys, sizeof(nozzleKeys) / sizeof(nozzleKeys[0]), -999);
    if (f == -999) f = nestedNozzleTemp(print, -999);
    if (f != -999) pr.nozzleTemp = normalizeTemp(f);
  }

  static const char* const bedKeys[] = {
    "bed_temper", "bed_temp", "bed_temperature", "bedTemperature",
    "hotbed_temper", "hotbed_temp", "hotbed_temperature"
  };
  f = jsonFloatForKeys(print, bedKeys, sizeof(bedKeys) / sizeof(bedKeys[0]), -999);
  if (f == -999) f = nestedBedTemp(print, -999);
  if (f != -999) pr.bedTemp = f;

  static const char* const remainingMinuteKeys[] = {
    "mc_remaining_time", "remaining_minutes", "remainingMinutes", "remaining_min", "remain_time"
  };
  int i = jsonIntForKeys(print, remainingMinuteKeys, sizeof(remainingMinuteKeys) / sizeof(remainingMinuteKeys[0]), -999);
  if (i == -999) {
    static const char* const remainingSecondKeys[] = {
      "remaining_seconds", "remainingSeconds", "remaining_time", "remainingTime", "mc_left_time"
    };
    int seconds = jsonIntForKeys(print, remainingSecondKeys, sizeof(remainingSecondKeys) / sizeof(remainingSecondKeys[0]), -999);
    if (seconds != -999) i = (seconds + 59) / 60;
  }
  if (i != -999) pr.remainingMin = i;

  static const char* const currentLayerKeys[] = {
    "layer_num", "current_layer", "currentLayer", "layer"
  };
  i = jsonIntForKeys(print, currentLayerKeys, sizeof(currentLayerKeys) / sizeof(currentLayerKeys[0]), -999);
  if (i != -999) pr.currentLayer = i;

  static const char* const totalLayerKeys[] = {
    "total_layer_num", "total_layers", "totalLayers", "layer_count", "layerCount"
  };
  i = jsonIntForKeys(print, totalLayerKeys, sizeof(totalLayerKeys) / sizeof(totalLayerKeys[0]), -999);
  if (i != -999) pr.totalLayers = i;

  pr.online = true;
  displayDirty = true;
}

const char* findPattern(const char* data, size_t length, const char* pattern) {
  size_t patternLen = strlen(pattern);
  if (patternLen == 0 || length < patternLen) return nullptr;
  for (size_t i = 0; i <= length - patternLen; ++i) {
    if (memcmp(data + i, pattern, patternLen) == 0) return data + i;
  }
  return nullptr;
}

const char* jsonObjectEnd(const char* start, const char* end) {
  int depth = 0;
  bool inString = false;
  bool escape = false;
  for (const char* p = start; p < end; ++p) {
    char c = *p;
    if (inString) {
      if (escape) escape = false;
      else if (c == '\\') escape = true;
      else if (c == '"') inString = false;
      continue;
    }
    if (c == '"') inString = true;
    else if (c == '{') depth++;
    else if (c == '}') {
      depth--;
      if (depth == 0) return p + 1;
    }
  }
  return nullptr;
}

void applyExtruderFromRawPayload(uint8_t* payload, size_t length) {
  const char* data = (const char*)payload;
  const char* payloadEnd = data + length;
  const char* extPos = findPattern(data, length, "\"extruder\":");
  if (!extPos) return;

  const char* objStart = extPos + strlen("\"extruder\":");
  while (objStart < payloadEnd && (*objStart == ' ' || *objStart == '\t')) objStart++;
  if (objStart >= payloadEnd || *objStart != '{') return;
  const char* objEnd = jsonObjectEnd(objStart, payloadEnd);
  if (!objEnd) return;

  JsonDocument extDoc;
  if (deserializeJson(extDoc, objStart, (size_t)(objEnd - objStart))) return;
  if (applyNozzleInfo(extDoc["info"].as<JsonArray>())) displayDirty = true;
}

void parseMqttPayload(uint8_t* payload, size_t length) {
  JsonDocument filter;
  filter["print"]["gcode_state"] = true;
  filter["print"]["print_status"] = true;
  filter["print"]["printStatus"] = true;
  filter["print"]["status"] = true;
  filter["print"]["task_status"] = true;
  filter["print"]["taskStatus"] = true;
  filter["print"]["state"] = true;
  filter["print"]["mc_percent"] = true;
  filter["print"]["percent"] = true;
  filter["print"]["progress"] = true;
  filter["print"]["task_progress"] = true;
  filter["print"]["taskProgress"] = true;
  filter["print"]["print_progress"] = true;
  filter["print"]["printProgress"] = true;
  filter["print"]["printPercent"] = true;
  filter["print"]["nozzle_temper"] = true;
  filter["print"]["nozzle_temp"] = true;
  filter["print"]["nozzle_temperature"] = true;
  filter["print"]["nozzleTemperature"] = true;
  filter["print"]["hotend_temp"] = true;
  filter["print"]["hotend_temperature"] = true;
  filter["print"]["tool0_nozzle_temp"] = true;
  filter["print"]["tool0_nozzle_temper"] = true;
  filter["print"]["tool1_nozzle_temp"] = true;
  filter["print"]["tool1_nozzle_temper"] = true;
  filter["print"]["left_nozzle_temp"] = true;
  filter["print"]["left_nozzle_temper"] = true;
  filter["print"]["right_nozzle_temp"] = true;
  filter["print"]["right_nozzle_temper"] = true;
  filter["print"]["bed_temper"] = true;
  filter["print"]["bed_temp"] = true;
  filter["print"]["bed_temperature"] = true;
  filter["print"]["bedTemperature"] = true;
  filter["print"]["hotbed_temper"] = true;
  filter["print"]["hotbed_temp"] = true;
  filter["print"]["hotbed_temperature"] = true;
  filter["print"]["mc_remaining_time"] = true;
  filter["print"]["remaining_minutes"] = true;
  filter["print"]["remainingMinutes"] = true;
  filter["print"]["remaining_min"] = true;
  filter["print"]["remain_time"] = true;
  filter["print"]["remaining_seconds"] = true;
  filter["print"]["remainingSeconds"] = true;
  filter["print"]["remaining_time"] = true;
  filter["print"]["remainingTime"] = true;
  filter["print"]["mc_left_time"] = true;
  filter["print"]["layer_num"] = true;
  filter["print"]["current_layer"] = true;
  filter["print"]["currentLayer"] = true;
  filter["print"]["layer"] = true;
  filter["print"]["total_layer_num"] = true;
  filter["print"]["total_layers"] = true;
  filter["print"]["totalLayers"] = true;
  filter["print"]["layer_count"] = true;
  filter["print"]["layerCount"] = true;
  filter["print"]["device"]["bed"]["info"]["temp"] = true;
  filter["print"]["device"]["bed_temp"] = true;
  filter["print"]["device"]["extruder"]["info"][0]["id"] = true;
  filter["print"]["device"]["extruder"]["info"][0]["temp"] = true;
  filter["print"]["device"]["extruder"]["info"][1]["id"] = true;
  filter["print"]["device"]["extruder"]["info"][1]["temp"] = true;
  filter["print"]["device"]["nozzle"]["info"][0]["id"] = true;
  filter["print"]["device"]["nozzle"]["info"][0]["temp"] = true;
  filter["print"]["device"]["nozzle"]["info"][1]["id"] = true;
  filter["print"]["device"]["nozzle"]["info"][1]["temp"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload, length, DeserializationOption::Filter(filter));
  if (err) return;
  applyPrint(doc["print"].as<JsonObject>());
  applyExtruderFromRawPayload(payload, length);
}

void mqttHandleIncoming() {
  while (mqttNet.connected() && mqttNet.available()) {
    uint8_t type = 0;
    size_t len = 0;
    if (!mqttReadPacket(&type, mqttBuf, sizeof(mqttBuf), &len, 100)) return;
    if (type == 3 && len > 2) {
      uint16_t topicLen = ((uint16_t)mqttBuf[0] << 8) | mqttBuf[1];
      if ((size_t)topicLen + 2 < len) {
        parseMqttPayload(mqttBuf + 2 + topicLen, len - 2 - topicLen);
      }
    }
  }
}

bool connectMqtt() {
  if (WiFi.status() != WL_CONNECTED || !mqttConfigReady()) return false;
  if (mqttNet.connected()) return true;

  char clientId[48];
  snprintf(clientId, sizeof(clientId), "PrintSphereLite-%06X", ESP.getChipId());
  Serial.printf("Cloud MQTT connecting %s serial=%s user=%s\n",
                stored.mqttHost.c_str(), stored.serial.c_str(), stored.mqttUsername.c_str());
  mqttNet.stop();
  if (!mqttNet.connect(stored.mqttHost.c_str(), MQTT_PORT)) {
    Serial.println("Cloud MQTT TCP failed");
    return false;
  }
  if (!mqttSendConnect()) {
    Serial.println("Cloud MQTT auth failed");
    mqttNet.stop();
    return false;
  }

  String topic = reportTopic();
  if (!mqttSendSubscribe(topic)) {
    Serial.println("Cloud MQTT subscribe failed");
    mqttNet.stop();
    return false;
  }

  publishMqttRequest("{\"info\":{\"sequence_id\":\"0\",\"command\":\"get_version\"}}");
  publishMqttRequest("{\"pushing\":{\"sequence_id\":\"0\",\"command\":\"start\"}}");
  requestPrinterState();
  Serial.print("Cloud MQTT subscribed: ");
  Serial.println(topic);
  return true;
}

void drawFrameTrack() {
  tft.fillRect(FRAME_X, FRAME_Y, FRAME_SIZE, FRAME_THICK, C_TRACK);
  tft.fillRect(FRAME_X + FRAME_SIZE - FRAME_THICK, FRAME_Y, FRAME_THICK, FRAME_SIZE, C_TRACK);
  tft.fillRect(FRAME_X, FRAME_Y + FRAME_SIZE - FRAME_THICK, FRAME_SIZE, FRAME_THICK, C_TRACK);
  tft.fillRect(FRAME_X, FRAME_Y, FRAME_THICK, FRAME_SIZE, C_TRACK);
}

void drawFrameProgressLen(int len, uint16_t color) {
  if (len <= 0) return;

  int top = min(len, FRAME_SIDE);
  if (top > 0) tft.fillRect(FRAME_X, FRAME_Y, top, FRAME_THICK, color);
  len -= top;

  int right = min(len, FRAME_SIDE);
  if (right > 0) tft.fillRect(FRAME_X + FRAME_SIZE - FRAME_THICK, FRAME_Y, FRAME_THICK, right, color);
  len -= right;

  int bottom = min(len, FRAME_SIDE);
  if (bottom > 0) tft.fillRect(FRAME_X + FRAME_SIZE - bottom, FRAME_Y + FRAME_SIZE - FRAME_THICK, bottom, FRAME_THICK, color);
  len -= bottom;

  int left = min(len, FRAME_SIDE);
  if (left > 0) tft.fillRect(FRAME_X, FRAME_Y + FRAME_SIZE - left, FRAME_THICK, left, color);
}

int progressToLen(float progress) {
  if (progress < 0) return 0;
  if (progress > 100) progress = 100;
  int len = (int)(progress * FRAME_TOTAL / 100.0f + 0.5f);
  if (progress > 0 && len < 1) len = 1;
  if (len > FRAME_TOTAL) len = FRAME_TOTAL;
  return len;
}

void updateFrameProgress() {
  int len = progressToLen(pr.progress);
  drawFrameTrack();
  drawFrameProgressLen(len, C_RING);
  cache.progress = len;
}

void drawBold(const String& text, int x, int y) {
  tft.drawString(text, x, y);
  tft.drawString(text, x + 1, y);
}

void drawTextBox(int x, int y, int w, int h, uint8_t font, uint16_t color, const String& text, bool bold) {
  tft.fillRect(x, y, w, h, BG_BLACK);
  tft.setTextFont(font);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(color, BG_BLACK);
  tft.setTextPadding(0);
  if (bold) drawBold(text, x + w / 2, y + h / 2);
  else tft.drawString(text, x + w / 2, y + h / 2);
}

String fitTextToWidth(String text, uint8_t font, int maxWidth, bool bold) {
  tft.setTextFont(font);
  int suffixW = tft.textWidth("..") + (bold ? 1 : 0);
  if (tft.textWidth(text) + (bold ? 1 : 0) <= maxWidth) return text;
  while (text.length() > 0 && tft.textWidth(text) + suffixW + (bold ? 1 : 0) > maxWidth) {
    text.remove(text.length() - 1);
  }
  return text.length() ? text + ".." : String("..");
}

void drawTempBox(int x, int y, int w, int h, int value, uint16_t color) {
  tft.fillRect(x, y, w, h, BG_BLACK);
  tft.setTextFont(4);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(color, BG_BLACK);
  tft.setTextPadding(0);
  if (value < 0) {
    drawBold("--", x + w / 2, y + h / 2);
    return;
  }

  char b[16];
  snprintf(b, sizeof(b), "%d", value);
  int cx = x + w / 2;
  int cy = y + h / 2;
  int numberW = tft.textWidth(b);
  int totalW = numberW + 17;
  int numberX = cx - totalW / 2 + numberW / 2;
  drawBold(b, numberX, cy);
  int dotX = numberX + numberW / 2 + 6;
  tft.drawCircle(dotX, cy - 8, 2, C_TEXT);
  tft.drawString("C", dotX + 11, cy);
}

void drawTempBoxSoft(int x, int y, int w, int h, int value, uint16_t color) {
  tft.fillRect(x, y, w, h, BG_BLACK);
  tft.setTextFont(4);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(color, BG_BLACK);
  tft.setTextPadding(0);
  if (value < 0) {
    tft.drawString("--", x + w / 2, y + h / 2);
    return;
  }

  char b[16];
  snprintf(b, sizeof(b), "%d", value);
  int cx = x + w / 2;
  int cy = y + h / 2;
  int numberW = tft.textWidth(b);
  int totalW = numberW + 16;
  int numberX = cx - totalW / 2 + numberW / 2;
  tft.drawString(b, numberX, cy);
  int dotX = numberX + numberW / 2 + 6;
  tft.drawCircle(dotX, cy - 8, 2, C_TEXT);
  tft.drawString("C", dotX + 10, cy);
}

void drawDualNozzleTempBox(int x, int y, int w, int h, int side, int value, uint16_t color) {
  int clearX = min(x, 21);
  int clearRight = max(x + w, 21 + 86);
  tft.fillRect(clearX, y, clearRight - clearX, h, BG_BLACK);
  tft.setTextFont(4);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(color, BG_BLACK);
  tft.setTextPadding(0);
  if (value < 0) {
    tft.drawString("--", x + w / 2, y + h / 2);
    return;
  }

  char b[16];
  snprintf(b, sizeof(b), "%c %d", side == 1 ? 'R' : 'L', value);
  int textW = tft.textWidth(b);
  int totalW = textW + 13;
  int cy = y + h / 2;
  int textX = x + w / 2 - totalW / 2 + textW / 2;
  tft.drawString(b, textX, cy);
  int dotX = textX + textW / 2 + 4;
  tft.drawCircle(dotX, cy - 8, 2, C_TEXT);
  tft.drawString("C", dotX + 8, cy);
}

bool hasDualNozzleTemps() {
  return pr.dualNozzle && pr.leftNozzleTemp >= 0 && pr.rightNozzleTemp >= 0;
}

bool dualNozzleDisplayMode() {
  return pr.dualNozzle;
}

int displayedNozzleSide() {
  if (hasDualNozzleTemps()) return ((millis() / DUAL_NOZZLE_SWITCH_MS) % 2) == 0 ? 0 : 1;
  if (pr.dualNozzle && pr.leftNozzleTemp >= 0) return 0;
  if (pr.dualNozzle && pr.rightNozzleTemp >= 0) return 1;
  return -1;
}

float displayedNozzleTemp() {
  int side = displayedNozzleSide();
  if (side == 0) return pr.leftNozzleTemp;
  if (side == 1) return pr.rightNozzleTemp;
  return pr.nozzleTemp;
}

const char* displayedNozzleLabel() {
  return "NOZZLE";
}

void drawLabels() {
  tft.fillRect(24, 98, 82, 18, BG_BLACK);
  tft.fillRect(134, 98, 82, 18, BG_BLACK);
  tft.setTextFont(2);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(C_DIM, BG_BLACK);
  tft.setTextPadding(0);
  tft.drawString(displayedNozzleLabel(), 65, 107);
  tft.drawString("BED", 175, 107);
}

String statusText() {
  if (isPrintingState(pr.status)) return "printing";
  if (isPreparingState(pr.status)) return "prepare";
  if (isPausedState(pr.status)) return "paused";
  if (isFinishedState(pr.status)) return "done";
  if (isFailedState(pr.status)) return "error";
  return "idle";
}

uint16_t statusColor() {
  if (isPrintingState(pr.status)) return C_RING;
  if (isPreparingState(pr.status)) return C_CYAN;
  if (isPausedState(pr.status)) return C_ORANGE;
  if (isFinishedState(pr.status)) return C_BLUE;
  if (isFailedState(pr.status)) return C_RED;
  return C_DIM;
}

void drawBase() {
  tft.fillScreen(BG_BLACK);
  drawFrameTrack();
  tft.drawBitmap(108, 102, BAMBU_LOGO, BAMBU_LOGO_W, BAMBU_LOGO_H, C_RING);
  drawLabels();

  cache.progress = -999;
  cache.progressPct = -999;
  cache.nozzleTemp = -999;
  cache.nozzleSide = -2;
  cache.bedTemp = -999;
  cache.remainingMin = -999;
  cache.currentLayer = -999;
  cache.totalLayers = -999;
  cache.status = "";
  cache.displayName = "";
  cache.baseDrawn = true;
  cache.offlineDrawn = false;
}

void updateFields() {
  int progressValue = pr.progress >= 0 ? (int)(pr.progress + 0.5f) : -1;
  if (progressToLen(pr.progress) != cache.progress) updateFrameProgress();
  if (progressValue != cache.progressPct) {
    char b[10];
    if (progressValue >= 0) {
      snprintf(b, sizeof(b), "%d%%", progressValue);
      drawTextBox(72, 37, 96, 34, 4, C_TEXT, b, true);
    } else {
      drawTextBox(72, 37, 96, 34, 4, C_DIM, "--", true);
    }
    cache.progressPct = progressValue;
  }

  String st = statusText();
  if (st != cache.status) {
    drawTextBox(50, 67, 140, 30, 4, statusColor(), st, true);
    drawLabels();
    cache.status = st;
  }

  int nozzleSide = displayedNozzleSide();
  bool nozzleSideChanged = nozzleSide != cache.nozzleSide;
  if (nozzleSideChanged) {
    drawLabels();
    cache.nozzleSide = nozzleSide;
  }

  float nozzleTemp = displayedNozzleTemp();
  int nozzle = nozzleTemp >= 0 ? (int)(nozzleTemp + 0.5f) : -1;
  if (nozzle != cache.nozzleTemp || nozzleSideChanged) {
    if (nozzleSide == 0 || nozzleSide == 1) {
      drawDualNozzleTempBox(17, 116, 86, 30, nozzleSide, nozzle, nozzle >= 0 ? C_TEXT : C_DIM);
    } else {
      drawTempBox(21, 116, 86, 30, nozzle, nozzle >= 0 ? C_TEXT : C_DIM);
    }
    cache.nozzleTemp = nozzle;
  }

  int bed = pr.bedTemp >= 0 ? (int)(pr.bedTemp + 0.5f) : -1;
  if (bed != cache.bedTemp) {
    if (dualNozzleDisplayMode()) {
      drawTempBoxSoft(133, 116, 86, 30, bed, bed >= 0 ? C_TEXT : C_DIM);
    } else {
      drawTempBox(133, 116, 86, 30, bed, bed >= 0 ? C_TEXT : C_DIM);
    }
    cache.bedTemp = bed;
  }

  if (pr.currentLayer != cache.currentLayer || pr.totalLayers != cache.totalLayers) {
    char b[24];
    if (pr.currentLayer >= 0 && pr.totalLayers > 0) snprintf(b, sizeof(b), "L%d/%d", pr.currentLayer, pr.totalLayers);
    else strcpy(b, "L--");
    drawTextBox(50, 153, 140, 24, 2, C_TEXT, b, true);
    cache.currentLayer = pr.currentLayer;
    cache.totalLayers = pr.totalLayers;
  }

  if (pr.remainingMin != cache.remainingMin || (pr.remainingMin <= 0 && pr.displayName != cache.displayName)) {
    String bottomText;
    if (pr.remainingMin > 0) {
      int h = pr.remainingMin / 60;
      int m = pr.remainingMin % 60;
      char b[16];
      if (h > 0) snprintf(b, sizeof(b), "%dh %dm", h, m);
      else snprintf(b, sizeof(b), "%dm", m);
      bottomText = b;
    } else {
      const String& label = stored.name.length() ? stored.name : pr.displayName;
      bottomText = fitTextToWidth(label.length() ? label : String("--"), 4, 204, true);
    }
    drawTextBox(18, 181, 204, 34, 4, C_CYAN, bottomText, true);
    cache.remainingMin = pr.remainingMin;
    cache.displayName = pr.displayName;
  }
}

void renderOffline() {
  if (cache.offlineDrawn) return;
  tft.startWrite();
  tft.fillScreen(BG_BLACK);
  drawFrameTrack();
  drawTextBox(50, 93, 140, 34, 4, C_ORANGE, "OFFLINE", true);
  String net = "NO WIFI";
  if (WiFi.status() == WL_CONNECTED) {
    net = String("IP ") + WiFi.localIP().toString();
  } else if (WiFi.status() == WL_IDLE_STATUS || WiFi.status() == WL_DISCONNECTED) {
    net = "WIFI...";
  }
  drawTextBox(28, 128, 184, 20, 2, C_DIM, net, false);
  tft.endWrite();
  cache.offlineDrawn = true;
  cache.baseDrawn = false;
}

void renderDisplay() {
  tft.startWrite();
  if (!cache.baseDrawn || cache.offlineDrawn) drawBase();
  updateFields();
  tft.endWrite();
}

void setup() {
  Serial.begin(115200);
  delay(300);
  loadStoredConfig();
  loadPrinterOptions();

  tft.begin();
  tft.invertDisplay(1);
  tft.setRotation(0);
  pinMode(LCD_BL_PIN, OUTPUT);
  analogWriteRange(1023);
  analogWriteFreq(1000);
  inactiveSince = millis();
  applyBrightness(stored.brightness);

  tft.fillScreen(BG_BLACK);
  drawTextBox(42, 92, 156, 28, 4, C_TEXT, "WIFI...", true);
  mqttNet.setInsecure();
  mqttNet.setTimeout(2500);
  startSetupAp();
  wifiConnect();
  if (wifiHasIp()) {
    tft.fillScreen(BG_BLACK);
    drawTextBox(24, 96, 192, 22, 2, C_TEXT, WiFi.localIP().toString(), true);
    delay(800);
  }
  if (httpNetworkReady()) startEspServer();
  lastMqttConnect = 0;
  renderDisplay();
}

void loop() {
  unsigned long now = millis();
  handleSerialConfig();
  handleApiClient();

  if (pendingHttpConfig) {
    String body = pendingHttpConfigBody;
    pendingHttpConfigBody = "";
    pendingHttpConfig = false;
    int statusCode = 200;
    String response = applyConfigBody(body, statusCode);
    Serial.println(response);
  }

  if (httpNetworkReady() && !serverStarted) startEspServer();

  bool configMode = configClientConnected();
  if (configMode && !lastConfigMode) {
    mqttNet.stop();
    lastMqttConnect = now;
    restartEspServer();
  }
  lastConfigMode = configMode;
  if (configMode && mqttNet.connected()) {
    mqttNet.stop();
    lastMqttConnect = now;
  }

  if (wifiReconnectPending) {
    wifiReconnectPending = false;
    mqttReconnectPending = false;
    mqttNet.stop();
    WiFi.disconnect();
    delay(100);
    wifiConnect();
    lastMqttConnect = 0;
  } else if (mqttReconnectPending) {
    mqttReconnectPending = false;
    mqttNet.stop();
    lastMqttConnect = 0;
  }

  if (now - lastWifiCheck > 30000) {
    lastWifiCheck = now;
    if (!wifiHasIp()) {
      wifiConnect();
      if (httpNetworkReady()) startEspServer();
    }
  }

  if (!configMode && WiFi.status() == WL_CONNECTED && mqttConfigReady()) {
    if (!mqttNet.connected() && now - lastMqttConnect >= MQTT_RECONNECT_INTERVAL) {
      lastMqttConnect = now;
      connectMqtt();
    }
    mqttHandleIncoming();
    if (mqttNet.connected() && now - lastMqttRequest >= MQTT_REQUEST_INTERVAL) {
      requestPrinterState();
    }
    if (mqttNet.connected() && now - lastMqttPing >= 30000) {
      mqttSendPing();
    }
  }

  if (hasDualNozzleTemps() && displayedNozzleSide() != cache.nozzleSide) {
    displayDirty = true;
  }

  updateBrightness(now);

  if (displayDirty && now - lastDisplay >= DISPLAY_REFRESH) {
    displayDirty = false;
    lastDisplay = now;
    if (pr.online) renderDisplay();
    else renderOffline();
  }

  delay(10);
}
