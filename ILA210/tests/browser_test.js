// Simulated browser checks; these do not validate sensor hardware or RTU registers.
let playwright;
try { playwright = require('../.tools/node_modules/playwright'); }
catch { playwright = require('../../.tools/node_modules/playwright'); }
const { chromium } = playwright;
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const http = require('node:http');
const root = path.resolve(__dirname, '..');
const boardRoot = path.resolve(root, process.env.ILA_BOARD_DIR || 'IRIV IOC');
const source = fs.readFileSync(path.join(boardRoot, 'src/main.cpp'), 'utf8');
function htmlFor(handler) {
  const start = source.indexOf(`void ${handler}() {`);
  const end = source.indexOf('\nvoid ', start + 6);
  return [...source.slice(start, end < 0 ? undefined : end).matchAll(/R"rawliteral\(([\s\S]*?)\)rawliteral"/g)]
    .map(match => match[1]).join('');
}
const charts = fs.readFileSync(path.join(boardRoot, 'src/local_charts.h'), 'utf8').match(/R"js\(([\s\S]*?)\)js"/)[1];
const keys = ['accelRms1','accelPeak1','accelRms2','accelPeak2','veloRms','veloPeak','displRms','displPeak','truePeak','crestFactor','stdDeviation','temperature','driveVoltage','boardTemperature'];
let offline = false, failWaveform = false, validMetrics = true, busyUntil = 0, captureCount = 0;
const server = http.createServer((req, res) => {
  const url = new URL(req.url, 'http://localhost');
  const send = (body, type = 'application/json', status = 200) => {
    res.writeHead(status, {'Content-Type': type});
    res.end(type === 'application/json' ? JSON.stringify(body) : body);
  };
  if (url.pathname === '/') return send(htmlFor('handleRoot'), 'text/html; charset=utf-8');
  if (url.pathname === '/charts.js') return send(charts, 'application/javascript');
  if (url.pathname === '/wifi_settings') return send(htmlFor('handleWifiSettings'), 'text/html; charset=utf-8');
  if (url.pathname === '/mqtt_settings') return send(htmlFor('handleMqttSettings'), 'text/html; charset=utf-8');
  if (offline) { res.destroy(); return; }
  if (url.pathname === '/data') {
    const busy = Date.now() < busyUntil;
    return send({...Object.fromEntries(keys.map(key => [key, 1.25])),
      status: busy ? 'Requesting new data...' : 'Monitoring (SIMULATED)',
      capture_busy: busy, newWaveformAvailable: !busy, newSpectrumAvailable: !busy,
      metrics_valid: validMetrics, metrics_age_ms: 100,
      mqtt_connected: false, storage_mode: 'Internal (Limited)'});
  }
  if (url.pathname === '/trigger_waveform') {
    if (Date.now() < busyUntil) return send('Busy', 'text/plain', 409);
    busyUntil = Date.now() + 2000; captureCount++;
    return send('Accepted', 'text/plain');
  }
  if (url.pathname === '/fullwaveform') return failWaveform
    ? send({error: 'busy'}, 'application/json', 503)
    : send(Array.from({length: 25600}, (_, i) => Math.sin(i / 10)));
  if (url.pathname === '/fullspectrum') return send({points: [0, .1, .5, .2], labels: [0, 1.5625, 3.125, 4.6875]});
  if (url.pathname === '/spectrum_analysis') return send({peaks: []});
  if (url.pathname === '/get_event_log') return send({files: []});
  if (url.pathname === '/get_log_data' || url.pathname === '/get_metrics_log_data')
    return send({is_recording: false, log_count: 0, max_log_size: 30, max_baseline_samples: 10, log: []});
  if (url.pathname === '/get_intervals') return send({interval: 600000});
  if (url.pathname === '/scanwifi') return send([{ssid: '<img src=x onerror="window.injected=true">', rssi: -50}]);
  return send('Not found', 'text/plain', 404);
});
(async () => {
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const base = `http://127.0.0.1:${server.address().port}`;
  const browser = await chromium.launch({headless: true,
    executablePath: process.env.CHROME_PATH || 'C:/Program Files/Google/Chrome/Application/chrome.exe'});
  try {
    const page = await browser.newPage({viewport: {width: 1440, height: 1000}});
    const errors = [], external = [];
    page.on('pageerror', error => errors.push(error.message));
    page.on('dialog', dialog => dialog.accept());
    await page.route('**/*', route => {
      if (!route.request().url().startsWith(base)) { external.push(route.request().url()); return route.abort(); }
      return route.continue();
    });
    await page.goto(base);
    await page.waitForFunction(() => document.getElementById('veloPeak')?.textContent.includes('1.25'));
    assert.match(await page.locator('#temperature').innerText(), /°C/);
    assert(await page.locator('#metricsHistoryChart').evaluate(canvas => canvas.width > 200));
    validMetrics = false;
    await page.evaluate(() => updateDashboardData());
    assert.equal(await page.locator('#veloPeak').innerText(), '--');
    assert.match(await page.locator('#lastUpdated').innerText(), /No valid sensor reading/);
    validMetrics = true;
    await page.evaluate(() => updateDashboardData());
    await page.getByRole('button', {name: 'Waveform & Spectrum', exact: true}).click();
    await page.getByRole('button', {name: 'View Full Waveform', exact: true}).click();
    await page.waitForFunction(() => document.getElementById('waveformLastUpdated').textContent.includes('Last updated:'));
    failWaveform = true;
    await page.getByRole('button', {name: 'View Full Waveform', exact: true}).click();
    await page.waitForFunction(() => document.getElementById('waveformLastUpdated').textContent.includes('Load failed'));
    assert(await page.getByRole('button', {name: 'View Full Waveform', exact: true}).isEnabled());
    failWaveform = false;
    await page.getByRole('button', {name: 'Collect Full Data Now'}).click();
    await page.waitForFunction(() => document.getElementById('status').textContent.includes('Requesting'));
    assert.equal(captureCount, 1);
    await page.evaluate(() => updateDashboardData());
    assert(await page.getByRole('button', {name: 'Loading...'}).first().isDisabled());
    busyUntil = 0;
    await page.evaluate(() => updateDashboardData());
    await page.waitForFunction(() => document.getElementById('status').textContent.includes('Monitoring'));
    offline = true;
    await page.evaluate(() => updateDashboardData());
    assert.match(await page.locator('#status').innerText(), /Connection lost/);
    offline = false;
    await page.evaluate(() => updateDashboardData());
    assert.match(await page.locator('#status').innerText(), /Monitoring/);
    await page.setViewportSize({width: 390, height: 844});
    for (const name of ['Overall Metrics', 'Waveform & Spectrum', 'Spectrum Analysis', 'Data Logger']) {
      await page.getByRole('button', {name, exact: true}).click();
      assert(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), `Mobile overflow: ${name}`);
    }
    await page.keyboard.press('Tab');
    assert(await page.evaluate(() => document.activeElement !== document.body));
    await page.goto(base + '/wifi_settings');
    await page.evaluate(() => scanWifi());
    await page.waitForFunction(() => document.querySelector('#wifi-list').textContent.includes('<img'));
    assert.equal(await page.locator('#wifi-list img').count(), 0);
    assert.equal(await page.evaluate(() => !!window.injected), false);
    await page.goto(base + '/mqtt_settings');
    assert.equal(await page.locator('#pass').inputValue(), '');
    assert.deepEqual(external, []);
    assert.deepEqual(errors, []);
    console.log('PASS: simulated desktop/mobile, local charts, invalid sensor readings, full waveform, capture busy, failed download/retry, offline/recovery, keyboard focus, SSID injection, blank broker password.');
  } finally { await browser.close(); server.close(); }
})().catch(error => { console.error(error); server.close(); process.exitCode = 1; });
