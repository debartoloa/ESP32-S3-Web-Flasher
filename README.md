# ESP32-S3 OTA Web Flasher

Progetto completo per **ESP32-S3** composto da:
1. **Firmware base PlatformIO / Arduino** con Access Point Wi-Fi (`ESP-Setup`), supporto **mDNS** (`esp-setup.local`), tabella partizioni **Dual-OTA** (`default_8MB.csv`) e Web Server HTTP integrato per aggiornamenti firmware **Over-The-Air (OTA)** tramite file `.bin`.
2. **Sito web statico pronto per GitHub Pages (`docs/`)** basato su **ESP Web Tools** e **Web Serial API** per flashare la scheda direttamente dal browser via USB.
3. **Script post-build (`copy_firmware.py`)** che esporta automaticamente tutti i binari necessari in `docs/firmware/` al termine di ogni compilazione.

---

## Struttura del Progetto

```text
OTA-ESP32/
├── platformio.ini              # Configurazione PlatformIO ESP32-S3 + Dual-OTA + extra_scripts
├── copy_firmware.py            # Script post-build per copiare i 4 binari in docs/firmware/
├── src/
│   └── main.cpp                # Firmware ESP32-S3 (Serial, Wi-Fi AP, mDNS, WebServer + Update OTA)
├── docs/                       # Cartella pubblicabile direttamente su GitHub Pages (/docs)
│   ├── .nojekyll               # Disabilita Jekyll su GitHub Pages
│   ├── index.html              # Web Flasher UI con esp-web-install-button
│   ├── manifest.json           # Manifest ESP Web Tools per ESP32-S3 (offset decimali esatti)
│   ├── css/style.css           # Stile Dark Mode responsive
│   ├── js/app.js               # Controllo Web Serial API e pulsanti copia rapida
│   └── firmware/               # Generata automaticamente da copy_firmware.py
│       ├── bootloader.bin      # Offset 0x0000  (0)
│       ├── partitions.bin      # Offset 0x8000  (32768)
│       ├── boot_app0.bin       # Offset 0xE000  (57344)
│       └── firmware.bin        # Offset 0x10000 (65536)
├── README.md
├── CHANGELOG.md
└── .gitignore
```

---

## Requisiti & Compilazione Firmware

1. Apri il progetto con **PlatformIO** (VS Code o CLI).
2. Compila il firmware:
   ```bash
   pio run
   ```
3. Al termine della compilazione, `copy_firmware.py` copierà automaticamente `bootloader.bin`, `partitions.bin`, `boot_app0.bin` e `firmware.bin` nella cartella `docs/firmware/`.
4. Puoi eseguire lo script anche manualmente in qualsiasi momento:
   ```bash
   python copy_firmware.py
   ```

---

## Primo Flash via Browser (GitHub Pages + Web Serial)

1. Effettua il push del repository su GitHub (assicurandoti che `docs/firmware/*.bin` sia incluso nel commit).
2. Nelle impostazioni del repository GitHub: **Settings → Pages → Source: Deploy from a branch → Branch `main` / Folder `/docs`**.
3. Apri l'URL GitHub Pages con **Google Chrome**, **Microsoft Edge** o **Opera** su desktop.
4. Collega l'ESP32-S3 via USB e clicca **Installa Firmware** (se necessario entra in modalità Bootloader tenendo premuto `BOOT`, premendo e rilasciando `RESET`, quindi rilasciando `BOOT`).

---

## Utilizzo del Firmware & Aggiornamenti OTA

All'avvio:
- Sulla porta seriale a `115200 baud` vengono stampati il messaggio di benvenuto, le credenziali Wi-Fi e l'indirizzo IP dell'Access Point.
- La scheda crea la rete Wi-Fi:
  - **SSID:** `ESP-Setup`
  - **Password:** `admin1234`
- Collegandoti a `ESP-Setup` da PC o smartphone e visitando **`http://192.168.4.1/`** (o `http://esp-setup.local/`), puoi selezionare un nuovo file `.bin` compilato (es. `.pio/build/esp32s3/firmware.bin`), seguire l'avanzamento in percentuale e attendere il riavvio automatico della scheda.
