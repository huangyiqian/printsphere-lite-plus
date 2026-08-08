import re

with open("M:/codex/1/src/main.cpp", "r", encoding="utf-8") as f:
    c = f.read()

changes = 0

# 1. AmsTrayInfo struct
if "struct AmsTrayInfo {" not in c:
    old = 'struct PrinterState {\n  float progress = -1;'
    new = 'struct AmsTrayInfo {\n  uint32_t trayColor = 0;\n  int remain = -1;\n  String trayType = "";\n  String traySubBrands = "";\n  bool valid = false;\n};\n\nstruct PrinterState {\n  float progress = -1;'
    c = c.replace(old, new, 1)
    changes += 1
    print("C1 done")

# 2. amsSlots in PrinterState
if "AmsTrayInfo amsSlots" not in c:
    old = '  bool dualNozzle = false;\n};'
    new = '  bool dualNozzle = false;\n  AmsTrayInfo amsSlots[4];\n};'
    c = c.replace(old, new, 1)
    changes += 1
    print("C2 done")

# 3. amsHash in RenderCache
if "uint32_t amsHash = 0;" not in c:
    old = '  int sec = -1;\n};'
    new = '  uint32_t amsHash = 0;\n  int sec = -1;\n};'
    c = c.replace(old, new, 1)
    changes += 1
    print("C3 done")

# 4. AMS reset in resetLivePrintFields
if "pr.amsSlots[i].remain = -1" not in c:
    old = '  displayDirty = true;\n}\n\nvoid wifiConnect()'
    new = '  for (int i = 0; i < 4; i++) {\n    pr.amsSlots[i].valid = false;\n    pr.amsSlots[i].trayColor = 0;\n    pr.amsSlots[i].remain = -1;\n    pr.amsSlots[i].trayType = "";\n    pr.amsSlots[i].traySubBrands = "";\n  }\n  displayDirty = true;\n}\n\nvoid wifiConnect()'
    c = c.replace(old, new, 1)
    changes += 1
    print("C4 done")

# 5. AMS parsing in applyPrint
if "Parse AMS tray data" not in c:
    old = '  if (i != -999) pr.totalLayers = i;\n\n  pr.online = true;\n  displayDirty = true;\n}'
    new = '  if (i != -999) pr.totalLayers = i;\n\n  // Parse AMS tray data\n  JsonObject amsObj = print["ams"];\n  if (!amsObj.isNull()) {\n    JsonArray amsArray = amsObj["ams"];\n    if (!amsArray.isNull() && amsArray.size() > 0) {\n      JsonObject firstAms = amsArray[0];\n      JsonArray trays = firstAms["tray"];\n      if (!trays.isNull()) {\n        int count = min((int)trays.size(), 4);\n        for (int i = 0; i < count; i++) {\n          JsonObject tray = trays[i];\n          const char* tt = tray["tray_type"];\n          if (tt && strlen(tt) > 0) {\n            pr.amsSlots[i].valid = true;\n            pr.amsSlots[i].trayType = tt;\n            pr.amsSlots[i].traySubBrands = tray["tray_sub_brands"] | "";\n            const char* tc = tray["tray_color"];\n            if (tc && strlen(tc) >= 8) {\n              pr.amsSlots[i].trayColor = strtoul(tc, nullptr, 16);\n            }\n            pr.amsSlots[i].remain = tray["remain"] | -1;\n          } else {\n            for (int j = i; j < 4; j++) {\n              pr.amsSlots[j].valid = false;\n              pr.amsSlots[j].trayColor = 0;\n              pr.amsSlots[j].remain = -1;\n              pr.amsSlots[j].trayType = "";\n              pr.amsSlots[j].traySubBrands = "";\n            }\n            break;\n          }\n        }\n      }\n    }\n  }\n\n  pr.online = true;\n  displayDirty = true;\n}'
    c = c.replace(old, new, 1)
    changes += 1
    print("C5 done")

# 6. AMS filter entries in parseMqttPayload
if 'filter["print"]["ams"]["ams"][0]' not in c:
    old = '  filter["print"]["device"]["nozzle"]["info"][1]["id"] = true;\n  filter["print"]["device"]["nozzle"]["info"][1]["temp"] = true;\n\n  JsonDocument doc;'
    new = '  filter["print"]["device"]["nozzle"]["info"][1]["id"] = true;\n  filter["print"]["device"]["nozzle"]["info"][1]["temp"] = true;\n\n  filter["print"]["ams"]["ams"][0]["tray"][0]["tray_type"] = true;\n  filter["print"]["ams"]["ams"][0]["tray"][0]["tray_sub_brands"] = true;\n  filter["print"]["ams"]["ams"][0]["tray"][0]["tray_color"] = true;\n  filter["print"]["ams"]["ams"][0]["tray"][0]["remain"] = true;\n\n  JsonDocument doc;'
    c = c.replace(old, new, 1)
    changes += 1
    print("C6 done")

# 7. filamentColor function
if "filamentColor" not in c:
    old = 'void drawDashboardBase() {'
    new = 'uint16_t filamentColor(uint32_t argb) {\n  uint8_t r = (argb >> 16) & 0xFF;\n  uint8_t g = (argb >> 8) & 0xFF;\n  uint8_t b = argb & 0xFF;\n\n  // Normal 565\n  uint16_t normal = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);\n\n  // Detect cyan: green+blue dominate, red low\n  bool isCyan = (g > 160 && b > 160 && r < 100);\n  // Detect yellow: red+green dominate, blue low\n  bool isYellow = (r > 160 && g > 160 && b < 100);\n\n  if (isCyan || isYellow) {\n    // Swap R and B to counteract display hw swap\n    return ((b >> 3) << 11) | ((g >> 2) << 5) | (r >> 3);\n  }\n  return normal;\n}\n\nvoid drawDashboardBase() {'
    c = c.replace(old, new, 1)
    changes += 1
    print("C7 done")

# 8. AMS display in drawDashboardFields
if "AMS filament info" not in c:
    old = '    cache.remainingMin = pr.remainingMin;\n  }\n}\n\n\nvoid drawBase() {'
    new = '    cache.remainingMin = pr.remainingMin;\n  }\n\n  // 7. AMS filament info - left and right of progress ring\n  {\n    uint32_t _amsHash = 0;\n    for (int i = 0; i < 4; i++) {\n      if (pr.amsSlots[i].valid) {\n        _amsHash ^= pr.amsSlots[i].trayColor;\n        _amsHash ^= (uint32_t)((uint8_t)pr.amsSlots[i].remain) << (i * 8);\n        _amsHash ^= (uint32_t)pr.amsSlots[i].trayType.hashCode() << 16;\n      }\n    }\n    if (_amsHash != cache.amsHash) {\n      for (int i = 0; i < 4; i++) {\n        const AmsTrayInfo& slot = pr.amsSlots[i];\n        int y = 78 + i * 30;\n\n        if (slot.valid) {\n          uint16_t swatchColor = filamentColor(slot.trayColor);\n          tft.fillCircle(37, y, 7, swatchColor);\n          tft.drawCircle(37, y, 7, 0x294A);\n\n          String label = slot.traySubBrands.length() ? slot.traySubBrands : slot.trayType;\n          if (label.startsWith("PLA")) label = "PLA";\n          else if (label.startsWith("ABS")) label = "ABS";\n          else if (label.startsWith("PETG")) label = "PETG";\n          else if (label.startsWith("TPU")) label = "TPU";\n          else if (label.startsWith("PA")) label = "PA";\n          else if (label.startsWith("PC")) label = "PC";\n          else if (label.startsWith("ASA")) label = "ASA";\n          else if (label.startsWith("PP")) label = "PP";\n          else if (label.startsWith("PPS")) label = "PPS";\n          else if (label.startsWith("HIPS")) label = "HIPS";\n          else if (label.startsWith("BVOH")) label = "BVOH";\n          else if (label.startsWith("PVA")) label = "PVA";\n          else if (label.startsWith("PE")) label = "PE";\n          else if (label.startsWith("GF")) label = "GF";\n          else if (label.startsWith("CF")) label = "CF";\n          else if (label.startsWith("Support")) label = "SPT";\n          else if (label.length() > 5) label = label.substring(0, 5);\n\n          tft.setTextDatum(MC_DATUM);\n          tft.setTextFont(1);\n          tft.setTextColor(C_TEXT, BG_BLACK);\n          tft.setTextPadding(0);\n          tft.drawString(label, 206, y);\n\n          if (slot.remain >= 0 && slot.remain <= 100) {\n            char rbuf[8];\n            snprintf(rbuf, sizeof(rbuf), "%d%%", slot.remain);\n            tft.setTextFont(1);\n            tft.setTextColor(C_DIM, BG_BLACK);\n            tft.drawString(rbuf, 206, y + 10);\n          }\n        } else {\n          tft.fillCircle(37, y, 7, C_CARD);\n          tft.drawCircle(37, y, 7, 0x294A);\n        }\n      }\n      cache.amsHash = _amsHash;\n    }\n  }\n}\n\n\nvoid drawBase() {'
    c = c.replace(old, new, 1)
    changes += 1
    print("C8 done")

# 9. amsHash reset in drawDashboardBase
if 'cache.amsHash = 0;\n  cache.layout = "dashboard";' not in c:
    old = '  cache.alias = "";\n  cache.layout = "dashboard";\n  cache.baseDrawn = true;'
    new = '  cache.alias = "";\n  cache.amsHash = 0;\n  cache.layout = "dashboard";\n  cache.baseDrawn = true;'
    c = c.replace(old, new, 1)
    changes += 1
    print("C9 done")

# 10. amsHash reset in drawBase (classic layout)
if 'cache.amsHash = 0;\n  cache.layout = "classic";' not in c:
    old = 'cache.layout = "classic";\n  cache.baseDrawn = true;'
    new = 'cache.amsHash = 0;\n  cache.layout = "classic";\n  cache.baseDrawn = true;'
    c = c.replace(old, new, 1)
    changes += 1
    print("C10 done")

# Fix: hashCode() doesn't exist on Arduino String, use a manual hash
# Replace hashCode() calls with manual hash computation
c = c.replace("pr.amsSlots[i].trayType.hashCode()", "manualStringHash(pr.amsSlots[i].trayType)")

# Add manualStringHash function before filamentColor
if "manualStringHash" not in c:
    hash_func = 'static uint32_t manualStringHash(const String& s) {\n  uint32_t h = 0;\n  for (unsigned int i = 0; i < s.length(); i++) {\n    h = h * 31 + (uint8_t)s.charAt(i);\n  }\n  return h;\n}\n\n'
    # Insert before filamentColor
    c = c.replace("uint16_t filamentColor(uint32_t argb) {", hash_func + "uint16_t filamentColor(uint32_t argb) {")
    changes += 1
    print("Hash function fix done")

# Verify brace balance
ob = c.count('{')
cb = c.count('}')
print(f"\nBrace balance: opens={ob}, closes={cb}, net={ob-cb}")

with open("M:/codex/1/src/main.cpp", "w", encoding="utf-8") as f:
    f.write(c)

print(f"{changes} changes applied, saved")