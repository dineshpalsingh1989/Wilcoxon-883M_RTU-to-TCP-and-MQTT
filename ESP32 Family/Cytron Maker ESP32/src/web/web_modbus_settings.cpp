#include "core/modules.h"

void handleModbusTcpSettings() {
    String ipAddress = gw::netIP().toString();
    
    // Streamed in pieces: building the whole page as one String needs a large contiguous
    // heap block, which the original ESP32 may not have.
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");
    server.sendContent_P(R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Modbus TCP Server</title>
    <style>
        :root { --primary: #0056b3; --secondary: #6c757d; --bg: #f8f9fa; --border: #dee2e6; --green: #28a745; --red: #dc3545; }
        body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; background-color: var(--bg); color: #333; margin: 0; padding: 20px; }
        .container { max-width: 1000px; margin: 0 auto; background: white; padding: 30px; border-radius: 12px; box-shadow: 0 4px 20px rgba(0,0,0,0.05); }
        h1 { margin-top: 0; color: #2c3e50; border-bottom: 2px solid var(--border); padding-bottom: 15px; display: flex; justify-content: space-between; align-items: center; }
        .config-box { background: #e9ecef; padding: 20px; border-radius: 8px; margin-bottom: 30px; border-left: 5px solid var(--primary); }
        .form-row { display: flex; align-items: center; justify-content: space-between; margin-bottom: 15px; }
        .info-row { display: flex; gap: 20px; margin-bottom: 20px; font-family: monospace; font-size: 1.1em; background: #fff; padding: 10px; border-radius: 6px; }
        label { font-weight: 600; color: #495057; }
        input[type=number] { padding: 8px; border-radius: 4px; border: 1px solid #ced4da; width: 80px; }
        button { background: var(--primary); color: white; border: none; padding: 10px 20px; border-radius: 6px; cursor: pointer; font-weight: 600; }
        button:hover { background: #004494; }
        
        /* Toggle Switch */
        .switch { position: relative; display: inline-block; width: 50px; height: 26px; }
        .switch input { opacity: 0; width: 0; height: 0; }
        .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; transition: .4s; border-radius: 34px; }
        .slider:before { position: absolute; content: ""; height: 20px; width: 20px; left: 3px; bottom: 3px; background-color: white; transition: .4s; border-radius: 50%; }
        input:checked + .slider { background-color: var(--green); }
        input:checked + .slider:before { transform: translateX(24px); }

        /* Tables */
        table { width: 100%; border-collapse: collapse; margin-top: 10px; font-size: 0.9em; margin-bottom: 30px; }
        th, td { border: 1px solid var(--border); padding: 8px 12px; text-align: left; vertical-align: top; }
        th { background-color: #f1f3f5; font-weight: 700; position: sticky; top: 0; }
        
        .addr-badge { background: #495057; color: white; padding: 2px 6px; border-radius: 4px; font-family: monospace; font-size: 0.9em; }
        .live-val { font-family: monospace; font-weight: bold; color: var(--primary); }
        .section-header { background-color: #343a40; color: white; padding: 10px; font-size: 1.1em; border-radius: 6px 6px 0 0; margin-bottom: 0; }
        .section-desc { font-size: 0.9em; color: #666; margin-bottom: 10px; padding: 0 5px; }
        
        .badge { padding: 4px 8px; border-radius: 4px; color: white; font-size: 0.8em; font-weight: bold; }
        .bg-green { background-color: var(--green); }
        .bg-blue { background-color: #17a2b8; }
        .axis-header { background-color: #e2e6ea; font-weight: bold; text-align: center; }
        .source-map { font-size: 0.85em; color: #555; margin-top: 5px; line-height: 1.4; border-top: 1px dashed #ccc; padding-top: 4px; }

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
</head>
<body>
<div class="container">
    <h1>Modbus TCP Server</h1>
    
    <div class="config-box">
        <div class="info-row">
            <span><strong>IP Address:</strong> <span style="color:var(--primary)">)rawliteral");
    sendPageText(ipAddress);
    server.sendContent_P(R"rawliteral(</span></span>
            <span><strong>Status:</strong> <span id="server-status" style="font-weight:bold;">)rawliteral");
    sendPageText(String(modbusTcpEnabled ? "RUNNING" : "STOPPED"));
    server.sendContent_P(R"rawliteral(</span></span>
            <span><strong>Slave ID:</strong> 1</span>
        </div>
        
        <form onsubmit="return saveSettings()">
            <div class="form-row">
                <label for="enabled">Enable Server</label>
                <label class="switch"><input type="checkbox" id="enabled" name="enabled"><span class="slider"></span></label>
            </div>
            <div class="form-row">
                <label for="port">Port (Default: 502)</label>
                <input type="number" id="port" name="port" min="1" max="65535" required>
            </div>
            <button type="submit">Save Configuration & Reboot</button>
        </form>
    </div>

    <div class="section-header">Live Metrics Map (Read Only)</div>
    <p class="section-desc">Registers 0-60 update in real-time based on sensor polling.</p>
    <table>
        <thead><tr><th>Address</th><th>Metric Name</th><th>Unit</th><th>Live Value</th></tr></thead>
        <tbody id="live-table-body"></tbody>
    </table>

    <div class="section-header">Section 1: Control & Status (Read/Write)</div>
    <p class="section-desc">Trigger captures and check status.</p>
    <table>
        <thead><tr><th>Addr</th><th>Name</th><th>R/W</th><th>Description</th></tr></thead>
        <tbody>
            <tr>
                <td><span class="addr-badge">90</span></td>
                <td>Waveform Status</td>
                <td>R</td>
                <td>0=Busy, 2=X Ready, 3=Y Ready, 4=Z Ready</td>
            </tr>
            <tr>
                <td><span class="addr-badge">91</span></td>
                <td>Spectrum Status</td>
                <td>R</td>
                <td>0=Busy, 2=X Ready, 3=Y Ready, 4=Z Ready</td>
            </tr>
            <tr>
                <td><span class="addr-badge">92</span></td>
                <td>Trigger Waveform</td>
                <td>W</td>
                <td>Write 1=XYZ, 2=X, 3=Y or 4=Z. Starts waveform and spectrum capture.</td>
            </tr>
            <tr>
                <td><span class="addr-badge">93</span></td>
                <td>Trigger Spectrum</td>
                <td>W</td>
                <td>Write 1=XYZ, 2=X, 3=Y or 4=Z. Starts waveform and spectrum capture.</td>
            </tr>
        </tbody>
    </table>

    <div class="section-header">Section 2: Time Waveform Data (Read Only)</div>
    <p class="section-desc"><strong>Range:</strong> 1000 to 14333. <strong>Type:</strong> Signed Int16 (x 0.001 g). Data updates when Status (90) changes.</p>
    <table>
        <thead><tr><th>Block #</th><th>Start</th><th>End</th><th>Length</th><th>Description</th></tr></thead>
        <tbody>
            <tr><td>1</td><td><span class="addr-badge">1000</span></td><td>1099</td><td>100</td><td>Points 1 - 100</td></tr>
            <tr><td>2</td><td><span class="addr-badge">1100</span></td><td>1199</td><td>100</td><td>Points 101 - 200</td></tr>
            <tr><td>...</td><td>...</td><td>...</td><td>...</td><td>...</td></tr>
            <tr><td>134</td><td><span class="addr-badge">14300</span></td><td>14333</td><td>34</td><td>Points 13301 - 13334</td></tr>
        </tbody>
    </table>

    <div class="section-header">Section 3: FFT Spectrum Data (Read Only)</div>
    <p class="section-desc"><strong>Range:</strong> 20000 to 26144. <strong>Type:</strong> Unsigned Int16 (x 0.001 g). Data updates when Status (91) changes.</p>
    <table>
        <thead><tr><th>Block #</th><th>Start</th><th>End</th><th>Length</th><th>Description</th></tr></thead>
        <tbody>
            <tr><td>1</td><td><span class="addr-badge">20000</span></td><td>20099</td><td>100</td><td>Points 1 - 100</td></tr>
            <tr><td>2</td><td><span class="addr-badge">20100</span></td><td>20199</td><td>100</td><td>Points 101 - 200</td></tr>
            <tr><td>...</td><td>...</td><td>...</td><td>...</td><td>...</td></tr>
            <tr><td>62</td><td><span class="addr-badge">26100</span></td><td>26144</td><td>45</td><td>Points 6101 - 6145</td></tr>
        </tbody>
    </table>

    <div class="config-box" style="border-left-color: #28a745;">
        <h3>Example Usage (PLC / Node-RED)</h3>
        <p><strong>Scenario: Manual X-Axis Capture</strong></p>
        <ol>
            <li><strong>Write:</strong> Value <code>2</code> to Address <code>92</code> (or <code>93</code>) to capture X. Use <code>1</code> for XYZ, <code>3</code> for Y or <code>4</code> for Z.</li>
            <li><strong>Poll:</strong> Read Addresses <code>90</code> and <code>91</code>. They return <code>0</code> while busy and <code>2</code> (X), <code>3</code> (Y) or <code>4</code> (Z) when that axis is ready.</li>
            <li><strong>Read:</strong> Read Address <code>1000</code> for waveform data or <code>20000</code> for spectrum data. Either trigger downloads both datasets.</li>
        </ol>
        <p>Trigger registers clear after processing. Triggers are ignored while the gateway is busy or when a value other than 1–4 is written. Write to only one trigger register at a time.</p>
    </div>

    <div style="margin-top:30px; text-align:center;">
        <a href="/settings" style="display:inline-block; padding:10px 20px; background:#6c757d; color:white; text-decoration:none; border-radius:6px; font-weight:600;">&larr; Back to Settings</a>
    </div>
</div>

<script>
    const liveRegisters=[
        // X-Axis
        {addr:0,name:"X Accel RMS (2-1k)",unit:"g",key:"accelRms",axis:"x"}, 
        {addr:1,name:"X Accel Peak (2-1k)",unit:"g",key:"accelPeak",axis:"x"},
        {addr:2,name:"X Accel RMS (10-5k)",unit:"g",key:"accelRms2",axis:"x"}, 
        {addr:3,name:"X Accel Peak (10-5k)",unit:"g",key:"accelPeak2",axis:"x"},
        {addr:4,name:"X Vel RMS (10-1k)",unit:"mm/s",key:"veloRms",axis:"x"}, 
        {addr:5,name:"X Vel Peak (10-1k)",unit:"mm/s",key:"veloPeak",axis:"x"},
        {addr:6,name:"X Disp RMS (10-1k)",unit:"um",key:"displRms",axis:"x"}, 
        {addr:7,name:"X Disp Peak (10-1k)",unit:"um",key:"displPeak",axis:"x"},
        {addr:8,name:"X True Peak",unit:"g",key:"truePeak",axis:"x"}, 
        {addr:9,name:"X Crest Factor",unit:"-",key:"crestFactor",axis:"x"},
        {addr:10,name:"X Accel Metric RMS",unit:"m/s²",key:"accelMetricRms",axis:"x"},
        {addr:14,name:"X Vel Wide RMS",unit:"mm/s",key:"veloWideRms",axis:"x"},
        {addr:16,name:"X Disp Low RMS",unit:"um",key:"displLowRms",axis:"x"},

        // Y-Axis
        {addr:20,name:"Y Accel RMS (2-1k)",unit:"g",key:"accelRms",axis:"y"}, 
        {addr:21,name:"Y Accel Peak (2-1k)",unit:"g",key:"accelPeak",axis:"y"},
        {addr:22,name:"Y Accel RMS (10-5k)",unit:"g",key:"accelRms2",axis:"y"},
        {addr:24,name:"Y Vel RMS (10-1k)",unit:"mm/s",key:"veloRms",axis:"y"},
        {addr:28,name:"Y True Peak",unit:"g",key:"truePeak",axis:"y"},
        {addr:29,name:"Y Crest Factor",unit:"-",key:"crestFactor",axis:"y"},

        // Z-Axis
        {addr:40,name:"Z Accel RMS (2-1k)",unit:"g",key:"accelRms",axis:"z"}, 
        {addr:41,name:"Z Accel Peak (2-1k)",unit:"g",key:"accelPeak",axis:"z"},
        {addr:42,name:"Z Accel RMS (10-5k)",unit:"g",key:"accelRms2",axis:"z"},
        {addr:44,name:"Z Vel RMS (10-1k)",unit:"mm/s",key:"veloRms",axis:"z"},
        {addr:48,name:"Z True Peak",unit:"g",key:"truePeak",axis:"z"},
        {addr:49,name:"Z Crest Factor",unit:"-",key:"crestFactor",axis:"z"},
        
        // System
        {addr:60,name:"Temperature",unit:"C",key:"temperature",axis:null},
        {addr:90,name:"Waveform Ready",unit:"Bool",key:"newWaveformAvailable",axis:null},
        {addr:91,name:"Spectrum Ready",unit:"Bool",key:"newSpectrumAvailable",axis:null}
    ];

    function generateLiveTable(){
        let h=''; 
        liveRegisters.forEach(r=>{
            let id = `val-${r.key}${r.axis ? '-'+r.axis : ''}`;
            // Add spacing rows for visual separation of axes
            if(r.addr === 20) h += '<tr class="axis-header"><td colspan="4">Y-AXIS</td></tr>';
            if(r.addr === 40) h += '<tr class="axis-header"><td colspan="4">Z-AXIS</td></tr>';
            if(r.addr === 60) h += '<tr class="axis-header"><td colspan="4">SYSTEM</td></tr>';
            
            h+=`<tr>
                <td><span class="addr-badge">${r.addr}</span></td>
                <td>${r.name}</td>
                <td>${r.unit}</td>
                <td class="live-val" id="${id}">--</td>
            </tr>`
        });
        document.getElementById('live-table-body').innerHTML=h;
    }

    function updateLiveValues(){
        fetch('/data').then(r=>r.json()).then(d=>{
            liveRegisters.forEach(r=>{
                let v = (r.axis) ? d[r.key][r.axis] : d[r.key];
                let el = document.getElementById(`val-${r.key}${r.axis ? '-'+r.axis : ''}`);
                if(el) {
                    if(typeof v === 'boolean') el.textContent = v ? "1 (YES)" : "0 (NO)";
                    else if(typeof v === 'number') el.textContent = v.toFixed(3);
                    else el.textContent = "--";
                }
            });
        });
    }

    function saveSettings(){
        const e = document.getElementById('enabled').checked;
        const p = document.getElementById('port').value;
        if(confirm("Save settings and reboot device?")) {
            window.location.href=`/save_modbustcp?enabled=${e}&port=${p}`;
        }
        return false;
    }

    window.onload=function(){
        document.getElementById('enabled').checked = )rawliteral");
    sendPageText(String(modbusTcpEnabled ? "true" : "false"));
    server.sendContent_P(R"rawliteral(;
        document.getElementById('port').value = )rawliteral");
    sendPageText(String(modbusTcpPort));
    server.sendContent_P(R"rawliteral(;
        
        generateLiveTable();
        setInterval(updateLiveValues, 2000); // Poll every 2s
        updateLiveValues();
    };
</script>
</body>
</html>
)rawliteral");
    server.sendContent("");
}

void handleSaveModbusTcp() {
    preferences.begin("modbustcp-cfg", false); 
    bool enabled = (server.arg("enabled") == "true");
    uint16_t port = server.arg("port").toInt();
    preferences.putBool("enabled", enabled);
    preferences.putUShort("port", port);
    preferences.end();
    server.send(200, "text/plain", "Settings saved. Rebooting now...");
    delay(1000);
    gw::restart();
}
