import sys
sys.stdout.reconfigure(encoding='utf-8')

filepath = "M:/codex/1/src/main.cpp"
with open(filepath, "r", encoding="utf-8") as f:
    content = f.read()

# Change '%02d:%02d' to '%02d %02d' in the clock function  
# Only replace the one in drawClockScreen, not other places
old = 'snprintf(timeStr, sizeof(timeStr), "%02d:%02d", h, m);'
new = 'snprintf(timeStr, sizeof(timeStr), "%02d %02d", h, m);'

if old in content:
    content = content.replace(old, new)
    with open(filepath, "w", encoding="utf-8") as f:
        f.write(content)
    print("Fixed: changed colon to space in clock time string")
else:
    print("Could not find the target string")
