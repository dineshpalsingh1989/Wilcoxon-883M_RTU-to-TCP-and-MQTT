// Demo server: serves the REAL dashboard HTML from src/web/web_dashboard.cpp
// and fakes the device API (/data etc.) with simulated vibration data.
// Run:  node demo/mock_server.js   then open http://localhost:8080
const http = require('http');
const fs = require('fs');
const path = require('path');

const PORT = process.env.PORT || 8080;
const MODEL = 'Wilcoxon 883M';
const src = fs.readFileSync(path.join(__dirname, '..', 'src', 'web', 'web_dashboard.cpp'), 'utf8');

// Rebuild handleRoot()'s page: raw-string segments joined by the sensor_model variable.
const rootBody = src.slice(src.indexOf('void handleRoot()'), src.indexOf('void handleMetrics()'));
const segments = [...rootBody.matchAll(/R"rawliteral\(([\s\S]*?)\)rawliteral"/g)].map(m => m[1]);
const dashboardHtml = segments.join(MODEL);

// ---- simulated device state ----
const state = {
  status: 'Monitoring', traffic: 'IDLE', capturedAxis: 'NONE',
  metricsInterval: 2000, captureInterval: 600000, t0: Date.now(),
};
const GROUPS = {
  accelRms: [0.12, 2], accelPeak: [0.4, 2], accelRms2: [0.1, 2], accelPeak2: [0.35, 2],
  accelMetricRms: [1.2, 2], accelMetricPeak: [3.9, 2], accelMetricRms2: [1.0, 2], accelMetricPeak2: [3.4, 2],
  veloRms: [2.1, 2], veloPeak: [6.0, 2], veloWideRms: [2.4, 2], veloWidePeak: [6.8, 2],
  displRms: [14, 2], displPeak: [40, 2], displLowRms: [15, 2], displLowPeak: [42, 2],
  truePeak: [0.5, 2], crestFactor: [3.1, 2], stdDeviation: [0.12, 2],
};
const noisy = base => base * (1 + (Math.random() - 0.5) * 0.1);

function metrics() {
  const out = {};
  for (const [k, [base]] of Object.entries(GROUPS)) {
    out[k] = { x: noisy(base), y: noisy(base * 0.8), z: noisy(base * 1.2) };
  }
  out.temperature = 31 + Math.random();
  return out;
}

let capturing = false;
function startCapture(axis) {
  if (capturing) return false;
  capturing = true;
  state.status = 'Sensor Calculating...'; state.traffic = 'TRIGGERED';
  setTimeout(() => {
    let pct = 0;
    state.traffic = 'BUSY';
    const timer = setInterval(() => {
      pct += 10;
      state.status = `Downloading... (${pct}%)`;
      if (pct >= 100) {
        clearInterval(timer);
        state.status = `Capture complete for ${axis}-axis.`;
        state.traffic = 'READY'; state.capturedAxis = axis; capturing = false;
        setTimeout(() => { if (!capturing) state.status = 'Monitoring'; }, 3000);
      }
    }, 400);
  }, 1000);
  return true;
}

function send(res, code, type, body) {
  res.writeHead(code, { 'Content-Type': type });
  res.end(body);
}
const json = (res, obj) => send(res, 200, 'application/json', JSON.stringify(obj));

http.createServer((req, res) => {
  const url = new URL(req.url, 'http://localhost');
  switch (url.pathname) {
    case '/': case '/home': case '/dashboard':
      return send(res, 200, 'text/html', dashboardHtml);
    case '/data':
      return json(res, {
        ...metrics(), status: state.status, traffic_light: state.traffic,
        mqtt_server: 'demo.broker.local', mqtt_topic: '883M', modbus_enabled: true,
        modbus_port: 502, modbus_id: 1, capturedAxis: state.capturedAxis, mqtt_connected: true,
      });
    case '/get_intervals':
      return json(res, { interval: state.captureInterval, metrics_interval: state.metricsInterval });
    case '/set_metrics_interval':
      state.metricsInterval = parseInt(url.searchParams.get('interval')) || 2000;
      return send(res, 200, 'text/plain', 'OK');
    case '/set_intervals':
      state.captureInterval = parseInt(url.searchParams.get('interval')) || 600000;
      return send(res, 200, 'text/plain', 'Interval set.');
    case '/trigger_capture': {
      const axis = url.searchParams.get('axis');
      if (!['X', 'Y', 'Z'].includes(axis)) return send(res, 400, 'text/plain', 'Invalid or missing axis parameter.');
      return send(res, 200, 'text/plain', startCapture(axis) ? 'Capture trigger accepted.' : 'Device is busy.');
    }
    case '/device_info':
      return json(res, {
        uptime: Math.floor((Date.now() - state.t0) / 1000) + 's', ram: '180000 bytes', log: 'Demo mode: no real device.\n',
        time: new Date().toISOString(), ip: '127.0.0.1', rssi: '-55 dBm', status: 'Demo', connected: true,
      });
    default:
      return send(res, 404, 'text/plain', 'Not available in demo mode: ' + url.pathname);
  }
}).listen(PORT, () => console.log(`Demo dashboard running at http://localhost:${PORT}`));
