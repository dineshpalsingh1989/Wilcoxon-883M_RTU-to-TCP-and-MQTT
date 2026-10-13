#include "core/modules.h"

// =================================================================
// --- NEW: Time & Date Settings Handlers ---
// =================================================================

void handleTimeSettings() {
    int offsetHours = gmtOffset_sec / 3600;
    String html = R"rawliteral(
<!DOCTYPE html><html><head><title>Time Settings</title><meta name="viewport" content="width=device-width, initial-scale=1">
<style>body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;margin:0;background-color:#f4f6f8;display:flex;justify-content:center;padding:20px 0;}
.container{width:100%;max-width:500px;background-color:white;border-radius:12px;padding:30px;box-shadow:0 8px 20px rgba(0,0,0,.1);}
h2{margin-top:0; color:#2c3e50; border-bottom:1px solid #eee; padding-bottom:15px;}
.group{margin-bottom:25px; padding:15px; background:#f8f9fa; border-radius:8px;}
label{display:block;margin-bottom:8px;font-weight:600;color:#555;}
input,select{width:100%;padding:10px;border:1px solid #ccc;border-radius:6px;box-sizing:border-box;margin-bottom:10px;}
button{width:100%;padding:12px;background:#007bff;color:white;border:none;border-radius:6px;font-weight:600;cursor:pointer;}
.current-time{text-align:center; font-size:18px; font-weight:bold; color:#007bff; margin-bottom:20px; font-family:monospace;}
.nav-link{display:block;text-align:center;margin-top:20px;color:#007bff;text-decoration:none;}
.note{font-size:12px; color:#666; margin-top:-5px; margin-bottom:10px;}
</style></head><body><div class="container"><h2>Time Configuration</h2>
<div class="current-time">Device Time:<br>)rawliteral" + getTimestamp() + R"rawliteral(</div>

<div class="group">
<h3>1. Timezone Settings</h3>
<p class="note">Set this for both Internet Sync and Manual Time.</p>
<form onsubmit="return saveZone()">
<label>GMT Offset (Seconds)</label>
<select id="offset">
<option value="0">UTC +0</option>
<option value="28800">UTC +8 (Malaysia/China)</option>
<option value="19800">UTC +5:30 (India)</option>
<option value="3600">UTC +1 (Europe)</option>
<option value="-18000">UTC -5 (US East)</option>
<option value="custom">Custom...</option>
</select>
<input type="number" id="custom_offset" placeholder="Enter offset in seconds" style="display:none;">
<button type="submit">Save Timezone</button>
</form>
</div>

<div class="group">
<h3>2. Manual Time Set</h3>
<p class="note">Use this if the device has No Internet connection.</p>
<label>Set Date & Time</label>
<input type="datetime-local" id="manualTime">
<button onclick="setManualTime()">Update Device Clock</button>
</div>

<a href="/settings" class="nav-link">&larr; Back to Settings</a>
</div>
<script>
window.onload = function() {
    let currentOff = )rawliteral" + String(gmtOffset_sec) + R"rawliteral(;
    let sel = document.getElementById('offset');
    let found = false;
    for(let i=0; i<sel.options.length; i++){ if(sel.options[i].value == currentOff){ sel.selectedIndex = i; found=true; break; }}
    if(!found){ sel.value='custom'; document.getElementById('custom_offset').style.display='block'; document.getElementById('custom_offset').value = currentOff; }

    sel.addEventListener('change', function() {
        if(this.value === 'custom') document.getElementById('custom_offset').style.display = 'block';
        else document.getElementById('custom_offset').style.display = 'none';
    });

    const now = new Date();
    // Adjust to local time for the input picker
    now.setMinutes(now.getMinutes() - now.getTimezoneOffset());
    document.getElementById('manualTime').value = now.toISOString().slice(0,16);
};

function saveZone() {
    let off = document.getElementById('offset').value;
    if(off === 'custom') off = document.getElementById('custom_offset').value;
    fetch('/save_time?offset=' + off).then(() => { alert('Timezone Saved. Rebooting...'); });
    return false;
}

function setManualTime() {
    const val = document.getElementById('manualTime').value;
    if(!val) return;
    // Get timestamp in seconds
    const ts = Math.floor(new Date(val).getTime() / 1000);
    fetch('/set_manual_time?ts=' + ts).then(() => { alert('Clock Updated!'); location.reload(); });
}
</script></body></html>)rawliteral";
    server.send(200, "text/html", html);
}

void handleSaveTimeSettings() {
    long offset = server.arg("offset").toInt();
    preferences.begin("device-cfg", false);
    preferences.putLong("tz_offset", offset);
    preferences.end();
    server.send(200, "text/plain", "Saved.");
    delay(500); ESP.restart();
}

void handleSetManualTime() {
    long ts = server.arg("ts").toInt();
    struct timeval tv;
    tv.tv_sec = ts;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
    logMessage("Manual Time Set to timestamp: " + String(ts));
    server.send(200, "text/plain", "OK");
}

void handleClearSystemLog() {
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        globalLogBuffer = ""; // Clear the string buffer
        String ts = getTimestamp();
        globalLogBuffer = ts + " [System] Log Cleared by User\n";
        xSemaphoreGive(dataMutex);
    }
    server.send(200, "text/plain", "System log cleared.");
}

void handleDeviceInfo() {
    // 1. Check if JSON format is requested for dynamic updates
    if (server.hasArg("format") && server.arg("format") == "json") {
        DynamicJsonDocument doc(1024);
        
        bool internetConnected = (WiFi.status() == WL_CONNECTED);
        String ipAddr = internetConnected ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
        String rssiVal = internetConnected ? String(WiFi.RSSI()) + " dBm" : "N/A";
        String statusText = internetConnected ? "Connected to Internet" : "No Internet (AP Mode)";
        
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            doc["uptime"] = formatUptime(millis());
            doc["ram"] = String(ESP.getFreeHeap()) + " bytes";
            doc["log"] = globalLogBuffer;
            xSemaphoreGive(dataMutex);
        } else {
            doc["uptime"] = "--";
            doc["ram"] = "--";
            doc["log"] = "Log busy...";
        }
        
        doc["time"] = getTimestamp();
        doc["ip"] = ipAddr;
        doc["rssi"] = rssiVal;
        doc["status"] = statusText;
        doc["connected"] = internetConnected;

        String payload;
        serializeJson(doc, payload);
        server.send(200, "application/json", payload);
        return;
    }

    // 2. Otherwise serve the HTML page
    String uptime = formatUptime(millis());
    String localLog;
    String d_model, d_serial, d_url, d_pn, d_fw;
    
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        localLog = globalLogBuffer;
        d_model = sensor_model;
        d_serial = sensor_serial;
        d_url = sensor_vendor_url; 
        d_pn = sensor_part_number;
        d_fw = sensor_fw_version;
        xSemaphoreGive(dataMutex);
    }
    localLog.replace("\n", "<br>");

    bool internetConnected = (WiFi.status() == WL_CONNECTED);
    String ipAddr = internetConnected ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
    String rssiVal = internetConnected ? String(WiFi.RSSI()) + " dBm" : "N/A";

    String html = R"rawliteral(
<!DOCTYPE html><html><head><title>Device Diagnostics</title><meta name="viewport" content="width=device-width, initial-scale=1">
<script src="https://unpkg.com/lucide@latest"></script>
<style>
body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;margin:0;background-color:#f4f6f8;display:flex;justify-content:center;padding:20px 0;}
.container{width:100%;max-width:800px;background-color:white;border-radius:12px;box-shadow:0 8px 20px rgba(0,0,0,.1);text-align:center;padding:30px}
h2{color:#2c3e50;margin-top:0}.info-grid{display:grid;grid-template-columns:1fr 1fr;gap:15px;text-align:left;margin-top:25px}
.info-item{background-color:#f8f9fa;padding:15px;border-radius:8px}
.info-label{font-size:12px;color:#6c757d;font-weight:600;display:block;margin-bottom:5px;text-transform:uppercase;letter-spacing:0.5px;}
.info-value{font-size:16px;color:#343a40;font-weight:500; word-wrap: break-word;}
.full-width { grid-column: 1 / -1; }
.log-box { margin-top: 25px; text-align: left; }
.log-header { display:flex; justify-content:space-between; align-items:center; margin-bottom: 10px; }
.log-content { background-color: #2c3e50; color: #a4b0be; border: 1px solid #e9ecef; border-radius: 8px; padding: 15px; max-height: 400px; overflow-y: auto; font-family: "Consolas", "Monaco", monospace; font-size: 12px; white-space: pre-wrap; line-height: 1.4; }
.btn-clear { background-color: #e74c3c; color: white; border: none; padding: 6px 12px; border-radius: 4px; cursor: pointer; font-size: 12px; display: flex; align-items: center; gap: 5px; }
.btn-clear:hover { background-color: #c0392b; }
.status-connected { color: #2ecc71; font-weight: bold; }
.status-disconnected { color: #e74c3c; font-weight: bold; }
.nav-link{display:inline-block;margin-top:30px;color:#007bff;text-decoration:none;font-weight:600; border: 1px solid #007bff; padding: 10px 20px; border-radius: 6px; transition: all 0.2s; }
.nav-link:hover { background-color: #007bff; color: white; }

/* OTA Section Styles */
.ota-box { background: #eef2f5; border: 1px solid #dee2e6; border-radius: 8px; padding: 20px; margin-top: 25px; text-align: left; }
.ota-title { font-weight: 600; color: #2c3e50; margin-bottom: 15px; display: flex; align-items: center; gap: 8px; }
.file-upload-row { display: flex; gap: 10px; align-items: center; margin-bottom: 15px; }
input[type=file] { background: white; padding: 8px; border-radius: 6px; border: 1px solid #ced4da; flex-grow: 1; }
.btn-upload { background-color: #007bff; color: white; border: none; padding: 10px 20px; border-radius: 6px; cursor: pointer; font-weight: 600; }
.btn-upload:disabled { background-color: #6c757d; cursor: not-allowed; }
.progress-container { width: 100%; background-color: #dfe6e9; border-radius: 10px; height: 20px; overflow: hidden; display: none; margin-bottom: 10px; }
.progress-bar { width: 0%; height: 100%; background-color: #2ecc71; text-align: center; line-height: 20px; color: white; font-size: 12px; font-weight: bold; transition: width 0.3s; }
#ota-status { font-size: 14px; font-weight: 500; min-height: 20px; }
.status-error { color: #dc3545; }
.status-success { color: #28a745; }
.ota-drop-zone { border: 2px dashed #ced4da; border-radius: 8px; padding: 30px; text-align: center; color: #6c757d; cursor: pointer; transition: 0.3s; background: white; }
.ota-drop-zone.active { border-color: #007bff; background-color: #f1f8ff; color: #007bff; }
.ota-drop-zone p { margin: 10px 0 0; font-size: 14px; }
</style></head><body><div class="container"><h2>Device Diagnostics</h2>
<div class="info-grid">

<div class="info-item"><span class="info-label">Sensor Model</span><span class="info-value">)rawliteral" + d_model + R"rawliteral(</span></div>
<div class="info-item"><span class="info-label">Part Number</span><span class="info-value">)rawliteral" + d_pn + R"rawliteral(</span></div>
<div class="info-item"><span class="info-label">Sensor Serial</span><span class="info-value">)rawliteral" + d_serial + R"rawliteral(</span></div>
<div class="info-item"><span class="info-label">Vendor URL</span><span class="info-value">)rawliteral" + d_url + R"rawliteral(</span></div>
<div class="info-item"><span class="info-label">Sensor Firmware</span><span class="info-value">)rawliteral" + d_fw + R"rawliteral(</span></div>
<div class="info-item"><span class="info-label">Connection Status</span><span class="info-value" id="val-status">)rawliteral" + (internetConnected ? "Connected to Internet" : "No Internet (AP Mode)") + R"rawliteral(</span></div>

<div class="info-item"><span class="info-label">Last Reset Reason</span><span class="info-value" style="color:#d63031">)rawliteral" + lastResetReason + R"rawliteral(</span></div>
<div class="info-item"><span class="info-label">IP Address</span><span class="info-value" id="val-ip">)rawliteral" + ipAddr + R"rawliteral(</span></div>
<div class="info-item"><span class="info-label">Signal Strength (RSSI)</span><span class="info-value" id="val-rssi">)rawliteral" + rssiVal + R"rawliteral(</span></div>
<div class="info-item"><span class="info-label">System Time</span><span class="info-value" id="val-time">)rawliteral" + getTimestamp() + R"rawliteral(</span></div>
<div class="info-item"><span class="info-label">System Uptime</span><span class="info-value" id="val-uptime">)rawliteral" + uptime + R"rawliteral(</span></div>
<div class="info-item"><span class="info-label">Free RAM</span><span class="info-value" id="val-ram">)rawliteral" + String(ESP.getFreeHeap()) + R"rawliteral( bytes</span></div>
<div class="info-item"><span class="info-label">Firmware</span><span class="info-value">)rawliteral" + String(FIRMWARE_VERSION) + R"rawliteral(</span></div>
</div>

<div class="ota-box">
    <div class="ota-title"><i data-lucide="upload-cloud"></i> Firmware Update (OTA)</div>
    <div class="ota-drop-zone" id="dropZone" onclick="document.getElementById('otafile').click()">
        <i data-lucide="file-up" style="width: 32px; height: 32px;"></i>
        <p>Click or Drag & Drop <strong>.bin</strong> firmware file here</p>
        <div id="fileName" style="margin-top: 10px; font-weight: bold; color: #333; display:none;"></div>
    </div>
    <input type="file" id="otafile" name="update" accept=".bin" style="display:none" onchange="fileSelected(this)">
    <button class="btn-upload" onclick="performOTA()" id="uploadBtn" style="width:100%; margin-top: 15px;" disabled>Start Update</button>
    <div class="progress-container" id="progressWrap">
        <div class="progress-bar" id="progressBar">0%</div>
    </div>
    <div id="ota-status"></div>
</div>

<div class="log-box">
  <div class="log-header">
    <h3>System Event Log</h3>
    <button class="btn-clear" onclick="clearLog()"><i data-lucide="trash-2" style="width:14px;"></i> Clear Log</button>
  </div>
  <div class="log-content" id="logContent">)rawliteral" + localLog + R"rawliteral(</div>
</div>
<a href="/settings" class="nav-link">&larr; Back to Settings</a></div>
<script>
if (typeof lucide !== 'undefined') { lucide.createIcons(); }

// --- Live Update Logic ---
setInterval(updateDiagnostics, 2000);

function updateDiagnostics() {
    fetch('/device_info?format=json')
        .then(response => response.json())
        .then(data => {
            document.getElementById('val-uptime').textContent = data.uptime;
            document.getElementById('val-ram').textContent = data.ram;
            document.getElementById('val-time').textContent = data.time;
            document.getElementById('val-ip').textContent = data.ip;
            document.getElementById('val-rssi').textContent = data.rssi;
            
            const statusEl = document.getElementById('val-status');
            statusEl.textContent = data.status;
            if(data.connected) {
                statusEl.classList.remove('status-disconnected');
                statusEl.classList.add('status-connected');
            } else {
                statusEl.classList.remove('status-connected');
                statusEl.classList.add('status-disconnected');
            }
            
            // Optional: Update Log if you want real-time logs
            // document.getElementById('logContent').innerHTML = data.log.replace(/\n/g, '<br>');
        })
        .catch(err => console.error('Error fetching diagnostics:', err));
}

function clearLog() {
    if(confirm("Clear system event log?")) {
        fetch('/clear_system_log').then(r => r.text()).then(msg => {
            document.getElementById('logContent').innerHTML = "Log cleared.";
        });
    }
}

// --- Drag & Drop Logic ---
const dropZone = document.getElementById('dropZone');
const fileInput = document.getElementById('otafile');

['dragenter', 'dragover', 'dragleave', 'drop'].forEach(eventName => {
  dropZone.addEventListener(eventName, preventDefaults, false);
});
function preventDefaults(e) { e.preventDefault(); e.stopPropagation(); }

['dragenter', 'dragover'].forEach(eventName => {
  dropZone.addEventListener(eventName, () => dropZone.classList.add('active'), false);
});
['dragleave', 'drop'].forEach(eventName => {
  dropZone.addEventListener(eventName, () => dropZone.classList.remove('active'), false);
});

dropZone.addEventListener('drop', handleDrop, false);

function handleDrop(e) {
  const dt = e.dataTransfer;
  const files = dt.files;
  if(files.length > 0) {
      fileInput.files = files;
      fileSelected(fileInput);
  }
}

function fileSelected(input) {
    const file = input.files[0];
    if (file) {
        if (!file.name.endsWith(".bin")) {
            alert("Invalid file type! Please select a .bin firmware file.");
            input.value = ""; // Clear
            document.getElementById('fileName').style.display = 'none';
            document.getElementById('uploadBtn').disabled = true;
            return;
        }
        document.getElementById('fileName').textContent = file.name + " (" + (file.size/1024/1024).toFixed(2) + " MB)";
        document.getElementById('fileName').style.display = 'block';
        document.getElementById('uploadBtn').disabled = false;
        document.getElementById('ota-status').innerHTML = '';
    }
}

function performOTA() {
    var file = fileInput.files[0];
    if (!file) return;

    if(!confirm("Start firmware update? Do not power off the device.")) return;

    var status = document.getElementById('ota-status');
    var btn = document.getElementById('uploadBtn');
    var progressBar = document.getElementById('progressBar');
    var progressWrap = document.getElementById('progressWrap');
    
    // Warn user before leaving page
    window.onbeforeunload = function() { return "Update in progress. Are you sure?"; };

    var formData = new FormData();
    formData.append("update", file);
    var xhr = new XMLHttpRequest();
    
    // UI Reset
    btn.disabled = true;
    btn.textContent = "Uploading...";
    status.innerHTML = '<span style="color:#007bff">Starting upload...</span>';
    progressWrap.style.display = 'block';
    progressBar.style.width = '0%';
    progressBar.innerHTML = '0%';

    // Progress Handler
    xhr.upload.addEventListener("progress", function(e) {
        if (e.lengthComputable) {
            var percent = Math.round((e.loaded / e.total) * 100);
            var loadedMB = (e.loaded / 1024 / 1024).toFixed(2);
            var totalMB = (e.total / 1024 / 1024).toFixed(2);
            
            progressBar.style.width = percent + '%';
            progressBar.innerHTML = percent + '%';
            status.innerHTML = `Uploading: ${loadedMB} MB / ${totalMB} MB`;
            
            if(percent === 100) status.innerHTML = '<span style="color:#28a745">Finalizing update... please wait.</span>';
        }
    }, false);

    // Completion Handler
    xhr.onload = function() {
        window.onbeforeunload = null; // Remove warning
        if (xhr.status === 200) {
            status.innerHTML = '<span class="status-success">Update Success! Rebooting...</span>';
            progressBar.style.backgroundColor = '#2ecc71';
            setTimeout(function() { window.location.href = '/'; }, 10000);
        } else {
            status.innerHTML = '<span class="status-error">Update Failed! Server Error.</span>';
            btn.disabled = false;
            btn.textContent = "Retry Upload";
        }
    };

    // Error Handler
    xhr.onerror = function() {
        window.onbeforeunload = null;
        status.innerHTML = '<span class="status-error">Update Failed! Network Error.</span>';
        btn.disabled = false;
        btn.textContent = "Retry Upload";
    };

    xhr.open("POST", "/update");
    xhr.send(formData);
}
</script>
</body></html>)rawliteral";
    server.send(200, "text/html", html);
}

