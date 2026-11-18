/**
 * @file main.cpp
 * @brief Firmware base ESP32-S3 con Access Point Wi-Fi ("ESP-Setup") e Web Server OTA.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <ESPmDNS.h>

// Configurazione Access Point e rete
static constexpr const char *AP_SSID = "ESP-Setup";
static constexpr const char *AP_PASS = "admin1234"; // Minimo 8 caratteri per WPA2
static constexpr const char *MDNS_HOST = "esp-setup";
static constexpr const char *FW_VERSION = "1.0.0";
static constexpr uint16_t HTTP_PORT = 80;

WebServer server(HTTP_PORT);
static bool otaUploadOk = false;

/**
 * @brief Porta eventuali attuatori/GPIO in stato di riposo sicuro prima di scrivere in Flash.
 */
static void setSafeHardwareState() {
  // Firmware generico: inserire qui eventuali reset sicuri di relè, PWM o motori
  // prima di avviare Update.begin().
}

// Pagina HTML minimale, responsive e dark-mode servita su http://192.168.4.1/
static const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="it">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32-S3 OTA Web Flasher</title>
  <style>
    :root {
      --bg: #0b0f17;
      --card: #131a26;
      --border: #253045;
      --text: #e6edf7;
      --muted: #93a4bd;
      --accent: #3b82f6;
      --accent-hover: #2563eb;
      --ok: #10b981;
      --err: #ef4444;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      background: var(--bg);
      color: var(--text);
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 16px;
    }
    .card {
      width: 100%;
      max-width: 460px;
      background: var(--card);
      border: 1px solid var(--border);
      border-radius: 14px;
      padding: 24px;
      box-shadow: 0 16px 40px rgba(0, 0, 0, 0.45);
    }
    .badge {
      display: inline-block;
      font-size: 12px;
      font-weight: 600;
      color: #93c5fd;
      background: rgba(59, 130, 246, 0.14);
      border: 1px solid rgba(59, 130, 246, 0.35);
      padding: 4px 10px;
      border-radius: 999px;
      margin-bottom: 12px;
    }
    h1 { font-size: 22px; margin-bottom: 6px; }
    p.sub { color: var(--muted); font-size: 14px; line-height: 1.45; margin-bottom: 18px; }
    .meta {
      background: rgba(255, 255, 255, 0.03);
      border: 1px solid var(--border);
      border-radius: 10px;
      padding: 12px;
      margin-bottom: 18px;
      font-size: 13px;
      display: grid;
      gap: 6px;
    }
    .meta div { display: flex; justify-content: space-between; gap: 12px; }
    .meta span { color: var(--muted); }
    .meta code { color: var(--text); font-family: ui-monospace, monospace; }
    .file-box {
      display: block;
      border: 2px dashed var(--border);
      border-radius: 10px;
      padding: 18px 14px;
      text-align: center;
      cursor: pointer;
      transition: border-color 0.2s, background 0.2s;
      margin-bottom: 14px;
    }
    .file-box:hover {
      border-color: var(--accent);
      background: rgba(59, 130, 246, 0.06);
    }
    .file-box input { width: 100%; color: var(--muted); font-size: 13px; }
    button {
      width: 100%;
      padding: 12px 16px;
      border: none;
      border-radius: 10px;
      background: var(--accent);
      color: #fff;
      font-size: 15px;
      font-weight: 600;
      cursor: pointer;
      transition: background 0.2s, opacity 0.2s;
    }
    button:hover:not(:disabled) { background: var(--accent-hover); }
    button:disabled { opacity: 0.55; cursor: not-allowed; }
    .progress-wrap {
      margin-top: 16px;
      display: none;
    }
    .progress-bar-bg {
      width: 100%;
      height: 10px;
      background: #1d2738;
      border-radius: 999px;
      overflow: hidden;
      margin-bottom: 8px;
    }
    .progress-bar-fill {
      height: 100%;
      width: 0%;
      background: linear-gradient(90deg, #3b82f6, #10b981);
      transition: width 0.15s ease;
    }
    .status {
      margin-top: 12px;
      font-size: 13px;
      line-height: 1.45;
      color: var(--muted);
      min-height: 20px;
    }
    .status.ok { color: var(--ok); }
    .status.err { color: var(--err); }
  </style>
</head>
<body>
  <main class="card">
    <span class="badge" id="chip-badge">ESP32 &bull; OTA Ready</span>
    <h1>Aggiornamento Firmware</h1>
    <p class="sub">Seleziona il file <code>firmware.bin</code> compilato e caricalo via Wi-Fi sulla scheda.</p>

    <section class="meta">
      <div><span>Versione attuale</span><code id="fw-ver">--</code></div>
      <div><span>Access Point IP</span><code id="ap-ip">192.168.4.1</code></div>
      <div><span>Hostname mDNS</span><code>esp-setup.local</code></div>
    </section>

    <form id="ota-form">
      <label class="file-box">
        <input type="file" id="bin-file" name="update" accept=".bin" required>
      </label>
      <button type="submit" id="upload-btn">Avvia Aggiornamento OTA</button>
    </form>

    <div class="progress-wrap" id="progress-wrap">
      <div class="progress-bar-bg">
        <div class="progress-bar-fill" id="progress-fill"></div>
      </div>
    </div>
    <div class="status" id="status-msg">In attesa di un file .bin</div>
  </main>

  <script>
    (function () {
      const form = document.getElementById("ota-form");
      const fileInput = document.getElementById("bin-file");
      const btn = document.getElementById("upload-btn");
      const wrap = document.getElementById("progress-wrap");
      const fill = document.getElementById("progress-fill");
      const status = document.getElementById("status-msg");

      fetch("/api/info")
        .then((r) => r.json())
        .then((d) => {
          document.getElementById("fw-ver").textContent = "v" + d.version;
          document.getElementById("ap-ip").textContent = d.ip;
          if (d.chip) {
            document.getElementById("chip-badge").textContent = d.chip + " \u2022 OTA Ready";
          }
        })
        .catch(() => {});

      function setStatus(text, cls) {
        status.textContent = text;
        status.className = "status" + (cls ? " " + cls : "");
      }

      form.addEventListener("submit", function (e) {
        e.preventDefault();
        const file = fileInput.files[0];
        if (!file) {
          setStatus("Seleziona prima un file .bin valido.", "err");
          return;
        }
        if (!file.name.toLowerCase().endsWith(".bin")) {
          setStatus("Formato non valido: seleziona un file con estensione .bin.", "err");
          return;
        }

        const formData = new FormData();
        formData.append("update", file, file.name);

        const xhr = new XMLHttpRequest();
        xhr.open("POST", "/update", true);

        btn.disabled = true;
        fileInput.disabled = true;
        wrap.style.display = "block";
        fill.style.width = "0%";
        setStatus("Caricamento in corso... 0%", "");

        xhr.upload.onprogress = function (ev) {
          if (ev.lengthComputable) {
            const pct = Math.round((ev.loaded / ev.total) * 100);
            fill.style.width = pct + "%";
            setStatus("Caricamento in corso... " + pct + "%", "");
          }
        };

        xhr.onload = function () {
          if (xhr.status === 200) {
            fill.style.width = "100%";
            setStatus("Aggiornamento completato! La scheda si sta riavviando...", "ok");
            setTimeout(function () {
              window.location.reload();
            }, 6000);
          } else {
            setStatus("Errore OTA: " + (xhr.responseText || "risposta HTTP " + xhr.status), "err");
            btn.disabled = false;
            fileInput.disabled = false;
          }
        };

        xhr.onerror = function () {
          setStatus("Errore di rete durante l'invio del firmware.", "err");
          btn.disabled = false;
          fileInput.disabled = false;
        };

        xhr.send(formData);
      });
    })();
  </script>
</body>
</html>
)rawliteral";

/**
 * @brief Handler pagina principale OTA.
 */
static void handleRoot() {
  server.send(200, "text/html; charset=utf-8", INDEX_HTML);
}

/**
 * @brief Handler informativo JSON per l'interfaccia web.
 */
static void handleApiInfo() {
  String json = "{";
  json += "\"version\":\"" + String(FW_VERSION) + "\",";
  json += "\"chip\":\"" + String(ESP.getChipModel()) + "\",";
  json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
  json += "\"ip\":\"" + WiFi.softAPIP().toString() + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

/**
 * @brief Handler di risposta finale POST /update e riavvio controllato.
 */
static void handleUpdateFinish() {
  server.sendHeader("Connection", "close");

  if (otaUploadOk && !Update.hasError()) {
    server.send(200, "text/plain; charset=utf-8", "OK");
    Serial.println("[OTA] Risposta 200 inviata. Riavvio tra 500 ms...");
    delay(500);
    ESP.restart();
  } else {
    server.send(500, "text/plain; charset=utf-8", Update.errorString());
    Serial.printf("[OTA] Aggiornamento fallito: %s\n", Update.errorString());
  }
}

/**
 * @brief Handler streaming upload firmware multipart verso Flash OTA.
 */
static void handleUpdateUpload() {
  HTTPUpload &upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    otaUploadOk = false;
    setSafeHardwareState();

    Serial.printf("[OTA] Avvio upload: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      otaUploadOk = true;
      Serial.printf("[OTA] Scrittura completata: %u byte totali.\n", upload.totalSize);
    } else {
      otaUploadOk = false;
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    otaUploadOk = false;
    Update.end();
    Serial.println("[OTA] Upload interrotto dal client.");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("==========================================");
  Serial.println("  ESP32 OTA Web Flasher - Ready         ");
  Serial.printf("  Versione: v%s | Chip: %s\n", FW_VERSION, ESP.getChipModel());
  Serial.println("==========================================");

  // Avvio Access Point Wi-Fi
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(AP_SSID, AP_PASS)) {
    Serial.println("[WiFi] ERRORE: impossibile avviare Access Point!");
  } else {
    IPAddress apIP = WiFi.softAPIP();
    Serial.printf("[WiFi] Access Point attivo\n");
    Serial.printf("[WiFi] SSID     : %s\n", AP_SSID);
    Serial.printf("[WiFi] Password : %s\n", AP_PASS);
    Serial.printf("[WiFi] Indirizzo IP: %s\n", apIP.toString().c_str());
  }

  // Avvio mDNS (http://esp-setup.local/)
  if (MDNS.begin(MDNS_HOST)) {
    MDNS.addService("http", "tcp", HTTP_PORT);
    Serial.printf("[mDNS] Attivo su http://%s.local/\n", MDNS_HOST);
  } else {
    Serial.println("[mDNS] Avviso: impossibile avviare mDNS.");
  }

  // Configurazione rotte Web Server HTTP (porta 80)
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/info", HTTP_GET, handleApiInfo);
  server.on("/update", HTTP_POST, handleUpdateFinish, handleUpdateUpload);

  server.begin();
  Serial.printf("[HTTP] Web Server avviato su http://%s/\n", WiFi.softAPIP().toString().c_str());
}

void loop() {
  server.handleClient();
  delay(2);
}
