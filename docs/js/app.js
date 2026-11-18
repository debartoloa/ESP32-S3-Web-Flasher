import { Transport, ESPLoader } from "https://unpkg.com/esptool-js/bundle.js";

const BASE_PARTS = {
  bootloader: { path: "./firmware/bootloader.bin", address: 0x0000, label: "bootloader.bin" },
  partitions: { path: "./firmware/partitions.bin", address: 0x8000, label: "partitions.bin" },
  bootApp0: { path: "./firmware/boot_app0.bin", address: 0xE000, label: "boot_app0.bin" },
  baseFirmware: { path: "./firmware/firmware.bin", address: 0x10000, label: "firmware.bin (Base OTA)" },
};

const state = {
  port: null,
  transport: null,
  esploader: null,
  chipName: null,
  fwMode: "base", // "base" | "custom"
  customFile: null,
  isFlashing: false,
};

const el = {
  serialBadge: document.getElementById("serial-badge"),
  unsupportedBanner: document.getElementById("unsupported-banner"),
  card1: document.getElementById("card-step-1"),
  card2: document.getElementById("card-step-2"),
  card3: document.getElementById("card-step-3"),
  btnConnect: document.getElementById("btn-connect"),
  btnDisconnect: document.getElementById("btn-disconnect"),
  deviceStatus: document.getElementById("device-status"),
  tabBase: document.getElementById("tab-base"),
  tabCustom: document.getElementById("tab-custom"),
  panelBase: document.getElementById("panel-base"),
  panelCustom: document.getElementById("panel-custom"),
  customBinInput: document.getElementById("custom-bin-input"),
  customFileLabel: document.getElementById("custom-file-label"),
  chkIncludeBaseParts: document.getElementById("chk-include-base-parts"),
  chkEraseAll: document.getElementById("chk-erase-all"),
  btnFlash: document.getElementById("btn-flash"),
  progressArea: document.getElementById("progress-area"),
  progressFill: document.getElementById("progress-fill"),
  progressText: document.getElementById("progress-text"),
  terminal: document.getElementById("terminal"),
  btnClearLog: document.getElementById("btn-clear-log"),
  partsPreview: document.getElementById("parts-preview"),
};

const logger = {
  clean() {
    el.terminal.textContent = "";
  },
  writeLine(data) {
    el.terminal.textContent += `${data}\n`;
    el.terminal.scrollTop = el.terminal.scrollHeight;
  },
  write(data) {
    el.terminal.textContent += data;
    el.terminal.scrollTop = el.terminal.scrollHeight;
  },
};

function arrayBufferToBinaryString(buffer) {
  const bytes = new Uint8Array(buffer);
  const chunkSize = 0x8000;
  let result = "";
  for (let i = 0; i < bytes.length; i += chunkSize) {
    result += String.fromCharCode.apply(null, bytes.subarray(i, i + chunkSize));
  }
  return result;
}

async function fetchBinaryString(url) {
  const response = await fetch(url);
  if (!response.ok) {
    throw new Error(`Impossibile scaricare ${url} (HTTP ${response.status})`);
  }
  const buffer = await response.arrayBuffer();
  return arrayBufferToBinaryString(buffer);
}

function updatePartsPreview() {
  const rows = [];

  if (state.fwMode === "base") {
    rows.push(["0x0000", "bootloader.bin (auto)"]);
    rows.push(["0x8000", "partitions.bin (auto, Dual-OTA)"]);
    rows.push(["0xE000", "boot_app0.bin (auto)"]);
    rows.push(["0x10000", "firmware/firmware.bin (Base OTA)"]);
  } else {
    const fileName = state.customFile ? state.customFile.name : "Seleziona file .bin...";
    if (el.chkIncludeBaseParts.checked) {
      rows.push(["0x0000", "bootloader.bin (auto)"]);
      rows.push(["0x8000", "partitions.bin (auto, Dual-OTA)"]);
      rows.push(["0xE000", "boot_app0.bin (auto)"]);
      rows.push(["0x10000", `${fileName} (tuo firmware)`]);
    } else {
      rows.push(["0x0000", `${fileName} (immagine completa)`]);
    }
  }

  el.partsPreview.innerHTML = rows
    .map(([addr, name]) => `<li><span><code>${addr}</code></span><span>${name}</span></li>`)
    .join("");
}

function updateUIState() {
  const hasDevice = Boolean(state.esploader && state.chipName);
  const hasFirmware = state.fwMode === "base" || Boolean(state.customFile);

  el.card1.classList.toggle("is-ready", hasDevice);
  el.card2.classList.toggle("is-ready", hasFirmware);
  el.card3.classList.toggle("is-ready", hasDevice && hasFirmware && !state.isFlashing);

  el.btnConnect.hidden = hasDevice;
  el.btnDisconnect.hidden = !hasDevice;
  el.btnFlash.disabled = !(hasDevice && hasFirmware) || state.isFlashing;

  updatePartsPreview();
}

async function disconnectDevice() {
  try {
    if (state.transport) {
      await state.transport.disconnect();
    }
  } catch {
    // Ignora errori di chiusura porta già scollegata
  }
  state.port = null;
  state.transport = null;
  state.esploader = null;
  state.chipName = null;

  el.deviceStatus.textContent = "Nessun dispositivo connesso";
  el.deviceStatus.classList.remove("connected");
  updateUIState();
}

async function connectDevice() {
  if (!("serial" in navigator)) {
    logger.writeLine("[ERRORE] Web Serial API non supportata da questo browser.");
    return;
  }

  try {
    logger.writeLine("Selezione porta seriale USB...");
    const port = await navigator.serial.requestPort();
    const transport = new Transport(port, true);

    const esploader = new ESPLoader({
      transport,
      baudrate: 460800,
      terminal: logger,
    });

    el.btnConnect.disabled = true;
    el.deviceStatus.textContent = "Connessione al bootloader...";

    const chipName = await esploader.main();
    state.port = port;
    state.transport = transport;
    state.esploader = esploader;
    state.chipName = chipName;

    el.deviceStatus.textContent = `Connesso: ${chipName}`;
    el.deviceStatus.classList.add("connected");
    logger.writeLine(`[OK] Dispositivo pronto: ${chipName}`);
  } catch (err) {
    logger.writeLine(`[ERRORE] Connessione fallita: ${err.message || err}`);
    await disconnectDevice();
  } finally {
    el.btnConnect.disabled = false;
    updateUIState();
  }
}

async function buildFileArray() {
  const fileArray = [];

  const includeSystemParts =
    state.fwMode === "base" || el.chkIncludeBaseParts.checked;

  if (includeSystemParts) {
    logger.writeLine("Caricamento componenti di sistema ESP32-S3 (bootloader, partitions, boot_app0)...");
    fileArray.push({
      label: BASE_PARTS.bootloader.label,
      address: BASE_PARTS.bootloader.address,
      data: await fetchBinaryString(BASE_PARTS.bootloader.path),
    });
    fileArray.push({
      label: BASE_PARTS.partitions.label,
      address: BASE_PARTS.partitions.address,
      data: await fetchBinaryString(BASE_PARTS.partitions.path),
    });
    fileArray.push({
      label: BASE_PARTS.bootApp0.label,
      address: BASE_PARTS.bootApp0.address,
      data: await fetchBinaryString(BASE_PARTS.bootApp0.path),
    });
  }

  if (state.fwMode === "base") {
    logger.writeLine("Caricamento Firmware Base OTA (v1.0.0)...");
    fileArray.push({
      label: BASE_PARTS.baseFirmware.label,
      address: BASE_PARTS.baseFirmware.address,
      data: await fetchBinaryString(BASE_PARTS.baseFirmware.path),
    });
  } else {
    if (!state.customFile) {
      throw new Error("Nessun file .bin personalizzato selezionato.");
    }
    logger.writeLine(`Lettura file locale: ${state.customFile.name} (${state.customFile.size} byte)...`);
    const buffer = await state.customFile.arrayBuffer();
    const targetAddr = el.chkIncludeBaseParts.checked ? 0x10000 : 0x0000;
    fileArray.push({
      label: state.customFile.name,
      address: targetAddr,
      data: arrayBufferToBinaryString(buffer),
    });
  }

  return fileArray;
}

async function startFlash() {
  if (!state.esploader || state.isFlashing) return;

  state.isFlashing = true;
  el.progressArea.hidden = false;
  el.progressFill.style.width = "0%";
  el.progressText.textContent = "Preparazione file binari...";
  updateUIState();

  try {
    const fileArray = await buildFileArray();
    const eraseAll = el.chkEraseAll.checked;

    logger.writeLine(
      `Avvio scrittura in Flash (${fileArray.length} partizioni, eraseAll=${eraseAll})...`
    );

    await state.esploader.writeFlash({
      fileArray: fileArray.map(({ data, address }) => ({ data, address })),
      flashSize: "keep",
      eraseAll,
      compress: true,
      reportProgress(fileIndex, written, total) {
        const pct = Math.round((written / total) * 100);
        const label = fileArray[fileIndex]?.label || `File ${fileIndex + 1}`;
        el.progressFill.style.width = `${pct}%`;
        el.progressText.textContent = `${label}: ${pct}% (${written}/${total} byte)`;
      },
    });

    el.progressFill.style.width = "100%";
    el.progressText.textContent = "Flash completato! Riavvio della scheda...";
    logger.writeLine("[OK] Flash completato con successo!");

    // Hard reset tramite linee RTS/DTR
    await state.transport.setDTR(false);
    await state.transport.setRTS(true);
    await new Promise((r) => setTimeout(r, 100));
    await state.transport.setRTS(false);

    logger.writeLine("Scheda riavviata. Se hai installato il Firmware Base OTA, collegati al Wi-Fi 'ESP-Setup' (password: admin1234) e apri http://192.168.4.1/");
  } catch (err) {
    el.progressText.textContent = `Errore: ${err.message || err}`;
    logger.writeLine(`[ERRORE FLASH] ${err.message || err}`);
  } finally {
    state.isFlashing = false;
    updateUIState();
  }
}

function setFirmwareMode(mode) {
  state.fwMode = mode;
  const isBase = mode === "base";

  el.tabBase.classList.toggle("active", isBase);
  el.tabCustom.classList.toggle("active", !isBase);
  el.panelBase.hidden = !isBase;
  el.panelCustom.hidden = isBase;

  updateUIState();
}

document.addEventListener("DOMContentLoaded", () => {
  if ("serial" in navigator) {
    el.serialBadge.textContent = "Web Serial Supportata";
    el.serialBadge.classList.add("badge-ok");
  } else {
    el.serialBadge.textContent = "Web Serial Non Supportata";
    el.serialBadge.classList.add("badge-warn");
    el.unsupportedBanner.hidden = false;
    el.btnConnect.disabled = true;
  }

  el.btnConnect.addEventListener("click", connectDevice);
  el.btnDisconnect.addEventListener("click", disconnectDevice);
  el.btnFlash.addEventListener("click", startFlash);
  el.btnClearLog.addEventListener("click", () => logger.clean());

  el.tabBase.addEventListener("click", () => setFirmwareMode("base"));
  el.tabCustom.addEventListener("click", () => setFirmwareMode("custom"));

  el.customBinInput.addEventListener("change", (event) => {
    const file = event.target.files?.[0] || null;
    state.customFile = file;
    el.customFileLabel.textContent = file
      ? `File pronto: ${file.name} (${Math.round(file.size / 1024)} KB)`
      : "Seleziona un file .bin dal PC...";
    updateUIState();
  });

  el.chkIncludeBaseParts.addEventListener("change", updatePartsPreview);

  updateUIState();
});
