# Changelog

Tutte le modifiche rilevanti a questo progetto saranno documentate in questo file secondo il formato [Keep a Changelog](https://keepachangelog.com/it/1.1.0/) e il [Semantic Versioning](https://semver.org/lang/it/).

## [1.0.0] - 2026-11-18

### Added
- Configurazione PlatformIO (`platformio.ini`) per scheda generica `esp32-s3-devkitc-1` con partizionamento Dual-OTA (`default_8MB.csv`) e `flash_mode = dio`.
- Firmware base ESP32-S3 (`src/main.cpp`) per **ESP32-S3 OTA Web Flasher** con output seriale a `115200` baud, Access Point Wi-Fi `ESP-Setup` (`admin1234`), supporto mDNS (`esp-setup.local`) e Web Server HTTP su porta `80` con interfaccia OTA integrata (`/` e `/update`).
- Endpoint diagnostico JSON `/api/info` e messa in sicurezza hardware pre-flash (`setSafeHardwareState()`).
- Script post-build `copy_firmware.py` per copiare automaticamente `bootloader.bin`, `partitions.bin`, `boot_app0.bin` e `firmware.bin` in `docs/firmware/`.
- Sito statico per GitHub Pages in `docs/` (`index.html`, `manifest.json`, `css/style.css`, `js/app.js`, `.nojekyll`) configurato con `esp-web-tools` via CDN e offset ESP32-S3 esatti.
- Documentazione di progetto (`README.md`, `CHANGELOG.md`, `.gitignore`).
