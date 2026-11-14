// Tests the real installer UI with simulated releases; never accesses USB.
const {chromium}=require('../.tools/node_modules/playwright');
const fs=require('node:fs'),http=require('node:http'),assert=require('node:assert/strict');
let channels=[{sensor:'883m',channel:'stable',tag:'v5.0.0',manifest:'stable/manifest.json',notes:'https://example.com/notes',uf2:{cytron_iriv_io_controller:'https://example.com/883m.uf2'}},
 {sensor:'ila210',channel:'beta',tag:'v5.0.1-beta.1',manifest:'ila210/beta/manifest.json',notes:'https://example.com/notes',uf2:{cytron_iriv_io_controller:'https://example.com/ila210.uf2'}}];
const server=http.createServer((req,res)=>{res.setHeader('Content-Type',req.url.startsWith('/channels.json')?'application/json':'text/html');res.end(req.url.startsWith('/channels.json')?JSON.stringify({repo:'https://github.com/example/gateway',channels}):fs.readFileSync('installer/index.html'));});
(async()=>{
 await new Promise(r=>server.listen(0,'127.0.0.1',r));
 const browser=await chromium.launch({headless:true,executablePath:process.env.CHROME_PATH||'C:/Program Files/Google/Chrome/Application/chrome.exe'});
 try{
  const page=await browser.newPage(),errors=[];page.on('pageerror',e=>errors.push(e.message));
  await page.route('https://unpkg.com/**',r=>r.fulfill({contentType:'application/javascript',body:''}));
  await page.goto(`http://127.0.0.1:${server.address().port}`);
  await page.waitForSelector('#channels esp-web-install-button');
  assert.equal(await page.locator('#channels esp-web-install-button').getAttribute('manifest'),'stable/manifest.json');
  await page.getByRole('button',{name:/Wilcoxon ILA210/}).click();
  assert(await page.locator('#ila210-section').isVisible());assert(!await page.locator('#sensor-883m').isVisible());
  assert.equal(await page.locator('#ila-channels esp-web-install-button').getAttribute('manifest'),'ila210/beta/manifest.json');
  await page.locator('.ila-board[data-board="ioc"]').click();
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
