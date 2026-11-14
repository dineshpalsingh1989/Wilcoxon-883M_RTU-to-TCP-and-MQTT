// Real embedded Ethernet page, simulated HTTP responses: no firmware, W5500 or network change.
// Checks the ILA210 IOC page and, when present, the identical 883M IOC copy.
let playwright;
try { playwright = require('../.tools/node_modules/playwright'); }
catch { playwright = require('../../.tools/node_modules/playwright'); }
const {chromium} = playwright;
const fs = require('node:fs'), path = require('node:path'), http = require('node:http');
const assert = require('node:assert/strict');
const pages = [path.join(__dirname, '../IRIV IOC/src/web/network_page.h'),
  path.join(__dirname, '../../IRIV IOC/src/web/network_page.h')].filter(file => fs.existsSync(file));
const htmlOf = file => fs.readFileSync(file, 'utf8').match(/R"html\(([\s\S]*?)\)html"/)[1];

async function check(browser, html) {
  let state = {mode: 'dhcp', ip: '', mask: '', gateway: '', dns: '', fallback_ip: '192.168.4.1',
    current: {link: true, description: 'Ethernet (DHCP) (SIMULATED)', ip: '10.0.0.23', mask: '255.255.255.0',
      gateway: '10.0.0.1', dns: '10.0.0.1', mac: '00:08:DC:00:00:01', hostname: 'ila-gateway'}};
  let offline = false, payloads = [];
  const server = http.createServer((req, res) => {
    const send = (code, value, type = 'application/json') => {
      res.writeHead(code, {'Content-Type': type});
      res.end(type === 'application/json' ? JSON.stringify(value) : value);
    };
    if (req.url === '/wifi_settings') return send(200, html, 'text/html; charset=utf-8');
    if (req.url !== '/api/network') return send(404, 'Missing', 'text/plain');
    if (offline) return res.destroy();
    if (req.method === 'POST') {
      let body = '';
      req.on('data', chunk => body += chunk);
      req.on('end', () => {
        const payload = JSON.parse(body); payloads.push(payload);
        if (payload.ip === '192.168.1.255') return send(400, 'IP address is not a usable host address on this subnet.', 'text/plain');
        state = {...state, ...payload};
        send(200, payload.mode === 'static' ? 'Static IP saved. The gateway restarts now.' : 'DHCP selected. The gateway restarts now.', 'text/plain');
      });
      return;
    }
    send(200, state);
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const base = `http://127.0.0.1:${server.address().port}`;
  const page = await browser.newPage();
  const errors = [], external = [];
  page.on('pageerror', error => errors.push(error.message));
  page.on('request', request => { if (!request.url().startsWith(base)) external.push(request.url()); });
  try {
    await page.goto(base + '/wifi_settings');
    await page.waitForFunction(() => !document.getElementById('save').disabled);
    assert.equal(await page.locator('#desc').innerText(), 'Ethernet (DHCP) (SIMULATED)');
    assert.equal(await page.locator('#hostname').innerText(), 'ila-gateway.local');
    assert(await page.locator('#mode-dhcp').isChecked());
    assert(await page.locator('#static-fields').isHidden());
    assert.match(await page.locator('#dhcp-note').innerText(), /192\.168\.4\.1/);

    // DHCP save sends only the mode.
    await page.locator('#save').click();
    await page.waitForFunction(() => document.getElementById('result').textContent.includes('restarts'));
    assert.deepEqual(payloads.at(-1), {mode: 'dhcp'});

    // Static: keyboard selection, client-side validation, server-side rejection, then success.
    await page.reload();
    await page.waitForFunction(() => !document.getElementById('save').disabled);
    await page.locator('#mode-dhcp').focus();
    await page.keyboard.press('ArrowDown');
    assert(await page.locator('#mode-static').isChecked());
    assert(await page.locator('#static-fields').isVisible());
    assert.equal(await page.locator('#mask').inputValue(), '255.255.255.0');
    const before = payloads.length;
    await page.locator('#ip').fill('192.168.1.300');
    await page.locator('#save').click();
    assert.match(await page.locator('#result').innerText(), /valid IP address/);
    assert.equal(await page.evaluate(() => document.activeElement.id), 'ip');
    assert.equal(await page.locator('#ip').getAttribute('aria-invalid'), 'true');
    assert.equal(payloads.length, before, 'invalid address must not be sent');
    await page.locator('#ip').fill('192.168.1.255');
    await page.locator('#save').click();
    await page.waitForFunction(() => document.getElementById('result').textContent.startsWith('Save failed'));
    assert(!(await page.locator('#save').isDisabled()), 'save stays available after a rejected address');
    await page.locator('#ip').fill(' 192.168.1.50 ');
    await page.locator('#gateway').fill('192.168.1.1');
    await page.locator('#save').click();
    await page.waitForFunction(() => document.getElementById('result').textContent.includes('http://192.168.1.50/'));
    assert.deepEqual(payloads.at(-1), {mode: 'static', ip: '192.168.1.50', mask: '255.255.255.0', gateway: '192.168.1.1', dns: ''});

    // Saved static settings are shown on the next load.
    await page.reload();
    await page.waitForFunction(() => !document.getElementById('save').disabled);
    assert(await page.locator('#mode-static').isChecked());
    assert.equal(await page.locator('#ip').inputValue(), '192.168.1.50');

    // Mobile width without horizontal scroll.
    await page.setViewportSize({width: 360, height: 740});
    assert(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), 'mobile overflow');

    // Offline: the page reports it and keeps Save disabled.
    offline = true;
    await page.reload();
    await page.waitForFunction(() => document.getElementById('result').textContent.startsWith('Unable to load'));
    assert(await page.locator('#save').isDisabled());
    assert.deepEqual(external, []);
    assert.deepEqual(errors, []);
  } finally { await page.close(); server.close(); }
}

(async () => {
  assert(pages.length > 0, 'network_page.h not found');
  const browser = await chromium.launch({headless: true, executablePath: process.env.CHROME_PATH || 'C:/Program Files/Google/Chrome/Application/chrome.exe'});
  try {
    for (const file of pages) {
      await check(browser, htmlOf(file));
      console.log('PASS (simulated):', path.relative(path.join(__dirname, '../..'), file));
    }
    console.log('PASS: Ethernet page DHCP/static save, validation, server rejection, keyboard, mobile width, offline, no external requests.');
  } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exitCode = 1; });
