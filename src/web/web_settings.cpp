#include "core/modules.h"

// --- SAFE CHUNKED SETTINGS PAGE ---
void handleSettings() {
    // 1. Tell the browser we are sending the page in parts (Unknown Length)
    server.setContentLength(CONTENT_LENGTH_UNKNOWN); 
    server.send(200, "text/html", ""); // Start the connection

    // 2. Send Part 1: The Header and CSS (Static)
    // We break this up so it doesn't use much RAM
    server.sendContent(R"rawliteral(
<!DOCTYPE html><html><head><title>Device Settings</title><meta name="viewport" content="width=device-width, initial-scale=1">
<script src="https://unpkg.com/lucide@latest"></script>
<style>body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;margin:0;background-color:#f4f6f8;display:flex;justify-content:center;align-items:center;min-height:100vh;padding:20px;box-sizing:border-box}.container{width:100%;max-width:500px;background-color:white;border-radius:12px;box-shadow:0 8px 30px rgba(0,0,0,.08);overflow:hidden}.header{text-align:center;padding:20px;border-bottom:1px solid #e9ecef}.header h2{margin:0;font-size:24px;font-weight:600;color:#2c3e50}.settings-list{list-style:none;padding:0;margin:0}.settings-item{display:flex;align-items:center;padding:15px 20px;border-bottom:1px solid #e9ecef;text-decoration:none;color:inherit;transition:background-color .2s}.settings-item:hover{background-color:#f8f9fa}.settings-item:last-child{border-bottom:none}.icon{width:24px;height:24px;margin-right:20px;color:#007bff}.content{flex-grow:1;text-align:left}.content h3{margin:0;font-size:16px;font-weight:500;color:#343a40}.content p{margin:2px 0 0;font-size:14px;color:#6c757d;display:flex;align-items:center}.chevron{width:20px;height:20px;color:#ced4da}.status-dot{height:9px;width:9px;border-radius:50%;display:inline-block;margin-right:6px}.status-dot.connected{background-color:#2ecc71}.status-dot.disconnected{background-color:#e74c3c}.actions{padding:20px;background-color:#f8f9fa;border-top:1px solid #e9ecef}.btn{display:block;width:100%;padding:12px;border:none;border-radius:8px;cursor:pointer;font-size:15px;font-weight:600;text-align:center;text-decoration:none;box-sizing:border-box;transition:background-color .2s,color .2s}.btn-danger{background-color:#e74c3c;color:white;margin-bottom:10px}.btn-danger:hover{background-color:#c0392b}.btn-secondary{background-color:transparent;color:#007bff}.btn-secondary:hover{background-color:#e9ecef}
/* Shared palette and accessible controls, embedded for offline device use. */
:root{color-scheme:light}body{background:#f2f5f7;color:#192f3d;line-height:1.5}
.container{border:1px solid #dfe8ed;border-radius:16px;box-shadow:0 12px 40px #173c490b;overflow:hidden}
.header{background:linear-gradient(115deg,#102c3b,#194655);color:white}
.header h2{letter-spacing:-.5px;color:#f8fafc}.settings-item:hover{background:#eef7f5}
.settings-item .icon{color:#147d78}.btn-primary,button[type=submit]{background-color:#147d78;color:white}
input,select,button,.btn{font:inherit}input:not([type=checkbox]),select{min-height:44px;border-radius:8px}
input[type=checkbox]{accent-color:#147d78}button,.btn{min-height:44px}
a:focus-visible,button:focus-visible,input:focus-visible,select:focus-visible{outline:3px solid #42a8bd;outline-offset:3px}
@media(max-width:480px){.container{margin:16px auto;max-width:calc(100% - 24px)}.settings-item{padding:16px}}
@media(prefers-reduced-motion:reduce){*{transition:none!important;scroll-behavior:auto!important}}
</style></head><body><div class="container"><div class="header"><h2>Device Settings</h2></div><div class="settings-list">
)rawliteral");

    // 3. Send Part 2: Wi-Fi Status (Dynamic)
    String wifiClass = (WiFi.status() == WL_CONNECTED) ? "connected" : "disconnected";
    String wifiText = (WiFi.status() == WL_CONNECTED) ? "Connected to " + WiFi.SSID() : "AP Mode";
    
    String chunk1 = R"rawliteral(<a href="/wifi_settings" class="settings-item"><div class="icon"><i data-lucide="wifi"></i></div><div class="content"><h3>Wi-Fi</h3><p><span class="status-dot )rawliteral";
    chunk1 += wifiClass + R"rawliteral("></span>)rawliteral" + wifiText + R"rawliteral(</p></div><div class="chevron"><i data-lucide="chevron-right"></i></div></a>)rawliteral";
    server.sendContent(chunk1);

    // 4. Send Part 3: MQTT Status (Dynamic)
    String mqttClass = (mqttClient && mqttClient->connected()) ? "connected" : "disconnected";
    String mqttText = (mqttEnabled) ? ((mqttClient && mqttClient->connected()) ? "Connected" : "Disconnected") : "Disabled";
    
    String chunk2 = R"rawliteral(<a href="/mqtt_settings" class="settings-item"><div class="icon"><i data-lucide="cloud"></i></div><div class="content"><h3>MQTT</h3><p><span class="status-dot )rawliteral";
    chunk2 += mqttClass + R"rawliteral("></span>)rawliteral" + mqttText + R"rawliteral(</p></div><div class="chevron"><i data-lucide="chevron-right"></i></div></a>)rawliteral";
    server.sendContent(chunk2);

    // 5. Send Part 4: Modbus Status (Dynamic)
    String mbClass = (modbusTcpEnabled) ? "connected" : "disconnected";
    String mbText = (modbusTcpEnabled) ? "Enabled" : "Disabled";

    String chunk3 = R"rawliteral(<a href="/rs485_settings" class="settings-item"><div class="icon"><i data-lucide="activity"></i></div><div class="content"><h3>Modbus RTU (RS485) Sensor</h3><p><span class="status-dot connected"></span>Ready</p></div><div class="chevron"><i data-lucide="chevron-right"></i></div></a>
<a href="/modbustcp_settings" class="settings-item"><div class="icon"><i data-lucide="server"></i></div><div class="content"><h3>Modbus TCP Server</h3><p><span class="status-dot )rawliteral";
    chunk3 += mbClass + R"rawliteral("></span>)rawliteral" + mbText + R"rawliteral(</p></div><div class="chevron"><i data-lucide="chevron-right"></i></div></a>)rawliteral";
    server.sendContent(chunk3);

    // 6. Send Part 5: The Footer and Scripts (Static)
    server.sendContent(R"rawliteral(
<a href="/time_settings" class="settings-item"><div class="icon"><i data-lucide="clock"></i></div><div class="content"><h3>Time & Date</h3><p>Timezone & Manual Set</p></div><div class="chevron"><i data-lucide="chevron-right"></i></div></a>
<a href="/device_info" class="settings-item"><div class="icon" style="color:#6c757d"><i data-lucide="info"></i></div><div class="content"><h3>Device Info</h3><p>View model, serial, and firmware</p></div><div class="chevron"><i data-lucide="chevron-right"></i></div></a>
<a href="/github_updates" class="settings-item"><div class="icon"><i data-lucide="download-cloud"></i></div><div class="content"><h3>GitHub Firmware Updates</h3><p>Check releases and download gateway firmware</p></div><div class="chevron"><i data-lucide="chevron-right"></i></div></a>
</div><div class="actions"><button class="btn btn-danger" onclick="rebootDevice()">Reboot Device</button><a href="/" class="btn btn-secondary">Back to Dashboard</a></div></div>
<script>
    if (typeof lucide !== 'undefined') { lucide.createIcons(); }
    function rebootDevice(){if(confirm("Are you sure you want to reboot the device?")){fetch("/reboot").then(()=>{alert("Device is rebooting...")})}}
</script></body></html>)rawliteral");

    // 7. Finish the transmission
    server.sendContent(""); 
}

// --- SAFE CHUNKED WIFI SETTINGS ---
void handleWifiSettings() {
    // 1. Start the Chunked Response
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");

    // 2. Send Header & CSS
    server.sendContent(R"rawliteral(
<!DOCTYPE html><html><head><title>Wi-Fi Settings</title><meta name="viewport" content="width=device-width, initial-scale=1"><style>body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;margin:0;background-color:#f4f6f8;display:flex;justify-content:center;align-items:center;min-height:100vh}.container{width:100%;max-width:500px;padding:20px;box-sizing:border-box;background-color:white;border-radius:12px;box-shadow:0 8px 20px rgba(0,0,0,.1)}.header{text-align:center;padding-bottom:20px;border-bottom:2px solid #e9ecef;margin-bottom:20px}.header h2{margin:0;font-size:24px;font-weight:600;color:#2c3e50}.form-group{margin-bottom:15px}label{display:block;margin-bottom:8px;font-size:14px;font-weight:600}input[type=text],input[type=password]{width:100%;padding:12px;box-sizing:border-box;border:1px solid #ced4da;border-radius:8px;font-size:16px;transition:border-color .3s}input[type=text]:focus,input[type=password]:focus{outline:0;border-color:#007bff}.btn-primary{width:100%;padding:12px;background-color:#007bff;color:white;border:none;border-radius:8px;cursor:pointer;font-size:16px;font-weight:600;transition:background-color .3s}.btn-primary:hover{background-color:#0056b3}.btn-scan{background-color:#6c757d;margin-bottom:15px}.btn-scan:hover{background-color:#5a6268}.wifi-list-container{max-height:250px;overflow-y:auto;border:1px solid #e9ecef;border-radius:8px;padding:5px;background-color:#f8f9fa}#wifi-list{list-style:none;padding:0;margin:0}#wifi-list li{padding:12px 15px;cursor:pointer;border-bottom:1px solid #e9ecef;transition:background-color .2s;display:flex;justify-content:space-between;align-items:center}#wifi-list li:last-child{border-bottom:none}#wifi-list li:hover{background-color:#e9ecef}.wifi-ssid{font-size:16px;font-weight:500;flex-grow:1}.wifi-rssi{font-size:14px;color:#6c757d;font-weight:400}.status-bar{text-align:center;margin-top:20px;padding-top:20px;border-top:2px solid #e9ecef;font-size:14px;color:#6c757d;font-weight:500}.nav-link{display:block;text-align:center;margin-top:20px;color:#007bff;text-decoration:none;font-weight:600;font-size:14px}.nav-link:hover{text-decoration:underline}
/* Shared palette and accessible controls, embedded for offline device use. */
:root{color-scheme:light}body{background:#f2f5f7;color:#192f3d;line-height:1.5}
.container{border:1px solid #dfe8ed;border-radius:16px;box-shadow:0 12px 40px #173c490b;overflow:hidden}
.header{background:linear-gradient(115deg,#102c3b,#194655);color:white}
.header h2{letter-spacing:-.5px;color:#f8fafc}.settings-item:hover{background:#eef7f5}
.settings-item .icon{color:#147d78}.btn-primary,button[type=submit]{background-color:#147d78;color:white}
input,select,button,.btn{font:inherit}input:not([type=checkbox]),select{min-height:44px;border-radius:8px}
input[type=checkbox]{accent-color:#147d78}button,.btn{min-height:44px}
a:focus-visible,button:focus-visible,input:focus-visible,select:focus-visible{outline:3px solid #42a8bd;outline-offset:3px}
@media(max-width:480px){.container{margin:16px auto;max-width:calc(100% - 24px)}.settings-item{padding:16px}}
@media(prefers-reduced-motion:reduce){*{transition:none!important;scroll-behavior:auto!important}}
</style></head><body><div class="container"><div class="header"><h2>Wi-Fi Settings</h2></div>
)rawliteral");

    // 3. Send Dynamic Status Bar
    String statusMsg = (WiFi.status() == WL_CONNECTED ? "Connected to " + WiFi.SSID() : "AP Mode active");
    String ipMsg = (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString());
    
    server.sendContent(R"rawliteral(<div class="status-bar">Current Status: )rawliteral" + statusMsg + R"rawliteral(<br>IP Address: )rawliteral" + ipMsg + R"rawliteral(</div>)rawliteral");

    // 4. Send Form & Scripts
    server.sendContent(R"rawliteral(
<div class="form-section"><button class="btn-primary btn-scan" onclick="scanWifi()">Scan for Networks</button><div class="wifi-list-container"><ul id="wifi-list"></ul></div><form onsubmit="return saveWifi()"><div class="form-group"><label for="ssid">Network Name (SSID)</label><input type="text" id="ssid" name="ssid" required></div><div class="form-group"><label for="pass">Password</label><input type="password" id="pass" name="pass"></div><button type="submit" class="btn-primary">Save and Connect</button></form></div><a href="/settings" class="nav-link">&larr; Back to Settings</a></div><script>function scanWifi(){const t=document.getElementById("wifi-list"),s=Date.now();let f=!0;t.innerHTML="<li>Scanning... Wi-Fi may pause for a few seconds; this page keeps trying.</li>";const p=()=>fetch("/scanwifi"+(f?"?start=1":""),{cache:"no-store"}).then(r=>{if(f=!1,202===r.status)throw 0;if(!r.ok)throw 0;return r.json()}).then(e=>{t.innerHTML="",0===e.length?t.innerHTML="<li>No networks found.</li>":e.forEach(e=>{const n=document.createElement("li"),a=document.createElement("span"),b=document.createElement("span");a.className="wifi-ssid",a.textContent=e.ssid,b.className="wifi-rssi",b.textContent=e.rssi+" dBm",n.append(a,b),n.onclick=()=>{document.getElementById("ssid").value=e.ssid},t.appendChild(n)})}).catch(()=>{Date.now()-s<45e3?setTimeout(p,1500):t.innerHTML="<li>Scan did not finish. Reconnect to ILA_Sensor_AP and press Scan again.</li>"});p()}function saveWifi(){const t=document.getElementById("ssid").value,e=document.getElementById("pass").value;return fetch("/savewifi?ssid="+encodeURIComponent(t)+"&pass="+encodeURIComponent(e)),alert("Saving credentials... The device will reboot."),!1}</script></body></html>
)rawliteral");

    // 5. Close Stream
    server.sendContent("");
}

void handleSaveWifi() { 
    preferences.begin("wifi-creds", false); 
    preferences.putString("ssid", server.arg("ssid")); 
    preferences.putString("password", server.arg("pass")); 
    preferences.end(); 
    server.send(200, "text/plain", "Credentials saved. Device will reboot."); 
    delay(1000); 
    ESP.restart(); 
}

// --- SAFE CHUNKED MQTT SETTINGS ---
void handleMqttSettings() {
    // 1. Start Stream
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");

    // 2. Header & Styles
    server.sendContent(R"rawliteral(
<!DOCTYPE html><html><head><title>MQTT Settings</title><meta name="viewport" content="width=device-width, initial-scale=1">
<script src="https://unpkg.com/lucide@latest"></script>
<style>
body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;margin:0;background-color:#f4f6f8;display:flex;justify-content:center;padding:20px 0;}
.container{width:100%;max-width:500px;padding:20px;box-sizing:border-box;background-color:white;border-radius:12px;box-shadow:0 8px 20px rgba(0,0,0,.1);}
.header{text-align:center;padding-bottom:20px;border-bottom:2px solid #e9ecef;margin-bottom:20px}.header h2{margin:0;font-size:24px;font-weight:600;color:#2c3e50}
.form-group{margin-bottom:15px}.form-group.inline{display:flex;align-items:center;justify-content:space-between;padding:10px 0; border-bottom: 1px solid #f0f0f0;}
label{display:block;margin-bottom:8px;font-size:14px;font-weight:600}
input[type=text],input[type=password],input[type=number]{width:100%;padding:12px;box-sizing:border-box;border:1px solid #ced4da;border-radius:8px;font-size:16px}
.btn-primary{width:100%;padding:12px;background-color:#007bff;color:white;border:none;border-radius:8px;cursor:pointer;font-size:16px;font-weight:600}
.nav-link{display:block;text-align:center;margin-top:20px;color:#007bff;text-decoration:none}
.switch{position:relative;display:inline-block;width:50px;height:28px}.switch input{opacity:0;width:0;height:0}
.slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background-color:#ccc;transition:.4s;border-radius:28px}
.slider:before{position:absolute;content:"";height:20px;width:20px;left:4px;bottom:4px;background-color:white;transition:.4s;border-radius:50%}
input:checked+.slider{background-color:#28a745} input:checked+.slider:before{transform:translateX(22px)}
#skip_validation_group { display: none; }
.status-bar{text-align:center;margin-top:20px;padding:15px;border-radius:8px;font-size:14px;font-weight:500;background-color:#f8f9fa;color:#6c757d;}
.info-box { background-color: #fff3cd; color: #856404; padding: 15px; border-radius: 8px; border: 1px solid #ffeeba; margin-bottom: 20px; font-size: 14px; line-height: 1.5; }
.info-box i { vertical-align: text-bottom; margin-right: 5px; }
.serial-badge { font-family: monospace; font-weight: bold; background: #fff; padding: 2px 6px; border-radius: 4px; border: 1px solid #e2e3e5; margin-left: 5px; color: #333; font-size: 1.1em;}

/* Shared palette and accessible controls, embedded for offline device use. */
:root{color-scheme:light}body{background:#f2f5f7;color:#192f3d;line-height:1.5}
.container{border:1px solid #dfe8ed;border-radius:16px;box-shadow:0 12px 40px #173c490b;overflow:hidden}
.header{background:linear-gradient(115deg,#102c3b,#194655);color:white}
.header h2{letter-spacing:-.5px;color:#f8fafc}.settings-item:hover{background:#eef7f5}
.settings-item .icon{color:#147d78}.btn-primary,button[type=submit]{background-color:#147d78;color:white}
input,select,button,.btn{font:inherit}input:not([type=checkbox]),select{min-height:44px;border-radius:8px}
input[type=checkbox]{accent-color:#147d78}button,.btn{min-height:44px}
a:focus-visible,button:focus-visible,input:focus-visible,select:focus-visible{outline:3px solid #42a8bd;outline-offset:3px}
@media(max-width:480px){.container{margin:16px auto;max-width:calc(100% - 24px)}.settings-item{padding:16px}}
@media(prefers-reduced-motion:reduce){*{transition:none!important;scroll-behavior:auto!important}}
</style>
</head><body><div class="container"><div class="header"><h2>MQTT Settings</h2></div>
)rawliteral");

    // 3. Dynamic Info Box (Serial Number)
    server.sendContent(R"rawliteral(
<div class="info-box">
    <strong><i data-lucide="alert-triangle"></i> Security Requirement:</strong><br>
    All MQTT commands sent to this device <u>MUST</u> include the correct <b>serial_number</b> field.<br>
    <strong>Target Device Serial Number:</strong> <span class="serial-badge">)rawliteral" + sensor_serial + R"rawliteral(</span>
</div>
<form onsubmit="return saveMqtt()">
)rawliteral");

    // 4. Form Controls (Toggles)
    // We send these as static HTML; the Javascript at the bottom will check/uncheck them
    server.sendContent(R"rawliteral(
<div class="form-group inline"><label for="enabled" style="margin-bottom:0;">Enable MQTT Client</label><label class="switch"><input type="checkbox" id="enabled" name="enabled"><span class="slider"></span></label></div>
<div class="form-group inline"><label for="tls" style="margin-bottom:0;">Use TLS/SSL Security</label><label class="switch"><input type="checkbox" id="tls" name="tls"><span class="slider"></span></label></div>
<div class="form-group inline" id="skip_validation_group"><label for="skip_val" style="margin-bottom:0;">Skip Certificate Validation</label><label class="switch"><input type="checkbox" id="skip_val" name="skip_val"><span class="slider"></span></label></div>
<div class="form-group inline"><label for="progress" style="margin-bottom:0;">Publish Progress Updates</label><label class="switch"><input type="checkbox" id="progress" name="progress"><span class="slider"></span></label></div>
)rawliteral");

    // 5. Form Inputs (Pre-filled with Variables)
    // We break this into smaller chunks to allow variable insertion
    server.sendContent(R"rawliteral(<div class="form-group"><label for="server">Broker Address</label><input type="text" id="server" name="server" value=")rawliteral" + mqtt_server_str + R"rawliteral(" required></div>)rawliteral");
    
    server.sendContent(R"rawliteral(<div class="form-group"><label for="port">Port</label><input type="number" id="port" name="port" value=")rawliteral" + String(mqtt_port_val) + R"rawliteral(" required></div>)rawliteral");
    
    server.sendContent(R"rawliteral(<div class="form-group"><label for="user">Username</label><input type="text" id="user" name="user" value=")rawliteral" + mqtt_user_str + R"rawliteral("></div>)rawliteral");
    
    server.sendContent(R"rawliteral(<div class="form-group"><label for="pass">Password</label><input type="password" id="pass" name="pass" value=")rawliteral" + mqtt_pass_str + R"rawliteral("></div>)rawliteral");
    
    server.sendContent(R"rawliteral(<div class="form-group"><label for="topic">Base Topic</label><input type="text" id="topic" name="topic" value=")rawliteral" + mqtt_topic_str + R"rawliteral(" required></div>)rawliteral");

    // 6. Footer & Scripts (Dynamic JS Injection for Checkboxes)
    server.sendContent(R"rawliteral(
<button type="submit" class="btn-primary">Save and Reboot</button></form>
<div class="status-bar" id="mqtt-status">Loading status...</div>
<a href="/settings" class="nav-link">&larr; Back to Settings</a></div>
<script>
if (typeof lucide !== 'undefined') { lucide.createIcons(); }
function saveMqtt(){
    const enabled = document.getElementById('enabled').checked;
    const tls = document.getElementById('tls').checked;
    const skip_val = document.getElementById('skip_val').checked;
    const progress = document.getElementById('progress').checked;
    const server = document.getElementById('server').value;
    const port = document.getElementById('port').value;
    const user = document.getElementById('user').value;
    const pass = document.getElementById('pass').value;
    const topic = document.getElementById('topic').value;
    alert('Saving MQTT settings... The device will reboot.');
    const url = `/save_mqtt?enabled=${enabled}&tls=${tls}&skip_val=${skip_val}&progress=${progress}&server=${encodeURIComponent(server)}&port=${port}&user=${encodeURIComponent(user)}&pass=${encodeURIComponent(pass)}&topic=${encodeURIComponent(topic)}`;
    fetch(url).then(() => { window.location.href = '/settings'; });
    return false;
}
function updateTlsOptions() {
    const tlsEnabled = document.getElementById('tls').checked;
    document.getElementById('skip_validation_group').style.display = tlsEnabled ? 'flex' : 'none';
    const portInput = document.getElementById('port');
    if (tlsEnabled && (portInput.value == '1883' || portInput.value == '')) {
        portInput.value = '8883';
    } else if (!tlsEnabled && (portInput.value == '8883' || portInput.value == '')) {
        portInput.value = '1883';
    }
}
function updateMqttStatus() {
    fetch('/mqtt_status').then(r => r.json()).then(data => { document.getElementById('mqtt-status').textContent = data.status; });
}
window.onload = function() {
    document.getElementById('enabled').checked = )rawliteral" + String(mqttEnabled ? "true" : "false") + R"rawliteral(;
    document.getElementById('tls').checked = )rawliteral" + String(mqtt_use_tls ? "true" : "false") + R"rawliteral(;
    document.getElementById('skip_val').checked = )rawliteral" + String(mqtt_skip_cert_validation ? "true" : "false") + R"rawliteral(;
    document.getElementById('progress').checked = )rawliteral" + String(mqtt_publish_progress ? "true" : "false") + R"rawliteral(;
    document.getElementById('tls').addEventListener('change', updateTlsOptions);
    updateTlsOptions();
    updateMqttStatus();
    setInterval(updateMqttStatus, 2000);
}
</script></body></html>
)rawliteral");

    // 7. Close Stream
    server.sendContent("");
}

void handleSaveMqtt() {
    preferences.begin("mqtt-cfg", false);
    preferences.putBool("enabled", (server.arg("enabled") == "true"));
    preferences.putBool("tls", (server.arg("tls") == "true"));
    preferences.putBool("skip_val", (server.arg("skip_val") == "true"));
    preferences.putBool("progress", (server.arg("progress") == "true")); 
    preferences.putString("server", server.arg("server"));
    preferences.putInt("port", server.arg("port").toInt());
    preferences.putString("user", server.arg("user"));
    preferences.putString("pass", server.arg("pass"));
    preferences.putString("topic", server.arg("topic"));
    preferences.end();
    server.send(200, "text/plain", "MQTT settings saved. Device will reboot.");
    delay(1000);
    ESP.restart();
}

void handleRs485Settings() {
    // Streamed in pieces: building the whole page as one String needs a large contiguous
    // heap block, which the original ESP32 may not have.
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");
    server.sendContent_P(R"rawliteral(
<!DOCTYPE html><html><head><title>Modbus RS485 Settings</title><meta name="viewport" content="width=device-width, initial-scale=1"><style>body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;margin:0;background-color:#f4f6f8;display:flex;justify-content:center;align-items:center;min-height:100vh}.container{width:100%;max-width:500px;padding:20px;box-sizing:border-box;background-color:white;border-radius:12px;box-shadow:0 8px 20px rgba(0,0,0,.1)}.header{text-align:center;padding-bottom:20px;border-bottom:2px solid #e9ecef;margin-bottom:20px}.header h2{margin:0;font-size:24px;font-weight:600;color:#2c3e50}.form-group{margin-bottom:15px}label{display:block;margin-bottom:8px;font-size:14px;font-weight:600}input[type=number],select{width:100%;padding:12px;box-sizing:border-box;border:1px solid #ced4da;border-radius:8px;font-size:16px;transition:border-color .3s}input[type=number]:focus,select:focus{outline:0;border-color:#007bff}.btn-primary{width:100%;padding:12px;background-color:#007bff;color:white;border:none;border-radius:8px;cursor:pointer;font-size:16px;font-weight:600;transition:background-color .3s}.btn-primary:hover{background-color:#0056b3}.status-bar{text-align:center;margin-top:20px;padding-top:20px;border-top:2px solid #e9ecef;font-size:14px;color:#6c757d;font-weight:500}.nav-link{display:block;text-align:center;margin-top:20px;color:#007bff;text-decoration:none;font-weight:600;font-size:14px}.nav-link:hover{text-decoration:underline}
/* Shared palette and accessible controls, embedded for offline device use. */
:root{color-scheme:light}body{background:#f2f5f7;color:#192f3d;line-height:1.5}
.container{border:1px solid #dfe8ed;border-radius:16px;box-shadow:0 12px 40px #173c490b;overflow:hidden}
.header{background:linear-gradient(115deg,#102c3b,#194655);color:white}
.header h2{letter-spacing:-.5px;color:#f8fafc}.settings-item:hover{background:#eef7f5}
.settings-item .icon{color:#147d78}.btn-primary,button[type=submit]{background-color:#147d78;color:white}
input,select,button,.btn{font:inherit}input:not([type=checkbox]),select{min-height:44px;border-radius:8px}
input[type=checkbox]{accent-color:#147d78}button,.btn{min-height:44px}
a:focus-visible,button:focus-visible,input:focus-visible,select:focus-visible{outline:3px solid #42a8bd;outline-offset:3px}
@media(max-width:480px){.container{margin:16px auto;max-width:calc(100% - 24px)}.settings-item{padding:16px}}
@media(prefers-reduced-motion:reduce){*{transition:none!important;scroll-behavior:auto!important}}
</style></head><body><div class="container"><div class="header"><h2>Modbus RS485 Settings</h2></div><form onsubmit="return saveRs485()"><div class="form-group"><label for="slave_id">Slave ID</label><input type="number" id="slave_id" name="slave_id" min="1" max="247" value=")rawliteral");
    sendPageText(String(modbus_slave_id_val));
    server.sendContent_P(R"rawliteral(" required></div><div class="form-group"><label for="baud_rate">Baud Rate</label><select id="baud_rate" name="baud_rate" required><option value="9600">9600</option><option value="19200">19200</option><option value="38400">38400</option><option value="57600">57600</option><option value="115200">115200</option></select></div><button type="submit" class="btn-primary">Save and Reboot</button></form><div class="status-bar" id="status-bar">Current settings are displayed above. Saving will reboot the device.</div><a href="/settings" class="nav-link">&larr; Back to Settings</a></div><script>
function saveRs485(){
    const slaveId=document.getElementById("slave_id").value;
    const baudRate=document.getElementById("baud_rate").value;
    const statusBar=document.getElementById("status-bar");
    statusBar.innerHTML="Saving RS485 settings... The device will reboot to apply changes.";
    fetch(`/save_rs485?slave_id=${slaveId}&baud_rate=${baudRate}`);
    return false;
}
window.onload=function(){document.getElementById("baud_rate").value=")rawliteral");
    sendPageText(String(modbus_baud_rate_val));
    server.sendContent_P(R"rawliteral("};</script></body></html>)rawliteral");
    server.sendContent("");
}

void handleSaveRs485() {
    long slaveId = server.arg("slave_id").toInt();
    long baud = server.arg("baud_rate").toInt();
    if (slaveId < 1 || slaveId > 247 || baud < 1200 || baud > 921600) {
        server.send(400, "text/plain", "Invalid slave ID or baud rate.");
        return;
    }
    preferences.begin("rs485-cfg", false);
    preferences.putUChar("slave_id", slaveId);
    preferences.putLong("baud_rate", server.arg("baud_rate").toInt()); 
    preferences.end();
    server.send(200, "text/plain", "RS485 settings saved. Device will reboot.");
    delay(1000);
    ESP.restart();
}

void handleSetIntervals() {
    long interval = server.arg("interval").toInt();
    if (interval < 10000) { server.send(400, "text/plain", "Interval must be >= 10000 ms."); return; }
    if(xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        dataCaptureInterval = interval;
        xSemaphoreGive(dataMutex);
    }
    preferences.begin("device-cfg", false);
    preferences.putLong("wf_interval", interval);
    preferences.end();
    server.send(200, "text/plain", "Interval set.");
}

void handleGetIntervals() {
    long interval = 0, metrics_interval = 0;
    if(xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        interval = dataCaptureInterval;
        metrics_interval = metricsReadInterval_ms;
        xSemaphoreGive(dataMutex);
    }
    server.send(200, "application/json", "{\"interval\":" + String(interval) + ", \"metrics_interval\":" + String(metrics_interval) + "}");
}

// Scans run in the background so the access point keeps serving the page, which polls:
// 202 while scanning, then 200 with the last result list. ?start=1 begins a fresh scan.
void handleScanWifi() {
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_FAILED || (server.hasArg("start") && n != WIFI_SCAN_RUNNING)) {
        WiFi.scanDelete();
        n = WiFi.scanNetworks(true);
    }
    if (n == WIFI_SCAN_RUNNING) { server.send(202, "application/json", "{\"scanning\":true}"); return; }
    String json = "[";
    if (n > 0) {
        for (int i = 0; i < n; ++i) {
            json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + WiFi.RSSI(i) + "}";
            if (i < n - 1) json += ",";
        }
    }
    json += "]";
    server.send(200, "application/json", json);
}

void handleGetMetricsLogData() {
    DynamicJsonDocument doc(4096); 
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        doc["is_recording"] = isMetricsRecording;
        doc["log_count"] = metricsLogCount;
        doc["max_log_size"] = METRICS_LOG_SIZE;
        JsonArray log = doc.createNestedArray("log");
        for (int i = 0; i < metricsLogCount; ++i) {
            if (metricsLog[i].is_valid) {
                JsonObject entry = log.createNestedObject();
                entry["timestamp"] = metricsLog[i].timestamp;
                JsonArray values = entry.createNestedArray("values");
                for(int j=0; j < METRICS_SNAPSHOT_SIZE; ++j) {
                    values.add(metricsLog[i].values[j]);
                }
            }
        }
        xSemaphoreGive(dataMutex);
    }
    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", output);
}

void handleMetricsLogControl() {
    String action = server.arg("action");
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (action == "start") {
            isMetricsRecording = true;
            metricsLogIndex = 0;
            metricsLogCount = 0;
            for(int i=0; i<METRICS_LOG_SIZE; ++i) metricsLog[i].is_valid = false;
        } else if (action == "stop") {
            isMetricsRecording = false;
        }
        xSemaphoreGive(dataMutex);
    }
    server.send(200, "text/plain", "OK");
}

void handleDownloadMetricsCsv() {
    String csv_header = "Timestamp,AccelRmsX,AccelRmsY,AccelRmsZ,AccelPeakX,AccelPeakY,AccelPeakZ,VeloRmsX,VeloRmsY,VeloRmsZ,VeloPeakX,VeloPeakY,VeloPeakZ,DisplRmsX,DisplRmsY,DisplRmsZ,DisplPeakX,DisplPeakY,DisplPeakZ,TruePeakX,TruePeakY,TruePeakZ,CrestFactorX,CrestFactorY,CrestFactorZ,StdDevX,StdDevY,StdDevZ,AccelRms2X,AccelRms2Y,AccelRms2Z,Temperature,AccelMetricRmsX,AccelMetricRmsY,AccelMetricRmsZ,AccelMetricPeakX,AccelMetricPeakY,AccelMetricPeakZ,AccelMetricRms2X,AccelMetricRms2Y,AccelMetricRms2Z,AccelMetricPeak2X,AccelMetricPeak2Y,AccelMetricPeak2Z,VeloWideRmsX,VeloWideRmsY,VeloWideRmsZ,VeloWidePeakX,VeloWidePeakY,VeloWidePeakZ,DisplLowRmsX,DisplLowRmsY,DisplLowRmsZ,DisplLowPeakX,DisplLowPeakY,DisplLowPeakZ\n";
    String csv_content = "";
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        for (int i = 0; i < metricsLogCount; i++) {
            if (metricsLog[i].is_valid) {
                csv_content += String(metricsLog[i].timestamp);
                for (int j = 0; j < METRICS_SNAPSHOT_SIZE; j++) {
                    csv_content += "," + String(metricsLog[i].values[j]);
                }
                csv_content += "\n";
            }
        }
        xSemaphoreGive(dataMutex);
    }
    server.sendHeader("Content-Disposition", "attachment; filename=metrics_log.csv");
    server.send(200, "text/csv", csv_header + csv_content);
}

void handleClearMetricsLog() {
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        metricsLogIndex = 0;
        metricsLogCount = 0;
        for(int i=0; i<METRICS_LOG_SIZE; ++i) metricsLog[i].is_valid = false;
        xSemaphoreGive(dataMutex);
    }
    server.send(200, "text/plain", "Metrics log cleared.");
}

void handleClearDynamicData() {
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        newWaveformAvailable = false;
        newSpectrumAvailable = false;
        
        if(modbusTcpEnabled) {
            setModbusTCPRegister(HREG_WAVEFORM_READY_STATUS, 0);
            setModbusTCPRegister(HREG_SPECTRUM_READY_STATUS, 0);
        }

        xSemaphoreGive(dataMutex);
    }
    server.send(200, "text/plain", "Cached data cleared.");
}

void handleReboot() {
    server.send(200, "text/plain", "Rebooting...");
    delay(1000);
    ESP.restart();
}

void handleNotFound() {
    server.send(404, "text/plain", "Not found");
}

void handleMqttStatus() {
    DynamicJsonDocument doc(256);
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        doc["status"] = mqttStatus;
        xSemaphoreGive(dataMutex);
    } else {
        doc["status"] = "Status unavailable (mutex locked)";
    }
    String payload;
    serializeJson(doc, payload);
    server.send(200, "application/json", payload);
}

void handleSetMetricsInterval() {
    long interval = server.arg("interval").toInt();
    if (interval >= 1000) {
        if(xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            metricsReadInterval_ms = interval;
            xSemaphoreGive(dataMutex);
        }
        preferences.begin("device-cfg", false);
        preferences.putLong("m_interval", interval);
        preferences.end();
        server.send(200, "text/plain", "Metrics interval updated.");
    } else {
        server.send(400, "text/plain", "Interval too short.");
    }
}

