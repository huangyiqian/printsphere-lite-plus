# -*- coding: latin-1 -*-
import sys

with open('src/main.cpp', 'r', encoding='latin-1') as f:
    content = f.read()

old_loop = '''  bool noActivePrint = !pr.online || !(isPrintingState(pr.status) || isPreparingState(pr.status) || isPausedState(pr.status));

  if (noActivePrint && now - lastDisplay >= 1000) {
    displayDirty = true;
  }'''

new_loop = '''  bool noActivePrint = !pr.online || !(isPrintingState(pr.status) || isPreparingState(pr.status) || isPausedState(pr.status));
  bool isClockMode = stored.layout == "clock";

  if ((noActivePrint || isClockMode) && now - lastDisplay >= 1000) {
    displayDirty = true;
  }'''

if old_loop in content:
    content = content.replace(old_loop, new_loop, 1)
    with open('src/main.cpp', 'w', encoding='latin-1') as f:
        f.write(content)
    print('SUCCESS: loop updated')
else:
    print('ERROR: Could not find old loop code')
    idx = content.find('noActivePrint')
    if idx >= 0:
        print('Context:', content[idx:idx+250])