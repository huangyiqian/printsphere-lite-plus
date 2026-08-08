with open('M:/codex/1/src/main.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Remove the duplicate amsHash=0 in drawDashboardBase (the one after totalLayers)
content = content.replace(
    '  cache.totalLayers = -999;\n  cache.amsHash = 0;\n  cache.status = "";\n  cache.displayName = "";',
    '  cache.totalLayers = -999;\n  cache.status = "";\n  cache.displayName = "";'
)

with open('M:/codex/1/src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
print('Done')
