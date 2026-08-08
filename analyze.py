import re

with open('M:/codex/1/src/main.cpp', 'r', encoding='utf-8') as f:
    content = f.read()
    lines = content.split('\n')

# Find key positions
positions = {}
for i, line in enumerate(lines):
    if 'struct PrinterState {' in line:
        positions['printer_state'] = i
    if 'struct RenderCache {' in line:
        positions['render_cache'] = i
    if 'void resetLivePrintFields() {' in line:
        positions['reset_live'] = i
    if 'void applyPrint(JsonObject print)' in line or 'void applyPrint(JsonObject print) {' in line:
        positions['apply_print'] = i
    if 'void parseMqttPayload(uint8_t* payload, size_t length) {' in line:
        positions['parse_mqtt'] = i
    if 'void drawDashboardFields() {' in line:
        positions['draw_dash_fields'] = i
    if 'void drawBase() {' in line:
        positions['draw_base'] = i

for name, pos in sorted(positions.items(), key=lambda x: x[1]):
    print(f'{name}: line {pos+1}')

# Find RenderCache ending
if 'render_cache' in positions:
    i = positions['render_cache']
    for j in range(i+1, len(lines)):
        if lines[j].strip() == '};':
            print(f'RenderCache ends at line {j+1}')
            # Show the last few fields (around j-4 to j)
            for k in range(max(i+1, j-4), j+1):
                print(f'  {k+1}: {lines[k].rstrip()}')
            break

# Find resetLivePrintFields ending  
if 'reset_live' in positions:
    i = positions['reset_live']
    depth = 1  # function body
    close_line = None
    for j in range(i+1, len(lines)):
        bc = lines[j].count('{') - lines[j].count('}')
        depth += bc
        if depth == 0:
            close_line = j
            break
        # Also check for standalone }
        if lines[j].strip() == '}' and depth == 1:
            close_line = j
            break
    
    if close_line:
        print(f'resetLivePrintFields ends at line {close_line+1}')
        # Show the last few lines
        for k in range(max(i+1, close_line-5), close_line+1):
            print(f'  {k+1}: {lines[k].rstrip()}')

# Find drawDashboardFields ending
if 'draw_dash_fields' in positions:
    i = positions['draw_dash_fields']
    depth = 0
    started = False
    close_line = None
    for j in range(i, len(lines)):
        bc = lines[j].count('{') - lines[j].count('}')
        depth += bc
        if not started:
            started = True
        elif depth == 0:
            close_line = j
            break
    
    if close_line:
        print(f'drawDashboardFields ends at line {close_line+1}')
        # Show last 5 lines
        for k in range(max(i+1, close_line-5), close_line+1):
            print(f'  {k+1}: {lines[k].rstrip()}')

# Show applyPrint ending
if 'apply_print' in positions:
    i = positions['apply_print']
    depth = 1
    close_line = None
    for j in range(i+1, len(lines)):
        bc = lines[j].count('{') - lines[j].count('}')
        depth += bc
        if depth == 0:
            close_line = j
            break
    
    if close_line:
        print(f'applyPrint ends at line {close_line+1}')

# Show parseMqttPayload ending
if 'parse_mqtt' in positions:
    i = positions['parse_mqtt']
    depth = 1
    close_line = None
    for j in range(i+1, len(lines)):
        bc = lines[j].count('{') - lines[j].count('}')
        depth += bc
        if depth == 0:
            close_line = j
            break
    
    if close_line:
        print(f'parseMqttPayload ends at line {close_line+1}')

# drawBase ending
if 'draw_base' in positions:
    i = positions['draw_base']
    depth = 1
    close_line = None
    for j in range(i+1, len(lines)):
        bc = lines[j].count('{') - lines[j].count('}')
        depth += bc
        if depth == 0:
            close_line = j
            break
    
    if close_line:
        print(f'drawBase ends at line {close_line+1}')
