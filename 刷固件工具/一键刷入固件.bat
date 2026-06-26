@echo off
setlocal
chcp 65001 >nul
cd /d "%~dp0"

set "PS_SCRIPT=%~dp0flash-firmware.ps1"

if not exist "%PS_SCRIPT%" (
  echo Flasher script not found: %PS_SCRIPT%
  pause
  exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -STA -File "%PS_SCRIPT%"
pause
