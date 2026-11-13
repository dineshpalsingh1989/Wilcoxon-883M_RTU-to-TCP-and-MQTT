// Run with DEMO_WEB_DIR pointing at IRIV IOC/src/web and the demo server running.
const { chromium } = require('../.tools/node_modules/playwright');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const base = process.env.DEMO_URL || 'http://localhost:8090';
const executablePath = process.env.CHROME_PATH || 'C:/Program Files/Google/Chrome/Application/chrome.exe';

(async () => {
  const browser = await chromium.launch({ executablePath, headless: true });
  try {
    const page = await browser.newPage({ viewport: { width: 1280, height: 900 } });
    const errors = [];
    page.on('pageerror', e => errors.push(e.message));
    await page.goto(base + '/');
    await page.locator('#analog-overview:visible').waitFor();
    assert.match(await page.locator('#analog-card-0').innerText(), /10\.00 mm\/s/);
    assert.match(await page.locator('#analog-card-0').innerText(), /12\.00 mA/);
    fs.mkdirSync('demo/artifacts', { recursive: true });
    await page.screenshot({ path: 'demo/artifacts/ioc-dashboard-desktop.png', fullPage: true });
    await page.setViewportSize({ width: 390, height: 844 });
    assert(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth));
    await page.screenshot({ path: 'demo/artifacts/ioc-dashboard-mobile.png', fullPage: true });
    await page.setViewportSize({ width: 1280, height: 900 });
    const response = await page.goto(base + '/io');
    assert.equal(response.status(), 200);
    await page.locator('#an0_model').waitFor();
    fs.mkdirSync('demo/artifacts', { recursive: true });
    await page.screenshot({ path: 'demo/artifacts/ioc-io-desktop.png', fullPage: true });
    assert.equal(await page.locator('#an0_model').inputValue(), '7');
    assert.equal(await page.locator('#an0_max').inputValue(), '20');
    await page.locator('#an0_model').selectOption('2');
    assert.equal(await page.locator('#an0_max').inputValue(), '25.4');
    assert.equal(await page.locator('#an0_unit').inputValue(), 'mm/s');
    await page.locator('#an0_model').selectOption('0');
    await page.locator('#an0_max').fill('30');
    await page.locator('#save-analog').click();
    await page.locator('#save-status').getByText('Demo: saved/acknowledged (simulated)').waitFor();
    await page.locator('#di2_act').selectOption('3');
    assert(await page.locator('#di2_target').isVisible());
    await page.locator('#di2_do').selectOption('2');
    await page.locator('#save-inputs').click();
    await page.locator('#in-status').getByText('Demo: saved/acknowledged (simulated)').waitFor();
    await page.locator('#do2_src').selectOption('4');
    await page.locator('#do3_src').selectOption('4');
    assert.equal(await page.locator('#do3_src').inputValue(), '4');
    await page.setViewportSize({ width: 390, height: 844 });
    assert(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth));
    await page.screenshot({ path: 'demo/artifacts/ioc-io-mobile.png', fullPage: true });
    await page.locator('#di0_act').focus();
    assert.equal(await page.evaluate(() => document.activeElement.id), 'di0_act');
    await page.route('**/io_status', route => route.fulfill({ status: 503, body: 'Gateway offline' }));
    await page.waitForFunction(() => document.getElementById('ack-status').textContent.includes('Gateway unavailable'), { timeout: 5000 });
    assert.deepEqual(errors, []);
    console.log('PASS: IOC model scaling, manual scale, DI actions, save controls, mobile width and focus (simulated).');
  } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exitCode = 1; });
