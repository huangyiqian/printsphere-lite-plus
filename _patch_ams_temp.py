import re

with open('M:/codex/1/src/main.cpp', 'r', encoding='utf-8') as f:
    c = f.read()

changes = 0

# 9. AMS display in drawDashboardFields - insert before "void drawBase()"
old = '    cache.remainingMin = pr.remainingMin;\n  }\n}\n\nvoid drawBase() {'
new = '''    cache.remainingMin = pr.remainingMin;
  }

  // AMS filament display - 4 slots on left and right sides between card rows
  {
    uint32_t _amsHash = 0;
    for (int i = 0; i < 4; i++) {
      if (pr.amsSlots[i].valid) {
        _amsHash ^= pr.amsSlots[i].trayColor;
        _amsHash ^= (uint32_t)((uint8_t)pr.amsSlots[i].remain) << (i * 8);
        _amsHash ^= manualStringHash(pr.amsSlots[i].trayType) << 16;
      }
    }
    if (_amsHash != cache.amsHash) {
      // Slot positions: 2 on left side, 2 on right side, in gaps between card rows
      const int cx[4] = {20, 220, 20, 220};
      const int cy[4] = {53, 53, 170, 170};
      const int tx[4] = {36, 204, 36, 204};
      const int ty[4] = {53, 53, 170, 170};
      const uint8_t datum[4] = {ML_DATUM, MR_DATUM, ML_DATUM, MR_DATUM};

      for (int i = 0; i < 4; i++) {
        const AmsTrayInfo& slot = pr.amsSlots[i];
        if (slot.valid) {
          uint16_t swatchColor = filamentColor(slot.trayColor);
          tft.fillCircle(cx[i], cy[i], 5, 0x2B6D);
          tft.fillCircle(cx[i], cy[i], 4, swatchColor);
          tft.drawCircle(cx[i], cy[i], 4, 0x294A);

          // Compact type label
          String label = slot.traySubBrands.length() ? slot.traySubBrands : slot.trayType;
          if (label.startsWith("PLA")) label = "PLA";
          else if (label.startsWith("ABS")) label = "ABS";
          else if (label.startsWith("PETG")) label = "PETG";
          else if (label.startsWith("TPU")) label = "TPU";
          else if (label.startsWith("PA")) label = "PA";
          else if (label.startsWith("PC")) label = "PC";
          else if (label.startsWith("ASA")) label = "ASA";
          else if (label.startsWith("PP")) label = "PP";
          else if (label.startsWith("PPS")) label = "PPS";
          else if (label.startsWith("HIPS")) label = "HIPS";
          else if (label.startsWith("BVOH")) label = "BVOH";
          else if (label.startsWith("PVA")) label = "PVA";
          else if (label.startsWith("PE")) label = "PE";
          else if (label.length() > 5) label = label.substring(0, 5);

          tft.setTextDatum(datum[i]);
          tft.setTextFont(1);
          tft.setTextColor(C_TEXT, 0x2B6D);
          tft.setTextPadding(0);
          tft.drawString(label, tx[i], ty[i]);
        } else {
          tft.fillCircle(cx[i], cy[i], 5, 0x2B6D);
          tft.drawCircle(cx[i], cy[i], 4, 0x294A);
        }
      }
      cache.amsHash = _amsHash;
    }
  }
}

void drawBase() {'''

if old in c:
    c = c.replace(old, new, 1)
    print('C9 AMS display added OK')
    changes += 1
else:
    print('C9 FAILED')

print(f'Changes: {changes}')
with open('M:/codex/1/src/main.cpp', 'w', encoding='utf-8', newline='\r\n') as f:
    f.write(c)
print('Saved')
