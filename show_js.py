import sys
with open("M:/codex/1/src/main.cpp","r",encoding="utf-8-sig")as f: lines = f.readlines()
for i in range(800,827): print(str(i) + ": " + lines[i].rstrip())
