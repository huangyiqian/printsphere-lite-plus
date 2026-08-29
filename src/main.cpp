// ============================================================
// SD2 PrintSphere Lite - ESP8266 Bambu Cloud MQTT display
// The companion backend only provisions cloud credentials and printer choice.
// ============================================================

#include "config.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include <WiFiClientSecureBearSSL.h>
#include <time.h>

TFT_eSPI tft;
WiFiServer apiServer(ESP_CONFIG_PORT);
BearSSL::WiFiClientSecure mqttNet;

#define LCD_BL_PIN 5

const char *FIRMWARE_VERSION = "firmware-v0.4.90-ams&webcfg";

// Color definitions for BGR565 display panel ((B<<11) | (G<<5) | R)
#define BG_BLACK 0x0000
#define C_RING 0x07E0 // Visual Green
#define C_TRACK 0x2104
#define C_TEXT 0xFFFF   // Visual White
#define C_DIM 0x8410    // Visual Dim Gray
#define C_CYAN 0xFFE0   // Visual Cyan
#define C_ORANGE 0x061F // Visual Orange
#define C_YELLOW 0x07FF // Visual Yellow
#define C_BLUE 0xF800   // Visual Blue
#define C_RED 0x001F    // Visual Red
#define C_PANEL 0x3B6D
#define C_PANEL2 0x2A6B
#define C_PANEL3 0x4C10
#define C_CARD 0x2124 // Dark card surface for dashboard

#define FRAME_X 4
#define FRAME_Y 4
#define FRAME_SIZE 232
#define FRAME_THICK 10
#define FRAME_SIDE 232
#define FRAME_TOTAL (FRAME_SIDE * 4)

#define BAMBU_LOGO_W 24
#define BAMBU_LOGO_H 32
#define MAX_PRINTER_OPTIONS 16
const uint8_t BAMBU_LOGO[] PROGMEM = {
    0xFF, 0xE7, 0xFF, 0xFF, 0xEF, 0xFF, 0xFF, 0xEF, 0xFF, 0xFF, 0xEF, 0xFF,
    0xFF, 0xEF, 0xFF, 0xFF, 0xEF, 0xFF, 0xFF, 0xEF, 0xFF, 0xFF, 0xEF, 0xFF,
    0xFF, 0xEF, 0xFF, 0xFF, 0xEF, 0xFF, 0xFF, 0xE7, 0xFF, 0xFF, 0xE1, 0xFF,
    0xFF, 0xEE, 0x3F, 0xFF, 0xEF, 0x8F, 0xFF, 0xEF, 0xF1, 0xFF, 0xEF, 0xFC,
    0xFF, 0xCF, 0xFF, 0xFE, 0x2F, 0xFF, 0xF8, 0xEF, 0xFF, 0xC7, 0xEF, 0xFF,
    0x1F, 0xEF, 0xFF, 0xFF, 0xEF, 0xFF, 0xFF, 0xEF, 0xFF, 0xFF, 0xEF, 0xFF,
    0xFF, 0xEF, 0xFF, 0xFF, 0xEF, 0xFF, 0xFF, 0xEF, 0xFF, 0xFF, 0xEF, 0xFF,
    0xFF, 0xEF, 0xFF, 0xFF, 0xEF, 0xFF, 0xFF, 0xEF, 0xFF, 0xFF, 0xE7, 0xFF};

const char CN_READY[] PROGMEM =
    "000000000000000000000000000000640000003000087e01c00030001c6303ff8030000cff"
    "c78380300006c60fcf0fffc005c600fe0c30c001ffc0fc0c30c001c607df0c30c004c60e01"
    "cc30c006c607ff8fffc00effc6318c30c00cc607ff8030000cc606318030001cc606318030"
    "0008ffe7ff80300000c0060180300000000000000000000000000000000000000000000000"
    "00000000000000000000000"; // 55x23
const char CN_PRINT[] PROGMEM =
    "0000000000000000000000000000030001c0003000033fefbfc0300003038c18c030001fc3"
    "8c18c0300003038c18cfffc003038c18cc30c003038fd8cc30c003038c18cc30c003c38c18"
    "cc30c01f038c18cfffc003038c18cc30c003038cd8c0300003038f9b80300003038c180030"
    "00030300180030000e1f001800300000000000000000000000000000000000000000000000"
    "00000000000000000000000"; // 55x23
const char CN_PAUSE[] PROGMEM =
    "0000000000000000000000000000000c0630c0003ff83ffc001e60200000167f67f8001f6c"
    "661800064ce7f8003ffce000000c0c2ffc000ffc280c000c0c27f8000ffc20c0000c0c23c0"
    "0000000000000000000000"; // 36x17
const char CN_DONE[] PROGMEM =
    "00000000000000000000000000000000c000d00000c000f8001ffe7ffc00180660c0000000"
    "7ed8000ffc66d800000066f0001ffe66f000033066e80006327cec000e33c1e800183e4338"
    "0000000000000000000000"; // 36x17
const char CN_ERR[] PROGMEM =
    "000000000000000000000000000000086c67f800186c7618001efe261800306c07f8003fff"
    "e00000080067f80008fe60c0003ec66ffc0008fe61e0000ac67320000cfe76100008c62c08"
    "0000000000000000000000"; // 36x17
const char CN_IDLE[] PROGMEM =
    "000000000000000000000000000000043033f0000c3033300019fefb3000103033300007ff"
    "7330000c007b30000c0c7f30001dfff33c002c8cb23c000ccc363c000c4c363c000c3c3c38"
    "0000000000000000000000"; // 36x17
const char CN_NOZ[] PROGMEM =
    "00000000000000000000000c001b40000c0f5f800f7fcb5b400f0c0b5b600f318bffc00fff"
    "eb38000f318b3f800f000b63000f7fcbffc00f60cb66c00f64cb7fc00f6ccb66c00f6ecb7f"
    "c00c0f0f66c00039c8c6c000e04047c0000000000000000000000000000000000000000000"
    "00000"; // 39x23
const char CN_BED[] PROGMEM =
    "00000000000000000000061800380006180038001fff07ffe0061b060000065b061c00067b"
    "061c0007fb061c001e3b47ffc0063f663c0006676e7e0006616cff000e41cddd8004cc8f9c"
    "c00ecdcd1c400cecdc1c00086cc81c00000000000000000000000000000000000000000000"
    "00000000"; // 39x23
const char CN_CHAM[] PROGMEM =
    "0000000000000000000000300c0000007806ff8000fc02c18001ce00c180038708ff800703"
    "8cc1800fffccc180030706ff800307000000030604ffc0031e0e96c003008c96c00300cc96"
    "c00301cc96c001ff8fffe00000040000000000000000000000000000000000000000000000"
    "00000000"; // 39x23
const char CN_PROGRESS[] PROGMEM =
    "0000000000000000000004330038000c330fffe006330c000007ffccc30000330dffc00033"
    "0cc3001e330cc30006330cff0006ffec000006730dff8006630cc38006e30c770006230c3e"
    "000f001c7e0009ffd8e78008000b00c0000000000000000000000000000000000000000000"
    "00000000"; // 39x23
const char CN_TIME[] PROGMEM =
    "0000000000000000000000000000000000000000000000001fc2038000613f80031a0781f0"
    "6181803ffa1ce1b7f78180031a3871b06601800bba7ff9b466fd803bda0301fe66cd800bba"
    "7ff9b666fd803bfa0301b366cd80079a1361b266fd801b623339b066c18013026309f06601"
    "80030e0f01b3c60f8000000000000000000000000000000000000000000000000000000000"
    "000000000000000000000000"; // 63x20
const char CN_ETA[] PROGMEM =
    "000000000000000000000000000000000000000000003efe23006000d000c67e0006306300"
    "7000d9f0c3020016fc7ff7ff3ffd3feb02001cc4e3060330c130c802003ff4e300003fc934"
    "c9f2000af5e303fe32d9fcc912000af5efe000325136c9f2000af46c67ffb27136c9120008"
    "f46c6098326933c9f20008786c6199beed30c9020008cc6fe319e1adf0c8060039846c661f"
    "211937881e0000000000000000000000000000000000000000000000000000000000000000"
    "000000000000000000000000000000000000000000000000"; // 85x19

struct AmsTrayInfo {
  uint32_t trayColor = 0;
  int remain = -1;
  String trayType = "";
  String traySubBrands = "";
  String tagUid = "";
  bool isOfficial = false;
  bool valid = false;
};

bool isOfficialTag(const char *tag) {
  if (!tag || strlen(tag) == 0)
    return false;
  for (size_t i = 0; tag[i] != '\0'; i++) {
    if (tag[i] != '0' && tag[i] != ' ' && tag[i] != '\r' && tag[i] != '\n') {
      return true;
    }
  }
  return false;
}

struct PrinterState {
  float progress = -1;
  float nozzleTemp = -1;
  float leftNozzleTemp = -1;
  float rightNozzleTemp = -1;
  float bedTemp = -1;
  float chamberTemp = -1;
  int remainingMin = -1;
  int currentLayer = -1;
  int totalLayers = -1;
  String status = "prepare";
  String displayName = "";
  String model = "";
  String serial = "";
  bool online = true;
  bool dualNozzle = false;
  int activeTray = -1;
  int spdLvl = -1;
  int spdMag = -1;
  bool amsExist = false;
  AmsTrayInfo amsSlots[4];
  AmsTrayInfo extSlot;
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
  String model = "";
  String alias = "";
  String layout = "classic";
  String aliasBitmapHex = "";
  uint8_t aliasBitmapW = 0;
  uint8_t aliasBitmapH = 0;
  uint8_t brightness = 100;
  String brightnessSchedule = "";
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
  int chamberTemp = -999;
  int remainingMin = -999;
  int currentLayer = -999;
  int totalLayers = -999;
  String status = "";
  String displayName = "";
  String model = "";
  String alias = "";
  String layout = "";
  int activeTray = -999;
  uint32_t amsHash = 0;
  bool amsExist = false;
  int dashTimeRemaining = -999;
  String dashStatus = "";
  int spdLvl = -999;
  int spdMag = -999;
  int hour = -1;
  int min = -1;
  int sec = -1;
  int clockProgressPct = -999;
  int clockNozzleTemp = -999;
  int clockBedTemp = -999;
  int clockRemainingMin = -999;
  String clockStatus = "";
  bool clockOnline = true;
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
uint8_t appliedBrightness = 0;

const unsigned long DUAL_NOZZLE_SWITCH_MS = 3000;

void startEspServer();
String normalizedModelName(const String &value);
bool chamberFallbackAllowedForModel(const String &value);

bool wifiHasIp() { return WiFi.localIP() != IPAddress(0, 0, 0, 0); }

bool httpNetworkReady() { return wifiHasIp(); }

bool configClientConnected() { return false; }

float jsonFloat(JsonVariant v, float fallback) {
  if (v.is<float>() || v.is<int>())
    return v.as<float>();
  if (v.is<const char *>())
    return atof(v.as<const char *>());
  return fallback;
}

int jsonInt(JsonVariant v, int fallback) {
  if (v.is<int>())
    return v.as<int>();
  if (v.is<float>())
    return (int)v.as<float>();
  if (v.is<const char *>())
    return atoi(v.as<const char *>());
  return fallback;
}

const char *jsonStringForKeys(JsonObject obj, const char *const *keys,
                              size_t count) {
  for (size_t i = 0; i < count; ++i) {
    const char *value = obj[keys[i]] | "";
    if (value[0])
      return value;
  }
  return "";
}

float jsonFloatForKeys(JsonObject obj, const char *const *keys, size_t count,
                       float fallback) {
  for (size_t i = 0; i < count; ++i) {
    float value = jsonFloat(obj[keys[i]], fallback);
    if (value != fallback)
      return value;
  }
  return fallback;
}

int jsonIntForKeys(JsonObject obj, const char *const *keys, size_t count,
                   int fallback) {
  for (size_t i = 0; i < count; ++i) {
    int value = jsonInt(obj[keys[i]], fallback);
    if (value != fallback)
      return value;
  }
  return fallback;
}

float normalizePercent(float value) {
  if (value >= 0 && value < 1.0f)
    return value * 100.0f;
  return value;
}

float normalizeTemp(float value) {
  if (value > 65535.0f)
    return (float)(((uint32_t)value) & 0xFFFF);
  return value;
}

void storeNozzleTemp(int side, float temp, bool invertToolSide = true) {
  if (temp == -999)
    return;
  temp = normalizeTemp(temp);
  int displaySide = side;
  if (invertToolSide) {
    if (side == 0)
      displaySide = 1;
    else if (side == 1)
      displaySide = 0;
  }
  if (displaySide == 0)
    pr.leftNozzleTemp = temp;
  else if (displaySide == 1)
    pr.rightNozzleTemp = temp;
  if (pr.nozzleTemp < 0 || displaySide == 0)
    pr.nozzleTemp = temp;
}

float firstTempFromInfo(JsonArray info, float fallback) {
  for (JsonObject item : info) {
    float temp = jsonFloat(item["temp"], -999);
    if (temp != -999)
      return normalizeTemp(temp);
  }
  return fallback;
}

bool applyNozzleInfo(JsonArray info) {
  if (info.isNull() || info.size() == 0)
    return false;

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
  if (dual && any)
    pr.dualNozzle = true;
  return any;
}

bool applyNestedNozzleTemps(JsonObject print) {
  JsonObject device = print["device"].as<JsonObject>();
  if (device.isNull())
    return false;

  bool got = applyNozzleInfo(device["extruder"]["info"].as<JsonArray>());
  if (applyNozzleInfo(device["nozzle"]["info"].as<JsonArray>()))
    got = true;
  return got;
}

float nestedNozzleTemp(JsonObject print, float fallback) {
  JsonObject device = print["device"].as<JsonObject>();
  if (device.isNull())
    return fallback;

  float temp =
      firstTempFromInfo(device["extruder"]["info"].as<JsonArray>(), fallback);
  if (temp != fallback)
    return temp;
  temp = firstTempFromInfo(device["nozzle"]["info"].as<JsonArray>(), fallback);
  if (temp != fallback)
    return temp;
  return fallback;
}

float nestedBedTemp(JsonObject print, float fallback) {
  JsonObject device = print["device"].as<JsonObject>();
  if (device.isNull())
    return fallback;

  int packed = jsonInt(device["bed"]["info"]["temp"], -999);
  if (packed != -999)
    return normalizeTemp((float)packed);
  float temp =
      firstTempFromInfo(device["bed"]["info"].as<JsonArray>(), fallback);
  if (temp != fallback)
    return temp;
  packed = jsonInt(device["bed_temp"], -999);
  if (packed != -999)
    return normalizeTemp((float)packed);
  return fallback;
}

bool hasToken(const String &s, const char *token) {
  String v = s;
  v.toLowerCase();
  return v.indexOf(token) >= 0;
}

bool isPrintingState(const String &s) {
  return hasToken(s, "running") || hasToken(s, "printing") ||
         hasToken(s, "processing");
}

bool isPreparingState(const String &s) {
  return hasToken(s, "prepare") || hasToken(s, "starting") ||
         hasToken(s, "heating") || hasToken(s, "download") ||
         s.equalsIgnoreCase("init") || s.equalsIgnoreCase("slicing");
}

bool isPausedState(const String &s) { return hasToken(s, "pause"); }

bool isFinishedState(const String &s) {
  return hasToken(s, "finish") || hasToken(s, "success") ||
         hasToken(s, "done") || hasToken(s, "complete");
}

bool isFailedState(const String &s) {
  return hasToken(s, "fail") || hasToken(s, "error") || hasToken(s, "cancel");
}

uint8_t normalizeBrightness(int value) {
  if (value < 0)
    return 0;
  if (value > 100)
    return 100;
  return (uint8_t)value;
}

void applyBrightness(uint8_t percent) {
  percent = normalizeBrightness(percent);
  if (appliedBrightness == percent)
    return;
  int duty = 1023 - ((int)percent * 1023 / 100);
  analogWrite(LCD_BL_PIN, duty);
  appliedBrightness = percent;
}
void applyBrightnessSchedule() {
  if (stored.brightnessSchedule.length() == 0) {
    applyBrightness(stored.brightness);
    return;
  }
  time_t now = time(nullptr);
  if (now < 1700000000) {
    applyBrightness(stored.brightness);
    return;
  }
  struct tm *info = localtime(&now);
  if (!info)
    return;
  int curMin = info->tm_hour * 60 + info->tm_min;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, stored.brightnessSchedule);
  if (err) {
    applyBrightness(stored.brightness);
    return;
  }
  JsonArray arr = doc.as<JsonArray>();
  bool matched = false;
  for (size_t i = 0; i < arr.size(); i++) {
    JsonObject slot = arr[i];
    int sh = slot["sh"] | -1;
    int sm = slot["sm"] | 0;
    int eh = slot["eh"] | -1;
    int em = slot["em"] | 0;
    int b = slot["b"] | -1;
    if (sh < 0 || eh < 0 || b < 0)
      continue;
    int startMin = sh * 60 + sm;
    int endMin = eh * 60 + em;
    bool inSlot = false;
    if (startMin == endMin) {
      inSlot = false;
    } else if (endMin > startMin) {
      inSlot = curMin >= startMin && curMin < endMin;
    } else {
      inSlot = curMin >= startMin || curMin < endMin;
    }
    if (inSlot) {
      applyBrightness((uint8_t)b);
      matched = true;
      break;
    }
  }
  if (!matched) {
    applyBrightness(stored.brightness);
  }
}

void resetLivePrintFields() {
  pr.progress = -1;
  pr.nozzleTemp = -1;
  pr.leftNozzleTemp = -1;
  pr.rightNozzleTemp = -1;
  pr.dualNozzle = false;
  pr.bedTemp = -1;
  pr.chamberTemp = -1;
  pr.remainingMin = -1;
  pr.currentLayer = -1;
  pr.totalLayers = -1;
  pr.status = "prepare";
  pr.activeTray = -1;
  pr.spdLvl = 1;
  pr.spdMag = 100;
  pr.amsExist = false;
  pr.extSlot.valid = false;
  pr.extSlot.trayColor = 0;
  pr.extSlot.remain = -1;
  pr.extSlot.trayType = "";
  pr.extSlot.traySubBrands = "";
  pr.extSlot.tagUid = "";
  pr.extSlot.isOfficial = false;
  for (int i = 0; i < 4; i++) {
    pr.amsSlots[i].valid = false;
    pr.amsSlots[i].trayColor = 0;
    pr.amsSlots[i].remain = -1;
    pr.amsSlots[i].trayType = "";
    pr.amsSlots[i].traySubBrands = "";
    pr.amsSlots[i].tagUid = "";
    pr.amsSlots[i].isOfficial = false;
  }
  printerStatusReceived = false;
  displayDirty = true;
}

void wifiConnect() {
  if (WiFi.status() == WL_CONNECTED)
    return;
  if (!stored.wifiSsid.length()) {
    Serial.println("WiFi not configured");
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.begin(stored.wifiSsid.c_str(), stored.wifiPassword.c_str());
  for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; ++i)
    delay(500);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected, IP=");
    Serial.println(WiFi.localIP());
    configTime(8 * 3600, 0, "ntp.aliyun.com", "cn.pool.ntp.org",
               "pool.ntp.org");
  } else {
    Serial.println("WiFi connect timeout");
  }
}

void startSetupAp() {
  setupApStarted = false;
  setupApSsid = "";
}

bool saveStoredConfig() {
  if (!LittleFS.begin())
    return false;
  File file = LittleFS.open("/cloud.json", "w");
  if (!file)
    return false;
  JsonDocument doc;
  doc["wifi_ssid"] = stored.wifiSsid;
  doc["wifi_password"] = stored.wifiPassword;
  doc["region"] = stored.region;
  doc["mqtt_host"] = stored.mqttHost;
  doc["mqtt_username"] = stored.mqttUsername;
  doc["token"] = stored.token;
  doc["serial"] = stored.serial;
  doc["name"] = stored.name;
  doc["model"] = stored.model;
  doc["alias"] = stored.alias;
  doc["layout"] = stored.layout;
  doc["alias_bitmap_hex"] = stored.aliasBitmapHex;
  doc["alias_bitmap_w"] = stored.aliasBitmapW;
  doc["alias_bitmap_h"] = stored.aliasBitmapH;
  doc["brightness"] = stored.brightness;
  doc["brightness_schedule"] = stored.brightnessSchedule;
  serializeJson(doc, file);
  file.close();
  return true;
}

bool savePrinterOptions() {
  if (!LittleFS.begin())
    return false;
  File file = LittleFS.open("/printers.json", "w");
  if (!file)
    return false;
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
  if (arr.isNull())
    return false;
  clearPrinterOptions();
  for (JsonObject item : arr) {
    if (printerOptionCount >= MAX_PRINTER_OPTIONS)
      break;
    const char *serial =
        item["serial"] | item["dev_id"] | item["device_id"] | "";
    if (!serial[0])
      continue;
    printerOptions[printerOptionCount].serial = serial;
    printerOptions[printerOptionCount].name =
        item["name"] | item["display_name"] | item["dev_name"] | serial;
    printerOptions[printerOptionCount].model = item["model"] | "";
    printerOptionCount++;
  }
  return true;
}

void loadPrinterOptions() {
  clearPrinterOptions();
  if (!LittleFS.begin())
    return;
  if (!LittleFS.exists("/printers.json"))
    return;
  File file = LittleFS.open("/printers.json", "r");
  if (!file)
    return;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, file);
  file.close();
  if (err)
    return;
  loadPrinterArray(doc["printers"].as<JsonArray>());
}

void loadStoredConfig() {
  if (!LittleFS.begin())
    return;
  if (!LittleFS.exists("/cloud.json"))
    return;
  File file = LittleFS.open("/cloud.json", "r");
  if (!file)
    return;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, file);
  file.close();
  if (err)
    return;
  stored.wifiSsid = doc["wifi_ssid"] | stored.wifiSsid;
  stored.wifiPassword = doc["wifi_password"] | stored.wifiPassword;
  stored.region = doc["region"] | stored.region;
  stored.mqttHost = doc["mqtt_host"] | stored.mqttHost;
  stored.mqttUsername = doc["mqtt_username"] | "";
  stored.token = doc["token"] | "";
  stored.serial = doc["serial"] | "";
  stored.name = doc["name"] | "";
  stored.model = doc["model"] | "";
  stored.alias = doc["alias"] | "";
  stored.layout = doc["layout"] | "classic";
  stored.aliasBitmapHex = doc["alias_bitmap_hex"] | "";
  stored.aliasBitmapW = doc["alias_bitmap_w"] | 0;
  stored.aliasBitmapH = doc["alias_bitmap_h"] | 0;
  stored.brightness = normalizeBrightness(doc["brightness"] | 100);
  stored.brightnessSchedule = doc["brightness_schedule"] | "";
  pr.serial = stored.serial;
  pr.displayName = stored.name;
  pr.model = stored.model;
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
  doc["ap_ip"] = "";
  doc["ap_ssid"] = "";
  doc["serial"] = pr.serial.length() ? pr.serial : stored.serial;
  doc["name"] = pr.displayName.length() ? pr.displayName : stored.name;
  doc["model"] =
      normalizedModelName(pr.model.length() ? pr.model : stored.model);
  doc["layout"] = stored.layout;
  doc["mqtt_host"] = stored.mqttHost;
  doc["mqtt_username"] = stored.mqttUsername.length() ? "set" : "";
  doc["mqtt_connected"] = mqttNet.connected();
  doc["brightness"] = appliedBrightness;
  doc["printer_count"] = printerOptionCount;
  doc["online"] = pr.online;
  doc["status"] = pr.status;
  doc["progress"] = pr.progress;
  doc["nozzle_temp"] = pr.nozzleTemp;
  doc["left_nozzle_temp"] = pr.leftNozzleTemp;
  doc["right_nozzle_temp"] = pr.rightNozzleTemp;
  doc["dual_nozzle"] = pr.dualNozzle;
  doc["bed_temp"] = pr.bedTemp;
  doc["chamber_temp"] = pr.chamberTemp;
  doc["remaining_min"] = pr.remainingMin;
  doc["current_layer"] = pr.currentLayer;
  doc["total_layers"] = pr.totalLayers;
  String out;
  serializeJson(doc, out);
  return out;
}

String jsonEscape(const String &text) {
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
  if (rssi <= -100)
    return 0;
  if (rssi >= -50)
    return 100;
  return 2 * (rssi + 100);
}

String wifiScanJson() {
  int count = WiFi.scanNetworks(false, true);
  String out = "{\"ok\":true,\"source\":\"esp\",\"networks\":[";
  int added = 0;
  for (int i = 0; i < count && added < 24; ++i) {
    String ssid = WiFi.SSID(i);
    if (!ssid.length())
      continue;
    bool duplicate = false;
    for (int j = 0; j < i; ++j) {
      if (WiFi.SSID(j) == ssid) {
        duplicate = true;
        break;
      }
    }
    if (duplicate)
      continue;
    int32_t rssi = WiFi.RSSI(i);
    if (added)
      out += ',';
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

String htmlEscape(const String &text) {
  String out;
  out.reserve(text.length() + 8);
  for (size_t i = 0; i < text.length(); ++i) {
    char c = text[i];
    if (c == '&')
      out += "&amp;";
    else if (c == '<')
      out += "&lt;";
    else if (c == '>')
      out += "&gt;";
    else if (c == '"')
      out += "&quot;";
    else
      out += c;
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

bool selectPrinterBySerial(const String &serial) {
  if (!serial.length())
    return false;
  for (uint8_t i = 0; i < printerOptionCount; ++i) {
    if (printerOptions[i].serial == serial) {
      if (stored.serial != printerOptions[i].serial) {
        stored.serial = printerOptions[i].serial;
        stored.name = printerOptions[i].name.length()
                          ? printerOptions[i].name
                          : printerOptions[i].serial;
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
  String selected = stored.name.length() ? stored.name : stored.serial;
  String modelName = normalizedModelName(stored.model);
  bool hasSchedule = stored.brightnessSchedule.length() > 2;

  String body;
  body.reserve(7500);
  body += F(
      "<!doctype html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\"><meta "
      "name=\"viewport\" content=\"width=device-width,initial-scale=1\">");
  body +=
      F("<link rel=\"icon\" type=\"image/svg+xml\" "
        "href=\"data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' "
        "viewBox='0 0 100 120'%3E%3Crect width='100' height='120' rx='16' "
        "fill='%230b0e17'/%3E%3Cpolygon points='13,13 47,13 47,54 13,69' "
        "fill='%23ffffff'/%3E%3Cpolygon points='13,75 47,60 47,107 13,107' "
        "fill='%23ffffff'/%3E%3Cpolygon points='53,13 87,13 87,55 53,39' "
        "fill='%23ffffff'/%3E%3Cpolygon points='53,45 87,61 87,107 53,107' "
        "fill='%23ffffff'/%3E%3C/svg%3E\">");
  body += F("<title>PrintSphere Lite Plus</title><style>");
  body += F("*{box-sizing:border-box;margin:0;padding:0;font-family:-apple-"
            "system,BlinkMacSystemFont,\"SF Pro Display\",\"SF Pro "
            "Text\",\"Segoe UI\",Roboto,sans-serif}");
  body += F("body{background:#0b0e17;background-image:radial-gradient(at 0% "
            "0%,rgba(0,122,255,0.18) 0px,transparent 50%),radial-gradient(at "
            "100% 100%,rgba(175,82,222,0.18) 0px,transparent "
            "50%);color:#f2f2f7;min-height:100vh;padding:24px "
            "16px;display:flex;justify-content:center;align-items:flex-start}");
  body += F(".container{width:100%;max-width:880px}");
  body += F(".header{margin-bottom:24px;text-align:left}");
  body += F(".header "
            "h1{font-size:28px;font-weight:700;letter-spacing:-0.5px;"
            "background:linear-gradient(135deg,#ffffff 0%,#a1a1a6 "
            "100%);-webkit-background-clip:text;-webkit-text-fill-color:"
            "transparent}");
  body += F(".header .sub{color:#8e8e93;font-size:14px;margin-top:4px}");
  body += F(".grid{display:grid;grid-template-columns:1fr;gap:16px}@media(min-"
            "width:640px){.grid{grid-template-columns:repeat(2,1fr)}}");
  body += F(".glass{background:rgba(255,255,255,0.06);-webkit-backdrop-filter:"
            "blur(30px) saturate(190%);backdrop-filter:blur(30px) "
            "saturate(190%);border:1px solid "
            "rgba(255,255,255,0.12);border-radius:20px;padding:20px;box-shadow:"
            "0 8px 32px 0 rgba(0,0,0,0.37);transition:transform .2s "
            "ease,border-color .2s ease}");
  body += F(".glass:hover{border-color:rgba(255,255,255,0.22)}");
  body +=
      F(".glass "
        "h2{font-size:16px;font-weight:600;color:#f2f2f7;margin-bottom:14px;"
        "display:flex;align-items:center;justify-content:space-between}");
  body +=
      F(".row{display:flex;justify-content:space-between;align-items:center;"
        "padding:8px 0;border-bottom:1px solid rgba(255,255,255,0.05)}");
  body += F(".row:last-child{border-bottom:none}");
  body += F(".label{color:#98989d;font-size:14px}");
  body += F(".value{color:#ffffff;font-size:14px;font-weight:500}");
  body += F(".badge{padding:4px "
            "10px;border-radius:12px;font-size:12px;font-weight:600;display:"
            "inline-block}");
  body += F(".badge.ok{background:rgba(52,199,89,0.2);color:#30d158;border:1px "
            "solid rgba(52,199,89,0.3)}");
  body += F(".badge.err{background:rgba(255,69,58,0.2);color:#ff453a;border:"
            "1px solid rgba(255,69,58,0.3)}");
  body += F(".slider-container{margin-top:10px;text-align:center}");
  body += F(".slider-val{font-size:32px;font-weight:700;color:#0a84ff;letter-"
            "spacing:-1px;margin-bottom:8px}");
  body += F("input[type=range]{width:100%;height:8px;-webkit-appearance:none;"
            "background:rgba(255,255,255,0.12);border-radius:4px;outline:none;"
            "margin:8px 0;transition:opacity .2s}");
  body +=
      F("input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:"
        "22px;height:22px;border-radius:50%;background:#ffffff;box-shadow:0 "
        "2px 8px rgba(0,0,0,0.4);cursor:pointer;transition:transform .1s "
        "ease,background .2s ease}");
  body += F(
      "input[type=range]::-webkit-slider-thumb:active{transform:scale(1.15)}");
  body += F("input[type=range]:disabled{opacity:0.25;cursor:not-allowed}");
  body += F("input[type=range]:disabled::-webkit-slider-thumb{background:#"
            "636366;box-shadow:none;cursor:not-allowed}");
  body +=
      F(".segmented{display:flex;background:rgba(0,0,0,0.3);padding:3px;border-"
        "radius:12px;border:1px solid rgba(255,255,255,0.08);gap:4px}");
  body += F(
      ".segmented button{flex:1;padding:8px "
      "12px;border:none;border-radius:9px;background:transparent;color:#98989d;"
      "font-size:13px;font-weight:500;cursor:pointer;transition:all .2s ease}");
  body += F(".segmented "
            "button.active{background:rgba(255,255,255,0.2);color:#ffffff;box-"
            "shadow:0 2px 8px rgba(0,0,0,0.25);font-weight:600}");
  body += F(
      ".switch{position:relative;display:inline-block;width:48px;height:28px}");
  body += F(".switch input{opacity:0;width:0;height:0}");
  body += F(".slider-round{position:absolute;cursor:pointer;top:0;left:0;right:"
            "0;bottom:0;background:rgba(255,255,255,0.15);border-radius:28px;"
            "transition:.3s;border:1px solid rgba(255,255,255,0.1)}");
  body += F(".slider-round:before{position:absolute;content:\"\";height:22px;"
            "width:22px;left:2px;bottom:2px;background:#ffffff;border-radius:"
            "50%;transition:.3s;box-shadow:0 2px 4px rgba(0,0,0,0.3)}");
  body += F("input:checked + .slider-round{background:#34c759}");
  body += F("input:checked + .slider-round:before{transform:translateX(20px)}");
  body += F(".slot-box{background:rgba(0,0,0,0.2);border:1px solid "
            "rgba(255,255,255,0.08);border-radius:14px;padding:12px;margin-top:"
            "10px}");
  body += F(".slot-row{display:flex;align-items:center;justify-content:space-"
            "between;gap:8px;margin-bottom:8px}");
  body += F(".slot-row:last-child{margin-bottom:0}");
  body +=
      F("input[type=time]{background:rgba(255,255,255,0.1);border:1px solid "
        "rgba(255,255,255,0.15);border-radius:8px;color:#fff;padding:4px "
        "8px;font-size:13px;outline:none}");
  body += F("pre{background:rgba(0,0,0,0.4);border:1px solid "
            "rgba(255,255,255,0.1);border-radius:12px;padding:12px;font-size:"
            "12px;color:#30d158;overflow-x:auto;white-space:pre-wrap;margin-"
            "top:12px;display:none}");
  body += F(".btn-action{background:rgba(255,255,255,0.1);border:1px solid "
            "rgba(255,255,255,0.15);color:#0a84ff;padding:6px "
            "14px;border-radius:10px;font-size:13px;font-weight:500;cursor:"
            "pointer;transition:all .2s}");
  body += F(".btn-action:hover{background:rgba(255,255,255,0.18)}");
  body += F(".btn-primary{width:100%;padding:10px "
            "16px;margin-top:14px;background:linear-gradient(135deg,#0a84ff "
            "0%,#0066cc 100%);border:1px solid "
            "rgba(255,255,255,0.25);border-radius:12px;color:#ffffff;font-size:"
            "14px;font-weight:600;cursor:pointer;box-shadow:0 4px 16px "
            "rgba(10,132,255,0.35);transition:all .2s ease}");
  body += F(".btn-primary:hover{transform:translateY(-1px);box-shadow:0 6px "
            "20px rgba(10,132,255,0.5)}");
  body += F(".btn-primary:active{transform:translateY(0)}");
  body += F(".toast{position:fixed;bottom:24px;left:50%;transform:translateX(-"
            "50%);background:rgba(40,40,40,0.9);-webkit-backdrop-filter:blur("
            "20px);backdrop-filter:blur(20px);color:#fff;padding:10px "
            "22px;border-radius:20px;font-size:13px;font-weight:500;border:1px "
            "solid rgba(255,255,255,0.2);box-shadow:0 10px 30px "
            "rgba(0,0,0,0.5);opacity:0;pointer-events:none;transition:opacity "
            ".3s ease;z-index:999}");
  body += F(".toast.show{opacity:1}");
  body += F("</style></head><body><div class=\"container\">");

  body += F("<div class=\"header\"><h1>PrintSphere Lite Plus</h1><p "
            "class=\"sub\">固件: ");
  body += FIRMWARE_VERSION;
  body += F(" | IP: ");
  body += htmlEscape(ip);
  body += F("</p></div>");

  body += F("<div class=\"grid\">");

  // Card 1: Device Status
  body += F("<div class=\"glass\"><h2>设备状态</h2>");
  body += F("<div class=\"row\"><span class=\"label\">打印机</span><span "
            "class=\"value\">");
  body += htmlEscape(selected.length() ? selected : String("未选择"));
  body += F("</span></div>");
  body += F("<div class=\"row\"><span class=\"label\">机型</span><span "
            "class=\"value\">");
  body += htmlEscape(modelName);
  body += F("</span></div>");
  body += F("<div class=\"row\"><span class=\"label\">MQTT</span><span "
            "class=\"value\"><span class=\"badge ");
  body += mqttNet.connected() ? F("ok") : F("err");
  body += F("\">");
  body += mqttNet.connected() ? F("已连接") : F("未连接");
  body += F("</span></span></div></div>");

  // Card 2: Screen Brightness
  body += F("<div class=\"glass\"><h2>屏幕亮度</h2><div "
            "class=\"slider-container\"><div class=\"slider-val\" id=\"bv\">");
  body += String(appliedBrightness);
  body += F("%</div>");
  body += F("<input type=\"range\" id=\"br\" min=\"0\" max=\"100\" value=\"");
  body += String(appliedBrightness);
  body += F("\">");
  body += F("<p id=\"brHint\" "
            "style=\"color:#8e8e93;font-size:12px;margin-top:6px\">"
            "拖动实时应用背光</p></div></div>");

  // Card 3: Screen Layout
  body += F("<div class=\"glass\"><h2>屏幕布局</h2><div class=\"segmented\">");
  String lc = stored.layout;
  body += F("<button id=\"lc0\" onclick=\"setLayout('classic')\"");
  if (lc == "classic")
    body += F(" class=\"active\"");
  body += F(">经典</button>");
  body += F("<button id=\"lc1\" onclick=\"setLayout('dashboard')\"");
  if (lc == "dashboard")
    body += F(" class=\"active\"");
  body += F(">面板</button>");
  body += F("<button id=\"lc2\" onclick=\"setLayout('clock')\"");
  if (lc == "clock")
    body += F(" class=\"active\"");
  body += F(">时钟</button>");
  body += F("</div></div>");

  // Card 4: Brightness Schedule
  body += F(
      "<div class=\"glass\"><h2><span>亮度定时</span><label class=\"switch\">");
  body +=
      F("<input type=\"checkbox\" id=\"bse\" onchange=\"toggleSchedule()\"");
  if (hasSchedule)
    body += F(" checked");
  body += F("><span class=\"slider-round\"></span></label></h2>");

  body += F("<div id=\"schedFields\"");
  if (!hasSchedule)
    body += F(" style=\"display:none\"");
  body += F("><div class=\"slot-box\">");

  // Night Slot 0
  body += F("<div class=\"slot-row\"><span class=\"label\">夜间段</span><div>");
  body += F("<input type=\"time\" id=\"s0s\" value=\"22:00\"><span "
            "style=\"color:#8e8e93;margin:0 4px\">~</span>");
  body += F("<input type=\"time\" id=\"s0e\" value=\"06:00\"></div></div>");
  body += F("<div class=\"slot-row\"><span class=\"label\">夜间亮度</span>");
  body +=
      F("<div style=\"display:flex;align-items:center;gap:8px;width:60%\">");
  body += F("<input type=\"range\" id=\"s0b\" min=\"0\" max=\"100\" "
            "value=\"20\"><span id=\"s0bv\" "
            "style=\"color:#0a84ff;font-weight:600;min-width:32px;text-align:"
            "right\">20%</span></div></div></div>");

  // Day Slot 1
  body += F("<div class=\"slot-box\"><div class=\"slot-row\"><span "
            "class=\"label\">日间段</span><div>");
  body += F("<input type=\"time\" id=\"s1s\" value=\"06:00\"><span "
            "style=\"color:#8e8e93;margin:0 4px\">~</span>");
  body += F("<input type=\"time\" id=\"s1e\" value=\"22:00\"></div></div>");
  body += F("<div class=\"slot-row\"><span class=\"label\">日间亮度</span>");
  body +=
      F("<div style=\"display:flex;align-items:center;gap:8px;width:60%\">");
  body += F("<input type=\"range\" id=\"s1b\" min=\"0\" max=\"100\" "
            "value=\"100\"><span id=\"s1bv\" "
            "style=\"color:#0a84ff;font-weight:600;min-width:32px;text-align:"
            "right\">100%</span></div></div></div>");

  body += F("<button class=\"btn-primary\" "
            "onclick=\"saveSchedule(true)\">保存并推送到设备</button>");
  body += F("</div></div>");

  // Card 5: Real-time Debug Status
  body += F("<div class=\"glass\" style=\"grid-column:1 / "
            "-1\"><h2><span>实时调试数据</span>");
  body += F("<button class=\"btn-action\" onclick=\"toggleJson(this)\">查看 "
            "JSON</button></h2><pre id=\"log\"></pre></div>");

  body +=
      F("</div></div><div id=\"toast\" class=\"toast\"></div>"); // end grid,
                                                                 // container,
                                                                 // toast

  // JavaScript
  body += F("<script>\n");
  body += F("const savedSchedule = ");
  body += (stored.brightnessSchedule.length() > 0 ? stored.brightnessSchedule
                                                  : F("[]"));
  body += F(";\n");
  body += F("function showToast(msg){\n");
  body += F("  let t=document.getElementById('toast');if(!t)return;\n");
  body += F("  t.textContent=msg;t.classList.add('show');\n");
  body += F("  setTimeout(()=>t.classList.remove('show'),2500);\n");
  body += F("}\n");
  body += F("function updateManualBrightnessState(){\n");
  body += F("  var c=document.getElementById('bse'), "
            "hint=document.getElementById('brHint');\n");
  body += F("  if(!hint)return;\n");
  body += F("  if(c&&c.checked){\n");
  body += F("    hint.textContent='显示目前屏幕实时亮度（已启用定时规则）';\n");
  body += F("  }else{\n");
  body += F("    hint.textContent='拖动实时应用背光';\n");
  body += F("  }\n");
  body += F("}\n");
  body += F("function initScheduleUI(){\n");
  body += F("  if(Array.isArray(savedSchedule) && savedSchedule.length>0){\n");
  body += F("    for(let i=0;i<Math.min(savedSchedule.length,2);i++){\n");
  body += F("      let s=savedSchedule[i];\n");
  body += F("      if(s.sh!==undefined){\n");
  body +=
      F("        let st=(s.sh<10?'0':'')+s.sh+':'+(s.sm<10?'0':'')+s.sm;\n");
  body +=
      F("        let et=(s.eh<10?'0':'')+s.eh+':'+(s.em<10?'0':'')+s.em;\n");
  body += F("        let elS=document.getElementById('s'+i+'s'), "
            "elE=document.getElementById('s'+i+'e'), "
            "elB=document.getElementById('s'+i+'b'), "
            "elBv=document.getElementById('s'+i+'bv');\n");
  body += F("        if(elS)elS.value=st; if(elE)elE.value=et;\n");
  body += F("        if(elB && s.b!==undefined){elB.value=s.b; "
            "if(elBv)elBv.textContent=s.b+'%';}\n");
  body += F("      }\n");
  body += F("    }\n");
  body += F("  }\n");
  body += F("  updateManualBrightnessState();\n");
  body += F("}\n");
  body += F("window.addEventListener('DOMContentLoaded', initScheduleUI);\n");

  body +=
      F("function "
        "refreshStatus(){fetch('/api/"
        "status',{cache:'no-store'}).then(r=>r.json()).then(d=>{let "
        "l=document.getElementById('log');if(l&&l.style.display==='block')l."
        "textContent=JSON.stringify(d,null,2);if(d.brightness!==undefined){var "
        "br=document.getElementById('br'),bv=document.getElementById('bv');if("
        "br)br.value=d.brightness;if(bv)bv.textContent=d.brightness+'%';}})."
        "catch(e=>{});}\n");
  body += F("setInterval(refreshStatus,3000);\n");
  body +=
      F("function toggleJson(btn){const "
        "el=document.getElementById('log');if(el.style.display==='none'||!el."
        "style.display){el.style.display='block';refreshStatus();btn."
        "textContent='隐藏 "
        "JSON'}else{el.style.display='none';btn.textContent='查看 JSON'}}\n");
  body += F("function "
            "postConfig(data){fetch('/api/"
            "config',{method:'POST',headers:{'Content-Type':'application/"
            "json'},body:JSON.stringify(data)}).then(()=>refreshStatus())."
            "catch(e=>console.error(e));}\n");
  body += F("let "
            "brt=document.getElementById('br');if(brt){brt.addEventListener('"
            "input',function(){document.getElementById('bv').textContent=this."
            "value+'%';});brt.addEventListener('change',function(){postConfig({"
            "brightness:parseInt(this.value)});});}\n");
  body += F("function setLayout(v){document.querySelectorAll('.segmented "
            "button').forEach(b=>b.classList.remove('active'));const "
            "idx=['classic','dashboard','clock'].indexOf(v);if(idx>=0)document."
            "getElementById('lc'+idx).classList.add('active');postConfig({"
            "layout:v});}\n");
  body += F("function toggleSchedule(){var "
            "e=document.getElementById('schedFields'),c=document."
            "getElementById('bse');e.style.display=c.checked?'block':'none';"
            "updateManualBrightnessState();saveSchedule(false);}\n");
  body += F("function saveSchedule(notify){\n");
  body += F("  var s=document.getElementById('bse');\n");
  body += F("  if(!s.checked){\n");
  body += F("    postConfig({brightness_schedule:''});\n");
  body += F("    if(notify) showToast('✅ 定时已关闭并即时生效');\n");
  body += F("    return;\n");
  body += F("  }\n");
  body += F("  var slots=[];\n");
  body += F("  for(var i=0;i<2;i++){\n");
  body += F("    var "
            "st=document.getElementById('s'+i+'s').value,et=document."
            "getElementById('s'+i+'e').value,b=parseInt(document."
            "getElementById('s'+i+'b').value);\n");
  body += F("    if(st&&et){\n");
  body += F("      var "
            "sh=parseInt(st.split(':')[0]),sm=parseInt(st.split(':')[1]),eh="
            "parseInt(et.split(':')[0]),em=parseInt(et.split(':')[1]);\n");
  body += F("      slots.push({sh:sh,sm:sm,eh:eh,em:em,b:b});\n");
  body += F("    }\n");
  body += F("  }\n");
  body += F("  postConfig({brightness_schedule:JSON.stringify(slots)});\n");
  body += F("  if(notify) showToast('✅ 已保存并即时推送到设备！');\n");
  body += F("}\n");
  body += F("document.querySelectorAll('#schedFields "
            "input').forEach(function(el){\n");
  body += F("  if(el.type==='range'){\n");
  body += F("    "
            "el.addEventListener('input',function(){document.getElementById("
            "this.id+'v').textContent=this.value+'%';});\n");
  body += F("  }\n");
  body += F("});\n");
  body += F("</script></body></html>");

  return body;
}

String applyConfigBody(const String &body, int &statusCode) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    statusCode = 400;
    return "{\"ok\":false,\"error\":\"bad json\"}";
  }

  const char *region = doc["region"] | "";
  const char *wifiSsid = doc["wifi_ssid"] | "";
  const char *wifiPassword = doc["wifi_password"] | "";
  const char *mqttHost = doc["mqtt_host"] | "";
  const char *mqttUsername = doc["mqtt_username"] | "";
  const char *token = doc["token"] | doc["access_token"] | "";
  const char *serial = doc["serial"] | doc["printer_serial"] | "";
  const char *name = doc["name"] | doc["display_name"] | "";
  const char *model = doc["model"] | doc["printer_model"] | "";
  const char *alias = doc["alias"] | doc["display_alias"] | "";
  const char *layout = doc["layout"] | doc["display_layout"] | "";
  const char *aliasBitmapHex = doc["alias_bitmap_hex"] | "";
  const char *printersPayload = doc["printers_json"] | "";
  int requestedBrightness = doc["brightness"] | doc["brightness_percent"] | -1;
  bool wifiChanged = false;
  bool mqttChanged = false;
  bool brightnessChanged = false;

  if (printersPayload[0]) {
    JsonDocument printersDoc;
    if (!deserializeJson(printersDoc, printersPayload)) {
      JsonArray arr = printersDoc.is<JsonArray>()
                          ? printersDoc.as<JsonArray>()
                          : printersDoc["printers"].as<JsonArray>();
      if (loadPrinterArray(arr))
        savePrinterOptions();
    }
  }
  if (doc["printers"].is<JsonArray>()) {
    if (loadPrinterArray(doc["printers"].as<JsonArray>()))
      savePrinterOptions();
  }
  if (wifiSsid[0] && stored.wifiSsid != wifiSsid) {
    stored.wifiSsid = wifiSsid;
    wifiChanged = true;
  }
  if (wifiPassword[0] && stored.wifiPassword != wifiPassword) {
    stored.wifiPassword = wifiPassword;
    wifiChanged = true;
  }
  if (region[0])
    stored.region = region;
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
      pr.chamberTemp = -1;
      pr.remainingMin = -1;
      pr.currentLayer = -1;
      pr.totalLayers = -1;
      pr.status = "prepare";
      printerStatusReceived = false;
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
  if (model[0]) {
    stored.model = model;
    pr.model = model;
    cache.baseDrawn = false;
  }
  if (alias[0] || doc["alias"].is<const char *>() ||
      doc["display_alias"].is<const char *>()) {
    stored.alias = alias;
    cache.baseDrawn = false;
  }
  if (layout[0]) {
    String nextLayout = layout;
    nextLayout.toLowerCase();
    stored.layout = nextLayout == "dashboard" ? "dashboard"
                    : nextLayout == "clock"   ? "clock"
                                              : "classic";
    cache.baseDrawn = false;
  }
  if (aliasBitmapHex[0] || doc["alias_bitmap_hex"].is<const char *>()) {
    stored.aliasBitmapHex = aliasBitmapHex;
    stored.aliasBitmapW = doc["alias_bitmap_w"] | 0;
    stored.aliasBitmapH = doc["alias_bitmap_h"] | 0;
    cache.baseDrawn = false;
  }

  bool scheduleChanged = false;
  if (doc["brightness_schedule"].is<const char *>() ||
      doc["brightness_schedule"].is<JsonVariant>()) {
    stored.brightnessSchedule = doc["brightness_schedule"].as<String>();
    scheduleChanged = true;
  }

  if (requestedBrightness >= 0) {
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
  if (brightnessChanged || scheduleChanged) {
    applyBrightnessSchedule();
  }

  JsonDocument outDoc;
  outDoc["ok"] = ok;
  outDoc["serial"] = stored.serial;
  outDoc["name"] = stored.name;
  outDoc["ip"] = WiFi.localIP().toString();
  outDoc["brightness"] = stored.brightness;
  outDoc["layout"] = stored.layout;
  String out;
  serializeJson(outDoc, out);
  statusCode = ok ? 200 : 500;
  return out;
}

String handleSerialJson(const String &body, int &statusCode) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (!err) {
    const char *cmd = doc["cmd"] | "";
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

String urlDecode(const String &text) {
  String out;
  out.reserve(text.length());
  for (size_t i = 0; i < text.length(); ++i) {
    char c = text[i];
    if (c == '+') {
      out += ' ';
    } else if (c == '%' && i + 2 < text.length()) {
      char hex[3] = {text[i + 1], text[i + 2], 0};
      out += (char)strtol(hex, nullptr, 16);
      i += 2;
    } else {
      out += c;
    }
  }
  return out;
}

String configBodyFromQuery(const String &query) {
  JsonDocument doc;
  size_t start = 0;
  while (start < query.length()) {
    int amp = query.indexOf('&', start);
    if (amp < 0)
      amp = query.length();
    String pair = query.substring(start, amp);
    int eq = pair.indexOf('=');
    if (eq > 0) {
      String key = urlDecode(pair.substring(0, eq));
      String value = urlDecode(pair.substring(eq + 1));
      if (key == "wifi_ssid" || key == "wifi_password" || key == "region" ||
          key == "mqtt_host" || key == "mqtt_username" || key == "token" ||
          key == "access_token" || key == "serial" || key == "printer_serial" ||
          key == "name" || key == "display_name" || key == "model" ||
          key == "printer_model" || key == "alias" || key == "display_alias" ||
          key == "layout" || key == "display_layout" ||
          key == "alias_bitmap_hex" || key == "alias_bitmap_w" ||
          key == "alias_bitmap_h" || key == "printers_json" ||
          key == "brightness" || key == "brightness_percent") {
        doc[key] = value;
      }
    }
    start = amp + 1;
  }
  String body;
  serializeJson(doc, body);
  body += F("</script>");
  body += F("</div></body></html>");
  body += F("</script>");
  body += F("</div></body></html>");
  return body;
}

void sendHttpJson(WiFiClient &client, int statusCode, const String &body) {
  const char *statusText = statusCode == 200   ? "OK"
                           : statusCode == 202 ? "Accepted"
                           : statusCode == 204 ? "No Content"
                           : statusCode == 400 ? "Bad Request"
                                               : "Not Found";
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

void sendHttpHtml(WiFiClient &client, const String &body) {
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

void queueHttpConfig(WiFiClient &client, const String &body) {
  if (!body.length() || body.length() > 4096) {
    sendHttpJson(client, 400, "{\"ok\":false,\"error\":\"bad config size\"}");
    return;
  }
  pendingHttpConfigBody = body;
  pendingHttpConfig = true;
  sendHttpJson(client, 202, "{\"ok\":true,\"queued\":true}");
}

void handleApiClient() {
  if (!serverStarted)
    return;
  WiFiClient client = apiServer.accept();
  if (!client)
    return;
  client.setTimeout(200);
  String line = client.readStringUntil('\n');
  line.trim();
  while (client.connected() && client.available()) {
    String discard = client.readStringUntil('\n');
    if (discard == "\r" || discard.length() == 0)
      break;
  }
  if (!line.length()) {
    client.stop();
    return;
  }
  int firstSpace = line.indexOf(' ');
  int secondSpace = line.indexOf(' ', firstSpace + 1);
  String method = firstSpace > 0 ? line.substring(0, firstSpace) : "";
  String target = (firstSpace > 0 && secondSpace > firstSpace)
                      ? line.substring(firstSpace + 1, secondSpace)
                      : "/";
  int q = target.indexOf('?');
  String path = q >= 0 ? target.substring(0, q) : target;
  String query = q >= 0 ? target.substring(q + 1) : "";
  if (method == "OPTIONS") {
    sendHttpJson(client, 204, "{}");
  } else if (method == "GET" && path == "/") {
    sendHttpHtml(client, espHomeHtml());
  } else if (method == "GET" && path == "/favicon.ico") {
    client.print(
        "HTTP/1.1 200 OK\r\nContent-Type: image/svg+xml\r\nCache-Control: "
        "max-age=86400\r\nConnection: close\r\n\r\n");
    client.print(
        "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 120'><rect "
        "width='100' height='120' rx='16' fill='#0b0e17'/><polygon "
        "points='13,13 47,13 47,54 13,69' fill='#ffffff'/><polygon "
        "points='13,75 47,60 47,107 13,107' fill='#ffffff'/><polygon "
        "points='53,13 87,13 87,55 53,39' fill='#ffffff'/><polygon "
        "points='53,45 87,61 87,107 53,107' fill='#ffffff'/></svg>");
    client.flush();
    client.stop();
  } else if (method == "GET" &&
             (path == "/api/status" || path == "/api/ping")) {
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
      if (amp < 0)
        amp = query.length();
      String pair = query.substring(start, amp);
      int eq = pair.indexOf('=');
      if (eq > 0 && urlDecode(pair.substring(0, eq)) == "serial")
        serial = urlDecode(pair.substring(eq + 1));
      start = amp + 1;
    }
    if (selectPrinterBySerial(serial))
      sendHttpJson(client, 200, "{\"ok\":true}");
    else
      sendHttpJson(client, 400,
                   "{\"ok\":false,\"error\":\"printer not found\"}");
  } else if (method == "GET" && path == "/api/refresh") {
    cache.baseDrawn = false;
    cache.offlineDrawn = false;
    displayDirty = true;
    sendHttpJson(client, 200, "{\"ok\":true}");
  } else if (method == "POST" && path == "/api/config") {
    delay(50);
    String postBody;
    while (client.connected() && client.available()) {
      postBody += (char)client.read();
    }
    if (postBody.length() > 0 && postBody.length() <= 4096) {
      int sc = 200;
      String resp = applyConfigBody(postBody, sc);
      sendHttpJson(client, sc, resp);
    } else {
      sendHttpJson(client, 400, "{\"ok\":false,\"error\":\"bad request\"}");
    }
  } else {
    sendHttpJson(client, 404, "{\"ok\":false,\"error\":\"not found\"}");
  }
}
void restartEspServer() {
  if (serverStarted) {
    apiServer.close();
    delay(20);
    apiServer.begin();
    Serial.printf("ESP server restarted: http://%s:%d/\n",
                  WiFi.localIP().toString().c_str(), ESP_CONFIG_PORT);
  }
}

void handleSerialConfig() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r')
      continue;
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
  if (serverStarted)
    return;
  apiServer.begin();
  serverStarted = true;
  Serial.printf("ESP server: http://%s:%d/\n",
                WiFi.localIP().toString().c_str(), ESP_CONFIG_PORT);
}

bool mqttConfigReady() {
  return stored.mqttHost.length() > 0 && stored.mqttUsername.length() > 0 &&
         stored.token.length() > 0 && stored.serial.length() > 0;
}

String reportTopic() { return String("device/") + stored.serial + "/report"; }

String requestTopic() { return String("device/") + stored.serial + "/request"; }

void mqttWriteRemainingLength(size_t len) {
  do {
    uint8_t digit = len % 128;
    len /= 128;
    if (len > 0)
      digit |= 0x80;
    mqttNet.write(&digit, 1);
  } while (len > 0);
}

void mqttWriteByte(uint8_t value) { mqttNet.write(&value, 1); }

void mqttWriteString(const String &value) {
  uint16_t len = value.length();
  mqttWriteByte((uint8_t)(len >> 8));
  mqttWriteByte((uint8_t)(len & 0xff));
  mqttNet.write((const uint8_t *)value.c_str(), len);
}

bool mqttReadPacket(uint8_t *type, uint8_t *body, size_t bodySize,
                    size_t *bodyLen, uint32_t timeoutMs) {
  uint32_t start = millis();
  while (!mqttNet.available()) {
    if (!mqttNet.connected() || millis() - start > timeoutMs)
      return false;
    handleApiClient();
    delay(1);
  }
  int header = mqttNet.read();
  if (header < 0)
    return false;

  size_t len = 0;
  int multiplier = 1;
  uint8_t digit = 0;
  do {
    start = millis();
    while (!mqttNet.available()) {
      if (!mqttNet.connected() || millis() - start > timeoutMs)
        return false;
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
        if (!mqttNet.connected() || millis() - start > timeoutMs)
          return false;
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
      if (!mqttNet.connected() || millis() - start > timeoutMs)
        return false;
      handleApiClient();
      delay(1);
    }
    int n = mqttNet.read(body + got, len - got);
    if (n > 0)
      got += (size_t)n;
  }

  *type = (uint8_t)header >> 4;
  *bodyLen = len;
  return true;
}

bool mqttSendConnect() {
  String clientId = String("PrintSphereLite-") + String(ESP.getChipId(), HEX);
  size_t remaining = 10 + 2 + clientId.length() + 2 +
                     stored.mqttUsername.length() + 2 + stored.token.length();
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
  if (!mqttReadPacket(&type, mqttBuf, sizeof(mqttBuf), &len, 3000))
    return false;
  return type == 2 && len >= 2 && mqttBuf[1] == 0;
}

bool mqttSendSubscribe(const String &topic) {
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
  return mqttReadPacket(&type, mqttBuf, sizeof(mqttBuf), &len, 3000) &&
         type == 9;
}

bool mqttSendPublish(const String &topic, const char *payload) {
  size_t payloadLen = strlen(payload);
  size_t remaining = 2 + topic.length() + payloadLen;
  mqttWriteByte(0x30);
  mqttWriteRemainingLength(remaining);
  mqttWriteString(topic);
  mqttNet.write((const uint8_t *)payload, payloadLen);
  return true;
}

void mqttSendPing() {
  mqttWriteByte(0xC0);
  mqttWriteByte(0);
  lastMqttPing = millis();
}

void publishMqttRequest(const char *payload) {
  if (!mqttNet.connected())
    return;
  String topic = requestTopic();
  mqttSendPublish(topic, payload);
}

void requestPrinterState() {
  publishMqttRequest(
      "{\"pushing\":{\"sequence_id\":\"0\",\"command\":\"pushall\"}}");
  lastMqttRequest = millis();
}

bool applyFlatToolNozzles(JsonObject print) {
  static const char *const tool0Keys[] = {"tool0_nozzle_temp",
                                          "tool0_nozzle_temper"};
  static const char *const tool1Keys[] = {"tool1_nozzle_temp",
                                          "tool1_nozzle_temper"};
  static const char *const leftKeys[] = {"left_nozzle_temp",
                                         "left_nozzle_temper"};
  static const char *const rightKeys[] = {"right_nozzle_temp",
                                          "right_nozzle_temper"};
  bool got = false;
  float f = jsonFloatForKeys(print, tool0Keys,
                             sizeof(tool0Keys) / sizeof(tool0Keys[0]), -999);
  if (f != -999) {
    storeNozzleTemp(0, f, true);
    got = true;
  }
  f = jsonFloatForKeys(print, tool1Keys,
                       sizeof(tool1Keys) / sizeof(tool1Keys[0]), -999);
  if (f != -999) {
    storeNozzleTemp(1, f, true);
    pr.dualNozzle = true;
    got = true;
  }
  f = jsonFloatForKeys(print, leftKeys, sizeof(leftKeys) / sizeof(leftKeys[0]),
                       -999);
  if (f != -999) {
    storeNozzleTemp(0, f, false);
    got = true;
  }
  f = jsonFloatForKeys(print, rightKeys,
                       sizeof(rightKeys) / sizeof(rightKeys[0]), -999);
  if (f != -999) {
    storeNozzleTemp(1, f, false);
    pr.dualNozzle = true;
    got = true;
  }
  return got;
}

void applyPrint(JsonObject print) {
  if (print.isNull())
    return;

  static const char *const statusKeys[] = {
      "gcode_state", "print_status", "printStatus", "status",
      "task_status", "taskStatus",   "state"};
  const char *status = jsonStringForKeys(
      print, statusKeys, sizeof(statusKeys) / sizeof(statusKeys[0]));
  if (status[0]) {
    pr.status = status;
    pr.status.toLowerCase();
    printerStatusReceived = true;
  }

  static const char *const progressKeys[] = {
      "mc_percent",   "percent",        "progress",      "task_progress",
      "taskProgress", "print_progress", "printProgress", "printPercent"};
  float f =
      jsonFloatForKeys(print, progressKeys,
                       sizeof(progressKeys) / sizeof(progressKeys[0]), -999);
  if (f != -999)
    pr.progress = normalizePercent(f);

  bool gotNestedNozzle = applyNestedNozzleTemps(print);
  bool gotFlatToolNozzle = applyFlatToolNozzles(print);
  if (!pr.dualNozzle && !gotNestedNozzle && !gotFlatToolNozzle) {
    static const char *const nozzleKeys[] = {
        "nozzle_temper",     "nozzle_temp", "nozzle_temperature",
        "nozzleTemperature", "hotend_temp", "hotend_temperature"};
    f = jsonFloatForKeys(print, nozzleKeys,
                         sizeof(nozzleKeys) / sizeof(nozzleKeys[0]), -999);
    if (f == -999)
      f = nestedNozzleTemp(print, -999);
    if (f != -999)
      pr.nozzleTemp = normalizeTemp(f);
  }

  static const char *const bedKeys[] = {
      "bed_temper",    "bed_temp",    "bed_temperature",   "bedTemperature",
      "hotbed_temper", "hotbed_temp", "hotbed_temperature"};
  f = jsonFloatForKeys(print, bedKeys, sizeof(bedKeys) / sizeof(bedKeys[0]),
                       -999);
  if (f == -999)
    f = nestedBedTemp(print, -999);
  if (f != -999)
    pr.bedTemp = f;

  static const char *const modelKeys[] = {"model", "dev_model_name",
                                          "dev_product_name", "product_name"};
  const char *model = jsonStringForKeys(
      print, modelKeys, sizeof(modelKeys) / sizeof(modelKeys[0]));
  if (model[0])
    pr.model = model;

  static const char *const chamberKeys[] = {"chamber_temper",
                                            "chamber_temp",
                                            "chamber_temperature",
                                            "chamberTemperature",
                                            "chamberTemp",
                                            "chamberTargetTemp",
                                            "chamberTargetTemperature",
                                            "chamber_target_temper",
                                            "chamber_target_temp",
                                            "chamber_target_temperature",
                                            "target_chamber_temp",
                                            "targetChamberTemp",
                                            "ctt"};
  f = jsonFloatForKeys(print, chamberKeys,
                       sizeof(chamberKeys) / sizeof(chamberKeys[0]), -999);
  String chamberModel = pr.model.length() ? pr.model : stored.model;
  if ((f == -999 || f < 0 || f > 120) &&
      chamberFallbackAllowedForModel(chamberModel)) {
    f = jsonFloat(print["device"]["ctc"]["info"]["temp"], -999);
  }
  if ((f == -999 || f < 0 || f > 120) &&
      chamberFallbackAllowedForModel(chamberModel)) {
    f = jsonFloat(print["info"]["temp"], -999);
  }
  if (f != -999 && f > -50 && f < 120)
    pr.chamberTemp = f;

  static const char *const remainingMinuteKeys[] = {
      "mc_remaining_time", "remaining_minutes", "remainingMinutes",
      "remaining_min", "remain_time"};
  int i = jsonIntForKeys(
      print, remainingMinuteKeys,
      sizeof(remainingMinuteKeys) / sizeof(remainingMinuteKeys[0]), -999);
  if (i == -999) {
    static const char *const remainingSecondKeys[] = {
        "remaining_seconds", "remainingSeconds", "remaining_time",
        "remainingTime", "mc_left_time"};
    int seconds = jsonIntForKeys(
        print, remainingSecondKeys,
        sizeof(remainingSecondKeys) / sizeof(remainingSecondKeys[0]), -999);
    if (seconds != -999)
      i = (seconds + 59) / 60;
  }
  if (i != -999)
    pr.remainingMin = i;

  static const char *const currentLayerKeys[] = {"layer_num", "current_layer",
                                                 "currentLayer", "layer"};
  i = jsonIntForKeys(print, currentLayerKeys,
                     sizeof(currentLayerKeys) / sizeof(currentLayerKeys[0]),
                     -999);
  if (i != -999)
    pr.currentLayer = i;

  static const char *const totalLayerKeys[] = {"total_layer_num",
                                               "total_layers", "totalLayers",
                                               "layer_count", "layerCount"};
  i = jsonIntForKeys(print, totalLayerKeys,
                     sizeof(totalLayerKeys) / sizeof(totalLayerKeys[0]), -999);
  if (i != -999)
    pr.totalLayers = i;

  // Check AMS presence via ams_exist_bits
  if (!print["ams_exist_bits"].isNull()) {
    const char *bits = print["ams_exist_bits"];
    if (bits) {
      pr.amsExist = (atoi(bits) > 0);
    } else {
      pr.amsExist = (print["ams_exist_bits"].as<int>() > 0);
    }
  } else if (!print["ams"]["ams_exist_bits"].isNull()) {
    const char *bits = print["ams"]["ams_exist_bits"];
    if (bits) {
      pr.amsExist = (atoi(bits) > 0);
    } else {
      pr.amsExist = (print["ams"]["ams_exist_bits"].as<int>() > 0);
    }
  }

  // Parse AMS tray data
  JsonObject amsObj = print["ams"];
  if (!amsObj.isNull()) {
    if (!amsObj["tray_now"].isNull()) {
      const char *tn = amsObj["tray_now"];
      if (tn)
        pr.activeTray = atoi(tn);
      else
        pr.activeTray = amsObj["tray_now"] | -1;
    }
    JsonArray amsArray = amsObj["ams"];
    if (!amsArray.isNull()) {
      if (amsArray.size() > 0) {
        pr.amsExist = true;
        JsonObject firstAms = amsArray[0];
        JsonArray trays = firstAms["tray"];
        if (!trays.isNull()) {
          int count = min((int)trays.size(), 4);
          for (int k = 0; k < count; k++) {
            JsonObject tray = trays[k];
            const char *tt = tray["tray_type"];
            if (tt && strlen(tt) > 0) {
              pr.amsSlots[k].valid = true;
              pr.amsSlots[k].trayType = tt;
              pr.amsSlots[k].traySubBrands = tray["tray_sub_brands"] | "";
              const char *tag = tray["tag_uid"];
              pr.amsSlots[k].tagUid = tag ? tag : "";
              pr.amsSlots[k].isOfficial = isOfficialTag(tag);
              const char *tc = tray["tray_color"];
              if (tc && strlen(tc) >= 8) {
                pr.amsSlots[k].trayColor = strtoul(tc, nullptr, 16);
              } else if (tc && strlen(tc) >= 6) {
                pr.amsSlots[k].trayColor = strtoul(tc, nullptr, 16) << 8;
              }
              pr.amsSlots[k].remain = tray["remain"] | -1;
            } else {
              pr.amsSlots[k].valid = false;
              pr.amsSlots[k].isOfficial = false;
              pr.amsSlots[k].tagUid = "";
            }
          }
        }
      } else {
        pr.amsExist = false;
      }
    }
  }

  // Parse External Spool / Virtual Tray (vt_tray) data
  JsonObject vtObj = print["vt_tray"];
  if (vtObj.isNull() && !amsObj.isNull()) {
    vtObj = amsObj["vt_tray"];
  }
  if (!vtObj.isNull()) {
    const char *tt = vtObj["tray_type"];
    if (tt && strlen(tt) > 0) {
      pr.extSlot.valid = true;
      pr.extSlot.trayType = tt;
      pr.extSlot.traySubBrands = vtObj["tray_sub_brands"] | "";
      const char *tag = vtObj["tag_uid"];
      pr.extSlot.tagUid = tag ? tag : "";
      pr.extSlot.isOfficial = isOfficialTag(tag);
      const char *tc = vtObj["tray_color"];
      if (tc && strlen(tc) >= 8) {
        pr.extSlot.trayColor = strtoul(tc, nullptr, 16);
      } else if (tc && strlen(tc) >= 6) {
        pr.extSlot.trayColor = strtoul(tc, nullptr, 16) << 8;
      }
      pr.extSlot.remain = vtObj["remain"] | -1;
    }
  }
  if (!print["tray_type"].isNull() || !print["filament_type"].isNull()) {
    const char *tt = print["tray_type"];
    if (!tt || strlen(tt) == 0)
      tt = print["filament_type"];
    if (tt && strlen(tt) > 0) {
      pr.extSlot.valid = true;
      pr.extSlot.trayType = tt;
      if (!print["tray_sub_brands"].isNull())
        pr.extSlot.traySubBrands = print["tray_sub_brands"].as<const char *>();
      if (!print["tag_uid"].isNull()) {
        const char *tag = print["tag_uid"];
        pr.extSlot.tagUid = tag ? tag : "";
        pr.extSlot.isOfficial = isOfficialTag(tag);
      }
      const char *tc = print["tray_color"];
      if (tc && strlen(tc) >= 8) {
        pr.extSlot.trayColor = strtoul(tc, nullptr, 16);
      } else if (tc && strlen(tc) >= 6) {
        pr.extSlot.trayColor = strtoul(tc, nullptr, 16) << 8;
      }
      if (!print["remain"].isNull()) {
        pr.extSlot.remain = print["remain"] | -1;
      }
    }
  }

  if (!print["subtray_id"].isNull()) {
    pr.activeTray = print["subtray_id"] | pr.activeTray;
  } else if (!print["tray_now"].isNull()) {
    const char *tn = print["tray_now"];
    if (tn)
      pr.activeTray = atoi(tn);
    else
      pr.activeTray = print["tray_now"] | pr.activeTray;
  }

  if (!print["spd_lvl"].isNull())
    pr.spdLvl = print["spd_lvl"] | 1;
  else if (!print["spdLvl"].isNull())
    pr.spdLvl = print["spdLvl"] | 1;

  if (!print["spd_mag"].isNull())
    pr.spdMag = print["spd_mag"] | 100;
  else if (!print["spdMag"].isNull())
    pr.spdMag = print["spdMag"] | 100;

  pr.online = true;
  displayDirty = true;
}

const char *findPattern(const char *data, size_t length, const char *pattern) {
  size_t patternLen = strlen(pattern);
  if (patternLen == 0 || length < patternLen)
    return nullptr;
  for (size_t i = 0; i <= length - patternLen; ++i) {
    if (memcmp(data + i, pattern, patternLen) == 0)
      return data + i;
  }
  return nullptr;
}

const char *jsonObjectEnd(const char *start, const char *end) {
  int depth = 0;
  bool inString = false;
  bool escape = false;
  for (const char *p = start; p < end; ++p) {
    char c = *p;
    if (inString) {
      if (escape)
        escape = false;
      else if (c == '\\')
        escape = true;
      else if (c == '"')
        inString = false;
      continue;
    }
    if (c == '"')
      inString = true;
    else if (c == '{')
      depth++;
    else if (c == '}') {
      depth--;
      if (depth == 0)
        return p + 1;
    }
  }
  return nullptr;
}

void applyExtruderFromRawPayload(uint8_t *payload, size_t length) {
  const char *data = (const char *)payload;
  const char *payloadEnd = data + length;
  const char *extPos = findPattern(data, length, "\"extruder\":");
  if (!extPos)
    return;

  const char *objStart = extPos + strlen("\"extruder\":");
  while (objStart < payloadEnd && (*objStart == ' ' || *objStart == '\t'))
    objStart++;
  if (objStart >= payloadEnd || *objStart != '{')
    return;
  const char *objEnd = jsonObjectEnd(objStart, payloadEnd);
  if (!objEnd)
    return;

  JsonDocument extDoc;
  if (deserializeJson(extDoc, objStart, (size_t)(objEnd - objStart)))
    return;
  if (applyNozzleInfo(extDoc["info"].as<JsonArray>()))
    displayDirty = true;
}

void parseMqttPayload(uint8_t *payload, size_t length) {
  JsonDocument filter;
  filter["print"]["ams_exist_bits"] = true;
  filter["print"]["ams"]["ams_exist_bits"] = true;
  filter["print"]["ams"]["ams"][0]["tray"][0]["tray_type"] = true;
  filter["print"]["ams"]["ams"][0]["tray"][0]["tray_sub_brands"] = true;
  filter["print"]["ams"]["ams"][0]["tray"][0]["tray_color"] = true;
  filter["print"]["ams"]["ams"][0]["tray"][0]["remain"] = true;
  filter["print"]["ams"]["ams"][0]["tray"][0]["tag_uid"] = true;
  filter["print"]["ams"]["tray_now"] = true;
  filter["print"]["ams"]["vt_tray"]["tray_type"] = true;
  filter["print"]["ams"]["vt_tray"]["tray_sub_brands"] = true;
  filter["print"]["ams"]["vt_tray"]["tray_color"] = true;
  filter["print"]["ams"]["vt_tray"]["remain"] = true;
  filter["print"]["ams"]["vt_tray"]["tag_uid"] = true;
  filter["print"]["vt_tray"]["tray_type"] = true;
  filter["print"]["vt_tray"]["tray_sub_brands"] = true;
  filter["print"]["vt_tray"]["tray_color"] = true;
  filter["print"]["vt_tray"]["remain"] = true;
  filter["print"]["vt_tray"]["tag_uid"] = true;
  filter["print"]["tag_uid"] = true;
  filter["print"]["tray_type"] = true;
  filter["print"]["tray_sub_brands"] = true;
  filter["print"]["tray_color"] = true;
  filter["print"]["filament_type"] = true;
  filter["print"]["remain"] = true;
  filter["print"]["tray_now"] = true;
  filter["print"]["subtray_id"] = true;
  filter["print"]["spd_lvl"] = true;
  filter["print"]["spd_mag"] = true;
  filter["print"]["spdLvl"] = true;
  filter["print"]["spdMag"] = true;
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
  filter["print"]["chamber_temper"] = true;
  filter["print"]["chamber_temp"] = true;
  filter["print"]["chamber_temperature"] = true;
  filter["print"]["chamberTemperature"] = true;
  filter["print"]["chamberTemp"] = true;
  filter["print"]["chamberTargetTemp"] = true;
  filter["print"]["chamberTargetTemperature"] = true;
  filter["print"]["chamber_target_temper"] = true;
  filter["print"]["chamber_target_temp"] = true;
  filter["print"]["chamber_target_temperature"] = true;
  filter["print"]["target_chamber_temp"] = true;
  filter["print"]["targetChamberTemp"] = true;
  filter["print"]["ctt"] = true;
  filter["print"]["device"]["ctc"]["info"]["temp"] = true;
  filter["print"]["info"]["temp"] = true;
  filter["print"]["model"] = true;
  filter["print"]["dev_model_name"] = true;
  filter["print"]["dev_product_name"] = true;
  filter["print"]["product_name"] = true;
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
  DeserializationError err = deserializeJson(
      doc, payload, length, DeserializationOption::Filter(filter));
  if (err)
    return;
  applyPrint(doc["print"].as<JsonObject>());
  applyExtruderFromRawPayload(payload, length);
}

void mqttHandleIncoming() {
  while (mqttNet.connected() && mqttNet.available()) {
    uint8_t type = 0;
    size_t len = 0;
    if (!mqttReadPacket(&type, mqttBuf, sizeof(mqttBuf), &len, 100))
      return;
    if (type == 3 && len > 2) {
      uint16_t topicLen = ((uint16_t)mqttBuf[0] << 8) | mqttBuf[1];
      if ((size_t)topicLen + 2 < len) {
        parseMqttPayload(mqttBuf + 2 + topicLen, len - 2 - topicLen);
      }
    }
  }
}

bool connectMqtt() {
  if (WiFi.status() != WL_CONNECTED || !mqttConfigReady())
    return false;
  if (mqttNet.connected())
    return true;

  char clientId[48];
  snprintf(clientId, sizeof(clientId), "PrintSphereLite-%06X", ESP.getChipId());
  Serial.printf("Cloud MQTT connecting %s serial=%s user=%s\n",
                stored.mqttHost.c_str(), stored.serial.c_str(),
                stored.mqttUsername.c_str());
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

  publishMqttRequest(
      "{\"info\":{\"sequence_id\":\"0\",\"command\":\"get_version\"}}");
  publishMqttRequest(
      "{\"pushing\":{\"sequence_id\":\"0\",\"command\":\"start\"}}");
  resetLivePrintFields();
  requestPrinterState();
  Serial.print("Cloud MQTT subscribed: ");
  Serial.println(topic);
  return true;
}

void drawFrameTrack() {
  tft.fillRect(FRAME_X, FRAME_Y, FRAME_SIZE, FRAME_THICK, C_TRACK);
  tft.fillRect(FRAME_X + FRAME_SIZE - FRAME_THICK, FRAME_Y, FRAME_THICK,
               FRAME_SIZE, C_TRACK);
  tft.fillRect(FRAME_X, FRAME_Y + FRAME_SIZE - FRAME_THICK, FRAME_SIZE,
               FRAME_THICK, C_TRACK);
  tft.fillRect(FRAME_X, FRAME_Y, FRAME_THICK, FRAME_SIZE, C_TRACK);
}

void drawFrameProgressLen(int len, uint16_t color) {
  if (len <= 0)
    return;

  int top = min(len, FRAME_SIDE);
  if (top > 0)
    tft.fillRect(FRAME_X, FRAME_Y, top, FRAME_THICK, color);
  len -= top;

  int right = min(len, FRAME_SIDE);
  if (right > 0)
    tft.fillRect(FRAME_X + FRAME_SIZE - FRAME_THICK, FRAME_Y, FRAME_THICK,
                 right, color);
  len -= right;

  int bottom = min(len, FRAME_SIDE);
  if (bottom > 0)
    tft.fillRect(FRAME_X + FRAME_SIZE - bottom,
                 FRAME_Y + FRAME_SIZE - FRAME_THICK, bottom, FRAME_THICK,
                 color);
  len -= bottom;

  int left = min(len, FRAME_SIDE);
  if (left > 0)
    tft.fillRect(FRAME_X, FRAME_Y + FRAME_SIZE - left, FRAME_THICK, left,
                 color);
}

int progressToLen(float progress) {
  if (progress < 0)
    return 0;
  if (progress > 100)
    progress = 100;
  int len = (int)(progress * FRAME_TOTAL / 100.0f + 0.5f);
  if (progress > 0 && len < 1)
    len = 1;
  if (len > FRAME_TOTAL)
    len = FRAME_TOTAL;
  return len;
}

void updateFrameProgress() {
  int len = progressToLen(pr.progress);
  drawFrameTrack();
  drawFrameProgressLen(len, C_RING);
  cache.progress = len;
}

void drawBold(const String &text, int x, int y) {
  tft.drawString(text, x, y);
  tft.drawString(text, x + 1, y);
}

void drawTextBox(int x, int y, int w, int h, uint8_t font, uint16_t color,
                 const String &text, bool bold) {
  tft.fillRect(x, y, w, h, BG_BLACK);
  tft.setTextFont(font);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(color, BG_BLACK);
  tft.setTextPadding(0);
  if (bold)
    drawBold(text, x + w / 2, y + h / 2);
  else
    tft.drawString(text, x + w / 2, y + h / 2);
}

String fitTextToWidth(String text, uint8_t font, int maxWidth, bool bold) {
  tft.setTextFont(font);
  int suffixW = tft.textWidth("..") + (bold ? 1 : 0);
  if (tft.textWidth(text) + (bold ? 1 : 0) <= maxWidth)
    return text;
  while (text.length() > 0 &&
         tft.textWidth(text) + suffixW + (bold ? 1 : 0) > maxWidth) {
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

void drawDualNozzleTempBox(int x, int y, int w, int h, int side, int value,
                           uint16_t color) {
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

bool dualNozzleDisplayMode() { return pr.dualNozzle; }

int displayedNozzleSide() {
  if (hasDualNozzleTemps())
    return ((millis() / DUAL_NOZZLE_SWITCH_MS) % 2) == 0 ? 0 : 1;
  if (pr.dualNozzle && pr.leftNozzleTemp >= 0)
    return 0;
  if (pr.dualNozzle && pr.rightNozzleTemp >= 0)
    return 1;
  return -1;
}

float displayedNozzleTemp() {
  int side = displayedNozzleSide();
  if (side == 0)
    return pr.leftNozzleTemp;
  if (side == 1)
    return pr.rightNozzleTemp;
  return pr.nozzleTemp;
}

const char *displayedNozzleLabel() { return "NOZZLE"; }

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
  if (isPrintingState(pr.status))
    return "printing";
  if (isPreparingState(pr.status))
    return "prepare";
  if (isPausedState(pr.status))
    return "paused";
  if (isFinishedState(pr.status))
    return "done";
  if (isFailedState(pr.status))
    return "error";
  return "idle";
}

uint16_t statusColor() {
  if (isPrintingState(pr.status))
    return C_RING;
  if (isPreparingState(pr.status))
    return C_CYAN;
  if (isPausedState(pr.status))
    return C_ORANGE;
  if (isFinishedState(pr.status))
    return C_BLUE;
  if (isFailedState(pr.status))
    return C_RED;
  return C_DIM;
}

void drawBold(const String &text, int x, int y);
String fitTextToWidth(String text, uint8_t font, int maxWidth, bool bold);

String dashboardStatusText() {
  if (isPrintingState(pr.status))
    return "PRINT";
  if (isPreparingState(pr.status))
    return "PREP";
  if (isPausedState(pr.status))
    return "PAUSE";
  if (isFinishedState(pr.status))
    return "DONE";
  if (isFailedState(pr.status))
    return "ERR";
  return "IDLE";
}

String timeText(int minutes) {
  if (minutes < 0)
    return "--";
  int h = minutes / 60;
  int m = minutes % 60;
  char b[18];
  if (h > 0)
    snprintf(b, sizeof(b), "%dh%02dm", h, m);
  else
    snprintf(b, sizeof(b), "%dm", m);
  return b;
}

String etaText() {
  if (pr.remainingMin < 0)
    return "--";
  time_t now = time(nullptr);
  if (now < 1700000000)
    return "~";
  now += (time_t)pr.remainingMin * 60;
  struct tm *info = localtime(&now);
  if (!info)
    return "~";
  char b[8];
  snprintf(b, sizeof(b), "%02d:%02d", info->tm_hour, info->tm_min);
  return b;
}

int hexNibble(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return 0;
}

void drawProgmemHexBitmap(const char *hex, int w, int h, int x, int y,
                          uint16_t color) {
  int bytesPerRow = (w + 7) / 8;
  for (int row = 0; row < h; ++row) {
    for (unsigned int col = 0; col < w; ++col) {
      int byteIndex = row * bytesPerRow + col / 8;
      int hexIndex = byteIndex * 2;
      char hi = (char)pgm_read_byte(hex + hexIndex);
      char lo = (char)pgm_read_byte(hex + hexIndex + 1);
      uint8_t value = (hexNibble(hi) << 4) | hexNibble(lo);
      if (value & (0x80 >> (col & 7)))
        tft.drawPixel(x + col, y + row, color);
    }
  }
}

void drawCnCentered(const char *hex, int w, int h, int x, int y, int boxW,
                    int boxH, uint16_t color) {
  drawProgmemHexBitmap(hex, w, h, x + (boxW - w) / 2, y + (boxH - h) / 2,
                       color);
}

void drawAliasBitmap(int x, int y, uint16_t color) {
  int w = stored.aliasBitmapW;
  int h = stored.aliasBitmapH;
  if (w <= 0 || h <= 0 || !stored.aliasBitmapHex.length())
    return;
  int bytesPerRow = (w + 7) / 8;
  for (int row = 0; row < h; ++row) {
    for (int col = 0; col < w; ++col) {
      int byteIndex = row * bytesPerRow + col / 8;
      int hexIndex = byteIndex * 2;
      if (hexIndex + 1 >= stored.aliasBitmapHex.length())
        return;
      uint8_t value = (hexNibble(stored.aliasBitmapHex[hexIndex]) << 4) |
                      hexNibble(stored.aliasBitmapHex[hexIndex + 1]);
      if (value & (0x80 >> (col & 7)))
        tft.drawPixel(x + col, y + row, color);
    }
  }
}

void drawDashboardStatusCn(int x, int y, int w, int h) {
  if (isPrintingState(pr.status)) {
    drawCnCentered(CN_PRINT, 55, 23, x, y, w, h, C_TEXT);
    return;
  }
  if (isPreparingState(pr.status)) {
    drawCnCentered(CN_READY, 55, 23, x, y, w, h, C_TEXT);
    return;
  }
  if (isPausedState(pr.status)) {
    drawCnCentered(CN_PAUSE, 36, 17, x, y, w, h, C_TEXT);
    return;
  }
  if (isFinishedState(pr.status)) {
    drawCnCentered(CN_DONE, 36, 17, x, y, w, h, C_TEXT);
    return;
  }
  if (isFailedState(pr.status)) {
    drawCnCentered(CN_ERR, 36, 17, x, y, w, h, C_TEXT);
    return;
  }
  drawCnCentered(CN_IDLE, 36, 17, x, y, w, h, C_TEXT);
}

void drawDashboardTextBox(int x, int y, int w, int h, uint16_t bg, uint8_t font,
                          uint16_t color, const String &text,
                          bool bold = false) {
  tft.fillRoundRect(x, y, w, h, 6, bg);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(font);
  tft.setTextColor(color, bg);
  tft.setTextPadding(w - 8);
  // Try requested font first, fall back to font 2 if text is too wide
  uint8_t useFont = font;
  tft.setTextFont(font);
  if (tft.textWidth(text) + (bold ? 1 : 0) > w - 8) {
    useFont = 2;
  }
  String shown = fitTextToWidth(text, useFont, w - 8, bold);
  if (bold)
    drawBold(shown, x + w / 2, y + h / 2);
  else
    tft.drawString(shown, x + w / 2, y + h / 2);
  tft.setTextPadding(0);
}

String nozzleSummary() {
  char b[20];
  if (pr.dualNozzle && (pr.leftNozzleTemp >= 0 || pr.rightNozzleTemp >= 0)) {
    char l[8], r[8];
    if (pr.leftNozzleTemp >= 0)
      snprintf(l, sizeof(l), "%d", (int)(pr.leftNozzleTemp + 0.5f));
    else
      strcpy(l, "--");
    if (pr.rightNozzleTemp >= 0)
      snprintf(r, sizeof(r), "%d", (int)(pr.rightNozzleTemp + 0.5f));
    else
      strcpy(r, "--");
    snprintf(b, sizeof(b), "%s/%s", l, r);
    return b;
  }
  if (pr.nozzleTemp >= 0) {
    snprintf(b, sizeof(b), "%d", (int)(pr.nozzleTemp + 0.5f));
    return b;
  }
  return "--";
}

String tempSummary(float value) {
  if (value < 0)
    return "~";
  char b[8];
  snprintf(b, sizeof(b), "%d", (int)(value + 0.5f));
  return b;
}

String normalizedModelName(const String &value) {
  String raw = value;
  raw.trim();
  String key = raw;
  key.toUpperCase();
  key.replace("-", "");
  key.replace("_", "");
  key.replace(" ", "");
  if (!key.length())
    return raw;
  if (key == "BLP001" || key.indexOf("X1C") >= 0 ||
      key.indexOf("X1CARBON") >= 0)
    return "X1C";
  if (key == "C11" || key.indexOf("P1P") >= 0)
    return "P1P";
  if (key == "C12" || key.indexOf("P1S") >= 0)
    return "P1S";
  if (key == "C13" || key.indexOf("X1E") >= 0)
    return "X1E";
  if (key == "N1" || key.indexOf("A1MINI") >= 0)
    return "A1 mini";
  if (key == "N2S" || key == "A1")
    return "A1";
  if (key == "N6V2" || key.indexOf("X2D") >= 0)
    return "X2D";
  if (key == "N7V2" || key.indexOf("P2S") >= 0)
    return "P2S";
  if (key == "O1C2V2" || key.indexOf("H2C") >= 0)
    return "H2C";
  if (key == "O1D" || key.indexOf("H2D") >= 0)
    return "H2D";
  if (key == "O1S" || key.indexOf("H2S") >= 0)
    return "H2S";
  return raw;
}

bool isPureAscii(const String &s) {
  if (!s.length())
    return false;
  for (unsigned int i = 0; i < s.length(); i++) {
    if ((uint8_t)s.charAt(i) > 127)
      return false;
  }
  return true;
}

String getTopLeftDisplayName() {
  String candidate = "";
  if (stored.alias.length())
    candidate = stored.alias;
  else if (pr.displayName.length())
    candidate = pr.displayName;
  else if (stored.name.length())
    candidate = stored.name;

  if (candidate.length() > 0 && isPureAscii(candidate)) {
    return candidate;
  }
  return normalizedModelName(pr.model.length() ? pr.model : stored.model);
}

bool chamberFallbackAllowedForModel(const String &value) {
  String model = normalizedModelName(value);
  model.toUpperCase();
  return model == "P2S" || model == "X2D" || model == "H2C" || model == "H2D" ||
         model == "H2S";
}

bool modelHasChamberSensor(const String &value) {
  String model = normalizedModelName(value);
  model.toUpperCase();
  // P1S, P1P, A1, A1 mini have no chamber sensor; others (X1C, X1E, P2S, X2D,
  // H2x) do
  return model != "P1S" && model != "P1P" && model != "A1" && model != "A1MINI";
}
void drawDashboardProgressRing(int progressValue) {
  int pct = progressValue;
  if (pct < 0)
    pct = 0;
  if (pct > 100)
    pct = 100;

  tft.fillRect(18, 74, 108, 80, C_CARD);
  char b[12];
  if (progressValue >= 0)
    snprintf(b, sizeof(b), "%d", progressValue);
  else
    strcpy(b, "--");
  drawDashboardTextBox(21, 99, 102, 34, C_CARD, 4, C_TEXT, b, false);
}

void drawDashboardMetricCard(int x, int y, const char *labelHex, int labelW,
                             int labelH, const String &value,
                             uint8_t valueFont = 4) {
  tft.fillRoundRect(x, y, 100, 48, 7, C_CARD);
  tft.drawRoundRect(x, y, 100, 48, 7, 0x294A);
  drawCnCentered(labelHex, labelW, labelH, x + 6, y + 4, 88, 20, C_TEXT);
  drawDashboardTextBox(x + 8, y + 24, 84, 20, C_CARD, valueFont, C_TEXT, value,
                       false);
}

void drawDashboardCelsiusValue(int x, int y, int w, int h, const String &value,
                               uint8_t font) {
  tft.fillRoundRect(x, y, w, h, 6, C_CARD);
  tft.drawRoundRect(x, y, w, h, 6, 0x294A);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(C_TEXT, C_CARD);
  tft.setTextFont(font);
  uint8_t drawFont = font;
  if (tft.textWidth(value) > w - 2) {
    drawFont = 2;
    tft.setTextFont(drawFont);
  }
  tft.setTextPadding(w);
  tft.drawString(value, x + w / 2, y + h / 2);
  tft.setTextPadding(0);
}

void drawDashboardTempCard(int x, int y, const char *labelHex, int labelW,
                           int labelH, const String &value,
                           uint8_t valueFont = 4) {
  tft.fillRoundRect(x, y, 100, 48, 7, C_CARD);
  tft.drawRoundRect(x, y, 100, 48, 7, 0x294A);
  drawCnCentered(labelHex, labelW, labelH, x + 6, y + 4, 88, 20, C_TEXT);
  drawDashboardCelsiusValue(x + 2, y + 24, 96, 20, value, valueFont);
}

static uint32_t manualStringHash(const String &s) {
  uint32_t h = 0;
  for (unsigned int i = 0; i < s.length(); i++) {
    h = h * 31 + (uint8_t)s.charAt(i);
  }
  return h;
}

uint16_t filamentColor(uint32_t argb) {
  uint8_t r = (argb >> 24) & 0xFF;
  uint8_t g = (argb >> 16) & 0xFF;
  uint8_t b = (argb >> 8) & 0xFF;
  return ((b >> 3) << 11) | ((g >> 2) << 5) | (r >> 3);
}

void drawDashboardSlotCard(int slotX, const AmsTrayInfo &slot, bool isActive,
                           const char *slotLabel, const char *emptyLabel) {
  // Card Background & Highlight Border
  uint16_t bgCol = isActive ? 0x1A04 : C_CARD;
  uint16_t borderCol = isActive ? C_RING : 0x294A;

  tft.fillRoundRect(slotX, 102, 44, 46, 6, bgCol);
  tft.drawRoundRect(slotX, 102, 44, 46, 6, borderCol);

  if (isActive) {
    // Active indicator triangle at top center of slot card
    tft.fillTriangle(slotX + 19, 100, slotX + 25, 100, slotX + 22, 103,
                     C_RING);
  } else {
    // Clear active indicator triangle above top border when slot is no longer active
    tft.fillRect(slotX + 18, 99, 9, 3, C_CARD);
  }

  if (slot.valid) {
    // Color Swatch
    uint16_t col565 = filamentColor(slot.trayColor);
    tft.fillRoundRect(slotX + 4, 106, 12, 12, 3, col565);
    tft.drawRoundRect(slotX + 4, 106, 12, 12, 3, C_TEXT);

    // Slot Number or "ext"
    tft.setTextDatum(TL_DATUM);
    tft.setTextFont(1);
    tft.setTextColor(isActive ? C_RING : C_DIM, bgCol);
    tft.drawString(slotLabel, slotX + 18, 107);

    // Short Filament Type (e.g. PLA, PETG, ABS)
    String shortType =
        slot.traySubBrands.length() ? slot.traySubBrands : slot.trayType;
    if (shortType.startsWith("PLA"))
      shortType = "PLA";
    else if (shortType.startsWith("ABS"))
      shortType = "ABS";
    else if (shortType.startsWith("PETG"))
      shortType = "PETG";
    else if (shortType.startsWith("TPU"))
      shortType = "TPU";
    else if (shortType.startsWith("PA"))
      shortType = "PA";
    else if (shortType.startsWith("PC"))
      shortType = "PC";
    else if (shortType.startsWith("ASA"))
      shortType = "ASA";
    else if (shortType.startsWith("Support"))
      shortType = "SPT";
    else if (shortType.length() > 5)
      shortType = shortType.substring(0, 5);

    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(1);
    tft.setTextColor(C_TEXT, bgCol);
    tft.drawString(shortType, slotX + 22, 124);

    // Remaining capacity % (Only displayed for official Bambu filaments with RFID)
    if (slot.isOfficial && slot.remain >= 0 && slot.remain <= 100) {
      char rStr[8];
      snprintf(rStr, sizeof(rStr), "%d%%", slot.remain);
      tft.setTextColor(C_DIM, bgCol);
      tft.drawString(rStr, slotX + 22, 137);
    }
  } else {
    // Empty / Invalid Slot
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(1);
    tft.setTextColor(C_DIM, bgCol);
    tft.drawString(emptyLabel, slotX + 22, 118);
    tft.drawString("--", slotX + 22, 132);
  }
}

void drawDashboardBase() {
  tft.fillScreen(BG_BLACK);
  tft.drawRoundRect(4, 4, 232, 232, 8, 0x294A);

  // 1. Top Header Capsule Bar (y: 10, h: 22, r: 11)
  tft.fillRoundRect(14, 10, 212, 22, 11, C_CARD);
  tft.drawRoundRect(14, 10, 212, 22, 11, 0x294A);

  // 2. Middle Progress Hero Card Base (y: 36, h: 118, r: 10)
  tft.fillRoundRect(14, 36, 212, 118, 10, C_CARD);
  tft.drawRoundRect(14, 36, 212, 118, 10, 0x294A);

  // Progress Bar Track Base inside Hero Card (y: 86, h: 12, r: 6)
  tft.fillRoundRect(24, 86, 192, 12, 6, BG_BLACK);
  tft.drawRoundRect(24, 86, 192, 12, 6, 0x294A);

  // 3. Bottom Metric Cards Base (y: 160, h: 68, r: 8)
  // Left Temp Card
  tft.fillRoundRect(14, 160, 102, 68, 8, C_CARD);
  tft.drawRoundRect(14, 160, 102, 68, 8, 0x294A);

  // Right Time Card
  tft.fillRoundRect(124, 160, 102, 68, 8, C_CARD);
  tft.drawRoundRect(124, 160, 102, 68, 8, 0x294A);

  cache.progressPct = -999;
  cache.nozzleTemp = -999;
  cache.nozzleSide = -2;
  cache.bedTemp = -999;
  cache.chamberTemp = -999;
  cache.remainingMin = -999;
  cache.currentLayer = -999;
  cache.totalLayers = -999;
  cache.status = "";
  cache.displayName = "";
  cache.model = "";
  cache.alias = "";
  cache.layout = "dashboard";
  cache.activeTray = -999;
  cache.amsHash = 0;
  cache.amsExist = false;
  cache.dashTimeRemaining = -999;
  cache.dashStatus = "";
  cache.spdLvl = -999;
  cache.spdMag = -999;
  cache.baseDrawn = true;
  cache.offlineDrawn = false;
}

void drawDashboardFields() {
  // 1. Top Header Capsule Bar (Model on left, Status + LED on right)
  String model = getTopLeftDisplayName();
  String st = dashboardStatusText();
  if (model != cache.model || st != cache.status) {
    tft.fillRoundRect(14, 10, 212, 22, 11, C_CARD);
    tft.drawRoundRect(14, 10, 212, 22, 11, 0x294A);

    // Left: Model
    tft.setTextDatum(TL_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(C_TEXT, C_CARD);
    tft.drawString(model.length() ? model : "--", 24, 13);

    // Right: Status + LED
    uint16_t ledColor = pr.online ? statusColor() : C_ORANGE;
    tft.fillCircle(206, 21, 4, ledColor);

    tft.setTextDatum(TR_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(statusColor(), C_CARD);
    tft.drawString(st, 196, 13);

    cache.model = model;
    cache.status = st;
  }

  // 2. Middle Progress Hero Card (Percentage, Layer, Speed Mode & Glow-Head
  // Progress Bar)
  int progressValue = pr.progress >= 0 ? (int)(pr.progress + 0.5f) : -1;
  bool heroDirty = (progressValue != cache.progressPct) ||
                   (pr.currentLayer != cache.currentLayer) ||
                   (pr.totalLayers != cache.totalLayers) ||
                   (pr.spdLvl != cache.spdLvl) || (pr.spdMag != cache.spdMag);

  if (heroDirty) {
    // Clear Hero Card top text section
    tft.fillRect(16, 38, 208, 46, C_CARD);

    // Large Percentage Text on Left
    tft.setTextDatum(TL_DATUM);
    tft.setTextFont(7);
    tft.setTextColor(C_TEXT, C_CARD);
    if (progressValue >= 0) {
      char b[8];
      snprintf(b, sizeof(b), "%d", progressValue);
      tft.drawString(b, 24, 37);

      // Percentage Symbol at bottom-right of number
      int pctX = 24 + tft.textWidth(b) + 2;
      tft.setTextFont(4);
      tft.setTextColor(C_RING, C_CARD);
      tft.drawString("%", pctX, 58);
    } else {
      tft.drawString("--", 24, 37);
    }

    // Right Upper: Layer Info
    tft.setTextDatum(TR_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(C_CYAN, C_CARD);
    if (pr.currentLayer >= 0 && pr.totalLayers > 0) {
      char l[16];
      snprintf(l, sizeof(l), "L %d/%d", pr.currentLayer, pr.totalLayers);
      tft.drawString(l, 216, 40);
    } else if (pr.currentLayer >= 0) {
      char l[8];
      snprintf(l, sizeof(l), "L %d", pr.currentLayer);
      tft.drawString(l, 216, 40);
    } else {
      tft.drawString(st, 216, 40);
    }

    // Right Lower: Speed Profile (Option 1)
    uint16_t spdColor = C_DIM;
    String spdStr = "----";
    if (pr.spdLvl == 1) {
      spdColor = C_RING; // Green for Silent
      spdStr =
          String("Silent ") + (pr.spdMag > 0 ? String(pr.spdMag) + "%" : "50%");
    } else if (pr.spdLvl == 3) {
      spdColor = C_ORANGE; // Orange for Sport
      spdStr =
          String("Sport ") + (pr.spdMag > 0 ? String(pr.spdMag) + "%" : "124%");
    } else if (pr.spdLvl == 4) {
      spdColor = C_RED; // Red for Ludicrous
      spdStr = String("Ludicrous ") +
               (pr.spdMag > 0 ? String(pr.spdMag) + "%" : "166%");
    } else if (pr.spdLvl == 2) {
      // spdLvl == 2 (Standard)
      spdColor = C_CYAN; // Cyan for Standard
      spdStr =
          String("Std ") + (pr.spdMag > 0 ? String(pr.spdMag) + "%" : "100%");
    } else {
      spdColor = C_DIM;
      spdStr = "----";
    }

    tft.setTextDatum(TR_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(spdColor, C_CARD);
    tft.drawString(spdStr, 216, 58);

    // Progress Bar Track with Glow Head (y: 86, h: 12)
    tft.fillRoundRect(24, 86, 192, 12, 6, BG_BLACK);
    tft.drawRoundRect(24, 86, 192, 12, 6, 0x294A);

    if (progressValue > 0) {
      int barW = (progressValue * 192) / 100;
      if (barW > 0) {
        if (barW < 12)
          barW = 12;
        tft.fillRoundRect(24, 86, barW, 12, 6, C_RING);
        // Light trail glowing tip
        if (barW > 5) {
          tft.fillRoundRect(24 + barW - 5, 86, 5, 12, 2, C_TEXT);
        }
      }
    }

    cache.progressPct = progressValue;
    cache.currentLayer = pr.currentLayer;
    cache.totalLayers = pr.totalLayers;
    cache.spdLvl = pr.spdLvl;
    cache.spdMag = pr.spdMag;
  }

  // 3. AMS / Ext Tray Panel inside Hero Card (y: 102, h: 48)
  uint32_t _amsHash = 0;
  if (pr.amsExist) {
    _amsHash = 0xAA550000;
    for (int k = 0; k < 4; k++) {
      if (pr.amsSlots[k].valid) {
        _amsHash ^= pr.amsSlots[k].trayColor;
        _amsHash ^= (uint32_t)((uint8_t)pr.amsSlots[k].remain) << (k * 8);
        _amsHash ^= manualStringHash(pr.amsSlots[k].trayType) << 16;
        _amsHash ^= (pr.amsSlots[k].isOfficial ? 1 : 0) << (k + 8);
      }
    }
  } else {
    _amsHash = 0x55AA0000;
    const AmsTrayInfo &ext = pr.extSlot.valid ? pr.extSlot : pr.amsSlots[0];
    if (ext.valid) {
      _amsHash ^= ext.trayColor;
      _amsHash ^= (uint32_t)((uint8_t)ext.remain);
      _amsHash ^= manualStringHash(ext.trayType) << 16;
      _amsHash ^= (ext.isOfficial ? 1 : 0) << 8;
    }
  }

  if (_amsHash != cache.amsHash || pr.activeTray != cache.activeTray ||
      pr.amsExist != cache.amsExist) {
    if (pr.amsExist) {
      for (int k = 0; k < 4; k++) {
        int slotX = 22 + k * 47;
        bool isActive = (pr.activeTray == k);
        char slotNumStr[4];
        snprintf(slotNumStr, sizeof(slotNumStr), "%d", k + 1);
        char emptyStr[8];
        snprintf(emptyStr, sizeof(emptyStr), "A%d", k + 1);
        drawDashboardSlotCard(slotX, pr.amsSlots[k], isActive, slotNumStr,
                              emptyStr);
      }
    } else {
      // No AMS connected: Only draw slot 0 as "ext"
      const AmsTrayInfo &ext = pr.extSlot.valid ? pr.extSlot : pr.amsSlots[0];
      int slotX = 22;
      bool isActive = (pr.activeTray == 254 || pr.activeTray == 255 ||
                       pr.activeTray == 0 || pr.activeTray == 1 ||
                       pr.status == "running" || pr.status == "pause");
      drawDashboardSlotCard(slotX, ext, isActive, "ext", "ext");

      // Clear the area for slots 2, 3, 4 inside Hero Card (x: 68..210, y: 99..150)
      tft.fillRect(68, 99, 142, 51, C_CARD);
    }
    cache.amsHash = _amsHash;
    cache.activeTray = pr.activeTray;
    cache.amsExist = pr.amsExist;
  }

  // 3. Bottom Left Card: Temperatures (y: 160, h: 68)
  int nozzleSide = displayedNozzleSide();
  float nozzleT = displayedNozzleTemp();
  int nozzleV = nozzleT >= 0 ? (int)(nozzleT + 0.5f) : -1;
  int bedV = pr.bedTemp >= 0 ? (int)(pr.bedTemp + 0.5f) : -1;
  int chamberV = pr.chamberTemp >= 0 ? (int)(pr.chamberTemp + 0.5f) : -1;

  String chamberModelName =
      normalizedModelName(pr.model.length() ? pr.model : stored.model);
  if (!modelHasChamberSensor(chamberModelName))
    chamberV = -1;

  if (nozzleV != cache.nozzleTemp || bedV != cache.bedTemp ||
      chamberV != cache.chamberTemp || nozzleSide != cache.nozzleSide) {
    tft.fillRoundRect(14, 160, 102, 68, 8, C_CARD);
    tft.drawRoundRect(14, 160, 102, 68, 8, 0x294A);

    tft.setTextDatum(TL_DATUM);
    tft.setTextFont(2);

    // Nozzle
    tft.setTextColor(C_DIM, C_CARD);
    tft.drawString("NOZ", 20, 166);
    tft.setTextColor(nozzleV >= 0 ? C_YELLOW : C_DIM, C_CARD);
    char nb[12];
    if (nozzleV >= 0)
      snprintf(nb, sizeof(nb), "%dC", nozzleV);
    else
      strcpy(nb, "--");
    tft.drawString(nb, 62, 166);

    // Bed
    tft.setTextColor(C_DIM, C_CARD);
    tft.drawString("BED", 20, 186);
    tft.setTextColor(bedV >= 0 ? C_ORANGE : C_DIM, C_CARD);
    char bb[12];
    if (bedV >= 0)
      snprintf(bb, sizeof(bb), "%dC", bedV);
    else
      strcpy(bb, "--");
    tft.drawString(bb, 62, 186);

    // Chamber
    tft.setTextColor(C_DIM, C_CARD);
    tft.drawString("CHM", 20, 206);
    char cb[12];
    if (chamberV >= 0) {
      tft.setTextColor(C_CYAN, C_CARD);
      snprintf(cb, sizeof(cb), "%dC", chamberV);
    } else if (!modelHasChamberSensor(chamberModelName)) {
      tft.setTextColor(C_DIM, C_CARD);
      strcpy(cb, "N/A");
    } else {
      tft.setTextColor(C_DIM, C_CARD);
      strcpy(cb, "--");
    }
    tft.drawString(cb, 62, 206);

    cache.nozzleTemp = nozzleV;
    cache.nozzleSide = nozzleSide;
    cache.bedTemp = bedV;
    cache.chamberTemp = chamberV;
  }

  // 4. Bottom Right Card: Remaining Time & ETA (y: 160, h: 68)
  if (pr.remainingMin != cache.dashTimeRemaining ||
      pr.status != cache.dashStatus) {
    tft.fillRoundRect(124, 160, 102, 68, 8, C_CARD);
    tft.drawRoundRect(124, 160, 102, 68, 8, 0x294A);

    // Header Label
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(1);
    tft.setTextColor(C_DIM, C_CARD);
    tft.drawString("REMAINING", 175, 170);

    // Value
    tft.setTextFont(4);
    tft.setTextColor(C_CYAN, C_CARD);
    String rmText = (pr.status == "finish" || pr.progress >= 100)
                        ? "DONE"
                        : timeText(pr.remainingMin);
    tft.drawString(rmText, 175, 190);

    // ETA
    tft.setTextFont(1);
    tft.setTextColor(C_ORANGE, C_CARD);
    String etaStr = (pr.status == "finish" || pr.progress >= 100)
                        ? "DONE"
                        : String("ETA ") + etaText();
    tft.drawString(etaStr, 175, 213);

    cache.dashTimeRemaining = pr.remainingMin;
    cache.dashStatus = pr.status;
  }
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
  cache.chamberTemp = -999;
  cache.remainingMin = -999;
  cache.currentLayer = -999;
  cache.totalLayers = -999;
  cache.status = "";
  cache.displayName = "";
  cache.model = "";
  cache.alias = "";
  cache.layout = "classic";
  cache.baseDrawn = true;
  cache.offlineDrawn = false;
}

void updateFields() {
  int progressValue = pr.progress >= 0 ? (int)(pr.progress + 0.5f) : -1;
  if (progressToLen(pr.progress) != cache.progress)
    updateFrameProgress();
  if (progressValue != cache.progressPct) {
    char b[10];
    if (progressValue >= 0) {
      snprintf(b, sizeof(b), "%d", progressValue);
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
      drawDualNozzleTempBox(17, 116, 86, 30, nozzleSide, nozzle,
                            nozzle >= 0 ? C_TEXT : C_DIM);
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

  if (pr.currentLayer != cache.currentLayer ||
      pr.totalLayers != cache.totalLayers) {
    char b[24];
    if (pr.currentLayer >= 0 && pr.totalLayers > 0)
      snprintf(b, sizeof(b), "%d/%d", pr.currentLayer, pr.totalLayers);
    else
      strcpy(b, "L--");
    drawTextBox(50, 153, 140, 24, 2, C_TEXT, b, true);
    cache.currentLayer = pr.currentLayer;
    cache.totalLayers = pr.totalLayers;
  }

  if (pr.remainingMin != cache.remainingMin ||
      (pr.remainingMin <= 0 && pr.displayName != cache.displayName)) {
    String bottomText;
    if (pr.status == "finish" || pr.progress >= 100) {
      bottomText = "DONE";
    } else if (pr.remainingMin > 0) {
      int h = pr.remainingMin / 60;
      int m = pr.remainingMin % 60;
      char b[16];
      if (h > 0)
        snprintf(b, sizeof(b), "%d:%02d", h, m);
      else
        snprintf(b, sizeof(b), "%dm", m);
      bottomText = b;
    } else {
      const String &label = stored.name.length() ? stored.name : pr.displayName;
      bottomText =
          fitTextToWidth(label.length() ? label : String("--"), 4, 204, true);
    }
    drawTextBox(18, 181, 204, 34, 4, C_CYAN, bottomText, true);
    cache.remainingMin = pr.remainingMin;
    cache.displayName = pr.displayName;
  }
}

void drawClockScreen() {
  bool fullRedraw = (cache.layout != "clock");
  if (fullRedraw) {
    cache.hour = -1;
    cache.min = -1;
    cache.sec = -1;
    cache.clockProgressPct = -999;
    cache.clockNozzleTemp = -999;
    cache.clockBedTemp = -999;
    cache.clockRemainingMin = -999;
    cache.clockStatus = "";
    cache.clockOnline = !pr.online;
    tft.fillScreen(BG_BLACK);
    tft.drawRoundRect(4, 4, 232, 232, 8, 0x294A);
  }
  time_t now = time(nullptr);
  struct tm *info = localtime(&now);

  if (!info || now < 1700000000) {
    tft.fillScreen(BG_BLACK);
    tft.drawRoundRect(4, 4, 232, 232, 8, 0x294A);
    tft.setTextFont(4);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(C_DIM, BG_BLACK);
    tft.drawString("SYNC...", 120, 120);
    cache.hour = -1;
    cache.min = -1;
    cache.sec = -1;
    cache.layout = "clock";
    return;
  }

  int h = info->tm_hour;
  int m = info->tm_min;
  int s = info->tm_sec;
  int y = info->tm_year + 1900;
  int mo = info->tm_mon + 1;
  int d = info->tm_mday;
  int wd = info->tm_wday;

  // 1. Header & Main Clock Card Base (Redrawn only on entry or when minute
  // changes)
  if (fullRedraw || h != cache.hour || m != cache.min) {
    // Top Header Bar
    tft.fillRoundRect(14, 12, 108, 22, 11, C_CARD);
    tft.drawRoundRect(14, 12, 108, 22, 11, 0x294A);
    char dateStr[16];
    snprintf(dateStr, sizeof(dateStr), "%04d.%02d.%02d", y, mo, d);
    tft.setTextFont(2);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(C_TEXT, C_CARD);
    tft.drawString(dateStr, 68, 23);

    const char *weekdays[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    tft.fillRoundRect(128, 12, 64, 22, 11, C_CARD);
    tft.drawRoundRect(128, 12, 64, 22, 11, 0x294A);
    tft.setTextColor(0xFF40, C_CARD);
    tft.drawString(weekdays[wd], 160, 23);

    // Online LED dot
    uint16_t ledColor = pr.online ? C_RING : C_ORANGE;
    tft.fillCircle(208, 23, 5, ledColor);
    tft.drawCircle(208, 23, 5, BG_BLACK);

    // Main Clock Card Base
    tft.fillRoundRect(14, 42, 212, 108, 10, C_CARD);
    tft.drawRoundRect(14, 42, 212, 108, 10, 0x294A);

    // Draw time string HH MM
    char timeStr[8];
    snprintf(timeStr, sizeof(timeStr), "%02d %02d", h, m);
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(7);

    // Glow layers
    tft.setTextColor(0x5940, C_CARD);
    tft.drawString(timeStr, 122, 90);
    tft.drawString(timeStr, 118, 90);
    tft.drawString(timeStr, 122, 86);
    tft.drawString(timeStr, 118, 86);

    tft.setTextColor(0x9C40, C_CARD);
    tft.drawString(timeStr, 121, 89);
    tft.drawString(timeStr, 119, 89);
    tft.drawString(timeStr, 121, 87);
    tft.drawString(timeStr, 119, 87);

    // Main text
    tft.setTextColor(0xFF40, C_CARD);
    tft.drawString(timeStr, 120, 88);

    // Reset seconds bar track background on new minute
    tft.fillRoundRect(26, 136, 188, 5, 2, BG_BLACK);

    cache.hour = h;
    cache.min = m;
    cache.sec = -1; // Force seconds bar update for new minute
  }

  // 2. Seconds Colon & Incremental Seconds Bar (Zero-flicker)
  if (fullRedraw || s != cache.sec) {
    // Colon dots
    if (s % 2 == 0) {
      tft.fillCircle(120, 77, 3, 0xFF40);
      tft.fillCircle(120, 99, 3, 0xFF40);
    } else {
      tft.fillCircle(120, 77, 3, C_CARD);
      tft.fillCircle(120, 99, 3, C_CARD);
    }

    // Seconds bar incremental update
    int newW = (s * 188) / 59;
    if (s == 0) {
      tft.fillRoundRect(26, 136, 188, 5, 2, BG_BLACK);
    } else {
      int oldW = cache.sec >= 0 ? (cache.sec * 188) / 59 : 0;
      if (newW > oldW) {
        tft.fillRect(26 + oldW, 136, newW - oldW, 5, 0xFF40);
      }
    }
    cache.sec = s;
  }

  // 3. Bottom Printer Mini Status Card (ONLY REDRAWN ON PRINTER STATE CHANGE)
  bool hasActivePrint =
      pr.online && (isPrintingState(pr.status) || isPreparingState(pr.status) ||
                    isPausedState(pr.status));
  int pct = pr.progress >= 0 ? (int)(pr.progress + 0.5f) : 0;
  int nozV = pr.nozzleTemp >= 0 ? (int)(pr.nozzleTemp + 0.5f) : -1;
  int bedV = pr.bedTemp >= 0 ? (int)(pr.bedTemp + 0.5f) : -1;
  String stStr = dashboardStatusText();

  bool bottomDirty =
      fullRedraw || (pr.online != cache.clockOnline) ||
      (stStr != cache.clockStatus) || (pct != cache.clockProgressPct) ||
      (nozV != cache.clockNozzleTemp) || (bedV != cache.clockBedTemp) ||
      (pr.remainingMin != cache.clockRemainingMin);

  if (bottomDirty) {
    tft.fillRoundRect(14, 158, 212, 68, 8, C_CARD);
    tft.drawRoundRect(14, 158, 212, 68, 8, 0x294A);

    if (hasActivePrint) {
      // Active Print layout: Model & Status Pill, Progress bar, Temps/ETA
      String modelStr =
          normalizedModelName(pr.model.length() ? pr.model : stored.model);
      tft.setTextFont(2);
      tft.setTextDatum(TL_DATUM);
      tft.setTextColor(C_TEXT, C_CARD);
      tft.drawString(modelStr.length() ? modelStr : "PRINT", 24, 166);

      // Status Pill on right
      tft.setTextDatum(TR_DATUM);
      tft.setTextColor(statusColor(), C_CARD);
      tft.drawString(stStr, 204, 166);

      // Progress Bar
      tft.fillRoundRect(24, 186, 180, 6, 3, BG_BLACK);
      if (pct > 0) {
        int pW = (pct * 180) / 100;
        tft.fillRoundRect(24, 186, pW, 6, 3, C_RING);
      }

      // Temps & ETA / Remaining Time
      tft.setTextDatum(TL_DATUM);
      tft.setTextFont(1);
      tft.setTextColor(C_DIM, C_CARD);
      char infoBuf[40];
      snprintf(infoBuf, sizeof(infoBuf), "NOZ %dC  BED %dC",
               nozV >= 0 ? nozV : 0, bedV >= 0 ? bedV : 0);
      tft.drawString(infoBuf, 24, 198);

      tft.setTextDatum(TR_DATUM);
      tft.setTextFont(2);
      tft.setTextColor(C_CYAN, C_CARD);
      String rmText = (pr.status == "finish" || pr.progress >= 100)
                          ? "DONE"
                          : timeText(pr.remainingMin);
      tft.drawString(rmText, 204, 198);

    } else {
      // Standby / Idle layout: Bambu Logo + Model Name + State
      tft.drawBitmap(24, 176, BAMBU_LOGO, BAMBU_LOGO_W, BAMBU_LOGO_H, C_RING);

      String modelStr =
          normalizedModelName(pr.model.length() ? pr.model : stored.model);
      String nameStr =
          stored.name.length()
              ? stored.name
              : (pr.displayName.length() ? pr.displayName : modelStr);

      tft.setTextFont(2);
      tft.setTextDatum(TL_DATUM);
      tft.setTextColor(C_TEXT, C_CARD);
      tft.drawString(
          fitTextToWidth(nameStr.length() ? nameStr : "Printer", 2, 140, false),
          56, 174);

      tft.setTextFont(1);
      tft.setTextColor(C_DIM, C_CARD);
      tft.drawString(modelStr.length() ? modelStr : "Bambu Lab", 56, 194);

      tft.setTextFont(2);
      tft.setTextDatum(TR_DATUM);
      if (!pr.online) {
        tft.setTextColor(C_ORANGE, C_CARD);
        tft.drawString("OFFLINE", 204, 184);
      } else {
        tft.setTextColor(C_CYAN, C_CARD);
        tft.drawString("READY", 204, 184);
      }
    }

    cache.clockOnline = pr.online;
    cache.clockStatus = stStr;
    cache.clockProgressPct = pct;
    cache.clockNozzleTemp = nozV;
    cache.clockBedTemp = bedV;
    cache.clockRemainingMin = pr.remainingMin;
  }

  cache.layout = "clock";
}
void renderOffline() {
  if (cache.offlineDrawn)
    return;
  tft.startWrite();
  tft.fillScreen(BG_BLACK);
  drawFrameTrack();
  drawTextBox(50, 93, 140, 34, 4, C_ORANGE, "OFFLINE", true);
  String net = "NO WIFI";
  if (WiFi.status() == WL_CONNECTED) {
    net = String("IP ") + WiFi.localIP().toString();
  } else if (WiFi.status() == WL_IDLE_STATUS ||
             WiFi.status() == WL_DISCONNECTED) {
    net = "WIFI...";
  }
  drawTextBox(28, 128, 184, 20, 2, C_DIM, net, false);
  tft.endWrite();
  cache.offlineDrawn = true;
  cache.baseDrawn = false;
}

void renderDisplay() {
  tft.startWrite();

  bool hasActivePrint =
      pr.online && (isPrintingState(pr.status) || isPreparingState(pr.status) ||
                    isPausedState(pr.status));

  if (stored.layout == "clock") {
    drawClockScreen();
    cache.baseDrawn = false;
    cache.offlineDrawn = false;
  } else if (hasActivePrint) {
    if (stored.layout == "dashboard") {
      if (!cache.baseDrawn || cache.offlineDrawn || cache.layout != "dashboard")
        drawDashboardBase();
      drawDashboardFields();
    } else {
      if (!cache.baseDrawn || cache.offlineDrawn || cache.layout != "classic")
        drawBase();
      updateFields();
    }
  } else {
    // Respect user layout choice when idle
    if (stored.layout == "dashboard") {
      if (!cache.baseDrawn || cache.offlineDrawn || cache.layout != "dashboard")
        drawDashboardBase();
      drawDashboardFields();
    } else if (stored.layout == "classic") {
      if (!cache.baseDrawn || cache.offlineDrawn || cache.layout != "classic")
        drawBase();
      updateFields();
    } else {
      drawClockScreen();
      cache.baseDrawn = false;
      cache.offlineDrawn = false;
    }
  }

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
  applyBrightness(stored.brightness);

  tft.fillScreen(BG_BLACK);
  drawTextBox(42, 92, 156, 28, 4, C_TEXT, "WIFI...", true);
  mqttNet.setInsecure();
  mqttNet.setBufferSizes(512, 512);
  mqttNet.setTimeout(8000);
  wifiConnect();
  if (wifiHasIp()) {
    tft.fillScreen(BG_BLACK);
    drawTextBox(24, 96, 192, 22, 2, C_TEXT, WiFi.localIP().toString(), true);
    delay(800);
  }
  if (httpNetworkReady())
    startEspServer();
  applyBrightnessSchedule();
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

  if (httpNetworkReady() && !serverStarted)
    startEspServer();

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
      if (httpNetworkReady())
        startEspServer();
    }
  }

  if (!configMode && WiFi.status() == WL_CONNECTED && mqttConfigReady()) {
    if (!mqttNet.connected() &&
        now - lastMqttConnect >= MQTT_RECONNECT_INTERVAL) {
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

  // NTP time sync retry (every 60s if not synced)
  static unsigned long lastNtpCheck = 0;
  if (now - lastNtpCheck >= 60000) {
    lastNtpCheck = now;
    if (time(nullptr) < 1700000000 && WiFi.status() == WL_CONNECTED) {
      configTime(8 * 3600, 0, "ntp.aliyun.com", "cn.pool.ntp.org",
                 "pool.ntp.org");
    }
  }

  // Brightness schedule check (every 3s for instant auto-apply on
  // boot/time-sync)
  static unsigned long lastBrsCheck = 0;
  if (now - lastBrsCheck >= 3000) {
    lastBrsCheck = now;
    applyBrightnessSchedule();
  }

  if (hasDualNozzleTemps() && displayedNozzleSide() != cache.nozzleSide) {
    displayDirty = true;
  }

  bool noActivePrint =
      !pr.online || !(isPrintingState(pr.status) ||
                      isPreparingState(pr.status) || isPausedState(pr.status));
  bool isClockMode = stored.layout == "clock";

  if ((noActivePrint || isClockMode) && now - lastDisplay >= 1000) {
    displayDirty = true;
  }

  if (displayDirty && now - lastDisplay >= DISPLAY_REFRESH) {
    displayDirty = false;
    lastDisplay = now;
    renderDisplay();
  }

  delay(10);
}
