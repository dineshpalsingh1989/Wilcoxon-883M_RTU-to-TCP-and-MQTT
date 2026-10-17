// Install locally: npm.cmd install --prefix .tools --no-package-lock playwright
// Run against the demo: node demo/browser_test.js
const { chromium } = require('../.tools/node_modules/playwright');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const base = process.env.DEMO_URL || 'http://localhost:8090';
const executablePath = process.env.CHROME_PATH || 'C:/Program Files/Google/Chrome/Application/chrome.exe';
(async () => {
  const browser = await chromium.launch({ executablePath, headless: true });
  try {
    const page = await browser.newPage({ viewport: { width: 1440, height: 1000 } });
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    page.on('dialog', dialog => dialog.accept());
    // The external icon library is optional; keep these checks independent of CDN availability.
    await page.route('https://unpkg.com/**', route => route.fulfill({ body: '' }));
    await page.goto(base);
    await page.waitForFunction(() => document.getElementById('sensor-health').textContent === 'Connected');
    assert.match(await page.locator('#summary-x').innerText(), /\d+\.\d+/);
    assert(await page.locator('#velocity-chart').evaluate(c => c.width > 100));
    await page.locator('summary').click();
    await page.locator('#alarm-enabled').check();
    await page.locator('#alarm-delay').fill('1');
    await page.getByRole('button', { name: 'Save alarm settings' }).click();
    await page.getByLabel('Demo sensor scenario').selectOption('alarm');
    await page.waitForFunction(() => document.getElementById('alarm-state').textContent === 'Alarm active', { timeout: 15000 });
    await page.getByLabel('Demo sensor scenario').selectOption('offline');
    await page.waitForFunction(() => document.getElementById('alarm-state').textContent.includes('Unknown'));
    await page.getByLabel('Demo sensor scenario').selectOption('normal');
    await page.waitForFunction(() => document.getElementById('sensor-health').textContent === 'Connected');
    await page.getByRole('button', { name: 'Control & Transmission' }).click();
    await page.getByRole('button', { name: 'Capture Selected' }).click();
    await page.waitForFunction(() => document.getElementById('capture-history').textContent.includes('Complete'), { timeout: 20000 });
    assert.match(await page.locator('#capture-history').innerText(), /publish attempted/);
    await page.getByRole('button', { name: 'Overall Metrics' }).click();
    fs.mkdirSync('demo/artifacts', { recursive: true });
    await page.screenshot({ path: 'demo/artifacts/dashboard-desktop.png', fullPage: true });
    await page.setViewportSize({ width: 390, height: 844 });
    assert(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth));
    await page.screenshot({ path: 'demo/artifacts/dashboard-mobile.png', fullPage: true });
    for (const route of ['/settings','/wifi_settings','/mqtt_settings','/rs485_settings','/modbustcp_settings','/time_settings','/device_info']) {
      const response = await page.goto(base + route); assert.equal(response.status(), 200, route);
      assert(await page.locator('h1,h2').first().isVisible(), route);
    }
    await page.goto(base + '/github_updates');
    await page.getByRole('button', { name: 'Check for release' }).click();
    await page.getByRole('button', { name: 'Simulate verified update' }).click();
    await page.waitForFunction(() => document.getElementById('update-status').textContent.includes('Demo update complete'), { timeout: 15000 });
    await page.screenshot({ path: 'demo/artifacts/github-update.png', fullPage: true });
    assert.deepEqual(errors, []);
    console.log('PASS: desktop/mobile dashboard, charts, alarm save/trigger/offline/recovery, capture history, all settings pages, simulated verified update, no browser errors.');
  } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exitCode = 1; });
