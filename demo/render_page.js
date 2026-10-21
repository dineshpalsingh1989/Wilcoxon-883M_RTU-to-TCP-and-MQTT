// Render firmware HTML with explicit demo values, without executing C++ expressions.
const fs = require('fs');
const path = require('path');
const values = {
  sensor_model: 'Wilcoxon 883M', sensor_serial: 'DEMO-883M-001',
  wifiClass: 'connected', wifiText: 'Connected to Demo Wi-Fi', netTitle: 'Wi-Fi',
  mqttClass: 'connected', mqttText: 'Connected', mbClass: 'connected', mbText: 'Enabled',
  statusMsg: 'Connected to Demo Wi-Fi', ipMsg: '127.0.0.1',
  mqtt_server_str: 'demo.broker.local', mqtt_port_val: '1883', mqtt_user_str: '', mqtt_pass_str: '', mqtt_topic_str: '883M',
  mqttEnabled: true, mqtt_use_tls: false, mqtt_skip_cert_validation: false, mqtt_publish_progress: true,
  modbus_slave_id_val: '1', modbus_baud_rate_val: '115200', modbusTcpEnabled: true, modbusTcpPort: '502',
  gmtOffset_sec: '28800', d_model: 'Wilcoxon 883M', d_pn: '883M', d_serial: 'DEMO-883M-001',
  d_url: 'https://wilcoxon.com', d_fw: 'Demo sensor', lastResetReason: 'Demo startup',
  ipAddr: '127.0.0.1', ipAddress: '127.0.0.1', rssiVal: '-55 dBm', uptime: 'Demo session',
  FIRMWARE_VERSION: '4.8.0', localLog: 'Demo mode: no hardware connected.',
  // Read from the library so the demo shows the version the firmware is built with.
  WILCOXON883M_LIBRARY_VERSION: fs.readFileSync(path.join(__dirname, '../experiments/Wilcoxon883MRTU-beta/src/Wilcoxon883MVersion.h'), 'utf8')
    .match(/WILCOXON883M_LIBRARY_VERSION "([^"]+)"/)[1],
};
function expression(text) {
  let key = text.trim();
  if (key.startsWith('String(') && key.endsWith(')')) key = key.slice(7, -1);
  const ternary = key.match(/^(\w+)\s*\?\s*"([^"]*)"\s*:\s*"([^"]*)"$/);
  if (ternary) return values[ternary[1]] ? ternary[2] : ternary[3];
  if (key === 'getTimestamp()') return new Date().toISOString();
  if (key === 'gw::freeHeap()') return '180000';
  if (key.includes('internetConnected')) return 'Demo connection';
  if (!Object.hasOwn(values, key)) throw new Error('Missing demo value: ' + key);
  return String(values[key]);
}
module.exports = function renderPage(file, handler) {
  const source = fs.readFileSync(path.join(__dirname, '../src/web', file), 'utf8');
  const start = source.indexOf('void ' + handler + '(');
  if (start < 0) throw new Error('Unknown page handler: ' + handler);
  const rest = source.slice(start);
  const next = rest.slice(5).search(/\nvoid \w+\(/);
  const body = next < 0 ? rest : rest.slice(0, next + 5);
  const segments = [...body.matchAll(/R"rawliteral\(([\s\S]*?)\)rawliteral"/g)];
  let html = '';
  segments.forEach((segment, i) => {
    if (i) {
      const previous = segments[i - 1];
      const gap = body.slice(previous.index + previous[0].length, segment.index);
      const inline = gap.match(/^\s*\+\s*([\s\S]*?)\s*\+\s*$/);
      const chunk = gap.match(/\+=\s*(\w+)\s*\+\s*$/);
      const streamed = gap.match(/^\);\s*sendPageText\(([\s\S]*?)\);\s*server\.sendContent_P\($/);
      if (inline) html += expression(inline[1]);
      else if (chunk) html += expression(chunk[1]);
      else if (streamed) html += expression(streamed[1]);
    }
    html += segment[1];
  });
  return html.replace('<html lang="en">', '<html lang="en" data-demo="true">').replace('<body>', '<body><div style="position:fixed;bottom:8px;right:8px;z-index:1000;background:#173c49;color:white;padding:6px 12px;border-radius:6px;font:12px system-ui">DEMO · Hardware actions simulated <label>Scenario <select aria-label="Demo sensor scenario" onchange="fetch(\'/demo/scenario\',{method:\'POST\',headers:{\'Content-Type\':\'application/json\'},body:JSON.stringify({scenario:this.value})})"><option value="normal">Normal</option><option value="alarm">High vibration</option><option value="offline">Sensor offline</option></select></label></div>');
};
