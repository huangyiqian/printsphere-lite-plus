import re

with open('M:/codex/1/src/main.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# 1) Add amsHash to RenderCache - insert before '};' closing the struct
content = content.replace(
    "    String layout = \"\";\n  };",
    "    String layout = \"\";\n    uint32_t amsHash = 0;\n  };"
)

# 2) In drawDashboardBase, add cache.amsHash = 0 to the cache reset section
content = content.replace(
    "  cache.layout = \"dashboard\";\n  cache.baseDrawn = true;\n  cache.offlineDrawn = false;\n}",
    "  cache.layout = \"dashboard\";\n  cache.amsHash = 0;\n  cache.baseDrawn = true;\n  cache.offlineDrawn = false;\n}"
)

# 3) In drawBase (classic layout), add cache.amsHash = 0
content = content.replace(
    "  cache.totalLayers = -999;\n  cache.status = \"\";\n  cache.displayName = \"\";",
    "  cache.totalLayers = -999;\n  cache.amsHash = 0;\n  cache.status = \"\";\n  cache.displayName = \"\";"
)

# 4) Fix brace structure: close the if(remainingMin) block before AMS section
#    Add proper caching for AMS display
old_ams = (
    "    cache.remainingMin = pr.remainingMin;\n"
    "\n"
    "  // 7. AMS filament info - left and right of progress ring\n"
    "  for (int i = 0; i < 4; i++) {\n"
    "    const AmsTrayInfo& slot = pr.amsSlots[i];\n"
    "    int y = 78 + i * 30;\n"
    "\n"
    "    if (slot.valid) {\n"
    "      uint16_t swatchColor = filamentColor(slot.trayColor);\n"
    "      tft.fillCircle(37, y, 7, swatchColor);\n"
    "      tft.drawCircle(37, y, 7, 0x294A);\n"
    "\n"
    "      String label = slot.traySubBrands.length() ? slot.traySubBrands : slot.trayType;\n"
    "      if (label.startsWith(\"PLA\")) label = \"PLA\";\n"
    "      else if (label.startsWith(\"ABS\")) label = \"ABS\";\n"
    "      else if (label.startsWith(\"PETG\")) label = \"PETG\";\n"
    "      else if (label.startsWith(\"TPU\")) label = \"TPU\";\n"
    "      else if (label.startsWith(\"PA\")) label = \"PA\";\n"
    "      else if (label.startsWith(\"PC\")) label = \"PC\";\n"
    "      else if (label.startsWith(\"ASA\")) label = \"ASA\";\n"
    "      else if (label.startsWith(\"PP\")) label = \"PP\";\n"
    "      else if (label.startsWith(\"PPS\")) label = \"PPS\";\n"
    "      else if (label.startsWith(\"HIPS\")) label = \"HIPS\";\n"
    "      else if (label.startsWith(\"BVOH\")) label = \"BVOH\";\n"
    "      else if (label.startsWith(\"PVA\")) label = \"PVA\";\n"
    "      else if (label.startsWith(\"PE\")) label = \"PE\";\n"
    "      else if (label.startsWith(\"GF\")) label = \"GF\";\n"
    "      else if (label.startsWith(\"CF\")) label = \"CF\";\n"
    "      else if (label.startsWith(\"Support\")) label = \"SPT\";\n"
    "      else if (label.length() > 5) label = label.substring(0, 5);\n"
    "\n"
    "      tft.setTextDatum(MC_DATUM);\n"
    "      tft.setTextFont(1);\n"
    "      tft.setTextColor(C_TEXT, BG_BLACK);\n"
    "      tft.setTextPadding(0);\n"
    "      tft.drawString(label, 206, y);\n"
    "\n"
    "      if (slot.remain >= 0 && slot.remain <= 100) {\n"
    "        char rbuf[8];\n"
    "        snprintf(rbuf, sizeof(rbuf), \"%d%%\", slot.remain);\n"
    "        tft.setTextFont(1);\n"
    "        tft.setTextColor(C_DIM, BG_BLACK);\n"
    "        tft.drawString(rbuf, 206, y + 10);\n"
    "      }\n"
    "    } else {\n"
    "      tft.fillCircle(37, y, 7, C_CARD);\n"
    "      tft.drawCircle(37, y, 7, 0x294A);\n"
    "    }\n"
    "  }\n"
    "  }\n"
    "}"
)

new_ams = (
    "    cache.remainingMin = pr.remainingMin;\n"
    "  }\n"
    "\n"
    "  // 7. AMS filament info - left and right of progress ring\n"
    "  {\n"
    "    uint32_t _amsHash = 0;\n"
    "    for (int i = 0; i < 4; i++) {\n"
    "      if (pr.amsSlots[i].valid) {\n"
    "        _amsHash ^= pr.amsSlots[i].trayColor;\n"
    "        _amsHash ^= (uint32_t)((uint8_t)pr.amsSlots[i].remain) << (i * 8);\n"
    "        _amsHash ^= (uint32_t)pr.amsSlots[i].trayType.hashCode() << 16;\n"
    "      }\n"
    "    }\n"
    "    if (_amsHash != cache.amsHash) {\n"
    "      for (int i = 0; i < 4; i++) {\n"
    "        const AmsTrayInfo& slot = pr.amsSlots[i];\n"
    "        int y = 78 + i * 30;\n"
    "\n"
    "        if (slot.valid) {\n"
    "          uint16_t swatchColor = filamentColor(slot.trayColor);\n"
    "          tft.fillCircle(37, y, 7, swatchColor);\n"
    "          tft.drawCircle(37, y, 7, 0x294A);\n"
    "\n"
    "          String label = slot.traySubBrands.length() ? slot.traySubBrands : slot.trayType;\n"
    "          if (label.startsWith(\"PLA\")) label = \"PLA\";\n"
    "          else if (label.startsWith(\"ABS\")) label = \"ABS\";\n"
    "          else if (label.startsWith(\"PETG\")) label = \"PETG\";\n"
    "          else if (label.startsWith(\"TPU\")) label = \"TPU\";\n"
    "          else if (label.startsWith(\"PA\")) label = \"PA\";\n"
    "          else if (label.startsWith(\"PC\")) label = \"PC\";\n"
    "          else if (label.startsWith(\"ASA\")) label = \"ASA\";\n"
    "          else if (label.startsWith(\"PP\")) label = \"PP\";\n"
    "          else if (label.startsWith(\"PPS\")) label = \"PPS\";\n"
    "          else if (label.startsWith(\"HIPS\")) label = \"HIPS\";\n"
    "          else if (label.startsWith(\"BVOH\")) label = \"BVOH\";\n"
    "          else if (label.startsWith(\"PVA\")) label = \"PVA\";\n"
    "          else if (label.startsWith(\"PE\")) label = \"PE\";\n"
    "          else if (label.startsWith(\"GF\")) label = \"GF\";\n"
    "          else if (label.startsWith(\"CF\")) label = \"CF\";\n"
    "          else if (label.startsWith(\"Support\")) label = \"SPT\";\n"
    "          else if (label.length() > 5) label = label.substring(0, 5);\n"
    "\n"
    "          tft.setTextDatum(MC_DATUM);\n"
    "          tft.setTextFont(1);\n"
    "          tft.setTextColor(C_TEXT, BG_BLACK);\n"
    "          tft.setTextPadding(0);\n"
    "          tft.drawString(label, 206, y);\n"
    "\n"
    "          if (slot.remain >= 0 && slot.remain <= 100) {\n"
    "            char rbuf[8];\n"
    "            snprintf(rbuf, sizeof(rbuf), \"%d%%\", slot.remain);\n"
    "            tft.setTextFont(1);\n"
    "            tft.setTextColor(C_DIM, BG_BLACK);\n"
    "            tft.drawString(rbuf, 206, y + 10);\n"
    "          }\n"
    "        } else {\n"
    "          tft.fillCircle(37, y, 7, C_CARD);\n"
    "          tft.drawCircle(37, y, 7, 0x294A);\n"
    "        }\n"
    "      }\n"
    "      cache.amsHash = _amsHash;\n"
    "    }\n"
    "  }\n"
    "}"
)

if old_ams in content:
    content = content.replace(old_ams, new_ams, 1)
    print("AMS block replaced successfully")
else:
    print("ERROR: Could not find old AMS block!")
    print("First 80 chars of old_ams:", repr(old_ams[:80]))
    # Search for partial match
    pos = content.find("cache.remainingMin = pr.remainingMin")
    if pos >= 0:
        print("Found 'cache.remainingMin = pr.remainingMin' at position", pos)
        print("Context:", repr(content[pos:pos+200]))

with open('M:/codex/1/src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print("Done!")
