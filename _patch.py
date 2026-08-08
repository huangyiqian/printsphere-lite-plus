import re

filepath = "M:/codex/1/src/main.cpp"
with open(filepath, "r", encoding="utf-8") as f:
    content = f.read()

# 1. Remove brightnessSchedule field from StoredConfig struct
content = content.replace('  String brightnessSchedule = "";\n', '')

# 2. Remove applyBrightnessSchedule function  
pattern1 = r'void applyBrightnessSchedule\(\) \{[^}]*\n\}'
content = re.sub(pattern1, '', content, count=1)

# 3. Remove brightness schedule check block in main loop
pattern2 = r'  // Brightness schedule check \(every 60s\)\n  static unsigned long lastBrsCheck = 0;\n  if \(now - lastBrsCheck >= 60000\) \{\n    lastBrsCheck = now;\n    applyBrightnessSchedule\(\);\n  \}\n'
content = re.sub(pattern2, '', content, count=1)

# 4. Remove brightness_schedule handling in applyConfigBody
pattern3 = r'  // .*?\n  if \(doc\.containsKey\("brightness_schedule"\)\) \{\n    stored\.brightnessSchedule = doc\["brightness_schedule"\]\.as<String>\(\);\n  \}\n'
content = re.sub(pattern3, '', content, count=1)

# Try another version without comment
pattern3b = r'  if \(doc\.containsKey\("brightness_schedule"\)\) \{\n    stored\.brightnessSchedule = doc\["brightness_schedule"\]\.as<String>\(\);\n  \}\n'
content = re.sub(pattern3b, '', content, count=1)

# 5. Replace espHomeHtml function
old_func_match = re.search(r'String espHomeHtml\(\) \{(?:[^{}]|\{(?:[^{}]|\{[^{}]*\})*\})*?\n  return body;\n\}', content, re.DOTALL)
if old_func_match:
    # Need a more reliable way - find function by brace counting
    pass

# Find espHomeHtml function by brace counting
func_start = content.find('String espHomeHtml() {')
if func_start >= 0:
    brace_count = 0
    i = func_start
    while i < len(content):
        if content[i] == '{':
            brace_count += 1
        elif content[i] == '}':
            brace_count -= 1
            if brace_count == 0:
                func_end = i + 1
                break
        i += 1
    
    old_func = content[func_start:func_end]
    
    new_func = '''String espHomeHtml() {\\n  String ip = WiFi.localIP().toString();\\n  String selected = stored.name.length() ? stored.name : stored.serial;\\n  String modelName = normalizedModelName(stored.model);\\n\\n  String body;\\n  body.reserve(5000);\\n  body += F(\\\"<!doctype html><html lang=\\\\\\\"zh-CN\\\\\\\"><head><meta charset=\\\\\\\"utf-8\\\\\\\"><meta name=\\\\\\\"viewport\\\\\\\" content=\\\\\\\"width=device-width,initial-scale=1\\\\\\\">\\\");\\n  body += F(\\\"<title>PrintSphere Lite</title><style>\\\");\\n  body += F(\\\"*{box-sizing:border-box;margin:0;padding:0}body{min-height:100vh;background:linear-gradient(135deg,#0f0c29,#302b63,#24243e);font-family:'Segoe UI',-apple-system,sans-serif;padding:20px;color:#fff}\\\");\\n  body += F(\\\".container{max-width:960px;margin:0 auto}\\\");\\n  body += F(\\\"h1{font-size:24px;font-weight:600;margin-bottom:4px;background:linear-gradient(135deg,#667eea,#764ba2);-webkit-background-clip:text;-webkit-text-fill-color:transparent}\\\");\\n  body += F(\\\".sub{color:rgba(255,255,255,0.6);font-size:13px;margin-bottom:20px}\\\");\\n  body += F(\\\".cards{display:grid;grid-template-columns:1fr;gap:24px}@media(min-width:600px){.cards{grid-template-columns:repeat(2,1fr)}}@media(min-width:960px){.cards{grid-template-columns:repeat(3,1fr)}}\\\");\\n  body += F(\\\".card{background:rgba(255,255,255,0.06);backdrop-filter:blur(12px);-webkit-backdrop-filter:blur(12px);border:1px solid rgba(255,255,255,0.12);border-radius:16px;padding:20px;box-shadow:0 8px 32px rgba(0,0,0,0.3)}\\\");\\n  body += F(\\\".card h2{font-size:14px;font-weight:500;margin-bottom:16px;color:rgba(255,255,255,0.7);text-transform:uppercase;letter-spacing:1px}\\\");\\n  body += F(\\\".row{display:flex;justify-content:space-between;align-items:center;padding:6px 0}.label{color:rgba(255,255,255,0.5);font-size:13px}.value{color:#fff;font-size:14px;font-weight:500}\\\");\\n  body += F(\\\"input[type=range]{width:100%;margin:12px 0;height:4px;-webkit-appearance:none;background:rgba(255,255,255,0.15);border-radius:2px;outline:none}input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:20px;height:20px;border-radius:50%;background:linear-gradient(135deg,#667eea,#764ba2);cursor:pointer;box-shadow:0 2px 8px rgba(102,126,234,0.4)}\\\");\\n  body += F(\\\".brightness-val{font-size:28px;font-weight:700;text-align:center;margin:8px 0 12px;background:linear-gradient(135deg,#667eea,#764ba2);-webkit-background-clip:text;-webkit-text-fill-color:transparent}\\\");\\n  body += F(\\\".layout-opt{display:flex;gap:10px;margin-top:12px}.layout-opt button{flex:1;padding:12px;border:1px solid rgba(255,255,255,0.15);border-radius:10px;background:rgba(255,255,255,0.05);color:rgba(255,255,255,0.7);font-size:13px;cursor:pointer;text-align:center;transition:all .3s ease;font-weight:500}.layout-opt button.active{background:linear-gradient(135deg,#667eea,#764ba2);border-color:transparent;color:#fff;box-shadow:0 4px 15px rgba(102,126,234,0.3)}.layout-opt button:hover:not(.active){background:rgba(255,255,255,0.1)}\\\");\\n  body += F(\\\"pre{background:rgba(0,0,0,0.3);border:1px solid rgba(255,255,255,0.1);border-radius:12px;padding:16px;font-size:12px;color:rgba(255,255,255,0.7);overflow-x:auto;white-space:pre-wrap;min-height:60px;margin-top:12px;display:none;font-family:'Cascadia Code','Fira Code',monospace}\\\");\\n  body += F(\\\".badge{display:inline-block;padding:3px 10px;border-radius:12px;font-size:11px;font-weight:500}.badge.ok{background:rgba(102,126,234,0.2);color:#667eea;border:1px solid rgba(102,126,234,0.3)}.badge.err{background:rgba(255,77,77,0.2);color:#ff4d4d;border:1px solid rgba(255,77,77,0.3)}\\\");\\n  body += F(\\\".toggle-btn{background:rgba(255,255,255,0.06);border:1px solid rgba(255,255,255,0.12);color:rgba(255,255,255,0.7);padding:8px 16px;border-radius:8px;cursor:pointer;font-size:12px;transition:all .3s ease}.toggle-btn:hover{background:rgba(255,255,255,0.12)}\\\");\\n  body += F(\\\"</style></head><body><div class=\\\\\\\"container\\\\\\\">\\\");\\n\\n  body += F(\\\"<h1>PrintSphere Lite</h1><p class=\\\\\\\"sub\\\\\\\">固件: \\\"); body += FIRMWARE_VERSION; body += F(\\\" | IP: \\\"); body += htmlEscape(ip); body += F(\\\"</p>\\\");\\n  body += F(\\\"<div class=\\\\\\\"cards\\\\\\\"><div class=\\\\\\\"card\\\\\\\"><h2>设备状态</h2>\\\");\\n  body += F(\\\"<div class=\\\\\\\"row\\\\\\\"><span class=\\\\\\\"label\\\\\\\">打印机</span><span class=\\\\\\\"value\\\\\\\">\\\"); body += htmlEscape(selected.length() ? selected : String(\\\"未选择\\\")); body += F(\\\"</span></div>\\\");\\n  body += F(\\\"<div class=\\\\\\\"row\\\\\\\"><span class=\\\\\\\"label\\\\\\\">机型</span><span class=\\\\\\\"value\\\\\\\">\\\"); body += htmlEscape(modelName); body += F(\\\"</span></div>\\\");\\n  body += F(\\\"<div class=\\\\\\\"row\\\\\\\"><span class=\\\\\\\"label\\\\\\\">MQTT</span><span class=\\\\\\\"value\\\\\\\"><span class=\\\\\\\"badge \\\"); body += mqttNet.connected() ? F(\\\"ok\\\") : F(\\\"err\\\"); body += F(\\\"\\\\\\\">\\\"); body += mqttNet.connected() ? F(\\\"已连接\\\") : F(\\\"未连接\\\"); body += F(\\\"</span></span></div></div>\\\");\\n\\n  body += F(\\\"<div class=\\\\\\\"card\\\\\\\"><h2>屏幕亮度</h2><div class=\\\\\\\"brightness-val\\\\\\\" id=\\\\\\\"bv\\\\\\\">\\\"); body += String(stored.brightness); body += F(\\\"</div>\\\");\\n  body += F(\\\"<input type=\\\\\\\"range\\\\\\\" id=\\\\\\\"br\\\\\\\" min=\\\\\\\"0\\\\\\\" max=\\\\\\\"100\\\\\\\" value=\\\\\\\"\\\"); body += String(stored.brightness); body += F(\\\"\\\\\\\">\\\");\\n  body += F(\\\"<p style=\\\\\\\"color:rgba(255,255,255,0.4);font-size:12px;text-align:center;margin-top:4px\\\\\\\">松开滑块后自动生效</p></div>\\\");\\n\\n  body += F(\\\"<div class=\\\\\\\"card\\\\\\\"><h2>屏幕布局</h2><div class=\\\\\\\"layout-opt\\\\\\\">\\\");\\n  String lc = stored.layout;\\n  body += F(\\\"<button id=\\\\\\\"lc0\\\\\\\" onclick=\\\\\\\"setLayout('classic')\\\\\\\"\\\"); if (lc == \\\"classic\\\") body += F(\\\" class=\\\\\\\"active\\\\\\\"\\\"); body += F(\\\">经典</button>\\\");\\n  body += F(\\\"<button id=\\\\\\\"lc1\\\\\\\" onclick=\\\\\\\"setLayout('dashboard')\\\\\\\"\\\"); if (lc == \\\"dashboard\\\") body += F(\\\" class=\\\\\\\"active\\\\\\\"\\\"); body += F(\\\">信息面板</button>\\\");\\n  body += F(\\\"<button id=\\\\\\\"lc2\\\\\\\" onclick=\\\\\\\"setLayout('clock')\\\\\\\"\\\"); if (lc == \\\"clock\\\") body += F(\\\" class=\\\\\\\"active\\\\\\\"\\\"); body += F(\\\">时钟</button>\\\");\\n  body += F(\\\"</div></div>\\\");\\n\\n  body += F(\\\"<div class=\\\\\\\"card\\\\\\\"><h2>实时数据</h2><button class=\\\\\\\"toggle-btn\\\\\\\" onclick=\\\\\\\"toggleJson(this)\\\\\\\">查看状态 JSON</button><pre id=\\\\\\\"log\\\\\\\"></pre></div>\\\");\\n\\n  body += F(\\\"<script>\\\\n\\\");\\n  body += F(\\\"function refreshStatus(){fetch('/api/status',{cache:'no-store'}).then(r=>r.text()).then(t=>{let l=document.getElementById('log');if(l)l.textContent=t;}).catch(e=>{});}\\\\n\\\");\\n  body += F(\\\"function toggleJson(btn){const el=document.getElementById('log');if(el.style.display==='none'||!el.style.display){el.style.display='block';refreshStatus();btn.textContent='隐藏状态 JSON'}else{el.style.display='none';btn.textContent='查看状态 JSON'}}\\\\n\\\");\\n  body += F(\\\"function postConfig(data){fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)}).then(()=>refreshStatus()).catch(e=>console.error(e));}\\\\n\\\");\\n  body += F(\\\"let brt=document.getElementById('br');if(brt){brt.addEventListener('change',function(){postConfig({brightness:parseInt(this.value)});});brt.addEventListener('input',function(){document.getElementById('bv').textContent=this.value;});}\\\\n\\\");\\n  body += F(\\\"function setLayout(v){document.querySelectorAll('.layout-opt button').forEach(b=>b.classList.remove('active'));const idx=['classic','dashboard','clock'].indexOf(v);if(idx>=0)document.getElementById('lc'+idx).classList.add('active');postConfig({layout:v});}\\\\n\\\");\\n  body += F(\\\"</script></div></body></html>\\\");\\n\\n  return body;\\n}'''
    
    content = content[:func_start] + new_func + content[func_end:]
    print("Replaced espHomeHtml function")
else:
    print("ERROR: Could not find espHomeHtml function")

with open(filepath, "w", encoding="utf-8") as f:
    f.write(content)

print("Done!")
