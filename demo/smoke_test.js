// Run with: node demo/smoke_test.js
const assert = require('node:assert/strict');
const vm = require('node:vm');
const renderPage = require('./render_page');
const pages = [
  ['web_dashboard.cpp', 'handleRoot'], ['web_settings.cpp', 'handleSettings'],
  ['web_settings.cpp', 'handleWifiSettings'], ['web_settings.cpp', 'handleMqttSettings'],
  ['web_settings.cpp', 'handleRs485Settings'], ['web_modbus_settings.cpp', 'handleModbusTcpSettings'],
  ['web_system.cpp', 'handleTimeSettings'], ['web_system.cpp', 'handleDeviceInfo'],
  ['web_updates.cpp', 'handleGithubUpdates'],
];
for (const [file, handler] of pages) {
  const html = renderPage(file, handler);
  assert(html.includes('</html>'), handler);
  assert(!html.includes(')rawliteral'), handler);
  for (const script of html.matchAll(/<script>([\s\S]*?)<\/script>/g)) new vm.Script(script[1]);
}
new vm.Script(renderPage('web_monitoring.cpp', 'handleMonitoringScript'));
const html = renderPage('web_updates.cpp', 'handleGithubUpdates');
const script = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)][0][1];
async function check(response, expected, downloadVisible) {
  const elements = {};
  const context = {
    URL,
    document: { documentElement: { dataset: {} }, getElementById(id) { return elements[id] ||= {}; } },
    setInterval() {}, AbortSignal,
    fetch: async url => url === '/firmware_info' ? { ok: true, json: async () => ({ version: '4.8.0' }) } : url === '/github_update_status' ? { ok: true, json: async () => ({state:'idle',message:'Idle',progress:0,active:false}) } : response,
  };
  vm.createContext(context);
  vm.runInContext(script, context);
  await context.checkRelease();
  assert.match(elements.message.textContent, expected);
  assert.equal(elements.download.hidden, !downloadVisible);
  assert.equal(elements.check.disabled, false);
}
(async () => {
  await check({ status: 404 }, /No public release/, false);
  await check({ status: 403 }, /rate limit/, false);
  const base = 'https://github.com/dineshpalsingh1989/Wilcoxon-883M_RTU-to-TCP-and-MQTT/releases/';
  const release = { tag_name: 'v4.9.0', html_url: base + 'tag/v4.9.0', assets: [] };
  await check({ ok: true, json: async () => release }, /binary.*missing/, false);
  release.assets.push({ name: 'firmware-adafruit_feather_esp32s3.bin', browser_download_url: base + 'download/v4.9.0/firmware-adafruit_feather_esp32s3.bin' });
  await check({ ok: true, json: async () => release }, /Latest release loaded/, true);
  release.assets[0].browser_download_url = 'https://example.com/firmware.bin';
  await check({ ok: true, json: async () => release }, /Unexpected release URL/, false);
  console.log('PASS: nine page renders and script syntax; GitHub release success, missing asset, no release, rate limit, and URL validation.');
})().catch(error => { console.error(error); process.exitCode = 1; });
