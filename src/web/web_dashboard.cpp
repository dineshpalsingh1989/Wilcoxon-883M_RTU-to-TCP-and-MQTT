#include "core/modules.h"

// =================================================================
// --- WEB SERVER HANDLERS (PART 2) - DASHBOARD & SETTINGS
// =================================================================

void handleRoot() {
    // We send the dashboard in one block because it is mostly static HTML.
    // The browser fills in the data via Javascript fetching /data.
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Sensor Dashboard | )rawliteral" + sensor_model + R"rawliteral(</title>
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
    </style>
</head>
<body>
    <div class="header">
        <div class="title-container">
            <div class="title">)rawliteral" + sensor_model + R"rawliteral(</div>
            <div class="subtitle">Serial: )rawliteral" + sensor_serial + R"rawliteral(</div>
        </div>
        <div class="time-container"><div class="time" id="clock">--:--:--</div></div>
        <div class="menu"><a href="/settings"><i data-lucide="settings"></i>Settings</a></div>
    </div>
    
    <div class="main-content">
        <div class="tabs">
            <button class="tab-button active" onclick="openTab(event, 'Metrics')">Overall Metrics</button>
            <button class="tab-button" onclick="openTab(event, 'Transmission')">Control & Transmission</button>
        </div>

        <div id="Metrics" class="tab-content active">
             <div id="progressBarContainer"><div id="progressBar">0%</div></div>
             
             <div class="actions-panel" style="display:flex; justify-content:space-between; align-items:center; padding:15px;">
                <div>
                    <strong>Live Update Rate:</strong>
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

             <table id="metrics-table">
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
            </table>
        </div>

        <div id="Transmission" class="tab-content">
            
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
                .then(() => console.log('Capture triggered for ' + axis + '-Axis'));
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
            fetch("/data")
            .then(r => r.json())
            .then(data => {
                const statusText = data.status || "Monitoring";
                const isCapturing = statusText.toLowerCase().includes('collecting') || statusText.toLowerCase().includes('waiting') || statusText.toLowerCase().includes('requesting') || statusText.toLowerCase().includes('downloading');
                
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
                        model: ")rawliteral" + sensor_model + R"rawliteral(",
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
                   if (data.capturedAxis === 'X') { setTimeout(() => trigger('Y', true), 1000); }
                   else if (data.capturedAxis === 'Y') { setTimeout(() => trigger('Z', true), 1000); }
                   else if (data.capturedAxis === 'Z') { isFullCycle = false; showToast("XYZ Cycle Complete"); }
                }
                
                wasCapturing = isCapturing;
            })
            .catch(err => console.log("Fetch error (normal if booting):", err));
        }

        window.onload = function() {
            if (typeof lucide !== 'undefined') lucide.createIcons();
            
            // Get initial interval settings
            fetch('/get_intervals').then(r => r.json()).then(d => {
                document.getElementById('metrics_interval').value = d.metrics_interval;
                document.getElementById('data_interval').value = d.interval;
                dashboardUpdateInterval = setInterval(updateDashboardData, d.metrics_interval);
            });

            updateClock();
            setInterval(updateClock, 1000);
            updateDashboardData();
        };
    </script>
</body>
</html>)rawliteral";
    server.send(200, "text/html", html);
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

    bool isBusy = false;
    Axis selectedAxis = AXIS_NONE;
    if (axis_str == "X") selectedAxis = AXIS_X;
    else if (axis_str == "Y") selectedAxis = AXIS_Y;
    else if (axis_str == "Z") selectedAxis = AXIS_Z;
    
    if (selectedAxis == AXIS_NONE) {
        server.send(400, "text/plain", "Invalid or missing axis parameter.");
        return;
    }

    if(xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        isBusy = (currentDataState != DD_IDLE) || isReadingWaveform || isReadingSpectrum;
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
    server.send(200, "text/plain", isBusy ? "Device is busy." : "Capture trigger accepted.");
}

