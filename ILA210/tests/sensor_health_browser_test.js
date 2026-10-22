// Real embedded page, simulated HTTP responses: no firmware or physical I/O.
let playwright;
try { playwright = require('../.tools/node_modules/playwright'); }
catch { playwright = require('../../.tools/node_modules/playwright'); }
const {chromium} = playwright;
const fs = require('node:fs'), path = require('node:path'), http = require('node:http');
const assert = require('node:assert/strict');
const html = fs.readFileSync(path.join(__dirname, '../src/web/sensor_health_page.h'), 'utf8')
  .match(/R"html\(([\s\S]*?)\)html"/)[1];
let state = {timeout_ms:10000, fault_alarm:true, output_mode:0,
  sensor_fault:true, sample_seen:false, gateway_id:'simulated-board-001'};
let offline = false, failSave = false, saved = 0, lastPayload;
const server = http.createServer((req, res) => {
  const send = (code, value, type = 'application/json') => {
    res.writeHead(code, {'Content-Type':type});
    res.end(type === 'application/json' ? JSON.stringify(value) : value);
  };
  if (req.url === '/sensor_fault_settings') return send(200, html, 'text/html; charset=utf-8');
  if (req.url !== '/api/sensor_health') return send(404, 'Missing', 'text/plain');
  if (offline) return res.destroy();
  if (req.method === 'POST') {
    let body = '';
    req.on('data', chunk => body += chunk);
    req.on('end', () => {
      lastPayload = JSON.parse(body);
      if (failSave) return send(503, 'Settings could not be stored; no changes applied.', 'text/plain');
      state = {...state, ...lastPayload}; ++saved;
      send(200, state);
    });
    return;
  }
  send(200, state);
});
(async () => {
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const base = `http://127.0.0.1:${server.address().port}`;
  let browser;
  try {
    browser = await chromium.launch({headless:true,
      executablePath:process.env.CHROME_PATH || 'C:/Program Files/Google/Chrome/Application/chrome.exe'});
    const page = await browser.newPage(), errors = [], external = [];
    page.on('pageerror', error => errors.push(error.message));
    await page.route('**/*', route => {
      if (!route.request().url().startsWith(base)) {
        external.push(route.request().url()); return route.abort();
      }
      return route.continue();
    });
    for (const viewport of [{width:1440,height:1000}, {width:390,height:844}]) {
      await page.setViewportSize(viewport);
      await page.goto(base + '/sensor_fault_settings');
      await page.waitForFunction(() => !document.getElementById('save').disabled);
      assert.equal(await page.locator('#timeout').inputValue(), String(state.timeout_ms));
      assert.equal(await page.locator('#fault').isChecked(), state.fault_alarm);
      assert.equal(await page.locator('#mode').inputValue(), String(state.output_mode));
      assert.match(await page.locator('#status').textContent(), /LOST/);
      assert.match(await page.locator('#identity').textContent(), /simulated-board-001/);
      assert(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), 'horizontal overflow');
      await page.keyboard.press('Tab');
      assert(await page.evaluate(() => document.activeElement.tagName === 'A'), 'keyboard link focus');
      await page.locator('#timeout').fill('25000');
      await page.locator('#fault').uncheck();
      await page.locator('#mode').selectOption('2');
      const beforeSave = saved;
      await page.locator('#save').focus();
      await page.keyboard.press('Enter');
      await page.waitForFunction(() => document.getElementById('result').textContent === 'Settings saved and applied.');
      assert.equal(saved, beforeSave + 1);
      assert.deepEqual(lastPayload, {timeout_ms:25000, fault_alarm:false, output_mode:2});
      failSave = true;
      await page.locator('#timeout').fill('30000');
      await page.locator('#save').click();
      await page.waitForFunction(() => document.getElementById('result').textContent.startsWith('Save failed:'));
      assert.match(await page.locator('#result').textContent(), /could not be stored/);
      assert.equal(state.timeout_ms, 25000);
      assert.equal(await page.locator('#timeout').inputValue(), '30000');
      assert(await page.locator('#save').isEnabled());
      failSave = false;
      offline = true;
      await page.locator('#save').click();
      await page.waitForFunction(() => document.getElementById('result').textContent.includes('Failed to fetch'));
      assert.equal(state.timeout_ms, 25000);
      assert(await page.locator('#save').isEnabled());
      await page.reload();
      await page.waitForFunction(() => document.getElementById('result').textContent.startsWith('Unable to load:'));
      assert(await page.locator('#save').isDisabled());
      offline = false;
    }
    assert.deepEqual(errors, []);
    assert.deepEqual(external, []);
    console.log('PASS: real sensor-health page with simulated API: desktop/mobile, initial state, keyboard save, HTTP503 and offline errors, unchanged failed-save state, local assets and no overflow.');
  } finally {
    if (browser) await browser.close();
    await new Promise(resolve => server.close(resolve));
  }
})().catch(error => { console.error(error); server.close(); process.exitCode = 1; });
