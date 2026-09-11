[ 简体中文 ](README.md) | [ English ](README_en.md)

# PrintSphere Lite Plus (Enhanced Fork)

![Version](https://img.shields.io/badge/Firmware-v0.5.00-brightgreen)
![Backend Version](https://img.shields.io/badge/WebUI_Backend-v0.4.71--ui--clean-blue)
![License](https://img.shields.io/badge/License-Non--Commercial-orange)

This project is an enhanced and improved fork of the original [PrintSphere Lite](https://github.com/ccord34/printsphere-lite). Powered by ESP8266EX and a 240x240 ST7789 display, it serves as a mini desktop monitor for real-time printing status and AMS filament tracking for Bambu Lab 3D printers.

---

## 📌 Fork Features & Improvements

### 1. 🌈 AMS (Bambu Multi-Material System) & External Spool (Ext Spool) Support
* **Multi-color filament & external spool rendering**: Fully parses Bambu AMS slot status in real time, accurately recognizing filament color, material type, and loading status for each slot.
* **Adaptive layout without AMS**: Automatically detects whether an AMS unit is connected via `ams_exist_bits`. When no AMS is present, it cleanly hides the 3 redundant empty slots and displays the external spool as `ext` in the first position, keeping the layout compact and elegant.
* **Smart discrimination between Official & 3rd-Party filaments**:
  * Automatically validates RFID tag data (`tag_uid`).
  * **Official filaments** (with RFID): Accurately displays remaining filament percentage (e.g., `85%`).
  * **3rd-party / non-RFID filaments** (Generic/spool holder): Automatically hides percentage numbers, keeping only the color block and material abbreviation (e.g., `PLA`, `PETG`) to prevent misleading estimates.
* **BGR565 color correction**: Recalibrated RGB/BGR565 color mapping specifically for the ST7789 display panel, delivering vibrant and realistic filament colors.
* **Intelligent screen cleanup**: Automatically clears AMS indicators and print speeds when a job finishes, is cancelled, or enters idle state, seamlessly switching back to standby/clock view.

### 2. 📊 Screen UI & Layout Optimization
* **Redesigned Dashboard UI**: Features a 2x2 data card grid showing print progress, nozzle temperature, bed temperature, chamber temperature, and estimated remaining time simultaneously.
* **Dual-nozzle & multi-model compatibility**: Optimized for various models (A1/A1 mini, P1P/P1S, P2S, X1C, X2D, H2 series, etc.), ensuring temperatures and dual-nozzle data are displayed cleanly without text truncation or overlap.
* **PWM backlight control**: Supports 0-100% free slider brightness adjustment, with an automatic low-power dimming standby mode after 5 minutes of inactivity.

### 3. 🌐 WebUI Backend Configuration Tool Upgrades
* **Clean & compact interface**: Streamlined web configuration tool with fast response and reduced whitespace, grouped logically into Wi-Fi scanning, printer switching, and screen layout options.
* **Multi-device profile management**: Supports managing multiple ESP devices under a single Bambu cloud account, isolating settings per hardware by MAC address and Chip ID.
* **USB Serial priority**: All configuration pushes prioritize USB Serial connection with HTTP LAN as a fallback, preventing cross-device misconfigurations on the local network.

### 4. 🧹 Repository Optimization & Privacy Sanitization
* **Sensitive privacy protection**: Configured `.gitignore` rules to exclude Wi-Fi credentials, Bambu Cloud tokens, and printer access codes (`config.json`), safeguarding private data.
* **Local backup isolation**: Keeps local development backups (`*.bak`) on your machine while preventing accidental cloud pushes.
* **Lightweight repository**: Removed bulky redundant binaries and temporary debug scripts for lightning-fast cloning and updates.

### 5. ⚡ ESP Built-in Web (:8081) & Long-term Stability Hardening (v0.5.00)
* **Zero dynamic heap overhead streaming**: Refactored the built-in web management page to stream HTML chunks directly from PROGMEM, dropping the dynamic heap allocation peak to 0 bytes and eliminating memory fragmentation and OOM crashes.
* **Port zombie self-healing**: Addressed the silent lwIP `_listen_pcb` release bug (port unresponsive while flagged as started). Added `isEspServerListening()` active status validation to automatically re-bind and listen if any anomaly occurs.
* **Wi-Fi jitter protection**: Removed destructive socket close calls triggered by transient Wi-Fi beacon losses or packet drops, preventing exhaustion of lwIP TCP PCBs.
* **Speculative connection handling**: Modern browser speculative pre-connections (0 bytes sent) are discarded within 100ms, and request line timeout is tightened from 2000ms to 600ms to avoid blocking the main loop or MQTT packets.
* **Smart idle polling**: WebUI polling interval extended to 8 seconds and coupled with `!document.hidden` visibility checks, completely halting requests when the browser tab is backgrounded or screen locked to prevent `TIME_WAIT` socket buildup.

---

## 🖼️ Interface & Hardware Previews

| Dashboard Layout (Dashboard + AMS) | Clock Standby Layout (Clock) |
| :---: | :---: |
| <img src="docs/images/dashboard-layout.jpg" width="340" /> | <img src="docs/images/clock-layout.jpg" width="340" /> |

### Web Backend Configuration Interface
<p align="center">
  <img src="docs/images/web-ui-preview.png" width="680" />
</p>

---

## 🏷️ Version Information

* **Firmware Version**: `v0.5.00`
* **Backend WebUI**: `v0.4.71-ui-clean`

---

## 📁 Directory Structure

```text
src/              ESP8266 firmware core C++ source code (main.cpp, config.h)
include/          TFT_eSPI display driver pin configuration
后端配置工具/     Windows Web configuration tool (server.js, 打开配置工具.bat)
固件/             Precompiled printsphere-lite-esp8266.bin
刷固件工具/       Windows one-click flashing tool and USB serial drivers
docs/             Documentation and assets
platformio.ini    PlatformIO project build configuration
build-release.ps1 Release package packaging script
```

---

## 🛠️ Hardware Requirements & Pinout

* **MCU**: ESP8266EX / NodeMCU compatible board
* **Display**: 240x240 7-pin ST7789 SPI LCD Screen
* **Enclosure**: 外壳模型可选择https://makerworld.com.cn/zh/models/2587841-cheng-ben-25-printsphere-litetuo-zhu-da-yin-zhuang#profileId-2978954
* **Pin Connections (PlatformIO Default)**:
  * `CS`: GPIO 15
  * `DC`: GPIO 0
  * `RST`: GPIO 2
  * `BL`: GPIO 5 (PWM Backlight Control)

---

## 🚀 Quick Start

1. Connect ESP8266 to your Windows PC using a USB data cable.
2. Open `后端配置工具\打开配置工具.bat`, which opens the WebUI in your default browser, and log in to your Bambu Lab account.
3. Select or enter your 2.4G Wi-Fi SSID and password, then click **“Save & Configure ESP WiFi”**.
4. Refresh printer list, select your target Bambu printer, and click **“Show This Printer & Sync”**.
5. Once print data appears on the ESP8266 screen, you can unplug the device from your computer and power it via any 5V USB source.
6. Once connected to Wi-Fi, you can also manage the device directly in your browser via `http://[ESP_IP_ADDRESS]:8081/`.

---

## 💻 Build & Compile

This project is built using [PlatformIO](https://platformio.org/):

```bash
# Build ESP8266 firmware
platformio run -e sd2
```

Compiled binary output: `.pio/build/sd2/firmware.bin`

---

## 📄 License & Acknowledgements

* Based on the original project [ccord34/printsphere-lite](https://github.com/ccord34/printsphere-lite).
* Intended for personal learning, hobbyist, and non-commercial use only. Commercial use, mass production, or integration into paid services requires authorization from the original author. See [LICENSE](LICENSE) for details.
