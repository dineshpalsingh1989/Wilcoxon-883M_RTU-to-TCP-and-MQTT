// Demo server: serves the REAL dashboard HTML from src/web/web_dashboard.cpp
// and fakes the device API (/data etc.) with simulated vibration data.
// Run:  node demo/mock_server.js   then open http://localhost:8080
const http = require('http');
const renderPage = require('./render_page');
const monitoring = require('./monitoring');
const pages = {
  '/settings': ['web_settings.cpp', 'handleSettings'],
  '/wifi_settings': ['web_settings.cpp', 'handleWifiSettings'],
  '/mqtt_settings': ['web_settings.cpp', 'handleMqttSettings'],
  '/rs485_settings': ['web_settings.cpp', 'handleRs485Settings'],
  '/modbustcp_settings': ['web_modbus_settings.cpp', 'handleModbusTcpSettings'],
  '/time_settings': ['web_system.cpp', 'handleTimeSettings'],
  '/device_info': ['web_system.cpp', 'handleDeviceInfo'],
  '/github_updates': ['web_updates.cpp', 'handleGithubUpdates'],
};

const PORT = process.env.PORT || 8080;
const MODEL = 'Wilcoxon 883M';
const dashboardHtml = renderPage('web_dashboard.cpp', 'handleRoot');

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
  out.veloRms = { x: monitoring.latest.x, y: monitoring.latest.y, z: monitoring.latest.z };
  out.temperature = monitoring.latest.temperature;
  return out;
}
function sampleTimer() { monitoring.sample(); setTimeout(sampleTimer, state.metricsInterval); }
setTimeout(sampleTimer, state.metricsInterval);

let capturing = false;
function startCapture(axis) {
  if (capturing || monitoring.update.active) return false;
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
        if (monitoring.snapshot(capturing).health.consecutive_errors) {
          monitoring.capture(axis, 'Sensor timeout');
          state.status = 'Sensor timeout'; state.traffic = 'ERROR'; state.cycle = false; capturing = false;
          return;
        }
        state.status = `Capture complete for ${axis}-axis.`;
        monitoring.capture(axis);
        state.traffic = 'READY'; state.capturedAxis = axis; capturing = false;
        if (state.cycle && axis !== 'Z') startCapture(axis === 'X' ? 'Y' : 'Z');
        else state.cycle = false;
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

async function bodyJson(req) {
  let body = '';
  for await (const chunk of req) { body += chunk; if (body.length > 2048) throw new Error('Body too large'); }
  return JSON.parse(body);
}
http.createServer(async (req, res) => {
  const url = new URL(req.url, 'http://localhost');
  if (pages[url.pathname] && !(url.pathname === '/device_info' && url.searchParams.get('format') === 'json')) {
    try { return send(res, 200, 'text/html', renderPage(...pages[url.pathname])); }
    catch (error) { console.error(error.message); return send(res, 500, 'text/plain', 'Demo page rendering failed.'); }
  }
  switch (url.pathname) {
    case '/monitoring_ui.js':
      return send(res, 200, 'application/javascript', renderPage('web_monitoring.cpp', 'handleMonitoringScript'));
    case '/monitoring':
      return json(res, monitoring.snapshot(capturing));
    case '/demo/scenario':
    case '/alarm_settings': {
      if (req.method !== 'POST') return send(res, 405, 'text/plain', 'Use POST');
      try { const body = await bodyJson(req); const ok = url.pathname === '/alarm_settings' ? monitoring.saveAlarm(body) : monitoring.setScenario(body.scenario); return send(res, ok ? 200 : 400, 'text/plain', ok ? 'Demo settings saved for this session' : 'Invalid settings'); }
      catch { return send(res, 400, 'text/plain', 'Invalid JSON'); }
    }
    case '/demo/github_release':
      return json(res, { tag_name: 'v4.9.0', body: 'DEMO RELEASE\nSensor health, vibration trends, saved alarms, capture history and verified firmware installation.\nThis demonstration does not install firmware.', html_url: 'https://github.com/dineshpalsingh1989/Wilcoxon-883M_RTU-to-TCP-and-MQTT/releases/tag/v4.9.0', assets: [{ name: 'firmware-adafruit_feather_esp32s3.bin', digest: 'sha256:' + 'a'.repeat(64), browser_download_url: 'https://github.com/dineshpalsingh1989/Wilcoxon-883M_RTU-to-TCP-and-MQTT/releases/download/v4.9.0/firmware-adafruit_feather_esp32s3.bin' }] });
    case '/github_install': {
      if (req.method !== 'POST') return send(res, 405, 'text/plain', 'Use POST');
      try { const body = await bodyJson(req); if(capturing || !monitoring.install(body.tag)) return send(res, 409, 'text/plain', 'Capture/update busy or unsupported demo release'); return send(res, 202, 'application/json', '{"accepted":true}'); }
      catch { return send(res, 400, 'text/plain', 'Invalid JSON'); }
    }
    case '/github_update_status':
      return json(res, monitoring.update);
    case '/firmware_info':
      return json(res, { version: '4.8.0', board: 'adafruit_feather_esp32s3' });
    case '/scanwifi':
      return json(res, [{ssid:'Demo Wi-Fi',rssi:-55},{ssid:'Factory Network',rssi:-68}]);
    case '/mqtt_status':
      return json(res, { status: 'Demo: connected to demo.broker.local' });
    case '/savewifi': case '/save_mqtt': case '/save_rs485': case '/save_modbustcp': case '/save_time': case '/set_manual_time': case '/reboot': case '/clear_system_log':
      return send(res, 200, 'text/html', '<p>Demo only: action simulated. No device settings were changed.</p><a href="/settings">Back to Settings</a>');
    case '/update':
      req.resume();
      return send(res, 400, 'text/plain', 'Demo only: firmware installation is disabled.');
    case '/download_metrics_csv': {
      const data = metrics();
      res.setHeader('Content-Disposition', 'attachment; filename="demo-metrics.csv"');
      return send(res, 200, 'text/csv', 'timestamp,x_velocity_rms,y_velocity_rms,z_velocity_rms,temperature\n' + [new Date().toISOString(), data.veloRms.x, data.veloRms.y, data.veloRms.z, data.temperature].join(',') + '\n');
    }
    case '/': case '/home': case '/dashboard':
      return send(res, 200, 'text/html', dashboardHtml);
    case '/data':
      return json(res, {
        ...metrics(), status: state.status, traffic_light: state.traffic, capture_busy: capturing, cycle_active: !!state.cycle,
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
      if(capturing || monitoring.update.active) return send(res, 409, 'text/plain', 'Device is busy.');
      state.cycle = url.searchParams.get('cycle') === 'true';
      startCapture(axis);
      return send(res, 200, 'text/plain', 'Capture trigger accepted.');
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
