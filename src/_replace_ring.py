lines = open('M:/codex/1/src/main.cpp', 'r', encoding='utf-8').readlines()

# ===== 1. drawDashboardBase - replace ring with bar bg =====
for i, line in enumerate(lines):
    if i >= 2029 and i <= 2034:
        print(f'{i+1}: {line.rstrip()}')
