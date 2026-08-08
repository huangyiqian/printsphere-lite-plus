with open('M:/codex/1/src/main.cpp','r',encoding='utf-8') as f:
    c = f.read()
checks = [
    ('struct AmsTrayInfo', 'AmsTrayInfo struct'),
    ('AmsTrayInfo amsSlots[4]', 'amsSlots[4] in PrinterState'),
    ('uint32_t amsHash = 0', 'amsHash in RenderCache'),
    ('pr.amsSlots[i].valid = false', 'AMS reset in resetLivePrintFields'),
    ('filter["print"]["ams"]["ams"][0]', 'AMS filter in parseMqttPayload'),
    ('Parse AMS tray data', 'AMS parsing in applyPrint'),
    ('manualStringHash', 'manualStringHash function'),
    ('filamentColor', 'filamentColor function'),
    ('cache.amsHash = 0', 'amsHash reset'),
    ('AMS filament display', 'AMS display in drawDashboardFields'),
]
for pattern, name in checks:
    found = pattern in c
    status = 'OK' if found else 'MISSING'
    print(f'{status}: {name}')
