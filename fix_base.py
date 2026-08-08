with open('M:/codex/1/src/main.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Add amsHash back to drawBase
content = content.replace(
    '  cache.totalLayers = -999;\n  cache.status = "";\n  cache.displayName = "";',
    '  cache.totalLayers = -999;\n  cache.amsHash = 0;\n  cache.status = "";\n  cache.displayName = "";'
)

with open('M:/codex/1/src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
print('Done')
