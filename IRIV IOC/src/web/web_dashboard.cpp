#include "core/modules.h"

// =================================================================
// --- WEB SERVER HANDLERS (PART 2) - DASHBOARD & SETTINGS
// =================================================================

void handleRoot() {
    // Mostly static HTML; the browser fills in the data via Javascript fetching /data.
    // Streamed in pieces: building the whole page as one String needs a large contiguous
    // heap block, which the original ESP32 may not have.
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");
    server.sendContent_P(R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <title>Sensor Dashboard | )rawliteral");
    sendPageText(sensor_model);
    server.sendContent_P(R"rawliteral(</title>
    <meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1">
    <script src="https://unpkg.com/lucide@latest"></script>
    <style>
        :root { --primary-color: #007bff; --light-gray: #f8f9fa; --medium-gray: #e9ecef; --dark-gray: #495057; --text-color: #212529; --success-green: #28a745; --inactive-gray: #adb5bd; }
        body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;margin:0;background-color:#f4f6f8; color: var(--text-color);}
        
        .header{background-color:#2c3e50;color:white;padding:10px 20px;display:flex;justify-content:space-between;align-items:center;box-shadow:0 2px 4px rgba(0,0,0,.1)}
        .title-container{display:flex;flex-direction:column;align-items:flex-start} .title{font-size:22px;font-weight:600} .subtitle{font-size:12px;color:#bdc3c7;font-weight:400}
        .header .menu a{color:white;text-decoration:none;font-size:15px;padding:8px 14px;border-radius:20px;margin-left:10px;border:1px solid #4a627a; display:inline-flex; align-items:center; gap: 6px;}
        .header .time-container { text-align: center; }
        .header .time{font-size:14px;color:#bdc3c7}
        
        .main-content{padding:20px; max-width: 1200px; margin: 0 auto;}
        .tabs{display:flex;border-bottom:2px solid var(--medium-gray)}
        .tab-button{background:0 0;border:none;padding:15px 20px;font-size:16px;cursor:pointer;color:#6c757d;border-bottom:2px solid transparent}
        .tab-button.active{color:var(--primary-color);border-bottom:2px solid var(--primary-color);font-weight:700}
        .tab-content{display:none;padding-top:20px}.tab-content.active{display:block}
        
        /* --- STATUS BOXES GRID --- */
        .status-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 15px; margin-bottom: 20px; }
        .status-box { 
            background: white; border: 2px solid var(--medium-gray); border-radius: 10px; padding: 20px; text-align: center; 
            transition: all 0.3s ease; opacity: 0.6; box-shadow: 0 2px 4px rgba(0,0,0,0.05);
        }
        .status-box .box-icon { margin-bottom: 10px; color: var(--inactive-gray); }
        .status-box .box-label { font-size: 16px; font-weight: 700; color: var(--dark-gray); }
        .status-box .box-value { font-size: 12px; color: #666; font-family: monospace; font-weight: 500; margin-top: 5px;}
        
        .status-box.active { 
            border-color: var(--success-green); background-color: #f0fff4; opacity: 1; transform: translateY(-3px);
            box-shadow: 0 8px 16px rgba(40, 167, 69, 0.15);
        }
        .status-box.active .box-icon { color: var(--success-green); }
        .status-box.active .box-value { color: var(--success-green); font-weight: bold; background: white; padding: 2px 6px; border-radius: 4px; border: 1px solid #c3e6cb;}

        /* --- DETAILED TABLE STYLES --- */
        #metrics-table{width:100%;border-collapse:collapse;background-color:white;border-radius:8px;box-shadow:0 4px 8px rgba(0,0,0,.05);overflow:hidden;font-size:14px;}
        #metrics-table th, #metrics-table td{padding:10px 14px;border-bottom:1px solid var(--medium-gray);}
        #metrics-table thead th{background-color:var(--light-gray);font-weight:600;color:var(--dark-gray);text-align:left;}
        #metrics-table thead th:not(:first-child){text-align:right;}
        #metrics-table tbody td:not(:first-child){text-align:right;font-weight:600;color:var(--primary-color);font-family:monospace;font-size:15px;}
        #metrics-table tbody tr:nth-child(even){background-color:var(--light-gray);}
        #metrics-table .group-header th {background-color:#e9ecef;font-weight:bold;color:#343a40;padding:8px 14px; text-transform: uppercase; font-size: 12px;}
        
        .actions-panel{background-color:white;padding:20px;border-radius:8px;box-shadow:0 4px 8px rgba(0,0,0,.05);margin-bottom:20px}
        .actions-panel h3 {margin-top:0; display:flex; align-items:center; gap: 8px; font-size: 18px;} 
        .actions-panel p {margin-top:5px; margin-bottom: 15px; color:#6c757d; font-size: 14px;}
        .actions-panel button, .actions-panel select{padding:8px 12px;font-size:14px;border:1px solid #ccc;border-radius:5px; cursor: pointer; margin-right: 10px;}
        button.btn-special { background-color: #28a745; color: white; border-color: #28a745; }
        button:disabled { opacity: 0.6; cursor: not-allowed; }
        
        /* --- TRANSMISSION TAB STYLES --- */
        .trans-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; }
        .trans-card { background: white; border-radius: 8px; box-shadow: 0 4px 8px rgba(0,0,0,.05); overflow: hidden; }
        .trans-header { background: #343a40; color: white; padding: 12px 15px; font-weight: 600; display: flex; align-items: center; justify-content: space-between; }
        .trans-body { padding: 20px; }
        .status-row { display: flex; justify-content: space-between; border-bottom: 1px solid #eee; padding: 8px 0; font-size: 14px; }
        .status-row:last-child { border-bottom: none; }
        .status-val { font-weight: 600; font-family: monospace; color: #333; }
        .json-preview { background: #2d3436; color: #55efc4; padding: 15px; border-radius: 6px; font-family: 'Courier New', monospace; font-size: 12px; height: 300px; overflow-y: auto; white-space: pre-wrap; margin-top: 15px; }
        .reg-table { width: 100%; border-collapse: collapse; font-size: 13px; margin-top: 10px; }
        .reg-table th { background: #f1f3f5; padding: 8px; text-align: left; border-bottom: 2px solid #ddd; }
        .reg-table td { padding: 6px 8px; border-bottom: 1px solid #eee; font-family: monospace; }
        
        #progressBarContainer{width:100%;background-color:var(--medium-gray);border-radius:8px;overflow:hidden;margin:15px 0;display:none}
        #progressBar{width:0%;height:24px;background-color:var(--primary-color);text-align:center;line-height:24px;color:white;font-weight:bold;transition:width .5s ease; white-space:nowrap;}
        .footer{text-align:center;font-size:12px;color:#888;margin-top:20px;}
        .badge { padding: 4px 8px; border-radius: 4px; color: white; font-size: 12px; font-weight: bold; }
        
        #toast-notification { visibility: hidden; min-width: 250px; background-color: #333; color: #fff; text-align: center; border-radius: 8px; padding: 16px; position: fixed; z-index: 10; left: 50%; transform: translateX(-50%); bottom: 30px; font-size: 16px; opacity: 0; transition: opacity 0.5s; }
        #toast-notification.show { visibility: visible; opacity: 1; }
        /* Compact, self-contained monitoring interface for the device web server. */
        :root{--primary-color:#147d78;--text-color:#192f3d;--medium-gray:#e2e9ed;--light-gray:#f6f9fa;--success-green:#14826d}
        *{box-sizing:border-box}
        body{background:#f2f5f7;font-size:14px;line-height:1.5}
        .header{background:#122c3a;padding:20px max(24px,calc((100vw - 1200px)/2));box-shadow:none;gap:20px}
        .title{font-size:20px;letter-spacing:-.5px}.subtitle{color:#adc3ce}
        .header .menu a{border-radius:8px;border-color:#48616e;margin:0;font-size:13px}
        .header .menu a:hover{background:#284653}.header .time{font-variant-numeric:tabular-nums;color:#d7e6ec}
        .main-content{padding:30px 24px 24px;max-width:1248px}
        .overview-heading{display:flex;justify-content:space-between;align-items:center;gap:16px;margin-bottom:22px}
        .eyebrow{font-size:11px;font-weight:700;letter-spacing:1.7px;text-transform:uppercase;color:#64808f;margin-bottom:5px}
        h1{font-size:28px;letter-spacing:-1px;line-height:1.2;margin:0 0 6px}.overview-heading p{margin:0;color:#667e8c}
        .connection{display:inline-flex;align-items:center;gap:8px;background:#e6eef2;color:#506774;padding:7px 12px;border-radius:30px;font-size:12px;font-weight:600;white-space:nowrap}
        .connection:before{content:'';height:7px;width:7px;border-radius:50%;background:currentColor}.connection.live{color:#11745f;background:#e1f2eb}.connection.offline{color:#a34536;background:#fce9e4}
        .summary-grid{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:16px;margin-bottom:26px}
        .summary-card{background:white;border:1px solid var(--medium-gray);border-radius:12px;padding:20px;border-top:3px solid #299b93}
        .summary-card:nth-child(2){border-top-color:#5688c3}.summary-card:nth-child(3){border-top-color:#9680c5}.summary-card:nth-child(4){border-top-color:#d8a45d}
        .summary-label{color:#637c8b;font-size:12px;font-weight:600;display:flex;justify-content:space-between}.summary-label span{font-size:10px;letter-spacing:1px}
        .summary-value{font-size:32px;font-weight:650;letter-spacing:-1px;margin:10px 0 3px;font-variant-numeric:tabular-nums;line-height:1.2}.summary-value small{font-size:13px;font-weight:400;color:#718591;letter-spacing:0}
        .summary-note{font-size:11px;color:#7b8e99}
        .tabs{gap:24px;border-bottom:1px solid #d8e3e9}.tab-button{font-size:14px;padding:12px 0;min-height:44px}.tab-content{padding-top:22px}
        .actions-panel,.trans-card{border:1px solid var(--medium-gray);border-radius:12px;box-shadow:0 2px 4px #18344203}
        .actions-panel{padding:22px}.actions-panel h3{font-size:16px}.actions-panel button,.actions-panel select{min-height:40px;border:1px solid #d3dfe5;border-radius:7px;margin:0;background:white;color:#284653;font:inherit}
        .actions-panel button{display:inline-flex;align-items:center;gap:7px;font-weight:600}.actions-panel button:hover{background:#edf5f5;border-color:#8fbab6}button.btn-special{background:#147d78;color:white;border-color:#147d78}button.btn-special:hover{background:#0d655f}
        svg{width:18px;height:18px;vertical-align:middle}button:focus-visible,a:focus-visible,select:focus-visible{outline:3px solid #42a8bd;outline-offset:3px}
        .table-wrap{overflow-x:auto;border:1px solid var(--medium-gray);border-radius:12px;background:white}
        #metrics-table{box-shadow:none;border-radius:0;min-width:480px}#metrics-table th,#metrics-table td{padding:12px 18px}#metrics-table thead th{font-size:11px;text-transform:uppercase;letter-spacing:.8px;background:#fff;padding-top:17px;padding-bottom:17px}
        #metrics-table tbody td:not(:first-child){font-size:14px;font-variant-numeric:tabular-nums;color:#245367}#metrics-table .group-header th{background:#eaf1f4;color:#5b7483;font-size:10px;letter-spacing:1px;padding:9px 18px}#metrics-table tbody tr:hover{background:#edf6f5}
        .status-box{opacity:1;border-width:1px;box-shadow:none;background:#f8fafb}.status-box.active{transform:none;box-shadow:none;border-color:#83c4b2;background:#eef9f4}
        .trans-header{background:#edf3f6;color:#234253;padding:16px 20px}.trans-header>span:first-child{display:flex;align-items:center;gap:8px}.badge{font-size:10px;letter-spacing:.5px;border-radius:5px}
        .status-val{max-width:60%;overflow-wrap:anywhere;text-align:right}.json-preview{background:#132f3d;color:#9cdbcc;line-height:1.7}.reg-table{display:block;overflow-x:auto}
        .footer{display:flex;justify-content:space-between;gap:12px;text-align:left;padding:16px 0;color:#6b8290}.footer p{margin:0}
        #toast-notification{font-size:13px;max-width:calc(100vw - 32px);min-width:0;background:#173c49;box-shadow:0 8px 32px #122c3a30}
        #progressBarContainer{background:#dcece8}#progressBar{font-size:12px;min-width:fit-content;padding:0 8px}
        @media(max-width:760px){.summary-grid{grid-template-columns:repeat(2,minmax(0,1fr));gap:12px}.trans-grid{grid-template-columns:1fr}.header{padding:16px 20px}.time-container{display:none}.main-content{padding:22px 16px}.overview-heading{align-items:flex-start;flex-direction:column}h1{font-size:25px}.actions-panel[style]{flex-wrap:wrap;gap:14px}.footer{flex-direction:column}.summary-card{padding:16px}.summary-value{font-size:28px}.status-grid{gap:8px}.status-box{padding:12px 6px}.status-box .box-label{font-size:13px}.status-box .box-value{font-size:10px}}
        @media(prefers-reduced-motion:reduce){*{transition:none!important}}
        .monitor-grid{display:grid;grid-template-columns:1fr 1fr;gap:16px;margin:20px 0}.monitor-card{background:white;border:1px solid var(--medium-gray);border-radius:12px;padding:20px;min-width:0}.monitor-card h2{font-size:16px;margin:0 0 12px}.monitor-card p{color:#637c8b}.monitor-card canvas{width:100%;height:200px;display:block}.monitor-card label{display:block;margin:10px 0 4px}.monitor-card input{max-width:100%;padding:8px;border:1px solid #cbd8df;border-radius:6px}.monitor-card button{padding:9px 14px;background:#147d78;color:white;border:0;border-radius:6px;cursor:pointer}.monitor-card .legend{font-size:12px;color:#637c8b}.monitor-card table{width:100%;border-collapse:collapse;font-size:12px}.monitor-card td,.monitor-card th{padding:10px 6px;text-align:left;border-bottom:1px solid #e2e9ed}.full-width{grid-column:1/-1}#alarm-state{font-weight:600}.alarm-active{color:#b03f32!important}.health-details{font-variant-numeric:tabular-nums}.monitor-card .history-wrap{overflow:auto}@media(max-width:760px){.monitor-grid{grid-template-columns:1fr}}

        /* Local visual polish: no additional network requests or UI dependencies. */
        :root{--surface-shadow:0 8px 28px #173c4908}
        body{background:radial-gradient(ellipse at top left,#e5f1f1 0,transparent 48%),#f2f5f7}
        .header{background:linear-gradient(115deg,#102c3b,#194655);border-bottom:1px solid #315765}
        .title-container{border-left:3px solid #5fd1bc;padding-left:14px}
        .overview-heading{padding:8px 0}.eyebrow{color:#486c7a}
        .summary-card,.monitor-card,.actions-panel,.trans-card{box-shadow:var(--surface-shadow)}
        .summary-card{position:relative;overflow:hidden}
        .summary-label span{display:grid;place-items:center;min-width:28px;height:24px;padding:0 6px;border-radius:7px;background:#eef5f5;color:#426676}
        .summary-value{margin-top:16px}.summary-note{color:#617783}
        .tabs{gap:8px;padding:5px;background:#e7eef1;border:1px solid #dbe5ea;border-radius:12px}
        .tab-button{flex:1;padding:10px 14px;border-radius:8px;border-bottom:0;font-weight:600;color:#4c6776}
        .tab-button.active{border-bottom:0;background:white;color:#116d67;box-shadow:0 2px 7px #173c4912}
        .monitor-card h2{letter-spacing:-.25px}.monitor-card small{color:#526d7b}
        .monitor-card summary{cursor:pointer;padding:10px 0;color:#245367;min-height:44px}
        .monitor-card input:not([type=checkbox]){width:100%;min-height:42px;background:#f8fbfc;font:inherit;color:var(--text-color)}
        .monitor-card input[type=checkbox]{width:18px;height:18px;accent-color:#147d78;vertical-align:middle}
        .monitor-card button{min-height:42px;font:inherit;font-weight:600}.monitor-card button:hover{background:#0d655f}
        .monitor-card button:disabled{opacity:.6;cursor:wait}
        input:focus-visible,summary:focus-visible,.table-wrap:focus-visible{outline:3px solid #42a8bd;outline-offset:3px}
        .monitor-card .legend{padding-top:10px;border-top:1px solid #edf2f5}
        .monitor-card.full-width{line-height:1.8}.monitor-card.full-width button{margin:4px 0}
        #sensor-health{display:inline-block;padding:5px 10px;background:#edf3f6;border-radius:7px}
        #alarm-state{padding:8px 0;min-height:40px}.alarm-active{border-left:3px solid currentColor;padding-left:10px!important}
        .status-row{gap:14px}.json-preview{scrollbar-width:thin}
        @media(max-width:480px){.header{padding:16px;gap:10px}.title{font-size:18px}.header .menu a{padding:8px 10px}.summary-card{padding:14px}.summary-value{font-size:27px}.summary-label{gap:4px}.monitor-card{padding:16px}.tab-button{font-size:12px;padding:10px 6px}.monitor-card.full-width button{display:block;width:100%}}
    </style>
</head>
<body>
    <div class="header">
        <div class="title-container">
            <div class="title">)rawliteral");
    sendPageText(sensor_model);
    server.sendContent_P(R"rawliteral(</div>
            <div class="subtitle">Serial: )rawliteral");
    sendPageText(sensor_serial);
    server.sendContent_P(R"rawliteral(</div>
        </div>
        <div class="time-container"><div class="time" id="clock">--:--:--</div></div>
        <div class="menu"><a href="/settings"><i data-lucide="settings"></i>Settings</a></div>
    </div>
    
    <div class="main-content">
        <div class="overview-heading">
            <div><div class="eyebrow">Condition monitoring / Live overview</div><h1>Vibration dashboard</h1><p>Monitor vibration across three axes and manage sensor captures.</p></div>
            <div id="connection-state" class="connection" role="status">Connecting</div>
        </div>
        <div class="summary-grid">
            <div class="summary-card"><div class="summary-label">X-axis velocity <span>X</span></div><div class="summary-value"><span id="summary-x">--</span> <small>mm/s</small></div><div class="summary-note">RMS &middot; 10 Hz &ndash; 1 kHz</div></div>
            <div class="summary-card"><div class="summary-label">Y-axis velocity <span>Y</span></div><div class="summary-value"><span id="summary-y">--</span> <small>mm/s</small></div><div class="summary-note">RMS &middot; 10 Hz &ndash; 1 kHz</div></div>
            <div class="summary-card"><div class="summary-label">Z-axis velocity <span>Z</span></div><div class="summary-value"><span id="summary-z">--</span> <small>mm/s</small></div><div class="summary-note">RMS &middot; 10 Hz &ndash; 1 kHz</div></div>
            <div class="summary-card"><div class="summary-label">Sensor temperature <span>TEMP</span></div><div class="summary-value"><span id="summary-temp">--</span> <small>&deg;C</small></div><div class="summary-note">Internal sensor reading</div></div>
        </div>
        <div class="tabs">
            <button class="tab-button active" onclick="openTab(event, 'Metrics')">Overall Metrics</button>
            <button class="tab-button" onclick="openTab(event, 'Transmission')">Control & Transmission</button>
        </div>

        <div id="Metrics" class="tab-content active">
             <div class="monitor-grid">
                <section class="monitor-card"><h2>Sensor communication</h2><strong id="sensor-health" role="status">Waiting for sensor health...</strong><p id="health-details" class="health-details">Reading RS485 diagnostics.</p><small>Metric read attempts and errors since gateway startup.</small></section>
                <section class="monitor-card"><h2>Machine alarms</h2><p id="alarm-state" role="status">Loading alarm status...</p><details><summary>Configure thresholds</summary><form id="alarm-form"><label><input type="checkbox" id="alarm-enabled"> Enable dashboard alarms</label><label for="alarm-velocity">Velocity RMS limit, any axis (mm/s)</label><input id="alarm-velocity" type="number" min="0.001" max="1000" step="0.001" required><label for="alarm-temperature">Temperature limit (&deg;C)</label><input id="alarm-temperature" type="number" min="-40" max="200" step="0.1" required><label for="alarm-delay">Sustained exceedance (seconds)</label><input id="alarm-delay" type="number" min="0" max="3600" step="1" required><p><button type="submit">Save alarm settings</button></p><small>Choose limits for your machine. These alarms do not control equipment or the buzzer.</small></form></details></section>
                <section class="monitor-card"><h2>Velocity RMS trend</h2><canvas id="velocity-chart" role="img" aria-label="Recent X, Y and Z velocity RMS in millimeters per second"></canvas><div class="legend">X: teal &nbsp; Y: blue &nbsp; Z: purple &middot; mm/s</div></section>
                <section class="monitor-card"><h2>Temperature trend</h2><canvas id="temperature-chart" role="img" aria-label="Recent sensor temperature in degrees Celsius"></canvas><div class="legend">Sensor temperature &middot; &deg;C</div></section>
                <section class="monitor-card full-width"><span id="trend-summary">Waiting for readings.</span> <button onclick="downloadTrends()">Export trend CSV</button><p>Last 120 successful metric snapshots. Gaps indicate interrupted readings; history resets on gateway reboot.</p></section>
             </div>
             <div id="progressBarContainer"><div id="progressBar">0%</div></div>
             
             <div class="actions-panel" style="display:flex; justify-content:space-between; align-items:center; padding:15px;">
                <div>
                    <label for="metrics_interval"><strong>Refresh every</strong></label>
                    <select id="metrics_interval" onchange="setMetricsInterval()">
                        <option value="1000">1 Second</option>
                        <option value="2000">2 Seconds (Default)</option>
                        <option value="5000">5 Seconds</option>
                    </select>
                </div>
                <div>
                    <button onclick="window.location.href='/download_metrics_csv'"><i data-lucide="download"></i> Download CSV Log</button>
                </div>
             </div>

             <div class="table-wrap" role="region" aria-label="Detailed axis measurements" tabindex="0"><table id="metrics-table">
                <thead><tr><th>Measurement</th><th>X-Axis</th><th>Y-Axis</th><th>Z-Axis</th></tr></thead>
                
                <tbody class="group-header"><tr><th colspan="4">Velocity (ISO 10-1k Hz)</th></tr></tbody>
                <tbody>
                    <tr><td>Peak (mm/s)</td><td id="veloPeakX">--</td><td id="veloPeakY">--</td><td id="veloPeakZ">--</td></tr>
                    <tr><td>RMS (mm/s)</td><td id="veloRmsX">--</td><td id="veloRmsY">--</td><td id="veloRmsZ">--</td></tr>
                </tbody>

                <tbody class="group-header"><tr><th colspan="4">Velocity Wide (10-5k Hz)</th></tr></tbody>
                <tbody>
                    <tr><td>Peak (mm/s)</td><td id="veloWidePeakX">--</td><td id="veloWidePeakY">--</td><td id="veloWidePeakZ">--</td></tr>
                    <tr><td>RMS (mm/s)</td><td id="veloWideRmsX">--</td><td id="veloWideRmsY">--</td><td id="veloWideRmsZ">--</td></tr>
                </tbody>

                <tbody class="group-header"><tr><th colspan="4">Acceleration (g)</th></tr></tbody>
                <tbody>
                    <tr><td>RMS (2-1k Hz)</td><td id="accelRmsX">--</td><td id="accelRmsY">--</td><td id="accelRmsZ">--</td></tr>
                    <tr><td>Peak (2-1k Hz)</td><td id="accelPeakX">--</td><td id="accelPeakY">--</td><td id="accelPeakZ">--</td></tr>
                    <tr><td>RMS (10-5k Hz)</td><td id="accelRms2X">--</td><td id="accelRms2Y">--</td><td id="accelRms2Z">--</td></tr>
                    <tr><td>Peak (10-5k Hz)</td><td id="accelPeak2X">--</td><td id="accelPeak2Y">--</td><td id="accelPeak2Z">--</td></tr>
                </tbody>

                <tbody class="group-header"><tr><th colspan="4">Acceleration Metric (m/s&sup2;)</th></tr></tbody>
                <tbody>
                    <tr><td>RMS (2-1k Hz)</td><td id="accelMetricRmsX">--</td><td id="accelMetricRmsY">--</td><td id="accelMetricRmsZ">--</td></tr>
                    <tr><td>Peak (2-1k Hz)</td><td id="accelMetricPeakX">--</td><td id="accelMetricPeakY">--</td><td id="accelMetricPeakZ">--</td></tr>
                    <tr><td>RMS (10-5k Hz)</td><td id="accelMetricRms2X">--</td><td id="accelMetricRms2Y">--</td><td id="accelMetricRms2Z">--</td></tr>
                    <tr><td>Peak (10-5k Hz)</td><td id="accelMetricPeak2X">--</td><td id="accelMetricPeak2Y">--</td><td id="accelMetricPeak2Z">--</td></tr>
                </tbody>

                <tbody class="group-header"><tr><th colspan="4">Displacement (&micro;m)</th></tr></tbody>
                <tbody>
                    <tr><td>RMS (10-1k Hz)</td><td id="displRmsX">--</td><td id="displRmsY">--</td><td id="displRmsZ">--</td></tr>
                    <tr><td>Peak (10-1k Hz)</td><td id="displPeakX">--</td><td id="displPeakY">--</td><td id="displPeakZ">--</td></tr>
                    <tr><td>RMS (2-1k Hz)</td><td id="displLowRmsX">--</td><td id="displLowRmsY">--</td><td id="displLowRmsZ">--</td></tr>
                    <tr><td>Peak (2-1k Hz)</td><td id="displLowPeakX">--</td><td id="displLowPeakY">--</td><td id="displLowPeakZ">--</td></tr>
                </tbody>
                
                <tbody class="group-header"><tr><th colspan="4">Overall & Temperature</th></tr></tbody>
                <tbody>
                    <tr><td>True Peak (g)</td><td id="truePeakX">--</td><td id="truePeakY">--</td><td id="truePeakZ">--</td></tr>
                    <tr><td>Crest Factor</td><td id="crestFactorX">--</td><td id="crestFactorY">--</td><td id="crestFactorZ">--</td></tr>
                    <tr><td>Std Deviation</td><td id="stdDeviationX">--</td><td id="stdDeviationY">--</td><td id="stdDeviationZ">--</td></tr>
                    <tr><td>Temperature (&deg;C)</td><td colspan="3" id="temperature-cell" style="text-align:right">--</td></tr>
                </tbody>
            </table></div>
        </div>

        <div id="Transmission" class="tab-content">
            <section class="monitor-card" style="margin-bottom:20px"><h2>Capture history</h2><p>Latest 20 completed or failed captures since startup. MQTT publish attempts do not confirm broker delivery.</p><button onclick="downloadCaptureHistory()">Export history CSV</button><div class="history-wrap"><table><thead><tr><th>Time (device local)</th><th>Axis</th><th>Result</th><th>Transmission</th></tr></thead><tbody id="capture-history"><tr><td colspan="4">No captures yet.</td></tr></tbody></table></div></section>
            
            <div class="actions-panel">
                <h3><i data-lucide="hard-drive"></i> Data Buffer Status</h3>
                <div class="status-grid">
                    <div id="box-x" class="status-box">
                        <div class="box-icon"><i data-lucide="activity"></i></div>
                        <div class="box-label">X-AXIS</div>
                        <div class="box-value">EMPTY</div>
                    </div>
                    <div id="box-y" class="status-box">
                        <div class="box-icon"><i data-lucide="activity"></i></div>
                        <div class="box-label">Y-AXIS</div>
                        <div class="box-value">EMPTY</div>
                    </div>
                    <div id="box-z" class="status-box">
                        <div class="box-icon"><i data-lucide="activity"></i></div>
                        <div class="box-label">Z-AXIS</div>
                        <div class="box-value">EMPTY</div>
                    </div>
                </div>
            </div>

            <div class="actions-panel">
                <h3><i data-lucide="scan-line"></i> Control Panel</h3>
                <p>Trigger a capture to populate the buffer and send via MQTT/Modbus.</p>
                <div style="display:flex; gap:15px; flex-wrap:wrap; align-items:center; background: #f8f9fa; padding: 15px; border-radius: 8px;">
                    <select id="manual_axis_select" style="min-width: 120px;">
                        <option value="X">X-Axis</option>
                        <option value="Y">Y-Axis</option>
                        <option value="Z">Z-Axis</option>
                    </select>
                    <button id="collectSingleBtn" onclick="triggerManualCapture()"><i data-lucide="scan-search"></i>Capture Selected</button>
                    <div style="width:1px; height:30px; background:#ccc; margin:0 10px;"></div>
                    <button id="collectAllBtn" class="btn-special" onclick="triggerAll()"><i data-lucide="scan"></i>Capture All (XYZ Cycle)</button>
                </div>
                
                <div style="margin-top: 15px; border-top: 1px dashed #ccc; padding-top: 10px;">
                    <label><strong>Auto-Capture Interval (MQTT):</strong></label>
                    <select id="data_interval" onchange="setIntervals()">
                        <option value="300000">5 Minutes</option>
                        <option value="600000">10 Minutes</option>
                        <option value="1800000">30 Minutes</option>
                        <option value="3600000">1 Hour</option>
                        <option value="2147483647">Never (Manual Only)</option>
                    </select>
                </div>
            </div>

            <div class="trans-grid">
                <div class="trans-card">
                    <div class="trans-header">
                        <span><i data-lucide="cloud"></i> MQTT Protocol</span>
                        <span id="mqtt-badge" class="badge" style="background:#dc3545;">Disconnected</span>
                    </div>
                    <div class="trans-body">
                        <div class="status-row"><span>Broker Address:</span><span class="status-val" id="mqtt-broker">--</span></div>
                        <div class="status-row"><span>Target Topic:</span><span class="status-val" id="mqtt-topic">--</span></div>
                        <div class="status-row"><span>Last Status:</span><span class="status-val" id="mqtt-msg">Waiting...</span></div>
                        
                        <div style="margin-top:15px; font-size:12px; font-weight:600; color:#666;">LIVE DATA PREVIEW (JSON):</div>
                        <div class="json-preview" id="mqtt-preview">Waiting for data...</div>
                    </div>
                </div>

                <div class="trans-card">
                    <div class="trans-header">
                        <span><i data-lucide="server"></i> Modbus TCP Server</span>
                        <span id="modbus-badge" class="badge" style="background:#ccc;">Checking...</span>
                    </div>
                    <div class="trans-body">
                        <div class="status-row"><span>Server Port:</span><span class="status-val" id="modbus-port">--</span></div>
                        <div class="status-row"><span>Slave ID:</span><span class="status-val" id="modbus-id">--</span></div>
                        <div class="status-row"><span>Holding Registers:</span><span class="status-val">Available</span></div>

                        <div style="margin-top:15px; font-size:12px; font-weight:600; color:#666;">REGISTER MAP GUIDE:</div>
                        <table class="reg-table">
                            <thead><tr><th>Address</th><th>Data Content</th><th>Scale</th></tr></thead>
                            <tbody>
                                <tr><td>0 - 60</td><td>Live Overall Metrics</td><td>Mixed</td></tr>
                                <tr><td>80 - 93</td><td>Control & Status Flags</td><td>Int16</td></tr>
                                <tr><td>1000+</td><td>Waveform Data Blocks</td><td>x0.001 g</td></tr>
                                <tr><td>20000+</td><td>Spectrum Data Blocks</td><td>x0.001 g</td></tr>
                            </tbody>
                        </table>
                    </div>
                </div>
            </div>
        </div>
    </div>
    <div class="footer"><span id="status">Status: Initializing...</span></div>
    <div id="toast-notification">Notification</div>
    
    <script>
        let dashboardUpdateInterval;
        let wasCapturing = false;
        let isFullCycle = false;

        function showToast(message) {
            const toast = document.getElementById("toast-notification");
            toast.textContent = message;
            toast.className = "show";
            setTimeout(() => { toast.className = toast.className.replace("show", ""); }, 3000);
        }

        function openTab(evt, tabName) {
            let tabcontent = document.getElementsByClassName("tab-content");
            for (let i = 0; i < tabcontent.length; i++) { tabcontent[i].style.display = "none"; }
            let tablinks = document.getElementsByClassName("tab-button");
            for (let i = 0; i < tablinks.length; i++) { tablinks[i].className = tablinks[i].className.replace(" active", ""); }
            document.getElementById(tabName).style.display = "block";
            evt.currentTarget.className += " active";
        }
        
        function updateClock() { document.getElementById('clock').textContent = new Date().toLocaleTimeString(); }

        function setMetricsInterval() {
            const interval = document.getElementById('metrics_interval').value;
            fetch(`/set_metrics_interval?interval=${interval}`).then(r => {
                if(r.ok) { 
                    showToast("Live update rate set to " + interval/1000 + "s"); 
                    clearInterval(dashboardUpdateInterval);
                    dashboardUpdateInterval = setInterval(updateDashboardData, interval);
                }
            });
        }

        function setIntervals() { 
            const val = document.getElementById('data_interval').value;
            fetch(`/set_intervals?interval=${val}`).then(() => showToast('Auto-Capture Interval Updated!')); 
        }

        function trigger(axis, isCycle = false) {
            fetch('/trigger_capture?axis=' + axis + '&cycle=' + isCycle)
                .then(async response => { if(!response.ok) throw Error(await response.text()); showToast('Capture accepted for ' + axis + '-axis'); updateDashboardData(); })
                .catch(error => { isFullCycle = false; showToast(error.message); });
        }
        
        function triggerManualCapture() {
            const selectedAxis = document.getElementById('manual_axis_select').value;
            isFullCycle = false;
            showToast(`Requesting ${selectedAxis}-Axis Capture...`);
            trigger(selectedAxis, false);
        }

        function triggerAll() {
            showToast('Starting XYZ Cycle...');
            isFullCycle = true;
            trigger('X', true); 
        }

        function updateStatusBoxes(capturedAxis) {
            ['x', 'y', 'z'].forEach(axis => {
                const box = document.getElementById('box-' + axis);
                if(box) {
                    box.classList.remove('active');
                    box.querySelector('.box-value').textContent = "EMPTY";
                }
            });
            if (capturedAxis && capturedAxis !== 'NONE') {
                const activeBox = document.getElementById('box-' + capturedAxis.toLowerCase());
                if (activeBox) {
                    activeBox.classList.add('active');
                    activeBox.querySelector('.box-value').textContent = "DATA IN MEMORY";
                }
            }
        }
        
        function updateDashboardData() {
            if (updateDashboardData.pending) return;
            updateDashboardData.pending = true;
            return fetch("/data")
            .then(r => { if (!r.ok) throw new Error('Device unavailable'); return r.json(); })
            .then(data => {
                if (!data.accelRms) throw new Error(data.status || 'Waiting for sensor data');
                const connection = document.getElementById('connection-state');
                connection.textContent = 'Live · ' + new Date().toLocaleTimeString();
                connection.className = 'connection live';
                ['x', 'y', 'z'].forEach(axis => {
                    const value = data.veloRms && data.veloRms[axis];
                    document.getElementById('summary-' + axis).textContent = Number.isFinite(value) ? value.toFixed(2) : '--';
                });
                document.getElementById('summary-temp').textContent = Number.isFinite(data.temperature) ? data.temperature.toFixed(1) : '--';
                const statusText = data.status || "Monitoring";
                const isCapturing = typeof data.capture_busy === 'boolean' ? data.capture_busy : /collecting|waiting|requesting|downloading|calculating/i.test(statusText);
                document.getElementById('collectSingleBtn').disabled = isCapturing;
                document.getElementById('collectAllBtn').disabled = isCapturing;
                
                document.getElementById('status').textContent = 'Status: ' + statusText;
                
                // Progress Bar
                const progressBarContainer = document.getElementById('progressBarContainer');
                const progressBar = document.getElementById('progressBar');
                if (isCapturing) {
                    progressBarContainer.style.display = 'block';
                    const matches = statusText.match(/\(([^,]+, )?(\d+)%\)/);
                    const percentage = matches ? parseInt(matches[2], 10) : 0;
                    progressBar.style.width = percentage + '%';
                    progressBar.textContent = statusText;
                } else {
                    progressBarContainer.style.display = 'none';
                }

                updateStatusBoxes(data.capturedAxis);

                // --- UPDATE METRICS TABLE (FULL LIST) ---
                try {
                    const metrics = [
                        'veloPeak', 'veloRms', 
                        'veloWidePeak', 'veloWideRms',
                        'accelPeak', 'accelRms', 
                        'accelPeak2', 'accelRms2', 
                        'accelMetricPeak', 'accelMetricRms',
                        'accelMetricPeak2', 'accelMetricRms2',
                        'displPeak', 'displRms',
                        'displLowPeak', 'displLowRms',
                        'truePeak', 'crestFactor', 'stdDeviation'
                    ];

                    if (data.accelRms) {
                        metrics.forEach(k => {
                            if(data[k]) {
                                const elX = document.getElementById(k+'X');
                                const elY = document.getElementById(k+'Y');
                                const elZ = document.getElementById(k+'Z');
                                if(elX) elX.textContent = data[k].x.toFixed(3);
                                if(elY) elY.textContent = data[k].y.toFixed(3);
                                if(elZ) elZ.textContent = data[k].z.toFixed(3);
                            }
                        });
                        document.getElementById('temperature-cell').textContent = data.temperature.toFixed(2);
                    }
                } catch(e) { console.error("Error updating table:", e); }

                // --- UPDATE TRANSMISSION TAB ---
                // MQTT
                const mqttBadge = document.getElementById('mqtt-badge');
                if(data.mqtt_connected) {
                    mqttBadge.textContent = "CONNECTED";
                    mqttBadge.style.background = "#28a745";
                } else {
                    mqttBadge.textContent = "DISCONNECTED";
                    mqttBadge.style.background = "#dc3545";
                }
                
                document.getElementById('mqtt-broker').textContent = data.mqtt_server || "Unknown";
                document.getElementById('mqtt-topic').textContent = data.mqtt_topic || "Unknown";
                document.getElementById('mqtt-msg').textContent = data.status;
                
                if(data.accelRms) {
                    const previewData = {
                        timestamp: new Date().toISOString(),
                        model: ")rawliteral");
    sendPageText(sensor_model);
    server.sendContent_P(R"rawliteral(",
                        type: "metrics",
                        data: {
                            accel: data.accelRms,
                            velo: data.veloRms,
                            temp: data.temperature
                        }
                    };
                    document.getElementById('mqtt-preview').textContent = JSON.stringify(previewData, null, 2);
                }

                // Modbus
                const mbBadge = document.getElementById('modbus-badge');
                if(data.modbus_enabled) {
                    mbBadge.textContent = "RUNNING";
                    mbBadge.style.background = "#28a745";
                } else {
                    mbBadge.textContent = "DISABLED";
                    mbBadge.style.background = "#6c757d";
                }
                document.getElementById('modbus-port').textContent = data.modbus_port || "502";
                document.getElementById('modbus-id').textContent = data.modbus_id || "1";

                // Cycle Logic
                if (wasCapturing && !isCapturing && isFullCycle && data.capturedAxis) {
                   if (!data.cycle_active) { isFullCycle = false; showToast(data.capturedAxis === 'Z' ? "XYZ Cycle Complete" : "Capture cycle stopped"); }
                }
                
                wasCapturing = isCapturing;
            })
            .catch(err => {
                const connection = document.getElementById('connection-state');
                connection.textContent = 'Updates unavailable · retrying';
                connection.className = 'connection offline';
                document.getElementById('status').textContent = 'Status: ' + err.message + '. Displayed readings may be stale.';
            })
            .finally(() => { updateDashboardData.pending = false; });
        }

        window.onload = function() {
            if (typeof lucide !== 'undefined') lucide.createIcons();
            
            // Get initial interval settings
            dashboardUpdateInterval = setInterval(updateDashboardData, 2000);
            document.getElementById('metrics_interval').value = '2000';
            fetch('/get_intervals').then(r => { if (!r.ok) throw new Error('Settings unavailable'); return r.json(); }).then(d => {
                document.getElementById('metrics_interval').value = d.metrics_interval;
                document.getElementById('data_interval').value = d.interval;
                clearInterval(dashboardUpdateInterval);
                dashboardUpdateInterval = setInterval(updateDashboardData, d.metrics_interval);
            }).catch(() => showToast('Using default refresh rate. Interval settings unavailable.'));

            updateClock();
            setInterval(updateClock, 1000);
            updateDashboardData();
        };
    </script>
<script src="/monitoring_ui.js"></script>
</body>
</html>)rawliteral");
    server.sendContent("");
}

void handleMetrics() {
    DynamicJsonDocument doc(4096); 
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        
        // --- 1. Populate All Metrics (Full Table Support) ---
        JsonObject accelRms = doc.createNestedObject("accelRms");
        accelRms["x"] = accelRmsX; accelRms["y"] = accelRmsY; accelRms["z"] = accelRmsZ;
        JsonObject accelPeak = doc.createNestedObject("accelPeak");
        accelPeak["x"] = accelPeakX; accelPeak["y"] = accelPeakY; accelPeak["z"] = accelPeakZ;
        JsonObject accelRms2 = doc.createNestedObject("accelRms2");
        accelRms2["x"] = accelRms2X; accelRms2["y"] = accelRms2Y; accelRms2["z"] = accelRms2Z;
        JsonObject accelPeak2 = doc.createNestedObject("accelPeak2");
        accelPeak2["x"] = accelPeak2X; accelPeak2["y"] = accelPeak2Y; accelPeak2["z"] = accelPeak2Z;
        
        JsonObject amRms = doc.createNestedObject("accelMetricRms");
        amRms["x"] = accelMetricRmsX; amRms["y"] = accelMetricRmsY; amRms["z"] = accelMetricRmsZ;
        JsonObject amPeak = doc.createNestedObject("accelMetricPeak");
        amPeak["x"] = accelMetricPeakX; amPeak["y"] = accelMetricPeakY; amPeak["z"] = accelMetricPeakZ;
        JsonObject amRms2 = doc.createNestedObject("accelMetricRms2");
        amRms2["x"] = accelMetricRms2X; amRms2["y"] = accelMetricRms2Y; amRms2["z"] = accelMetricRms2Z;
        JsonObject amPeak2 = doc.createNestedObject("accelMetricPeak2");
        amPeak2["x"] = accelMetricPeak2X; amPeak2["y"] = accelMetricPeak2Y; amPeak2["z"] = accelMetricPeak2Z;

        JsonObject veloRms = doc.createNestedObject("veloRms");
        veloRms["x"] = veloRmsX; veloRms["y"] = veloRmsY; veloRms["z"] = veloRmsZ;
        JsonObject veloPeak = doc.createNestedObject("veloPeak");
        veloPeak["x"] = veloPeakX; veloPeak["y"] = veloPeakY; veloPeak["z"] = veloPeakZ;
        
        JsonObject vwRms = doc.createNestedObject("veloWideRms");
        vwRms["x"] = veloWideRmsX; vwRms["y"] = veloWideRmsY; vwRms["z"] = veloWideRmsZ;
        JsonObject vwPeak = doc.createNestedObject("veloWidePeak");
        vwPeak["x"] = veloWidePeakX; vwPeak["y"] = veloWidePeakY; vwPeak["z"] = veloWidePeakZ;
        
        JsonObject displRms = doc.createNestedObject("displRms");
        displRms["x"] = displRmsX; displRms["y"] = displRmsY; displRms["z"] = displRmsZ;
        JsonObject displPeak = doc.createNestedObject("displPeak");
        displPeak["x"] = displPeakX; displPeak["y"] = displPeakY; displPeak["z"] = displPeakZ;
        
        JsonObject dlRms = doc.createNestedObject("displLowRms");
        dlRms["x"] = displLowRmsX; dlRms["y"] = displLowRmsY; dlRms["z"] = displLowRmsZ;
        JsonObject dlPeak = doc.createNestedObject("displLowPeak");
        dlPeak["x"] = displLowPeakX; dlPeak["y"] = displLowPeakY; dlPeak["z"] = displLowPeakZ;

        JsonObject truePeak = doc.createNestedObject("truePeak");
        truePeak["x"] = truePeakX; truePeak["y"] = truePeakY; truePeak["z"] = truePeakZ;
        JsonObject crestFactor = doc.createNestedObject("crestFactor");
        crestFactor["x"] = crestFactorX; crestFactor["y"] = crestFactorY; crestFactor["z"] = crestFactorZ;
        JsonObject stdDeviation = doc.createNestedObject("stdDeviation");
        stdDeviation["x"] = stdDeviationX; stdDeviation["y"] = stdDeviationY; stdDeviation["z"] = stdDeviationZ;
        
        doc["temperature"] = temperature;
        doc["status"] = systemStatus;
        doc["capture_busy"] = currentDataState != DD_IDLE || isReadingWaveform || isReadingSpectrum;
        doc["cycle_active"] = isAutoCaptureCycleActive;
        doc["traffic_light"] = trafficLightStatus; 
        
        // --- 2. Add Configuration Details for the Transmission Tab ---
        doc["mqtt_server"] = mqtt_server_str;
        doc["mqtt_topic"] = mqtt_topic_str;
        doc["modbus_enabled"] = modbusTcpEnabled;
        doc["modbus_port"] = modbusTcpPort;
        doc["modbus_id"] = modbus_slave_id_val;
        
        switch(lastCapturedAxis) {
            case AXIS_X: doc["capturedAxis"] = "X"; break;
            case AXIS_Y: doc["capturedAxis"] = "Y"; break;
            case AXIS_Z: doc["capturedAxis"] = "Z"; break;
            default: doc["capturedAxis"] = "NONE"; break;
        }
        
        xSemaphoreGive(dataMutex);
    } else {
        doc["status"] = "System Busy";
    }
    
    // Check MQTT connection outside Mutex to avoid deadlocks
    doc["mqtt_connected"] = (mqttClient && mqttClient->connected());
    
    String payload;
    serializeJson(doc, payload);
    server.send(200, "application/json", payload);
}

// NOTE: handleFullWaveform/Spectrum are unused without graphs but kept for API compat
void handleFullWaveform() { server.send(200, "application/json", "[]"); }
void handleFullSpectrum() { server.send(200, "application/json", "{}"); }

void handleTriggerCapture() {
    String axis_str = server.arg("axis");
    bool isCycleStart = (server.arg("cycle") == "true"); 

    bool isBusy = true;
    Axis selectedAxis = AXIS_NONE;
    if (axis_str == "X") selectedAxis = AXIS_X;
    else if (axis_str == "Y") selectedAxis = AXIS_Y;
    else if (axis_str == "Z") selectedAxis = AXIS_Z;
    
    if (selectedAxis == AXIS_NONE) {
        server.send(400, "text/plain", "Invalid or missing axis parameter.");
        return;
    }

    if(xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        isBusy = firmwareUpdateActive || (currentDataState != DD_IDLE) || isReadingWaveform || isReadingSpectrum;
        if (!isBusy) {
            requestedAxis = selectedAxis;
            requestedCapture = CAPTURE_BOTH; // Still captures buffer for MQTT/Modbus
            currentDataState = DD_REQUEST_DATA;

            isAutoCaptureCycleActive = isCycleStart;
            if (isCycleStart) {
                xyzPeakDataCache.clear();
                autoCaptureAxisIndex = 0;
            }
        }
        xSemaphoreGive(dataMutex);
    }
    server.send(isBusy ? 409 : 200, "text/plain", isBusy ? "Device is busy." : "Capture trigger accepted.");
}

