// Tests the real installer UI with simulated releases; never accesses USB.
const {chromium}=require('../.tools/node_modules/playwright');
const fs=require('node:fs'),http=require('node:http'),assert=require('node:assert/strict');
const crypto=require('node:crypto');
const firmware=Buffer.from('883M');
const checksum=crypto.createHash('sha256').update(firmware).digest('hex');
let channels=[{sensor:'883m',channel:'stable',tag:'v5.0.0',manifest:'stable/manifest.json',checksums:{'cytron_maker_esp32/firmware.bin':checksum},notes:'https://example.com/notes',uf2:{cytron_iriv_io_controller:'https://example.com/883m.uf2'}},
 {sensor:'ila210',channel:'beta',tag:'v5.0.1-beta.1',manifest:'ila210/beta/manifest.json',notes:'https://example.com/notes',uf2:{cytron_iriv_io_controller:'https://example.com/ila210.uf2'}}];
const server=http.createServer((req,res)=>{
  if(req.url.startsWith('/channels.json')){res.setHeader('Content-Type','application/json');res.end(JSON.stringify({repo:'https://github.com/example/gateway',registry_endpoint:'https://registry.example/installations',channels}));}
  else if(req.url==='/stable/manifest.json'){res.setHeader('Content-Type','application/json');res.end(JSON.stringify({builds:[{chipFamily:'ESP32',parts:[{path:'cytron_maker_esp32/firmware.bin',offset:65536}]}]}));}
  else if(req.url==='/stable/cytron_maker_esp32/firmware.bin'){res.setHeader('Content-Type','application/octet-stream');res.end(firmware);}
  else {res.setHeader('Content-Type','text/html');res.end(fs.readFileSync('installer/index.html'));}
});
(async()=>{
 await new Promise(r=>server.listen(0,'127.0.0.1',r));
 const browser=await chromium.launch({headless:true,executablePath:process.env.CHROME_PATH||'C:/Program Files/Google/Chrome/Application/chrome.exe'});
 try{
  const page=await browser.newPage(),errors=[];page.on('pageerror',e=>errors.push(e.message));
  let savedRecord=null;
  await page.route('https://registry.example/installations',async route=>{
    savedRecord=JSON.parse(route.request().postData());
    assert.equal(route.request().headers()['x-install-key'],'test-key');
    await route.fulfill({status:201,contentType:'application/json',body:'{"saved":true,"issue_url":"https://github.com/example/private-records/issues/12"}'});
  });
  await page.addInitScript(() => {
    Object.defineProperty(navigator, 'serial', {value: {requestPort: async () => ({
      open: async () => {}, close: async () => {}, readable: {getReader: () => {
        let reads = 0;
        return {read: async () => {
          const chunks=['ESP32 ready\nINSTALL_INFO {"board":"maker","version":"v5.0.0","board_id":"AABB',
            'CC","gateway_serial":"GW-ESP32-AABBCC","wifi_mac":"AA:BB:CC:DD:EE:FF","sensor_serial":"883M123"}\n'];
          return reads < chunks.length ? {value:new TextEncoder().encode(chunks[reads++]),done:false} : {done:true};
        }, cancel: async () => {}, releaseLock: () => {}};
      }}
    })}});
  });
  await page.route('https://unpkg.com/**',r=>r.fulfill({contentType:'application/javascript',body:''}));
  await page.route('https://unpkg.com/esptool-js@0.7.1/bundle.js',r=>r.fulfill({contentType:'application/javascript',headers:{'access-control-allow-origin':'*'},body:`export class Transport {constructor(port){this.port=port} async disconnect(){}} export class ESPLoader {constructor(){this.chip={CHIP_NAME:'ESP32'}} async main(){return 'ESP32'} async detectFlashSize(){return window.__flashSize||'8MB'} async after(){}}`}));
  await page.goto(`http://127.0.0.1:${server.address().port}`);
  await page.waitForSelector('#channels esp-web-install-button');
  assert.equal(await page.locator('#channels esp-web-install-button').getAttribute('manifest'),'stable/manifest.json');
  assert(await page.locator('#channels button.install').isDisabled());
  await page.evaluate(()=>window.__flashSize='unknown');
  await page.locator('#channels .preflight-start').click();
  await page.waitForFunction(()=>document.querySelector('#channels .preflight-status').textContent.includes('Flash size could not be read'));
  assert(await page.locator('#channels button.install').isDisabled());
  await page.evaluate(()=>window.__flashSize='4MB');
  await page.locator('#channels .preflight-start').click();
  await page.waitForFunction(()=>document.querySelector('#channels .preflight-status').textContent.includes('Only 4MB'));
  assert(await page.locator('#channels button.install').isDisabled());
  await page.evaluate(()=>window.__flashSize='8MB');
  await page.locator('#channels .preflight-start').click();
  await page.waitForFunction(()=>document.querySelector('#channels .preflight-status').textContent.includes('Firmware images verified'));
  await page.locator('#channels .preflight-confirm input').check();
  await page.locator('#device-name').fill('Pump 3 gateway');
  await page.locator('#company-name').fill('ABC Manufacturing');
  assert(await page.locator('#channels button.install').isEnabled());
  assert(await page.locator('#serial-console').isVisible());
  assert(await page.getByText('Check your board and wiring before flashing').isVisible());
  await page.locator('#registry-key').fill('test-key');
  await page.locator('#registry-key').blur();
  await page.getByRole('button',{name:'Connect to USB'}).click();
  await page.waitForFunction(() => document.querySelector('#serial-output').textContent.includes('ESP32 ready'));
  await page.waitForFunction(() => !document.querySelector('#receipt-download').disabled);
  assert((await page.locator('#device-summary').textContent()).includes('AA:BB:CC:DD:EE:FF'));
  await page.waitForFunction(() => document.querySelector('#registry-status').textContent.includes('Saved automatically'));
  assert.equal(savedRecord.sensor_serial,'883M123');
  assert.equal(savedRecord.gateway_serial,'GW-ESP32-AABBCC');
  assert.equal(savedRecord.device_name,'Pump 3 gateway');
  assert.equal(savedRecord.company_name,'ABC Manufacturing');
  assert.equal(await page.locator('#registry-link a').getAttribute('href'),'https://github.com/example/private-records/issues/12');
  await page.getByRole('button',{name:'Clear logs'}).click();
  assert.equal(await page.locator('#serial-output').textContent(),'');
  await page.locator('.board[data-board="ioc"]').click();
  assert.equal(await page.locator('#ioc-channels a.install').getAttribute('href'),null);
  await page.locator('#ioc-board-checked').check();
  assert.equal(await page.locator('#ioc-channels a.install').getAttribute('href'),'https://example.com/883m.uf2');
  await page.getByRole('button',{name:/Wilcoxon ILA210/}).click();
  assert(await page.locator('#ila210-section').isVisible());assert(!await page.locator('#sensor-883m').isVisible());
  assert.equal(await page.locator('#ila-channels esp-web-install-button').getAttribute('manifest'),'ila210/beta/manifest.json');
  await page.locator('.ila-board[data-board="ioc"]').click();
  assert(!await page.locator('#serial-console').isVisible());
  assert.equal(await page.locator('#ila-ioc-channels a.install').getAttribute('href'),'https://example.com/ila210.uf2');
  await page.setViewportSize({width:390,height:844});assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
  await page.keyboard.press('Tab');assert(await page.evaluate(()=>document.activeElement!==document.body));
  channels=channels.filter(c=>c.sensor==='883m');
  await page.reload();await page.getByRole('button',{name:/Wilcoxon ILA210/}).click();
  await page.waitForFunction(()=>document.querySelector('#ila-channels').textContent.includes('No installable release'));
  assert.equal(await page.locator('#ila-channels esp-web-install-button').count(),0);
  assert.equal(await page.locator('#ila-ioc-channels a.install').count(),0);assert.deepEqual(errors,[]);
  console.log('PASS: real installer sensor/board choices, isolated manifest and UF2 links, unpublished ILA fallback, mobile and keyboard.');
 }finally{await browser.close();await new Promise(r=>server.close(r));}
})().catch(e=>{console.error(e);process.exitCode=1;});
