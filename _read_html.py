lines = open("M:/codex/1/src/main.cpp", "r", encoding="utf-8").readlines()

# === Find espHomeHtml function ===
start = None
end = None
for i, line in enumerate(lines):
    if "String espHomeHtml()" in line:
        start = i
    if start and i > start and "return body;" in line:
        end = i
        break

print(f"espHomeHtml: lines {start+1} to {end+1}")

# Print the first 10 lines to see structure
for i in range(start, min(start+10, end+1)):
    print(f'  {i+1}: {lines[i].rstrip()}')

# Print the last 10 lines
for i in range(max(start, end-9), end+1):
    print(f'  {i+1}: {lines[i].rstrip()}')