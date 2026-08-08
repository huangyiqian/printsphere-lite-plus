with open('M:/codex/1/src/main.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Remove the duplicate amsHash in drawDashboardBase
content = content.replace(
    '  cache.amsHash = 0;\n  cache.status = "";\n  cache.displayName = "";\n  cache.model = "";\n  cache.alias = "";\n  cache.layout = "dashboard";\n  cache.amsHash = 0;\n  cache.baseDrawn',
    '  cache.amsHash = 0;\n  cache.status = "";\n  cache.displayName = "";\n  cache.model = "";\n  cache.alias = "";\n  cache.layout = "dashboard";\n  cache.baseDrawn'
)

with open('M:/codex/1/src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
print('Done')
