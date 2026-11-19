@echo off
rem i2r simulator: double-click to start. Close this window to stop.
chcp 65001 >nul
set PYTHONIOENCODING=utf-8
cd /d "%~dp0"

where python >nul 2>nul
if errorlevel 1 (
  echo.
  echo [i2r] Python is not installed.
  echo       Install it from https://www.python.org/downloads/  ^(check "Add python.exe to PATH"^)
  echo       Or ask Claude: "start the simulator" - Claude installs it for you.
  echo.
  pause
  exit /b 1
)

python -c "import paho.mqtt" >nul 2>nul
if errorlevel 1 (
  echo [i2r] Installing paho-mqtt...
  python -m pip install --user "paho-mqtt>=2.0"
)

python sim.py %*
pause
