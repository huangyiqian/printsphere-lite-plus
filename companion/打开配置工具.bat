@echo off
setlocal
cd /d "%~dp0"

set "NODE_EXE=%~dp0node\node.exe"
if not exist "%NODE_EXE%" set "NODE_EXE=node"
set "STATE_FILE=%~dp0data\server-state.json"

"%NODE_EXE%" -v >nul 2>nul
if errorlevel 1 (
  echo Node.js cannot be started.
  echo.
  echo Please keep companion\node\node.exe in this folder.
  echo If antivirus removed it, unzip the full tool package again.
  pause
  exit /b 1
)

echo Starting PrintSphere Lite setup tool...
echo.
if exist "%STATE_FILE%" del "%STATE_FILE%" >nul 2>nul

start "PrintSphere Lite Backend" /min "%NODE_EXE%" "%~dp0server.js" 8795

set "CONFIG_URL="
for /l %%i in (1,1,40) do (
  if exist "%STATE_FILE%" (
    for /f "usebackq delims=" %%u in (`powershell -NoProfile -ExecutionPolicy Bypass -Command "$s=Get-Content -Raw '%STATE_FILE%' | ConvertFrom-Json; $s.urls[0]"`) do set "CONFIG_URL=%%u"
    if defined CONFIG_URL goto open_tool
  )
  timeout /t 1 /nobreak >nul
)

echo Setup tool startup timed out.
echo Please check whether security software blocked node.exe.
pause
exit /b 1

:open_tool
echo Setup page: %CONFIG_URL%
echo.
start "" "%CONFIG_URL%"
echo Keep the backend window open while configuring the ESP.
echo After the ESP screen updates normally, you may close this tool.
echo.
pause
