#pragma once
#include <Arduino.h>

// Main dashboard, served from flash with send_P so no page-sized String is built on the heap.
// Board details (model, firmware, IOC link, SD support) come from /data at runtime.
static const char DASHBOARD_PAGE[] PROGMEM = R"html(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>ILA Sensor Dashboard</title>
<script src="/charts.js"></script>
<style>
:root{--bg:#f3f6fa;--panel:#fff;--ink:#203044;--muted:#5b6b7d;--line:#dbe3ec;--brand:#1765ad;--brand-dark:#0f4f86;--head:#203448;--ok:#198754;--warn:#b26a00;--bad:#c0392b;--soft:#eef3f8}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--ink);font:15px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,Helvetica,Arial,sans-serif}
:focus-visible{outline:3px solid #f0b429;outline-offset:2px}
.skip{position:absolute;left:-999px;top:8px;background:#fff;color:var(--ink);padding:8px 12px;border-radius:6px;z-index:200}.skip:focus{left:8px}
button,select{font:inherit}
.header{background:var(--head);color:#fff;padding:12px max(16px,calc((100% - 1240px)/2));display:flex;flex-wrap:wrap;align-items:center;gap:12px 20px}
.brand{flex:1 1 220px;min-width:0}.title{font-size:21px;font-weight:650}.subtitle{font-size:12.5px;color:#c4d1de;overflow-wrap:anywhere}
.header-statuses{display:flex;flex-wrap:wrap;gap:8px}
.pill{display:inline-flex;align-items:center;gap:7px;padding:5px 11px;border-radius:999px;background:rgba(255,255,255,.08);border:1px solid rgba(255,255,255,.18);font-size:13px;white-space:nowrap}
.status-dot{height:10px;width:10px;border-radius:50%;display:inline-block;flex:none;background:#8a99a8}
.status-dot.connected,.status-dot.ok{background:#2ecc71}.status-dot.disconnected,.status-dot.bad,.status-dot.active{background:#ff5b4a}.status-dot.warn{background:#f5a623}.status-dot.inactive{background:#8a99a8}
.pill.alerting{background:#8f2318;border-color:#ff8a7d}
.header-right{display:flex;align-items:center;gap:14px}.time{font-size:13px;color:#c4d1de;font-variant-numeric:tabular-nums}
.menu{display:flex;gap:8px}.menu a{color:#fff;text-decoration:none;font-size:14px;padding:7px 14px;border:1px solid #4d6680;border-radius:8px}.menu a:hover{background:#2f4a64}
.main-content{max-width:1240px;margin:0 auto;padding:16px}
.banner{padding:12px 16px;border-radius:8px;margin:0 0 12px;font-weight:600;border:1px solid}
.banner.danger{background:#fdecea;border-color:#f3b4ad;color:#8f2318}.banner.warn{background:#fff6e5;border-color:#f2d29b;color:#7a4a00}.banner.info{background:#eaf3fc;border-color:#b9d4ef;color:#174a7c}
.banner button{margin-left:8px}
.tabs{display:flex;gap:4px;border-bottom:2px solid var(--line);overflow-x:auto;margin-bottom:16px}
.tab-button{background:none;border:0;border-bottom:3px solid transparent;margin-bottom:-2px;padding:12px 16px;font-size:15px;color:var(--muted);cursor:pointer;white-space:nowrap}
.tab-button:hover{color:var(--ink)}.tab-button.active{color:var(--brand);border-bottom-color:var(--brand);font-weight:650}
.tab-content{display:none}.tab-content.active{display:block}
.panel{background:var(--panel);border:1px solid var(--line);border-radius:10px;padding:16px;margin-bottom:16px;max-width:100%}
.panel h2,.panel h3{margin:0 0 10px;font-size:17px;display:flex;align-items:center;gap:8px}
.muted{color:var(--muted)}.small{font-size:13px}
.group{margin-bottom:16px}.group-title{font-size:13px;font-weight:650;text-transform:uppercase;letter-spacing:.04em;color:var(--muted);margin:0 0 8px}
.container{display:grid;grid-template-columns:repeat(auto-fill,minmax(180px,1fr));gap:10px}
.card{background:var(--panel);border:1px solid var(--line);border-radius:10px;padding:12px 14px;text-align:left;cursor:pointer;color:inherit;width:100%;transition:border-color .15s,box-shadow .15s}
.card:hover{border-color:#9fb8d1}.card.selected{border-color:var(--brand);box-shadow:0 0 0 2px rgba(23,101,173,.18)}
.card-title{font-size:14px;font-weight:600}.card-sub{font-size:12px;color:var(--muted);min-height:16px}
.card-value{font-size:24px;font-weight:700;color:var(--brand-dark);margin-top:4px;font-variant-numeric:tabular-nums;overflow-wrap:anywhere}
.metrics-dim .card-value{opacity:.45}.stale .card-value{color:var(--muted)}
.wait-message{display:none}
.capture-strip{display:flex;align-items:center;gap:10px;padding:10px 14px;border-radius:8px;background:#fff6e5;border:1px solid #f2d29b;color:#7a4a00;margin-bottom:12px;font-weight:600}
.spinner{width:16px;height:16px;border:3px solid #f2d29b;border-top-color:#b26a00;border-radius:50%;animation:spin 1s linear infinite;flex:none}
@keyframes spin{to{transform:rotate(360deg)}}
@media (prefers-reduced-motion:reduce){.spinner{animation:none}}
.chart-controls,.toolbar{display:flex;flex-wrap:wrap;gap:10px;align-items:center;margin-bottom:12px}
canvas{display:block;max-width:100%}
.chart-actions{display:flex;flex-wrap:wrap;justify-content:flex-end;align-items:center;gap:12px;margin-top:8px}
.last-updated-text{font-size:12.5px;color:var(--muted);font-style:normal}
.data-status{font-size:12.5px;font-weight:650}
.btn{border:1px solid #c3cfdc;background:#fff;color:var(--ink);padding:8px 14px;border-radius:8px;cursor:pointer}
.btn:hover{background:var(--soft)}.btn:disabled{opacity:.55;cursor:not-allowed}
.btn-primary{background:var(--brand);border-color:var(--brand);color:#fff}.btn-primary:hover{background:var(--brand-dark)}
.btn-danger{background:#fff;border-color:#e3a39b;color:#9b2a1d}.btn-danger:hover{background:#fdecea}
.btn-small{padding:5px 10px;font-size:13px}
select{padding:7px 10px;border:1px solid #c3cfdc;border-radius:8px;background:#fff;color:var(--ink)}
.table-wrap{overflow-x:auto;max-width:100%}
.logger-table-container{max-height:380px;overflow:auto}
table{width:100%;border-collapse:collapse;font-size:14px}th,td{text-align:left;padding:9px 10px;border-bottom:1px solid var(--line);white-space:nowrap}
th{background:#f6f8fb;font-weight:650;position:sticky;top:0}tbody tr:hover{background:#f7f9fc}
.badge{display:inline-block;padding:2px 9px;border-radius:999px;font-size:12.5px;font-weight:650;background:var(--soft);color:var(--muted)}
.badge.up{background:#fdecea;color:#9b2a1d}.badge.down{background:#e7f5ee;color:#146c43}.badge.new{background:#eaf3fc;color:#174a7c}
.help-icon{display:inline-flex;align-items:center;justify-content:center;width:22px;height:22px;border-radius:50%;border:1px solid #9aaabb;background:#fff;color:var(--muted);font-weight:700;font-size:13px;cursor:pointer;padding:0}
dialog{border:0;border-radius:12px;max-width:600px;width:calc(100% - 32px);padding:20px;box-shadow:0 20px 50px rgba(0,0,0,.25);color:var(--ink)}
dialog::backdrop{background:rgba(15,25,35,.5)}dialog h2{margin-top:0}
.logger-section h3{flex-wrap:wrap}.logger-controls{display:flex;flex-wrap:wrap;align-items:center;gap:10px;margin-bottom:14px}
.progress-container{flex:1 1 220px;min-width:180px}.logger-status{font-weight:650;font-size:14px;margin-bottom:4px}
.progress-bar{width:100%;background:#e6ecf2;border-radius:6px;height:20px;overflow:hidden}
.progress-bar-fill{height:100%;width:0;background:var(--brand);color:#fff;font-size:12px;line-height:20px;text-align:center;transition:width .4s;white-space:nowrap}
.logger-header{display:flex;flex-wrap:wrap;justify-content:space-between;align-items:center;gap:10px;margin-bottom:10px}.logger-header h3{margin:0}
.anomaly-banner,.warning-banner{display:none}
.toast{position:fixed;right:16px;bottom:16px;max-width:min(420px,calc(100% - 32px));background:#203448;color:#fff;padding:12px 16px;border-radius:10px;box-shadow:0 10px 30px rgba(0,0,0,.25);opacity:0;transform:translateY(10px);transition:opacity .2s,transform .2s;pointer-events:none;z-index:150}
.toast.show{opacity:1;transform:none}.toast.error{background:#8f2318}
.footer{max-width:1240px;margin:0 auto;padding:12px 16px 24px;font-size:12.5px;color:var(--muted);display:flex;flex-wrap:wrap;gap:6px 16px}
@media(max-width:650px){.title{font-size:18px}.header-right{width:100%;justify-content:space-between}.card-value{font-size:20px}.container{grid-template-columns:repeat(auto-fill,minmax(140px,1fr))}.panel{padding:12px}}
</style>
</head>
<body>
<a class="skip" href="#main">Skip to dashboard</a>
<dialog id="analysis-help-modal" aria-labelledby="analysis-help-title"><h2 id="analysis-help-title">Spectrum analysis help</h2><p>The table lists the strongest frequencies in the latest spectrum read from the sensor and how their amplitude is changing.</p><ul><li><b>Rank:</b> highest amplitude first.</li><li><b>Status:</b> the amplitude trend across recent captures.</li><li><b>History:</b> the last five amplitudes, oldest to newest. Hover or focus a value to see its time.</li></ul><button class="btn" type="button" onclick="closeModal('analysis-help-modal')">Close</button></dialog>
<dialog id="logger-help-modal" aria-labelledby="logger-help-title"><h2 id="logger-help-title">Anomaly detection help</h2><ol><li><b>Learning:</b> after you start recording, the gateway uses the first readings to learn the normal amplitude (mean and standard deviation) of each tracked peak. The LED blinks cyan.</li><li><b>Monitoring:</b> each new reading is compared with that baseline. A peak more than three standard deviations above its mean is flagged as an anomaly and the LED pulses red.</li></ol><button class="btn" type="button" onclick="closeModal('logger-help-modal')">Close</button></dialog>
<header class="header">
  <div class="brand"><div class="title">ILA Sensor Dashboard</div><div class="subtitle" id="device-subtitle">Connecting to gateway…</div></div>
  <div class="header-statuses" role="group" aria-label="Gateway status">
    <span class="pill" id="internet-status"><span class="status-dot disconnected"></span>MQTT offline</span>
    <span class="pill" id="sensor-status"><span class="status-dot"></span>Sensor: waiting</span>
    <span class="pill" id="alert-relay-status"><span class="status-dot inactive"></span>Alert output</span>
    <span class="pill" id="alarm-relay-status"><span class="status-dot inactive"></span>Alarm output</span>
  </div>
  <div class="header-right"><span class="time" id="clock">--:--:--</span><nav class="menu" aria-label="Gateway pages"><a id="ioc-link" href="/ioc" hidden>IOC I/O</a><a href="/settings">Settings</a></nav></div>
</header>
<div id="toast" class="toast" role="status" aria-live="polite"></div>
<main class="main-content" id="main">
  <div id="connection-banner" class="banner danger" role="alert" hidden>Gateway unreachable. Showing the last values received; retrying automatically.</div>
  <div id="sensor-fault-banner" class="banner danger" role="alert" hidden>Sensor communication lost. Values below are not current. Check RS485 wiring, sensor power and the RS485 settings.</div>
  <div id="global-anomaly-banner" class="banner danger" hidden><span id="global-anomaly-text"></span><button class="btn btn-small" type="button" onclick="showTab('Logger', true)">Open data logger</button></div>
  <div class="tabs" role="tablist" aria-label="Dashboard sections">
    <button class="tab-button active" role="tab" id="tab-Metrics" aria-controls="Metrics" aria-selected="true">Overall Metrics</button>
    <button class="tab-button" role="tab" id="tab-Waveforms" aria-controls="Waveforms" aria-selected="false" tabindex="-1">Waveform &amp; Spectrum</button>
    <button class="tab-button" role="tab" id="tab-Analysis" aria-controls="Analysis" aria-selected="false" tabindex="-1">Spectrum Analysis</button>
    <button class="tab-button" role="tab" id="tab-Logger" aria-controls="Logger" aria-selected="false" tabindex="-1">Data Logger</button>
  </div>

  <section id="Metrics" class="tab-content active" role="tabpanel" aria-labelledby="tab-Metrics">
    <div class="wait-message" id="wait-message"><div class="capture-strip" role="status"><span class="spinner" aria-hidden="true"></span><span id="wait-message-text">Collecting data…</span><span class="small">Live metrics resume after the capture.</span></div></div>
    <div id="metrics-container">
      <div class="group"><h2 class="group-title">Acceleration</h2><div class="container">
        <button type="button" class="card" data-index="0"><div class="card-title">Accel RMS</div><div class="card-sub">2 Hz – 1 kHz</div><div class="card-value" id="accelRms1">--</div></button>
        <button type="button" class="card" data-index="1"><div class="card-title">Accel Peak</div><div class="card-sub">2 Hz – 1 kHz</div><div class="card-value" id="accelPeak1">--</div></button>
        <button type="button" class="card" data-index="2"><div class="card-title">Accel RMS</div><div class="card-sub">10 Hz – 10 kHz</div><div class="card-value" id="accelRms2">--</div></button>
        <button type="button" class="card" data-index="3"><div class="card-title">Accel Peak</div><div class="card-sub">10 Hz – 10 kHz</div><div class="card-value" id="accelPeak2">--</div></button>
        <button type="button" class="card" data-index="8"><div class="card-title">True Peak</div><div class="card-sub">&nbsp;</div><div class="card-value" id="truePeak">--</div></button>
      </div></div>
      <div class="group"><h2 class="group-title">Velocity &amp; displacement</h2><div class="container">
        <button type="button" class="card" data-index="4"><div class="card-title">Velo RMS</div><div class="card-sub">10 Hz – 1 kHz</div><div class="card-value" id="veloRms">--</div></button>
        <button type="button" class="card selected" data-index="5"><div class="card-title">Velo Peak</div><div class="card-sub">10 Hz – 1 kHz · drives outputs</div><div class="card-value" id="veloPeak">--</div></button>
        <button type="button" class="card" data-index="6"><div class="card-title">Displ RMS</div><div class="card-sub">2 Hz – 1 kHz</div><div class="card-value" id="displRms">--</div></button>
        <button type="button" class="card" data-index="7"><div class="card-title">Displ Peak</div><div class="card-sub">2 Hz – 1 kHz</div><div class="card-value" id="displPeak">--</div></button>
      </div></div>
      <div class="group"><h2 class="group-title">Signal &amp; sensor health</h2><div class="container">
        <button type="button" class="card" data-index="9"><div class="card-title">Crest Factor</div><div class="card-sub">&nbsp;</div><div class="card-value" id="crestFactor">--</div></button>
        <button type="button" class="card" data-index="10"><div class="card-title">Std. Deviation</div><div class="card-sub">&nbsp;</div><div class="card-value" id="stdDeviation">--</div></button>
        <button type="button" class="card" data-index="11"><div class="card-title">Temperature</div><div class="card-sub">Sensor</div><div class="card-value" id="temperature">--</div></button>
        <button type="button" class="card" data-index="12"><div class="card-title">Drive Voltage</div><div class="card-sub">&nbsp;</div><div class="card-value" id="driveVoltage">--</div></button>
        <button type="button" class="card" data-index="13"><div class="card-title">Board Temp</div><div class="card-sub">Sensor electronics</div><div class="card-value" id="boardTemperature">--</div></button>
      </div></div>
    </div>
    <div class="panel">
      <div class="chart-controls"><label for="metric-chart-selector"><b>Live trend</b></label><select id="metric-chart-selector"></select><span class="small muted">Last 50 readings in this browser. Select a card to plot it.</span></div>
      <canvas id="metricsHistoryChart"></canvas>
    </div>
  </section>

  <section id="Waveforms" class="tab-content" role="tabpanel" aria-labelledby="tab-Waveforms">
    <div class="panel">
      <div class="toolbar">
        <button type="button" class="btn btn-primary" id="collect-btn" onclick="trigger()">Collect full data now</button>
        <label for="data_interval">Automatic collection</label>
        <select id="data_interval" onchange="setIntervals()">
          <option value="300000">Every 5 minutes</option>
          <option value="600000">Every 10 minutes</option>
          <option value="1200000">Every 20 minutes</option>
          <option value="2147483647">Never (manual only)</option>
        </select>
        <button type="button" class="btn" onclick="clearDynamicData()">Clear cached data</button>
      </div>
      <p class="small muted" id="capture-note" role="status" aria-live="polite">A capture reads the waveform and spectrum from the sensor. Live metrics pause while it runs.</p>
    </div>
    <div class="panel"><canvas id="waveformChart"></canvas><div class="chart-actions"><span class="data-status" id="waveform-data-status"></span><em id="waveformLastUpdated" class="last-updated-text"></em><button type="button" class="btn btn-small" id="wf-load-btn" onclick="loadFullWaveform()">View Full Waveform</button></div></div>
    <div class="panel"><canvas id="spectrumChart"></canvas><div class="chart-actions"><span class="data-status" id="spectrum-data-status"></span><em id="spectrumLastUpdated" class="last-updated-text"></em><button type="button" class="btn btn-small" id="sp-load-btn" onclick="loadFullSpectrum()">View Full Spectrum</button></div></div>
  </section>

  <section id="Analysis" class="tab-content" role="tabpanel" aria-labelledby="tab-Analysis">
    <div class="panel analysis-container">
      <h2>Top 4 frequency peaks <button type="button" class="help-icon" aria-label="Spectrum analysis help" onclick="openModal('analysis-help-modal')">?</button></h2>
      <p class="muted small">Strongest peaks in the latest spectrum read from the sensor, and how their amplitude is changing over time.</p>
      <div class="table-wrap"><table id="analysis-table"><thead><tr><th>Rank</th><th>Frequency (Hz)</th><th>Amplitude (g)</th><th>Status</th><th>History (g)</th></tr></thead><tbody><tr><td colspan="5">Waiting for data…</td></tr></tbody></table></div>
    </div>
  </section>

  <section id="Logger" class="tab-content" role="tabpanel" aria-labelledby="tab-Logger">
    <div id="storage-warning-banner" class="banner warn warning-banner"></div>
    <div class="panel logger-section">
      <h3>Spectrum peak logger &amp; anomaly detection <button type="button" class="help-icon" aria-label="Anomaly detection help" onclick="openModal('logger-help-modal')">?</button></h3>
      <div id="anomaly-banner" class="banner danger anomaly-banner" role="alert"></div>
      <div class="logger-controls">
        <button type="button" class="btn btn-primary" id="record-btn" onclick="togglePeakRecording()">Start Recording</button>
        <button type="button" class="btn btn-danger" id="clear-log-btn" onclick="clearPeakLog()">Clear Log</button>
        <div class="progress-container"><div class="logger-status" id="logger-status">Stopped</div><div class="progress-bar"><div class="progress-bar-fill" id="progress-bar-fill">0/30</div></div></div>
        <button type="button" class="btn" id="download-csv-btn" onclick="downloadPeakCSV()" disabled>Download Peaks CSV</button>
      </div>
      <div class="logger-table-container"><table id="logger-table"><thead><tr><th>Timestamp</th><th id="peak-header-1">Peak 1 (F/A)</th><th id="peak-header-2">Peak 2 (F/A)</th><th id="peak-header-3">Peak 3 (F/A)</th><th id="peak-header-4">Peak 4 (F/A)</th></tr></thead><tbody></tbody></table></div>
    </div>
    <div class="panel logger-section">
      <h3>Overall metrics logger</h3>
      <div class="logger-controls">
        <button type="button" class="btn btn-primary" id="metrics-record-btn" onclick="toggleMetricsRecording()">Start Metrics Recording</button>
        <button type="button" class="btn btn-danger" id="clear-metrics-log-btn" onclick="clearMetricsLog()">Clear Log</button>
        <div class="progress-container"><div class="logger-status" id="metrics-logger-status">Stopped</div><div class="progress-bar"><div class="progress-bar-fill" id="metrics-progress-bar-fill" style="background:#198754">0/30</div></div></div>
        <button type="button" class="btn" id="metrics-download-csv-btn" onclick="downloadMetricsCSV()" disabled>Download Metrics CSV</button>
      </div>
      <div class="logger-table-container"><table id="metrics-logger-table"><thead><tr><th>Timestamp</th><th>A(rms)1</th><th>A(pk)1</th><th>A(rms)2</th><th>A(pk)2</th><th>V(rms)</th><th>V(pk)</th><th>D(rms)</th><th>D(pk)</th><th>TruePk</th><th>CF</th><th>StdDev</th><th>Temp</th><th>DriveV</th><th>BoardT</th></tr></thead><tbody></tbody></table></div>
    </div>
    <div class="panel logger-section" id="sd-log-section" hidden>
      <div class="logger-header"><h3>SD card files</h3><button type="button" id="clear-events-btn" class="btn btn-danger" onclick="clearEventLog()">Clear event_log.csv</button></div>
      <div class="logger-table-container"><table id="event-log-table"><thead><tr><th>Last Modified</th><th>File Name</th><th>Size</th></tr></thead><tbody><tr><td colspan="3">No files found.</td></tr></tbody></table></div>
    </div>
  </section>
</main>
<footer class="footer"><span id="status">Status: Initializing...</span><span>Last metrics update: <span id="lastUpdated">--</span></span><span id="firmware-info"></span></footer>
<script>
'use strict';
let waveformChart, spectrumChart, metricsHistoryChart, isPeakRecording = false, isMetricsRecording = false;
let liveMetricsHistory = [], activeTab = 'Metrics', pollTick = 0, pollTimer = 0, sdStorage = false, toastTimer = 0;
const LIVE_HISTORY_SIZE = 50, POLL_MS = 3000, STALE_MS = 60000;
const metricKeys = ['accelRms1','accelPeak1','accelRms2','accelPeak2','veloRms','veloPeak','displRms','displPeak','truePeak','crestFactor','stdDeviation','temperature','driveVoltage','boardTemperature'];
const metricNames = ['Accel RMS 1 (g)','Accel Peak 1 (g)','Accel RMS 2 (g)','Accel Peak 2 (g)','Velo RMS (mm/s)','Velo Peak (mm/s)','Displ RMS (µm)','Displ Peak (µm)','True Peak (g)','Crest Factor','Std. Deviation','Temperature (°C)','Drive Voltage (V)','Board Temp (°C)'];
const metricUnits = ['g','g','g','g','mm/s','mm/s','µm','µm','g','','','°C','V','°C'];
const metricDigits = [3,3,3,3,2,2,2,2,3,2,3,2,2,2];
const $ = id => document.getElementById(id);
function el(tag, text, className) { const e = document.createElement(tag); if (text !== undefined) e.textContent = text; if (className) e.className = className; return e; }
function setPill(id, state, text) { const p = $(id); p.replaceChildren(el('span', undefined, 'status-dot ' + state), document.createTextNode(text)); return p; }
function emptyRow(body, cols, text) { const tr = el('tr'), td = el('td', text); td.colSpan = cols; tr.append(td); body.replaceChildren(tr); }
function toast(text, error) {
  const t = $('toast'); t.textContent = text; t.className = 'toast show' + (error ? ' error' : '');
  clearTimeout(toastTimer); toastTimer = setTimeout(() => { t.className = 'toast'; }, 4000);
}
function openModal(id) { const d = $(id); if (d.showModal) d.showModal(); else d.setAttribute('open', ''); }
function closeModal(id) { const d = $(id); if (d.close) d.close(); else d.removeAttribute('open'); }
function showTab(name, focus) {
  activeTab = name;
  document.querySelectorAll('.tab-button').forEach(tab => {
    const on = tab.id === 'tab-' + name;
    tab.classList.toggle('active', on); tab.setAttribute('aria-selected', on); tab.tabIndex = on ? 0 : -1;
    if (on && focus) tab.focus();
  });
  document.querySelectorAll('.tab-content').forEach(panel => panel.classList.toggle('active', panel.id === name));
  if (name === 'Metrics' && metricsHistoryChart) metricsHistoryChart.update();
  refreshSoon();
}
function updateClock() { $('clock').textContent = new Date().toLocaleTimeString(); }
async function requestJSON(url) {
  const controller = new AbortController(), timer = setTimeout(() => controller.abort(), 15000);
  try {
    const response = await fetch(url, {signal: controller.signal, cache: 'no-store'});
    if (!response.ok) throw new Error(`Request failed (${response.status})`);
    return await response.json();
  } finally { clearTimeout(timer); }
}
async function requestAction(url, successMessage) {
  const controller = new AbortController(), timer = setTimeout(() => controller.abort(), 15000);
  try {
    const response = await fetch(url, {signal: controller.signal, cache: 'no-store'});
    const text = await response.text();
    if (!response.ok) throw new Error(text || `Request failed (${response.status})`);
    if (successMessage) toast(successMessage);
    return true;
  } catch (error) { toast(error.name === 'AbortError' ? 'Gateway did not answer in time.' : (error.message || 'Gateway unreachable.'), true); return false; }
  finally { clearTimeout(timer); }
}
function showOffline() {
  setPill('internet-status', 'bad', 'Gateway unreachable');
  $('connection-banner').hidden = false;
  $('status').textContent = 'Connection lost. Retrying...';
}
function setLoadButtons(busy) {
  [['wf-load-btn', 'View Full Waveform'], ['sp-load-btn', 'View Full Spectrum']].forEach(([id, label]) => {
    const b = $(id); if (b.dataset.loading) return;
    b.textContent = busy ? 'Loading...' : label; b.disabled = busy;
  });
}
async function loadInto(buttonId, label, url, stampId, apply) {
  const button = $(buttonId); button.dataset.loading = '1'; button.textContent = 'Loading...'; button.disabled = true;
  try {
    const d = await requestJSON(url);
    if (apply(d)) $(stampId).textContent = 'Last updated: ' + new Date().toLocaleTimeString();
    else toast('No captured data is available on the gateway yet.');
  } catch (e) { $(stampId).textContent = 'Load failed. Try again.'; }
  finally { delete button.dataset.loading; button.textContent = label; button.disabled = false; }
}
function loadFullWaveform() {
  return loadInto('wf-load-btn', 'View Full Waveform', '/fullwaveform', 'waveformLastUpdated', d => {
    if (!Array.isArray(d) || !d.length) return false;
    waveformChart.data.datasets[0].data = d; waveformChart.data.labels = Array.from({length: d.length}, (_, i) => i);
    waveformChart.update('none'); return true;
  });
}
function loadFullSpectrum() {
  return loadInto('sp-load-btn', 'View Full Spectrum', '/fullspectrum', 'spectrumLastUpdated', d => {
    if (!d || !Array.isArray(d.points) || !d.points.length) return false;
    const max = d.points.reduce((m, v) => v > m ? v : m, 0);
    spectrumChart.data.datasets[0].data = d.points; spectrumChart.data.labels = d.labels;
    spectrumChart.options.scales.y.max = Math.max(0.01, max * 1.1);
    spectrumChart.update('none'); return true;
  });
}
async function trigger() {
  $('collect-btn').disabled = true;
  if (await requestAction('/trigger_waveform', 'Data collection requested.')) await updateDashboardData();
  else $('collect-btn').disabled = false;
}
function setIntervals() { requestAction(`/set_intervals?interval=${encodeURIComponent($('data_interval').value)}`, 'Automatic collection interval updated.'); }
function clearDynamicData() { if (confirm('Clear cached waveform and spectrum data?')) requestAction('/clear_dynamic_data', 'Cached data cleared.'); }
function clearPeakLog() { if (confirm('Delete all recorded peak data?')) requestAction('/clear_peak_log', 'Peak log cleared.').then(refreshSoon); }
function clearMetricsLog() { if (confirm('Delete all recorded metrics data?')) requestAction('/clear_metrics_log', 'Metrics log cleared.').then(refreshSoon); }
function clearEventLog() { if (confirm('Delete event_log.csv?')) requestAction('/clear_event_log', 'event_log.csv deleted.').then(refreshSoon); }
function togglePeakRecording() { requestAction(`/datalog_control?action=${isPeakRecording ? 'stop' : 'start'}`).then(refreshSoon); }
function toggleMetricsRecording() { requestAction(`/metrics_log_control?action=${isMetricsRecording ? 'stop' : 'start'}`).then(refreshSoon); }
function downloadPeakCSV() { window.location.href = '/download_csv'; }
function downloadMetricsCSV() { window.location.href = '/download_metrics_csv'; }
function setProgress(fillId, count, max) {
  const fill = $(fillId), pct = max > 0 ? Math.min(100, count / max * 100) : 0;
  fill.style.width = pct + '%'; fill.textContent = `${count}/${max}`;
}
function setRecordButton(button, recording, startText, stopText) {
  button.textContent = recording ? stopText : startText;
  button.className = 'btn ' + (recording ? 'btn-danger' : 'btn-primary');
}
function updatePeakLoggerUI(data) {
  isPeakRecording = !!data.is_recording;
  setRecordButton($('record-btn'), isPeakRecording, 'Start Recording', 'Stop Recording');
  const status = $('logger-status');
  if (!isPeakRecording) { status.textContent = 'Stopped'; status.style.color = 'var(--muted)'; }
  else if (!data.is_baseline_established) { status.textContent = `Learning baseline (${data.baseline_samples}/${data.max_baseline_samples})`; status.style.color = 'var(--brand)'; }
  else { status.textContent = 'Monitoring for anomalies'; status.style.color = 'var(--ok)'; }
  setProgress('progress-bar-fill', data.log_count || 0, data.max_log_size || 0);
  $('download-csv-btn').disabled = !(data.log_count > 0);
  const banner = $('anomaly-banner'), global = $('global-anomaly-banner');
  if (data.anomaly_detected) {
    banner.style.display = 'block'; banner.textContent = 'Anomaly detected: ' + (data.anomaly_details || '');
    $('global-anomaly-text').textContent = 'Spectrum anomaly detected: ' + (data.anomaly_details || '');
    global.hidden = activeTab === 'Logger';
  } else { banner.style.display = 'none'; global.hidden = true; }
  const freqs = data.is_baseline_established ? data.baseline_freqs : (data.log_count > 0 ? data.initial_freqs : []);
  for (let i = 1; i <= 4; i++) {
    const f = freqs && freqs[i - 1];
    $(`peak-header-${i}`).textContent = Number.isFinite(f) ? `Peak ${i} (${f.toFixed(2)} Hz)` : `Peak ${i} (F/A)`;
  }
  const body = document.querySelector('#logger-table tbody');
  if (!(data.log_count > 0) || !Array.isArray(data.log)) { emptyRow(body, 5, 'No data recorded.'); return; }
  body.replaceChildren(...data.log.slice().reverse().map(entry => {
    const tr = el('tr'); tr.append(el('td', entry.timestamp));
    (entry.peaks || []).forEach(p => tr.append(el('td', `${p.f.toFixed(2)} Hz / ${p.a.toFixed(4)} g`)));
    return tr;
  }));
}
function updateMetricsLoggerUI(data) {
  isMetricsRecording = !!data.is_recording;
  setRecordButton($('metrics-record-btn'), isMetricsRecording, 'Start Metrics Recording', 'Stop Metrics Recording');
  const status = $('metrics-logger-status');
  status.textContent = isMetricsRecording ? `Recording (${data.log_count}/${data.max_log_size})` : 'Stopped';
  status.style.color = isMetricsRecording ? 'var(--ok)' : 'var(--muted)';
  setProgress('metrics-progress-bar-fill', data.log_count || 0, data.max_log_size || 0);
  $('metrics-download-csv-btn').disabled = !(data.log_count > 0);
  const body = document.querySelector('#metrics-logger-table tbody');
  if (!(data.log_count > 0) || !Array.isArray(data.log)) { emptyRow(body, 15, 'No historical metrics recorded yet.'); return; }
  body.replaceChildren(...data.log.slice().reverse().map(entry => {
    const tr = el('tr'); tr.append(el('td', entry.timestamp));
    (entry.values || []).forEach(v => tr.append(el('td', Number.isFinite(v) ? v.toFixed(3) : '--')));
    return tr;
  }));
}
function updateEventLogUI(data) {
  const body = document.querySelector('#event-log-table tbody');
  if (!data.files || !data.files.length) { emptyRow(body, 3, 'No files found on the SD card.'); return; }
  body.replaceChildren(...data.files.map(f => {
    const tr = el('tr'); tr.append(el('td', f.timestamp), el('td', f.name), el('td', (f.size / 1024).toFixed(2) + ' KB')); return tr;
  }));
}
function updateAnalysis(data) {
  const body = document.querySelector('#analysis-table tbody');
  const peaks = (data.peaks || []).filter(p => p.frequency > 0);
  if (!peaks.length) emptyRow(body, 5, 'No significant peaks detected.');
  else body.replaceChildren(...peaks.map((peak, index) => {
    const tr = el('tr'), status = String(peak.status || ''), lower = status.toLowerCase();
    const badge = el('span', status || '—', 'badge' + (/increas|ris|up/.test(lower) ? ' up' : /decreas|fall|down/.test(lower) ? ' down' : /new/.test(lower) ? ' new' : ''));
    const statusCell = el('td'); statusCell.append(badge);
    const history = el('td');
    (peak.history || []).forEach((h, i) => {
      if (i) history.append(document.createTextNode(' → '));
      const v = el('span', h.amp.toFixed(4)); v.title = h.ts; v.tabIndex = 0; v.setAttribute('aria-label', `${h.amp.toFixed(4)} g at ${h.ts}`); history.append(v);
    });
    if (!(peak.history || []).length) history.textContent = 'No history yet.';
    tr.append(el('td', '#' + (index + 1)), el('td', peak.frequency.toFixed(2)), el('td', peak.amplitude.toFixed(4)), statusCell, history);
    return tr;
  }));
  if (spectrumChart) { spectrumChart.data.datasets[1].data = peaks.map(p => ({x: p.frequency, y: p.amplitude})); spectrumChart.update('none'); }
}
function formatMetric(i, value) { return value.toFixed(metricDigits[i]) + (metricUnits[i] ? ' ' + metricUnits[i] : ''); }
function drawMetricsChart() {
  if (!metricsHistoryChart) return;
  const index = Number($('metric-chart-selector').value), key = metricKeys[index];
  document.querySelectorAll('#metrics-container .card').forEach(c => c.classList.toggle('selected', Number(c.dataset.index) === index));
  metricsHistoryChart.data.labels = liveMetricsHistory.map(d => d.time);
  metricsHistoryChart.data.datasets[0].data = liveMetricsHistory.map(d => d.metrics[key]);
  metricsHistoryChart.data.datasets[0].label = metricNames[index];
  metricsHistoryChart.options.scales.y.title.text = metricNames[index];
  metricsHistoryChart.options.plugins.customCanvasMessage.display = liveMetricsHistory.length === 0;
  metricsHistoryChart.update();
}
let wasCapturing = false, dashboardPending = false, deviceShown = false;
function applyMetrics(data) {
  const statusText = data.status || 'Monitoring', isCapturing = !!data.capture_busy, valid = data.metrics_valid !== false;
  if (!deviceShown && data.model !== undefined) {
    deviceShown = true;
    $('device-subtitle').textContent = `Model: ${data.model}` + (data.gateway_id ? ` · Gateway ID: ${data.gateway_id}` : '') + (data.network ? ` · ${data.network}` : '');
    $('firmware-info').textContent = data.firmware ? 'Firmware ' + data.firmware : '';
    $('ioc-link').hidden = !data.ioc;
  }
  if (!deviceShown && data.gateway_id) $('device-subtitle').textContent = 'Gateway ID: ' + data.gateway_id;
  $('connection-banner').hidden = true;
  setPill('internet-status', data.mqtt_connected ? 'connected' : 'disconnected', data.mqtt_connected ? 'MQTT online' : 'MQTT offline');
  if (data.sensor_fault) setPill('sensor-status', 'bad', 'Sensor lost');
  else if (!valid) setPill('sensor-status', 'warn', 'No sensor reading');
  else setPill('sensor-status', 'ok', 'Sensor OK');
  $('sensor-fault-banner').hidden = !data.sensor_fault;
  [['alert-relay-status', data.alert_relay_active, data.alert_relay_enabled, 'Alert'], ['alarm-relay-status', data.alarm_relay_active, data.alarm_relay_enabled, 'Alarm']].forEach(([id, active, enabled, name]) => {
    const pill = setPill(id, active ? 'active' : 'inactive', `${name}: ${active ? 'ACTIVE' : enabled === false ? 'disabled' : 'INACTIVE'}`);
    pill.classList.toggle('alerting', !!active);
  });
  $('status').textContent = 'Status: ' + statusText;
  const wf = $('waveform-data-status'), sp = $('spectrum-data-status'), lower = statusText.toLowerCase();
  if (isCapturing) { [wf, sp].forEach(s => { s.textContent = 'Capture in progress…'; s.style.color = 'var(--warn)'; }); }
  else if (lower.includes('failed') || lower.includes('error')) { [wf, sp].forEach(s => { s.textContent = 'Last capture failed.'; s.style.color = 'var(--bad)'; }); }
  else {
    wf.textContent = data.newWaveformAvailable ? 'Captured data available' : 'No captured data'; wf.style.color = data.newWaveformAvailable ? 'var(--ok)' : 'var(--muted)';
    sp.textContent = data.newSpectrumAvailable ? 'Captured data available' : 'No captured data'; sp.style.color = data.newSpectrumAvailable ? 'var(--ok)' : 'var(--muted)';
  }
  $('capture-note').textContent = isCapturing ? 'Capture in progress: ' + statusText : 'A capture reads the waveform and spectrum from the sensor. Live metrics pause while it runs.';
  $('collect-btn').disabled = isCapturing;
  if (wasCapturing && !isCapturing) { loadFullWaveform(); loadFullSpectrum(); }
  wasCapturing = isCapturing;
  setLoadButtons(isCapturing);
  sdStorage = data.storage_mode === 'SD Card';
  const storage = $('storage-warning-banner');
  if (sdStorage) storage.style.display = 'none';
  else {
    storage.textContent = data.sd_supported === false
      ? 'Logs are kept in RAM on this gateway. Only recent readings are stored, and they are lost on reboot. Download the CSV files to keep them.'
      : 'No SD card detected. Logging is limited to recent readings in internal memory and is lost on reboot.';
    storage.style.display = 'block';
  }
  $('sd-log-section').hidden = !sdStorage;
  const container = $('metrics-container'), age = data.metrics_age_ms || 0;
  container.classList.toggle('metrics-dim', isCapturing);
  container.classList.toggle('stale', valid && age > STALE_MS);
  $('wait-message').style.display = isCapturing ? 'block' : 'none';
  if (isCapturing) { $('wait-message-text').textContent = statusText; }
  else {
    if (valid && Number.isFinite(data.veloPeak)) {
      liveMetricsHistory.push({time: new Date().toLocaleTimeString(), metrics: data});
      if (liveMetricsHistory.length > LIVE_HISTORY_SIZE) liveMetricsHistory.shift();
    }
    metricKeys.forEach((key, i) => { const v = data[key]; $(key).textContent = valid && Number.isFinite(v) ? formatMetric(i, v) : '--'; });
    $('lastUpdated').textContent = valid ? new Date(Date.now() - age).toLocaleTimeString() + (age > STALE_MS ? ' (stale)' : '') : 'No valid sensor reading';
  }
  drawMetricsChart();
}
// Requests run one after another: the gateway web server handles a single client at a time.
async function updateDashboardData() {
  if (dashboardPending) return;
  dashboardPending = true;
  try {
    applyMetrics(await requestJSON('/data'));
    const background = pollTick % 5 === 0;
    if (activeTab === 'Waveforms' || activeTab === 'Analysis' || background) updateAnalysis(await requestJSON('/spectrum_analysis'));
    if (activeTab === 'Logger' || background) updatePeakLoggerUI(await requestJSON('/get_log_data'));
    if (activeTab === 'Logger') {
      updateMetricsLoggerUI(await requestJSON('/get_metrics_log_data'));
      if (sdStorage) updateEventLogUI(await requestJSON('/get_event_log'));
    }
  } catch (error) { showOffline(); }
  finally { dashboardPending = false; pollTick++; }
}
function schedulePoll(delay) { clearTimeout(pollTimer); pollTimer = setTimeout(poll, delay); }
async function poll() { if (!document.hidden) await updateDashboardData(); schedulePoll(POLL_MS); }
function refreshSoon() { schedulePoll(50); }
document.addEventListener('visibilitychange', () => { if (!document.hidden) refreshSoon(); });
window.onload = function () {
  const tabs = [...document.querySelectorAll('.tab-button')];
  tabs.forEach((tab, i) => {
    tab.addEventListener('click', () => showTab(tab.getAttribute('aria-controls')));
    tab.addEventListener('keydown', e => {
      const next = e.key === 'ArrowRight' ? i + 1 : e.key === 'ArrowLeft' ? i - 1 : e.key === 'Home' ? 0 : e.key === 'End' ? tabs.length - 1 : null;
      if (next === null) return;
      e.preventDefault(); showTab(tabs[(next + tabs.length) % tabs.length].getAttribute('aria-controls'), true);
    });
  });
  const selector = $('metric-chart-selector');
  metricNames.forEach((name, index) => { const o = el('option', name); o.value = index; selector.append(o); });
  selector.value = '5';
  selector.addEventListener('change', drawMetricsChart);
  document.querySelectorAll('#metrics-container .card').forEach(card => {
    card.setAttribute('aria-label', `Plot ${metricNames[card.dataset.index]}`);
    card.addEventListener('click', () => { selector.value = card.dataset.index; drawMetricsChart(); });
  });
  requestJSON('/get_intervals').then(d => { $('data_interval').value = d.interval; }).catch(() => {});
  waveformChart = new Chart($('waveformChart').getContext('2d'), {type: 'line', data: {datasets: [{label: 'Time waveform (g vs. sample)', data: [], borderColor: '#1f8a8a', borderWidth: 1.5, pointRadius: 0}]}, options: {animation: false, scales: {y: {title: {display: true, text: 'Acceleration (g)'}}, x: {title: {display: true, text: 'Sample point'}}}}});
  spectrumChart = new Chart($('spectrumChart').getContext('2d'), {type: 'line', data: {datasets: [{label: 'Frequency spectrum (g vs. Hz)', data: [], borderColor: '#c2410c', borderWidth: 1.5, pointRadius: 0}, {label: 'Top peaks', data: [], backgroundColor: 'rgba(192,57,43,.85)', pointRadius: 6, showLine: false, type: 'scatter'}]}, options: {animation: false, scales: {y: {title: {display: true, text: 'Amplitude (g)'}, min: 0}, x: {type: 'linear', title: {display: true, text: 'Frequency (Hz)'}}}}});
  metricsHistoryChart = new Chart($('metricsHistoryChart').getContext('2d'), {type: 'line', data: {labels: [], datasets: [{data: [], borderColor: '#1765ad', borderWidth: 2, pointRadius: 2}]}, options: {animation: false, scales: {y: {title: {display: true}}, x: {title: {display: true, text: 'Time'}}}, plugins: {legend: {display: false}, customCanvasMessage: {display: true, text: 'Waiting for live data to build the trend…'}}}});
  updateClock(); setInterval(updateClock, 1000);
  drawMetricsChart();
  poll();
};
</script>
</body>
</html>
)html";
