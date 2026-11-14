#include "core/modules.h"
#include <W5500SecureMqttVersion.h>
#include <WilcoxonMqttVersion.h>
#include <IRIVIOCVersion.h> // version only; the library itself is not used by this firmware

#if GATEWAY_ETHERNET
// "Our Libraries" page: live state of the project's own libraries (Wilcoxon883MRTU,
// Wilcoxon883MTCP, W5500SecureMqtt and the LittleFS settings store) plus one-shot tests
// that show the exact error a library returned.

namespace {
bool gatewayBusy() {
    return firmwareUpdateActive || currentDataState != DD_IDLE || isReadingWaveform ||
           isReadingSpectrum || isPublishingCapture;
}

void sendLibraryJson(JsonDocument& doc, int code = 200) {
    String payload;
    serializeJson(doc, payload);
    server.send(code, "application/json", payload);
}

// RTU results carry the library's own text, plus the Modbus exception code when there is one.
void addRtuResult(JsonDocument& doc, Wilcoxon883MRTU::Result result, uint32_t started) {
    doc["ok"] = result == Wilcoxon883MRTU::Result::Ok;
    doc["result"] = Wilcoxon883MRTU::resultText(result);
    if (result == Wilcoxon883MRTU::Result::Exception) doc["exception"] = sensorRtu.exceptionCode();
    doc["ms"] = millis() - started;
}
}

void handleLibraries() {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");
    server.sendContent(R"rawliteral(
<!DOCTYPE html><html lang="en"><head><title>Our Libraries</title><meta name="viewport" content="width=device-width, initial-scale=1">
<style>
:root{color-scheme:light;--ink:#192f3d;--muted:#5b6b75;--line:#e6edf1;--brand:#147d78;--ok:#1d7a45;--ok-bg:#e6f4ec;--bad:#b3261e;--bad-bg:#fbe9e7;--idle-bg:#eef2f4}
*{box-sizing:border-box}body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;margin:0;background:#f2f5f7;color:var(--ink);line-height:1.5;padding:20px 16px}
.container{max-width:680px;margin:0 auto}
.header{background:linear-gradient(115deg,#102c3b,#194655);color:#f8fafc;border-radius:16px;padding:22px 20px;text-align:center}
.header h2{margin:0;font-size:24px;letter-spacing:-.5px}.header p{margin:4px 0 0;font-size:14px;color:#cfe3ea}
.card{background:white;border:1px solid #dfe8ed;border-radius:14px;box-shadow:0 8px 28px #173c490a;margin-top:14px;padding:16px 18px}
.card-head{display:flex;align-items:center;gap:8px;flex-wrap:wrap}.card-head h3{margin:0;font-size:17px;flex:1 1 auto}
.ver{font:12px/1.6 monospace;background:var(--idle-bg);color:var(--muted);border-radius:6px;padding:1px 7px}
.pill{font-size:12px;font-weight:600;border-radius:999px;padding:2px 10px;background:var(--idle-bg);color:var(--muted)}.pill.ok{background:var(--ok-bg);color:var(--ok)}.pill.bad{background:var(--bad-bg);color:var(--bad)}
.lib-note{margin:4px 0 10px;font-size:13px;color:var(--muted)}
table{width:100%;border-collapse:collapse;font-size:14px}td{padding:6px 0;border-bottom:1px solid var(--line);vertical-align:top;overflow-wrap:anywhere}tr:last-child td{border-bottom:none}td:first-child{color:var(--muted);width:44%;padding-right:10px}
.ok{color:var(--ok);font-weight:600}.bad{color:var(--bad);font-weight:600}
.actions{display:flex;flex-wrap:wrap;gap:8px;margin-top:12px}
button{font:inherit;min-height:44px;padding:8px 14px;border:none;border-radius:8px;background:var(--brand);color:white;font-weight:600;cursor:pointer}button.secondary{background:var(--idle-bg);color:var(--ink)}button:disabled{opacity:.55;cursor:not-allowed}
.result{margin-top:10px;padding:10px 12px;border-radius:8px;background:#f8f9fa;border-left:4px solid #c5d1d8;font:13px/1.5 monospace;white-space:pre-wrap;word-break:break-word}.result:empty{display:none}.result.ok,.result.bad{font-weight:400;color:var(--ink)}.result.ok{border-left-color:var(--ok)}.result.bad{border-left-color:var(--bad)}
.notice{margin:14px 0 0;padding:10px 14px;border-radius:10px;background:#fff3cd;color:#6b5200;font-size:14px}.notice:empty{display:none}
.footer{display:flex;justify-content:space-between;align-items:center;gap:10px;flex-wrap:wrap;margin-top:16px}
.nav-link{color:var(--brand);font-weight:600;text-decoration:none;padding:10px 0}
a:focus-visible,button:focus-visible{outline:3px solid #42a8bd;outline-offset:3px}
@media(max-width:480px){body{padding:12px}.card{padding:14px}td:first-child{width:50%}.actions button{flex:1 1 100%}}
@media(prefers-reduced-motion:reduce){*{transition:none!important}}
</style></head><body><div class="container">
<div class="header"><h2>Our Libraries</h2><p>Versions, live state and one-click tests for the gateway's own libraries</p></div>
<p class="notice" id="busy-note" role="status"></p>
<section class="card" aria-labelledby="ver-title"><div class="card-head"><h3 id="ver-title">Versions</h3></div>
<p class="lib-note">Built into this firmware. Include these when reporting a problem.</p>
<table><tbody id="versions"><tr><td>Loading...</td><td></td></tr></tbody></table></section>
<section class="card" aria-labelledby="rtu-title"><div class="card-head"><h3 id="rtu-title">Wilcoxon883MRTU</h3><span class="ver" id="rtu-version">--</span><span class="pill" id="rtu-pill">--</span></div>
<p class="lib-note">RS485 Modbus RTU client for the 883M sensor. Each test sends one real request to the sensor.</p>
<table><tbody><tr><td>Baud rate / slave ID</td><td id="rtu-config">--</td></tr><tr><td>Sensor link</td><td id="rtu-state">--</td></tr>
<tr><td>Reads / failures / timeouts</td><td id="rtu-counts">--</td></tr><tr><td>Last error code</td><td id="rtu-error">--</td></tr></tbody></table>
<div class="actions"><button id="test-rtu" onclick="runTest('rtu')">Test read (0x0000-0x0002)</button><button id="test-identity" onclick="runTest('identity')">Read identity</button></div>
<div class="result" id="result-rtu" role="status" aria-live="polite"></div><div class="result" id="result-identity" role="status" aria-live="polite"></div></section>
<section class="card" aria-labelledby="tcp-title"><div class="card-head"><h3 id="tcp-title">Wilcoxon883MTCP</h3><span class="ver" id="tcp-version">--</span><span class="pill" id="tcp-pill">--</span></div>
<p class="lib-note">Modbus TCP server for PLCs. Counters run since the last restart.</p>
<table><tbody><tr><td>Enabled / port</td><td id="tcp-config">--</td></tr><tr><td>Server</td><td id="tcp-state">--</td></tr>
<tr><td>Requests / exceptions</td><td id="tcp-requests">--</td></tr><tr><td>Malformed / timed out / rejected / other unit</td><td id="tcp-errors">--</td></tr></tbody></table></section>
<section class="card" aria-labelledby="mqtt-title"><div class="card-head"><h3 id="mqtt-title">W5500SecureMqtt / WilcoxonMqtt</h3><span class="ver" id="mqtt-version">--</span><span class="pill" id="mqtt-pill">--</span></div>
<p class="lib-note">MQTT client. Plain MQTT uses WilcoxonMqtt directly; TLS wraps it in W5500SecureMqtt. A test connects if needed, sends one message to &lt;base topic&gt;/test and watches the connection for 2 s.</p>
<table><tbody><tr><td>Enabled / mode</td><td id="mqtt-config">--</td></tr><tr><td>Broker</td><td id="mqtt-broker">--</td></tr>
<tr><td>Base topic</td><td id="mqtt-topic">--</td></tr><tr><td>Library state</td><td id="mqtt-state">--</td></tr><tr><td>Gateway status</td><td id="mqtt-status">--</td></tr>
<tr><td>Connections / drops (TLS)</td><td id="mqtt-sessions">--</td></tr><tr><td>Last session length / closed with</td><td id="mqtt-lastdrop">--</td></tr>
<tr><td>Last TLS error</td><td id="mqtt-tls">--</td></tr><tr><td>MQTT client state</td><td id="mqtt-code">--</td></tr></tbody></table>
<div class="actions"><button id="test-mqtt_small" onclick="runTest('mqtt_small')">Send small test (100 B)</button><button id="test-mqtt_large" onclick="runTest('mqtt_large')">Send large test (3000 B)</button></div>
<div class="result" id="result-mqtt_small" role="status" aria-live="polite"></div><div class="result" id="result-mqtt_large" role="status" aria-live="polite"></div></section>
<section class="card" aria-labelledby="fs-title"><div class="card-head"><h3 id="fs-title">Settings storage (LittleFS)</h3><span class="pill" id="fs-pill">Not tested</span></div>
<p class="lib-note">Writes a test value to its own file under /prefs and reads it back. Gateway settings are not touched.</p>
<div class="actions"><button id="test-storage" onclick="runTest('storage')">Test write and read back</button></div>
<div class="result" id="result-storage" role="status" aria-live="polite"></div></section>
<div class="footer"><a href="/settings" class="nav-link">&larr; Back to Settings</a><button class="secondary" id="copy-report" onclick="copyReport()">Copy diagnostics</button></div>
</div>
<script>
const mqttStates={'-4':'connection timeout','-3':'connection lost','-2':'connect failed','-1':'disconnected','0':'connected','1':'bad protocol','2':'bad client ID','3':'broker unavailable','4':'bad username or password','5':'not authorised'};
const tests=['rtu','identity','storage','mqtt_small','mqtt_large'];
const set=(id,text)=>{document.getElementById(id).textContent=text;};
function mark(id,ok,text){const el=document.getElementById(id);el.textContent=text;el.className=ok?'ok':'bad';}
function pill(id,state,text){const el=document.getElementById(id);el.textContent=text;el.className='pill'+(state===true?' ok':state===false?' bad':'');}
let versionsShown=false;
function showVersions(rows){
  const body=document.getElementById('versions');body.textContent='';
  rows.forEach(([name,value])=>{const tr=body.insertRow();tr.insertCell().textContent=name;tr.insertCell().textContent=value;});
  versionsShown=true;
}
function refresh(){
  fetch('/lib_status',{cache:'no-store'}).then(r=>{if(!r.ok)throw 0;return r.json();}).then(d=>{
    if(!versionsShown&&d.versions)showVersions(d.versions);
    set('rtu-version','v'+d.rtu.version);set('tcp-version','v'+d.rtu.version);
    set('mqtt-version',d.mqtt.tls?'v'+d.mqtt.library_version:'plain MQTT');
    set('rtu-config',d.rtu.baud+' baud / ID '+d.rtu.slave_id);
    set('tcp-config',(d.tcp.enabled?'Enabled':'Disabled')+' / '+d.tcp.port);
    mark('tcp-state',d.tcp.running,d.tcp.running?'Listening, '+d.tcp.clients+' client(s)':'Not running');
    pill('tcp-pill',d.tcp.enabled?d.tcp.running:null,!d.tcp.enabled?'Disabled':d.tcp.running?'Running':'Not running');
    set('tcp-requests',d.tcp.requests+' / '+d.tcp.exceptions);
    set('tcp-errors',d.tcp.malformed+' / '+d.tcp.timeouts+' / '+d.tcp.rejected+' / '+d.tcp.ignored);
    set('mqtt-config',(d.mqtt.enabled?'Enabled':'Disabled')+' / '+(d.mqtt.tls?(d.mqtt.skip_validation?'TLS, certificate unchecked':'TLS, verified'):'plain'));
    set('mqtt-broker',d.mqtt.server+':'+d.mqtt.port);
    mark('mqtt-topic',d.mqtt.topic_valid,d.mqtt.base_topic+(d.mqtt.topic_valid?'':' (invalid: # and + are only for subscribing)'));
    mark('mqtt-state',d.mqtt.connected,d.mqtt.state||'--');set('mqtt-status',d.mqtt.status);
    pill('mqtt-pill',d.mqtt.enabled?d.mqtt.connected:null,!d.mqtt.enabled?'Disabled':d.mqtt.connected?'Connected':'Not connected');
    set('mqtt-tls',d.mqtt.tls?(d.mqtt.tls_error?d.mqtt.tls_error+': '+(d.mqtt.tls_error_text||''):'none'):'not used');
    set('mqtt-sessions',d.mqtt.tls?d.mqtt.connects+' / '+d.mqtt.drops:'not used');
    set('mqtt-lastdrop',d.mqtt.tls&&d.mqtt.drops?d.mqtt.last_session_s+' s / state '+d.mqtt.last_drop_state+' ('+(mqttStates[d.mqtt.last_drop_state]||'unknown')+')':'--');
    set('mqtt-code',d.mqtt.mqtt_state+' ('+(mqttStates[d.mqtt.mqtt_state]||'unknown')+')');
    set('busy-note',d.busy?'A capture, MQTT send or firmware update is running. Tests are paused until it finishes.':'');
    tests.forEach(n=>{const b=document.getElementById('test-'+n);if(!b.dataset.running)b.disabled=d.busy;});
  }).catch(()=>set('busy-note','Gateway unavailable or restarting.'));
  fetch('/monitoring',{cache:'no-store'}).then(r=>{if(!r.ok)throw 0;return r.json();}).then(m=>{
    const h=m.health;mark('rtu-state',h.state==='Connected',h.state);
    pill('rtu-pill',h.state==='Connected'?true:(h.has_reading||h.failures)?false:null,h.state);
    set('rtu-counts',h.reads+' / '+h.failures+' / '+h.timeouts);set('rtu-error',h.last_error?String(h.last_error):'none');
  }).catch(()=>{});
}
function runTest(name){
  const target=document.getElementById('result-'+name);
  const button=document.getElementById('test-'+name);
  button.disabled=true;button.dataset.running='1';target.className='result';target.textContent='Running...';
  fetch('/lib_test?test='+name,{method:'POST'}).then(r=>r.text().then(t=>{
    let d;try{d=JSON.parse(t);}catch(e){target.className='result bad';target.textContent='FAIL - '+t;return;}
    const lines=[(d.ok?'PASS':'FAIL')+' - '+d.result+' ('+d.ms+' ms)'];
    if(d.exception!==undefined)lines.push('Modbus exception code: '+d.exception);
    if(d.registers)lines.push('Raw registers: '+d.registers.join(', '));
    if(d.identity)Object.entries(d.identity).forEach(([k,v])=>lines.push(k+': '+v));
    if(d.details)Object.entries(d.details).forEach(([k,v])=>lines.push(k+': '+v));
    target.className='result '+(d.ok?'ok':'bad');target.textContent=lines.join('\n');
    if(name==='storage')pill('fs-pill',!!d.ok,d.ok?'Passed':'Failed');
  })).catch(()=>{target.className='result bad';target.textContent='FAIL - gateway unavailable';}).finally(()=>{delete button.dataset.running;button.disabled=false;refresh();});
}
function copyReport(){
  const lines=['Our Libraries report - '+new Date().toISOString()];
  document.querySelectorAll('section.card').forEach(card=>{
    const tags=[...card.querySelectorAll('.card-head .ver,.card-head .pill')].map(e=>'['+e.textContent+']').join(' ');
    lines.push('','== '+card.querySelector('h3').textContent+(tags?' '+tags:''));
    card.querySelectorAll('tr').forEach(tr=>lines.push([...tr.cells].map(c=>c.textContent).join(': ')));
    card.querySelectorAll('.result').forEach(r=>{if(r.textContent)lines.push(r.textContent);});
  });
  const text=lines.join('\n'),button=document.getElementById('copy-report');
  const done=ok=>{button.textContent=ok?'Copied':'Copy failed';setTimeout(()=>{button.textContent='Copy diagnostics';},2000);};
  if(navigator.clipboard&&window.isSecureContext){navigator.clipboard.writeText(text).then(()=>done(true),()=>done(false));return;}
  const area=document.createElement('textarea');area.value=text;document.body.appendChild(area);area.select();
  let ok=false;try{ok=document.execCommand('copy');}catch(e){}area.remove();done(ok);
}
document.addEventListener('DOMContentLoaded',()=>{refresh();setInterval(refresh,5000);});
</script></body></html>)rawliteral");
    server.sendContent("");
}

void handleLibraryStatus() {
    DynamicJsonDocument doc(3072);
    doc["busy"] = gatewayBusy();
    JsonArray versions = doc.createNestedArray("versions");
    auto addVersion = [&](const char* name, const char* value) {
        JsonArray row = versions.createNestedArray();
        row.add(name);
        row.add(value);
    };
    addVersion("Gateway firmware", FIRMWARE_VERSION);
    addVersion("Built", __DATE__ " " __TIME__); // tells two builds with the same version apart
    addVersion("Board", GATEWAY_BOARD);
    addVersion("Wilcoxon883MRTU / Wilcoxon883MTCP", WILCOXON883M_LIBRARY_VERSION);
    addVersion("WilcoxonMqtt", WILCOXONMQTT_VERSION);
    addVersion("W5500SecureMqtt", W5500SECUREMQTT_VERSION);
    addVersion("IRIVIOCHardware", IRIVIOCHARDWARE_VERSION " (separate library, not built into this firmware)");
    addVersion("ArduinoJson", ARDUINOJSON_VERSION);
    addVersion("Arduino-Pico core", ARDUINO_PICO_VERSION_STR);
    JsonObject rtu = doc.createNestedObject("rtu");
    rtu["version"] = WILCOXON883M_LIBRARY_VERSION;
    rtu["baud"] = modbus_baud_rate_val;
    rtu["slave_id"] = modbus_slave_id_val;
    JsonObject tcp = doc.createNestedObject("tcp");
    tcp["enabled"] = modbusTcpEnabled;
    tcp["port"] = modbusTcpPort;
    addModbusTcpLibraryStatus(tcp);
    JsonObject mqtt = doc.createNestedObject("mqtt");
    mqtt["enabled"] = mqttEnabled;
    mqtt["tls"] = mqtt_use_tls;
    mqtt["skip_validation"] = mqtt_skip_cert_validation;
    mqtt["server"] = mqtt_server_str;
    mqtt["port"] = mqtt_port_val;
    mqtt["connected"] = mqttClient && mqttClient->connected();
    mqtt["mqtt_state"] = mqttClient ? mqttClient->state() : -1;
    mqtt["status"] = mqttStatus;
    mqtt["base_topic"] = mqtt_topic_str;
    mqtt["topic_valid"] = mqtt_topic_str.length() && mqtt_topic_str.indexOf('#') < 0 && mqtt_topic_str.indexOf('+') < 0;
    mqtt["library_version"] = W5500SECUREMQTT_VERSION;
    if (mqtt_use_tls) addSecureMqttLibraryStatus(mqtt);
    else mqtt["state"] = !mqttEnabled ? "Disabled" : !mqttClient ? "Not configured" : mqttClient->connected() ? "Connected" : "Not connected";
    sendLibraryJson(doc);
}

void handleLibraryTest() {
    const String test = server.arg("test");
    // The RTU client and the sensor belong to the capture while it runs.
    if (gatewayBusy()) { server.send(409, "text/plain", "Device busy with a capture, MQTT send or update; try again shortly"); return; }
    DynamicJsonDocument doc(2048);
    const uint32_t started = millis();
    if (test == "rtu") {
        uint16_t registers[3] = {};
        const Wilcoxon883MRTU::Result result = sensorRtu.readInputRegisters(0x0000, 3, registers, 3, 1000);
        addRtuResult(doc, result, started);
        if (result == Wilcoxon883MRTU::Result::Ok) {
            JsonArray values = doc.createNestedArray("registers");
            for (uint16_t value : registers) values.add(value);
        }
    } else if (test == "identity") {
        Wilcoxon883MRTU::Identity identity;
        const Wilcoxon883MRTU::Result result = sensorRtu.readIdentity(identity, 1000);
        addRtuResult(doc, result, started);
        if (result == Wilcoxon883MRTU::Result::Ok) {
            JsonObject out = doc.createNestedObject("identity");
            out["Product code"] = identity.product;
            out["Revision"] = identity.revision;
            out["Product name"] = identity.productName;
            out["Model"] = identity.model;
            out["Vendor URL"] = identity.vendorUrl;
        }
    } else if (test == "storage") {
        Preferences store;
        const uint32_t probe = millis();
        bool opened = store.begin("lib-test", false);
        const bool wrote = opened && store.putULong("probe", probe) == 4;
        store.end();
        opened = opened && store.begin("lib-test", true);
        const bool readBack = opened && store.getULong("probe", 0) == probe;
        store.end();
        doc["ok"] = wrote && readBack;
        doc["result"] = !opened ? "Storage busy or not mounted" : !wrote ? "Write failed" : !readBack ? "Read-back did not match" : "Wrote and read back";
        doc["ms"] = millis() - started;
    } else if (test == "mqtt_small" || test == "mqtt_large") {
        // Connects itself if needed (the gateway may only stay connected for a moment), sends one
        // non-retained message and keeps servicing the session for 2 s, so a broker or TLS problem
        // that closes the connection shows up here. Nothing else runs on the IRIV during the test.
        if (!mqttEnabled || !mqttClient) { server.send(409, "text/plain", "MQTT is disabled or not configured in MQTT settings"); return; }
        bool connectedByTest = false;
        if (!mqttClient->connected()) {
            const bool up = mqtt_use_tls ? connectSecureMqttNow()
                : mqttClient->connect(("esp32-ila-" + gw::deviceId()).c_str(), mqtt_user_str.c_str(), mqtt_pass_str.c_str());
            if (!up) {
                doc["ok"] = false;
                doc["result"] = "Could not connect to the broker for the test";
                doc["ms"] = millis() - started;
                JsonObject out = doc.createNestedObject("details");
                out["MQTT client state"] = mqttClient->state();
                if (mqtt_use_tls) addSecureMqttLibraryStatus(out);
                logMessage("[Libraries] Test " + test + ": could not connect, state " + String(mqttClient->state()));
                sendLibraryJson(doc);
                return;
            }
            connectedByTest = true;
        }
        const bool topicValid = mqtt_topic_str.length() && mqtt_topic_str.indexOf('#') < 0 && mqtt_topic_str.indexOf('+') < 0;
        const String topic = (topicValid ? mqtt_topic_str : String("883M")) + "/test";
        const size_t size = test == "mqtt_large" ? 3000 : 100;
        String payload = "{\"type\":\"library_test\",\"bytes\":" + String(size) + ",\"fill\":\"";
        while (payload.length() < size - 2) payload += 'x';
        payload += "\"}";
        const bool sent = mqttClient->publish(topic.c_str(), (const uint8_t*)payload.c_str(), payload.length(), false);
        const uint32_t waitStart = millis();
        while (millis() - waitStart < 2000 && mqttClient->connected()) { mqttClient->loop(); delay(20); }
        const bool stillConnected = mqttClient->connected();
        doc["ok"] = sent && stillConnected;
        doc["result"] = !sent ? "Publish refused before sending (not connected or message larger than the MQTT buffer)"
                      : stillConnected ? "Sent and still connected after 2 s"
                      : "Sent, then the connection closed within 2 s";
        doc["ms"] = millis() - started;
        JsonObject out = doc.createNestedObject("details");
        out["Topic"] = topic;
        out["Message size"] = String(payload.length()) + " bytes";
        out["MQTT client state"] = mqttClient->state();
        out["Connected by this test"] = connectedByTest ? "yes" : "no (already connected)";
        if (!topicValid) out["Note"] = "Base topic contains # or +; sent to 883M/test instead. Fix the Base Topic in MQTT settings.";
    } else {
        server.send(400, "text/plain", "Unknown test; use rtu, identity, storage, mqtt_small or mqtt_large"); return;
    }
    logMessage("[Libraries] Test " + test + ": " + String(doc["result"].as<const char*>()));
    sendLibraryJson(doc);
}
#endif
