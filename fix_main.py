import re

with open('M:/codex/1/src/main.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Fix 1: single-quote to double-quote for JSON object keys
content = content.replace("slot['sh']", 'slot["sh"]')
content = content.replace("slot['sm']", 'slot["sm"]')
content = content.replace("slot['eh']", 'slot["eh"]')
content = content.replace("slot['em']", 'slot["em"]')
content = content.replace("slot['b']", 'slot["b"]')
print('Fix 1 done: single quotes -> double quotes')

# Fix 2 & 3: broken F() strings with embedded newlines
# The F() macro needs the string to end on the same line
# We need to find and fix the broken patterns

# Pattern: line with two body += F() statements, second one broken by newline
# Fix 2: line 809-810 
lines = content.split('\n')
for i in range(len(lines)):
    # Find lines with two body += F() where the second F() string doesn't close
    line = lines[i]
    if 'body += F(' in line and line.count('body += F(') > 1:
        # Split into two statements at the boundary
        parts = line.split('body += F(')
        if len(parts) == 3:
            # First: part before first F(), then F() content between
            before = parts[0]
            first_f = parts[1]
            second_f = parts[2]
            
            # First F() should end with ");
            # Second F() starts with " but may be broken
            # Extract first F() content
            first_close = first_f.find('");')
            if first_close >= 0:
                first_f_content = first_f[:first_close]
                second_f_content = first_f[first_close+2:] + '\n' + lines[i+1] if i+1 < len(lines) else ''
                
                # The second F() starts with " and may not have been closed
                # We need to fix this
                rest = second_f_content
                
                # Check if the second F() string contains a newline
                if '\\n' in rest or ('\n' in rest):
                    # This is a broken F() - need to fix
                    # Extract what was meant to be in the F() string
                    # Remove the trailing ")
                    if rest.endswith('");'):
                        rest = rest[:-3]
                    
                    # Remove any trailing \n and )}; etc
                    rest = rest.strip()
                    
                    # Reconstruct as a proper single F() call
                    if 'brightness_schedule' in rest:
                        # This is the schedule sending - fix to valid JSON
                        if "':+" in rest or "'+JSON" in rest:
                            # x.send("brightness_schedule":'+JSON...')
                            # Fix to: x.send('{"brightness_schedule":"'+JSON...+'"}');
                            new_js = "x.send('{\"brightness_schedule\":\"'+JSON.stringify(slots)+'\"}')"
                            lines[i] = before + 'body += F("' + first_f_content + '");  body += F("' + new_js + '");'
                            lines[i+1] = ''  # clear the continuation line
                            print(f'Fix applied at line {i+1}: brightness_schedule send')
                        elif '":""' in rest:
                            # x.send("brightness_schedule":"")
                            # Fix to: x.send('{"brightness_schedule":""}')
                            new_js = "x.send('{\"brightness_schedule\":\"\"}')"
                            lines[i] = before + 'body += F("' + first_f_content + '");  body += F("' + new_js + '");'
                            lines[i+1] = ''  # clear the continuation line
                            print(f'Fix applied at line {i+1}: brightness_schedule empty send')

# Fix 4: }String -> }\\nString
content = '\n'.join(lines)
content = content.replace('}String applyConfigBody', '}\n\nString applyConfigBody')
print('Fix 4 done: added newline before applyConfigBody')

with open('M:/codex/1/src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
print('File written successfully')
