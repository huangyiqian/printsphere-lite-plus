import re

with open('M:/codex/1/src/main.cpp', 'r', encoding='utf-8') as f:
    c = f.read()

changes = 0

# 1. AmsTrayInfo struct before PrinterState
if 'struct AmsTrayInfo' not in c:
    old = 'struct PrinterState {\n  float progress = -1;'
    new = 'struct AmsTrayInfo {\n  uint32_t trayColor = 0;\n  int remain = -1;\n  String trayType = "";\n  String traySubBrands = "";\n  bool valid = false;\n};\n\nstruct PrinterState {\n  float progress = -1;'
    c = c.replace(old, new, 1)
    changes += 1
    print("C1 done")

# 2. amsSlots in PrinterState  
if 'AmsTrayInfo amsSlots' not in c:
    old = '  bool dualNozzle = false;\n};'
    new = '  bool dualNozzle = false;\n  AmsTrayInfo amsSlots[4];\n};'
    c = c.replace(old, new, 1)
    changes += 1
    print("C2 done")

# 3. amsHash in RenderCache
if 'uint32_t amsHash' not in c:
    old = '  String layout = "";\n};'
    new = '  String layout = "";\n  uint32_t amsHash = 0;\n};'
    c = c.replace(old, new, 1)
    changes += 1
    print("C3 done")

# 4. AMS reset in resetLivePrintFields
if 'pr.amsSlots[i].remain = -1' not in c:
    old = '  displayDirty = true;\n}\n\nvoid wifiConnect()'
    new = '  for (int i = 0; i < 4; i++) {\n    pr.amsSlots[i].valid = false;\n    pr.amsSlots[i].trayColor = 0;\n    pr.amsSlots[i].remain = -1;\n    pr.amsSlots[i].trayType = "";\n    pr.amsSlots[i].traySubBrands = "";\n  }\n  displayDirty = true;\n}\n\nvoid wifiConnect()'
    c = c.replace(old, new, 1)
    changes += 1
    print("C4 done")

# 5. AMS filter entries in parseMqttPayload
if 'filter["print"]["ams"]["ams"][0]' not in c:
    old = '  filter["print"]["device"]["nozzle"]["info"][1]["id"] = true;\n  filter["print"]["device"]["nozzle"]["info"][1]["temp"] = true;\n\n  JsonDocument doc;'
    new = '  filter["print"]["device"]["nozzle"]["info"][1]["id"] = true;\n  filter["print"]["device"]["nozzle"]["info"][1]["temp"] = true;\n\n  filter["print"]["ams"]["ams"][0]["tray"][0]["tray_type"] = true;\n  filter["print"]["ams"]["ams"][0]["tray"][0]["tray_sub_brands"] = true;\n  filter["print"]["ams"]["ams"][0]["tray"][0]["tray_color"] = true;\n  filter["print"]["ams"]["ams"][0]["tray"][0]["remain"] = true;\n\n  JsonDocument doc;'
    c = c.replace(old, new, 1)
    changes += 1
    print("C5 done")
else:
    print("C5 already present")

# 6. AMS parsing in applyPrint
if 'Parse AMS tray data' not in c:
    old = '  if (i != -999) pr.totalLayers = i;\n\n  pr.online = true;\n  displayDirty = true;\n}\n\nconst char* findPattern'
    new = '  if (i != -999) pr.totalLayers = i;\n\n  // Parse AMS tray data\n  JsonObject amsObj = print["ams"];\n  if (!amsObj.isNull()) {\n    JsonArray amsArray = amsObj["ams"];\n    if (!amsArray.isNull() && amsArray.size() > 0) {\n      JsonObject firstAms = amsArray[0];\n      JsonArray trays = firstAms["tray"];\n      if (!trays.isNull()) {\n        int count = min((int)trays.size(), 4);\n        for (int i = 0; i < count; i++) {\n          JsonObject tray = trays[i];\n          const char* tt = tray["tray_type"];\n          if (tt && strlen(tt) > 0) {\n            pr.amsSlots[i].valid = true;\n            pr.amsSlots[i].trayType = tt;\n            pr.amsSlots[i].traySubBrands = tray["tray_sub_brands"] | "";\n            const char* tc = tray["tray_color"];\n            if (tc && strlen(tc) >= 8) {\n              pr.amsSlots[i].trayColor = strtoul(tc, nullptr, 16);\n            }\n            pr.amsSlots[i].remain = tray["remain"] | -1;\n          } else {\n            for (int j = i; j < 4; j++) {\n              pr.amsSlots[j].valid = false;\n              pr.amsSlots[j].trayColor = 0;\n              pr.amsSlots[j].remain = -1;\n              pr.amsSlots[j].trayType = "";\n              pr.amsSlots[j].traySubBrands = "";\n            }\n            break;\n          }\n        }\n      }\n    }\n  }\n\n  pr.online = true;\n  displayDirty = true;\n}\n\nconst char* findPattern'
    c = c.replace(old, new, 1)
    changes += 1
    print("C6 done")
else:
    print("C6 already present")

# 7. manualStringHash and filamentColor functions
if 'manualStringHash' not in c:
    old = 'void drawDashboardBase() {'
    new = 'static uint32_t manualStringHash(const String& s) {\n  uint32_t h = 0;\n  for (unsigned int i = 0; i < s.length(); i++) {\n    h = h * 31 + (uint8_t)s.charAt(i);\n  }\n  return h;\n}\n\n/**\n * Convert ARGB (32-bit) to RGB565 for TFT display.\n * SD2 display has cyan and yellow swapped.\n * If the filament color is cyan, swap to yellow (and vice versa).\n */\nuint16_t filamentColor(uint32_t argb) {\n  uint8_t r = (argb >> 16) & 0xFF;\n  uint8_t g = (argb >> 8) & 0xFF;\n  uint8_t b = argb & 0xFF;\n\n  // Normal 565 conversion\n  uint16_t normal = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);\n\n  // Detect cyan: green+blue dominate, red low\n  bool isCyan = (g > 160 && b > 160 && r < 100);\n  // Detect yellow: red+green dominate, blue low\n  bool isYellow = (r > 160 && g > 160 && b < 100);\n\n  if (isCyan || isYellow) {\n    // Swap red and blue to counteract display hw swap\n    return ((b >> 3) << 11) | ((g >> 2) << 5) | (r >> 3);\n  }\n  return normal;\n}\n\nvoid drawDashboardBase() {'
    c = c.replace(old, new, 1)
    changes += 1
    print("C7 done")
else:
    print("C7 already present")

# 8. amsHash reset in drawDashboardBase
if 'cache.amsHash = 0;' not in c:
    old = '  cache.alias = "";\n  cache.layout = "dashboard";\n  cache.baseDrawn = true;\n  cache.offlineDrawn = false;'
    new = '  cache.alias = "";\n  cache.amsHash = 0;\n  cache.layout = "dashboard";\n  cache.baseDrawn = true;\n  cache.offlineDrawn = false;'
    c = c.replace(old, new, 1)
    changes += 1
    print("C8 done")
else:
    print("C8 already present")

print(f"Total changes: {changes}")
with open('M:/codex/1/src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(c)
print("Saved")
